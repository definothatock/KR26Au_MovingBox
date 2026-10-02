#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LES_ConfigStructs.h"
#include "LES_Session.generated.h"

class ALESGoal;

class UBoxComponent;
class UCameraComponent;
class ULES_EditorComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/* ==================== Declares ==================== */

DECLARE_LOG_CATEGORY_EXTERN(LES_Session, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLESPhaseChanged, ELESPhase, NewPhase);

// Placed Entity entry for tracking and removal during the editing phase.
USTRUCT()
struct FLESPlacedRecord
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<AActor> Actor = nullptr;

    UPROPERTY(Transient)
    int32 DefinitionIndex = INDEX_NONE;
};

// Weak references intentionally do not keep physics components alive.
struct FLESFrozenBody
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    FVector LinearVelocity = FVector::ZeroVector;
    FVector AngularVelocity = FVector::ZeroVector;
};

/*
 * Monitor and coordinator of LES during a Session.
 * Responsible for phase management & flow control & editing actions (grid).
 *
 * Functions:
 * - Validates, spawns, relocates, removes, and activates placeables.
 * - Coordinates editing/gameplay/completion.
 * - Reloads the map for a complete reset.
 *
 * Rules:
 * - Only actors in PlacedRecords are editable.
 * - Placement definitions are read-only during a session.
 * - All mutation APIs check the current phase.
 *
 * Workflow:
 * Current control flow switches in Tick().
 * - Validate setup -> wait for local pawn -> Editing -> Gameplay -> Completed.
 *
 * Boundary:
 * - Original actors are never moved/deleted by editing requests.
 * - Placement height is fixed; no adaptation depending on altitude (may add).
 * - No serialised saving.
 *
 * TODO:
 * - Refactor Tick() to enhance flow-control maintainability (preferable delegates).
 * - Consider breaking Editing actions in to a different componenet, making manager only managing flows.
 * 
 * Note:
 * - 'this' ptr MIGHT not work after modifying this file;
 * cause unknown, but can be solved by reinstantiating Session Actor.
 * - For grid placing in 'Engine Editor', use the Engine alignment, according to specific level DataAsset.
 */
UCLASS(Blueprintable)
class KR26AU_MOVINGBOX_API ALES_Session : public AActor
{
    GENERATED_BODY()

public:
    ALES_Session();

    /* ==================== Overrides ==================== */

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /* ==================== APIs ==================== */

    /*--- Edit Mode: Selection ---*/
    
    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_SelectType(int32 DefinitionIndex);

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_CycleType();

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_CancelPreview();

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_SelectPlaced(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_MoveSelected();

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_RemoveSelected();

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_RotatePreview();

    /*--- Edit Mode: Preview / Placement ---*/

    // Called by the interaction component after cursor-plane intersection.
    void UpdatePlaceablePreview(const FVector& PlanePoint, bool bHasPlanePoint);
    
    UFUNCTION(BlueprintCallable, Category="LES")
    bool Request_ConfirmPlacement();
    
    /*--- level flow control ---*/

    UFUNCTION(BlueprintCallable, Category="LES")
    bool Request_FinishEditing();

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_ResetLevel();

    UFUNCTION(BlueprintCallable, Category="LES")
    void Request_NextLevel();


    /* ==================== Queries ==================== */

    UFUNCTION(BlueprintPure, Category="LES")
    ELESPhase GetPhase() const { return Phase; }

    UFUNCTION(BlueprintPure, Category="LES")
    int32 GetRemainingQuantity(int32 DefinitionIndex) const;

    UFUNCTION(BlueprintPure, Category="LES")
    bool IsSessionActor(AActor* Actor) const;

    UFUNCTION(BlueprintPure, Category="LES")
    bool HasPreview() const { return bHasPreview; }

    UFUNCTION(BlueprintPure, Category="LES")
    bool IsPreviewValid() const { return bCandidateValid; }

    UFUNCTION(BlueprintPure, Category="LES")
    FString GetStatusText() const { return StatusText; }

    ULES_ConfigStructs* GetLevelConfig() const { return LevelConfig.Get(); }
    int32 GetCurrentDefinition() const { return CurrentDefinition; }
    AActor* GetSelectedActor() const { return SelectedActor.Get(); }
    int32 GetPlacedCount() const;
    float GetPlacementPlaneZ() const { return GetActorLocation().Z; }
    bool IsMovingSelection() const { return bMovingSelection; }

    /* ==================== Event Delegates ==================== */

    UPROPERTY(BlueprintAssignable, Category="LES")
    FLESPhaseChanged OnPhaseChanged;

protected:
    /* ==================== Components ==================== */

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<UBoxComponent> AreaBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<UCameraComponent> OverviewCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<UStaticMeshComponent> PreviewMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<ULES_EditorComponent> EditorInteraction;

private:
    /* ==================== Internal Function ==================== */

    /*--- Validation ---*/
    
    bool ValidateSetup(FString& OutReason) const;

    bool ValidatePlacement(
        int32 DefinitionIndex,
        const FTransform& Transform,
        AActor* IgnoredActor,
        FString& OutReason) const;

    bool DoesSpawnFitFootprint(
        AActor* Actor,
        int32 DefinitionIndex,
        const FTransform& Transform) const;
    
    void Reject(const FString& Reason);

    /*--- Phase / Physics ---*/

    int32 FindRecord(AActor* Actor) const;
    void ChangePhase(ELESPhase NewPhase);

    void FreezeInitialPhysics();
    void RestoreInitialPhysics();
    
    void RefreshPreviewMesh();
    
    void DrawEditingDebug() const;

    /* ==================== Runtime State ==================== */

    UPROPERTY(Transient)
    ELESPhase Phase = ELESPhase::Waiting;
    // Debug/Logging Use; Verbose Phase state. 
    FString StatusText = TEXT("Waiting for the local player.");

    UPROPERTY(Transient)
    TArray<FLESPlacedRecord> PlacedRecords;

    TArray<FLESFrozenBody> FrozenBodies;
    
    TWeakObjectPtr<AActor> SelectedActor;

    FTransform CandidateTransform = FTransform::Identity;

    int32 CurrentDefinition = INDEX_NONE;
    float PreviewYaw = 0.0f;
    float GoalHoldTime = 0.0f;

    // ANCHOR: consider switching to enum/tags if more state
    bool bHasPreview = false;
    bool bCandidateValid = false;
    bool bMovingSelection = false;
    bool bTravelRequested = false;

    /* ==================== Config ==================== */

    // Null creates an empty runtime config, which setup validation rejects.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    TObjectPtr<ULES_ConfigStructs> LevelConfig;
    // Any Actor; Root Required to detect level-susses.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    TObjectPtr<AActor> TargetActor;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    TObjectPtr<ALESGoal> GoalActor;

    // Freezes existing simulated primitive components until gameplay starts.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    bool bFreezeExistingPhysics = true;
};
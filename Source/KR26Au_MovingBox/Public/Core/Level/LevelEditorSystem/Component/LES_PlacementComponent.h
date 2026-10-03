#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LES_PlacementComponent.generated.h"

class ALES_SessionManager;
class ULES_ConfigStructs;
class UPrimitiveComponent;

class UStaticMeshComponent;
class ALES_PlaceableBase;


USTRUCT()
struct FLES_PlacedRecord
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<AActor> Actor = nullptr;

    UPROPERTY(Transient)
    int32 DefinitionIndex = INDEX_NONE;
};

struct FLES_PreviewCollisionState
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    ECollisionEnabled::Type CollisionEnabled =
        ECollisionEnabled::NoCollision;
    bool bGenerateOverlapEvents = false;
    bool bSimulatingPhysics = false;
};


/*
 * Owns the runtime editing (placement) state for LES session.
 *
 * Function:
 * - Performs all editing actions.
 *
 * Rules:
 * - 
 *
 * Note:
 * ALES_SessionManager remains responsible for deciding when editing is permitted.
 *
 * TODO:
 *  - bound colour only reflects premature collision check, should I make it reflect other condition too (ie out of availability)?
 *  - I should reconsider: should the Bound or Mesh be authoritative check? 
 *  - If EditorAdaptor Changed the keys, remember the change the logs in here too! Should change to referencing bt then!
 *
 * Ref:
 * UE now has snapping in FMath: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FMath/GridSnap?lang=en-US
 */
UCLASS(ClassGroup=(LES), meta=(BlueprintSpawnableComponent))
class KR26AU_MOVINGBOX_API ULES_PlacementComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    ULES_PlacementComponent();

protected:
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    /* ==================== APIs ==================== */
public:
    /* ----- Lifecycle ----- */
    
    void Initialize(
        ALES_SessionManager* InSession,
        ULES_ConfigStructs* InConfig);

    void BeginEditing();
    void EndEditing();

    bool ValidateAllPlacedActors(FString& OutReason) const;
    void ActivatePlacedActors(bool bActive);

    /* ----- Editing Commands ----- */
    
    void SelectType(int32 DefinitionIndex);
    void CycleType();
    void CancelPreview();

    void SelectPlacedActor(AActor* Actor);
    void BeginMoveSelected();
    void RemoveSelected();
    void RotatePreview();

    void UpdatePreviewFromPlane(
        const FVector& PlanePoint,
        bool bHasPlanePoint);

    bool ConfirmPlacement();

    /* ==================== Queries ==================== */

    int32 GetRemainingQuantity(int32 DefinitionIndex) const;
    int32 GetPlacedCount() const;

    bool IsManagedActor(const AActor* Actor) const;
    bool HasPreview() const { return bHasPreview; }
    bool IsPreviewValid() const { return bCandidateValid; }
    bool IsMovingSelection() const { return bMovingSelection; }

    int32 GetCurrentDefinition() const { return CurrentDefinition; }
    AActor* GetSelectedActor() const { return SelectedActor.Get(); }

    FString GetStatusText() const { return StatusText; }
    void SetStatusText(const FString& InStatus) { StatusText = InStatus; }

    /* ==================== Internal Functions ==================== */

private:
    int32 FindRecord(const AActor* Actor) const;
    void RemoveInvalidRecords();

    const ALES_PlaceableBase* GetPlaceableDefaults(
    int32 DefinitionIndex,
    FString& OutReason) const;

    bool BuildCandidateTransform(
        const FVector& PlanePoint,
        FTransform& OutTransform,
        FString& OutReason) const;
    
    // Designer-provided placement bounds; advisory only - Preview colour/status, never decides.
    bool ValidatePreviewBounds(
        const ALES_PlaceableBase* Preview,
        FString& OutReason) const;
    
    // Authoritative placement validation; VisualMesh collision geometry.
    bool ValidateMeshPlacement(
        const ALES_PlaceableBase* Placeable,
        AActor* IgnoredActor,
        FString& OutReason) const;

    bool SpawnPreviewActor(
        int32 DefinitionIndex,
        FString& OutReason);

    void PrepareActorForPreview(ALES_PlaceableBase* Actor);
    void RestorePreviewActorCollision();

    void DestroyUnconfirmedPreview();
    void RestoreMovedActorTransform();

    void ShowPreview() const;
    void HidePreview() const;

    void RejectPlacement(const FString& Reason);
    void DrawDebug() const;

    UFUNCTION()
    void HandlePlacedActorDestroyed(AActor* DestroyedActor);
    
    /* ==================== Runtime State ==================== */
    
    TWeakObjectPtr<ALES_SessionManager> Session;
    UPROPERTY(Transient)
    TObjectPtr<ULES_ConfigStructs> LevelConfig;

    UPROPERTY(Transient)
    TArray<FLES_PlacedRecord> PlacedRecords;

    TWeakObjectPtr<AActor> SelectedActor;
    // Real instance of selected placeable class; New placements promote this into PlacedRecords.
    TWeakObjectPtr<ALES_PlaceableBase> PreviewActor;

    TArray<FLES_PreviewCollisionState> PreviewCollisionStates;

    int32 CurrentDefinition = INDEX_NONE;
    float PreviewYaw = 0.0f;

    FTransform CandidateTransform = FTransform::Identity;
    FTransform PreviewStartTransform = FTransform::Identity;

    bool bEditingActive = false;
    bool bHasPreview = false;
    bool bCandidateValid = false;
    bool bMovingSelection = false;

    FString StatusText = TEXT("Editing is not active.");
};
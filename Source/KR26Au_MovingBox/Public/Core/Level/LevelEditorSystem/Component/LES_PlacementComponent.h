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
 * Input:
 * - Command from SessionManager
 *
 * Function:
 * - Performs all editing actions.
 *
 * Output:
 * - Spawn Placeable objects.
 *
 * Rules:
 * - ALES_SessionManager decides when editing is permitted (Owner).
 * - Only actors stored in PlacedRecords are editable.
 * - Preview is a real spawned instance; promotes to placed when confirm and valid.
 * - Grid is intentional placement aids only: some Entity is not unify in shape, e.g. WoodenPLank.
 * - LESPlacementBounds provide advisory check; VisualMesh validate actual collisions.
 *
 * Flow:
 * Initialize -> BeginEditing -> Choose a placeable type
 *     ↓
 * Spawn hidden preview actor -> Build snapped candidate transform -> Move/show preview actor
 *     ↓
 * Run advisory placement-bound check -> User Confirm placement
 *     ↓
 * Run authoritative VisualMesh collision check
 *     ↓
 *     ├─ Invalid → reject placement
 *     └─ Valid
 *          ├─ New placement → register actor and create next preview
 *          └─ Move existing actor → keep new transform
 *     ↓
 * Continue editing -> Cancel preview / end editing
 *
 *
 * TODO:
 *  - Make Bound disallow placement too! Currently only upper bound will stop placing 
 *  - If EditorAdaptor Changed the keys, remember the change the logs in here too! Should change to referencing bt then!
 *
 * Ref:
 * https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FMath/GridSnap?lang=en-US
 * https://www.youtube.com/watch?v=b88Dj_k9b84
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

    void AdjustPlacementPlane(float ScrollDelta);
    
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

    float GetPlacementPlaneZ() const { return PlacementPlaneZ; }
    float GetMinimumPlacementPlaneZ() const;
    float GetMaximumPlacementPlaneZ() const;

    /* ==================== Internal Functions ==================== */

private:
    int32 FindRecord(const AActor* Actor) const;
    void RemoveInvalidRecords();

    // Clears references and flags after preview actor cleanup has already occurred.
    void ResetPreviewRuntimeState();

    const ALES_PlaceableBase* GetPlaceableDefaults(
        int32 DefinitionIndex,
        FString& OutReason) const;

    // Build preview of the placement candidate. Grid for placement aid.
    bool BuildCandidateTransform(
        const FVector& PlanePoint,
        FTransform& OutTransform,
        FString& OutReason) const;

    // Designer-provided placement bounds; advisory via bound colour/status.
    bool ValidatePreviewBounds(
        const ALES_PlaceableBase* Preview,
        FString& OutReason) const;

    // Authoritative placement validation using VisualMesh collision geometry.
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

    float PlacementPlaneZ = 0.0f;

    bool bEditingActive = false;
    bool bHasPreview = false;
    bool bCandidateValid = false;
    bool bMovingSelection = false;

    FString StatusText = TEXT("Editing is not active.");
};
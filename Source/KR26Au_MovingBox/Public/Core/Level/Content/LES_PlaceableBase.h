#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Interface/LES_PlaceableInterface.h"
#include "LES_PlaceableBase.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/*
 * Base class for all LES-placeable entities.
 *
 * Designer contract:
 * - PlacementBounds is visual aid for placement check; it may be any size appropriate for the Mesh (Entity).
 * - Actual placement check always depends on mesh overlaps.
 * - use OnLESGameplayActiveChanged instead.
 *
 * TODO
 * - RReconsider the purpose of bSimulateVisualMeshDuringGameplay & OnLESGameplayActiveChanged;
 * I could have just use the interface?
 */
UCLASS(Abstract, Blueprintable)
class KR26AU_MOVINGBOX_API ALES_PlaceableBase
    : public AActor
    , public ILES_PlaceableInterface
{
    GENERATED_BODY()

public:
    ALES_PlaceableBase();

    virtual void SetGameplayActive_Implementation(bool bActive) override;
    // virtual void HandleLESGameplayActiveChanged(bool bNowGameplayActive);

    /* ==================== Queries ==================== */

    bool GetLESPlacementData(
        FVector& OutPlacementHalfExtent,
        UStaticMesh*& OutPreviewMesh,
        FTransform& OutPreviewMeshLocalTransform,
        FString& OutReason) const;

    UStaticMeshComponent* GetVisualMesh() const { return VisualMesh; }
    UBoxComponent* GetPlacementBounds() const { return LESPlacementBounds; }

    bool IsGameplayActive() const { return bGameplayActive; }

    /* ==================== Internal Function ==================== */

protected:
    virtual void PostInitializeComponents() override;

    /*
     * Blueprint extension point.
     *
     * Use this for gameplay behavior such as enabling movement, AI, timers,
     * custom physics, effects, and so on.
     *
     * Do not reimplement SetGameplayActive directly in child Blueprints.
     */
    UFUNCTION(BlueprintImplementableEvent, Category="LES")
    void OnLESGameplayActiveChanged(bool bNowGameplayActive);
    
    /*
     * If true, the base class enables physics for VisualMesh when gameplay
     * begins, and disables it while in placement/editing state.
     *
     * Leave false for normal static placeables. More complicated entities
     * should manage their own additional components in the activation hook.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES|Gameplay")
    bool bSimulateVisualMeshDuringGameplay = false;

private:
    void ApplyGameplayState(bool bActive);

    UPROPERTY(Transient)
    bool bGameplayActive = false;

    
    /* ==================== Components ==================== */

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="LES")
    TObjectPtr<USceneComponent> SceneRoot;

    /*
     * The one static mesh used by the lightweight placement preview.
     *
     * Designers may adjust its relative transform. That transform is copied
     * to the session preview mesh.
     */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="LES")
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    /*
     * Authoritative LES placement footprint.
     * It has no collision itself; it is metadata for LES.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES")
    TObjectPtr<UBoxComponent> LESPlacementBounds;

};
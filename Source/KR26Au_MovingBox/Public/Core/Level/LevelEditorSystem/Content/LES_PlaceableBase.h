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
    
    void ApplyGameplayState(bool bActive);
    
    // Blueprint extension point; add custom behaviours.
    UFUNCTION(BlueprintImplementableEvent, Category="LES")
    void OnLESGameplayActiveChanged(bool bNowGameplayActive);



    /* ==================== Runtime State ==================== */

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES|Gameplay")
    bool bSimulateVisualMeshDuringGameplay = true;
    
    UPROPERTY(Transient)
    bool bGameplayActive = false;
    
    /* ==================== Components ==================== */

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="LES")
    TObjectPtr<USceneComponent> SceneRoot;
    
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="LES")
    TObjectPtr<UStaticMeshComponent> VisualMesh;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES")
    TObjectPtr<UBoxComponent> LESPlacementBounds;

};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DataStruct/LES_LevelEditorConfig.h"
#include "LES_SessionManager.generated.h"

class ALES_Goal;

class UBoxComponent;
class UCameraComponent;
class USceneComponent;

class ULES_EditorAdaptorComponent;
class ULES_ObjectiveComponent;
class ULES_PhysicsFreezeComponent;
class ULES_PlacementComponent;


DECLARE_LOG_CATEGORY_EXTERN(LES_Session, Log, All);


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLESPhaseChanged, ELES_Phase, NewPhase);

/*
 * Facade of the Level Editor System. Coordinates level-session via rerouting to components.
 *
 * Function:
 * - level configuration and level references;
 * - phase transitions;
 * - startup validation;
 * - entering/leaving editing;
 * - gameplay objective lifecycle;
 * - reset / next-level travel.
 * 
 * Note:
 * - Grid is only suggestion! actual overlapping check always depends on the mesh.
 * - For grid placing in 'Engine Editor', use the Engine alignment, according to specific level DataAsset.
 *
 * TODO:
 * - make bound size and camera distance adjustable via Config (LES Config).
 * - make Grid size adjustable via Config (LES Config).
 * 
 */
UCLASS(Blueprintable)
class KR26AU_MOVINGBOX_API ALES_SessionManager : public AActor
{
    GENERATED_BODY()

public:
    ALES_SessionManager();

    // UE Editor runtime update for instance of this actor.
    virtual void OnConstruction(const FTransform& Transform) override;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /* ==================== APIs ==================== */
    
    /* ----- Level session flow ----- */

    UFUNCTION(BlueprintCallable, Category="LES|Flow")
    bool Request_FinishEditing();

    UFUNCTION(BlueprintCallable, Category="LES|Flow")
    void Request_ResetLevel();

    UFUNCTION(BlueprintCallable, Category="LES|Flow")
    void Request_NextLevel();

    /* ----- Editing ----- */

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_SelectType(int32 DefinitionIndex);

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_CycleType();

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_CancelPreview();

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_SelectPlaced(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_MoveSelected();

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_RemoveSelected();

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_RotatePreview();

    void UpdatePlaceablePreview(const FVector& PlanePoint, bool bHasPlanePoint) const;

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    bool Request_ConfirmPlacement();

    UFUNCTION(BlueprintCallable, Category="LES|Editing")
    void Request_AdjustPlacementPlane(float ScrollDelta);


    /* ==================== Queries ==================== */

    UFUNCTION(BlueprintPure, Category="LES")
    ELES_Phase GetPhase() const { return Phase; }

    UFUNCTION(BlueprintPure, Category="LES")
    int32 GetRemainingQuantity(int32 DefinitionIndex) const;

    UFUNCTION(BlueprintPure, Category="LES")
    bool IsSessionActor(AActor* Actor) const;

    UFUNCTION(BlueprintPure, Category="LES")
    bool HasPreview() const;

    UFUNCTION(BlueprintPure, Category="LES")
    bool IsPreviewValid() const;

    UFUNCTION(BlueprintPure, Category="LES")
    FString GetStatusText() const;

    ULES_LevelEditorConfig* GetLevelConfig() const {return LevelConfig.Get();}

    int32 GetCurrentDefinition() const;
    AActor* GetSelectedActor() const;
    int32 GetPlacedCount() const;

    float GetPlacementPlaneZ() const;

    bool IsMovingSelection() const;

    /* ==================== Events ==================== */

    UPROPERTY(BlueprintAssignable, Category="LES")
    FLESPhaseChanged OnPhaseChanged;

    /* ==================== Internal Function ==================== */

    /* ----- Setup ----- */
private:
    bool ValidateSetup(FString& OutReason) const;
    bool TryEnterEditing();

    /* ----- Flow ----- */
    
    void ChangePhase(ELES_Phase NewPhase);
    UFUNCTION()
    void HandleObjectiveCompleted();

    /* ----- Debug ----- */
    
    void Reject(const FString& Reason);
    void DrawSessionDebug() const;

    /* ==================== Runtime State ==================== */
    
    UPROPERTY(Transient)
    ELES_Phase Phase = ELES_Phase::Waiting;
    FString FlowStatusText = TEXT("Waiting for the local player.");

    bool bTravelRequested = false;

    /* ==================== Config ==================== */

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    TObjectPtr<ULES_LevelEditorConfig> LevelConfig;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    TObjectPtr<AActor> TargetActor;

    UPROPERTY( EditInstanceOnly, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    TObjectPtr<ALES_Goal> GoalActor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Config", meta=(AllowPrivateAccess="true"))
    bool bFreezeExistingPhysics = true;

    /* ==================== Components ==================== */
    
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<UBoxComponent> AreaBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<UCameraComponent> OverviewCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<ULES_EditorAdaptorComponent> EditorInteraction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<ULES_PlacementComponent> PlacementEditor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<ULES_ObjectiveComponent> ObjectiveMonitor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<ULES_PhysicsFreezeComponent> PhysicsFreeze;
};
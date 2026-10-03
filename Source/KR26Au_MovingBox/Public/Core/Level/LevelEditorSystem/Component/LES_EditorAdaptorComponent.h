#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LES_EditorAdaptorComponent.generated.h"

class ALES_SessionManager;
class APlayerController;
class UInputComponent;

DECLARE_LOG_CATEGORY_EXTERN(LES_Editor, Log, All);

/*
 * LES input/camera adapter during the editing phase.
 *
 * Workflow:
 * - Adds persistent reset/next-level bindings.
 * - Pushes a blocking editing input component during editing.
 * - Restores the existing view target and normal game input on finish.
 *
 * Boundary:
 * - Assumes normal gameplay uses GameOnly input.
 * - Custom controller Tick-driven interaction must also check session phase.
 *
 * TODO:
 * - Add mouse scroll to change plane height?
 * - Move Editor inputs to IAM and IA.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class KR26AU_MOVINGBOX_API ULES_EditorAdaptorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULES_EditorAdaptorComponent();

    /* ==================== Overrides ==================== */

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /* ==================== APIs ==================== */

    bool Request_InitEditModeInput(
        ALES_SessionManager* InSession,
        APlayerController* InController);

    // Take over FP control and use TD view.
    bool Request_EnterEditing();
    bool Request_LeaveEditing();

private:
    /* ==================== Internal Function ==================== */

    /*--- Cursor ---*/

    void UpdateCursorPreview();

    /*--- Inputs ---*/

    void Input_ConfirmOrSelect();
    void Input_CycleType();
    void Input_Cancel();
    void Input_Rotate();
    void Input_Move();
    void Input_Remove();
    void Input_Finish();
    void Input_Reset();
    void Input_NextLevel();

    /* ==================== Runtime State ==================== */

    TWeakObjectPtr<ALES_SessionManager> Session;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> PreviousViewTarget;

    /*--- ANCHOR: Change to IMC and Actions later, for better input configuration ---*/
    UPROPERTY(Transient)
    TObjectPtr<UInputComponent> EditingInput;

    UPROPERTY(Transient)
    TObjectPtr<UInputComponent> GlobalInput;

    bool bEditingApplied = false;
    bool bPreviousShowCursor = false;
};
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_EditorComponent.h"

#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_Session.h"

#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY(LES_Editor);

ULES_EditorComponent::ULES_EditorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

/* ==================== APIs ==================== */


/*--- ANCHOR: Change to IMC and Actions later, for better input configuration ---*/
// Especially Function keys they overlap with editor 
bool ULES_EditorComponent::Request_InitEditModeInput(
    ALES_Session* InSession,
    APlayerController* InController)
{
    if (!IsValid(InSession)
        || !IsValid(InController)
        || !InController->IsLocalController()
        || !IsValid(InController->GetPawn()))
    {return false;}

    if (GlobalInput)
    {
        return Session.Get() == InSession
            && Controller.Get() == InController;
    }

    Session = InSession;
    Controller = InController;

    // Ensure cursor update runs after controller's input process; stabilise behaviour like cursor selection 
    AddTickPrerequisiteActor(InController);
    
    GlobalInput = NewObject<UInputComponent>(
        GetOwner(), TEXT("LESGlobalInput"));

    GlobalInput->RegisterComponent();
    GlobalInput->Priority = 255;
    GlobalInput->bBlockInput = false;

    GlobalInput->BindKey(
        EKeys::F5, IE_Pressed,
        this, &ULES_EditorComponent::Input_Reset);

    GlobalInput->BindKey(
        EKeys::F6, IE_Pressed,
        this, &ULES_EditorComponent::Input_NextLevel);

    InController->PushInputComponent(GlobalInput);

    EditingInput = NewObject<UInputComponent>(
        GetOwner(), TEXT("LESEditingInput"));

    EditingInput->RegisterComponent();
    EditingInput->Priority = 10000;
    EditingInput->bBlockInput = true;

    EditingInput->BindKey(
        EKeys::LeftMouseButton, IE_Pressed,
        this, &ULES_EditorComponent::Input_ConfirmOrSelect);

    EditingInput->BindKey(
        EKeys::RightMouseButton, IE_Pressed,
        this, &ULES_EditorComponent::Input_Cancel);

    EditingInput->BindKey(
        EKeys::Tab, IE_Pressed,
        this, &ULES_EditorComponent::Input_CycleType);

    EditingInput->BindKey(
        EKeys::R, IE_Pressed,
        this, &ULES_EditorComponent::Input_Rotate);

    EditingInput->BindKey(
        EKeys::M, IE_Pressed,
        this, &ULES_EditorComponent::Input_Move);

    EditingInput->BindKey(
        EKeys::Delete, IE_Pressed,
        this, &ULES_EditorComponent::Input_Remove);

    EditingInput->BindKey(
        EKeys::Enter, IE_Pressed,
        this, &ULES_EditorComponent::Input_Finish);

    return true;
}

bool ULES_EditorComponent::Request_EnterEditing()
{
    APlayerController* PC = Controller.Get();
    ALES_Session* ActiveSession = Session.Get();

    if (!IsValid(PC) || !IsValid(ActiveSession) || bEditingApplied)
    {
        return false;
    }

    PreviousViewTarget = PC->GetViewTarget();
    bPreviousShowCursor = PC->bShowMouseCursor;

    if (APawn* Pawn = PC->GetPawn())
    {
        if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
        {
            Movement->StopMovementImmediately();
        }
    }

    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);

    PC->PushInputComponent(EditingInput);
    PC->SetViewTarget(ActiveSession);

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;

    bEditingApplied = true;

    return true;
}

bool ULES_EditorComponent::Request_LeaveEditing()
{
    APlayerController* PC = Controller.Get();

    if (!IsValid(PC) || !bEditingApplied)
    {
        return false;
    }

    PC->PopInputComponent(EditingInput);

    // These calls balance the ignore-input calls made by this component.
    PC->SetIgnoreMoveInput(false);
    PC->SetIgnoreLookInput(false);

    AActor* RestoreTarget = PreviousViewTarget.Get();

    if (!IsValid(RestoreTarget))
    {
        RestoreTarget = PC->GetPawn();
    }

    if (IsValid(RestoreTarget))
    {
        PC->SetViewTarget(RestoreTarget);
    }

    PC->SetInputMode(FInputModeGameOnly());
    PC->bShowMouseCursor = bPreviousShowCursor;

    bEditingApplied = false;

    return true;
}

/* ==================== Overrides ==================== */

// ANCHOR: check can ticking-cursor-check be avoided later.
void ULES_EditorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ALES_Session* ActiveSession = Session.Get();

    if (IsValid(ActiveSession)
        && ActiveSession->GetPhase() == ELESPhase::Editing)
    {
        UpdateCursorPreview();
    }
}

void ULES_EditorComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    Request_LeaveEditing();

    if (APlayerController* PC = Controller.Get())
    {
        if (GlobalInput)
        {
            PC->PopInputComponent(GlobalInput);
        }
    }

    Super::EndPlay(EndPlayReason);
}

/* ==================== Internal ==================== */

/*--- Cursor ---*/

void ULES_EditorComponent::UpdateCursorPreview()
{
    APlayerController* PC = Controller.Get();
    ALES_Session* ActiveSession = Session.Get();

    if (!IsValid(PC)
        || !IsValid(ActiveSession)
        || !ActiveSession->HasPreview())
    {return;}

    FVector RayOrigin;
    FVector RayDirection;

    if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection) // ray from cam to cursor
        || FMath::Abs(RayDirection.Z) < KINDA_SMALL_NUMBER) // ANCHOR: edge case - parallel
    {
        ActiveSession->UpdatePlaceablePreview(FVector::ZeroVector, false);
        return;
    }

    const double DistFromLESPlane =
        (ActiveSession->GetPlacementPlaneZ() - RayOrigin.Z)
        / RayDirection.Z;

    if (DistFromLESPlane < 0.0) // ANCHOR: edge case - parallel
    {
        ActiveSession->UpdatePlaceablePreview(FVector::ZeroVector, false);
        return;
    }

    ActiveSession->UpdatePlaceablePreview(
        RayOrigin + RayDirection * DistFromLESPlane,
        true);
}

/*--- Input ---*/

void ULES_EditorComponent::Input_ConfirmOrSelect()
{
    ALES_Session* ActiveSession = Session.Get();
    APlayerController* PC = Controller.Get();

    if (!IsValid(ActiveSession)
        || !IsValid(PC)
        || ActiveSession->GetPhase() != ELESPhase::Editing)
    {
        return;
    }

    if (ActiveSession->HasPreview())
    {
        UpdateCursorPreview();
        ActiveSession->Request_ConfirmPlacement();
        return;
    }

    FHitResult Hit;

    PC->GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        Hit);

    ActiveSession->Request_SelectPlaced(Hit.GetActor());
}

void ULES_EditorComponent::Input_CycleType()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_CycleType();
        UpdateCursorPreview();
    }
}

void ULES_EditorComponent::Input_Cancel()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_CancelPreview();
    }
}

void ULES_EditorComponent::Input_Rotate()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_RotatePreview();
        UpdateCursorPreview();
    }
}

void ULES_EditorComponent::Input_Move()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_MoveSelected();
        UpdateCursorPreview();
    }
}

void ULES_EditorComponent::Input_Remove()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_RemoveSelected();
    }
}

void ULES_EditorComponent::Input_Finish()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_FinishEditing();
    }
}

void ULES_EditorComponent::Input_Reset()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_ResetLevel();
    }
}

void ULES_EditorComponent::Input_NextLevel()
{
    if (ALES_Session* ActiveSession = Session.Get())
    {
        ActiveSession->Request_NextLevel();
    }
}
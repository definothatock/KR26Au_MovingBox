#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_EditorAdaptorComponent.h"

#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_SessionManager.h"

#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY(LES_Editor);

ULES_EditorAdaptorComponent::ULES_EditorAdaptorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

/* ==================== APIs ==================== */


/*--- ANCHOR: Change to IMC and Actions later, for better input configuration ---*/
// Especially Function keys they overlap with the UE Editor 
bool ULES_EditorAdaptorComponent::Request_InitEditModeInput(
    ALES_SessionManager* InSession,
    APlayerController* InController)
{
    if (!IsValid(InSession)
        || !IsValid(InController)
        || !InController->IsLocalController()
        || !IsValid(InController->GetPawn()))
    {
        UE_LOG(LogTemp, Error, TEXT("[LES][Adaptor] Init Failed! Check prerequisite conditions!"));
        return false;
    }

    if (GlobalInput)
    {
        const bool bSameBinding =
            Session.Get() == InSession
            && Controller.Get() == InController;

        if (!bSameBinding)
        {UE_LOG(LES_Editor, Warning, TEXT("[LES][Adaptor] Init rejected: input is already bound to a different session or controller."));}
        else
        {UE_LOG(LES_Editor, Warning, TEXT("[LES][Adaptor] Init ignored: adaptor is already initialized."));}

        return bSameBinding;
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
        this, &ULES_EditorAdaptorComponent::Input_Reset);

    GlobalInput->BindKey(
        EKeys::F6, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_NextLevel);

    InController->PushInputComponent(GlobalInput);

    EditingInput = NewObject<UInputComponent>(
        GetOwner(), TEXT("LESEditingInput"));

    EditingInput->RegisterComponent();
    EditingInput->Priority = 10000;
    EditingInput->bBlockInput = true;

    EditingInput->BindKey(
        EKeys::LeftMouseButton, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_ConfirmOrSelect);

    EditingInput->BindKey(
        EKeys::RightMouseButton, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_Cancel);

    EditingInput->BindKey(
        EKeys::Tab, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_CycleType);

    EditingInput->BindKey(
        EKeys::R, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_Rotate);

    EditingInput->BindKey(
        EKeys::M, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_Move);

    EditingInput->BindKey(
        EKeys::Delete, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_Remove);

    EditingInput->BindKey(
        EKeys::Enter, IE_Pressed,
        this, &ULES_EditorAdaptorComponent::Input_Finish);

    return true;
}

bool ULES_EditorAdaptorComponent::Request_EnterEditing()
{
    APlayerController* PC = Controller.Get();
    ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(PC))
    {
        UE_LOG(LES_Editor, Warning, TEXT("[LES][Adaptor] Enter editing rejected: controller is invalid."));
        return false;
    }
    if (!IsValid(ActiveSession))
    {
        UE_LOG(LES_Editor, Warning, TEXT("[LES][Adaptor] Enter editing rejected: session is invalid."));
        return false;
    }
    if (bEditingApplied)
    {
        UE_LOG(LES_Editor, Warning, TEXT("[LES][Adaptor] Enter editing ignored: editing is already active."));
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

bool ULES_EditorAdaptorComponent::Request_LeaveEditing()
{
    APlayerController* PC = Controller.Get();

    if (!IsValid(PC) || !bEditingApplied)
    {
        return false;
    }

    PC->PopInputComponent(EditingInput);

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

void ULES_EditorAdaptorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ALES_SessionManager* ActiveSession = Session.Get();

    if (IsValid(ActiveSession)
        && ActiveSession->GetPhase() == ELES_Phase::Editing)
    {
        UpdateCursorPreview();
    }
}

void ULES_EditorAdaptorComponent::EndPlay(
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

void ULES_EditorAdaptorComponent::UpdateCursorPreview() const
{
    const APlayerController* PlyCtrl = Controller.Get();
    const ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(PlyCtrl)
        || !IsValid(ActiveSession)
        || !ActiveSession->HasPreview())
    {return;}

    FVector RayOrigin;
    FVector RayDirection;

    if (!PlyCtrl->DeprojectMousePositionToWorld(
            RayOrigin,
            RayDirection))
    {
        ActiveSession->UpdatePlaceablePreview(FVector::ZeroVector, false);
        // UE_LOG(LogTemp, Warning, TEXT("LES][Adaptor] Cursor could not be projected into the world."));
        return;
    }

    if (FMath::IsNearlyZero(RayDirection.Z))
    {
        ActiveSession->UpdatePlaceablePreview(FVector::ZeroVector, false);
        UE_LOG(LogTemp, Warning, TEXT("[LES][Adaptor] Cursor ray is parallel to the placement plane."));
        return;
    }

    const double DistFromLESPlane =
        (ActiveSession->GetPlacementPlaneZ() - RayOrigin.Z)
        / RayDirection.Z;

    if (DistFromLESPlane < 0.0)
    {
        ActiveSession->UpdatePlaceablePreview(FVector::ZeroVector, false);
        UE_LOG(LogTemp, Warning, TEXT("LES][Adaptor] Cursor ray intersects the placement plane behind the camera."));
        return;
    }

    ActiveSession->UpdatePlaceablePreview(
        RayOrigin + RayDirection * DistFromLESPlane,
        true);
}

/*--- Input ---*/

void ULES_EditorAdaptorComponent::Input_ConfirmOrSelect()
{
    ALES_SessionManager* ActiveSession = Session.Get();
    APlayerController* PC = Controller.Get();

    if (!IsValid(ActiveSession)
        || !IsValid(PC)
        || ActiveSession->GetPhase() != ELES_Phase::Editing)
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

void ULES_EditorAdaptorComponent::Input_CycleType()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_CycleType();
        UpdateCursorPreview();
    }
}

void ULES_EditorAdaptorComponent::Input_Cancel()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_CancelPreview();
    }
}

void ULES_EditorAdaptorComponent::Input_Rotate()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_RotatePreview();
        UpdateCursorPreview();
    }
}

void ULES_EditorAdaptorComponent::Input_Move()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_MoveSelected();
        UpdateCursorPreview();
    }
}

void ULES_EditorAdaptorComponent::Input_Remove()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_RemoveSelected();
    }
}

void ULES_EditorAdaptorComponent::Input_Finish()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_FinishEditing();
    }
}

void ULES_EditorAdaptorComponent::Input_Reset()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_ResetLevel();
    }
}

void ULES_EditorAdaptorComponent::Input_NextLevel()
{
    if (ALES_SessionManager* ActiveSession = Session.Get())
    {
        ActiveSession->Request_NextLevel();
    }
}
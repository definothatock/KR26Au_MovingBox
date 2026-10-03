#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_SessionManager.h"

#include "KR26Au_MovingBox/Public/Core/Level/COntent/LES_Goal.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_EditorAdaptorComponent.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_ObjectiveComponent.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_PhysicsFreezeComponent.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_PlacementComponent.h"

#include "KR26Au_MovingBox/Public/Core/Level/Content/LES_PlaceableBase.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LES_Session);

ALES_SessionManager::ALES_SessionManager()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    AreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBounds"));
    AreaBounds->SetupAttachment(SceneRoot);
    AreaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AreaBounds->SetBoxExtent(FVector(1000.0f, 1000.0f, 500.0f));
    AreaBounds->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
    AreaBounds->ShapeColor = FColor::Cyan;

    OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));

    OverviewCamera->SetupAttachment(SceneRoot);
    OverviewCamera->SetRelativeLocation(FVector(0.0f, -1600.0f, 1800.0f));
    OverviewCamera->SetRelativeRotation(FRotator(-48.0f, 90.0f, 0.0f));
    OverviewCamera->FieldOfView = 75.0f;
    OverviewCamera->bConstrainAspectRatio = false;

    EditorInteraction =
        CreateDefaultSubobject<ULES_EditorAdaptorComponent>(TEXT("LES_EditorInteraction"));

    PlacementEditor =
        CreateDefaultSubobject<ULES_PlacementComponent>(TEXT("LES_PlacementEditor"));

    ObjectiveMonitor =
        CreateDefaultSubobject<ULES_ObjectiveComponent>(TEXT("LES_ObjectiveMonitor"));

    PhysicsFreeze =
        CreateDefaultSubobject<ULES_PhysicsFreezeComponent>(TEXT("LES_PhysicsFreeze"));
}

void ALES_SessionManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    const FVector Extent = LevelConfig
        ? LevelConfig->AreaHalfExtent
        : FVector(1000.0f, 1000.0f, 500.0f);

    AreaBounds->SetBoxExtent(Extent);
    AreaBounds->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Z));
}

void ALES_SessionManager::BeginPlay()
{
    Super::BeginPlay();

    if (!LevelConfig)
    {
        UE_LOG(LES_Session, Error, TEXT("LevelConfig is NULL; Creating default LevelConfig."));

        LevelConfig = NewObject<ULES_ConfigStructs>(this);
    }

    AreaBounds->SetBoxExtent(LevelConfig->AreaHalfExtent);
    AreaBounds->SetRelativeLocation(
        FVector(0.0f, 0.0f, LevelConfig->AreaHalfExtent.Z));

    PlacementEditor->Initialize(this, LevelConfig);

    ObjectiveMonitor->OnObjectiveCompleted.AddDynamic(
        this,
        &ALES_SessionManager::HandleObjectiveCompleted);

    FString Reason;

    if (!ValidateSetup(Reason))
    {
        Reject(Reason);
        ChangePhase(ELES_Phase::Error);
        return;
    }

    UE_LOG(
        LES_Session,
        Log,
        TEXT("[LES] Configuration passed. Waiting for player."));
}

void ALES_SessionManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bTravelRequested || Phase == ELES_Phase::Error)
    {
        return;
    }

    if (Phase == ELES_Phase::Waiting)
    {
        TryEnterEditing();
        return;
    }

    if (Phase == ELES_Phase::Editing)
    {
        DrawSessionDebug();
    }
}

bool ALES_SessionManager::TryEnterEditing()
{
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PlayerController)
        || !PlayerController->IsLocalController()
        || !IsValid(PlayerController->GetPawn()))
    {
        return false;
    }

    if (!EditorInteraction->Request_InitEditModeInput(
        this,
        PlayerController))
    {
        Reject(TEXT("Could not initialize editing input."));
        ChangePhase(ELES_Phase::Error);

        return false;
    }

    if (bFreezeExistingPhysics)
    {
        PhysicsFreeze->FreezeWorldPhysics(GetWorld());
    }

    if (!EditorInteraction->Request_EnterEditing())
    {
        PhysicsFreeze->RestoreWorldPhysics();

        UE_LOG(
            LES_Session,
            Warning,
            TEXT("[LES] Failed entering edit mode; retrying."));

        return false;
    }

    PlacementEditor->BeginEditing();

    ChangePhase(ELES_Phase::Editing);

    PlacementEditor->SelectType(0);

    return true;
}

/* ==================== APIs: Level session flow ==================== */

bool ALES_SessionManager::Request_FinishEditing()
{
    if (Phase != ELES_Phase::Editing)
    {
        return false;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PlayerController)
        || !IsValid(PlayerController->GetPawn()))
    {
        PlacementEditor->SetStatusText(
            TEXT("Cannot start gameplay without the player pawn."));

        return false;
    }

    if (!IsValid(TargetActor) || !IsValid(GoalActor))
    {
        PlacementEditor->SetStatusText(
            TEXT("Cannot start gameplay: target or goal is missing."));

        return false;
    }

    FString Reason;

    if (!PlacementEditor->ValidateAllPlacedActors(Reason))
    {
        PlacementEditor->SetStatusText(
            TEXT("A placed entity is no longer valid. Move/remove it."));

        return false;
    }

    PlacementEditor->EndEditing();

    if (!EditorInteraction->Request_LeaveEditing())
    {
        UE_LOG(
            LES_Session,
            Warning,
            TEXT("[LES] Failed to restore normal player input."));
    }

    if (bFreezeExistingPhysics)
    {
        PhysicsFreeze->RestoreWorldPhysics();
    }

    // Phase is changed before gameplay activation callbacks.
    FlowStatusText =
        TEXT("Move the target into the goal. F5 resets the level.");

    ChangePhase(ELES_Phase::Gameplay);

    PlacementEditor->ActivatePlacedActors(true);

    if (!ObjectiveMonitor->StartMonitoring(TargetActor, GoalActor))
    {
        Reject(TEXT("Could not start the level objective monitor."));
        ChangePhase(ELES_Phase::Error);

        return false;
    }

    return true;
}

void ALES_SessionManager::Request_ResetLevel()
{
    if (bTravelRequested)
    {
        return;
    }

    const FString LevelName =
        UGameplayStatics::GetCurrentLevelName(this, true);

    if (LevelName.IsEmpty())
    {
        Reject(TEXT("Cannot determine the current map name."));
        return;
    }

    bTravelRequested = true;

    UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void ALES_SessionManager::Request_NextLevel()
{
    if (Phase != ELES_Phase::Completed || bTravelRequested)
    {
        return;
    }

    if (!LevelConfig || LevelConfig->NextLevel.IsNull())
    {
        Reject(TEXT("No next level is configured. F5 restarts this level."));
        return;
    }

    bTravelRequested = true;

    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this,
        LevelConfig->NextLevel);
}

/* ==================== APIs: Editing ==================== */

void ALES_SessionManager::Request_SelectType(int32 DefinitionIndex)
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->SelectType(DefinitionIndex);
    }
}

void ALES_SessionManager::Request_CycleType()
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->CycleType();
    }
}

void ALES_SessionManager::Request_CancelPreview()
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->CancelPreview();
    }
}

void ALES_SessionManager::Request_SelectPlaced(AActor* Actor)
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->SelectPlacedActor(Actor);
    }
}

void ALES_SessionManager::Request_MoveSelected()
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->BeginMoveSelected();
    }
}

void ALES_SessionManager::Request_RemoveSelected()
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->RemoveSelected();
    }
}

void ALES_SessionManager::Request_RotatePreview()
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->RotatePreview();
    }
}

void ALES_SessionManager::UpdatePlaceablePreview(
    const FVector& PlanePoint,
    bool bHasPlanePoint) const
{
    if (Phase == ELES_Phase::Editing)
    {
        PlacementEditor->UpdatePreviewFromPlane(
            PlanePoint,
            bHasPlanePoint);
    }
}

bool ALES_SessionManager::Request_ConfirmPlacement()
{
    return Phase == ELES_Phase::Editing
        && PlacementEditor->ConfirmPlacement();
}

/* ==================== Queries ==================== */

int32 ALES_SessionManager::GetRemainingQuantity(int32 DefinitionIndex) const
{
    return PlacementEditor
        ? PlacementEditor->GetRemainingQuantity(DefinitionIndex)
        : 0;
}

bool ALES_SessionManager::IsSessionActor(AActor* Actor) const
{
    return PlacementEditor && PlacementEditor->IsManagedActor(Actor);
}

bool ALES_SessionManager::HasPreview() const
{
    return PlacementEditor && PlacementEditor->HasPreview();
}

bool ALES_SessionManager::IsPreviewValid() const
{
    return PlacementEditor && PlacementEditor->IsPreviewValid();
}

FString ALES_SessionManager::GetStatusText() const
{
    if (Phase == ELES_Phase::Editing && PlacementEditor)
    {
        return PlacementEditor->GetStatusText();
    }

    return FlowStatusText;
}

int32 ALES_SessionManager::GetCurrentDefinition() const
{
    return PlacementEditor
        ? PlacementEditor->GetCurrentDefinition()
        : INDEX_NONE;
}

AActor* ALES_SessionManager::GetSelectedActor() const
{
    return PlacementEditor
        ? PlacementEditor->GetSelectedActor()
        : nullptr;
}

int32 ALES_SessionManager::GetPlacedCount() const
{
    return PlacementEditor
        ? PlacementEditor->GetPlacedCount()
        : 0;
}

bool ALES_SessionManager::IsMovingSelection() const
{
    return PlacementEditor && PlacementEditor->IsMovingSelection();
}

/* ==================== Internal Function ==================== */

bool ALES_SessionManager::ValidateSetup(FString& OutReason) const
{
    int32 SessionCount = 0;

    for (TActorIterator<ALES_SessionManager> It(GetWorld()); It; ++It)
    {
        ++SessionCount;
    }

    if (SessionCount != 1)
    {
        OutReason =
            TEXT("The map must contain exactly one LES session actor.");

        return false;
    }

    if (!GetActorQuat().Equals(FQuat::Identity, 0.001f)
        || !GetActorScale3D().Equals(FVector::OneVector, 0.001f))
    {
        OutReason =
            TEXT("Session actor rotation must be zero and scale must be one.");

        return false;
    }

    if (!IsValid(TargetActor) || !IsValid(GoalActor))
    {
        OutReason = TEXT("Target Actor and Goal Actor must be assigned.");
        return false;
    }

    if (TargetActor == this || TargetActor == GoalActor)
    {
        OutReason =
            TEXT("Target, Session, and Goal must be separate actors.");

        return false;
    }

    if (!LevelConfig
        || LevelConfig->AreaHalfExtent.GetMin() <= 0.0f
        || LevelConfig->GridSize <= 0.0f
        || LevelConfig->FloorClearance < 0.0f
        || LevelConfig->AvailableEntities.IsEmpty())
    {
        OutReason =
            TEXT("Invalid LevelConfig: LevelConfig self, area, grid, clearance, or empty entity catalog.");

        return false;
    }

    for (const FLES_LevelPlaceableEntry& Entry : LevelConfig->AvailableEntities)
    {
        const ULES_PlaceableDefinition* Definition = Entry.Definition;

        if (!IsValid(Definition))
        {
            OutReason = TEXT("At least 1 placeable definition asset is missing.");
            return false;
        }

        UClass* PlaceableClass = Definition->ActorClass.Get();

        if (!PlaceableClass
            || PlaceableClass->HasAnyClassFlags(CLASS_Abstract)
            || Entry.Quantity < 0)
        {
            OutReason = TEXT("Invalid Placeable Definition: class or quantity.");

            return false;
        }

        const ALES_PlaceableBase* Defaults =
            Definition->ActorClass->GetDefaultObject<ALES_PlaceableBase>();

        if (!IsValid(Defaults))
        {
            OutReason = TEXT("Invalid Placeable Definition: could not obtain base actor defaults.");

            return false;
        }

        FVector HalfExtent;
        UStaticMesh* PreviewMeshAsset = nullptr;
        FTransform PreviewLocalTransform;
        FString PlaceableReason;

        if (!Defaults->GetLESPlacementData(
            HalfExtent,
            PreviewMeshAsset,
            PreviewLocalTransform,
            PlaceableReason))
        {
            OutReason = FString::Printf(
                TEXT("Invalid Placeable Definition '%s': %s"),
                *GetNameSafe(Definition), *PlaceableReason);

            return false;
        }
    }

    
    if (GetNetMode() != NM_Standalone)
    {
        OutReason = TEXT(
            "LES currently supports standalone play only.");

        return false;
    }

    OutReason.Reset();
    return true;
}

void ALES_SessionManager::ChangePhase(ELES_Phase NewPhase)
{
    if (Phase == NewPhase)
    {
        return;
    }

    Phase = NewPhase;

    UE_LOG(
        LES_Session,
        Log,
        TEXT("[LES] Phase -> %s"),
        *StaticEnum<ELES_Phase>()->GetNameStringByValue(
            static_cast<int64>(Phase)));

    OnPhaseChanged.Broadcast(Phase);
}

void ALES_SessionManager::HandleObjectiveCompleted()
{
    if (Phase != ELES_Phase::Gameplay)
    {
        return;
    }

    FlowStatusText =
        TEXT("Level completed! F6: next level. F5: reset.");

    ChangePhase(ELES_Phase::Completed);
}

void ALES_SessionManager::Reject(const FString& Reason)
{
    FlowStatusText = Reason;

    UE_LOG(LES_Session, Warning, TEXT("[LES] %s"), *Reason);
}

void ALES_SessionManager::DrawSessionDebug() const
{
    if (!LevelConfig || !LevelConfig->bDrawArea)
    {
        return;
    }

    DrawDebugBox(
        GetWorld(),
        AreaBounds->GetComponentLocation(),
        AreaBounds->GetScaledBoxExtent(),
        FColor::Cyan,
        false,
        0.0f,
        0,
        2.0f);
}

#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_Session.h"

#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_EditorComponent.h"
#include "KR26Au_MovingBox/Public/Core/Level/COntent/LESGoal.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_PlaceableInterface.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LES_Session);

/* ==================== Construction ==================== */

ALES_Session::ALES_Session()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    AreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBounds"));
    AreaBounds->SetupAttachment(SceneRoot);
    AreaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AreaBounds->SetBoxExtent(FVector(1000.0, 1000.0, 500.0));
    AreaBounds->SetRelativeLocation(FVector(0.0, 0.0, 500.0));
    AreaBounds->ShapeColor = FColor::Cyan;

    OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
    OverviewCamera->SetupAttachment(SceneRoot);
    OverviewCamera->SetRelativeLocation(FVector(0.0, -1600.0, 1800.0));
    OverviewCamera->SetRelativeRotation(FRotator(-48.0, 90.0, 0.0));
    OverviewCamera->FieldOfView = 75.0f;
    OverviewCamera->bConstrainAspectRatio = false;

    PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(SceneRoot);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->SetCastShadow(false);
    PreviewMesh->SetHiddenInGame(true);
    // Anchor: Change this view later
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    PreviewMesh->SetStaticMesh(CubeMesh.Object);

    EditorInteraction = CreateDefaultSubobject<ULES_EditorComponent>(TEXT("LES_EditorInteraction"));
}

/* ==================== Overrides ==================== */

// Update on demand during BP editor actions.
void ALES_Session::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    const FVector Extent = LevelConfig ?
        LevelConfig->AreaHalfExtent
        : FVector(1000.0, 1000.0, 500.0);

    AreaBounds->SetBoxExtent(Extent);
    AreaBounds->SetRelativeLocation(FVector(0.0, 0.0, Extent.Z));
}

void ALES_Session::BeginPlay()
{
    Super::BeginPlay();

    if (!LevelConfig)
    {
        UE_LOG(LES_Session, Error, TEXT("LevelConfig is NULL!"));
        LevelConfig = NewObject<ULES_ConfigStructs>(this);
    }

    AreaBounds->SetBoxExtent(LevelConfig->AreaHalfExtent);
    AreaBounds->SetRelativeLocation(FVector(0.0, 0.0, LevelConfig->AreaHalfExtent.Z));

    FString Reason;

    if (!ValidateSetup(Reason))
    {
        Reject(Reason);
        ChangePhase(ELESPhase::Error);
        return;
    }

    UE_LOG(LES_Session, Log, TEXT("[LES] Configuration Passed. Waiting for player."));
}

// AHCHOR: Consider using delegates to control states; but be mindful with GoalHoldTime (requires time tracking)
void ALES_Session::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bTravelRequested || Phase == ELESPhase::Error)
    {
        return;
    }

    if (Phase == ELESPhase::Waiting)
    {
        APlayerController* PlyCtrl = UGameplayStatics::GetPlayerController(this, 0);

        if (!IsValid(PlyCtrl) || !PlyCtrl->IsLocalController() || !IsValid(PlyCtrl->GetPawn()))
        {return;}

        // AHCHOR: Check TODO in this func
        if (!EditorInteraction->Request_InitEditModeInput(this, PlyCtrl))
        {
            Reject(TEXT("Could not initialize editing input."));
            ChangePhase(ELESPhase::Error);
            return;
        }

        FreezeInitialPhysics();
        if (!EditorInteraction->Request_EnterEditing())
        {
            UE_LOG(LES_Session, Error, TEXT("[LES] failed entering edit mode, retry..."));
            return;
        }

        StatusText = TEXT("Tab: choose a type. Right mouse: selection mode.");
        ChangePhase(ELESPhase::Editing);
        Request_SelectType(0);
    }

    if (Phase == ELESPhase::Editing)
    {
        DrawEditingDebug();
        return;
    }

    if (Phase == ELESPhase::Gameplay)
    {
        if (!IsValid(TargetActor) || !IsValid(GoalActor))
        {
            StatusText = TEXT("Target or goal missing. Press F5 to reset.");
            return;
        }

        if (GoalActor->ContainsWorldPoint(TargetActor->GetActorLocation()))
        {
            GoalHoldTime += DeltaSeconds;

            if (GoalHoldTime >= GoalActor->GetRequiredHoldSeconds())
            {
                StatusText = TEXT("Level completed! F6: next level. F5: reset.");
                ChangePhase(ELESPhase::Completed);
            }
        }
        else
        {
            GoalHoldTime = 0.0f;
        }
    }
}

/* ==================== APIs ==================== */

/*--- Edit Mode: Selection ---*/

void ALES_Session::Request_SelectType(int32 DefinitionIndex)
{
    if (Phase != ELESPhase::Editing
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        return;
    }

    CurrentDefinition = DefinitionIndex;
    SelectedActor.Reset();
    PreviewYaw = 0.0f;
    bMovingSelection = false;
    bHasPreview = true;
    bCandidateValid = false;

    RefreshPreviewMesh();
    StatusText = TEXT("Move the cursor into the play area.");
}

void ALES_Session::Request_CycleType()
{
    if (Phase != ELESPhase::Editing || LevelConfig->AvailableEntities.IsEmpty())
    {return;}

    const int32 NextIndex = (CurrentDefinition + 1) % LevelConfig->AvailableEntities.Num();

    Request_SelectType(NextIndex);
}

void ALES_Session::Request_CancelPreview()
{
    if (Phase != ELESPhase::Editing)
    {return;}

    CurrentDefinition = INDEX_NONE;
    bHasPreview = false;
    bCandidateValid = false;
    bMovingSelection = false;

    PreviewMesh->SetHiddenInGame(true);
    StatusText = TEXT("Selection mode: click a player-placed entity.");
}

void ALES_Session::Request_SelectPlaced(AActor* Actor)
{
    if (Phase != ELESPhase::Editing)
    {return;}

    Request_CancelPreview();

    if (IsSessionActor(Actor))
    {
        SelectedActor = Actor;
        StatusText = TEXT("Selected. M: move. Delete: remove.");
    }
    else
    {
        SelectedActor.Reset();
        StatusText = TEXT("That actor is not editable in this session.");
    }
}

void ALES_Session::Request_MoveSelected()
{
    if (Phase != ELESPhase::Editing)
    {return;}

    AActor* Actor = SelectedActor.Get();
    const int32 RecordIndex = FindRecord(Actor);

    if (RecordIndex == INDEX_NONE)
    {
        Reject(TEXT("Select a player-placed entity first."));
        return;
    }

    CurrentDefinition = PlacedRecords[RecordIndex].DefinitionIndex;
    PreviewYaw = Actor->GetActorRotation().Yaw;

    bMovingSelection = true;
    bHasPreview = true;
    bCandidateValid = false;

    RefreshPreviewMesh();

    // The original remains untouched until confirmation succeeds.
    StatusText = TEXT("Moving preview. Right mouse cancels without changes.");
}

void ALES_Session::Request_RemoveSelected()
{
    if (Phase != ELESPhase::Editing)
    {return;}

    AActor* Actor = SelectedActor.Get();
    const int32 RecordIndex = FindRecord(Actor);

    if (RecordIndex == INDEX_NONE)
    {
        Reject(TEXT("Only current-session actors may be removed."));
        return;
    }

    const FString ActorName = Actor->GetName();

    if (!Actor->Destroy())
    {
        Reject(TEXT("Actor refused removal."));
        return;
    }

    PlacedRecords.RemoveAt(RecordIndex);
    SelectedActor.Reset();
    Request_CancelPreview();

    StatusText = TEXT("Entity removed; its quantity was returned.");
    UE_LOG(LES_Session, Log, TEXT("[LES] Removed %s"), *ActorName);
}

void ALES_Session::Request_RotatePreview()
{
    if (Phase == ELESPhase::Editing && bHasPreview)
    {
        PreviewYaw = FMath::Fmod(PreviewYaw + 90.0f, 360.0f);
        bCandidateValid = false;
    }
}

/*--- Edit Mode: Preview / Placement ---*/

void ALES_Session::UpdatePlaceablePreview(
    const FVector& PlanePoint,
    bool bHasPlanePoint)
{
    if (Phase != ELESPhase::Editing || !bHasPreview)
    {return;}

    if (!bHasPlanePoint)
    {
        bCandidateValid = false;
        PreviewMesh->SetHiddenInGame(true);
        StatusText = TEXT("Cursor does not intersect the placement plane.");
        return;
    }
    if (!LevelConfig->AvailableEntities.IsValidIndex(CurrentDefinition))
    {
        bCandidateValid = false;
        PreviewMesh->SetHiddenInGame(true);
        StatusText = TEXT("Current Entity Definition invalid.");
        return;
    }

    const FLESLevelPlaceableEntry& Entry =
        LevelConfig->AvailableEntities[CurrentDefinition];
    const ULESPlaceableDefinition& Definition = *Entry.Definition;

    /*--- quantise entity preview location ---*/
    
    const FVector Origin = GetActorLocation();
    const float Grid = LevelConfig->GridSize;

    FVector Location;
    Location.X = Origin.X + FMath::GridSnap(PlanePoint.X - Origin.X, Grid);
    Location.Y = Origin.Y + FMath::GridSnap(PlanePoint.Y - Origin.Y, Grid);
    Location.Z = Origin.Z + Definition.HalfExtent.Z + LevelConfig->FloorClearance;

    CandidateTransform = FTransform(
        FRotator(0.0, PreviewYaw, 0.0),
        Location,
        FVector::OneVector);

    PreviewMesh->SetWorldLocationAndRotation(Location, CandidateTransform.GetRotation());

    PreviewMesh->SetHiddenInGame(false);

    AActor* IgnoredActor = bMovingSelection ?
        SelectedActor.Get()
        : nullptr;

    if (bMovingSelection && !IsSessionActor(IgnoredActor))
    {
        bCandidateValid = false;
        StatusText = TEXT("The selected entity no longer exists.");
        return;
    }

    FString Reason;
    bCandidateValid = ValidatePlacement(
        CurrentDefinition, CandidateTransform, IgnoredActor, Reason);

    StatusText = bCandidateValid
        ? TEXT("VALID - left click confirms.")
        : FString::Printf(TEXT("INVALID - %s"), *Reason);
}

bool ALES_Session::Request_ConfirmPlacement()
{
    if (Phase != ELESPhase::Editing || !bHasPreview || !bCandidateValid)
    {return false;}

    AActor* MovingActor = bMovingSelection ?
        SelectedActor.Get() : nullptr;

    if (bMovingSelection && !IsSessionActor(MovingActor))
    {
        Reject(TEXT("The selected entity no longer exists."));
        return false;
    }

    FString Reason;
    // Never trust only the preceding frame's preview result.
    if (!ValidatePlacement(
        CurrentDefinition, CandidateTransform, MovingActor, Reason))
    {
        bCandidateValid = false;
        Reject(Reason);
        return false;
    }

    if (bMovingSelection)
    {
        const bool bMoved = MovingActor->SetActorTransform(
            CandidateTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);

        if (!bMoved)
        {
            Reject(TEXT("The selected entity could not be moved."));
            return false;
        }

        UE_LOG(LES_Session, Log, TEXT("[LES] Moved %s"),
            *MovingActor->GetName());

        Request_CancelPreview();
        StatusText = TEXT("Entity moved.");
        return true;
    }

    const ULESPlaceableDefinition& Definition =
        *LevelConfig->AvailableEntities[CurrentDefinition].Definition;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Spawned = GetWorld()->SpawnActor<AActor>(
        Definition.ActorClass.Get(),
        CandidateTransform,
        SpawnParameters);

    if (!IsValid(Spawned))
    {
        Reject(TEXT("Spawn failed. Quantity was not consumed."));
        return false;
    }

    ILES_PlaceableInterface::Execute_SetGameplayActive(Spawned, false);

    if (!IsValid(Spawned))
    {
        Reject(TEXT("Placeable destroyed itself during initialization."));
        return false;
    }

    const bool bTransformMatches =
        Spawned->GetActorTransform().Equals(CandidateTransform, 0.1f);

    if (!bTransformMatches
        || !DoesSpawnFitFootprint(
            Spawned, CurrentDefinition, CandidateTransform)
        || !ValidatePlacement(
            CurrentDefinition, CandidateTransform, Spawned, Reason))
    {
        Spawned->Destroy();
        Reject(TEXT("Spawned actor violated its placement footprint or collision."));
        return false;
    }

    FLESPlacedRecord Record;
    Record.Actor = Spawned;
    Record.DefinitionIndex = CurrentDefinition;
    PlacedRecords.Add(Record);

    UE_LOG(LES_Session, Log, TEXT("[LES] Placed %s; type %d; remaining %d"),
        *Spawned->GetName(),
        CurrentDefinition,
        GetRemainingQuantity(CurrentDefinition));

    // Remain in placement mode for repeated placement.
    bCandidateValid = false;
    StatusText = TEXT("Entity placed.");
    return true;
}

/*--- level flow control ---*/

bool ALES_Session::Request_FinishEditing()
{
    if (Phase != ELESPhase::Editing)
    {
        return false;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PC) || !IsValid(PC->GetPawn()))
    {
        Reject(TEXT("Cannot start gameplay without the player pawn."));
        return false;
    }

    if (!IsValid(TargetActor) || !IsValid(GoalActor))
    {
        Reject(TEXT("Cannot start gameplay: target or goal is missing."));
        return false;
    }

    // Protect against custom placeables changing themselves during editing.
    for (const FLESPlacedRecord& Record : PlacedRecords)
    {
        if (!IsValid(Record.Actor))
        {
            continue;
        }

        FString Reason;

        if (!DoesSpawnFitFootprint(
                Record.Actor,
                Record.DefinitionIndex,
                Record.Actor->GetActorTransform())
            || !ValidatePlacement(
                Record.DefinitionIndex,
                Record.Actor->GetActorTransform(),
                Record.Actor,
                Reason))
        {
            Reject(TEXT("A placed entity is no longer valid. Move/remove it."));
            return false;
        }
    }

    Request_CancelPreview();
    SelectedActor.Reset();

    // Set the phase before invoking entity callbacks.
    Phase = ELESPhase::Gameplay;

    EditorInteraction->Request_LeaveEditing();
    RestoreInitialPhysics();

    for (const FLESPlacedRecord& Record : PlacedRecords)
    {
        if (IsValid(Record.Actor))
        {
            ILES_PlaceableInterface::Execute_SetGameplayActive(
                Record.Actor, true);
        }
    }

    GoalHoldTime = 0.0f;
    StatusText = TEXT("Move the target into the goal. F5 resets the level.");

    UE_LOG(LES_Session, Log, TEXT("[LES] Phase -> Gameplay"));
    OnPhaseChanged.Broadcast(Phase);
    return true;
}

void ALES_Session::Request_ResetLevel()
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

    UE_LOG(LES_Session, Log, TEXT("[LES] Reset by reloading %s"), *LevelName);

    UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void ALES_Session::Request_NextLevel()
{
    if (Phase != ELESPhase::Completed || bTravelRequested)
    {
        return;
    }

    if (LevelConfig->NextLevel.IsNull())
    {
        Reject(TEXT("No next level is configured. F5 restarts this level."));
        return;
    }

    bTravelRequested = true;

    UE_LOG(LES_Session, Log, TEXT("[LES] Opening next level: %s"),
        *LevelConfig->NextLevel.ToString());

    UGameplayStatics::OpenLevelBySoftObjectPtr(
        this, LevelConfig->NextLevel);
}

/* ==================== Queries ==================== */

int32 ALES_Session::GetRemainingQuantity(int32 DefinitionIndex) const
{
    if (!LevelConfig
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        return 0;
    }

    int32 Used = 0;

    for (const FLESPlacedRecord& Record : PlacedRecords)
    {
        if (Record.DefinitionIndex == DefinitionIndex && IsValid(Record.Actor))
        {
            ++Used;
        }
    }

    return FMath::Max(
        0,
        LevelConfig->AvailableEntities[DefinitionIndex].Quantity - Used);
}

bool ALES_Session::IsSessionActor(AActor* Actor) const
{
    return IsValid(Actor) && FindRecord(Actor) != INDEX_NONE;
}

int32 ALES_Session::GetPlacedCount() const
{
    int32 Count = 0;

    for (const FLESPlacedRecord& Record : PlacedRecords)
    {
        if (IsValid(Record.Actor))
        {
            ++Count;
        }
    }

    return Count;
}

int32 ALES_Session::FindRecord(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return INDEX_NONE;
    }

    return PlacedRecords.IndexOfByPredicate(
        [Actor](const FLESPlacedRecord& Record)
        {
            return Record.Actor.Get() == Actor;
        });
}

/* ==================== Internal Function ==================== */

/*--- Validation ---*/

bool ALES_Session::ValidateSetup(FString& OutReason) const
{
    int32 SessionCount = 0;
    for (TActorIterator<ALES_Session> It(GetWorld()); It; ++It)
    {++SessionCount;}

    if (SessionCount != 1)
    {
        OutReason = TEXT("The map MUST contain exactly one LES session actor!");
        return false;
    }

    if (!GetActorQuat().Equals(FQuat::Identity, 0.001f)
        || !GetActorScale3D().Equals(FVector::OneVector, 0.001f))
    {
        OutReason = TEXT("Session actor rotation must be zero and scale must be one!");
        return false;
    }

    if (!IsValid(TargetActor) || !IsValid(GoalActor))
    {
        OutReason = TEXT("Target Actor and Goal Actor NOT assigned on the session!");
        return false;
    }

    if (TargetActor == this || TargetActor == GoalActor)
    {
        OutReason = TEXT("Target/Session/Goal are assigned to the same Actor! Decouple!");
        return false;
    }

    if (!LevelConfig
        || LevelConfig->AreaHalfExtent.GetMin() <= 0.0
        || LevelConfig->GridSize <= 0.0f
        || LevelConfig->FloorClearance < 0.0f
        || LevelConfig->AvailableEntities.IsEmpty())
    {
        OutReason = TEXT("Invalid area, grid, clearance, or empty entity catalog!");
        return false;
    }

    for (const FLESLevelPlaceableEntry& Entry : LevelConfig->AvailableEntities)
    {
        const ULESPlaceableDefinition* Definition = Entry.Definition;

        if (!Definition)
        {
            OutReason = TEXT("Bad Placeable Definition(s) in Config! Missing definition asset.");
            return false;
        }

        UClass* Class = Definition->ActorClass.Get();

        if (!Class
            || Class->HasAnyClassFlags(CLASS_Abstract)
            || !Definition->PreviewMesh
            || !Class->ImplementsInterface(ULES_PlaceableInterface::StaticClass())
            || Definition->HalfExtent.GetMin() <= 0.0
            || Entry.Quantity < 0)
        {
            OutReason =
                TEXT("Bad Placeable Definition(s) in Config! ")
                TEXT("MISSING: PlaceableInterface (not inherited from BP Base class) / preview mesh; ");
                TEXT("BAD-INIT: Abstract Class / Quantity<0 / HalfExtent<=0");
            return false;
        }
    }

    if (GetNetMode() != NM_Standalone)
    {
        OutReason = TEXT("Playable; But this prototype supports Standalone ONLY! Any remote client would not work.");
        return true;
    }

    return true;
}

bool ALES_Session::ValidatePlacement(
    int32 DefinitionIndex,
    const FTransform& Transform,
    AActor* IgnoredActor,
    FString& OutReason) const
{
    if (!LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        OutReason = TEXT("No entity type selected.");
        return false;
    }

    const ULESPlaceableDefinition& Definition =
        *LevelConfig->AvailableEntities[DefinitionIndex].Definition;

    // IgnoredActor -> relocation, existing-placement validation, or validation of just-spawned actor before register.
    if (!IgnoredActor && GetRemainingQuantity(DefinitionIndex) <= 0)
    {
        OutReason = TEXT("No quantity remaining.");
        return false;
    }

    if (!Transform.GetScale3D().Equals(FVector::OneVector, 0.001f))
    {
        OutReason = TEXT("Scaled placeable actors are unsupported.");
        return false;
    }

    const FRotator Rotation = Transform.Rotator();

    if (!FMath::IsNearlyZero(Rotation.Pitch, 0.1f)
        || !FMath::IsNearlyZero(Rotation.Roll, 0.1f)
        || !FMath::IsNearlyEqual(
            Rotation.Yaw,
            FMath::GridSnap(Rotation.Yaw, 90.0),
            0.1))
    {
        OutReason = TEXT("Only quarter-turn yaw rotation is supported.");
        return false;
    }

    const FVector Position = Transform.GetLocation();
    const FVector Half = Definition.HalfExtent;
    const FQuat Orientation = Transform.GetRotation();

    // Quarter-turn yaw makes this an exact axis-aligned world footprint.
    const FVector WorldHalf =
        Orientation.RotateVector(FVector(Half.X, 0.0, 0.0)).GetAbs()
        + Orientation.RotateVector(FVector(0.0, Half.Y, 0.0)).GetAbs()
        + Orientation.RotateVector(FVector(0.0, 0.0, Half.Z)).GetAbs();

    const FVector Origin = GetActorLocation();
    const FVector Area = LevelConfig->AreaHalfExtent;

    const FVector MinAllowed =
        Origin + FVector(-Area.X, -Area.Y, 0.0);

    const FVector MaxAllowed =
        Origin + FVector(Area.X, Area.Y, Area.Z * 2.0);

    const FVector CandidateMin = Position - WorldHalf;
    const FVector CandidateMax = Position + WorldHalf;

    if (CandidateMin.X < MinAllowed.X
        || CandidateMin.Y < MinAllowed.Y
        || CandidateMin.Z < MinAllowed.Z
        || CandidateMax.X > MaxAllowed.X
        || CandidateMax.Y > MaxAllowed.Y
        || CandidateMax.Z > MaxAllowed.Z)
    {
        OutReason = TEXT("The entire entity must fit inside the play area.");
        return false;
    }

    // Reserve configured footprints even if custom actors have small colliders.
    for (const FLESPlacedRecord& Record : PlacedRecords)
    {
        AActor* Other = Record.Actor.Get();

        if (!IsValid(Other) || Other == IgnoredActor)
        {
            continue;
        }

        const FVector OtherHalf =
            LevelConfig->AvailableEntities[Record.DefinitionIndex]
                .Definition->HalfExtent;

        const FQuat OtherRotation = Other->GetActorQuat();

        const FVector OtherWorldHalf =
            OtherRotation.RotateVector(FVector(OtherHalf.X, 0.0, 0.0)).GetAbs()
            + OtherRotation.RotateVector(FVector(0.0, OtherHalf.Y, 0.0)).GetAbs()
            + OtherRotation.RotateVector(FVector(0.0, 0.0, OtherHalf.Z)).GetAbs();

        const FVector Distance =
            (Position - Other->GetActorLocation()).GetAbs();

        const FVector Sum = WorldHalf + OtherWorldHalf;

        // 0.1 cm tolerance permits touching faces without false rejection.
        if (Distance.X < Sum.X - 0.1
            && Distance.Y < Sum.Y - 0.1
            && Distance.Z < Sum.Z - 0.1)
        {
            OutReason = TEXT("Overlaps another placed entity.");
            return false;
        }
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(LESPlacement), false);

    QueryParams.AddIgnoredActor(this);

    if (IgnoredActor)
    {
        QueryParams.AddIgnoredActor(IgnoredActor);
    }

    TArray<FOverlapResult> Overlaps;

    const FVector QueryHalf(
        FMath::Max(0.01, Half.X - 0.1),
        FMath::Max(0.01, Half.Y - 0.1),
        FMath::Max(0.01, Half.Z - 0.1));

    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        Position,
        Orientation,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(QueryHalf),
        QueryParams);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        UPrimitiveComponent* Component = Overlap.GetComponent();

        if (!IsValid(Component))
        {
            continue;
        }

        // Trigger-only volumes do not invalidate a placement.
        if (Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
            || Component->GetCollisionResponseToChannel(ECC_PhysicsBody)
                == ECR_Block)
        {
            OutReason = FString::Printf(
                TEXT("Blocked by %s."),
                *GetNameSafe(Component->GetOwner()));

            return false;
        }
    }

    OutReason.Reset();
    return true;
}

bool ALES_Session::DoesSpawnFitFootprint(
    AActor* Actor,
    int32 DefinitionIndex,
    const FTransform& Transform) const
{
    if (!IsValid(Actor)
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        return false;
    }

    const FVector Allowed =
        LevelConfig->AvailableEntities[DefinitionIndex]
            .Definition->HalfExtent
        + FVector(0.2);

    TInlineComponentArray<UPrimitiveComponent*> Components;
    Actor->GetComponents(Components);

    for (UPrimitiveComponent* Component : Components)
    {
        if (!IsValid(Component)
            || Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        const FBox Box = Component->Bounds.GetBox();

        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector WorldCorner(
                (Corner & 1) ? Box.Max.X : Box.Min.X,
                (Corner & 2) ? Box.Max.Y : Box.Min.Y,
                (Corner & 4) ? Box.Max.Z : Box.Min.Z);

            const FVector LocalCorner =
                Transform.InverseTransformPosition(WorldCorner).GetAbs();

            if (LocalCorner.X > Allowed.X
                || LocalCorner.Y > Allowed.Y
                || LocalCorner.Z > Allowed.Z)
            {
                return false;
            }
        }
    }

    return true;
}


void ALES_Session::Reject(const FString& Reason)
{
    StatusText = Reason;
    UE_LOG(LES_Session, Warning, TEXT("[LES] %s"), *Reason);
}


/*--- Phase / Physics ---*/


void ALES_Session::FreezeInitialPhysics()
{
    FrozenBodies.Reset();

    if (!bFreezeExistingPhysics)
    {
        return;
    }

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TInlineComponentArray<UPrimitiveComponent*> Components;
        It->GetComponents(Components);

        for (UPrimitiveComponent* Component : Components)
        {
            if (!IsValid(Component) || !Component->IsSimulatingPhysics())
            {
                continue;
            }

            FLESFrozenBody Body;
            Body.Component = Component;
            Body.LinearVelocity = Component->GetPhysicsLinearVelocity();
            Body.AngularVelocity =
                Component->GetPhysicsAngularVelocityInDegrees();

            FrozenBodies.Add(Body);
            Component->SetSimulatePhysics(false);
        }
    }

    UE_LOG(LES_Session, Log, TEXT("[LES] Froze %d physics components."),
        FrozenBodies.Num());
}

void ALES_Session::RestoreInitialPhysics()
{
    for (const FLESFrozenBody& Body : FrozenBodies)
    {
        UPrimitiveComponent* Component = Body.Component.Get();

        if (!IsValid(Component))
        {
            continue;
        }

        Component->SetSimulatePhysics(true);
        Component->SetPhysicsLinearVelocity(Body.LinearVelocity);
        Component->SetPhysicsAngularVelocityInDegrees(Body.AngularVelocity);
        Component->WakeAllRigidBodies();
    }

    FrozenBodies.Reset();
}

void ALES_Session::ChangePhase(ELESPhase NewPhase)
{
    Phase = NewPhase;

    UE_LOG(LES_Session, Log, TEXT("[LES] Phase -> %s"),
        *StaticEnum<ELESPhase>()->GetNameStringByValue(static_cast<int64>(Phase)));

    OnPhaseChanged.Broadcast(Phase);
}



/* ==================== Internal: Visuals ==================== */

void ALES_Session::RefreshPreviewMesh()
{
    const ULESPlaceableDefinition& Definition =
        *LevelConfig->AvailableEntities[CurrentDefinition].Definition;

    PreviewMesh->SetStaticMesh(Definition.PreviewMesh);

    const FVector MeshHalf =
        Definition.PreviewMesh->GetBounds().BoxExtent;

    PreviewMesh->SetWorldScale3D(FVector(
        Definition.HalfExtent.X / FMath::Max(MeshHalf.X, 0.01),
        Definition.HalfExtent.Y / FMath::Max(MeshHalf.Y, 0.01),
        Definition.HalfExtent.Z / FMath::Max(MeshHalf.Z, 0.01)));

    // UpdatePlaceablePreview unhides it only after a valid cursor-plane intersection.
    PreviewMesh->SetHiddenInGame(true);
}

void ALES_Session::DrawEditingDebug() const
{
    if (LevelConfig->bDrawArea)
    {
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

    if (LevelConfig->bDrawPlacement
        && bHasPreview
        && !PreviewMesh->bHiddenInGame
        && LevelConfig->AvailableEntities.IsValidIndex(CurrentDefinition))
    {
        DrawDebugBox(
            GetWorld(),
            CandidateTransform.GetLocation(),
            LevelConfig->AvailableEntities[CurrentDefinition]
                .Definition->HalfExtent,
            CandidateTransform.GetRotation(),
            bCandidateValid ? FColor::Green : FColor::Red,
            false,
            0.0f,
            0,
            3.0f);
    }

    AActor* Selected = SelectedActor.Get();

    if (LevelConfig->bDrawPlacement && IsValid(Selected))
    {
        const int32 RecordIndex = FindRecord(Selected);

        if (RecordIndex != INDEX_NONE)
        {
            DrawDebugBox(
                GetWorld(),
                Selected->GetActorLocation(),
                LevelConfig->AvailableEntities[
                    PlacedRecords[RecordIndex].DefinitionIndex]
                    .Definition->HalfExtent
                    + FVector(2.0),
                Selected->GetActorQuat(),
                FColor::Yellow,
                false,
                0.0f,
                0,
                3.0f);
        }
    }
}
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_PlacementComponent.h"

#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Content/LES_PlaceableBase.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/DataStruct/LES_ConfigStructs.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Interface/LES_PlaceableInterface.h"
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_SessionManager.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

ULES_PlacementComponent::ULES_PlacementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    UActorComponent::SetComponentTickEnabled(false);
}

void ULES_PlacementComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEditingActive)
    {
        DrawDebug();
    }
}

/* ==================== APIs ==================== */

void ULES_PlacementComponent::Initialize(
    ALES_SessionManager* InSession,
    ULES_ConfigStructs* InConfig)
{
    Session = InSession;
    LevelConfig = InConfig;

    HidePreview();
}

void ULES_PlacementComponent::BeginEditing()
{
    bEditingActive = true;

    SelectedActor.Reset();
    ResetPreviewRuntimeState();

    StatusText = TEXT("Tab: choose a type. Right mouse: selection mode.");

    SetComponentTickEnabled(true);
}

void ULES_PlacementComponent::EndEditing()
{
    CancelPreview();
    SelectedActor.Reset();

    bEditingActive = false;

    SetComponentTickEnabled(false);
}

bool ULES_PlacementComponent::ValidateAllPlacedActors(
    FString& OutReason) const
{
    for (const FLES_PlacedRecord& Record : PlacedRecords)
    {
        ALES_PlaceableBase* Placeable =
            Cast<ALES_PlaceableBase>(Record.Actor.Get());

        if (!IsValid(Placeable))
        {
            continue;
        }

        if (!ValidateMeshPlacement(Placeable, Placeable, OutReason))
        {
            return false;
        }
    }

    OutReason.Reset();
    return true;
}

void ULES_PlacementComponent::ActivatePlacedActors(bool bActive)
{
    for (const FLES_PlacedRecord& Record : PlacedRecords)
    {
        AActor* Actor = Record.Actor.Get();

        if (IsValid(Actor)
            && Actor->GetClass()->ImplementsInterface(
                ULES_PlaceableInterface::StaticClass()))
        {
            ILES_PlaceableInterface::Execute_SetGameplayActive(
                Actor,
                bActive);
        }
    }
}

void ULES_PlacementComponent::SelectType(int32 DefinitionIndex)
{
    if (!bEditingActive
        || !LevelConfig
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        return;
    }

    CancelPreview();

    CurrentDefinition = DefinitionIndex;
    SelectedActor.Reset();

    PreviewYaw = 0.0f;
    bMovingSelection = false;
    bCandidateValid = false;

    FString Reason;

    if (!SpawnPreviewActor(CurrentDefinition, Reason))
    {
        bHasPreview = false;
        StatusText = Reason;
        return;
    }

    bHasPreview = true;

    StatusText = TEXT("Move the cursor into the play area.");
}

void ULES_PlacementComponent::CycleType()
{
    if (!bEditingActive
        || !LevelConfig
        || LevelConfig->AvailableEntities.IsEmpty())
    {
        return;
    }

    const int32 NextIndex =
        LevelConfig->AvailableEntities.IsValidIndex(CurrentDefinition)
        ? (CurrentDefinition + 1) % LevelConfig->AvailableEntities.Num()
        : 0;

    SelectType(NextIndex);
}

void ULES_PlacementComponent::CancelPreview()
{
    if (bMovingSelection)
    {
        RestoreMovedActorTransform();
    }
    else
    {
        DestroyUnconfirmedPreview();
    }

    RestorePreviewActorCollision();
    ResetPreviewRuntimeState();

    CurrentDefinition = INDEX_NONE;

    if (bEditingActive)
    {
        StatusText = TEXT("Selection mode: click a player-placed entity.");
    }
}

void ULES_PlacementComponent::SelectPlacedActor(AActor* Actor)
{
    if (!bEditingActive)
    {
        return;
    }

    CancelPreview();

    if (IsManagedActor(Actor))
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

void ULES_PlacementComponent::BeginMoveSelected()
{
    if (!bEditingActive)
    {
        return;
    }

    AActor* Actor = SelectedActor.Get();
    const int32 RecordIndex = FindRecord(Actor);

    if (RecordIndex == INDEX_NONE)
    {
        StatusText = TEXT("Select a player-placed entity first.");
        return;
    }

    ALES_PlaceableBase* Placeable = Cast<ALES_PlaceableBase>(Actor);

    if (!IsValid(Placeable))
    {
        StatusText = TEXT("Selected actor is not a valid LES placeable.");
        return;
    }

    CancelPreview();

    CurrentDefinition = PlacedRecords[RecordIndex].DefinitionIndex;
    PreviewYaw = Placeable->GetActorRotation().Yaw;

    PreviewActor = Placeable;
    PreviewStartTransform = Placeable->GetActorTransform();

    PrepareActorForPreview(Placeable);

    bMovingSelection = true;
    bHasPreview = true;
    bCandidateValid = false;

    StatusText = TEXT("Moving entity. Right mouse cancels without changes.");
}

void ULES_PlacementComponent::RemoveSelected()
{
    if (!bEditingActive)
    {
        return;
    }

    AActor* Actor = SelectedActor.Get();

    if (!IsManagedActor(Actor))
    {
        StatusText = TEXT("Only current-session actors may be removed.");
        return;
    }

    const FString ActorName = Actor->GetName();

    SelectedActor.Reset();
    CancelPreview();

    if (!Actor->Destroy())
    {
        StatusText = TEXT("Actor refused removal.");
        return;
    }

    StatusText = TEXT("Entity removed; its quantity was returned.");

    UE_LOG(LES_Session, Log, TEXT("[LES] Removed %s"), *ActorName);
}

void ULES_PlacementComponent::RotatePreview()
{
    if (!bEditingActive || !bHasPreview)
    {
        return;
    }

    PreviewYaw = FMath::Fmod(PreviewYaw + 90.0f, 360.0f);
    bCandidateValid = false;
}

void ULES_PlacementComponent::UpdatePreviewFromPlane(
    const FVector& PlanePoint,
    bool bHasPlanePoint)
{
    if (!bEditingActive || !bHasPreview)
    {
        return;
    }

    ALES_PlaceableBase* Preview = PreviewActor.Get();

    if (!IsValid(Preview))
    {
        bCandidateValid = false;
        StatusText = TEXT("Preview actor no longer exists.");
        return;
    }

    if (!bHasPlanePoint)
    {
        bCandidateValid = false;
        HidePreview();

        StatusText = TEXT("Cursor does not intersect the placement plane.");
        return;
    }

    FTransform NewCandidate;
    FString Reason;

    if (!BuildCandidateTransform(PlanePoint, NewCandidate, Reason))
    {
        bCandidateValid = false;
        HidePreview();

        StatusText = Reason;
        return;
    }

    CandidateTransform = NewCandidate;

    Preview->SetActorTransform(
        CandidateTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    ShowPreview();

    // advisory only. Confirmation always runs ValidateMeshPlacement()
    bCandidateValid = ValidatePreviewBounds(Preview, Reason);

    StatusText = bCandidateValid
        ? TEXT("BOUND CLEAR - left click checks actual mesh collision.")
        : FString::Printf(TEXT("BOUND WARNING - %s Placement check still available, in case designer error."), *Reason);
}

bool ULES_PlacementComponent::ConfirmPlacement()
{
    if (!bEditingActive)
    {
        RejectPlacement(TEXT("Cannot place: placement editing is not active."));
        return false;
    }

    ALES_PlaceableBase* Preview = PreviewActor.Get();

    if (!bHasPreview || !IsValid(Preview))
    {
        RejectPlacement(TEXT("Cannot place: no active placeable preview."));
        return false;
    }

    FString Reason;

    if (!bMovingSelection
        && GetRemainingQuantity(CurrentDefinition) <= 0)
    {
        RejectPlacement(TEXT("Cannot place: no quantity remaining."));
        return false;
    }

    if (!ValidateMeshPlacement(Preview, Preview, Reason))
    {
        RejectPlacement(FString::Printf(TEXT("Cannot place: actual mesh collision failed: %s"),*Reason));

        return false;
    }

    RestorePreviewActorCollision();

    if (bMovingSelection)
    {
        ResetPreviewRuntimeState();

        StatusText = TEXT("Entity moved.");

        UE_LOG(LES_Session, Log, TEXT("[LES][Placement] Moved %s to %s"),
            *Preview->GetName(), *Preview->GetActorLocation().ToString());

        return true;
    }

    FLES_PlacedRecord& Record = PlacedRecords.AddDefaulted_GetRef();
    Record.Actor = Preview;
    Record.DefinitionIndex = CurrentDefinition;

    Preview->OnDestroyed.AddDynamic(
        this,
        &ULES_PlacementComponent::HandlePlacedActorDestroyed);

    UE_LOG(
        LES_Session,
        Log,
        TEXT("[LES][Placement] Placed '%s'; type=%d; location=%s; remaining=%d"),
        *Preview->GetName(), CurrentDefinition, *Preview->GetActorLocation().ToString(), GetRemainingQuantity(CurrentDefinition));

    const int32 PlacedDefinition = CurrentDefinition;

    ResetPreviewRuntimeState();

    FString SpawnReason;

    if (SpawnPreviewActor(PlacedDefinition, SpawnReason))
    {
        bHasPreview = true;
        bCandidateValid = false;

        StatusText = TEXT("Entity placed. Move cursor to place another.");
    }
    else
    {
        CurrentDefinition = INDEX_NONE;
        StatusText = TEXT("Entity placed.");
    }

    return true;
}

/* ==================== Queries ==================== */

int32 ULES_PlacementComponent::GetRemainingQuantity(
    int32 DefinitionIndex) const
{
    if (!LevelConfig
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {return 0;}

    int32 Used = 0;

    for (const FLES_PlacedRecord& Record : PlacedRecords)
    {
        if (Record.DefinitionIndex == DefinitionIndex
            && IsValid(Record.Actor))
        {
            ++Used;
        }
    }

    return FMath::Max(
        0,
        LevelConfig->AvailableEntities[DefinitionIndex].Quantity - Used);
}

int32 ULES_PlacementComponent::GetPlacedCount() const
{
    int32 Count = 0;

    for (const FLES_PlacedRecord& Record : PlacedRecords)
    {
        if (IsValid(Record.Actor))
        {
            ++Count;
        }
    }

    return Count;
}

bool ULES_PlacementComponent::IsManagedActor(
    const AActor* Actor) const
{
    return IsValid(Actor) && FindRecord(Actor) != INDEX_NONE;
}

int32 ULES_PlacementComponent::FindRecord(
    const AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return INDEX_NONE;
    }

    return PlacedRecords.IndexOfByPredicate(
        [Actor](const FLES_PlacedRecord& Record)
        {
            return Record.Actor.Get() == Actor;
        });
}

void ULES_PlacementComponent::RemoveInvalidRecords()
{
    PlacedRecords.RemoveAll(
        [](const FLES_PlacedRecord& Record)
        {
            return !IsValid(Record.Actor);
        });
}

void ULES_PlacementComponent::ResetPreviewRuntimeState()
{
    PreviewActor.Reset();
    PreviewCollisionStates.Reset();

    bHasPreview = false;
    bCandidateValid = false;
    bMovingSelection = false;
}

const ALES_PlaceableBase* ULES_PlacementComponent::GetPlaceableDefaults(
    int32 DefinitionIndex,
    FString& OutReason) const
{
    if (!LevelConfig
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        OutReason = TEXT("Placeable definition index is invalid.");
        return nullptr;
    }

    const ULES_PlaceableDefinition* Definition =
        LevelConfig->AvailableEntities[DefinitionIndex].Definition;

    if (!IsValid(Definition) || !Definition->ActorClass)
    {
        OutReason = TEXT("Placeable actor class is missing.");
        return nullptr;
    }

    // Unreal Reflection always make a def instance for instantiating. Use that to preview.
    const ALES_PlaceableBase* Defaults =
        Definition->ActorClass->GetDefaultObject<ALES_PlaceableBase>();

    if (!IsValid(Defaults))
    {
        OutReason = TEXT("Could not obtain placeable actor defaults.");
        return nullptr;
    }

    OutReason.Reset();
    return Defaults;
}

bool ULES_PlacementComponent::BuildCandidateTransform(
    const FVector& PlanePoint,
    FTransform& OutTransform,
    FString& OutReason) const
{
    ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(ActiveSession)
        || !LevelConfig
        || !LevelConfig->AvailableEntities.IsValidIndex(CurrentDefinition))
    {
        OutReason = TEXT("Current entity definition is invalid.");
        return false;
    }

    const ALES_PlaceableBase* Defaults =
        GetPlaceableDefaults(CurrentDefinition, OutReason);

    if (!Defaults)
    {
        return false;
    }

    FVector UnusedHalfExtent;
    UStaticMesh* Mesh = nullptr;
    FTransform MeshLocalTransform;

    if (!Defaults->GetLESPlacementData(
            UnusedHalfExtent,
            Mesh,
            MeshLocalTransform,
            OutReason))
    {
        return false;
    }

    if (!IsValid(Mesh))
    {
        OutReason = TEXT("Selected placeable has no preview mesh.");
        return false;
    }

    const FVector Origin = ActiveSession->GetActorLocation();
    const float Grid = LevelConfig->GridSize;

    const FTransform ActorRotationAtOrigin(
        FRotator(0.0f, PreviewYaw, 0.0f),
        FVector::ZeroVector,
        FVector::OneVector);

    const FTransform MeshTransformAtOrigin =
        MeshLocalTransform * ActorRotationAtOrigin;

    const FBoxSphereBounds MeshBounds =
        Mesh->GetBounds().TransformBy(MeshTransformAtOrigin);

    const float MeshBottomZ =
        MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;

    FVector Location;
    Location.X =
        Origin.X + FMath::GridSnap(PlanePoint.X - Origin.X, Grid);

    Location.Y =
        Origin.Y + FMath::GridSnap(PlanePoint.Y - Origin.Y, Grid);
    
    // rid only aligns X & Y; Z is derived from the actual mesh, not LESPlacementBounds.
    Location.Z =
        Origin.Z
        + LevelConfig->FloorClearance
        - MeshBottomZ;

    OutTransform = FTransform(
        FRotator(0.0f, PreviewYaw, 0.0f),
        Location,
        FVector::OneVector);

    OutReason.Reset();
    return true;
}

bool ULES_PlacementComponent::ValidatePreviewBounds(
    const ALES_PlaceableBase* Preview,
    FString& OutReason) const
{
    ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(ActiveSession) || !IsValid(Preview) || !LevelConfig)
    {
        OutReason = TEXT("Preview validation state is invalid.");
        return false;
    }

    const UBoxComponent* Bounds = Preview->GetPlacementBounds();

    if (!IsValid(Bounds))
    {
        OutReason = TEXT("Preview actor has no LESPlacementBounds component.");
        return false;
    }

    const FVector HalfExtent = Bounds->GetScaledBoxExtent();

    if (HalfExtent.GetMin() <= 0.0f)
    {
        OutReason = TEXT("LESPlacementBounds has invalid extents.");
        return false;
    }

    const FVector Position = Bounds->GetComponentLocation();
    const FQuat Rotation = Bounds->GetComponentQuat();

    const FVector WorldHalf =
        Rotation.RotateVector(FVector(HalfExtent.X, 0.0f, 0.0f)).GetAbs()
        + Rotation.RotateVector(FVector(0.0f, HalfExtent.Y, 0.0f)).GetAbs()
        + Rotation.RotateVector(FVector(0.0f, 0.0f, HalfExtent.Z)).GetAbs();

    const FVector Area = LevelConfig->AreaHalfExtent;
    const FVector Origin = ActiveSession->GetActorLocation();

    const FVector MinAllowed =
        Origin + FVector(-Area.X, -Area.Y, 0.0f);

    const FVector MaxAllowed =
        Origin + FVector(Area.X, Area.Y, 0.0f);

    if (Position.X - WorldHalf.X < MinAllowed.X
        || Position.Y - WorldHalf.Y < MinAllowed.Y
        || Position.X + WorldHalf.X > MaxAllowed.X
        || Position.Y + WorldHalf.Y > MaxAllowed.Y)
    {
        OutReason = TEXT("Placement bounds extend outside the edit area.");
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(LESPreviewBounds),
        false,
        Preview);

    QueryParams.AddIgnoredActor(ActiveSession);

    TArray<FOverlapResult> Overlaps;

    ActiveSession->GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        Position,
        Rotation,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(HalfExtent),
        QueryParams);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        UPrimitiveComponent* Component = Overlap.GetComponent();

        if (!IsValid(Component)
            || Component->GetCollisionEnabled()
                == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        if (Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
            || Component->GetCollisionResponseToChannel(ECC_PhysicsBody)
                == ECR_Block)
        {
            OutReason = FString::Printf(
                TEXT("Placement bounds overlap %s."),
                *GetNameSafe(Component->GetOwner()));

            return false;
        }
    }

    OutReason.Reset();
    return true;
}

bool ULES_PlacementComponent::ValidateMeshPlacement(
    const ALES_PlaceableBase* Placeable,
    AActor* IgnoredActor,
    FString& OutReason) const
{
    ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(ActiveSession) || !IsValid(Placeable))
    {
        OutReason = TEXT("Mesh placement validation state is invalid.");
        return false;
    }

    UStaticMeshComponent* VisualMesh = Placeable->GetVisualMesh();

    if (!IsValid(VisualMesh) || !IsValid(VisualMesh->GetStaticMesh()))
    {
        OutReason = TEXT("Placeable has no valid VisualMesh.");
        return false;
    }

    if (VisualMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        OutReason = TEXT(
            "VisualMesh collision is disabled; it cannot be used for placement validation.");

        return false;
    }

    FComponentQueryParams QueryParams(
        SCENE_QUERY_STAT(LESPlaceableMesh), Placeable);

    QueryParams.AddIgnoredActor(ActiveSession);

    if (IgnoredActor)
    {
        QueryParams.AddIgnoredActor(IgnoredActor);
    }

    TArray<FOverlapResult> Overlaps;

    ActiveSession->GetWorld()->ComponentOverlapMulti(
        Overlaps,
        VisualMesh,
        VisualMesh->GetComponentLocation(),
        VisualMesh->GetComponentQuat(),
        QueryParams,
        FCollisionObjectQueryParams::AllObjects);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        UPrimitiveComponent* OtherComponent = Overlap.GetComponent();

        if (!IsValid(OtherComponent)
            || OtherComponent->GetCollisionEnabled()
                == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        const ECollisionResponse VisualMeshResponse =
            VisualMesh->GetCollisionResponseToChannel(
                OtherComponent->GetCollisionObjectType());

        const ECollisionResponse OtherComponentResponse =
            OtherComponent->GetCollisionResponseToChannel(
                VisualMesh->GetCollisionObjectType());

        const bool bMutuallyBlocking =
            VisualMeshResponse == ECR_Block
            && OtherComponentResponse == ECR_Block;

        if (!bMutuallyBlocking)
        {
            continue;
        }

        OutReason = FString::Printf(TEXT("VisualMesh overlaps %s."),
            *GetNameSafe(OtherComponent->GetOwner()));

        return false;
    }

    OutReason.Reset();
    return true;
}

bool ULES_PlacementComponent::SpawnPreviewActor(
    int32 DefinitionIndex,
    FString& OutReason)
{
    DestroyUnconfirmedPreview();
    RestorePreviewActorCollision();

    ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(ActiveSession)
        || !LevelConfig
        || !LevelConfig->AvailableEntities.IsValidIndex(DefinitionIndex))
    {
        OutReason = TEXT("Cannot create preview: invalid placement state.");
        return false;
    }

    const ULES_PlaceableDefinition* Definition =
        LevelConfig->AvailableEntities[DefinitionIndex].Definition;

    if (!IsValid(Definition) || !Definition->ActorClass)
    {
        OutReason = TEXT("Cannot create preview: placeable class is missing.");
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = ActiveSession;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ALES_PlaceableBase* SpawnedPreview =
        ActiveSession->GetWorld()->SpawnActor<ALES_PlaceableBase>(
            Definition->ActorClass.Get(),
            FTransform(
                FRotator::ZeroRotator,
                FVector(0.0f, 0.0f, -1000000.0f),
                FVector::OneVector),
            SpawnParameters);

    if (!IsValid(SpawnedPreview))
    {
        OutReason = TEXT("Cannot create preview actor.");
        return false;
    }

    ILES_PlaceableInterface::Execute_SetGameplayActive(
        SpawnedPreview,
        false);

    PreviewActor = SpawnedPreview;

    PrepareActorForPreview(SpawnedPreview);

    SpawnedPreview->SetActorHiddenInGame(true);

    OutReason.Reset();
    return true;
}

void ULES_PlacementComponent::PrepareActorForPreview(
    ALES_PlaceableBase* Actor)
{
    PreviewCollisionStates.Reset();

    if (!IsValid(Actor))
    {
        return;
    }

    TInlineComponentArray<UPrimitiveComponent*> Components;
    Actor->GetComponents(Components);

    for (UPrimitiveComponent* Component : Components)
    {
        if (!IsValid(Component))
        {
            continue;
        }

        FLES_PreviewCollisionState& State =
            PreviewCollisionStates.AddDefaulted_GetRef();

        State.Component = Component;
        State.CollisionEnabled = Component->GetCollisionEnabled();
        State.bGenerateOverlapEvents =
            Component->GetGenerateOverlapEvents();
        State.bSimulatingPhysics =
            Component->IsSimulatingPhysics();
        
        // QueryOnly keeps real collision geometry avail for CompOverlapMulti w/o letting preview physics affect play.
        if (State.CollisionEnabled != ECollisionEnabled::NoCollision)
        {
            Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }

        Component->SetGenerateOverlapEvents(false);
        Component->SetSimulatePhysics(false);
    }
}

void ULES_PlacementComponent::RestorePreviewActorCollision()
{
    for (const FLES_PreviewCollisionState& State : PreviewCollisionStates)
    {
        UPrimitiveComponent* Component = State.Component.Get();

        if (!IsValid(Component))
        {
            continue;
        }

        Component->SetCollisionEnabled(State.CollisionEnabled);
        Component->SetGenerateOverlapEvents(
            State.bGenerateOverlapEvents);

        if (State.bSimulatingPhysics)
        {
            Component->SetSimulatePhysics(true);
        }
    }
}

void ULES_PlacementComponent::DestroyUnconfirmedPreview()
{
    if (bMovingSelection)
    {
        return;
    }

    ALES_PlaceableBase* Preview = PreviewActor.Get();

    if (IsValid(Preview))
    {
        Preview->Destroy();
    }
}

void ULES_PlacementComponent::RestoreMovedActorTransform()
{
    ALES_PlaceableBase* Preview = PreviewActor.Get();

    if (!IsValid(Preview))
    {
        return;
    }

    Preview->SetActorTransform(
        PreviewStartTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}

void ULES_PlacementComponent::ShowPreview() const
{
    if (ALES_PlaceableBase* Preview = PreviewActor.Get())
    {
        Preview->SetActorHiddenInGame(false);
    }
}

void ULES_PlacementComponent::HidePreview() const
{
    if (ALES_PlaceableBase* Preview = PreviewActor.Get())
    {
        Preview->SetActorHiddenInGame(true);
    }
}

void ULES_PlacementComponent::RejectPlacement(const FString& Reason)
{
    bCandidateValid = false;
    StatusText = Reason;

    UE_LOG(LES_Session, Warning, TEXT("[LES][Placement] %s"), *Reason);
}
void ULES_PlacementComponent::DrawDebug() const
{
    ALES_SessionManager* ActiveSession = Session.Get();

    if (!IsValid(ActiveSession)
        || !LevelConfig
        || !LevelConfig->bDrawPlacement)
    {
        return;
    }

    const ALES_PlaceableBase* Preview = PreviewActor.Get();

    if (bHasPreview && IsValid(Preview))
    {
        const UBoxComponent* Bounds = Preview->GetPlacementBounds();

        if (IsValid(Bounds) && !Preview->IsHidden())
        {
            DrawDebugBox(
                ActiveSession->GetWorld(),
                Bounds->GetComponentLocation(),
                Bounds->GetScaledBoxExtent(),
                Bounds->GetComponentQuat(),
                bCandidateValid ? FColor::Green : FColor::Red,
                false,
                0.0f,
                0,
                3.0f);
        }
    }

    AActor* Selected = SelectedActor.Get();
    const int32 RecordIndex = FindRecord(Selected);

    if (RecordIndex == INDEX_NONE)
    {
        return;
    }

    const ALES_PlaceableBase* SelectedPlaceable =
        Cast<ALES_PlaceableBase>(Selected);

    if (!IsValid(SelectedPlaceable))
    {
        return;
    }

    const UBoxComponent* Bounds =
        SelectedPlaceable->GetPlacementBounds();

    if (!IsValid(Bounds))
    {
        return;
    }

    DrawDebugBox(
        ActiveSession->GetWorld(),
        Bounds->GetComponentLocation(),
        Bounds->GetScaledBoxExtent() + FVector(2.0f),
        Bounds->GetComponentQuat(),
        FColor::Yellow,
        false,
        0.0f,
        0,
        3.0f);
}

void ULES_PlacementComponent::HandlePlacedActorDestroyed(AActor* DestroyedActor)
{
    if (SelectedActor.Get() == DestroyedActor)
    {
        SelectedActor.Reset();
    }

    RemoveInvalidRecords();
}
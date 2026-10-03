#include "KR26Au_MovingBox/Public/Core/Level/Content/LES_PlaceableBase.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ALES_PlaceableBase::ALES_PlaceableBase()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    VisualMesh =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(SceneRoot);

    LESPlacementBounds =
        CreateDefaultSubobject<UBoxComponent>(TEXT("LESPlacementBounds"));
    LESPlacementBounds->SetupAttachment(SceneRoot);

    LESPlacementBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LESPlacementBounds->SetGenerateOverlapEvents(false);
    LESPlacementBounds->SetHiddenInGame(true);

    // The base class guarantees inactive behavior by default.
    VisualMesh->SetSimulatePhysics(false);
}

void ALES_PlaceableBase::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    ApplyGameplayState(false);
}

bool ALES_PlaceableBase::GetLESPlacementData(
    FVector& OutPlacementHalfExtent,
    UStaticMesh*& OutPreviewMesh,
    FTransform& OutPreviewMeshLocalTransform,
    FString& OutReason) const
{
    OutPlacementHalfExtent = FVector::ZeroVector;
    OutPreviewMesh = nullptr;
    OutPreviewMeshLocalTransform = FTransform::Identity;

    if (!IsValid(VisualMesh))
    {
        OutReason = TEXT("Placeable base actor is missing VisualMesh.");
        return false;
    }

    if (!IsValid(LESPlacementBounds))
    {
        OutReason = TEXT("Placeable base actor is missing LESPlacementBounds.");
        return false;
    }

    UStaticMesh* Mesh = VisualMesh->GetStaticMesh();

    if (!IsValid(Mesh))
    {
        OutReason = TEXT("VisualMesh has no static mesh assigned.");
        return false;
    }

    const FTransform BoundsRelativeTransform =
        LESPlacementBounds->GetRelativeTransform();

    if (!BoundsRelativeTransform.GetLocation().IsNearlyZero(0.01f)
        || !BoundsRelativeTransform.GetRotation().Equals(
            FQuat::Identity,
            0.001f))
    {
        OutReason = TEXT(
            "LESPlacementBounds must be centered and unrotated.");

        return false;
    }

    const FVector HalfExtent = LESPlacementBounds->GetScaledBoxExtent();

    if (HalfExtent.GetMin() <= 0.0f)
    {
        OutReason = TEXT("LESPlacementBounds must have positive extents.");
        return false;
    }

    OutPlacementHalfExtent = HalfExtent;
    OutPreviewMesh = Mesh;
    OutPreviewMeshLocalTransform = VisualMesh->GetRelativeTransform();

    OutReason.Reset();
    return true;
}

void ALES_PlaceableBase::SetGameplayActive_Implementation(bool bActive)
{
    if (bGameplayActive == bActive)
    {
        return;
    }

    bGameplayActive = bActive;

    ApplyGameplayState(bGameplayActive);
    // HandleLESGameplayActiveChanged(bGameplayActive);
    OnLESGameplayActiveChanged(bGameplayActive);
}

void ALES_PlaceableBase::ApplyGameplayState(bool bActive)
{
    if (!IsValid(VisualMesh))
    {
        return;
    }

    /*
     * Keep query collision configuration under designer control. This is
     * important because inactive actors still need to be selectable and
     * considered during placement validation.
     */
    VisualMesh->SetSimulatePhysics(
        bActive && bSimulateVisualMeshDuringGameplay);

    if (!bActive)
    {
        VisualMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        VisualMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
}
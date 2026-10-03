#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Content/LES_Goal.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ALES_Goal::ALES_Goal()
{
	PrimaryActorTick.bCanEverTick = true;

	GoalBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GoalBounds"));
	SetRootComponent(GoalBounds);
	GoalBounds->SetBoxExtent(FVector(125.0, 125.0, 100.0));
	GoalBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GoalBounds->ShapeColor = FColor::Green;

	FloorMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMarker"));
	FloorMarker->SetupAttachment(GoalBounds);
	FloorMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorMarker->SetRelativeLocation(FVector(0.0, 0.0, -98.0));
	FloorMarker->SetRelativeScale3D(FVector(2.5, 2.5, 0.04));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	FloorMarker->SetStaticMesh(CubeMesh.Object);
}

void ALES_Goal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawGoal)
	{
		DrawDebugBox(
			GetWorld(),
			GoalBounds->GetComponentLocation(),
			GoalBounds->GetScaledBoxExtent(),
			GoalBounds->GetComponentQuat(),
			FColor::Green,
			false,
			0.0f,
			0,
			2.0f);
	}
}

bool ALES_Goal::ContainsWorldPoint(FVector WorldPoint) const
{
	const FVector LocalPoint = GoalBounds->GetComponentTransform().InverseTransformPosition(WorldPoint);

	const FVector Extent = GoalBounds->GetUnscaledBoxExtent();

	return FMath::Abs(LocalPoint.X) <= Extent.X
		&& FMath::Abs(LocalPoint.Y) <= Extent.Y
		&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
}
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_TestBlock.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ALES_TestBlock::ALES_TestBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(CubeMesh.Object);

	ActivationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ActivationMarker"));
	ActivationMarker->SetupAttachment(Mesh);
	ActivationMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ActivationMarker->SetRelativeLocation(FVector(0.0, 0.0, 65.0));
	ActivationMarker->SetRelativeScale3D(FVector(0.2));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ActivationMarker->SetStaticMesh(SphereMesh.Object);

	ActivationMarker->SetHiddenInGame(true);
}

void ALES_TestBlock::SetGameplayActive_Implementation(bool bActive)
{
	bGameplayActive = bActive;

	ActivationMarker->SetHiddenInGame(!bActive);
	Mesh->SetSimulatePhysics(bActive && bSimulateOnGameplay);

	if (bActive && bSimulateOnGameplay)
	{
		Mesh->WakeAllRigidBodies();
	}

	UE_LOG(LogTemp, Log, TEXT("[LES] %s gameplay active: %s"),
		*GetName(), bActive ? TEXT("true") : TEXT("false"));
}
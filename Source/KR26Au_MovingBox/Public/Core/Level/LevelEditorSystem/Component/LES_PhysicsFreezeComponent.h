#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LES_PhysicsFreezeComponent.generated.h"

class UPrimitiveComponent;

struct FLES_FrozenBody
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
};

/*
 * Captures and disables simulating components during editing, restores physics state when gameplay begins.
 */
UCLASS(ClassGroup=(LES), meta=(BlueprintSpawnableComponent))
class KR26AU_MOVINGBOX_API ULES_PhysicsFreezeComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	void FreezeWorldPhysics(UWorld* World);
	void RestoreWorldPhysics();

	bool HasFrozenBodies() const { return !FrozenBodies.IsEmpty(); }

private:
	TArray<FLES_FrozenBody> FrozenBodies;
};
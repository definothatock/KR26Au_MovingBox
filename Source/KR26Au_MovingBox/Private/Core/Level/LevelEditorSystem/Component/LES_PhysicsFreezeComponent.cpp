#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_PhysicsFreezeComponent.h"

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void ULES_PhysicsFreezeComponent::FreezeWorldPhysics(UWorld* World)
{
	FrozenBodies.Reset();

	if (!IsValid(World))
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components;
		It->GetComponents(Components);

		for (UPrimitiveComponent* Component : Components)
		{
			if (!IsValid(Component) || !Component->IsSimulatingPhysics())
			{
				continue;
			}

			FLES_FrozenBody& Body = FrozenBodies.AddDefaulted_GetRef();

			Body.Component = Component;
			Body.LinearVelocity = Component->GetPhysicsLinearVelocity();
			Body.AngularVelocity =
				Component->GetPhysicsAngularVelocityInDegrees();

			Component->SetSimulatePhysics(false);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LES] Froze %d physics components."), FrozenBodies.Num());
}

void ULES_PhysicsFreezeComponent::RestoreWorldPhysics()
{
	for (const FLES_FrozenBody& Body : FrozenBodies)
	{
		UPrimitiveComponent* Component = Body.Component.Get();

		if (!IsValid(Component))
		{
			continue;
		}

		Component->SetSimulatePhysics(true);
		Component->SetPhysicsLinearVelocity(Body.LinearVelocity);
		Component->SetPhysicsAngularVelocityInDegrees(
			Body.AngularVelocity);
		Component->WakeAllRigidBodies();
	}

	FrozenBodies.Reset();
}
#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/Component/LES_ObjectiveComponent.h"

#include "KR26Au_MovingBox/Public/Core/Level/COntent/LES_Goal.h"

ULES_ObjectiveComponent::ULES_ObjectiveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	UActorComponent::SetComponentTickEnabled(false);
}

bool ULES_ObjectiveComponent::StartMonitoring(
	AActor* InTarget,
	ALES_Goal* InGoal)
{
	if (!IsValid(InTarget) || !IsValid(InGoal))
	{
		return false;
	}

	TargetActor = InTarget;
	GoalActor = InGoal;

	HoldTime = 0.0f;
	bMonitoring = true;

	SetComponentTickEnabled(true);

	return true;
}

void ULES_ObjectiveComponent::StopMonitoring()
{
	TargetActor.Reset();
	GoalActor.Reset();

	HoldTime = 0.0f;
	bMonitoring = false;

	SetComponentTickEnabled(false);
}

void ULES_ObjectiveComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMonitoring)
	{
		return;
	}

	AActor* Target = TargetActor.Get();
	ALES_Goal* Goal = GoalActor.Get();

	if (!IsValid(Target) || !IsValid(Goal))
	{
		StopMonitoring();
		return;
	}

	if (!Goal->ContainsWorldPoint(Target->GetActorLocation()))
	{
		HoldTime = 0.0f;
		return;
	}

	HoldTime += DeltaTime;

	if (HoldTime >= Goal->GetRequiredHoldSeconds())
	{
		StopMonitoring();
		OnObjectiveCompleted.Broadcast();
	}
}
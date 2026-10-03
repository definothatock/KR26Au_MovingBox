#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LES_ObjectiveComponent.generated.h"

class ALES_Goal;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLESObjectiveCompleted);

/*
 * Runtime objective monitor for the gameplay phase.
 * 
 * Functions:
 * Success level: target remains inside the goal for a required duration.
 */
UCLASS(ClassGroup=(LES), meta=(BlueprintSpawnableComponent))
class KR26AU_MOVINGBOX_API ULES_ObjectiveComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	ULES_ObjectiveComponent();
	
	/* ==================== APIs ==================== */
	
	bool StartMonitoring(AActor* InTarget, ALES_Goal* InGoal);
	void StopMonitoring();

	/* ==================== Queries ==================== */
	
	bool IsMonitoring() const { return bMonitoring; }
	float GetHoldTime() const { return HoldTime; }

	/* ==================== Event ==================== */
	
	UPROPERTY(BlueprintAssignable, Category="LES")
	FLESObjectiveCompleted OnObjectiveCompleted;

	/* ==================== Internal Function ==================== */
	
protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/* ==================== Runtime State ==================== */
	
private:
	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<ALES_Goal> GoalActor;

	float HoldTime = 0.0f;
	bool bMonitoring = false;
};
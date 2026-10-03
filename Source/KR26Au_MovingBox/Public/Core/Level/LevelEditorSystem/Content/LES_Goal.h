#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LES_Goal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/*
 * Target-specific goal region.
 *
 * Rules:
 * - Detection uses target root position, not arbitrary overlapping actors.
 * - The session decides when detection is allowed.
 *
 * Boundary:
 * - Point containment, not full-object containment or swept detection.
 */
UCLASS(Blueprintable)
class KR26AU_MOVINGBOX_API ALES_Goal : public AActor
{
	GENERATED_BODY()

public:
	ALES_Goal();

	/* ==================== Overrides ==================== */

	virtual void Tick(float DeltaSeconds) override;

	/* ==================== Queries ==================== */

	UFUNCTION(BlueprintPure, Category="LES")
	bool ContainsWorldPoint(FVector WorldPoint) const;

	float GetRequiredHoldSeconds() const { return RequiredHoldSeconds; }

protected:
	/* ==================== Components ==================== */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
	TObjectPtr<UBoxComponent> GoalBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
	TObjectPtr<UStaticMeshComponent> FloorMarker;

	/* ==================== Config ==================== */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LES|Config", meta=(ClampMin="0.0"))
	float RequiredHoldSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LES|Debug")
	bool bDrawGoal = true;
};
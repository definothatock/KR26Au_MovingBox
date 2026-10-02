#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LES_PlaceableInterface.h"
#include "LES_TestBlock.generated.h"

class UStaticMeshComponent;

/*
 * Used for testing LES functionality only; Does nothing gameplay wise.
 */
UCLASS(Blueprintable)
class KR26AU_MOVINGBOX_API ALES_TestBlock
	: public AActor
	, public ILES_PlaceableInterface
{
	GENERATED_BODY()

public:
	ALES_TestBlock();

	/* ==================== Overrides ==================== */

	virtual void SetGameplayActive_Implementation(bool bActive) override;

	/* ==================== Queries ==================== */

	UFUNCTION(BlueprintPure, Category="LES")
	bool IsGameplayActive() const { return bGameplayActive; }

protected:
	/* ==================== Components ==================== */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LES")
	TObjectPtr<UStaticMeshComponent> ActivationMarker;

	/* ==================== Runtime State ==================== */

	UPROPERTY(Transient, BlueprintReadOnly, Category="LES")
	bool bGameplayActive = true;

	/* ==================== Config ==================== */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES|Config")
	bool bSimulateOnGameplay = true;
};
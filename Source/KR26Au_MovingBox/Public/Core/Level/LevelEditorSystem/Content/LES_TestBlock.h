#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interface/LES_PlaceableInterface.h"
#include "LES_TestBlock.generated.h"

class UStaticMeshComponent;

/**
 * Minimal interface example for exercising LES activation behavior.
 *
 * This actor implements ILES_PlaceableInterface directly, so activation
 * toggles its marker and can enable mesh physics. It does not inherit
 * ALES_PlaceableBase and therefore is not a placeable class for the current
 * ULES_PlaceableDefinition catalog.
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
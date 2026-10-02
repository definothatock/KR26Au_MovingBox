#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LES_PlaceableInterface.generated.h"

/*
 * Phase related interactions; for editor-placeable actors.
 *
 * Contract:
 * - Construction and BeginPlay must not start gameplay behavior.
 * - Inactive actors keep query collision for placement/selection.
 * - Gameplay activation is explicit and should be idempotent.
 *
 * TODO: Make a Placeable cpp to enforce the above.
 */
UINTERFACE(BlueprintType, Blueprintable)
class KR26AU_MOVINGBOX_API ULES_PlaceableInterface : public UInterface
{
	GENERATED_BODY()
};

class KR26AU_MOVINGBOX_API ILES_PlaceableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="LES")
	void SetGameplayActive(bool bActive);
};
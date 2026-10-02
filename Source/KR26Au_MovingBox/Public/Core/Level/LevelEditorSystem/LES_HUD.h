#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LES_HUD.generated.h"

class ALES_Session;

/* ==================== Declares ==================== */

constexpr float PanelEditModeHeight = 200.0f;
constexpr float PanelCharHeight = 22.0f;
constexpr float PanelPlayModeHeight = 100.0f;

/*
 * Text based prototype UI for LES; very barebone.
 *
 * Displays:
 * - Phase, entity catalog, quantities, selection, validity, and controls.
 * - Completion feedback and optional runtime debug information.
 */
UCLASS()
class KR26AU_MOVINGBOX_API ALES_HUD : public AHUD
{
	GENERATED_BODY()

public:
	/* ==================== Overrides ==================== */
	
	virtual void DrawHUD() override;

private:
	/* ==================== Runtime State ==================== */

	TWeakObjectPtr<ALES_Session> CachedSession;

	/* ==================== Config ==================== */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES|Debug",
		meta=(AllowPrivateAccess="true"))
	bool bShowDebug = true;
};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LES_HUD.generated.h"

class ALES_SessionManager;

/* ==================== Declares ==================== */

constexpr float PanelEditModeHeight = 200.0f;
constexpr float PanelCharHeight = 22.0f;
constexpr float PanelPlayModeHeight = 100.0f;

/*
 * Text based prototype UI for LES; very barebone.
 *
 * Function:
 * - Probe the entire world to get SessionManager and SessionConfigs.
 * - Displays verbose LES actions, usages, states.
 *
 * TODO:
 * - consider using delegate to communicate instead.
 */
UCLASS()
class KR26AU_MOVINGBOX_API ALES_HUD : public AHUD
{
	GENERATED_BODY()

public:
	/* ==================== Overrides ==================== */

	// ANCHOR: currently very inefficient. It asks the entire world for the SessionManager and LESConfig.
	virtual void DrawHUD() override;

private:
	/* ==================== Runtime State ==================== */

	TWeakObjectPtr<ALES_SessionManager> CachedSession;

	/* ==================== Config ==================== */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LES|Debug",
		meta=(AllowPrivateAccess="true"))
	bool bShowDebug = true;
};
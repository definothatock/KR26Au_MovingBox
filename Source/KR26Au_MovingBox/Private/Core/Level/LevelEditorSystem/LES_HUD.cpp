#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_HUD.h"

#include "KR26Au_MovingBox/Public/Core/Level/LevelEditorSystem/LES_SessionManager.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

void ALES_HUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas || !GEngine)
    {return;}

    if (!CachedSession.IsValid())
    {   // ANCHOR: inefficient
        for (TActorIterator<ALES_SessionManager> It(GetWorld()); It; ++It)
        {
            CachedSession = *It;
            break;
        }
    }

    ALES_SessionManager* Session = CachedSession.Get();

    if (!IsValid(Session))
    {return;}

    ULES_LevelEditorConfig* Config = Session->GetLevelConfig();

    const int32 EntryCount = Config ?
        Config->AvailableEntities.Num() : 0;

    const float PanelHeight =
        Session->GetPhase() == ELES_Phase::Editing ?
        PanelEditModeHeight + EntryCount * PanelCharHeight
        : PanelPlayModeHeight;

    DrawRect(
        FLinearColor(0.0f, 0.0f, 0.0f, 0.4f),
        10.0f,
        10.0f,
        700.0f,
        PanelHeight);

    float Y = 24.0f;

    auto Line = [this, &Y](
        const FString& Text,
        const FLinearColor& Color = FLinearColor::White)
    {
        DrawText(Text, Color, 24.0f, Y, GEngine->GetSmallFont(), 1.1f);
        Y += PanelCharHeight;
    };

    const FString PhaseName = StaticEnum<ELES_Phase>()->GetNameStringByValue(
        static_cast<int64>(Session->GetPhase()));

    Line(FString::Printf(TEXT("LEVEL EDITOR | %s"), *PhaseName));

    if (Session->GetPhase() == ELES_Phase::Editing && Config)
    {
        Line(TEXT("Available entities (Tab cycles):"), FLinearColor::Yellow);

        for (int32 Index = 0; Index < EntryCount; ++Index)
        {
            const FLES_LevelPlaceableEntry& Entry =
                Config->AvailableEntities[Index];
            const ULES_PlaceableDefinition& Definition = *Entry.Definition;

            const bool bSelected =
                Session->GetCurrentDefinition() == Index;

            Line(
                FString::Printf(
                    TEXT("%s %s | Remaining: %d / %d"),
                    bSelected ? TEXT(">") : TEXT(" "),
                    *Definition.DisplayName.ToString(),
                    Session->GetRemainingQuantity(Index),
                    Entry.Quantity),
                bSelected ? FLinearColor::Yellow : FLinearColor::White);
        }

        // ANCHOR: Change this once LES Editor Keys switched to IAM
        Line(TEXT("Left mouse: place / select | Right mouse: cancel preview | Tab: next type | R: rotate preview | M: move selected"));
        Line(TEXT("Delete: remove selected | Enter: finish editing | F5: reset level"));

        Line(
            FString::Printf(
                TEXT("Selected actor: %s"),
                *GetNameSafe(Session->GetSelectedActor())),
            FLinearColor::Yellow);

        const FLinearColor StatusColor =
            Session->HasPreview()
            ? (Session->IsPreviewValid()
                ? FLinearColor::Green
                : FLinearColor::Red)
            : FLinearColor::White;

        Line(Session->GetStatusText(), StatusColor);
    }
    else
    {
        Line(Session->GetStatusText());

        if (Session->GetPhase() == ELES_Phase::Completed)
        {
            Line(TEXT("SUCCESS"), FLinearColor::Green);
            Line(TEXT("F6: next level | F5: restart"));
        }
        else if (Session->GetPhase() == ELES_Phase::Gameplay)
        {
            Line(TEXT("Bring the Target to the Goal."));
        }
    }

    if (bShowDebug)
    {
        Line(
            FString::Printf(
                TEXT("Debug: Placed=%d | Preview=%s | Moving=%s"),
                Session->GetPlacedCount(),
                Session->HasPreview() ? TEXT("Yes") : TEXT("No"),
                Session->IsMovingSelection() ? TEXT("Yes") : TEXT("No")),
            FLinearColor(0.5f, 0.8f, 1.0f));
    }
}
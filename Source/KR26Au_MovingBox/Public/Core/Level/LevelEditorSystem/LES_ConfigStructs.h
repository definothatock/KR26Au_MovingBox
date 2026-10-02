#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LES_ConfigStructs.generated.h"

class AActor;
class UStaticMesh;
class UWorld;

/* ==================== Declares ==================== */

UENUM(BlueprintType)
enum class ELESPhase : uint8
{
    Waiting, // waiting for player setup
    Editing, // player editing the level
    Gameplay, // player playing the level
    Completed, // level finished
    Error
};



/*
 * Reusable entity metadata shared by multiple level configs.
 */
UCLASS(BlueprintType)
class KR26AU_MOVINGBOX_API ULESPlaceableDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES")
    FText DisplayName = FText::FromString(TEXT("Unnamed-during-Config!"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES", meta=(MustImplement="/Script/YourProject.LESPlaceableInterface"))
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES", meta=(ClampMin="1.0"))
    FVector HalfExtent = FVector(50.0);

    // Uses HalfExtent during preview; SHOULD use the same mesh as ActorClass (beware of transforms).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<UStaticMesh> PreviewMesh = nullptr;
};

USTRUCT(BlueprintType)
struct KR26AU_MOVINGBOX_API FLESLevelPlaceableEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES")
    TObjectPtr<ULESPlaceableDefinition> Definition = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES", meta=(ClampMin="0"))
    int32 Quantity = 1;
};


/*
 * Per-Level LES level Configs.
 *
 * Rules:
 * - Session Actor origin is placement plane center.
 * - Placeables use centered pivots, unit actor scale, and quarter-turn yaw.
 */
UCLASS(BlueprintType)
class KR26AU_MOVINGBOX_API ULES_ConfigStructs : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Area", meta=(ClampMin="1.0"))
    FVector AreaHalfExtent = FVector(1200.0, 1000.0, 300.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Placement", meta=(ClampMin="1.0"))
    float GridSize = 100.0f;

    // Small uplift from the placement floor.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Placement", meta=(ClampMin="0.0"))
    float FloorClearance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Placement")
    TArray<FLESLevelPlaceableEntry> AvailableEntities;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Flow")
    TSoftObjectPtr<UWorld> NextLevel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Debug")
    bool bDrawPlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LES|Debug")
    bool bDrawArea = true;
};
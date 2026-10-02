// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "InputActionValue.h"

#include "DefaultPlayer_Char.generated.h"

/* ==================== Declares ==================== */

class UInputMappingContext;
class UInputAction;

/**
 * 
 */
UCLASS()
class KR26AU_MOVINGBOX_API ADefaultPlayer_Char : public ACharacter
{
	GENERATED_BODY()

public:
	ADefaultPlayer_Char();

	/* ==================== Overrides ==================== */
public:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	
	/* ==================== Components ==================== */
	
	
	/* ==================== Input Assets ==================== */
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="InputAction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="InputAction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> MoveAction;
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="InputAction", meta=(AllowPrivateAccess="true"))
	// TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="InputAction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="InputAction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> LMBAction;

	
public:
	/* ==================== APIs ==================== */

	
	/* ==================== Queries ==================== */
	

	/* ==================== Internal Functions ==================== */
private:

	/* ----- Input Action ----- */
	
	void AddInputMappingContext(UInputMappingContext* ContextToAdd, int32 InPriority = 0);
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);
	// void HandleLMBInput(const FInputActionValue& Value);
	
	/* ==================== Config ==================== */
};

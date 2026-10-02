// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Player/DefaultPlayer_Char.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "InputAction.h"
#include "InputMappingContext.h"

ADefaultPlayer_Char::ADefaultPlayer_Char()
{
	/*
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true; // cant rotate when not using ControllerRot
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	*/
}


/* ==================== Overrides ==================== */


void ADefaultPlayer_Char::BeginPlay()
{
	Super::BeginPlay();

	AddInputMappingContext(DefaultMappingContext, 0);
}


void ADefaultPlayer_Char::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent) return;

	/*
	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	*/

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADefaultPlayer_Char::HandleMoveInput);
	}
	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADefaultPlayer_Char::HandleLookInput);
	}
	/*
	if (LMBAction)
	{
		EnhancedInputComponent->BindAction(LMBAction, ETriggerEvent::Triggered, this, &ADefaultPlayer_Char::HandleLMBInput);
	}
	*/
}


/* ==================== APIs ==================== */



/* ==================== Internal Functions ==================== */


void ADefaultPlayer_Char::AddInputMappingContext(UInputMappingContext* ContextToAdd, int32 InPriority)
{
	if (!ContextToAdd) return;

	if (APlayerController* PlyC = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LPlayer = PlyC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LPlayer))
			{
				Subsystem->AddMappingContext(ContextToAdd, InPriority);
			}
		}
	}
}

void ADefaultPlayer_Char::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!Controller) {return;}

	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector RightDirection   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}


void ADefaultPlayer_Char::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();
	if (!Controller) {return;}

	AddControllerYawInput(LookAxis.X);
	AddControllerPitchInput(-LookAxis.Y); // flight sim my ass
}

/*
void ADefaultPlayer_Char::HandleLMBInput(const FInputActionValue& Value)
{
	
}
*/

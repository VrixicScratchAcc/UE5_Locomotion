// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterviewLocomotionCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AInterviewLocomotionCharacter

const char* gMovementStateString[(uint8)EMovementState::MS_InAir + 1] = { ("MS_None"), ("MS_Grounded"), ("MS_InAir") };
const char* gJumpStateString[(uint8)EJumpState::JS_LandHigh + 1] = { ("JS_None"), ("JS_Start"), ("JS_Loop"), ("JS_LandHigh") };
const char* gCharacterStateString[(uint8)ECharacterState::CS_Sliding + 1] = { ("CS_None"), ("CS_Walking"), ("CS_Running"), ("CS_Crouching"), ("CS_Sliding") };

AInterviewLocomotionCharacter::AInterviewLocomotionCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	CharacterState = ECharacterState::CS_None;
	MovementState = EMovementState::MS_None;
	JumpState = EJumpState::JS_None;

	JumpSettings.bCanJump = true;
	JumpSettings.HighMinJumpVelocityZ = -600.0f;
	JumpSettings.JumpGroundTraceLength = 20.0f;

	SlideSettings.DeaccelerationRate = 0.1f;
	SlideSettings.SlideSpeed = 1000.0f;
}

void AInterviewLocomotionCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AInterviewLocomotionCharacter::Tick(float DeltaTime)
{
	ACharacter::Tick(DeltaTime);

	FVector& CharacterVelocity = GetCharacterMovement()->Velocity;
	GroundSpeed = FVector(CharacterVelocity.X, CharacterVelocity.Y, 0.0f).Length();

	// First we need to set the correct movement state
	UpdateMovementState();

	if (MovementState == EMovementState::MS_InAir)
	{
		if (CharacterVelocity.Z < JumpSettings.HighMinJumpVelocityZ && JumpState != EJumpState::JS_LandHigh)
		{
			const FVector ToGround = -GetCapsuleComponent()->GetUpVector();

			const FVector Start = GetCapsuleComponent()->GetComponentLocation() + (ToGround * GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			const FVector End = Start + (ToGround * JumpSettings.JumpGroundTraceLength);

			// Perform trace to retrieve hit info
			FCollisionQueryParams TraceParams(FName(TEXT("GroundTrace")), false, this);

			DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.25f);

			FHitResult Result;
			if (GetWorld()->LineTraceSingleByChannel(Result, Start, End, ECollisionChannel::ECC_WorldDynamic, TraceParams))
				TransitionJumpState(EJumpState::JS_LandHigh);
		}
	}
	else // Grounded
	{
		// Set the correct charater state 
		UpdateCharacterState();

		if (CharacterState == ECharacterState::CS_Sliding)
		{
			if (GroundSpeed < 10.0f && SlideSettings.SlideSpeed < 10.0f)
			{
				TransitionCharacterState(ECharacterState::CS_None);
			}
			else
			{
				SlideSettings.SlideSpeed -= SlideSettings.DeaccelerationRate;
				GetCharacterMovement()->AddInputVector(SlideSettings.SlideDirection * SlideSettings.SlideSpeed, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AInterviewLocomotionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AInterviewLocomotionCharacter::OnJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AInterviewLocomotionCharacter::OnJumpEnd);

		// Sliding
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &AInterviewLocomotionCharacter::OnSlideStart);
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Completed, this, &AInterviewLocomotionCharacter::OnSlideEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInterviewLocomotionCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInterviewLocomotionCharacter::Look);
	}
	else
	{
		//UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AInterviewLocomotionCharacter::UpdateMovementState()
{
	bool bIsInAir = GetMovementComponent()->IsFalling();
	if (bIsInAir)
	{
		TransitionMovementState(EMovementState::MS_InAir);
	}
	else
	{
		TransitionMovementState(EMovementState::MS_Grounded);
	}
}

void AInterviewLocomotionCharacter::UpdateCharacterState()
{
	if (CharacterState == ECharacterState::CS_Sliding)
	{
		return;
	}

	if (MovementState == EMovementState::MS_Grounded)
	{
		TransitionCharacterState(ECharacterState::CS_None);
		if (GroundSpeed > 150.0f)
			TransitionCharacterState(ECharacterState::CS_Running);
		else if (GroundSpeed > 0.0f)
			TransitionCharacterState(ECharacterState::CS_Walking);
	}
}

bool AInterviewLocomotionCharacter::TransitionMovementState(EMovementState inNewState)
{
	if (MovementState == EMovementState::MS_InAir && inNewState == EMovementState::MS_Grounded)
	{
		//UE_LOG(LogTemp, Warning, TEXT("MovementState[Old, New]: [%s, %s]"), *FString(gMovementStateString[(uint8)MovementState]), *FString(gMovementStateString[(uint8)inNewState]));
		if (JumpState != EJumpState::JS_LandHigh)
			TransitionJumpState(EJumpState::JS_None);
	}

	MovementState = inNewState;
	return true;
}

bool AInterviewLocomotionCharacter::TransitionCharacterState(ECharacterState inNewState)
{
	if (inNewState == ECharacterState::CS_Sliding)
	{
		if (CharacterState != ECharacterState::CS_Running || JumpState != EJumpState::JS_None)
			return false;
	}

	//UE_LOG(LogTemp, Warning, TEXT("CharacterState[Old, New]: [%s, %s]"), *FString(gCharacterStateString[(uint8)CharacterState]), *FString(gCharacterStateString[(uint8)inNewState]));
	CharacterState = inNewState;
	return true;
}

bool AInterviewLocomotionCharacter::TransitionJumpState(EJumpState inNewState)
{
	if (CharacterState == ECharacterState::CS_Sliding)
	{
		TransitionCharacterState(ECharacterState::CS_None);
		return false;
	}

	if (JumpState!= EJumpState::JS_None)
		if(inNewState == EJumpState::JS_None)
			JumpSettings.bCanJump = true;

	//UE_LOG(LogTemp, Warning, TEXT("JumpState[Old, New]: [%s, %s]"), *FString(gJumpStateString[(uint8)JumpState]), *FString(gJumpStateString[(uint8)inNewState]));
	JumpState = inNewState;
	return true;
}

void AInterviewLocomotionCharacter::OnJumpAnimStart()
{
	ACharacter::Jump();
}

void AInterviewLocomotionCharacter::OnJumpAnimEnded()
{
	JumpSettings.bCanJump = true; // Allow to jump again
	TransitionJumpState(EJumpState::JS_None);
}

void AInterviewLocomotionCharacter::OnJumpStart()
{
	if (JumpSettings.bCanJump)
	{
		if(TransitionJumpState(EJumpState::JS_Start))
			JumpSettings.bCanJump = false;
	}
}

void AInterviewLocomotionCharacter::OnJumpEnd()
{
	ACharacter::StopJumping();
}

void AInterviewLocomotionCharacter::OnSlideStart()
{
	if (TransitionCharacterState(ECharacterState::CS_Sliding))
	{
		SlideSettings.SlideSpeed = 1000.0f;
		SlideSettings.SlideDirection = GetCapsuleComponent()->GetForwardVector();
	}
}

void AInterviewLocomotionCharacter::OnSlideEnd()
{
}

void AInterviewLocomotionCharacter::Move(const FInputActionValue& Value)
{
	// Early exits for jump state
	if (JumpState != EJumpState::JS_None)
	{
		// Cannot move until animation stops...
		if (JumpState == EJumpState::JS_LandHigh)
			return;

		// Allow character to get off ground before moving..
		if (JumpState == EJumpState::JS_Start && GetCharacterMovement()->Velocity.Z < 100.0f)
		{
			return;
		}
	}

	// Early exit for charater state validation..
	if (CharacterState == ECharacterState::CS_Sliding)
		return;

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AInterviewLocomotionCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
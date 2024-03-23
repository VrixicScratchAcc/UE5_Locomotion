// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InterviewLocomotionCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
* Movement state is used to switch between different character states...
*/
UENUM(BlueprintType)
enum class EMovementState : uint8
{
	MS_None, // Error: something is wrong
	MS_Grounded,
	MS_InAir
};

/**
* 
*/
UENUM(BLueprintType)
enum class EJumpState : uint8
{
	JS_None = 0,
	JS_Start,
	JS_Loop,
	JS_LandLow, // Landed from a low height
	JS_LandHigh, // Landed from a high height
};

USTRUCT(BlueprintType)
struct FJumpSettings
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(VisibleAnywhere)
	bool bCanJump;

	UPROPERTY(EditAnywhere)
	float HighMinJumpVelocityZ;

	UPROPERTY(EditAnywhere)
	float JumpGroundTraceLength;
};

/**
* The state of the character, used for animations and overall gameplay...
*/
UENUM(BlueprintType)
enum class ECharacterState :uint8
{
	CS_Invalid = 0, // Something wrong has happened..
	CS_None, // Meaning standing or in air if jumping..
	CS_Walking,
	CS_Running,
	CS_Crouching,
	CS_Sliding,
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AInterviewLocomotionCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Current speed of character on the ground*/
	UPROPERTY(VisibleAnywhere, Category = CharacterMotion, meta = (AllowPrivateAccess = "true"))
	float GroundSpeed;

	// Current movement state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CharacterMotion, meta = (AllowPrivateAccess = "true"))
	EMovementState  MovementState;
	// Current character state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CharacterMotion, meta = (AllowPrivateAccess = "true"))
	ECharacterState CharacterState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CharacterMotion, meta = (AllowPrivateAccess = "true"))
	EJumpState JumpState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CharacterMotion, meta = (AllowPrivateAccess = "true"))
	FJumpSettings JumpSettings;

private:
	void UpdateMovementState();
	void UpdateCharacterState();

	/** Transitions the current movement state to a new movement state */
	void TransitionMovementState(EMovementState inNewState);

	/** Transitions the current characters state to a new character state */
	void TransitionCharacterState(ECharacterState inNewState);

	/** Transitions the current jump state to a new jump state */
	void TransitionJumpState(EJumpState inNewState);

	/** Called from BP when animation notify for jump start fires */
	UFUNCTION(BlueprintCallable, Category=CharacterAnimation)
	void OnJumpAnimStart();
	/** Fired when the jump land animation was completed */
	UFUNCTION(BlueprintCallable, Category=CharacterAnimation)
	void OnJumpAnimEnded();
	/** Actual jump functions for Character Movement. Binded to Input. */
	void OnJumpStart();
	void OnJumpEnd();

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
			

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay();

	virtual void Tick(float DeltaTime) override;

public:
	AInterviewLocomotionCharacter();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetGroundSpeed() const { return GroundSpeed; }
};


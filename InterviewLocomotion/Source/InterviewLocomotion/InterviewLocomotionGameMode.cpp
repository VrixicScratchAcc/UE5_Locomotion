// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterviewLocomotionGameMode.h"
#include "InterviewLocomotionCharacter.h"
#include "UObject/ConstructorHelpers.h"

AInterviewLocomotionGameMode::AInterviewLocomotionGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

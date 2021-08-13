// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIMotionGameMode.h"
#include "AIMotionCharacter.h"
#include "UObject/ConstructorHelpers.h"

AAIMotionGameMode::AAIMotionGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

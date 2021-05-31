// Copyright Epic Games, Inc. All Rights Reserved.

#include "LazerTagGameMode.h"
#include "LazerTagHUD.h"
#include "LazerTagCharacter.h"
#include "UObject/ConstructorHelpers.h"

ALazerTagGameMode::ALazerTagGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ALazerTagHUD::StaticClass();
}

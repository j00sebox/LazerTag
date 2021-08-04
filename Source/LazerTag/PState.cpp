// Fill out your copyright notice in the Description page of Project Settings.


#include "PState.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "SaveName.h"

APState::APState()
{
	bReplicates = true;
	/*if (USaveName* file = Cast<USaveName>(UGameplayStatics::LoadGameFromSlot(TEXT("Save0"), 0)))
	{
		playerName = file->savedName;
	}*/
}

void APState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APState, playerName);
	DOREPLIFETIME(APState, playerScore);
}

int APState::GetCurrentScore() const
{
	return playerScore;
}

FString APState::GetName() const
{
	return playerName;
}

void APState::UpdateScore(int delta)
{
	if (GetLocalRole() == ROLE_Authority)
	{
		playerScore += delta;

		UpdateLeaderBoardPos();
	}
}

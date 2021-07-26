// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PState.generated.h"

/**
 * 
 */
UCLASS()
class LAZERTAG_API APState : public APlayerState
{
	GENERATED_BODY()

public:

	APState();

	// required network setup
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(blueprintPure, category = "Score")
	int GetCurrentScore() const;

	UFUNCTION(blueprintPure, category = "Name")
	FString GetName() const;

	UFUNCTION(blueprintCallable, blueprintAuthorityOnly, category = "Score")
	void UpdateScore(int delta);

	UFUNCTION(blueprintImplementableEvent)
	void UpdateLeaderBoardPos();

protected:

	UPROPERTY(replicated, visibleAnywhere, blueprintReadWrite)
	FString playerName;

	UPROPERTY(replicated, visibleAnywhere, blueprintReadOnly)
	int playerScore;
	
};

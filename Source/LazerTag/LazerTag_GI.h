// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerController.h"
#include "LazerTag_GI.generated.h"


/**
 * 
 */
UCLASS()
class LAZERTAG_API ULazerTag_GI : public UGameInstance
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Names")
	TArray<FString> MapNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Modes")
	TArray<TSubclassOf<class AGameMode>> GameModes;

	UFUNCTION(blueprintCallable, Category = "Time")
	void SetTimeLimit(int limit);

	UFUNCTION(blueprintPure, Category = "Time")
	int GetTimeLimit() const;

	UFUNCTION(blueprintCallable, Category = "Score")
	void SetScoreLimit(int limit);

	UFUNCTION(blueprintPure, Category = "Score")
	int GetScoreLimit() const;

	UFUNCTION(blueprintCallable)
	void Travel(FString mapPath, FString gmPath, FString options);

protected:

	UPROPERTY(visibleAnywhere, blueprintReadOnly)
	int insScoreLimit;

	UPROPERTY(visibleAnywhere, blueprintReadOnly)
	int insTimeLimit;
	
};

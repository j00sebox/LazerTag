// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveName.generated.h"

/**
 * 
 */
UCLASS(blueprintable, blueprintType)
class LAZERTAG_API USaveName : public USaveGame
{
	GENERATED_BODY()

public:
	// required network setup
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& outLifetimeProps) const override;

	UPROPERTY(replicated, editAnywhere, blueprintReadWrite)
	FString savedName;
	
};

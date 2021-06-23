// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "Shield.generated.h"

/**
 * 
 */
UCLASS()
class LAZERTAG_API AShield : public APickup
{
	GENERATED_BODY()

public:

	AShield();

	// handle being pickedup
	void WasCollected_Implementation() override;
	
};

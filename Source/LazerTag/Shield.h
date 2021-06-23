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

	UFUNCTION(BlueprintAuthorityOnly, Category = "Pickup")
	void Server_PickedUpBy(APawn* Pawn) override;
	
};

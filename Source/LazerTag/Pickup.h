
#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "Pickup.generated.h"

/**
 * 
 */
UCLASS()
class LAZERTAG_API APickup : public AStaticMeshActor
{
	GENERATED_BODY()

public:

	// set default values
	APickup();

	// required net code
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// get the pickup state of this object
	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool IsActive();

	// set the state of the object, picked up or not
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void SetActive(bool newState);

	// function to call when pickup is collected
	UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
	void WasCollected();
	virtual void WasCollected_Implementation();

	// server handling of being picked up
	virtual void Server_PickedUpBy(APawn* Pawn);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Pickup")
	float f_lifeSpan = .5f;

protected:
	
	// true when object is pickedup
	UPROPERTY(Replicated)
	bool b_isActive;

	// called when b_isActive is updated
	UFUNCTION()
	virtual void OnRep_IsActive();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	APawn* pickupInsitgator;

private:

	// client handling of being picked up
	UFUNCTION(NetMulticast, Unreliable)
	void OnPickedUpBy(APawn* Pawn);
	void OnPickedUpBy_Implementation(APawn* Pawn);
	
};

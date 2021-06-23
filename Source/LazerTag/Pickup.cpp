
#include "Net/UnrealNetwork.h"
#include "Pickup.h"

APickup::APickup()
{
	// replicate actor
	bReplicates = true;

	// these don't need to tick every frame
	PrimaryActorTick.bCanEverTick = false;

	b_isActive = true;
}

void APickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickup, b_isActive);
}

bool APickup::IsActive()
{
	return b_isActive;
}

void APickup::SetActive(bool newState)
{
	// authority guard
	if (GetLocalRole() == ROLE_Authority)
	{
		b_isActive = newState;
	}	
}

void APickup::WasCollected_Implementation()
{
	// log debug message
	UE_LOG(LogClass, Log, TEXT("Pickup Collected %s"), *GetName());
}

void APickup::OnRep_PickUp()
{
	// TODO
}


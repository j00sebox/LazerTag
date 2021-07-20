#include "Pickup.h"
#include "Net/UnrealNetwork.h"

APickup::APickup()
{
	// replicate actor
	bReplicates = true;

	// these don't need to tick every frame
	PrimaryActorTick.bCanEverTick = false;

	// StaticMeshActorDisables overlap events by default
	GetStaticMeshComponent()->SetGenerateOverlapEvents(true);

	if (GetLocalRole() == ROLE_Authority)
	{
		b_isActive = true;
	}
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

void APickup::Server_PickedUpBy(APawn* Pawn)
{
	if ( GetLocalRole() == ROLE_Authority )
	{
		// store pawn that picked up the object on server side
		pickupInsitgator = Pawn;

		// broadcast to clients of the pickup event
		OnPickedUpBy(Pawn);
	}
}

void APickup::OnPickedUpBy_Implementation(APawn* Pawn)
{
	// store instigator on client side
	pickupInsitgator = Pawn;

	// trigger blueprint native event that cannot be replicated
	WasCollected();
}

void APickup::OnRep_IsActive()
{
	// TODO
}


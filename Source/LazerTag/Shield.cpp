
#include "Shield.h"


AShield::AShield()
{
	// keep movement synced from server to clinet
	SetReplicateMovement(true);

	// this pickup is physics enabled
	GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	
	// enable physics on the object
	GetStaticMeshComponent()->SetSimulatePhysics(true);
}

void AShield::WasCollected_Implementation()
{
	// allow parent class to handle first
	Super::WasCollected_Implementation();

	// destroy shield object
	Destroy();
}

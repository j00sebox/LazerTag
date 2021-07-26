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

void AShield::Server_PickedUpBy(APawn* Pawn)
{
	Super::Server_PickedUpBy(Pawn);

	if ( GetLocalRole() == ROLE_Authority )
	{
		// give client time to play effects
		SetLifeSpan(f_lifeSpan);
	}
}

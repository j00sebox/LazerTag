#include "LazerTag.h"
#include "SpawnVolume.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/BoxComponent.h"
#include "Pickup.h"

// Sets default values
ASpawnVolume::ASpawnVolume()
{
 	// do not nned to tick
	PrimaryActorTick.bCanEverTick = false;

	// only server should have access to this
	if (GetLocalRole() == ROLE_Authority)
	{
		m_spawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
		RootComponent = m_spawnArea;

		// default values for delay
		f_spawnDelayRangeLow = 1.f;
		f_spawnDelayRangeHigh = 4.5f;
	}
}

// Called when the game starts or when spawned
void ASpawnVolume::BeginPlay()
{
	Super::BeginPlay();
	
	// set timer to start spawning pickups
	Delay();
}

// Called every frame
void ASpawnVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FVector ASpawnVolume::GetSpawnablePoint()
{
	if (m_spawnArea != NULL)
	{
		return UKismetMathLibrary::RandomPointInBoundingBox(m_spawnArea->Bounds.Origin, m_spawnArea->Bounds.BoxExtent);
	}

	return FVector();
}

void ASpawnVolume::Delay()
{
	f_spawnDelay = FMath::FRandRange(f_spawnDelayRangeLow, f_spawnDelayRangeHigh);
	GetWorldTimerManager().SetTimer(spawnTimer, this, &ASpawnVolume::Spawn, f_spawnDelay, false);
}

void ASpawnVolume::Spawn()
{
	// only server can spawn new items
	if (GetLocalRole() == ROLE_Authority && m_spawnObject != NULL)
	{
		// check if the world is valid
		if (UWorld* const world = GetWorld())
		{
			// setup required params
			FActorSpawnParameters spawnParams;
			spawnParams.Owner = this;
			spawnParams.Instigator = GetInstigator();

			// set random rotation
			FRotator spawnRot;
			spawnRot.Yaw = FMath::FRand() * 360.f;
			spawnRot.Pitch = FMath::FRand() * 360.f;
			spawnRot.Roll = FMath::FRand() * 360.f;

			// place item in world
			APickup* const spawnedPickup = GetWorld()->SpawnActor<APickup>(m_spawnObject, GetSpawnablePoint(), spawnRot, spawnParams);

			Delay();
		}
	}
}


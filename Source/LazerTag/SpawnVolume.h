#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnVolume.generated.h"

class UBoxComponent;
class APickup;

UCLASS()
class LAZERTAG_API ASpawnVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASpawnVolume();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	FORCEINLINE UBoxComponent* GetSpawnArea() const { return m_spawnArea; }

	// get random point within spawn area
	UFUNCTION(BlueprintPure)
	FVector GetSpawnablePoint();

protected:

	// enforces what type of objects can be spawn in the volume
	UPROPERTY(EditAnywhere, Category = "Spawning")
	TSubclassOf<APickup> m_spawnObject;

	// access to timer
	FTimerHandle spawnTimer;

	// minimum spawn delay (seconds)
	UPROPERTY(EditAnywhere, BluePrintReadWrite, Category = "Spawning")
	float f_spawnDelayRangeLow;

	// maximum spawn delay (seconds)
	UPROPERTY(EditAnywhere, BluePrintReadWrite, Category = "Spawning")
	float f_spawnDelayRangeHigh;

private:

	// spawn area for pickups
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawning", Meta = (AllowPrivateAccess = "true"))
	UBoxComponent* m_spawnArea;

	// handles spawning new pickup object
	void Spawn();

	// starts the timer with a new delay value within the range
	void Delay();

	// actual spawn delay in seconds
	float f_spawnDelay;

};

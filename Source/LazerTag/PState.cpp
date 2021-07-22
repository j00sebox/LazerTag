// Fill out your copyright notice in the Description page of Project Settings.


#include "PState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "SaveName.h"

// Sets default values for this component's properties
UPState::UPState()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UPState::BeginPlay()
{
	Super::BeginPlay();

	if (USaveName* file = Cast<USaveName>(UGameplayStatics::LoadGameFromSlot(TEXT("Save0"), 0)))
	{
		_name = file->savedName;
	}
	
}


// Called every frame
void UPState::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


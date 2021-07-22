// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveName.h"
#include "Net/UnrealNetwork.h"

void USaveName::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USaveName, savedName);
}

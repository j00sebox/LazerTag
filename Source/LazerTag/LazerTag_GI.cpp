// Fill out your copyright notice in the Description page of Project Settings.


#include "LazerTag_GI.h"

void ULazerTag_GI::SetTimeLimit(int limit)
{
	insTimeLimit = limit;
}

int ULazerTag_GI::GetTimeLimit() const
{
	return insTimeLimit;
}

void ULazerTag_GI::SetScoreLimit(int limit)
{
	if (limit != 0 && limit < 10000)
	{
		insScoreLimit = limit;
	}
}

int ULazerTag_GI::GetScoreLimit() const
{
	return insScoreLimit;
}

void ULazerTag_GI::Travel(FString mapPath, FString gmPath, FString options)
{
	FString URL = mapPath;
	URL += "?Game=" + gmPath;
	URL += "?listen";

	GetWorld()->ServerTravel(URL);
}

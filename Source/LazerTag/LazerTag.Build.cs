// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LazerTag : ModuleRules
{
	public LazerTag(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "OnlineSubsystem", "OnlineSubsystemNull" });
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MultiServerReplicationEx : ModuleRules
{
	public MultiServerReplicationEx(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
                "CoreUObject",
                "CoreOnline",
                "Engine",
				"NetCore",
				"MultiServerConfigurationEx"
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"OnlineSubsystemUtils"
			}
		);
    }
}

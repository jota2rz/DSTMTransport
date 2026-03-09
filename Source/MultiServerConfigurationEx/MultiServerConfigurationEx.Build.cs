// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MultiServerConfigurationEx : ModuleRules
{
	public MultiServerConfigurationEx(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
                "CoreUObject"
            }
        );
    }
}

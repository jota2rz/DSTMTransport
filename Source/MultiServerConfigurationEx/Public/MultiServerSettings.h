// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MultiServerSettings.generated.h"

/**
 * Settings that control how a multi-server setup is configured and launched.
 * For example, to define 4 servers in a Game ini file it might look like this:
 * [/Script/MultiServerConfiguration.MultiServerSettings]
 * TotalNumServers=4
 */
UCLASS(Config=Game)
class MULTISERVERCONFIGURATIONEX_API UMultiServerSettings : public UObject
{
	GENERATED_BODY()

private:
	// This is read from the LaunchMultiServer script (not code). We take the runtime value from -MultiServerNumServers parameter.
	UPROPERTY(Config)
	int32 TotalNumServers = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

/** See MultiServerNode for more details about multi-server networking as a whole. */
class FMultiServerConfigurationExModule : public IModuleInterface
{
public:

	FMultiServerConfigurationExModule() {}
	virtual ~FMultiServerConfigurationExModule() {}

	// IModuleInterface
	virtual bool SupportsDynamicReloading() override { return false; }
	virtual bool SupportsAutomaticShutdown() override { return false; }
};



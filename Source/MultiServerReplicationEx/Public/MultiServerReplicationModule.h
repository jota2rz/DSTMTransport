// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#if UE_WITH_REMOTE_OBJECT_HANDLE
#include "UObject/RemoteObjectTransfer.h"
#include "UObject/RemoteObjectTypes.h"
#endif

/**
 * Module for Multi-server Replication Ex.
 *
 * Extends the stock MultiServerReplication plugin with DSTM transport support.
 *
 * Responsibilities:
 *   1. Initialize FRemoteServerId for this server instance (from -DedicatedServerId=)
 *   2. Pre-bind the DSTM transport delegates BEFORE InitRemoteObjects() runs,
 *      routing serialized migration data through the beacon mesh instead of disk I/O.
 *
 * Timing:
 *   StartupModule() runs during module load, before any UWorld is created.
 *   InitRemoteObjects() runs during world initialization.
 *   → Our delegate bindings win over the default disk-based fallbacks
 *     (RemoteObject.cpp checks !IsBound() before applying defaults).
 */
class FMultiServerReplicationExModule : public IModuleInterface
{
public:

	FMultiServerReplicationExModule() {}
	virtual ~FMultiServerReplicationExModule() {}

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override { return false; }
	virtual bool SupportsAutomaticShutdown() override { return false; }

	static FMultiServerReplicationExModule& Get();

private:
	/** Parse -DedicatedServerId= and call FRemoteServerId::InitGlobalServerId(). */
	void InitializeServerIdentity();

	/** Bind RemoteObjectTransferDelegate and RequestRemoteObjectDelegate. */
	void BindTransportDelegates();

#if UE_WITH_REMOTE_OBJECT_HANDLE
	/**
	 * Static callback for RemoteObjectTransferDelegate.
	 * Called by the engine when an object is ready to be sent to a remote server.
	 * Finds the DSTMSubsystem and routes serialized data through the beacon mesh.
	 */
	static void OnRemoteObjectTransfer(const UE::RemoteObject::Transfer::FMigrateSendParams& Params);

	/**
	 * Static callback for RequestRemoteObjectDelegate.
	 * Called when a server needs to pull an object from another server.
	 * Routes the request through the beacon mesh.
	 */
	static void OnRequestRemoteObject(
		FRemoteWorkPriority Priority,
		FRemoteObjectId ObjectId,
		FRemoteServerId LastKnownServerId,
		FRemoteServerId DestServerId);
#endif

	/** Whether we successfully initialized the server identity. */
	bool bServerIdentityInitialized = false;
};



// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSTMTransportModule.h"
#include "DSTMSubsystem.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if UE_WITH_REMOTE_OBJECT_HANDLE
#include "UObject/RemoteObjectTransfer.h"
#include "UObject/RemoteObjectTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDSTM, Log, All);

// ─── Module Interface ─────────────────────────────────────────────

void FDSTMTransportModule::StartupModule()
{
	UE_LOG(LogDSTM, Log, TEXT("DSTMTransport: StartupModule"));

	InitializeServerIdentity();
	BindTransportDelegates();
}

void FDSTMTransportModule::ShutdownModule()
{
	UE_LOG(LogDSTM, Log, TEXT("DSTMTransport: ShutdownModule"));

#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (bServerIdentityInitialized)
	{
		UE::RemoteObject::Transfer::RemoteObjectTransferDelegate.Unbind();
		UE::RemoteObject::Transfer::RequestRemoteObjectDelegate.Unbind();

		UE_LOG(LogDSTM, Log, TEXT("DSTMTransport: Transport delegates unbound."));
	}
#endif
}

FDSTMTransportModule& FDSTMTransportModule::Get()
{
	return FModuleManager::GetModuleChecked<FDSTMTransportModule>(TEXT("DSTMTransport"));
}

// ─── Server Identity ──────────────────────────────────────────────

void FDSTMTransportModule::InitializeServerIdentity()
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	// Parse the server ID from command line: -DedicatedServerId=server-1
	FString ServerIdStr;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-DedicatedServerId="), ServerIdStr, /*bShouldStopOnSeparator=*/false))
	{
		UE_LOG(LogDSTM, Log, TEXT("DSTMTransport: No -DedicatedServerId= on command line. "
			"Server identity not initialized (OK for clients/editor)."));
		return;
	}

	// FRemoteServerId is packed into a 10-bit field inside FRemoteObjectId
	// (REMOTE_OBJECT_SERVER_ID_BIT_SIZE = 10). The valid range is
	// [FirstValid(1) .. FirstReserved-1(1020)], giving 1020 usable IDs.
	// Hash the human-readable string into that range.
	const uint32 Hash = GetTypeHash(ServerIdStr);
	constexpr uint32 MinId = static_cast<uint32>(ERemoteServerIdConstants::FirstValid);  // 1
	constexpr uint32 MaxId = static_cast<uint32>(ERemoteServerIdConstants::FirstReserved); // 1021
	constexpr uint32 Range = MaxId - MinId;  // 1020
	const uint32 ServerId = (Hash % Range) + MinId;

	// InitGlobalServerId can only be called once — it asserts on re-initialization.
	// This must happen before any UObjects are allocated with remote object handles.
	FRemoteServerId::InitGlobalServerId(FRemoteServerId::FromIdNumber(ServerId));
	bServerIdentityInitialized = true;

	UE_LOG(LogDSTM, Log,
		TEXT("DSTMTransport: Server identity initialized — DedicatedServerId='%s' → FRemoteServerId=%u (hash %u mod %u + %u)"),
		*ServerIdStr, ServerId, Hash, Range, MinId);
#else
	UE_LOG(LogDSTM, Warning,
		TEXT("DSTMTransport: UE_WITH_REMOTE_OBJECT_HANDLE is disabled. "
			"DSTM transport requires an engine build with this define set to 1."));
#endif
}

// ─── Transport Delegate Binding ───────────────────────────────────

void FDSTMTransportModule::BindTransportDelegates()
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (!bServerIdentityInitialized)
	{
		UE_LOG(LogDSTM, Log,
			TEXT("DSTMTransport: Skipping delegate binding — server identity not initialized."));
		return;
	}

	// Bind the send delegate — called by the engine when an object migration
	// is ready to send. Our binding replaces the default disk I/O transport.
	// RemoteObject.cpp:317 checks !IsBound() before applying disk defaults,
	// so binding here (before InitRemoteObjects) ensures our transport wins.
	UE::RemoteObject::Transfer::RemoteObjectTransferDelegate.BindStatic(
		&FDSTMTransportModule::OnRemoteObjectTransfer);

	// Bind the request delegate — called when a server needs to pull an object
	// from a remote server (pull-migration).
	UE::RemoteObject::Transfer::RequestRemoteObjectDelegate.BindStatic(
		&FDSTMTransportModule::OnRequestRemoteObject);

	UE_LOG(LogDSTM, Log,
		TEXT("DSTMTransport: Transport delegates bound (beacon-based, replacing disk I/O)."));
#endif
}

// ─── Static Transport Callbacks ───────────────────────────────────

#if UE_WITH_REMOTE_OBJECT_HANDLE

/**
 * Find the DSTMSubsystem in any active game instance.
 * Works on dedicated servers where GameViewport may not exist.
 * Caches the result to avoid iterating world contexts on every delegate call.
 * Note: Called exclusively from the game thread (engine transport delegates).
 * The TWeakObjectPtr self-invalidates when the subsystem is GC'd.
 */
static UDSTMSubsystem* FindDSTMSubsystem()
{
	static TWeakObjectPtr<UDSTMSubsystem> CachedSubsystem;

	if (CachedSubsystem.IsValid())
	{
		return CachedSubsystem.Get();
	}

	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UGameInstance* GI = Context.OwningGameInstance;
		if (GI)
		{
			UDSTMSubsystem* Sub = GI->GetSubsystem<UDSTMSubsystem>();
			if (Sub)
			{
				CachedSubsystem = Sub;
				return Sub;
			}
		}
	}

	return nullptr;
}

void FDSTMTransportModule::OnRemoteObjectTransfer(
	const UE::RemoteObject::Transfer::FMigrateSendParams& Params)
{
	UDSTMSubsystem* Sub = FindDSTMSubsystem();
	if (Sub)
	{
		Sub->HandleOutgoingMigration(Params);
	}
	else
	{
		UE_LOG(LogDSTM, Error,
			TEXT("DSTMTransport: RemoteObjectTransferDelegate fired but no DSTMSubsystem found! "
				"Migration data will be lost."));
	}
}

void FDSTMTransportModule::OnRequestRemoteObject(
	FRemoteWorkPriority Priority,
	FRemoteObjectId ObjectId,
	FRemoteServerId LastKnownServerId,
	FRemoteServerId DestServerId)
{
	UDSTMSubsystem* Sub = FindDSTMSubsystem();
	if (Sub)
	{
		Sub->HandleObjectRequest(Priority, ObjectId, LastKnownServerId, DestServerId);
	}
	else
	{
		UE_LOG(LogDSTM, Error,
			TEXT("DSTMTransport: RequestRemoteObjectDelegate fired but no DSTMSubsystem found!"));
	}
}

#endif // UE_WITH_REMOTE_OBJECT_HANDLE

// ─── Module Registration ──────────────────────────────────────────

IMPLEMENT_MODULE(FDSTMTransportModule, DSTMTransport)

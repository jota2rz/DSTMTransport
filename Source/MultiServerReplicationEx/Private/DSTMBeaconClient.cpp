// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSTMBeaconClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DSTMBeaconClient)

DEFINE_LOG_CATEGORY_STATIC(LogDSTMBeacon, Log, All);

// ─── Constructor ──────────────────────────────────────────────────

ADSTMBeaconClient::ADSTMBeaconClient()
{
}

// ─── Connection ───────────────────────────────────────────────────

void ADSTMBeaconClient::OnConnected()
{
	Super::OnConnected();

	UE_LOG(LogDSTMBeacon, Log,
		TEXT("DSTMBeacon: Connected to peer %s (local=%s) — DSTM transport ready"),
		*GetRemotePeerId(), *GetLocalPeerId());
}

// ─── Migration Data Transfer RPCs ─────────────────────────────────

void ADSTMBeaconClient::ServerReceiveMigratedObject_Implementation(
	uint64 ObjectIdRaw,
	uint32 OwnerServerIdRaw,
	uint32 PhysicsServerIdRaw,
	uint32 PhysicsLocalIslandId,
	uint32 SenderServerIdRaw,
	const TArray<uint8>& SerializedData)
{
	UE_LOG(LogDSTMBeacon, Log,
		TEXT("DSTM Recv [Server RPC]: ObjectId=%llu, Owner=%u, Physics=%u, Sender=%u, DataSize=%d bytes"),
		ObjectIdRaw, OwnerServerIdRaw, PhysicsServerIdRaw, SenderServerIdRaw, SerializedData.Num());

	OnMigrationDataReceived.Broadcast(
		ObjectIdRaw, OwnerServerIdRaw, PhysicsServerIdRaw,
		PhysicsLocalIslandId, SenderServerIdRaw, SerializedData);
}

void ADSTMBeaconClient::ClientReceiveMigratedObject_Implementation(
	uint64 ObjectIdRaw,
	uint32 OwnerServerIdRaw,
	uint32 PhysicsServerIdRaw,
	uint32 PhysicsLocalIslandId,
	uint32 SenderServerIdRaw,
	const TArray<uint8>& SerializedData)
{
	UE_LOG(LogDSTMBeacon, Log,
		TEXT("DSTM Recv [Client RPC]: ObjectId=%llu, Owner=%u, Physics=%u, Sender=%u, DataSize=%d bytes"),
		ObjectIdRaw, OwnerServerIdRaw, PhysicsServerIdRaw, SenderServerIdRaw, SerializedData.Num());

	OnMigrationDataReceived.Broadcast(
		ObjectIdRaw, OwnerServerIdRaw, PhysicsServerIdRaw,
		PhysicsLocalIslandId, SenderServerIdRaw, SerializedData);
}

// ─── Pull-Migration Request RPCs ──────────────────────────────────

bool ADSTMBeaconClient::ServerRequestMigrateObject_Validate(
	uint64 ObjectIdRaw,
	uint32 RequestingServerIdRaw)
{
	// Basic validation: object ID should be non-zero
	return ObjectIdRaw != 0;
}

void ADSTMBeaconClient::ServerRequestMigrateObject_Implementation(
	uint64 ObjectIdRaw,
	uint32 RequestingServerIdRaw)
{
	UE_LOG(LogDSTMBeacon, Log,
		TEXT("DSTM Request [Server RPC]: ObjectId=%llu requested by server %u"),
		ObjectIdRaw, RequestingServerIdRaw);

	OnMigrationRequested.Broadcast(ObjectIdRaw, RequestingServerIdRaw);
}

void ADSTMBeaconClient::ClientRequestMigrateObject_Implementation(
	uint64 ObjectIdRaw,
	uint32 RequestingServerIdRaw)
{
	UE_LOG(LogDSTMBeacon, Log,
		TEXT("DSTM Request [Client RPC]: ObjectId=%llu requested by server %u"),
		ObjectIdRaw, RequestingServerIdRaw);

	OnMigrationRequested.Broadcast(ObjectIdRaw, RequestingServerIdRaw);
}

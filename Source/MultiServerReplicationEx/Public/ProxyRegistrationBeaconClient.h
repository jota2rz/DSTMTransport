// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineBeaconClient.h"
#include "ProxyRegistrationBeaconClient.generated.h"

/**
 * Beacon client used by game servers to register themselves with a proxy.
 *
 * The game server address is passed to the proxy via GetLoginOptions(),
 * which injects it into the NMT_Login URL. The proxy extracts it from
 * Connection->RequestURL in OnClientConnected and calls
 * RegisterGameServerAndConnectClients() — the same code path used by
 * the static -ProxyGameServers= registration.
 *
 * This avoids relying on Client/Server RPCs, which require actor
 * replication that doesn't work on the proxy's non-standard net driver.
 */
UCLASS(transient, notplaceable)
class MULTISERVERREPLICATIONEX_API AProxyRegistrationBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()

public:
	AProxyRegistrationBeaconClient();

	/** Set the address (host:port) this game server is listening on. Must be called before connecting. */
	void SetGameServerAddress(const FString& InAddress) { GameServerAddress = InAddress; }

	//~ Begin AOnlineBeaconClient interface
	virtual FString GetLoginOptions(const FUniqueNetIdRepl& PlayerId) override;
	virtual void OnConnected() override;
	virtual void OnFailure() override;
	//~ End AOnlineBeaconClient interface

private:
	/** The host:port address this game server can be reached at. */
	FString GameServerAddress;
};

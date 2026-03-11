// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineBeaconHostObject.h"
#include "OnlineBeaconHost.h"
#include "ProxyRegistrationBeaconHostObject.generated.h"

class AProxyRegistrationBeaconClient;

/**
 * Beacon host subclass that enables the NMT_Login handshake flow.
 *
 * By default AOnlineBeaconHost skips NMT_Login when bAuthRequired is false,
 * which means Connection->RequestURL is never populated. We need it
 * so the game server can pass its listen address via GetLoginOptions().
 */
UCLASS(transient, notplaceable)
class AProxyRegistrationBeaconHost : public AOnlineBeaconHost
{
	GENERATED_BODY()
public:
	AProxyRegistrationBeaconHost();

	virtual bool StartVerifyAuthentication(
		const FUniqueNetId& PlayerId,
		const FString& LoginOptions,
		const FString& AuthenticationToken,
		const FOnAuthenticationVerificationCompleteDelegate& OnComplete) override;
};

/**
 * Beacon host object that runs on the proxy server.
 *
 * When a game server's AProxyRegistrationBeaconClient connects and sends its
 * address, this object looks up the world's UProxyNetDriver and calls
 * RegisterGameServerAndConnectClients() — the same code path used by the
 * static -ProxyGameServers= command-line registration.
 */
UCLASS(transient, notplaceable)
class AProxyRegistrationBeaconHostObject : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	AProxyRegistrationBeaconHostObject(const FObjectInitializer& ObjectInitializer);

	//~ Begin AOnlineBeaconHostObject interface
	virtual void OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection) override;
	virtual void NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor) override;
	//~ End AOnlineBeaconHostObject interface

	/** Called by AProxyRegistrationBeaconClient::ServerRegisterGameServer_Implementation. */
	void HandleGameServerRegistration(const FString& GameServerAddress);
};

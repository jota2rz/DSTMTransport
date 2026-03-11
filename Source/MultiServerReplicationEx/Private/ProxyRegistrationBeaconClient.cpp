// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyRegistrationBeaconClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProxyRegistrationBeaconClient)

DEFINE_LOG_CATEGORY_STATIC(LogProxyRegistrationClient, Log, All);

AProxyRegistrationBeaconClient::AProxyRegistrationBeaconClient()
{
}

FString AProxyRegistrationBeaconClient::GetLoginOptions(const FUniqueNetIdRepl& PlayerId)
{
	// Append game server address to the login URL so the proxy can extract it
	// from Connection->RequestURL when OnClientConnected fires.
	FString Base = Super::GetLoginOptions(PlayerId);
	if (!GameServerAddress.IsEmpty())
	{
		Base += FString::Printf(TEXT("?GameServerAddress=%s"), *GameServerAddress);
	}
	return Base;
}

void AProxyRegistrationBeaconClient::OnConnected()
{
	Super::OnConnected();
	UE_LOG(LogProxyRegistrationClient, Log, TEXT("Connected to proxy registration beacon (address: %s)"), *GameServerAddress);
}

void AProxyRegistrationBeaconClient::OnFailure()
{
	UE_LOG(LogProxyRegistrationClient, Error, TEXT("Failed to connect to proxy registration beacon"));
	Super::OnFailure();
}

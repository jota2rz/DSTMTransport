// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyRegistrationBeaconHostObject.h"
#include "ProxyRegistrationBeaconClient.h"
#include "MultiServerProxy.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "OnlineError.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProxyRegistrationBeaconHostObject)

DEFINE_LOG_CATEGORY_STATIC(LogProxyRegistrationHost, Log, All);

// ─── AProxyRegistrationBeaconHost ────────────────────────────────────────────

AProxyRegistrationBeaconHost::AProxyRegistrationBeaconHost()
{
	bAuthRequired = true;
}

bool AProxyRegistrationBeaconHost::StartVerifyAuthentication(
	const FUniqueNetId& PlayerId,
	const FString& LoginOptions,
	const FString& AuthenticationToken,
	const FOnAuthenticationVerificationCompleteDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(FOnlineError::Success());
	return true;
}

// ─── AProxyRegistrationBeaconHostObject ──────────────────────────────────────

AProxyRegistrationBeaconHostObject::AProxyRegistrationBeaconHostObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ClientBeaconActorClass = AProxyRegistrationBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}

void AProxyRegistrationBeaconHostObject::OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	Super::OnClientConnected(NewClientActor, ClientConnection);

	UE_LOG(LogProxyRegistrationHost, Log, TEXT("Game server beacon client connected (%d total)"), ClientActors.Num());

	// Extract the game server address from the login URL that the beacon client
	// injected via GetLoginOptions(). The beacon host stored it in
	// Connection->RequestURL during NMT_Login handling.
	// ParseOption expects the string to start with '?' so extract options portion.
	if (ClientConnection)
	{
		const int32 QMark = ClientConnection->RequestURL.Find(TEXT("?"));
		const FString Options = (QMark != INDEX_NONE) ? ClientConnection->RequestURL.Mid(QMark) : FString();
		const FString GameServerAddress = UGameplayStatics::ParseOption(Options, TEXT("GameServerAddress"));
		if (!GameServerAddress.IsEmpty())
		{
			HandleGameServerRegistration(GameServerAddress);
		}
		else
		{
			UE_LOG(LogProxyRegistrationHost, Error, TEXT("No GameServerAddress in beacon login URL: %s"), *ClientConnection->RequestURL);
		}
	}
}

void AProxyRegistrationBeaconHostObject::NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor)
{
	Super::NotifyClientDisconnected(LeavingClientActor);

	UE_LOG(LogProxyRegistrationHost, Log, TEXT("Game server beacon client disconnected (%d remaining)"), ClientActors.Num());
}

void AProxyRegistrationBeaconHostObject::HandleGameServerRegistration(const FString& GameServerAddress)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("No world — cannot register game server"));
		return;
	}

	UProxyNetDriver* ProxyDriver = Cast<UProxyNetDriver>(World->GetNetDriver());
	if (!ProxyDriver)
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("World net driver is not UProxyNetDriver — cannot register game server"));
		return;
	}

	FURL GameServerURL(nullptr, *GameServerAddress, ETravelType::TRAVEL_Absolute);
	if (!GameServerURL.Valid)
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("Invalid game server URL: %s"), *GameServerAddress);
		return;
	}

	UE_LOG(LogProxyRegistrationHost, Log, TEXT("Registering game server via beacon: %s"), *GameServerAddress);
	ProxyDriver->RegisterGameServerAndConnectClients(GameServerURL);
}

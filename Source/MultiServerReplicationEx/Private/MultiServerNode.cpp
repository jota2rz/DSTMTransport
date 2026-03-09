// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiServerNode.h"
#include "MultiServerBeaconHost.h"
#include "MultiServerBeaconHostObject.h"
#include "MultiServerBeaconClient.h"
#include "MultiServerPeerConnection.h"
#include "MultiServerReplicationTypes.h"
#include "Algo/Sort.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MultiServerNode)

void UMultiServerNode::ParseCommandLineIntoCreateParams(FMultiServerNodeCreateParams& InOutParams)
{
	FParse::Value(FCommandLine::Get(), TEXT("MultiServerLocalId="), InOutParams.LocalPeerId);
	FParse::Value(FCommandLine::Get(), TEXT("MultiServerListenPort="), InOutParams.ListenPort);

	FString PeerAddressesString;
	FParse::Value(FCommandLine::Get(), TEXT("MultiServerPeers="), PeerAddressesString, false);

	PeerAddressesString.ParseIntoArray(InOutParams.PeerAddresses, TEXT(","), true);

	FParse::Value(FCommandLine::Get(), TEXT("MultiServerNumServers="), InOutParams.NumServers);
	if (InOutParams.NumServers < 1)
	{
		InOutParams.NumServers = InOutParams.PeerAddresses.Num();
	}
}

UMultiServerNode::UMultiServerNode()
	: UObject()
{
	RetryConnectDelay = 0.5f;
	RetryConnectMaxDelay = 30.0f;
}

bool UMultiServerNode::AreAllServersConnected() const
{
	if (NumExpectedServers <= 1)
	{
		return true;
	}

	// We are doing a bit more logic than "connected" and instead ensuring the Peer ID has been exchanged
	uint32 NumAcknowledgedPeers = 0;
	ForEachBeaconClient([&NumAcknowledgedPeers](AMultiServerBeaconClient* BeaconClient)
	{
		if (!BeaconClient->GetRemotePeerId().IsEmpty())
		{
			NumAcknowledgedPeers++;
		}
	});

	const bool bAllPeersAreAcknowledged = NumAcknowledgedPeers >= (NumExpectedServers - 1);
	return bAllPeersAreAcknowledged;
}

UMultiServerNode* UMultiServerNode::Create(const FMultiServerNodeCreateParams& Params)
{
	UMultiServerNode* NewNode = NewObject<UMultiServerNode>(Params.World);
	const bool bResult = NewNode->RegisterServer(Params);

	if (bResult)
	{
		NewNode->NumExpectedServers = Params.NumServers;
		NewNode->RegisterTickEvents();
		return NewNode;
	}
	else
	{
		// Is this the best way to clean up on failed registration?
		NewNode->MarkAsGarbage();
	}
	
	return nullptr;
}

void UMultiServerNode::BeginDestroy()
{
	UnregisterTickEvents();

	Super::BeginDestroy();
}

bool UMultiServerNode::RegisterServer(const FMultiServerNodeCreateParams& Params)
{
	if (Params.World == nullptr)
	{
		UE_LOG(LogMultiServerReplication, Warning, TEXT("UMultiServerNode::RegisterServer: null world - failed to register."));
		return false;
	}

	if (Params.LocalPeerId.IsEmpty())
	{
		UE_LOG(LogMultiServerReplication, Warning, TEXT("UMultiServerNode::RegisterServer: no MultiServerLocalId specified - required for multiserver to work properly."));
		return false;
	}

	LocalPeerId = Params.LocalPeerId;
	UserBeaconClass = Params.UserBeaconClass;

	OnMultiServerConnected = Params.OnMultiServerConnected;

	if (Params.ListenPort == 0)
	{
		UE_LOG(LogMultiServerReplication, Log, TEXT("UMultiServerNode::RegisterServer: no listen port specified, not listening."));
	}
	else
	{
		UE_LOG(LogMultiServerReplication, Log, TEXT("UMultiServerNode::RegisterServer: setting up host beacon for %s."), ToCStr(LocalPeerId));

		// Set up host beacon
		if (ensureMsgf(BeaconHost == nullptr, TEXT("UMultiServerNode::RegisterServer: BeaconHost already created.")))
		{
			// Always create a new beacon host, state will be determined in a moment
			BeaconHost = Params.World->SpawnActor<AMultiServerBeaconHost>(AMultiServerBeaconHost::StaticClass());
			check(BeaconHost);

			BeaconHost->ListenPort = Params.ListenPort;

			bool bBeaconInit = false;
			if (BeaconHost->InitHost())
			{ 
				BeaconHostObject = Params.World->SpawnActor<AMultiServerBeaconHostObject>(AMultiServerBeaconHostObject::StaticClass());
				check(BeaconHostObject);

				BeaconHostObject->SetClientBeaconActorClass(Params.UserBeaconClass);
				BeaconHostObject->SetOwningNode(this);

				BeaconHost->RegisterHost(BeaconHostObject);
				BeaconHost->PauseBeaconRequests(false);
			}
			else
			{
				UE_LOG(LogMultiServerReplication, Warning, TEXT("Failed to init multiserver host beacon %s"), *BeaconHost->GetName());
				return false;
			}
		}
	}

	// If the -MultiServerPeers command-line option is set, start a client beacon for each one and connect to them.
	// (they are expected to be listening already)
	if (Params.PeerAddresses.Num() == 0)
	{
		UE_LOG(LogMultiServerReplication, Log, TEXT("UMultiServerNode::RegisterServer: no peers specified, not connecting to any. LocalPeerId %s"), ToCStr(LocalPeerId));
	}
	else
	{
		// Assume every server wil receive the same list of addresses. Sort it so all servers can deterministically figure out who else they need to connect to.
		TArray<FString> SortedPeerAddresses = Params.PeerAddresses;
		Algo::Sort(SortedPeerAddresses);

		// If our local IP wasn't passed in, assume localhost.
		FString ListenIp = Params.ListenIp.IsEmpty() ? TEXT("127.0.0.1") : Params.ListenIp;

		for (const FString& PeerAddress : SortedPeerAddresses)
		{
			FURL PeerURL(nullptr, ToCStr(PeerAddress), ETravelType::TRAVEL_Absolute);
			
			if (!PeerURL.Valid)
			{
				UE_LOG(LogMultiServerReplication, Verbose, TEXT("Failed to parse peer address %s, not connecting."), ToCStr(PeerAddress));
				continue;
			}

			// Only connect to peers that are before this instance in the list to prevent redundant connections.
			if (PeerURL.Host.Equals(ListenIp) && PeerURL.Port == Params.ListenPort)
			{
				break;
			}

			if (!PeerAddress.IsEmpty())
			{
				UMultiServerPeerConnection* Peer = NewObject<UMultiServerPeerConnection>(this);
				Peer->SetOwningNode(this);
				Peer->SetRemoteAddress(PeerAddress);
				Peer->SetLocalPeerId(LocalPeerId);
				Peer->InitClientBeacon();
				PeerConnections.Add(Peer);
			}
		}
	}

	return true;
}

void UMultiServerNode::RegisterTickEvents()
{
	UWorld* World = GetWorld();
	if (World)
	{
		TickDispatchDelegateHandle = World->OnTickDispatch().AddUObject(this, &UMultiServerNode::InternalTickDispatch);
		TickFlushDelegateHandle = World->OnTickFlush().AddUObject(this, &UMultiServerNode::InternalTickFlush);
	}
}

void UMultiServerNode::UnregisterTickEvents()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->OnTickDispatch().Remove(TickDispatchDelegateHandle);
		World->OnTickFlush().Remove(TickFlushDelegateHandle);
	}
}

void UMultiServerNode::InternalTickDispatch(float DeltaSeconds)
{
	ForEachNetDriver([DeltaSeconds](UNetDriver* NetDriver)
		{
			if (NetDriver->GetWorld())
			{
				NetDriver->TickDispatch(DeltaSeconds);
				NetDriver->PostTickDispatch();
			}
		});
}

void UMultiServerNode::InternalTickFlush(float DeltaSeconds)
{
	ForEachNetDriver([DeltaSeconds](UNetDriver* NetDriver)
		{
			if (NetDriver->GetWorld())
			{
				NetDriver->TickFlush(DeltaSeconds);
				NetDriver->PostTickFlush();
			}
		});
}

AMultiServerBeaconClient* UMultiServerNode::GetBeaconClientForRemotePeer(FStringView RemotePeerId)
{
	// See if we are the host of the target server
	UNetDriver* HostNetDriver = BeaconHost ? BeaconHost->GetNetDriver() : nullptr;
	if (HostNetDriver)
	{
		for (UNetConnection* ClientConnection : HostNetDriver->ClientConnections)
		{
			AMultiServerBeaconClient* BeaconClient = Cast<AMultiServerBeaconClient>(BeaconHost->GetClientActor(ClientConnection));
			if (BeaconClient)
			{
				if (RemotePeerId.Equals(BeaconClient->GetRemotePeerId(), ESearchCase::IgnoreCase))
				{
					return BeaconClient;
				}
			}
		}
	}

	// See if we are a client of the target server
	for (UMultiServerPeerConnection* Peer : PeerConnections)
	{
		AMultiServerBeaconClient* Beacon = Peer->BeaconClient;
		if (Beacon)
		{
			if (RemotePeerId.Equals(Beacon->GetRemotePeerId(), ESearchCase::IgnoreCase))
			{
				return Beacon;
			}
		}
	}

	return nullptr;
}

AMultiServerBeaconClient* UMultiServerNode::GetBeaconClientForURL(const FString& InURL)
{
	FURL URL(nullptr, ToCStr(InURL), TRAVEL_Absolute);

	// See if we are the host of the target server
	UNetDriver* HostNetDriver = BeaconHost ? BeaconHost->GetNetDriver() : nullptr;
	if (HostNetDriver)
	{
		for (UNetConnection* ClientConnection : HostNetDriver->ClientConnections)
		{
			if (ClientConnection &&
				ClientConnection->URL.Host.Equals(URL.Host, ESearchCase::IgnoreCase) &&
				ClientConnection->URL.Port == URL.Port)
			{
				return Cast<AMultiServerBeaconClient>(BeaconHost->GetClientActor(ClientConnection));
			}
		}
	}

	// See if we are a client of the target server
	for (UMultiServerPeerConnection* Peer : PeerConnections)
	{
		AMultiServerBeaconClient* Beacon = Peer->BeaconClient;
		if (Beacon)
		{
			UNetConnection* ClientConnection = Beacon->GetNetConnection();
			if (ClientConnection &&
				ClientConnection->URL.Host.Equals(URL.Host, ESearchCase::IgnoreCase) &&
				ClientConnection->URL.Port == URL.Port)
			{
				return Beacon;
			}
		}
	}

	return nullptr;
}

void UMultiServerNode::ForEachBeaconClient(TFunctionRef<void(AMultiServerBeaconClient*)> Operation) const
{
	UNetDriver* HostNetDriver = BeaconHost ? BeaconHost->GetNetDriver() : nullptr;
	if (HostNetDriver)
	{
		for (UNetConnection* ClientConnection : HostNetDriver->ClientConnections)
		{
			AMultiServerBeaconClient* BeaconClient = Cast<AMultiServerBeaconClient>(BeaconHost->GetClientActor(ClientConnection));
			if (BeaconClient)
			{
				Operation(BeaconClient);
			}
		}
	}
	for (UMultiServerPeerConnection* Peer : PeerConnections)
	{
		AMultiServerBeaconClient* BeaconClient = Peer->BeaconClient;
		if (BeaconClient)
		{
			Operation(BeaconClient);
		}
	}
}

void UMultiServerNode::ForEachNetDriver(TFunctionRef<void(UNetDriver*)> Operation) const
{
	TSet<UNetDriver*> UniqueNetDrivers;

	if (UNetDriver* HostNetDriver = BeaconHost->GetNetDriver())
	{
		UniqueNetDrivers.Add(HostNetDriver);
	}

	for (int32 PeerIndex = 0; PeerIndex < PeerConnections.Num(); PeerIndex++)
	{
		UMultiServerPeerConnection* Peer = PeerConnections[PeerIndex];
		if (AMultiServerBeaconClient* BeaconClient = Peer->BeaconClient)
		{
			if (UNetConnection* Connection = BeaconClient->GetNetConnection())
			{
				if (UNetDriver* NetDriver = Connection->GetDriver())
				{
					UniqueNetDrivers.Add(NetDriver);
				}
			}
		}
	}

	for (UNetDriver* NetDriver : UniqueNetDrivers)
	{
		Operation(NetDriver);
	}
}

int32 UMultiServerNode::GetConnectionCount() const
{
	int32 ConnectionCount = 0;

	const_cast<UMultiServerNode*>(this)->ForEachBeaconClient([&ConnectionCount](AMultiServerBeaconClient*)
		{
			ConnectionCount++;
		});

	return ConnectionCount;
}

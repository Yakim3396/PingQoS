
#include "PingQoSWorker.h"


uint32 FPingQoSWorker::Run()
{
	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Run] Getting IPs"));
	ParseURLs();

	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Run] Sending ECHO"));
	SendEchos();

	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Run] Start receiving"));
	while (!Stopping)
	{
		Update(PacketWaitTime);
	}

	return 0;
}

void FPingQoSWorker::Stop()
{
	Stopping = true;

	if (Socket != nullptr)
	{
		if (Socket->Close())
		{
			Socket->Shutdown(ESocketShutdownMode::ReadWrite);
			SocketSubsystem->DestroySocket(Socket);
			Socket = nullptr;
		}
	}
}

bool FPingQoSWorker::CheckTimeout()
{
	FDateTime lTimeRecv = FDateTime::UtcNow();
	FTimespan LResultPing;
	LResultPing = lTimeRecv - TimeSend;
	int32 CurWaitTime = LResultPing.GetSeconds();
	if (CurWaitTime >= PingTimeoutTime)
	{
		FinishRequest("[PingQoSWorker - CheckTimeout] WARNING: Timed out, quitting early!");

		return true;
	}
	return false;
}

void FPingQoSWorker::ParseURLs()
{
	for (FPingQoSInfo& ServerInfo : ServerInfosRequested)
	{
		if (ServerInfo.bUseIP) { continue; }

		UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - ParseURLs] Get ip from url : %s"), *ServerInfo.URL);
		FAddressInfoResult GAIResult = SocketSubsystem->GetAddressInfo(*ServerInfo.URL, nullptr, EAddressInfoFlags::Default, NAME_None);
		if (GAIResult.Results.Num() > 0)
		{
			ServerInfo.IP = GAIResult.Results[0].Address->ToString(false);
		}
		UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - ParseURLs] Current IP - %s : %d"), *ServerInfo.IP, ServerInfo.Port);
	}
}

void FPingQoSWorker::SendEchos()
{
	for (FPingQoSInfo& ServerInfo : ServerInfosRequested)
	{
		bool bIsValid;
		TSharedRef<FInternetAddr> HostAddr = SocketSubsystem->CreateInternetAddr();
		HostAddr->SetIp(*ServerInfo.IP, bIsValid);
		HostAddr->SetPort(ServerInfo.Port);

		if (!bIsValid)
		{
			UE_LOG(LogTemp, Error, TEXT("[PingQoSWorker - SendEchos] IP is invalid <%s:%d>"), *ServerInfo.IP, ServerInfo.Port);
			continue;
		}

		int32 BytesSent = 0;
		uint8 SendData[2] = { 0xFF, 0xFF };

		//	FString Message = BytesToHex(SendData, sizeof(SendData));
		//	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - SendEchos] Data to send : %s"), *Message);

		bool bSent = Socket->SendTo(SendData, sizeof(SendData), BytesSent, *HostAddr);
		TimeSend = FDateTime::UtcNow();
		//	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - SendEchos] Bytes sent : %d "), BytesSent);
	}
}

// Update this Ping Worker
void FPingQoSWorker::Update(const FTimespan& SocketWaitTime)
{
	if (Stopping)
	{
		return;
	}

	// Check timeout while we're waiting to receive a ping response
	if (!Socket->Wait(ESocketWaitConditions::WaitForRead, SocketWaitTime))
	{
		UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Update] Waiting"));
		CheckTimeout();
		return;
	}

	TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
	uint32 Size;

	// Loop while we're waiting to receive another ping response
	while (Socket->HasPendingData(Size) && !Stopping)
	{
		UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Update] Pending data..."));

		if (CheckTimeout()) { break; }

		int32 Read = 0;
		uint8 RecvData[2];

		if (Socket->RecvFrom(RecvData, sizeof(RecvData), Read, *Sender))
		{
			UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Update] Received data"));
			// FString Message = BytesToHex(RecvData, sizeof(RecvData));
			// Check if the data we received is correct from PlayFab
			if (RecvData[0] == 0x00 && RecvData[1] == 0x00)
			{
				FPingQoSInfo ServerInfoToRemove;
				for (FPingQoSInfo& ServerInfo : ServerInfosRequested)
				{
					// Check for which server from PlayFab this request for ping is from
					if (ServerInfo.IP == Sender->ToString(false))
					{
						//	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Update] Get data : %s"), *Message);
						TimeRecv = FDateTime::UtcNow();
						FTimespan LResultPing;
						LResultPing = TimeRecv - TimeSend;
						ServerInfo.Ping = LResultPing.GetFractionMilli();
						UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - Update] Region : %s, result ping : %d"), *ServerInfo.Region, ServerInfo.Ping);

						// Update queues
						ServerInfosToReturn.Emplace(ServerInfo);
						ServerInfoToRemove = ServerInfo;

						break;
					}
				}

				// Need to remove from ServerInfosRequested array because we assigned a valid ping ServerInfo to ServerInfosToReturn?
				if (ServerInfoToRemove.Ping != -1)
				{
					ServerInfosRequested.Remove(ServerInfoToRemove);
				}

				// If we've gone through all requests, then we can finish this request
				if (ServerInfosRequested.IsValidIndex(0) == false)
				{
					FinishRequest("[PingQoSWorker - Update] Found pings for all requests");
					return;
				}
			}
		}
	}
}

// When ping worker is finished for any reason we'll reach here
void FPingQoSWorker::FinishRequest(FString ReasonForFinishingRequest)
{
	Stopping = true;
	AsyncTask(ENamedThreads::GameThread, [&]()
		{
			DataReceiveDelegate.ExecuteIfBound(ServerInfosToReturn);
		});
	UE_LOG(LogTemp, Log, TEXT("[PingQoSWorker - FinishRequest] ReasonForFinishingRequest: %s"), *ReasonForFinishingRequest);
}

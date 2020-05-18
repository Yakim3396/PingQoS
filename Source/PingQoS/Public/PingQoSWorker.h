// Copyright 2020 Dmitry Yakimenko. All Rights Reserved.

#pragma once

#include "ArrayWriter.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "PingQoSSubsystem.h"

DECLARE_DELEGATE_OneParam(FOnSocketDataReceive, TArray<FPingQoSInfo>&);

/**
 * Asynchronously Ping Worker.
 */
class FPingQoSWorker
	: public FRunnable
	, private FSingleThreadRunnable
{
public:

	/**
    * Creates and initializes a new Ping Worker.
    *
    * @param Info Ping data
    * @param InWaitTime The amount of time to wait for the socket to be readable.
    * @param InThreadName The receiver thread name (for debugging).
    */
    FPingQoSWorker(const TArray<FPingQoSInfo> Infos,  const FTimespan& InWaitTime, const TCHAR* InThreadName)
        : Infos(Infos)
        , Stopping(false)
        , Thread(nullptr)
        , ThreadName(InThreadName)
        , WaitTime(InWaitTime)
    {
    	SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    	Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("QoSWorker_Ping"), true);
    	check(Socket != nullptr);
    	check(Socket->GetSocketType() == SOCKTYPE_Datagram);
    }

	/** Virtual destructor. */
	virtual ~FPingQoSWorker()
    {
		if (Thread != nullptr)
		{
			Thread->Kill(true);
			delete Thread;
		}
	}

public:
	
	/** Start the Ping Worker thread. */
	void Start()
	{
		Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
	}

	/**
	 * Returns a delegate that is executed when data has been received.
	 *
	 * This delegate must be bound before the receiver thread is started with
	 * the Start() method. It cannot be unbound while the thread is running.
	 *
	 * @return The delegate.
	 */
	FOnSocketDataReceive& OnDataReceived()
	{
		check(Thread == nullptr);
		return DataReceiveDelegate;
	}

	//~ FRunnable interface

	virtual FSingleThreadRunnable* GetSingleThreadInterface() override
	{
		return this;
	}

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		UE_LOG(LogTemp, Log, TEXT("Getting ips..."));
		ParseURLs();
		
		UE_LOG(LogTemp, Log, TEXT("Sends ECHO..."));
		SendEchos();
		
		UE_LOG(LogTemp, Log, TEXT("Start receiving..."));
		while (!Stopping)
		{
			Update(WaitTime);
		}
		
		return 0;
	}

	virtual void Stop() override
	{
		Stopping = true;
		
		if(Socket != nullptr)
		{
			if(Socket->Close())
			{
				Socket->Shutdown(ESocketShutdownMode::ReadWrite);
				SocketSubsystem->DestroySocket(Socket);
				Socket = nullptr;
			}
		}
	}

	virtual void Exit() override { }

protected:
		
	virtual bool CheckTimeout()
	{
		FDateTime lTimeRecv = FDateTime::UtcNow();
		FTimespan LResultPing;
		LResultPing = lTimeRecv - TimeSend;
		int32 CurWaitTime = LResultPing.GetSeconds();
		if(CurWaitTime >= 1)
		{
			UE_LOG(LogTemp, Log, TEXT("Timed out, end the stream"));
			DataReceiveDelegate.ExecuteIfBound(Infos);
			return true;
		}
		return false;
	}
	
	virtual void ParseURLs()
	{
		for(auto &Info : Infos)
		{
			if(Info.bUseIP) { continue; }

		//	UE_LOG(LogTemp, Log, TEXT("Get ip from url : %s"), *Inf.URL);
			FAddressInfoResult GAIResult = SocketSubsystem->GetAddressInfo(*Info.URL, nullptr, EAddressInfoFlags::Default, NAME_None);
			if (GAIResult.Results.Num() > 0)
			{
				Info.IP = GAIResult.Results[0].Address->ToString(false);
			}
		//	UE_LOG(LogTemp, Log, TEXT("current IP - %s : %d"), *Inf.IP, Inf.Port);
		}
	}

	virtual void SendEchos()
	{
		for(auto &Info : Infos)
		{
			bool bIsValid;
			TSharedRef<FInternetAddr> HostAddr = SocketSubsystem->CreateInternetAddr();
			HostAddr->SetIp(*Info.IP, bIsValid);
			HostAddr->SetPort(Info.Port);
			
			if (!bIsValid)
			{
				UE_LOG(LogTemp, Error, TEXT("IP is invalid <%s:%d>"), *Info.IP, Info.Port);
				continue;
			}
			
			int32 BytesSent = 0;
			uint8 SendData[2] = {0xFF, 0xFF};
		
		//	FString Message = BytesToHex(SendData, sizeof(SendData));
		//	UE_LOG(LogTemp, Log, TEXT("Data to send : %s"), *Message);
		
			bool bSent = Socket->SendTo(SendData, sizeof(SendData), BytesSent, *HostAddr);
			TimeSend = FDateTime::UtcNow();
		//	UE_LOG(LogTemp, Log, TEXT("Bytes sent : %d "), BytesSent);
		}
	}
	
	/** Update this Ping Worker. */
	void Update(const FTimespan& SocketWaitTime)
	{
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, SocketWaitTime))
		{
			UE_LOG(LogTemp, Log, TEXT("Wait..."));
			CheckTimeout();
			return;
		}

		TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
		uint32 Size;
		
		while (Socket->HasPendingData(Size) && !Stopping)
		{
		//	UE_LOG(LogTemp, Log, TEXT("Pending data..."));

			if(CheckTimeout()) { break; }
			
			int32 Read = 0;
			uint8 RecvData[2];
			
			if (Socket->RecvFrom(RecvData, sizeof(RecvData), Read, *Sender))
			{
			//	UE_LOG(LogTemp, Log, TEXT("Receive data..."));
			//	FString Message = BytesToHex(RecvData, sizeof(RecvData));
				if(RecvData[0] == 0x00 && RecvData[1] == 0x00)
				{
					for(auto &Info : Infos)
					{
						if(Info.IP == Sender->ToString(false))
						{
						//	UE_LOG(LogTemp, Log, TEXT("Get data : %s"), *Message);
							TimeRecv = FDateTime::UtcNow();
							FTimespan LResultPing;
							LResultPing = TimeRecv - TimeSend;
							Info.Ping = LResultPing.GetFractionMilli();
						//	UE_LOG(LogTemp, Log, TEXT("Region : %s, result ping : %d"), *Inf.Region, Inf.Ping);
							break;
						}
					}
				}
			}
		}
	}
	
	//~ FSingleThreadRunnable interface
	
	virtual void Tick() override
	{
		Update(FTimespan::Zero());
	}

private:
	
	TArray<FPingQoSInfo> Infos;
	
	FDateTime TimeSend;
	FDateTime TimeRecv;

	/** The network socket. */
	FSocket* Socket;

	/** Pointer to the socket sub-system. */
	ISocketSubsystem* SocketSubsystem;

	/** Flag indicating that the thread is stopping. */
	bool Stopping;

	/** The thread object. */
	FRunnableThread* Thread;

	/** The receiver thread's name. */
	FString ThreadName;

	/** The amount of time to wait for inbound packets. */
	FTimespan WaitTime;
	
	/** Holds the data received delegate. */
	FOnSocketDataReceive DataReceiveDelegate;
};

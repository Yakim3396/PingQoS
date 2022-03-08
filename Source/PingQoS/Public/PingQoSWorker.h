// Copyright 2020 Dmitry Yakimenko. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
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
	* @param NewServerInfos Ping data
	* @param InboundPacketWaitTime The amount of time to wait for the socket to be readable.
	* @param NewPingTimeoutTime The amount of time to wait before giving up on unfinsihed pings.
	* @param InThreadName The receiver thread name (for debugging).
	*/
	FPingQoSWorker(const TArray<FPingQoSInfo> NewServerInfos, const FTimespan& InboundPacketWaitTime, int32 NewPingTimeoutTime, const TCHAR* InThreadName)
		: ServerInfosRequested(NewServerInfos)
		, Stopping(false)
		, Thread(nullptr)
		, ThreadName(InThreadName)
		, PingTimeoutTime(NewPingTimeoutTime)
		, PacketWaitTime(InboundPacketWaitTime)
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

	virtual uint32 Run() override;

	virtual void Stop() override;

	virtual void Exit() override { }

protected:

	virtual bool CheckTimeout();

	virtual void ParseURLs();

	virtual void SendEchos();

	/* Update this Ping Worker. */
	void Update(const FTimespan& SocketWaitTime);

	void FinishRequest(FString ReasonForFinishingRequest);

	
	//~ FSingleThreadRunnable interface
	
	virtual void Tick() override
	{
		Update(FTimespan::Zero());
	}

private:
	
	TArray<FPingQoSInfo> ServerInfosRequested; // Regional server pings requested
	TArray<FPingQoSInfo> ServerInfosToReturn; // Regional server pings found
	
	FDateTime TimeSend;
	FDateTime TimeRecv;
	// Timeout in seconds if no response. 1 second = 1000ms which is plenty of time for one ping, but if multiple pings are requested this
	// should be dynamic update based off those possibilities.
	int32 PingTimeoutTime; 

	/* The network socket. */
	FSocket* Socket;

	/* Pointer to the socket sub-system. */
	ISocketSubsystem* SocketSubsystem;

	/* Flag indicating that the thread is stopping. */
	bool Stopping;

	/* The thread object. */
	FRunnableThread* Thread;

	/* The receiver thread's name. */
	FString ThreadName;

	/* The amount of time to wait for inbound packets. */
	FTimespan PacketWaitTime;
	
	/* Holds the data received delegate. */
	FOnSocketDataReceive DataReceiveDelegate;
};

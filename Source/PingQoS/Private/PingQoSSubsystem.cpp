// Copyright 2020 Dmitry Yakimenko. All Rights Reserved.

#include "PingQoSSubsystem.h"
#include "PingQoS.h"
#include "PingQoSWorker.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"

#include "Async/Async.h"


// Overload == for FPingQoSInfo. This is necessary for TArray comparisons
inline bool operator==(const FPingQoSInfo& lhs, const FPingQoSInfo& rhs)
{
	return	lhs.URL == rhs.URL &&
			lhs.IP == rhs.IP &&
			lhs.bUseIP == rhs.bUseIP &&
			lhs.Port == rhs.Port &&
			lhs.Region == rhs.Region &&
			lhs.Ping == rhs.Ping;
}

UPingQoSSubsystem::UPingQoSSubsystem()
	: UEngineSubsystem()
{
}

void UPingQoSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogPingQoS, Log, TEXT("PingQoS subsystem initialized"));
	
	bIsCompleted = true;
	Worker = nullptr;
}

void UPingQoSSubsystem::Deinitialize()
{
	Super::Deinitialize();
	
	if(!Worker) { return; }
	Worker->Stop();
	Worker->Exit();
	delete Worker;
	Worker = nullptr;
}

void UPingQoSSubsystem::Init(TArray<FPingQoSInfo> SetInfo)
{
	if (SetInfo.IsValidIndex(0) == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PingQoSSubsystem - Init] SetInfo array is empty!"));
		return;
	}

	Infos = SetInfo;
	WorkerPingTimeoutTime = SetInfo.Num(); // Assign 1 second for each IP to ping
	Update();
}

bool UPingQoSSubsystem::Update()
{
	if(!bIsCompleted) { return false; }
	bIsCompleted = false;
	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
	Worker = new FPingQoSWorker(Infos, ThreadWaitTime, WorkerPingTimeoutTime, TEXT("thread_udp_receiver_ping"));
	Worker->OnDataReceived().BindUObject(this, &UPingQoSSubsystem::Recv);
	Worker->Start();
	return true;
}

void UPingQoSSubsystem::Recv(TArray<FPingQoSInfo>& ResultInfos)
{
	for (auto &Info : ResultInfos)
	{
		UE_LOG(LogTemp, Log, TEXT("[PingQoSSubsystem - Recv] Region : %s, result ping : %d"), *Info.Region, Info.Ping);
	}

	AsyncTask(ENamedThreads::GameThread, [&, ResultInfos]()
    {
        OnPingCompleted.Broadcast(ResultInfos);
    });
	
	UE_LOG(LogTemp, Log, TEXT("[PingQoSSubsystem - Recv] Finished! Destroying worker"));
	bIsCompleted = true;
	Worker->Stop();
	Worker->Exit();
	delete Worker;
	Worker = nullptr;
}
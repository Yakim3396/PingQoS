// Copyright 2020 Dmitry Yakimenko. All Rights Reserved.

#include "PingQoSSubsystem.h"
#include "PingQoS.h"
#include "PingQoSWorker.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"

#include "Async/Async.h"

UPingQoSSubsystem::UPingQoSSubsystem()
	: UEngineSubsystem()
{
}

void UPingQoSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogPingQoS, Log, TEXT("PingQoS subsystem initialized"));
}

void UPingQoSSubsystem::Deinitialize()
{
	Super::Deinitialize();
	
	if(!Worker) { return; }
	Worker->Stop();
	Worker->Exit();
	Worker = nullptr;
	delete Worker;
}

void UPingQoSSubsystem::Init(TArray<FPingQoSInfo> SetInfo)
{
	Infos = SetInfo;
	Update();
}

bool UPingQoSSubsystem::Update()
{
	if(Worker) { return false; }
	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
	Worker = new FPingQoSWorker(Infos, ThreadWaitTime, TEXT("thread_udp_receiver_ping"));
	Worker->OnDataReceived().BindUObject(this, &UPingQoSSubsystem::Recv);
	Worker->Start();
	return true;
}

void UPingQoSSubsystem::Recv(TArray<FPingQoSInfo>& ResultInfos)
{
	for (auto &Info : ResultInfos)
	{
		UE_LOG(LogTemp, Log, TEXT("Region : %s, result ping : %d"), *Info.Region, Info.Ping);
	}

	//Pass the reference to be used on gamethread
	AsyncTask(ENamedThreads::GameThread, [&, ResultInfos]()
    {
        OnPingCompleted.Broadcast(ResultInfos);
    });
	
	UE_LOG(LogTemp, Log, TEXT("Finish! Destroy worker."));
	Worker->Stop();
	Worker->Exit();
	Worker = nullptr;
	delete Worker;
}
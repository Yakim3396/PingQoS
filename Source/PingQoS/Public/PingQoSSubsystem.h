// Copyright 2020 Dmitry Yakimenko. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

#include "PingQoSSubsystem.generated.h"


USTRUCT(BlueprintType)
struct PINGQOS_API FPingQoSInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PingQoS")
	FString URL = "";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PingQoS")
	FString IP = "";
	//Use IP instead URL?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PingQoS")
	bool bUseIP = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PingQoS")
	int32 Port = 3075;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PingQoS")
	FString Region = "";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PingQoS")
	int32 Ping = -1;
	
	FPingQoSInfo()
	{
		URL = "";
		IP = "";
		bUseIP = false;
		Port = 3075;
		Region = "";
		Ping = -1;
	}
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPingQoSDelegate, const TArray<FPingQoSInfo>&, Result);

UCLASS()
class PINGQOS_API UPingQoSSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	UPingQoSSubsystem();

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	
	UFUNCTION(BlueprintCallable, Category = "PingQoS|Utility")
    virtual void Init(TArray<FPingQoSInfo> SetInfo);
	
	UFUNCTION(BlueprintCallable, Category = "PingQoS|Utility")
    virtual bool Update();

	UPROPERTY(BlueprintAssignable, Category = "PingQoS|Utility")
    FPingQoSDelegate OnPingCompleted;

private:
	
	void Recv(TArray<FPingQoSInfo>& ResultInfos);
	
	class FPingQoSWorker* Worker;

	TArray<FPingQoSInfo> Infos;
};

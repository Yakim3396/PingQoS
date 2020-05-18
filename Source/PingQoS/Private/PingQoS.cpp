// Copyright 2020 Dmitry Yakimenko. All Rights Reserved.

#include "PingQoS.h"

#define LOCTEXT_NAMESPACE "FPingQoSModule"

void FPingQoSModule::StartupModule()
{
	UE_LOG(LogPingQoS, Log, TEXT("PingQoS module started"));
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FPingQoSModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FPingQoSModule, PingQoS)

DEFINE_LOG_CATEGORY(LogPingQoS);

#undef LOCTEXT_NAMESPACE
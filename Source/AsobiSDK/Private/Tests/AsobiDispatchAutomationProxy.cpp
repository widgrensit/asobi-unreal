#include "AsobiDispatchAutomationProxy.h"

void UAsobiDispatchAutomationProxy::HandleRaw(const FString& Type, const FString& /*Payload*/)
{
	bRawFired = true;
	RawType = Type;
}

void UAsobiDispatchAutomationProxy::HandleString(const FString& /*Value*/)
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleVoid()
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleStringString(const FString& /*A*/, const FString& /*B*/)
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleWorldJoined(const FAsobiWorldInfo& /*Info*/)
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleWorldList(const TArray<FAsobiWorldInfo>& /*Worlds*/)
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleWorldTick(int64 /*Tick*/, const FString& /*UpdatesJson*/)
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleWorldTerrain(const FAsobiWorldTerrainChunk& /*Chunk*/)
{
	bTypedFired = true;
}

void UAsobiDispatchAutomationProxy::HandleDmMessage(const FAsobiDirectMessage& /*Message*/)
{
	bTypedFired = true;
}

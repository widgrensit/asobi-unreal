#include "AsobiSmokeAutomationProxy.h"

void UAsobiSmokeAutomationProxy::HandleResult(bool bSuccess, const FString& Message)
{
	bOk = bSuccess;
	LastMessage = Message;
	bDone = true;
}

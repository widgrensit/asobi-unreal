#include "AsobiGameMessageAutomationProxy.h"

void UAsobiGameMessageAutomationProxy::HandleGameMessage(const FAsobiGameMessage& Message)
{
	LastMessage = Message;
	bFired = true;
}

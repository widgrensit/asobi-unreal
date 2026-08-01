#include "AsobiGameErrorAutomationProxy.h"

void UAsobiGameErrorAutomationProxy::HandleGameError(const FAsobiGameError& Error)
{
	LastError = Error;
	bFired = true;
}

#include "AsobiModuleEventAutomationProxy.h"

void UAsobiModuleEventAutomationProxy::HandleModuleEvent(const FAsobiModuleEvent& Event)
{
	LastEvent = Event;
	bFired = true;
}

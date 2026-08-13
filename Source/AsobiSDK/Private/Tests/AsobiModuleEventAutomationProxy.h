#pragma once

// Internal helper for the Asobi.ModuleEvent automation test. Lives under
// Private/Tests/ so it never ships in the plugin's public surface. Exists
// only to give the dynamic FOnAsobiModuleEvent delegate a UFUNCTION receiver
// (AddLambda is not legal on dynamic multicast delegates).

#include "CoreMinimal.h"
#include "AsobiTypes.h"
#include "AsobiModuleEventAutomationProxy.generated.h"

UCLASS()
class UAsobiModuleEventAutomationProxy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleModuleEvent(const FAsobiModuleEvent& Event);

	bool bFired = false;
	FAsobiModuleEvent LastEvent;
};

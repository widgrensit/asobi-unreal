#pragma once

// Internal helper for the Asobi.Smoke automation test. Lives under Private/
// Tests/ so it never ships in the plugin's public surface. Exists only to
// give the dynamic FOnAsobiSmokeResult delegate a UFUNCTION receiver.

#include "CoreMinimal.h"
#include "AsobiSmokeTest.h"
#include "AsobiSmokeAutomationProxy.generated.h"

UCLASS()
class UAsobiSmokeAutomationProxy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleResult(bool bSuccess, const FString& Message);

	bool bDone = false;
	bool bOk = false;
	FString LastMessage;
};

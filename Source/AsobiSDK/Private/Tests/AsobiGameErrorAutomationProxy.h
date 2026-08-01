#pragma once

// Internal helper for the Asobi.GameError automation test. Lives under
// Private/Tests/ so it never ships in the plugin's public surface. Exists
// only to give the dynamic FOnAsobiGameError delegate a UFUNCTION receiver
// (AddLambda is not legal on dynamic multicast delegates).

#include "CoreMinimal.h"
#include "AsobiTypes.h"
#include "AsobiGameErrorAutomationProxy.generated.h"

UCLASS()
class UAsobiGameErrorAutomationProxy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleGameError(const FAsobiGameError& Error);

	bool bFired = false;
	FAsobiGameError LastError;
};

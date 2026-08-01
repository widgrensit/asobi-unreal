#pragma once

// Internal helper for the Asobi.GameMessage automation test. Lives under
// Private/Tests/ so it never ships in the plugin's public surface. Exists
// only to give the dynamic FOnAsobiGameMessage delegate a UFUNCTION receiver
// (AddLambda is not legal on dynamic multicast delegates).

#include "CoreMinimal.h"
#include "AsobiTypes.h"
#include "AsobiGameMessageAutomationProxy.generated.h"

UCLASS()
class UAsobiGameMessageAutomationProxy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleGameMessage(const FAsobiGameMessage& Message);

	bool bFired = false;
	FAsobiGameMessage LastMessage;
};

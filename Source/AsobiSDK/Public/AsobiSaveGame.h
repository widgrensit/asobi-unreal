#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AsobiSaveGame.generated.h"

UCLASS()
class ASOBISDK_API UAsobiSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Session slot ("AsobiSession"): the persisted refresh token.
	UPROPERTY()
	FString RefreshToken;

	// Guest-device slot ("AsobiGuestDevice"): the stable guest credential pair
	// managed by AsobiDevice. Empty in the session slot; RefreshToken is empty
	// in the device slot.
	UPROPERTY()
	FString DeviceId;

	UPROPERTY()
	FString DeviceSecret;
};

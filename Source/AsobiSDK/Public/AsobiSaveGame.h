#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AsobiSaveGame.generated.h"

UCLASS()
class ASOBISDK_API UAsobiSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString RefreshToken;
};

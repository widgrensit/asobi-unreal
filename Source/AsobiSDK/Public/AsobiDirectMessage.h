#pragma once

#include "CoreMinimal.h"
#include "AsobiClient.h"
#include "AsobiTypes.h"
#include "AsobiDirectMessage.generated.h"

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAsobiDmSendResponse, bool, bSuccess, const FString&, ChannelId, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAsobiDmHistoryResponse, bool, bSuccess, const FString&, ChannelId, const TArray<FAsobiDirectMessage>&, Messages);

UCLASS(BlueprintType)
class ASOBISDK_API UAsobiDirectMessage : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Asobi")
	void Init(UAsobiClient* InClient);

	UFUNCTION(BlueprintCallable, Category = "Asobi|DM")
	void Send(const FString& RecipientId, const FString& Content, const FOnAsobiDmSendResponse& Callback);

	UFUNCTION(BlueprintCallable, Category = "Asobi|DM")
	void History(const FString& OtherPlayerId, int32 Limit, const FOnAsobiDmHistoryResponse& Callback);

private:
	UPROPERTY()
	UAsobiClient* Client;
};

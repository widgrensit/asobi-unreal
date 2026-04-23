#include "AsobiDirectMessage.h"

void UAsobiDirectMessage::Init(UAsobiClient* InClient)
{
	Client = InClient;
}

void UAsobiDirectMessage::Send(const FString& RecipientId, const FString& Content, const FOnAsobiDmSendResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("recipient_id"), RecipientId);
	Body->SetStringField(TEXT("content"), Content);

	Client->Post(TEXT("/api/v1/dm"), Body,
		FOnAsobiResponse::CreateLambda([Callback](bool bSuccess, const FString& Response)
		{
			FString ChannelId;
			FString Error;
			TSharedPtr<FJsonObject> Json = UAsobiClient::ParseJson(Response);
			if (Json.IsValid())
			{
				Json->TryGetStringField(TEXT("channel_id"), ChannelId);
				Json->TryGetStringField(TEXT("error"), Error);
			}
			Callback.ExecuteIfBound(bSuccess, ChannelId, Error);
		}));
}

void UAsobiDirectMessage::History(const FString& OtherPlayerId, int32 Limit, const FOnAsobiDmHistoryResponse& Callback)
{
	FString Path = FString::Printf(TEXT("/api/v1/dm/%s/history"), *OtherPlayerId);
	if (Limit > 0)
	{
		Path += FString::Printf(TEXT("?limit=%d"), Limit);
	}

	Client->Get(Path,
		FOnAsobiResponse::CreateLambda([Callback](bool bSuccess, const FString& Response)
		{
			FString ChannelId;
			TArray<FAsobiDirectMessage> Messages;
			if (bSuccess)
			{
				TSharedPtr<FJsonObject> Json = UAsobiClient::ParseJson(Response);
				if (Json.IsValid())
				{
					Json->TryGetStringField(TEXT("channel_id"), ChannelId);
					const TArray<TSharedPtr<FJsonValue>>* Arr;
					if (Json->TryGetArrayField(TEXT("messages"), Arr))
					{
						for (const auto& Val : *Arr)
						{
							const TSharedPtr<FJsonObject>* Obj;
							if (Val->TryGetObject(Obj))
							{
								Messages.Add(UAsobiClient::ParseDirectMessage(*Obj));
							}
						}
					}
				}
			}
			Callback.ExecuteIfBound(bSuccess, ChannelId, Messages);
		}));
}

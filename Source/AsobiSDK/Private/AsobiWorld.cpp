#include "AsobiWorld.h"
#include "GenericPlatform/GenericPlatformHttp.h"

void UAsobiWorld::Init(UAsobiClient* InClient)
{
	Client = InClient;
}

void UAsobiWorld::List(const FString& Mode, bool bOnlyWithCapacity, const FOnAsobiWorldListResponse& Callback)
{
	FString Path = TEXT("/api/v1/worlds");
	TArray<FString> Params;
	if (!Mode.IsEmpty())
	{
		Params.Add(FString::Printf(TEXT("mode=%s"), *FGenericPlatformHttp::UrlEncode(Mode)));
	}
	if (bOnlyWithCapacity)
	{
		Params.Add(TEXT("has_capacity=true"));
	}
	if (Params.Num() > 0)
	{
		Path += TEXT("?") + FString::Join(Params, TEXT("&"));
	}

	Client->Get(Path,
		FOnAsobiResponse::CreateLambda([Callback](bool bSuccess, const FString& Response)
		{
			TArray<FAsobiWorldInfo> Worlds;
			if (bSuccess)
			{
				TSharedPtr<FJsonObject> Json = UAsobiClient::ParseJson(Response);
				if (Json.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* Arr;
					if (Json->TryGetArrayField(TEXT("worlds"), Arr))
					{
						for (const auto& Val : *Arr)
						{
							const TSharedPtr<FJsonObject>* Obj;
							if (Val->TryGetObject(Obj))
							{
								Worlds.Add(UAsobiClient::ParseWorldInfo(*Obj));
							}
						}
					}
				}
			}
			Callback.ExecuteIfBound(bSuccess, Worlds);
		}));
}

void UAsobiWorld::Get(const FString& WorldId, const FOnAsobiWorldResponse& Callback)
{
	Client->Get(FString::Printf(TEXT("/api/v1/worlds/%s"), *WorldId),
		FOnAsobiResponse::CreateLambda([Callback](bool bSuccess, const FString& Response)
		{
			FAsobiWorldInfo Info;
			if (bSuccess)
			{
				TSharedPtr<FJsonObject> Json = UAsobiClient::ParseJson(Response);
				if (Json.IsValid())
				{
					Info = UAsobiClient::ParseWorldInfo(Json);
				}
			}
			Callback.ExecuteIfBound(bSuccess, Info);
		}));
}

void UAsobiWorld::Create(const FString& Mode, const FOnAsobiWorldResponse& Callback)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("mode"), Mode);

	Client->Post(TEXT("/api/v1/worlds"), Body,
		FOnAsobiResponse::CreateLambda([Callback](bool bSuccess, const FString& Response)
		{
			FAsobiWorldInfo Info;
			if (bSuccess)
			{
				TSharedPtr<FJsonObject> Json = UAsobiClient::ParseJson(Response);
				if (Json.IsValid())
				{
					Info = UAsobiClient::ParseWorldInfo(Json);
				}
			}
			Callback.ExecuteIfBound(bSuccess, Info);
		}));
}

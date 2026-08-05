#include "AsobiRpcAction.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UAsobiRpcAction* UAsobiRpcAction::AsobiRpc(UAsobiWebSocket* Realtime, const FString& Method,
                                           const FString& ParamsJson)
{
	UAsobiRpcAction* Action = NewObject<UAsobiRpcAction>();
	Action->Socket = Realtime;
	Action->PendingMethod = Method;
	Action->PendingParamsJson = ParamsJson;
	return Action;
}

void UAsobiRpcAction::Activate()
{
	if (!Socket)
	{
		FAsobiRpcError Error;
		Error.Code = TEXT("not_connected");
		Error.Message = TEXT("No realtime socket was given to the RPC node.");
		Error.DetailsJson = TEXT("{}");
		Complete(FString(), &Error);
		return;
	}

	TSharedPtr<FJsonObject> Params;
	if (!PendingParamsJson.IsEmpty())
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PendingParamsJson);
		if (!FJsonSerializer::Deserialize(Reader, Params) || !Params.IsValid())
		{
			// Fail here rather than sending {} and letting the extension reject
			// it: a typo in the params should say so, not look like a server
			// rejecting a well-formed call.
			FAsobiRpcError Error;
			Error.Code = TEXT("invalid_params");
			Error.Message = TEXT("Params is not a JSON object.");
			Error.DetailsJson = TEXT("{}");
			Complete(FString(), &Error);
			return;
		}
	}

	// Keep this node alive until the reply lands - nothing else holds a
	// reference to it once Activate returns.
	AddToRoot();

	TWeakObjectPtr<UAsobiRpcAction> WeakThis(this);
	Socket->Rpc(PendingMethod, Params,
		[WeakThis](const FString& ResultJson, const FAsobiRpcError* Error)
		{
			// The Blueprint that started this may be gone by now - a level
			// change, or the actor destroyed while the call was in flight.
			if (UAsobiRpcAction* Self = WeakThis.Get())
			{
				Self->Complete(ResultJson, Error);
			}
		});
}

void UAsobiRpcAction::Complete(const FString& ResultJson, const FAsobiRpcError* Error)
{
	if (Error)
	{
		Failure.Broadcast(FString(), *Error);
	}
	else
	{
		Success.Broadcast(ResultJson, FAsobiRpcError());
	}

	// Exactly one pin fires and the node is done, so drop both the root
	// reference and the delegates rather than leaking either.
	Success.Clear();
	Failure.Clear();
	if (IsRooted())
	{
		RemoveFromRoot();
	}
	SetReadyToDestroy();
}

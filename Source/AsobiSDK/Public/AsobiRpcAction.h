#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsobiWebSocket.h"
#include "AsobiRpcAction.generated.h"

/**
 * Success/failure pins for the "Asobi RPC" node. Two delegates rather than one
 * with a bSuccess flag, so a Blueprint cannot forget to check it.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiRpcComplete,
	const FString&, ResultJson, const FAsobiRpcError&, Error);

/**
 * Latent Blueprint node for calling a server extension's RPC method.
 *
 *   Asobi RPC (Realtime, "quests.claim", "{\"quest_key\":\"daily\"}")
 *     -> Success (ResultJson)
 *     -> Failure (Error.Code, Error.Message, Error.DetailsJson)
 *
 * ResultJson is the result object as raw JSON - the extension defines its
 * shape, so parse it with whatever the extension documents. On Failure, branch
 * on Error.Code; Message is for humans and may be reworded at any time.
 *
 * Exactly one pin fires, always. A call made while disconnected fails with
 * code "not_connected" rather than leaving the node latent forever.
 *
 * C++ callers want UAsobiWebSocket::Rpc directly - it takes a TFunction and
 * skips the UObject.
 */
UCLASS()
class ASOBISDK_API UAsobiRpcAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnAsobiRpcComplete Success;

	UPROPERTY(BlueprintAssignable)
	FOnAsobiRpcComplete Failure;

	/**
	 * @param Realtime   The connected realtime socket.
	 * @param Method     Namespaced method, e.g. "quests.claim".
	 * @param ParamsJson The params object as JSON. Empty sends "{}".
	 */
	UFUNCTION(BlueprintCallable, Category = "Asobi|RPC",
		meta = (BlueprintInternalUseOnly = "true", DisplayName = "Asobi RPC"))
	static UAsobiRpcAction* AsobiRpc(UAsobiWebSocket* Realtime, const FString& Method,
	                                 const FString& ParamsJson);

	virtual void Activate() override;

private:
	void Complete(const FString& ResultJson, const FAsobiRpcError* Error);

	UPROPERTY()
	TObjectPtr<UAsobiWebSocket> Socket;

	FString PendingMethod;
	FString PendingParamsJson;
};

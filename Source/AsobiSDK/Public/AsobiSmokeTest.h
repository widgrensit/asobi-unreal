#pragma once

#include "CoreMinimal.h"
#include "AsobiClient.h"
#include "AsobiAuth.h"
#include "AsobiMatchmaker.h"
#include "AsobiWebSocket.h"
#include "AsobiSmokeTest.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiSmokeResult, bool, bSuccess, const FString&, Message);

/**
 * Runs the canonical smoke scenarios against asobi-test-harness.
 * Spawn one instance in a level, bind `OnResult`, and call `RunTest`.
 *
 * Scenarios (see widgrensit/asobi-test-harness/scenarios/canonical.md):
 *   1. Register two players + WS connect.
 *   2. matchmaker.add → receive match.matched on both.
 *   3. Send match.input → receive match.state with input applied.
 *
 * CI story: run via `UnrealEditor -run=Automation` once an Epic
 * container image is wired up. See Source/AsobiSDK/SmokeTest.md.
 */
UCLASS(BlueprintType, Blueprintable)
class ASOBISDK_API UAsobiSmokeTest : public UObject
{
	GENERATED_BODY()

public:
	UAsobiSmokeTest();

	UFUNCTION(BlueprintCallable, Category = "Asobi|Smoke")
	void RunTest(const FString& BaseUrl);

	UPROPERTY(BlueprintAssignable, Category = "Asobi|Smoke")
	FOnAsobiSmokeResult OnResult;

	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	FString MatchMode = TEXT("smoke");

	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	float MatchTimeoutSeconds = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	float StateTimeoutSeconds = 3.0f;

private:
	struct FPlayer
	{
		UAsobiClient* Client = nullptr;
		UAsobiAuth* Auth = nullptr;
		UAsobiMatchmaker* Matchmaker = nullptr;
		UAsobiWebSocket* WebSocket = nullptr;
		FString PlayerId;
		bool bMatched = false;
		FString MatchedMatchId;
	};

	UFUNCTION()
	void HandleAuthResultA(bool bSuccess, const FAsobiAuthTokens& Tokens);
	UFUNCTION()
	void HandleAuthResultB(bool bSuccess, const FAsobiAuthTokens& Tokens);

	UFUNCTION()
	void HandleWsConnectedA();
	UFUNCTION()
	void HandleWsConnectedB();

	UFUNCTION()
	void HandleMatchEventA(const FString& Event, const FString& PayloadJson);
	UFUNCTION()
	void HandleMatchEventB(const FString& Event, const FString& PayloadJson);

	UFUNCTION()
	void HandleMatchStateA(const FString& StateJson);

	void SetupPlayer(FPlayer& P, const FString& BaseUrl, bool bIsA);
	void CheckMatchedBoth();
	void Finish(bool bOk, const FString& Msg);
	void OnStateReady(float X);

	FPlayer A;
	FPlayer B;

	bool bAQueued = false;
	bool bBQueued = false;
	bool bInputSent = false;
	bool bFinished = false;
	FTimerHandle TimeoutHandle;

	FString ExtractMatchId(const FString& PayloadJson) const;
	float ExtractPlayerX(const FString& StateJson, const FString& PlayerId) const;
};

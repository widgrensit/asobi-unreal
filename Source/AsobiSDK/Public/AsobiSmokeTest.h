#pragma once

#include "CoreMinimal.h"
#include "AsobiClient.h"
#include "AsobiAuth.h"
#include "AsobiMatchmaker.h"
#include "AsobiWebSocket.h"
#include "AsobiSmokeTest.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiSmokeResult, bool, bSuccess, const FString&, Message);

/**
 * Runs the canonical smoke scenarios against widgrensit/sdk_demo_backend.
 * Spawn one instance in a level, bind `OnResult`, and call `RunTest`.
 *
 * Scenarios (see widgrensit/sdk_demo_backend/SMOKE.md):
 *   1. Register two players + WS connect.
 *   2. matchmaker.add (mode "demo") -> receive match.matched on both.
 *   3. Send match.input {move_x:1} -> observe match.state with x > x_initial + 10.
 *
 * Manual run via UE Automation:
 *   UE5Editor-Cmd MyProject.uproject \
 *     -ExecCmds="Automation RunTests Asobi.Smoke; Quit" \
 *     -unattended -nullrhi -log
 *
 * See Source/AsobiSDK/Tests/SmokeTest.md.
 */
UCLASS(BlueprintType, Blueprintable)
class ASOBISDK_API UAsobiSmokeTest : public UObject
{
	GENERATED_BODY()

public:
	UAsobiSmokeTest();

	UFUNCTION(BlueprintCallable, Category = "Asobi|Smoke")
	void RunTest(const FString& InBaseUrl);

	UPROPERTY(BlueprintAssignable, Category = "Asobi|Smoke")
	FOnAsobiSmokeResult OnResult;

	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	FString MatchMode = TEXT("demo");

	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	float MatchTimeoutSeconds = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	float StateTimeoutSeconds = 3.0f;

	/** Per-player x advance threshold from x_initial after move_x=1 (px). */
	UPROPERTY(EditDefaultsOnly, Category = "Asobi|Smoke")
	float MinXAdvancePx = 10.0f;

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
	void HandleAuthResultA(bool bSuccess, const FAsobiAuthTokens& Tokens, const FAsobiError& Error);
	UFUNCTION()
	void HandleAuthResultB(bool bSuccess, const FAsobiAuthTokens& Tokens, const FAsobiError& Error);

	UFUNCTION()
	void HandleWsConnectedA();
	UFUNCTION()
	void HandleWsConnectedB();

	UFUNCTION()
	void HandleMatchEventA(const FString& PayloadJson);
	UFUNCTION()
	void HandleMatchEventB(const FString& PayloadJson);

	UFUNCTION()
	void HandleMatchStateA(const FString& StateJson);

	void SetupPlayer(FPlayer& P, const FString& InBaseUrl, bool bIsA);
	void CheckMatchedBoth();
	void Finish(bool bOk, const FString& Msg);

	FPlayer A;
	FPlayer B;

	FString BaseUrl;
	FString WsUrl;
	bool bAQueued = false;
	bool bBQueued = false;
	bool bInputSent = false;
	bool bXInitialCaptured = false;
	float XInitial = 0.0f;
	bool bFinished = false;
	FTimerHandle TimeoutHandle;

	FString DeriveWsUrl(const FString& InBaseUrl) const;
	FString ExtractMatchId(const FString& PayloadJson) const;
	float ExtractPlayerX(const FString& StateJson, const FString& PlayerId) const;
};

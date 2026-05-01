#pragma once

// Internal helper for the Asobi.Dispatch automation test. Lives under
// Private/Tests/ so it never ships in the plugin's public surface. Exists
// purely to give the BlueprintAssignable (dynamic) delegates on
// UAsobiWebSocket a UFUNCTION receiver — AddLambda is not legal on dynamic
// multicast delegates.
//
// Each typed dispatch on UAsobiWebSocket has a distinct delegate signature;
// we provide one UFUNCTION per signature, all of which flip the same
// `bTypedFired` flag. The dispatch test binds whichever one matches the
// fixture under test.

#include "CoreMinimal.h"
#include "AsobiTypes.h"
#include "AsobiDispatchAutomationProxy.generated.h"

UCLASS()
class UAsobiDispatchAutomationProxy : public UObject
{
	GENERATED_BODY()

public:
	bool bRawFired = false;
	FString RawType;
	bool bTypedFired = false;

	UFUNCTION()
	void HandleRaw(const FString& Type, const FString& Payload);

	// One-param string variants (match.state, match.joined, match.matched,
	// matchmaker.queued, chat.joined, chat.left, notification.new,
	// presence.updated, dm.sent).
	UFUNCTION()
	void HandleString(const FString& Value);

	// No-param variants (match.left, matchmaker.removed, vote.cast_ok,
	// vote.veto_ok, world.left).
	UFUNCTION()
	void HandleVoid();

	// Two-string variants (match.event, chat.received, world.event).
	UFUNCTION()
	void HandleStringString(const FString& A, const FString& B);

	UFUNCTION()
	void HandleWorldJoined(const FAsobiWorldInfo& Info);

	UFUNCTION()
	void HandleWorldList(const TArray<FAsobiWorldInfo>& Worlds);

	UFUNCTION()
	void HandleWorldTick(int64 Tick, const FString& UpdatesJson);

	UFUNCTION()
	void HandleWorldTerrain(const FAsobiWorldTerrainChunk& Chunk);

	UFUNCTION()
	void HandleDmMessage(const FAsobiDirectMessage& Message);
};

#pragma once

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "AsobiTypes.h"
#include "AsobiWebSocket.generated.h"

class UAsobiClient;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiWsConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWsDisconnected, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiWsMessage, const FString&, Type, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWsError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiWsAuthExpired);

// Typed event delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiMatchState, const FString&, StateJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiMatchEvent, const FString&, Event, const FString&, PayloadJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiMatchJoined, const FString&, InfoJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiMatchMatched, const FString&, InfoJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiMatchLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiMatchmakerQueued, const FString&, TicketId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiMatchmakerRemoved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiChatReceived, const FString&, ChannelId, const FString&, MessageJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiChatJoined, const FString&, ChannelId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiChatLeft, const FString&, ChannelId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiNotificationReceived, const FString&, NotifJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiPresenceUpdated, const FString&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiVoteCastOk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiVoteVetoOk);

// World events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWorldJoined, const FAsobiWorldInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsobiWorldLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWorldList, const TArray<FAsobiWorldInfo>&, Worlds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiWorldTick, int64, Tick, const FString&, UpdatesJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWorldTerrain, const FAsobiWorldTerrainChunk&, Chunk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAsobiWorldEvent, const FString&, Event, const FString&, PayloadJson);

// Direct message events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiDmMessage, const FAsobiDirectMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiDmSent, const FString&, ChannelId);

// Dev-mode Lua script errors (server-gated behind ASOBI_DEV_ERRORS=true)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiGameError, const FAsobiGameError&, Error);

// Messages pushed by a Lua game script via game.send(player_id, message).
// Emitted unconditionally in production (unlike FOnAsobiGameError).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiGameMessage, const FAsobiGameMessage&, Message);

UCLASS(BlueprintType)
class ASOBISDK_API UAsobiWebSocket : public UObject
{
	GENERATED_BODY()

public:
	UAsobiWebSocket();

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void Connect(const FString& Url);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void Disconnect();

	UFUNCTION(BlueprintPure, Category = "Asobi|WebSocket")
	bool IsConnected() const;

	// Session
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void Authenticate(const FString& Token);

	// Re-sends session.connect with a rotated access token (e.g. after a REST
	// 401 auto-refresh). Wire UAsobiClient::OnTokenRotated to this.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void Reauthenticate(const FString& NewToken);

	// Binds this socket to a client so a REST 401 auto-refresh transparently
	// re-auths the live socket. Idiomatically the two objects are constructed
	// independently (see AsobiSmokeTest), so this is the SDK's one-call wiring
	// point: it hooks UAsobiClient::OnTokenRotated -> Reauthenticate. Safe to
	// call before or after Connect; call once per client.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void BindToClient(UAsobiClient* InClient);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void SendHeartbeat();

	// Match
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void SendMatchInput(const FString& DataJson);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void JoinMatch(const FString& MatchId);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void LeaveMatch();

	// Matchmaker
	// PropertiesJson is an optional JSON object of matchmaking properties; pass
	// an empty string to omit it.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void MatchmakerAdd(const FString& Mode, const FString& PropertiesJson, const TArray<FString>& Party);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void MatchmakerRemove(const FString& TicketId);

	// Chat
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void ChatJoin(const FString& ChannelId);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void ChatLeave(const FString& ChannelId);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void ChatSend(const FString& ChannelId, const FString& Content);

	// Presence
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void UpdatePresence(const FString& Status);

	// Voting
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void CastVote(const FString& VoteId, const FString& OptionId);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void UseVeto(const FString& VoteId);

	// World
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldList(const FString& Mode);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldCreate(const FString& Mode);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldFindOrCreate(const FString& Mode);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldJoin(const FString& WorldId);

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldLeave();

	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldInput(const FString& DataJson);

	// Direct messages
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void DmSend(const FString& RecipientId, const FString& Content);

	// Connection events
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWsConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWsDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWsMessage OnMessage;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWsError OnError;

	// Fires when the socket is closed or errors for an auth reason (server 1008
	// idle_auth_timeout, session_revoked, invalid_token). Treat as force
	// re-login rather than a blind reconnect.
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWsAuthExpired OnAuthExpired;

	// Typed events
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchState OnMatchState;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchEvent OnMatchEvent;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchJoined OnMatchJoined;

	/**
	 * Fires when the matchmaker forms a match including this player.
	 * Pair this with OnMatchJoined: matchmade flows fire OnMatchMatched only;
	 * direct-join flows (client-initiated match.join) fire OnMatchJoined only.
	 * Both signal "in a match — match.state will follow."
	 */
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchMatched OnMatchMatched;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchLeft OnMatchLeft;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchmakerQueued OnMatchmakerQueued;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiMatchmakerRemoved OnMatchmakerRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiChatReceived OnChatReceived;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiChatJoined OnChatJoined;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiChatLeft OnChatLeft;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiNotificationReceived OnNotificationReceived;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiPresenceUpdated OnPresenceUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiVoteCastOk OnVoteCastOk;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiVoteVetoOk OnVoteVetoOk;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldJoined OnWorldJoined;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldLeft OnWorldLeft;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldList OnWorldList;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldTick OnWorldTick;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldTerrain OnWorldTerrain;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldEvent OnWorldEvent;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiDmMessage OnDmMessage;

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiDmSent OnDmSent;

	// Fires on a dev-mode Lua script error triggered by this player's input.
	// Only emitted when the backend runs with ASOBI_DEV_ERRORS=true;
	// production keeps script errors server-side. Surface these in a dev
	// console/HUD rather than treating them as fatal.
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiGameError OnGameError;

	// Fires on a game.message push (game.send/2 from a Lua game script).
	// Unlike OnGameError, this fires unconditionally in production.
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiGameMessage OnGameMessage;

#if WITH_DEV_AUTOMATION_TESTS
	// Test-only entry point that drives the same dispatch path the live
	// WebSocket message callback uses. Lets unit tests feed canonical
	// fixtures through HandleMessage without standing up a real socket.
	void HandleMessageForTest(const FString& MessageString) { HandleMessage(MessageString); }
#endif

private:
	void Send(const FString& Type, const TSharedPtr<FJsonObject>& Payload);
	void HandleMessage(const FString& MessageString);
	FString SerializeJson(const TSharedPtr<FJsonObject>& Obj);

	TSharedPtr<IWebSocket> WebSocket;
	int32 NextCid = 1;
	FString LastAuthToken;
};

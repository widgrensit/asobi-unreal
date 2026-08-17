#pragma once

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "AsobiTypes.h"
#include "AsobiCore/Wire.h"
#include "AsobiWebSocket.generated.h"

class UAsobiClient;

/**
 * The shared error object an extension returns when it rejects an RPC call.
 */
USTRUCT(BlueprintType)
struct ASOBISDK_API FAsobiRpcError
{
	GENERATED_BODY()

	/** The one part to branch on. Never empty - an absent code reads "internal". */
	UPROPERTY(BlueprintReadOnly, Category = "Asobi|RPC")
	FString Code;

	/** For humans. May be reworded at any time - do not branch on it. */
	UPROPERTY(BlueprintReadOnly, Category = "Asobi|RPC")
	FString Message;

	/** Raw JSON. Details are defined by the extension, not by this SDK. */
	UPROPERTY(BlueprintReadOnly, Category = "Asobi|RPC")
	FString DetailsJson;
};

/**
 * Completion for a single RPC call. Error is null on success.
 */
using FAsobiRpcCallback = TFunction<void(const FString& ResultJson, const FAsobiRpcError* Error)>;

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
// The whole world.tick payload, unparsed, as a companion to FOnAsobiWorldTick.
//
// asobi core v0.89.0 added `zone`, `frame_seq` and `kf` to that payload, and
// FOnAsobiWorldTick has nowhere to put them: it is a fixed two-parameter
// signature. Widening it would change a BlueprintAssignable delegate and break
// every Blueprint node already bound to it, which no version bump can fix here
// because this SDK ships as copied source.
//
// So the fields arrive this way instead, matching what OnMatchState already does
// with its own payload. Bind whichever suits you; both fire for every frame.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWorldTickPayload, const FString&, PayloadJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiWorldAck, const FAsobiWorldAck&, Ack);

// A world.tick that arrived on the binary wire, already decoded.
//
// A plain C++ delegate rather than a BlueprintAssignable one, deliberately. The
// decoded frame holds a per-record map of variant values, which has no faithful
// Blueprint representation - flattening it into parallel typed maps to satisfy the
// reflection system would hand Blueprint users a worse shape than the JSON they
// already have, for a saving only C++ code can spend. Blueprint stays on
// OnWorldTickPayload; nothing there changes.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAsobiWorldTickBinary, const asobi::core::WireFrame&);
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

// A named push from a server extension, delivered via module.event. The app
// routes on the Event field; the inner event name is data, not a dispatch gate.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsobiModuleEvent, const FAsobiModuleEvent&, Event);

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

	// Gets the player into a live match of Mode, spawning one if there is none.
	// The match twin of WorldFindOrCreate, and the race-free alternative to
	// browsing match.list and then joining. Answers with match.joined, the same
	// reply as JoinMatch, so OnMatchJoined covers it.
	// The mode must set quick_play = true (match modes default to false) or the
	// server refuses with quick_play_disabled. A mode name that is unknown or
	// not configured is refused with not_found; the README lists the rest.
	// Requires asobi server v0.86.0 or later.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void MatchFindOrCreate(const FString& Mode);

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

	// Send one input to the world you are in. DataJson is the input map itself,
	// as a JSON object: it goes on the wire as the payload, because that is what
	// the server forwards to handle_input/3. An empty string sends an empty map;
	// anything that is not a JSON object is dropped with a log line rather than
	// going out as the empty map it would otherwise become.
	//
	// `data` is reserved at the top level of that map in one shape only: a
	// payload whose sole key is `data` mapped to an object is unwrapped, a
	// deprecated shape that goes at the next protocol break.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldInput(const FString& DataJson);

	// Send input stamped with a per-input Seq to opt into world.ack
	// reconciliation - the server echoes back the highest Seq it has consumed
	// via OnWorldAck. Seq rides as a top-level sibling of payload, not nested.
	// UFUNCTIONs cannot overload by name, so this is a distinct entry point from
	// WorldInput rather than an optional argument on it. DataJson is read exactly
	// as WorldInput reads it.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldInputWithSeq(const FString& DataJson, int64 Seq);

	// Ask the server to re-send one zone's complete baseline, after frames for it
	// went missing.
	//
	// Call this when that zone's `frame_seq` on OnWorldTickPayload jumps by more
	// than one. The reply is an ordinary world.tick for the zone with `kf` true,
	// listing every entity it holds: replace that zone's entities with it rather
	// than merging.
	//
	// ONE zone per call, never the whole interest ring. The ring is nine zones at
	// the default view radius, so asking for all of them turns a small request
	// into nine full baselines - and you already know which zone gapped, because
	// the sequence is per zone.
	//
	// Rate limited server-side to twice per ten seconds per player: ask once per
	// gap and wait for the keyframe rather than retrying. Requires asobi core
	// v0.89.0 or later; an older server answers `unknown_type`.
	UFUNCTION(BlueprintCallable, Category = "Asobi|WebSocket")
	void WorldResync(int64 ZoneX, int64 ZoneY);

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

	/**
	 * Every world.tick frame's complete payload, unparsed.
	 *
	 * Use this rather than OnWorldTick when you need the zone-aware fields core
	 * v0.89.0 added, which OnWorldTick's fixed signature cannot carry:
	 *
	 * - `zone` as [x, y]. KEY YOUR ENTITIES ON THIS. A player is subscribed to an
	 *   interest ring of several zones at once, each an independent server
	 *   process, and frames from two of them have no order relative to each
	 *   other. A crossing emits op:"r" from the zone being left and op:"a" from
	 *   the zone being entered, so merging every zone into one entity map is
	 *   last-writer-wins - and when the remove lands last the entity is gone for
	 *   good, because the server will not re-add something already in its own
	 *   baseline.
	 * - `frame_seq`, which counts frames the zone has broadcast and never skips,
	 *   so a jump means frames were lost. `tick` cannot serve this purpose: it
	 *   skips on the server's broadcast interval and is suppressed entirely on a
	 *   tick that changed nothing, so a gap in it is ambiguous. Note that
	 *   sequence tracking cannot see the crossing problem above either - both
	 *   zones' sequences stay perfectly contiguous through it.
	 * - `kf`, true when the frame is a complete baseline for its zone: replace
	 *   that zone's entities with it rather than merging. Adopt it even when
	 *   `frame_seq` moves BACKWARDS, because a zone restart resets the sequence
	 *   while the zone's identity does not.
	 *
	 * Ask for a fresh baseline with UAsobiWebSocket::WorldResync when a zone's
	 * frame_seq jumps.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldTickPayload OnWorldTickPayload;

	// Fires on a world.ack push - the highest world.input Seq the server has
	// consumed for you as of Ack.Tick. Fires only if you stamped a Seq via
	// WorldInputWithSeq; use it to reconcile client-side prediction.
	/**
	 * Every world.tick that arrived on the binary wire, decoded.
	 *
	 * Fires instead of OnWorldTick and OnWorldTickPayload for a connection that
	 * set bRequestBinaryWire, and never alongside them: re-serialising a decoded
	 * frame to fire them would hand back exactly the text-parsing cost the binary
	 * wire exists to remove.
	 *
	 * The frame's entity ids are already resolved from the wire's 2-byte slots, so
	 * Record.Id is the same id the JSON wire gives. It is empty only when the add
	 * that would have established the binding was lost - a FrameSeq gap, which
	 * WorldResync repairs.
	 *
	 * C++ only. See the delegate's own comment for why.
	 */
	FOnAsobiWorldTickBinary OnWorldTickBinary;

	/**
	 * Ask the server for the binary world.tick encoding: roughly a fifth of the
	 * bytes, and already decoded rather than text the game still has to parse.
	 *
	 * Set it before Connect. A server with the binary wire switched off answers
	 * `json` and this client silently stays on text, so read GrantedWire after
	 * OnConnected rather than assuming the request was honoured. Only world.tick is
	 * affected; every other frame is JSON text on both wires.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Asobi|WebSocket")
	bool bRequestBinaryWire = false;

	/** The wire the server actually granted: "json" or "binary". Valid after
	 * OnConnected. */
	UPROPERTY(BlueprintReadOnly, Category = "Asobi|WebSocket")
	FString GrantedWire = TEXT("json");

	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiWorldAck OnWorldAck;

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

	// Fires on a module.event push - a named event from a server extension.
	// The whole payload is surfaced; the app routes on Event.Event.
	UPROPERTY(BlueprintAssignable, Category = "Asobi|WebSocket")
	FOnAsobiModuleEvent OnModuleEvent;

#if WITH_DEV_AUTOMATION_TESTS
	// Test-only entry point that drives the same dispatch path the live
	// WebSocket message callback uses. Lets unit tests feed canonical
	// fixtures through HandleMessage without standing up a real socket.
	void HandleMessageForTest(const FString& MessageString) { HandleMessage(MessageString); }
#endif

	/**
	 * Call a server extension's RPC method.
	 *
	 * Correlated by cid, so several calls may be in flight at once and may
	 * answer out of order. Callback fires exactly once: with ResultJson and a
	 * null Error on success, or with an Error to branch on via its Code.
	 *
	 * Params may be null, which sends an empty object. Blueprint users want
	 * the "Asobi RPC" latent node instead - see UAsobiRpcAction.
	 */
	void Rpc(const FString& Method, const TSharedPtr<FJsonObject>& Params, FAsobiRpcCallback Callback);

private:
	void Send(const FString& Type, const TSharedPtr<FJsonObject>& Payload);

	/**
	 * Send and return the cid written, so a caller can await the reply. When Seq
	 * is set it is stamped as a top-level sibling of payload (numeric), for the
	 * world.input prediction path.
	 */
	FString SendWithCid(const FString& Type, const TSharedPtr<FJsonObject>& Payload,
	                    const TOptional<int64>& Seq = TOptional<int64>());

	/**
	 * The one send path behind WorldInput and WorldInputWithSeq. The payload IS
	 * the input map, so DataJson goes on the wire as payload; text that is not
	 * a JSON object is dropped and reported once rather than sent.
	 */
	void SendWorldInput(const FString& DataJson, const TOptional<int64>& Seq);

	/** Returns true if the frame was an RPC reply and was routed to its caller. */
	bool RouteRpcReply(const FString& Type, const FString& MessageString);

	void HandleMessage(const FString& MessageString);

	/** Decodes one complete binary frame and broadcasts OnWorldTickBinary. */
	void HandleBinaryFrame(const TArray<uint8>& Bytes);

	FString SerializeJson(const TSharedPtr<FJsonObject>& Obj);

	TSharedPtr<IWebSocket> WebSocket;

	/**
	 * Accumulates a fragmented binary frame. A world.tick fits one frame in
	 * practice - the steady state is under a kilobyte - but nothing in the
	 * WebSocket protocol promises that, and a decoder handed half a frame would
	 * report it as malformed rather than wait for the rest.
	 */
	TArray<uint8> BinaryBuffer;

	/**
	 * Holds the slot bindings, so it lives as long as the connection. One per
	 * socket; the bindings are established by the adds THIS connection received.
	 */
	TUniquePtr<asobi::core::WireDecoder> WireDecoder = MakeUnique<asobi::core::WireDecoder>();
	int32 NextCid = 1;
	FString LastAuthToken;

	/**
	 * Set once a world.input has been dropped for not being a JSON object. The
	 * mistake is the same on every frame of a 60Hz send loop, so it is reported
	 * once per socket instead of flooding the log.
	 */
	bool bLoggedBadWorldInput = false;

	/**
	 * In-flight RPC calls, keyed by the RAW cid token as it appears on the
	 * wire (quotes included). Raw because correlation only ever compares it to
	 * what we sent, so it never needs interpreting - and comparing tokens
	 * sidesteps a server that echoes 42 as 42.0.
	 */
	TMap<FString, FAsobiRpcCallback> PendingRpc;
};

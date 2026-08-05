// AsobiCore — engine-agnostic protocol identity for the Asobi SDK.
//
// This header is the single source of truth for "what server-event types
// exist" and "given a wire `type` string, which event identity does it map
// to?". It contains zero Unreal Engine types so it can be unit-tested on
// stock CI runners with plain C++17.
//
// The UE-side SDK (AsobiSDK) bridges into this at the FString/std::string
// seam in AsobiWebSocket.cpp; the Catch2/doctest-style tests under
// Source/AsobiCore/Tests/ are the canonical dispatch gate that runs on
// every PR (matches the love2d, js, godot, defold, dart per-SDK gate).
//
// Mirrors:
//   - asobi/priv/protocol/fixtures/  (canonical 35-event corpus)
//   - asobi-love2d/asobi/realtime.lua SERVER_EVENTS
//   - the dispatch table in AsobiSDK/Private/AsobiWebSocket.cpp

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace asobi::core
{

// Identity of every server-emitted event in the canonical asobi protocol
// corpus. Order is alphabetical to keep the table readable; do not rely on
// numeric values.
enum class EventId
{
    ChatJoined,
    ChatLeft,
    ChatMessage,
    DmMessage,
    DmSent,
    Error,
    GameError,
    GameMessage,
    MatchFinished,
    MatchJoined,
    MatchLeft,
    MatchList,
    MatchMatched,
    MatchMatchmakerExpired,
    MatchMatchmakerFailed,
    MatchState,
    MatchVoteResult,
    MatchVoteStart,
    MatchVoteTally,
    MatchVoteVetoed,
    MatchmakerQueued,
    MatchmakerRemoved,
    NotificationNew,
    PresenceUpdated,
    SessionConnected,
    SessionHeartbeat,
    VoteCastOk,
    VoteVetoOk,
    WorldFinished,
    WorldJoined,
    WorldLeft,
    WorldList,
    WorldPhaseChanged,
    WorldTerrain,
    WorldTick
};

// Canonical event count — keep in sync with EventId enum and the fixture
// corpus. Compile-time constant so tests can assert without runtime work.
constexpr std::size_t kEventCount = 35;

// Maps a wire-`type` string ("match.matched", "world.tick", ...) to its
// EventId. Returns nullopt if the type is unknown.
std::optional<EventId> ParseEventId(std::string_view Type);

// Inverse of ParseEventId. Returns the canonical wire string for an
// EventId. Result is a stable string_view into a static table.
std::string_view EventIdName(EventId Id);

// Every known event id, in stable order. Useful for tests that want to
// enumerate the corpus without duplicating the list.
const std::array<EventId, kEventCount>& AllKnownEvents();

// Extracts the `type` field from a JSON envelope. Engine-agnostic
// utility shared between SDKs that don't want to drag in a heavy JSON
// library just to identify the event before dispatching. Handles the
// minimal subset of JSON the Asobi protocol actually emits for the
// envelope:
//   {"type":"match.state","payload":{...},"cid":42}
// Whitespace, basic escapes (\\, \", \/, \n, \r, \t, \b, \f) and the
// type field appearing in any envelope position are supported. Nested
// objects/strings (including a `"type":"..."` inside `payload`) are
// skipped over.
//
// Returns nullopt if the JSON is malformed or has no top-level `type`
// field. The unit tests under AsobiCore/Tests/ rely on this to read
// fixtures without pulling in nlohmann::json.
std::optional<std::string> ParseEnvelopeType(std::string_view Json);

// Extracts the `cid` field from a JSON envelope as its RAW token - `"c-1"`
// including quotes, or `42` for a number. Raw rather than decoded because
// correlation only ever compares it to the token this SDK sent, so it never
// needs interpreting, and keeping it raw means a server that echoes a string
// and one that echoes a number both correlate without a type negotiation.
//
// Returns nullopt if the JSON is malformed or has no top-level `cid` - which
// is the normal case for a server push, as opposed to a reply.
std::optional<std::string> ParseEnvelopeCid(std::string_view Json);

// The shared error object an extension returns when it rejects a call.
struct RpcError
{
    // The one part a caller can branch on. Never empty: an absent code reads
    // as "internal", or a server defect and a domain outcome look identical.
    std::string Code;

    // For humans. May be reworded at any time - do not branch on it.
    std::string Message;

    // Raw JSON, because details are defined by the extension, not by us.
    std::string DetailsJson;
};

// The decoded form of an rpc.ok / rpc.error frame.
struct RpcReply
{
    bool bIsError = false;

    // rpc.ok only. The `result` object as raw JSON - the caller parses it into
    // whatever shape the extension documents. Never empty: an absent result
    // reads as "{}".
    std::string ResultJson;

    // rpc.error only.
    RpcError Error;
};

// Decodes an rpc.ok / rpc.error frame. Returns nullopt if the JSON is
// malformed. Anything that is not rpc.error is treated as a success, so the
// caller decides which frame types reach this at all.
std::optional<RpcReply> ParseRpcReply(std::string_view Json);

} // namespace asobi::core

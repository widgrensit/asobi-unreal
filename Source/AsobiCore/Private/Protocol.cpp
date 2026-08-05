#include "AsobiCore/Protocol.h"

#include "AsobiCore/Json.h"

#include <cstddef>

namespace asobi::core
{

namespace
{

// Single canonical table of (wire-string, EventId) pairs. Linear scan —
// the corpus is 35 entries, branch-predictor-friendly, and avoids
// dragging in <unordered_map> + hash overhead.
struct Entry
{
    std::string_view Wire;
    EventId Id;
};

constexpr Entry kTable[] = {
    {"chat.joined",              EventId::ChatJoined},
    {"chat.left",                EventId::ChatLeft},
    {"chat.message",             EventId::ChatMessage},
    {"dm.message",               EventId::DmMessage},
    {"dm.sent",                  EventId::DmSent},
    {"error",                    EventId::Error},
    {"game.error",               EventId::GameError},
    {"game.message",             EventId::GameMessage},
    {"match.finished",           EventId::MatchFinished},
    {"match.joined",             EventId::MatchJoined},
    {"match.left",               EventId::MatchLeft},
    {"match.list",               EventId::MatchList},
    {"match.matched",            EventId::MatchMatched},
    {"match.matchmaker_expired", EventId::MatchMatchmakerExpired},
    {"match.matchmaker_failed",  EventId::MatchMatchmakerFailed},
    {"match.state",              EventId::MatchState},
    {"match.vote_result",        EventId::MatchVoteResult},
    {"match.vote_start",         EventId::MatchVoteStart},
    {"match.vote_tally",         EventId::MatchVoteTally},
    {"match.vote_vetoed",        EventId::MatchVoteVetoed},
    {"matchmaker.queued",        EventId::MatchmakerQueued},
    {"module.error",             EventId::ModuleError},
    {"module.message",           EventId::ModuleMessage},
    {"matchmaker.removed",       EventId::MatchmakerRemoved},
    {"notification.new",         EventId::NotificationNew},
    {"presence.updated",         EventId::PresenceUpdated},
    {"session.connected",        EventId::SessionConnected},
    {"session.heartbeat",        EventId::SessionHeartbeat},
    {"vote.cast_ok",             EventId::VoteCastOk},
    {"vote.veto_ok",             EventId::VoteVetoOk},
    {"world.finished",           EventId::WorldFinished},
    {"world.joined",             EventId::WorldJoined},
    {"world.left",               EventId::WorldLeft},
    {"world.list",               EventId::WorldList},
    {"world.phase_changed",      EventId::WorldPhaseChanged},
    {"world.terrain",            EventId::WorldTerrain},
    {"world.tick",               EventId::WorldTick},
};

static_assert(sizeof(kTable) / sizeof(kTable[0]) == kEventCount,
              "kTable size must match kEventCount; update both when adding events");

} // namespace

std::optional<EventId> ParseEventId(std::string_view Type)
{
    for (const Entry& E : kTable)
    {
        if (E.Wire == Type)
        {
            return E.Id;
        }
    }
    return std::nullopt;
}

std::string_view EventIdName(EventId Id)
{
    for (const Entry& E : kTable)
    {
        if (E.Id == Id)
        {
            return E.Wire;
        }
    }
    return {};
}

const std::array<EventId, kEventCount>& AllKnownEvents()
{
    static const std::array<EventId, kEventCount> Cache = []
    {
        std::array<EventId, kEventCount> A{};
        for (std::size_t I = 0; I < kEventCount; ++I)
        {
            A[I] = kTable[I].Id;
        }
        return A;
    }();
    return Cache;
}

std::optional<std::string> ParseEnvelopeType(std::string_view Json)
{
    return json::TopLevelString(Json, "type");
}

std::optional<std::string> ParseEnvelopeCid(std::string_view Json)
{
    return json::RawPath(Json, {"cid"});
}

std::optional<RpcReply> ParseRpcReply(std::string_view Json)
{
    auto Type = ParseEnvelopeType(Json);
    if (!Type)
    {
        return std::nullopt;
    }

    RpcReply Reply;
    if (*Type == "rpc.error")
    {
        Reply.bIsError = true;
        auto Code = json::RawPath(Json, {"payload", "error", "code"});
        auto Message = json::RawPath(Json, {"payload", "error", "message"});
        auto Details = json::RawPath(Json, {"payload", "error", "details"});

        Reply.Error.Code = Code ? json::Unquote(*Code).value_or("internal") : "internal";
        if (Reply.Error.Code.empty())
        {
            Reply.Error.Code = "internal";
        }
        // Falls back to the code rather than to nothing, so an error is never
        // reported to a player with an empty message.
        Reply.Error.Message =
            Message ? json::Unquote(*Message).value_or(Reply.Error.Code) : Reply.Error.Code;
        Reply.Error.DetailsJson = Details.value_or("{}");
        return Reply;
    }

    Reply.bIsError = false;
    Reply.ResultJson = json::RawPath(Json, {"payload", "result"}).value_or("{}");
    return Reply;
}

} // namespace asobi::core

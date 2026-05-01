#include "AsobiCore/Protocol.h"

#include <cstddef>
#include <string>
#include <utility>

namespace asobi::core
{

namespace
{

// Single canonical table of (wire-string, EventId) pairs. Linear scan —
// the corpus is 32 entries, branch-predictor-friendly, and avoids
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
    {"match.finished",           EventId::MatchFinished},
    {"match.joined",             EventId::MatchJoined},
    {"match.left",               EventId::MatchLeft},
    {"match.matched",            EventId::MatchMatched},
    {"match.matchmaker_expired", EventId::MatchMatchmakerExpired},
    {"match.matchmaker_failed",  EventId::MatchMatchmakerFailed},
    {"match.state",              EventId::MatchState},
    {"match.vote_result",        EventId::MatchVoteResult},
    {"match.vote_start",         EventId::MatchVoteStart},
    {"match.vote_tally",         EventId::MatchVoteTally},
    {"match.vote_vetoed",        EventId::MatchVoteVetoed},
    {"matchmaker.queued",        EventId::MatchmakerQueued},
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

// Skips whitespace, returns new index.
std::size_t SkipWs(std::string_view S, std::size_t I)
{
    while (I < S.size())
    {
        const char C = S[I];
        if (C == ' ' || C == '\t' || C == '\n' || C == '\r')
        {
            ++I;
        }
        else
        {
            break;
        }
    }
    return I;
}

// Reads a JSON string starting at S[I] (must point at the opening quote).
// Returns the unescaped string and advances I past the closing quote. On
// malformed input returns nullopt and leaves I in an unspecified state.
std::optional<std::string> ReadString(std::string_view S, std::size_t& I)
{
    if (I >= S.size() || S[I] != '"')
    {
        return std::nullopt;
    }
    ++I;
    std::string Out;
    while (I < S.size())
    {
        const char C = S[I];
        if (C == '"')
        {
            ++I;
            return Out;
        }
        if (C == '\\')
        {
            if (I + 1 >= S.size())
            {
                return std::nullopt;
            }
            const char Esc = S[I + 1];
            switch (Esc)
            {
            case '"':  Out.push_back('"');  break;
            case '\\': Out.push_back('\\'); break;
            case '/':  Out.push_back('/');  break;
            case 'b':  Out.push_back('\b'); break;
            case 'f':  Out.push_back('\f'); break;
            case 'n':  Out.push_back('\n'); break;
            case 'r':  Out.push_back('\r'); break;
            case 't':  Out.push_back('\t'); break;
            case 'u':
                // Asobi envelopes do not use \uXXXX in the `type` field,
                // but consume the 4 hex digits to stay tolerant. We
                // intentionally do not decode the codepoint — anyone who
                // ships an event type with a non-ASCII codepoint can wire
                // up a real JSON lib in their fork.
                if (I + 5 >= S.size())
                {
                    return std::nullopt;
                }
                Out.append(S.substr(I + 2, 4));
                I += 4;
                break;
            default:
                return std::nullopt;
            }
            I += 2;
        }
        else
        {
            Out.push_back(C);
            ++I;
        }
    }
    return std::nullopt;
}

// Skips a JSON value at S[I]. Used to walk past payloads we do not care
// about while looking for the top-level `type`. Recurses for nested
// objects/arrays.
bool SkipValue(std::string_view S, std::size_t& I);

bool SkipObject(std::string_view S, std::size_t& I)
{
    if (I >= S.size() || S[I] != '{')
    {
        return false;
    }
    ++I;
    I = SkipWs(S, I);
    if (I < S.size() && S[I] == '}')
    {
        ++I;
        return true;
    }
    while (I < S.size())
    {
        I = SkipWs(S, I);
        auto Key = ReadString(S, I);
        if (!Key)
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I >= S.size() || S[I] != ':')
        {
            return false;
        }
        ++I;
        I = SkipWs(S, I);
        if (!SkipValue(S, I))
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I < S.size() && S[I] == ',')
        {
            ++I;
            continue;
        }
        if (I < S.size() && S[I] == '}')
        {
            ++I;
            return true;
        }
        return false;
    }
    return false;
}

bool SkipArray(std::string_view S, std::size_t& I)
{
    if (I >= S.size() || S[I] != '[')
    {
        return false;
    }
    ++I;
    I = SkipWs(S, I);
    if (I < S.size() && S[I] == ']')
    {
        ++I;
        return true;
    }
    while (I < S.size())
    {
        I = SkipWs(S, I);
        if (!SkipValue(S, I))
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I < S.size() && S[I] == ',')
        {
            ++I;
            continue;
        }
        if (I < S.size() && S[I] == ']')
        {
            ++I;
            return true;
        }
        return false;
    }
    return false;
}

bool SkipValue(std::string_view S, std::size_t& I)
{
    I = SkipWs(S, I);
    if (I >= S.size())
    {
        return false;
    }
    const char C = S[I];
    if (C == '{')
    {
        return SkipObject(S, I);
    }
    if (C == '[')
    {
        return SkipArray(S, I);
    }
    if (C == '"')
    {
        auto V = ReadString(S, I);
        return V.has_value();
    }
    if (C == 't' || C == 'f' || C == 'n')
    {
        // true / false / null
        while (I < S.size())
        {
            const char Ch = S[I];
            if ((Ch >= 'a' && Ch <= 'z'))
            {
                ++I;
            }
            else
            {
                break;
            }
        }
        return true;
    }
    // number — consume digits, sign, exponent, decimal
    if (C == '-' || C == '+' || (C >= '0' && C <= '9'))
    {
        while (I < S.size())
        {
            const char Ch = S[I];
            if ((Ch >= '0' && Ch <= '9') || Ch == '.' || Ch == '-' || Ch == '+'
                || Ch == 'e' || Ch == 'E')
            {
                ++I;
            }
            else
            {
                break;
            }
        }
        return true;
    }
    return false;
}

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
    std::size_t I = SkipWs(Json, 0);
    if (I >= Json.size() || Json[I] != '{')
    {
        return std::nullopt;
    }
    ++I;
    while (I < Json.size())
    {
        I = SkipWs(Json, I);
        if (I < Json.size() && Json[I] == '}')
        {
            return std::nullopt;
        }
        auto Key = ReadString(Json, I);
        if (!Key)
        {
            return std::nullopt;
        }
        I = SkipWs(Json, I);
        if (I >= Json.size() || Json[I] != ':')
        {
            return std::nullopt;
        }
        ++I;
        I = SkipWs(Json, I);
        if (*Key == "type")
        {
            return ReadString(Json, I);
        }
        if (!SkipValue(Json, I))
        {
            return std::nullopt;
        }
        I = SkipWs(Json, I);
        if (I < Json.size() && Json[I] == ',')
        {
            ++I;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace asobi::core

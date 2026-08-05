// RPC decoding unit test for AsobiCore.
//
// The reply half of the extension seam: given an rpc.ok / rpc.error frame off
// the wire, what reaches the caller. This is the whole of the decoding -
// AsobiWebSocket does nothing with a reply except look up the pending call by
// cid and hand it one of these.
//
// Pure C++17 + doctest, no Unreal Engine install required.
//
// Build & run:
//   cmake -S Source/AsobiCore -B build
//   cmake --build build
//   cd build && ctest --output-on-failure

#include "doctest.h"

#include "AsobiCore/Json.h"
#include "AsobiCore/Protocol.h"

#include <string>

using namespace asobi::core;

TEST_CASE("rpc.ok yields the result, not the envelope")
{
    auto Reply = ParseRpcReply(R"({"type":"rpc.ok","cid":"c-1","payload":{"result":{"reward":100}}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->bIsError == false);
    CHECK(Reply->ResultJson == R"({"reward":100})");
}

TEST_CASE("rpc.error carries the code callers branch on")
{
    auto Reply = ParseRpcReply(
        R"({"type":"rpc.error","cid":"c-1","payload":{"error":{)"
        R"("code":"quests.already_claimed","message":"This quest was already claimed.",)"
        R"("details":{}}}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->bIsError == true);
    CHECK(Reply->Error.Code == "quests.already_claimed");
    CHECK(Reply->Error.Message == "This quest was already claimed.");
    CHECK(Reply->Error.DetailsJson == "{}");
}

// Otherwise a server defect and a domain outcome look identical to a caller
// branching on Code.
TEST_CASE("an empty error object still gets a code")
{
    auto Reply = ParseRpcReply(R"({"type":"rpc.error","cid":"1","payload":{}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->Error.Code == "internal");
    CHECK(Reply->Error.Message == "internal");
    CHECK(Reply->Error.DetailsJson == "{}");
}

TEST_CASE("an absent result is an empty object, never empty string")
{
    auto Reply = ParseRpcReply(R"({"type":"rpc.ok","cid":"1","payload":{}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->ResultJson == "{}");
}

// Details are defined by the extension, so they reach the caller as raw JSON
// rather than flattened into something we guessed at.
TEST_CASE("nested details survive intact")
{
    auto Reply = ParseRpcReply(
        R"({"type":"rpc.error","cid":"1","payload":{"error":{)"
        R"("code":"quests.locked","message":"no",)"
        R"("details":{"quest":{"key":"daily","tier":[1,2]}}}}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->Error.DetailsJson == R"({"quest":{"key":"daily","tier":[1,2]}})");
}

// The reason the reader tracks string boundaries rather than counting braces.
TEST_CASE("braces inside a message do not truncate the value")
{
    auto Reply = ParseRpcReply(
        R"({"type":"rpc.error","cid":"1","payload":{"error":{)"
        R"("code":"bad","message":"expected } or ] here","details":{"n":1}}}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->Error.Message == "expected } or ] here");
    CHECK(Reply->Error.DetailsJson == R"({"n":1})");
}

TEST_CASE("escapes in a message are decoded")
{
    auto Reply = ParseRpcReply(
        R"({"type":"rpc.error","cid":"1","payload":{"error":{)"
        "\"code\":\"bad\",\"message\":\"line\\none\\t\\\"quoted\\\"\"}}}");
    REQUIRE(Reply.has_value());
    CHECK(Reply->Error.Message == "line\none\t\"quoted\"");
}

TEST_CASE("a result array is returned whole")
{
    auto Reply = ParseRpcReply(R"({"type":"rpc.ok","cid":"1","payload":{"result":[{"a":1},{"b":2}]}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->ResultJson == R"([{"a":1},{"b":2}])");
}

// A key deeper in the document must not be mistaken for the one being looked
// up at this level.
TEST_CASE("a shadowing key inside the result is not picked up")
{
    auto Reply = ParseRpcReply(R"({"type":"rpc.ok","cid":"1","payload":{"result":{"result":"inner"}}})");
    REQUIRE(Reply.has_value());
    CHECK(Reply->ResultJson == R"({"result":"inner"})");
}

TEST_CASE("malformed json is rejected rather than guessed at")
{
    CHECK_FALSE(ParseRpcReply("not json").has_value());
    CHECK_FALSE(ParseRpcReply("{").has_value());
    CHECK_FALSE(ParseRpcReply("{\"no\":\"type\"}").has_value());
}

// Correlation compares the cid to the token this SDK sent, so it is kept raw:
// a server echoing a string and one echoing a number both correlate without
// the SDK having to agree a type first. This SDK sends numbers today while the
// protocol guide documents strings, so both must work.
TEST_CASE("cid is read raw, string or number")
{
    auto AsString = ParseEnvelopeCid(R"({"type":"rpc.ok","cid":"c-1","payload":{}})");
    REQUIRE(AsString.has_value());
    CHECK(*AsString == R"("c-1")");

    auto AsNumber = ParseEnvelopeCid(R"({"type":"rpc.ok","cid":42,"payload":{}})");
    REQUIRE(AsNumber.has_value());
    CHECK(*AsNumber == "42");
}

// A server push is not a reply and has no cid; correlation must not match one.
TEST_CASE("a push without a cid reads as absent")
{
    CHECK_FALSE(ParseEnvelopeCid(R"({"type":"world.tick","payload":{"tick":1}})").has_value());
}

TEST_CASE("an absent path reads as absent, not as empty")
{
    CHECK_FALSE(asobi::core::json::RawPath(R"({"payload":{}})", {"payload", "error", "code"}).has_value());
    CHECK_FALSE(asobi::core::json::RawPath("{}", {"payload"}).has_value());
    CHECK(asobi::core::json::RawPath(R"({"a":{"b":7}})", {"a", "b"}).value_or("") == "7");
}

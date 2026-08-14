// Outbound world.input payload test for AsobiCore.
//
// The wire shape of an input frame: the payload IS the input map, so the
// caller's JSON goes out as it stands and is never nested under a `data` key.
// A payload whose sole key is `data` mapped to an object is unwrapped by the
// server, a deprecated shape that goes at the next protocol break.
//
// Lives here rather than beside AsobiWebSocket.cpp because this tier needs no
// Unreal Engine install: it runs on stock CI with plain C++17 + doctest, the
// same gate the dispatch and auth tests use.
//
// Build & run:
//   cmake -S Source/AsobiCore -B build
//   cmake --build build
//   cd build && ctest --output-on-failure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "AsobiCore/Protocol.h"

#include <string>

using namespace asobi::core;

TEST_CASE("an input object goes out as the payload itself, never wrapped")
{
    auto Payload = WorldInputPayload(R"({"move_x":1,"move_y":0})");
    REQUIRE(Payload.has_value());
    CHECK(*Payload == R"({"move_x":1,"move_y":0})");
}

// The frame has to stay valid JSON, and an input nobody supplied is an empty
// map rather than a missing payload.
TEST_CASE("an absent input is an empty map")
{
    CHECK(WorldInputPayload("").value_or("") == "{}");
    CHECK(WorldInputPayload("   ").value_or("") == "{}");
    CHECK(WorldInputPayload("\t\n").value_or("") == "{}");
}

TEST_CASE("leading whitespace does not hide the object")
{
    auto Payload = WorldInputPayload("  {\"kind\":\"move\"}");
    REQUIRE(Payload.has_value());
    CHECK(*Payload == "  {\"kind\":\"move\"}");
}

// None of these can be a payload, so left alone each becomes an empty map and
// the game watches its input do nothing.
TEST_CASE("anything that is not a JSON object is rejected rather than sent")
{
    CHECK_FALSE(WorldInputPayload("not json").has_value());
    CHECK_FALSE(WorldInputPayload("[1,2,3]").has_value());
    CHECK_FALSE(WorldInputPayload("  [1,2,3]").has_value());
    CHECK_FALSE(WorldInputPayload("null").has_value());
    CHECK_FALSE(WorldInputPayload("42").has_value());
    CHECK_FALSE(WorldInputPayload(R"("move")").has_value());
}

// `data` is an ordinary word for a game input field, and the server reserves
// it at the top level. The SDK still sends it verbatim: rewriting a caller's
// map is not the SDK's call, and the trap is tracked upstream.
TEST_CASE("a caller's own data key is passed through, not rewritten")
{
    auto Payload = WorldInputPayload(R"({"kind":"move","data":{"x":1}})");
    REQUIRE(Payload.has_value());
    CHECK(*Payload == R"({"kind":"move","data":{"x":1}})");
}

TEST_CASE("nested objects and arrays survive whole")
{
    auto Payload = WorldInputPayload(R"({"aim":{"x":0.5,"y":-1},"buttons":[1,2]})");
    REQUIRE(Payload.has_value());
    CHECK(*Payload == R"({"aim":{"x":0.5,"y":-1},"buttons":[1,2]})");
}

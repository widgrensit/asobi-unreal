// Dispatch unit test for AsobiCore.
//
// Loads every fixture from the canonical 33-event corpus
// (Source/AsobiSDK/Tests/Fixtures/) and asserts:
//   1. Each fixture's `type` string parses to the expected EventId.
//   2. Every EventId in AsobiCore has a vendored fixture (no stale ids).
//   3. Every fixture has a known EventId (no orphan fixtures).
//
// Mirrors the dispatch gates used by the love2d, godot, defold, dart,
// js, and unreal SDKs. This is the canonical CI dispatch test for the
// Unreal SDK — it runs on stock ubuntu-24.04 runners with plain
// g++/clang and doctest, no Unreal Engine install required.
//
// Build & run:
//   cmake -S Source/AsobiCore -B build
//   cmake --build build
//   cd build && ctest --output-on-failure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "AsobiCore/Protocol.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

namespace fs = std::filesystem;
using namespace asobi::core;

// CMake configures the absolute fixture directory at build time so the
// test binary works no matter where ctest is invoked from. Falls back to
// a path relative to the binary if the macro isn't defined (e.g. someone
// hand-builds the .cpp without the cmake config).
fs::path FixtureDir()
{
#ifdef ASOBI_CORE_FIXTURE_DIR
    return fs::path(ASOBI_CORE_FIXTURE_DIR);
#else
    // Best-effort fallback: ../../AsobiSDK/Tests/Fixtures/ relative to cwd.
    return fs::path("..") / ".." / "AsobiSDK" / "Tests" / "Fixtures";
#endif
}

std::string ReadFile(const fs::path& Path)
{
    std::ifstream In(Path, std::ios::binary);
    if (!In.is_open())
    {
        return {};
    }
    std::ostringstream Ss;
    Ss << In.rdbuf();
    return Ss.str();
}

std::vector<fs::path> ListFixtures()
{
    std::vector<fs::path> Out;
    const fs::path Dir = FixtureDir();
    if (!fs::exists(Dir) || !fs::is_directory(Dir))
    {
        return Out;
    }
    for (const auto& Entry : fs::directory_iterator(Dir))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".json")
        {
            Out.push_back(Entry.path());
        }
    }
    std::sort(Out.begin(), Out.end());
    return Out;
}

} // namespace

TEST_CASE("ParseEventId round-trips every known id")
{
    for (EventId Id : AllKnownEvents())
    {
        const std::string_view Wire = EventIdName(Id);
        REQUIRE_FALSE(Wire.empty());
        const auto Parsed = ParseEventId(Wire);
        REQUIRE(Parsed.has_value());
        CHECK(*Parsed == Id);
    }
}

TEST_CASE("ParseEventId rejects unknown wire strings")
{
    CHECK_FALSE(ParseEventId("not.a.real.event").has_value());
    CHECK_FALSE(ParseEventId("").has_value());
    CHECK_FALSE(ParseEventId("match.matched.extra").has_value());
    CHECK_FALSE(ParseEventId("MATCH.MATCHED").has_value());
}

TEST_CASE("AllKnownEvents has no duplicates and matches kEventCount")
{
    const auto& Events = AllKnownEvents();
    CHECK(Events.size() == kEventCount);
    std::set<EventId> Seen;
    for (EventId Id : Events)
    {
        Seen.insert(Id);
    }
    CHECK(Seen.size() == kEventCount);
}

TEST_CASE("ParseEnvelopeType extracts the top-level type field")
{
    CHECK(ParseEnvelopeType(R"({"type":"match.matched","payload":{}})")
          == std::optional<std::string>("match.matched"));
    CHECK(ParseEnvelopeType(R"({  "type"  :  "world.tick" , "cid": 42 })")
          == std::optional<std::string>("world.tick"));
    // Nested payload with its own `type` key must not shadow the outer one.
    CHECK(ParseEnvelopeType(R"({"payload":{"type":"inner"},"type":"chat.message"})")
          == std::optional<std::string>("chat.message"));
    // Missing `type` returns nullopt.
    CHECK_FALSE(ParseEnvelopeType(R"({"payload":{}})").has_value());
    // Garbage returns nullopt.
    CHECK_FALSE(ParseEnvelopeType("not json").has_value());
    CHECK_FALSE(ParseEnvelopeType("").has_value());
}

TEST_CASE("every fixture dispatches to a known EventId")
{
    const auto Fixtures = ListFixtures();
    REQUIRE_MESSAGE(!Fixtures.empty(),
        "no fixtures found under " << FixtureDir().string()
        << " — check ASOBI_CORE_FIXTURE_DIR");

    std::set<EventId> Dispatched;
    for (const auto& Path : Fixtures)
    {
        const std::string Stem = Path.stem().string();
        const std::string Raw = ReadFile(Path);
        REQUIRE_MESSAGE(!Raw.empty(), "fixture empty: " << Path.string());

        const auto EnvelopeType = ParseEnvelopeType(Raw);
        REQUIRE_MESSAGE(EnvelopeType.has_value(),
            "envelope has no `type` field: " << Path.string());
        CHECK_MESSAGE(*EnvelopeType == Stem,
            "envelope type mismatch in " << Path.string()
            << ": filename says " << Stem << ", body says " << *EnvelopeType);

        const auto Id = ParseEventId(*EnvelopeType);
        REQUIRE_MESSAGE(Id.has_value(),
            "fixture `" << *EnvelopeType
            << "` has no EventId — add it to AsobiCore::ParseEventId or remove the fixture");
        Dispatched.insert(*Id);
    }
    CHECK_MESSAGE(Dispatched.size() == kEventCount,
        "expected " << kEventCount << " distinct events, got " << Dispatched.size());
}

TEST_CASE("every EventId has a vendored fixture")
{
    const auto Fixtures = ListFixtures();
    std::set<std::string> FixtureStems;
    for (const auto& Path : Fixtures)
    {
        FixtureStems.insert(Path.stem().string());
    }
    for (EventId Id : AllKnownEvents())
    {
        const std::string Wire = std::string(EventIdName(Id));
        CHECK_MESSAGE(FixtureStems.count(Wire) == 1,
            "EventId `" << Wire
            << "` has no fixture — add Source/AsobiSDK/Tests/Fixtures/"
            << Wire << ".json");
    }
}

TEST_CASE("fixture corpus matches kEventCount exactly")
{
    const auto Fixtures = ListFixtures();
    CHECK_MESSAGE(Fixtures.size() == kEventCount,
        "expected " << kEventCount << " fixtures, found " << Fixtures.size()
        << " — drift between corpus and EventId table");
}

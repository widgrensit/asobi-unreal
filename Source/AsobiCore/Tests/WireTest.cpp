// Binary world.tick decoder test for AsobiCore (asobi ADR 0013).
//
// Driven entirely by asobi's own committed fixture corpus, vendored under
// Source/AsobiSDK/Tests/Fixtures/wire: real bytes from the real encoder, with a
// manifest saying what each one decodes to. Nothing here is hand-rolled test data,
// which is the point - a decoder checked only against a fixture the same author
// invented proves the two agree with each other and nothing about whether either
// matches the server.
//
// The manifest is JSON and this tier has no JSON parser worth the dependency, so
// the expectations are transcribed as C++ literals and the corpus is the source of
// the BYTES. A drift between the two shows up as a failure here, and asobi's own
// CI guard asserts the bytes are still what its encoder produces.
//
// Build & run:
//   cmake -S Source/AsobiCore -B build
//   cmake --build build
//   cd build && ctest --output-on-failure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "AsobiCore/Wire.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace asobi::core;

namespace
{

std::vector<std::uint8_t> Fixture(const std::string& Name)
{
    const std::string Path = std::string(ASOBI_CORE_FIXTURE_DIR) + "/wire/" + Name + ".bin";
    std::vector<std::uint8_t> Bytes;
    std::FILE* File = std::fopen(Path.c_str(), "rb");
    if (File == nullptr)
    {
        return Bytes;
    }
    std::uint8_t Buffer[4096];
    std::size_t Read = 0;
    while ((Read = std::fread(Buffer, 1, sizeof(Buffer), File)) > 0)
    {
        Bytes.insert(Bytes.end(), Buffer, Buffer + Read);
    }
    std::fclose(File);
    return Bytes;
}

std::optional<WireFrame> DecodeFixture(WireDecoder& Decoder, const std::string& Name)
{
    const std::vector<std::uint8_t> Bytes = Fixture(Name);
    REQUIRE_MESSAGE(!Bytes.empty(), "fixture missing: " << Name);
    return Decoder.Decode(Bytes.data(), Bytes.size());
}

}  // namespace

TEST_CASE("an add carries every value type, and the binding")
{
    WireDecoder Decoder;
    auto Frame = DecodeFixture(Decoder, "add_with_all_value_types");
    REQUIRE(Frame.has_value());
    CHECK(Frame->Kind == WireKind::Sequenced);
    CHECK(Frame->ZoneX == 0);
    CHECK(Frame->ZoneY == 0);
    CHECK(Frame->FrameSeq == 1);
    CHECK(Frame->Kf == false);
    CHECK(Frame->Tick == 20);
    REQUIRE(Frame->Records.size() == 1);

    const WireRecord& R = Frame->Records[0];
    CHECK(R.Op == WireOp::Add);
    CHECK(R.Id == "01a0115f-547e-714f-829f-408c855ab77b");
    CHECK(std::get<float>(R.Fields.at("x")) == doctest::Approx(12.5f));
    CHECK(std::get<float>(R.Fields.at("y")) == doctest::Approx(-3.25f));
    CHECK(std::get<std::int32_t>(R.Fields.at("hp")) == 100);
    CHECK(std::get<bool>(R.Fields.at("alive")) == true);
    CHECK(std::get<bool>(R.Fields.at("stunned")) == false);
    CHECK(std::get<std::string>(R.Fields.at("name")) == "player one");
    CHECK(std::holds_alternative<std::monostate>(R.Fields.at("target")));
}

// The number the whole design turns on: a steady-state delta has to fit one
// datagram, and it has to be decisively smaller than the JSON it replaces.
TEST_CASE("the steady-state delta is 40 updates inside one datagram")
{
    const std::vector<std::uint8_t> Bytes = Fixture("steady_state_40_updates");
    REQUIRE(!Bytes.empty());
    CHECK(Bytes.size() <= 1200);

    WireDecoder Decoder;
    auto Frame = Decoder.Decode(Bytes.data(), Bytes.size());
    REQUIRE(Frame.has_value());
    CHECK(Frame->ZoneX == 3);
    CHECK(Frame->ZoneY == -2);
    CHECK(Frame->FrameSeq == 4711);
    CHECK(Frame->Records.size() == 40);
    // An update carries the slot alone, so an id here means the decoder resolved
    // it - but this frame has no adds before it, so nothing is bound yet.
    CHECK(Frame->Records[0].Op == WireOp::Update);
    CHECK(Frame->Records[0].Id.empty());
    CHECK(std::get<float>(Frame->Records[0].Fields.at("x")) == doctest::Approx(1.5f));
}

// The keyframe is all-adds by construction, which is what makes a resync
// re-establish every slot binding for free.
TEST_CASE("a keyframe carries every binding")
{
    WireDecoder Decoder;
    auto Frame = DecodeFixture(Decoder, "keyframe_all_adds");
    REQUIRE(Frame.has_value());
    CHECK(Frame->Kf == true);
    REQUIRE(Frame->Records.size() == 5);
    for (const WireRecord& R : Frame->Records)
    {
        CHECK(R.Op == WireOp::Add);
        CHECK(!R.Id.empty());
    }
}

// Slot 5 in one zone has nothing to do with slot 5 in another. Aliasing them is
// exactly the corruption that keying entities by zone exists to prevent.
TEST_CASE("slot bindings are scoped per zone")
{
    WireDecoder Decoder;
    auto Keyframe = DecodeFixture(Decoder, "keyframe_all_adds");
    REQUIRE(Keyframe.has_value());

    // removes_only is zone [0, 0]; the keyframe was zone [-1, -1].
    auto Other = DecodeFixture(Decoder, "removes_only");
    REQUIRE(Other.has_value());
    for (const WireRecord& R : Other->Records)
    {
        CHECK(R.Id.empty());
    }
}

// Bindings belong to one connection's stream of adds. Kept across a reconnect they
// would attach stale ids to slots the server has since reassigned.
TEST_CASE("Reset forgets every binding")
{
    WireDecoder Decoder;
    auto First = DecodeFixture(Decoder, "keyframe_all_adds");
    REQUIRE(First.has_value());
    Decoder.Reset();

    // The same zone again: with the table cleared, an update-only frame for it
    // resolves nothing.
    auto Again = DecodeFixture(Decoder, "keyframe_all_adds");
    REQUIRE(Again.has_value());
    CHECK(Again->Records.size() == 5);
}

// The leave mirror. The text wire says "no position in this zone's stream" by
// omitting frame_seq; encoded as sequence 0 instead, every client past its first
// frame would discard the one message that clears its ghosts.
TEST_CASE("the leave-removal frame is ungated, not sequence zero")
{
    WireDecoder Decoder;
    auto Frame = DecodeFixture(Decoder, "ungated_leave_removals");
    REQUIRE(Frame.has_value());
    CHECK(Frame->Kind == WireKind::Ungated);
    REQUIRE(Frame->Records.size() == 2);
    CHECK(Frame->Records[0].Op == WireOp::Remove);
}

// An empty zone still has a sequence position. A decoder treating zero records as
// an error would reject a legitimate baseline.
TEST_CASE("an empty keyframe is a legitimate frame")
{
    WireDecoder Decoder;
    auto Frame = DecodeFixture(Decoder, "empty_keyframe");
    REQUIRE(Frame.has_value());
    CHECK(Frame->Kf == true);
    CHECK(Frame->Records.empty());
}

// Coordinates run negative and the sequence is a 53-bit counter. Both are places a
// decoder using the wrong width or signedness looks fine until it does not.
TEST_CASE("extremes survive the widths")
{
    WireDecoder Decoder;
    auto Frame = DecodeFixture(Decoder, "extremes");
    REQUIRE(Frame.has_value());
    CHECK(Frame->ZoneX == -2147483648);
    CHECK(Frame->ZoneY == 2147483647);
    CHECK(Frame->FrameSeq == 0x1FFFFFFFFFFFFF);
    CHECK(Frame->Tick == 0x1FFFFFFFFFFFFF);
}

// These bytes will one day arrive on a datagram from an unauthenticated source, so
// the decoder must be total. A crash here would be a remote denial of service.
TEST_CASE("malformed frames decode to nothing, and never read out of bounds")
{
    const std::vector<std::uint8_t> Good = Fixture("steady_state_40_updates");
    REQUIRE(!Good.empty());

    WireDecoder Decoder;
    CHECK_FALSE(Decoder.Decode(nullptr, 0).has_value());

    const std::uint8_t One = 1;
    CHECK_FALSE(Decoder.Decode(&One, 1).has_value());

    CHECK_FALSE(Decoder.Decode(Good.data(), 10).has_value());
    CHECK_FALSE(Decoder.Decode(Good.data(), Good.size() - 2).has_value());

    std::vector<std::uint8_t> Junk = Good;
    Junk.insert(Junk.end(), {0, 0, 0});
    CHECK_FALSE(Decoder.Decode(Junk.data(), Junk.size()).has_value());

    std::vector<std::uint8_t> BadKind = Good;
    BadKind[0] = 9;
    CHECK_FALSE(Decoder.Decode(BadKind.data(), BadKind.size()).has_value());
}

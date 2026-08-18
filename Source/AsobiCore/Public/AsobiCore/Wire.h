// AsobiCore — the binary world.tick decoder (asobi ADR 0013).
//
// Same information as the JSON frame in roughly a fifth of the bytes, and it
// arrives already typed rather than as text the game still has to parse. That is
// the real saving on this SDK: OnWorldTickPayload hands you the payload unparsed
// and OnWorldTick's fixed signature cannot carry the frame's own fields at all.
//
// Zero Unreal Engine types, like the rest of AsobiCore, so the decoder is unit
// tested against asobi's own committed fixture corpus on stock CI runners with
// plain C++17. The UE-side bridge lives in AsobiWebSocket.cpp.
//
// Layout, every multi-byte value LITTLE-endian - not the usual choice for a wire
// format, and deliberate: Godot's byte readers have no big-endian counterpart, so
// the wire follows the runtime with the least room to spare.
//
//   frame    Kind:8, ZX:32, ZY:32, FrameSeq:64, Kf:8, Tick:64,
//            DictLen:8, Dict, RecCount:16, Records
//   dict     for each name: Len:8, Name/utf8            (at most 32 names)
//   record   Op:8, Slot:16, Gen:8, [IdLen:8, Id/utf8]?, FieldCount:8, Fields
//   field    Type:3, Idx:5, Value                       (one header byte)
//
// Mirrors:
//   - asobi/src/ws/asobi_wire.erl
//   - asobi/priv/wire_fixtures/  (the corpus this is tested against)

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace asobi::core
{

// One entity field's value. The wire carries six scalar types and nothing else;
// a game putting a list or a nested map in an entity keeps the whole zone on the
// text wire, so the two wires can never disagree about what an entity is.
using WireValue = std::variant<std::monostate, float, std::int32_t, bool, std::string>;

enum class WireOp
{
    Add,
    Update,
    Remove,
};

// A frame either holds a position in its zone's sequence or does not. The text
// wire says the latter by omitting frame_seq, which a fixed-layout binary frame
// cannot do, so the distinction rides a header byte. The ungated frame is the
// removal list for a zone being left, and it must be applied WITHOUT the sequence
// check - gating it would leave a client holding ghosts forever.
enum class WireKind
{
    Sequenced,
    Ungated,
};

struct WireRecord
{
    WireOp Op = WireOp::Update;

    // The entity id, empty only when the frame's slot has no binding yet.
    //
    // A binding is established by an add, so empty means the add that would have
    // established it was lost. The record is still reported rather than dropped,
    // because the frame genuinely says this slot changed; the frame_seq gap that
    // caused it is what drives the resync that repairs the mapping.
    std::string Id;

    // The slot's generation, advancing every time it is rebound to a different
    // entity. Redundant on this ordered, reliable wire - the sequencing already
    // bounds the reuse hazard - and carried anyway so a client also running the
    // datagram plane can keep ONE slot table for both carriers.
    std::uint8_t Gen = 0;

    std::map<std::string, WireValue> Fields;
};

struct WireFrame
{
    WireKind Kind = WireKind::Sequenced;

    // KEY YOUR ENTITIES ON THIS. A player is subscribed to an interest ring of
    // several zones at once, each an independent server process, and frames from
    // two of them have no order relative to each other.
    std::int32_t ZoneX = 0;
    std::int32_t ZoneY = 0;

    // Contiguous per zone, advancing only on a frame actually sent, so a jump by
    // more than one means frames were lost. Meaningless on an ungated frame; check
    // Kind before reading it.
    std::int64_t FrameSeq = 0;

    // True when the frame is a complete baseline for its zone: replace, do not
    // merge. Adopt it unconditionally, including when FrameSeq moves BACKWARDS,
    // because a zone restart resets the sequence while the zone's identity does
    // not.
    bool Kf = false;

    std::int64_t Tick = 0;

    std::vector<WireRecord> Records;
};

// Decodes binary world.tick frames, resolving the wire's 2-byte entity slots back
// to entity ids.
//
// Slot tables are kept per ZONE, because slot 5 in one zone has nothing to do with
// slot 5 in another and a single flat table would alias entities across zones -
// the same corruption that keying entities by zone exists to prevent. An add
// REPLACES any binding already on its slot, which is what makes the server's slot
// reuse safe.
//
// One instance per connection, and it holds state, so it is not thread-safe.
class WireDecoder
{
public:
    // Decodes one frame, or returns nothing if the bytes are malformed.
    //
    // Never throws and never reads out of bounds: these bytes come off the network
    // and will one day arrive on a datagram from an unauthenticated source, so a
    // crash here would be a remote denial of service.
    std::optional<WireFrame> Decode(const std::uint8_t* Bytes, std::size_t Length);

    // Forgets every slot binding, for a reconnect. Bindings are established by the
    // adds THIS connection received, so carrying them over would attach stale ids
    // to slots the server has since handed to different entities. The keyframe that
    // follows a reconnect rebuilds the whole table anyway.
    void Reset();

private:
    std::map<std::int64_t, std::map<std::uint16_t, std::string>> Slots;
};

}  // namespace asobi::core

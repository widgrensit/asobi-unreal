#include "AsobiCore/Wire.h"

#include <cstring>

namespace asobi::core
{
namespace
{

constexpr std::uint8_t KindSequenced = 1;
constexpr std::uint8_t KindUngated = 2;

constexpr int TF32 = 0;
constexpr int TI32 = 1;
constexpr int TTrue = 2;
constexpr int TFalse = 3;
constexpr int TStr = 4;
constexpr int TNull = 5;

// The header alone, before any dictionary or record.
constexpr std::size_t MinFrame = 27;

// Assembled by shifting rather than by memcpy-and-cast, so the decoder reads
// little-endian regardless of the host's own byte order. Unreal targets
// big-endian platforms too.
std::uint16_t ReadU16(const std::uint8_t* B, std::size_t I)
{
    return static_cast<std::uint16_t>(B[I] | (B[I + 1] << 8));
}

std::int32_t ReadI32(const std::uint8_t* B, std::size_t I)
{
    const std::uint32_t V = static_cast<std::uint32_t>(B[I]) |
                            (static_cast<std::uint32_t>(B[I + 1]) << 8) |
                            (static_cast<std::uint32_t>(B[I + 2]) << 16) |
                            (static_cast<std::uint32_t>(B[I + 3]) << 24);
    return static_cast<std::int32_t>(V);
}

std::int64_t ReadI64(const std::uint8_t* B, std::size_t I)
{
    std::uint64_t V = 0;
    for (int K = 7; K >= 0; --K)
    {
        V = (V << 8) | static_cast<std::uint64_t>(B[I + static_cast<std::size_t>(K)]);
    }
    return static_cast<std::int64_t>(V);
}

// The one type with no shift path. Assemble the four bytes in little-endian order
// and memcpy into a float, which is the only standard-conformant reinterpretation.
float ReadF32(const std::uint8_t* B, std::size_t I)
{
    const std::uint32_t Bits = static_cast<std::uint32_t>(B[I]) |
                               (static_cast<std::uint32_t>(B[I + 1]) << 8) |
                               (static_cast<std::uint32_t>(B[I + 2]) << 16) |
                               (static_cast<std::uint32_t>(B[I + 3]) << 24);
    float Out = 0.0f;
    std::memcpy(&Out, &Bits, sizeof(Out));
    return Out;
}

}  // namespace

void WireDecoder::Reset()
{
    Slots.clear();
}

std::optional<WireFrame> WireDecoder::Decode(const std::uint8_t* B, std::size_t Len)
{
    if (B == nullptr || Len < MinFrame)
    {
        return std::nullopt;
    }

    const std::uint8_t KindByte = B[0];
    if (KindByte != KindSequenced && KindByte != KindUngated)
    {
        return std::nullopt;
    }

    WireFrame Frame;
    Frame.Kind = KindByte == KindSequenced ? WireKind::Sequenced : WireKind::Ungated;
    Frame.ZoneX = ReadI32(B, 1);
    Frame.ZoneY = ReadI32(B, 5);
    Frame.FrameSeq = ReadI64(B, 9);
    Frame.Kf = B[17] != 0;
    Frame.Tick = ReadI64(B, 18);

    std::size_t Pos = 26;
    const std::size_t DictLen = B[Pos++];
    std::vector<std::string> Names;
    Names.reserve(DictLen);
    for (std::size_t I = 0; I < DictLen; ++I)
    {
        if (Pos >= Len)
        {
            return std::nullopt;
        }
        const std::size_t NameLen = B[Pos++];
        if (Pos + NameLen > Len)
        {
            return std::nullopt;
        }
        Names.emplace_back(reinterpret_cast<const char*>(B + Pos), NameLen);
        Pos += NameLen;
    }

    if (Pos + 2 > Len)
    {
        return std::nullopt;
    }
    const std::size_t RecCount = ReadU16(B, Pos);
    Pos += 2;

    const std::int64_t ZoneKey =
        (static_cast<std::int64_t>(Frame.ZoneX) << 32) ^
        (static_cast<std::int64_t>(static_cast<std::uint32_t>(Frame.ZoneY)));
    std::map<std::uint16_t, std::string>& Table = Slots[ZoneKey];

    Frame.Records.reserve(RecCount);
    for (std::size_t R = 0; R < RecCount; ++R)
    {
        if (Pos + 3 > Len)
        {
            return std::nullopt;
        }
        const std::uint8_t OpByte = B[Pos];
        if (OpByte > 2)
        {
            return std::nullopt;
        }
        const std::uint16_t Slot = ReadU16(B, Pos + 1);
        Pos += 3;

        WireRecord Record;
        Record.Op = OpByte == 0 ? WireOp::Add : (OpByte == 1 ? WireOp::Update : WireOp::Remove);

        if (Record.Op == WireOp::Add)
        {
            if (Pos >= Len)
            {
                return std::nullopt;
            }
            const std::size_t IdLen = B[Pos++];
            if (Pos + IdLen > Len)
            {
                return std::nullopt;
            }
            Record.Id.assign(reinterpret_cast<const char*>(B + Pos), IdLen);
            Pos += IdLen;
            // An add ESTABLISHES the binding and replaces whatever was there. Slots
            // are reused once freed, so a stale binding surviving an add would attach
            // the wrong entity to every later update on that slot.
            Table[Slot] = Record.Id;
        }
        else
        {
            const auto Found = Table.find(Slot);
            if (Found != Table.end())
            {
                Record.Id = Found->second;
            }
        }

        if (Pos >= Len)
        {
            return std::nullopt;
        }
        const std::size_t FieldCount = B[Pos++];
        for (std::size_t F = 0; F < FieldCount; ++F)
        {
            if (Pos >= Len)
            {
                return std::nullopt;
            }
            const std::uint8_t Header = B[Pos++];
            const int Type = Header >> 5;
            const std::size_t Idx = Header & 0x1F;
            if (Idx >= Names.size())
            {
                return std::nullopt;
            }
            const std::string& Key = Names[Idx];
            switch (Type)
            {
                case TF32:
                    if (Pos + 4 > Len)
                    {
                        return std::nullopt;
                    }
                    Record.Fields[Key] = ReadF32(B, Pos);
                    Pos += 4;
                    break;
                case TI32:
                    if (Pos + 4 > Len)
                    {
                        return std::nullopt;
                    }
                    Record.Fields[Key] = ReadI32(B, Pos);
                    Pos += 4;
                    break;
                case TTrue:
                    Record.Fields[Key] = true;
                    break;
                case TFalse:
                    Record.Fields[Key] = false;
                    break;
                case TStr:
                {
                    if (Pos + 2 > Len)
                    {
                        return std::nullopt;
                    }
                    const std::size_t StrLen = ReadU16(B, Pos);
                    Pos += 2;
                    if (Pos + StrLen > Len)
                    {
                        return std::nullopt;
                    }
                    Record.Fields[Key] =
                        std::string(reinterpret_cast<const char*>(B + Pos), StrLen);
                    Pos += StrLen;
                    break;
                }
                case TNull:
                    Record.Fields[Key] = std::monostate{};
                    break;
                default:
                    return std::nullopt;
            }
        }

        // Released only AFTER the record is built, so the frame that announces an
        // entity's departure still carries its id.
        if (Record.Op == WireOp::Remove)
        {
            Table.erase(Slot);
        }

        Frame.Records.push_back(std::move(Record));
    }

    if (Pos != Len)
    {
        // Trailing bytes mean the frame and this decoder disagree about the layout,
        // and accepting it would hand the game a half-read frame.
        return std::nullopt;
    }

    return Frame;
}

}  // namespace asobi::core

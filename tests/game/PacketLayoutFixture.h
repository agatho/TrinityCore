/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_TESTS_PACKET_LAYOUT_FIXTURE_H
#define TRINITY_TESTS_PACKET_LAYOUT_FIXTURE_H

/*
 * Wire-layout harness for ClientPacket::Read() implementations.
 *
 * WHAT THIS IS FOR
 * ----------------
 * The usual way to test a packet is to Write() it and Read() it back. That only
 * proves Write() and Read() agree with *each other*: if both are wrong in the
 * same way the round trip still passes, and for a CMSG the server never calls
 * Write() at all, so the round trip tests code that never runs in production.
 *
 * These tests take the opposite approach. The expected byte layout is written
 * out by hand, as a constant, taken from an independent transcription of the
 * *client's own* serializer for that opcode (client 12.0.7 build 68275; see
 * tools/cmsg_sweep, `cmsg_layouts_68275.json`). Nothing in this file calls any
 * of our serialization code to produce the fixture bytes - integers are emitted
 * little-endian by hand, PackedGuid is encoded from the documented mask format,
 * and bit runs are packed MSB-first the way ByteBuffer::ReadBits() consumes them.
 *
 * So a failure here means our Read() disagrees with the client, not that two
 * halves of our own code drifted apart.
 *
 * WHAT AssertLayout() PROVES
 * --------------------------
 * Given the exact byte count the client emits, it checks three things:
 *   1. exact buffer      -> Read() succeeds and rpos() lands exactly on size().
 *   2. buffer minus 1 B  -> Read() throws ByteBufferPositionException. This is
 *                           the over-read guard: it proves the reader genuinely
 *                           needs every byte and does not stop early.
 *   3. buffer plus 1 B   -> Read() succeeds and stops at the layout size. This
 *                           is the under-read guard, and it also catches a
 *                           reader that papers over a mismatch with rfinish().
 *
 * DETERMINISM
 * -----------
 * Nothing here touches the database, DB2 stores, the clock or randomness.
 * Test guids are built with HighGuid::Uniq because ObjectGuidFactory::CreateUniq
 * is constexpr and, unlike the Player/WorldObject factories, does not consult
 * the sRealmList singleton.
 */

#include "tc_catch2.h"

#include "ByteBuffer.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Packet.h"
#include "WorldPacket.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace PacketLayout
{
    // Builds a raw wire buffer from a token layout, without using any of our
    // own serialization code. Chain the field calls on a named instance:
    //
    //     Wire wire;
    //     wire.U64(0x1122334455667788).PGuid(guid);
    class Wire
    {
    public:
        Wire& U8(uint8 value)
        {
            Align();
            _bytes.push_back(value);
            return *this;
        }

        Wire& U16(uint16 value) { return LittleEndian(value, 2); }
        Wire& U32(uint32 value) { return LittleEndian(value, 4); }
        Wire& U64(uint64 value) { return LittleEndian(value, 8); }

        Wire& F32(float value)
        {
            static_assert(sizeof(float) == 4, "wire floats are 4 bytes");
            uint32 raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            return U32(raw);
        }

        // PackedGuid: uint16 little-endian mask of which of the 16 guid bytes are
        // non-zero, followed by exactly those non-zero bytes, low byte first.
        Wire& PGuid(ObjectGuid const& guid)
        {
            Align();

            uint8 guidBytes[ObjectGuid::BytesSize] = { };
            for (std::size_t i = 0; i < 8; ++i)
            {
                guidBytes[i]     = uint8((guid.GetRawValue(0) >> (8 * i)) & 0xFF);
                guidBytes[8 + i] = uint8((guid.GetRawValue(1) >> (8 * i)) & 0xFF);
            }

            uint16 mask = 0;
            for (std::size_t i = 0; i < ObjectGuid::BytesSize; ++i)
                if (guidBytes[i])
                    mask = uint16(mask | (1u << i));

            LittleEndian(mask, 2);
            for (std::size_t i = 0; i < ObjectGuid::BytesSize; ++i)
                if (guidBytes[i])
                    _bytes.push_back(guidBytes[i]);

            return *this;
        }

        // A run of `bitCount` bits, packed the way ByteBuffer::ReadBits() reads
        // them: most significant bit of the value first, filling each byte from
        // its high bits down.
        Wire& Bits(uint32 bitCount, uint64 value)
        {
            while (bitCount)
            {
                if (_bitPos >= 8)
                {
                    _bytes.push_back(0);
                    _bitPos = 0;
                }

                uint32 const available = 8 - _bitPos;
                uint32 const take = std::min(available, bitCount);
                uint64 const chunk = (value >> (bitCount - take)) & ((UI64LIT(1) << take) - 1);

                _bytes.back() = uint8(_bytes.back() | uint8(chunk << (available - take)));
                _bitPos += take;
                bitCount -= take;
            }

            return *this;
        }

        std::vector<uint8> const& Bytes() const { return _bytes; }

    private:
        // A byte-sized field always starts on a byte boundary: ByteBuffer::read<T>()
        // calls ResetBitPos(), discarding whatever is left of the in-flight bit byte.
        void Align() { _bitPos = 8; }

        Wire& LittleEndian(uint64 value, std::size_t byteCount)
        {
            Align();
            for (std::size_t i = 0; i < byteCount; ++i)
                _bytes.push_back(uint8((value >> (8 * i)) & 0xFF));
            return *this;
        }

        std::vector<uint8> _bytes;
        uint32 _bitPos = 8;
    };

    inline WorldPacket MakePacket(OpcodeClient opcode, std::vector<uint8> bytes)
    {
        WorldPacket packet(std::move(bytes), CONNECTION_TYPE_DEFAULT);
        packet.SetOpcode(opcode);
        return packet;
    }

    struct ReadOutcome
    {
        bool ThrewPositionException = false;    // ran off the end of the buffer
        bool ThrewOtherException = false;       // rejected the contents for some other reason
        std::size_t Consumed = 0;               // rpos() after Read()
        std::size_t Size = 0;                   // bytes fed in
    };

    template <typename PacketType>
    ReadOutcome ReadOnce(OpcodeClient opcode, std::vector<uint8> bytes)
    {
        ReadOutcome outcome;
        outcome.Size = bytes.size();

        PacketType packet(MakePacket(opcode, std::move(bytes)));
        try
        {
            packet.Read();
        }
        catch (ByteBufferPositionException const&)
        {
            outcome.ThrewPositionException = true;
        }
        catch (ByteBufferException const&)
        {
            outcome.ThrewOtherException = true;
        }

        outcome.Consumed = packet.GetRawPacket()->rpos();
        return outcome;
    }

    // Assert that PacketType::Read() consumes exactly the client's layout - no
    // more, no less. See the file header for what each of the three cases proves.
    template <typename PacketType>
    void AssertLayout(OpcodeClient opcode, Wire const& wire)
    {
        std::vector<uint8> const expected = wire.Bytes();
        REQUIRE_FALSE(expected.empty());

        {
            INFO("exact buffer, " << expected.size() << " bytes");
            ReadOutcome const outcome = ReadOnce<PacketType>(opcode, expected);
            CHECK_FALSE(outcome.ThrewPositionException);
            CHECK_FALSE(outcome.ThrewOtherException);
            CHECK(outcome.Consumed == expected.size());
        }

        {
            INFO("truncated buffer, " << (expected.size() - 1) << " bytes (over-read guard)");
            std::vector<uint8> const truncated(expected.begin(), expected.end() - 1);
            ReadOutcome const outcome = ReadOnce<PacketType>(opcode, truncated);
            CHECK(outcome.ThrewPositionException);
        }

        {
            INFO("padded buffer, " << (expected.size() + 1) << " bytes (under-read guard)");
            std::vector<uint8> padded = expected;
            padded.push_back(0xCD);
            ReadOutcome const outcome = ReadOnce<PacketType>(opcode, padded);
            CHECK_FALSE(outcome.ThrewPositionException);
            CHECK_FALSE(outcome.ThrewOtherException);
            CHECK(outcome.Consumed == expected.size());
        }
    }

    // Guids for fixtures. HighGuid::Uniq keeps ObjectGuid construction constexpr
    // and free of singletons; the high qword is (1 << 58), so byte 15 is non-zero
    // and the packed reader will not discard the guid.
    inline ObjectGuid ShortGuid() { return ObjectGuid::Create<HighGuid::Uniq>(UI64LIT(0xFF)); }
    inline ObjectGuid LongGuid()  { return ObjectGuid::Create<HighGuid::Uniq>(UI64LIT(0x0102030405060708)); }
    inline ObjectGuid OtherGuid() { return ObjectGuid::Create<HighGuid::Uniq>(UI64LIT(0x00FF00FF00FF00FF)); }
}

#endif // TRINITY_TESTS_PACKET_LAYOUT_FIXTURE_H

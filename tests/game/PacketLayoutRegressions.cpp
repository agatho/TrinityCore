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

/*
 * Regression tests for two wire-parsing defect classes we have actually shipped,
 * plus a self-check on the layout harness itself.
 *
 *  1. A field the reader does not want still occupies bytes on the wire. Dropping
 *     it (rather than skipping over it) silently shifts everything after it.
 *  2. A raw uint64 is 8 fixed bytes; a PackedGuid is a 2-byte mask plus one byte
 *     per set mask bit. Reading one as the other was the root cause of several
 *     real defects found by the client-serializer sweep, among them
 *     CMSG_GARRISON_GET_MISSION_REWARD (client writes a raw uint64, we read a
 *     PackedGuid) and CMSG_QUERY_GARRISON_PET_NAME (client writes a raw uint64
 *     then a PackedGuid, we read a single PackedGuid).
 */

#include "PacketLayoutFixture.h"

#include <cstddef>

#include "GuildPackets.h"
#include "MailPackets.h"

using namespace PacketLayout;

namespace
{
    // Emits `value` the way it would look if someone had packed a bare uint64
    // through the PackedGuid encoder: a 16-bit mask covering the low 8 bytes,
    // then the non-zero bytes. Deliberately built from Wire's byte primitives so
    // it stays independent of ObjectGuid's own writer.
    void AppendPackedUInt64(Wire& wire, uint64 value)
    {
        uint16 mask = 0;
        for (std::size_t i = 0; i < 8; ++i)
            if (uint8((value >> (8 * i)) & 0xFF))
                mask = uint16(mask | (1u << i));

        wire.U16(mask);
        for (std::size_t i = 0; i < 8; ++i)
            if (uint8 const byte = uint8((value >> (8 * i)) & 0xFF))
                wire.U8(byte);
    }

    bool ConsumedCleanly(ReadOutcome const& outcome)
    {
        return !outcome.ThrewPositionException && !outcome.ThrewOtherException && outcome.Consumed == outcome.Size;
    }
}

TEST_CASE("Layout harness encodes the wire the way ByteBuffer reads it", "[packet][layout][harness]")
{
    // The harness is the oracle, so it gets pinned first. These byte counts are
    // worked out by hand from the PackedGuid format: uint16 mask, then one byte
    // per set bit, low byte first.

    SECTION("PackedGuid is a mask plus only the non-zero bytes")
    {
        // Uniq guid, low qword 0xFF, high qword 1 << 58: byte 0 and byte 15 are
        // set, so mask == 0x8001 and two data bytes follow.
        Wire shortGuid;
        shortGuid.PGuid(ShortGuid());
        std::vector<uint8> const expected = { 0x01, 0x80, 0xFF, 0x04 };
        CHECK(shortGuid.Bytes() == expected);

        // Low qword 0x0102030405060708 -> 8 data bytes, plus byte 15 -> 9 total.
        Wire longGuid;
        longGuid.PGuid(LongGuid());
        CHECK(longGuid.Bytes().size() == std::size_t(2 + 9));

        // Low qword 0x00FF00FF00FF00FF -> bytes 0, 2, 4, 6 set, plus byte 15.
        Wire otherGuid;
        otherGuid.PGuid(OtherGuid());
        CHECK(otherGuid.Bytes().size() == std::size_t(2 + 5));
    }

    SECTION("PackedGuid encoding agrees with ObjectGuid's own writer")
    {
        // A cross-check, not the oracle: if these two disagree, one of them is
        // wrong and every PackedGuid assertion in the suite is suspect.
        ObjectGuid const guids[] = { ShortGuid(), LongGuid(), OtherGuid() };
        for (ObjectGuid const& guid : guids)
        {
            Wire wire;
            wire.PGuid(guid);

            WorldPacket buffer;
            buffer << guid;

            REQUIRE(buffer.size() == wire.Bytes().size());
            for (std::size_t i = 0; i < buffer.size(); ++i)
                CHECK(buffer[i] == wire.Bytes()[i]);
        }
    }

    SECTION("a bit run costs a whole byte and is packed most significant bit first")
    {
        Wire twoBits;
        twoBits.Bits(2, 0x3);
        REQUIRE(twoBits.Bytes().size() == std::size_t(1));
        CHECK(twoBits.Bytes()[0] == 0xC0);

        // Nine bits do not fit in one byte.
        Wire nineBits;
        nineBits.Bits(9, 0x1FF);
        CHECK(nineBits.Bytes().size() == std::size_t(2));

        // A byte-sized field after a partial bit run starts on the next byte:
        // ByteBuffer::read<T>() resets the bit position and the remainder is lost.
        Wire mixed;
        mixed.U8(0x11).Bits(1, 1).U8(0x22);
        CHECK(mixed.Bytes().size() == std::size_t(3));
        CHECK(mixed.Bytes()[1] == 0x80);
    }

    SECTION("integers are emitted little-endian")
    {
        Wire wire;
        wire.U32(0x11223344);
        std::vector<uint8> const expected = { 0x44, 0x33, 0x22, 0x11 };
        CHECK(wire.Bytes() == expected);
    }
}

TEST_CASE("A field the reader does not want must still be consumed", "[packet][regression]")
{
    // No reader in this tree currently calls ByteBuffer::read_skip<T>(), so this
    // pins the primitive that the fix for that defect class depends on: skipping
    // a field has to advance rpos by exactly the field's width, and has to leave
    // the bit position in the same state a real read would.
    //
    // The failure mode this guards against is a reader that simply omits an
    // unwanted field instead of skipping it. That parses without throwing and
    // leaves a tail, which is exactly what AssertLayout()'s exact-buffer case
    // catches - but only if the total size is asserted, which is the point.

    SECTION("read_skip advances by exactly sizeof(T)")
    {
        Wire wire;
        wire.U32(0x11223344).U64(UI64LIT(0x1122334455667788)).U16(0x1234);
        REQUIRE(wire.Bytes().size() == std::size_t(14));

        std::vector<uint8> bytes = wire.Bytes();
        ByteBuffer buffer(std::move(bytes));
        REQUIRE(buffer.rpos() == std::size_t(0));

        buffer.read_skip<uint32>();
        CHECK(buffer.rpos() == std::size_t(4));

        buffer.read_skip<uint64>();
        CHECK(buffer.rpos() == std::size_t(12));

        // The field after the skipped ones must still land correctly.
        CHECK(buffer.read<uint16>() == uint16(0x1234));
        CHECK(buffer.rpos() == buffer.size());
    }

    SECTION("read_skip past the end throws instead of silently clamping")
    {
        Wire wire;
        wire.U32(0x11223344);

        std::vector<uint8> bytes = wire.Bytes();
        ByteBuffer buffer(std::move(bytes));
        buffer.read_skip<uint16>();
        REQUIRE(buffer.rpos() == std::size_t(2));

        CHECK_THROWS_AS(buffer.read_skip<uint32>(), ByteBufferPositionException);
    }

    SECTION("read_skip after a bit run starts on the next byte boundary")
    {
        Wire wire;
        wire.Bits(1, 1).U32(0x11223344);
        REQUIRE(wire.Bytes().size() == std::size_t(5));

        std::vector<uint8> bytes = wire.Bytes();
        ByteBuffer buffer(std::move(bytes));
        CHECK(buffer.ReadBit());
        REQUIRE(buffer.rpos() == std::size_t(1));

        buffer.read_skip<uint32>();
        CHECK(buffer.rpos() == buffer.size());
    }

    SECTION("a reader that skips a field still ends on the client's layout size")
    {
        // CMSG_GUILD_BANK_DEPOSIT_MONEY is pguid u64. A reader that did not care
        // about the money value would have to skip 8 bytes, not drop the field.
        Wire wire;
        wire.PGuid(LongGuid()).U64(UI64LIT(0x0102030405060708));
        std::size_t const layoutSize = wire.Bytes().size();
        REQUIRE(layoutSize == std::size_t(11 + 8));

        std::vector<uint8> bytes = wire.Bytes();
        ByteBuffer buffer(std::move(bytes));
        ObjectGuid banker;
        buffer >> banker;
        CHECK(banker == LongGuid());

        buffer.read_skip<uint64>();
        CHECK(buffer.rpos() == layoutSize);
    }
}

TEST_CASE("PackedGuid and raw uint64 are not interchangeable", "[packet][regression]")
{
    SECTION("CMSG_MAIL_RETURN_TO_SENDER reads a raw uint64 then a PackedGuid")
    {
        uint64 const mailId = UI64LIT(0x1122334455667788);

        Wire correct;
        correct.U64(mailId).PGuid(LongGuid());
        // 8 raw bytes, then a 2-byte mask and 9 guid bytes.
        REQUIRE(correct.Bytes().size() == std::size_t(8 + 11));

        AssertLayout<WorldPackets::Mail::MailReturnToSender>(CMSG_MAIL_RETURN_TO_SENDER, correct);

        // Now the same two values, but with MailID packed as if it were a guid.
        // If the reader ever treats MailID as a PackedGuid this buffer is what it
        // would accept - so it must not parse cleanly today.
        Wire mailIdPacked;
        AppendPackedUInt64(mailIdPacked, mailId);
        mailIdPacked.PGuid(LongGuid());
        REQUIRE(mailIdPacked.Bytes().size() == std::size_t((2 + 8) + 11));

        ReadOutcome const outcome =
            ReadOnce<WorldPackets::Mail::MailReturnToSender>(CMSG_MAIL_RETURN_TO_SENDER, mailIdPacked.Bytes());
        CHECK_FALSE(ConsumedCleanly(outcome));
    }

    SECTION("CMSG_MAIL_RETURN_TO_SENDER rejects a raw 16-byte guid")
    {
        // The mirror defect: writing the guid unpacked, as two raw qwords.
        Wire guidUnpacked;
        guidUnpacked.U64(UI64LIT(0x1122334455667788))
                    .U64(LongGuid().GetRawValue(0))
                    .U64(LongGuid().GetRawValue(1));

        ReadOutcome const outcome =
            ReadOnce<WorldPackets::Mail::MailReturnToSender>(CMSG_MAIL_RETURN_TO_SENDER, guidUnpacked.Bytes());
        CHECK_FALSE(ConsumedCleanly(outcome));
    }

    SECTION("CMSG_GUILD_BANK_DEPOSIT_MONEY reads a PackedGuid then a raw uint64")
    {
        uint64 const money = UI64LIT(0x0102030405060708);

        Wire correct;
        correct.PGuid(LongGuid()).U64(money);
        REQUIRE(correct.Bytes().size() == std::size_t(11 + 8));

        AssertLayout<WorldPackets::Guild::GuildBankDepositMoney>(CMSG_GUILD_BANK_DEPOSIT_MONEY, correct);

        // Money packed as if it were a guid: 10 bytes instead of 8.
        Wire moneyPacked;
        moneyPacked.PGuid(LongGuid());
        AppendPackedUInt64(moneyPacked, money);
        REQUIRE(moneyPacked.Bytes().size() == std::size_t(11 + (2 + 8)));

        ReadOutcome const outcome =
            ReadOnce<WorldPackets::Guild::GuildBankDepositMoney>(CMSG_GUILD_BANK_DEPOSIT_MONEY, moneyPacked.Bytes());
        CHECK_FALSE(ConsumedCleanly(outcome));
    }

    SECTION("the packed length of a guid depends on its value")
    {
        // Which is why a fixed 8-byte assumption cannot be right for a guid:
        // the same reader has to cope with all of these widths.
        Wire shortGuid;
        shortGuid.PGuid(ShortGuid()).U64(UI64LIT(1));
        AssertLayout<WorldPackets::Guild::GuildBankDepositMoney>(CMSG_GUILD_BANK_DEPOSIT_MONEY, shortGuid);
        CHECK(shortGuid.Bytes().size() == std::size_t(4 + 8));

        Wire otherGuid;
        otherGuid.PGuid(OtherGuid()).U64(UI64LIT(1));
        AssertLayout<WorldPackets::Guild::GuildBankDepositMoney>(CMSG_GUILD_BANK_DEPOSIT_MONEY, otherGuid);
        CHECK(otherGuid.Bytes().size() == std::size_t(7 + 8));

        Wire longGuid;
        longGuid.PGuid(LongGuid()).U64(UI64LIT(1));
        AssertLayout<WorldPackets::Guild::GuildBankDepositMoney>(CMSG_GUILD_BANK_DEPOSIT_MONEY, longGuid);
        CHECK(longGuid.Bytes().size() == std::size_t(11 + 8));
    }
}

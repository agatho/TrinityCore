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

#include "tc_catch2.h"

#include "MajorFactionPackets.h"
#include "WorldPacket.h"

// Phase 10K - Wire-format tests for the SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE
// builder. The packet's byte layout was reconstructed from IDA decompilation
// of the 12.0.5.67186 client (sub_7FF75C0EB140); these tests assert the
// server side encodes exactly that format.

TEST_CASE("[MajorFactions] CovenantRenownSendCatchupState: empty payload writes single header byte 0", "[MajorFactions][packets]")
{
    WorldPackets::MajorFactions::CovenantRenownSendCatchupState pkt;
    WorldPacket const* serialized = pkt.Write();

    REQUIRE(serialized->size() == 1);
    REQUIRE(serialized->contents()[0] == 0x00);
}

TEST_CASE("[MajorFactions] CovenantRenownSendCatchupState: one entry writes header (8<<1)=0x10 + 8B payload", "[MajorFactions][packets]")
{
    WorldPackets::MajorFactions::CovenantRenownSendCatchupState pkt;
    pkt.Entries.push_back({ .FactionID = 2507 /* Dragonscale */, .CatchupPercent = 42 });

    WorldPacket const* serialized = pkt.Write();
    uint8 const* data = serialized->contents();

    REQUIRE(serialized->size() == 1 + 8);

    // Header: (payloadLen << 1) & 0xFE  -> payloadLen=8 -> header = 0x10.
    REQUIRE(data[0] == 0x10);

    // Payload: int32 FactionID little-endian, int32 CatchupPercent little-endian.
    REQUIRE(data[1] == uint8(2507 & 0xFF));
    REQUIRE(data[2] == uint8((2507 >> 8) & 0xFF));
    REQUIRE(data[3] == 0);
    REQUIRE(data[4] == 0);
    REQUIRE(data[5] == 42);
    REQUIRE(data[6] == 0);
    REQUIRE(data[7] == 0);
    REQUIRE(data[8] == 0);
}

TEST_CASE("[MajorFactions] CovenantRenownSendCatchupState: 15-entry packet fits exactly in the 127-byte header cap", "[MajorFactions][packets]")
{
    WorldPackets::MajorFactions::CovenantRenownSendCatchupState pkt;
    for (int32 i = 0; i < 15; ++i)
        pkt.Entries.push_back({ .FactionID = 2500 + i, .CatchupPercent = i * 6 });

    WorldPacket const* serialized = pkt.Write();
    REQUIRE(serialized->size() == 1 + 15 * 8);

    // Header byte = (120 << 1) & 0xFE = 240 = 0xF0.
    REQUIRE(serialized->contents()[0] == 0xF0);
}

TEST_CASE("[MajorFactions] CovenantRenownSendCatchupState: header bit 0 is always clear", "[MajorFactions][packets]")
{
    // Per IDA decomp, the client masks bit 0 off before right-shifting by 1
    // to recover the length. Verify it is never set.
    for (int32 n = 0; n <= 15; ++n)
    {
        WorldPackets::MajorFactions::CovenantRenownSendCatchupState pkt;
        for (int32 i = 0; i < n; ++i)
            pkt.Entries.push_back({ .FactionID = 1 + i, .CatchupPercent = 50 });

        WorldPacket const* serialized = pkt.Write();
        REQUIRE((serialized->contents()[0] & 0x01) == 0);
    }
}

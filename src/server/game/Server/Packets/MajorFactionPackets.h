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

#ifndef TRINITYCORE_MAJOR_FACTION_PACKETS_H
#define TRINITYCORE_MAJOR_FACTION_PACKETS_H

#include "Packet.h"

namespace WorldPackets
{
    namespace MajorFactions
    {
        // CMSG_COVENANT_RENOWN_REQUEST_CATCHUP_STATE (0x3B0111).
        // The client sends this when opening the Renown UI (covenant or major
        // faction). Empty payload - the server replies with the current
        // catchup state.
        class RequestCatchupState final : public ClientPacket
        {
        public:
            explicit RequestCatchupState(WorldPacket&& packet) : ClientPacket(CMSG_COVENANT_RENOWN_REQUEST_CATCHUP_STATE, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE (0x42030D).
        //
        // Wire format at build 12.0.7.68275+: a SINGLE 1-bit bool - "is
        // accelerated renown catch-up active for this player right now".
        // Evidence: C:\dumps\AREA_grand_factions_LAYOUTS_68275.md, opcode
        // 0x42030d (struct sub_7FF7290B72C0 = one bool(1 bit)).
        //
        // The former Phase 10E entry-list format (header byte + array of
        // (FactionID, CatchupPercent) records) was reconstructed from the
        // 12.0.5.67186 client and is obsolete - the 68275 client reads only
        // the bit. Per-faction renown state reaches the client through the
        // reputation/currency sync paths instead, not through this packet.
        class CovenantRenownSendCatchupState final : public ServerPacket
        {
        public:
            CovenantRenownSendCatchupState()
                : ServerPacket(SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE, 1) { }

            WorldPacket const* Write() override;

            bool IsActive = false;
        };
    }
}

#endif // TRINITYCORE_MAJOR_FACTION_PACKETS_H

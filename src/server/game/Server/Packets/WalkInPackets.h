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

#ifndef TRINITYCORE_WALK_IN_PACKETS_H
#define TRINITYCORE_WALK_IN_PACKETS_H

#include "Packet.h"

namespace WorldPackets
{
    namespace WalkIn
    {
        // "Walk-in" is the 12.x term for the private, queue free instance session behind a delve or
        // follower dungeon entrance: C_PartyInfo.IsPartyWalkIn, InstanceDifficulty.lua picks
        // WalkInTexture when IsInDelve(), and LeaveWalkInParty() is C_PartyInfo.DelveTeleportOut().

        // Values the client distinguishes, read from handler RVA 0x21938A0 and its jump table at
        // RVA 0x21938F0. Only three bits are on the wire, so 5..7 are all the client can express
        // beyond the named ones - and it maps every one of them to the same text.
        enum class WalkInResultCode : uint8
        {
            Success                 = 0,    ///< no client feedback at all
            PlayerDead              = 1,    ///< ERR_PLAYER_DEAD (GameError 171)
            NotWhileFalling         = 2,    ///< ERR_NOT_WHILE_FALLING (GameError 764)
            NotWhileFatigued        = 3,    ///< ERR_NOT_WHILE_FATIGUED (GameError 766)
            InvalidTeleportLocation = 4,    ///< ERR_INVALID_TELEPORT_LOCATION (GameError 850)
            LockedOut               = 5     ///< ERR_CLIENT_LOCKED_OUT (GameError 172); 6 and 7 show the same text
        };

        // Every value above has a producer in WorldSession::HandleDelveTeleportOut, which mirrors
        // the condition list LFGMgr::TeleportPlayer already uses for the same operation - see the
        // comment there. NotWhileFatigued comes from Player::IsMirrorTimerActive(FATIGUE_TIMER),
        // which this core does have; LockedOut carries the cases that have no dedicated wire value
        // (vehicle, transport, charm, Freeze) because ERR_CLIENT_LOCKED_OUT is the generic "not
        // right now" text the UI itself uses for them.
        // UNVERIFIED: which condition RETAIL puts behind NotWhileFatigued and behind 5, 6 and 7.
        // The values and their texts are read out of the jump table at RVA 0x21938F0, and 5..7 are
        // three distinct codes the client renders identically, so the client cannot tell us what
        // distinguishes them. No recording shows a server sending any of them. The mapping above is
        // this core's, chosen to match its own instance exit rules, not read off retail.

        // Empty request sent by C_PartyInfo.DelveTeleportOut (client RVA 0x12DCAC0, serializer
        // RVA 0x6DC690 writes the opcode and no payload). The client pre-checks dead, falling,
        // on-transport and fatigued itself before it sends.
        class DelveTeleportOut final : public ClientPacket
        {
        public:
            explicit DelveTeleportOut(WorldPacket&& packet) : ClientPacket(CMSG_DELVE_TELEPORT_OUT, std::move(packet)) { }

            void Read() override { }
        };

        // The whole message is one byte: three result bits plus five fill bits.
        class WalkInResult final : public ServerPacket
        {
        public:
            explicit WalkInResult() : ServerPacket(SMSG_WALK_IN_RESULT, 1) { }

            WorldPacket const* Write() override;

            WalkInResultCode Result = WalkInResultCode::Success;
        };
    }
}

#endif // TRINITYCORE_WALK_IN_PACKETS_H

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

#ifndef TRINITYCORE_DUEL_PACKETS_H
#define TRINITYCORE_DUEL_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"

namespace WorldPackets
{
    namespace Duel
    {
        class CanDuel final : public ClientPacket
        {
        public:
            explicit CanDuel(WorldPacket&& packet) : ClientPacket(CMSG_CAN_DUEL, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
            bool ToTheDeath = false;
        };

        class CanDuelResult final : public ServerPacket
        {
        public:
            explicit CanDuelResult() : ServerPacket(SMSG_CAN_DUEL_RESULT, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid TargetGUID;
            bool Result = false;
        };

        // Confirms to both duellists that the duel is on. Arbiter first: the consumer 0x265A520 stores field 1
        // into xmmword_7FF7877FFDB8 and field 2 into xmmword_7FF7877FFDC8, the same two globals SMSG_DUEL_REQUESTED
        // fills in the same order. ...FDB8 is the only guid C.AcceptDuel/C.CancelDuel/C.ForfeitDuel (0x265A940,
        // 0x265A9B0, 0x265AA20) put into CMSG_DUEL_RESPONSE, and they use its HighGuid type field (>> 58) as the
        // "a duel is running" predicate; ...FDC8 is set from SMSG_CAN_DUEL_RESULT's only field, TargetGUID.
        // That makes this packet load-bearing rather than cosmetic: without it (or a preceding DUEL_REQUESTED)
        // the client cannot fill CMSG_DUEL_RESPONSE at all, so the player can neither accept, cancel nor forfeit.
        class DuelArranged final : public ServerPacket
        {
        public:
            explicit DuelArranged() : ServerPacket(SMSG_DUEL_ARRANGED, 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid ArbiterGUID;
            ObjectGuid RequestedByGUID;
        };

        class DuelComplete final : public ServerPacket
        {
        public:
            explicit DuelComplete() : ServerPacket(SMSG_DUEL_COMPLETE, 1) { }

            WorldPacket const* Write() override;

            bool Started = false;
        };

        class DuelCountdown final : public ServerPacket
        {
        public:
            explicit DuelCountdown(uint32 countdown) : ServerPacket(SMSG_DUEL_COUNTDOWN, 4), Countdown(countdown) { }

            WorldPacket const* Write() override;

            uint32 Countdown = 0;
        };

        class DuelInBounds final : public ServerPacket
        {
        public:
            explicit DuelInBounds() : ServerPacket(SMSG_DUEL_IN_BOUNDS, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class DuelOutOfBounds final : public ServerPacket
        {
        public:
            explicit DuelOutOfBounds() : ServerPacket(SMSG_DUEL_OUT_OF_BOUNDS, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class DuelRequested final : public ServerPacket
        {
        public:
            explicit DuelRequested() : ServerPacket(SMSG_DUEL_REQUESTED, 16 * 3 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid ArbiterGUID;
            ObjectGuid RequestedByGUID;
            ObjectGuid RequestedByWowAccount;
            bool ToTheDeath = false;
        };

        class DuelResponse final : public ClientPacket
        {
        public:
            explicit DuelResponse(WorldPacket&& packet) : ClientPacket(CMSG_DUEL_RESPONSE, std::move(packet)) { }

            void Read() override;

            ObjectGuid ArbiterGUID;
            bool Accepted = false;
            bool Forfeited = false;
        };

        class DuelWinner final : public ServerPacket
        {
        public:
            explicit DuelWinner() : ServerPacket(SMSG_DUEL_WINNER, 4 * 4 + 1) { }

            WorldPacket const* Write() override;

            std::string BeatenName;
            std::string WinnerName;
            uint32 BeatenVirtualRealmAddress = 0;
            uint32 WinnerVirtualRealmAddress = 0;
            bool Fled = false;
        };
    }
}

#endif // TRINITYCORE_DUEL_PACKETS_H

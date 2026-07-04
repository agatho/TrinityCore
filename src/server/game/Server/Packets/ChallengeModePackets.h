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

#ifndef TRINITYCORE_CHALLENGE_MODE_PACKETS_H
#define TRINITYCORE_CHALLENGE_MODE_PACKETS_H

#include "Packet.h"
#include "MythicPlusPacketsCommon.h"
#include <vector>

namespace WorldPackets
{
    namespace ChallengeMode
    {
        // CMSG_REQUEST_MYTHIC_PLUS_SEASON_DATA -- empty request (client serializer carries no payload @68275).
        class RequestMythicPlusSeasonData final : public ClientPacket
        {
        public:
            explicit RequestMythicPlusSeasonData(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_MYTHIC_PLUS_SEASON_DATA, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_REQUEST_MYTHIC_PLUS_AFFIXES -- empty request (client serializer carries no payload @68275).
        class RequestMythicPlusAffixes final : public ClientPacket
        {
        public:
            explicit RequestMythicPlusAffixes(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_MYTHIC_PLUS_AFFIXES, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_RESET_CHALLENGE_MODE -- player requests to abort/reset the active run (empty payload).
        class ResetChallengeMode final : public ClientPacket
        {
        public:
            explicit ResetChallengeMode(WorldPacket&& packet) : ClientPacket(CMSG_RESET_CHALLENGE_MODE, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_START_CHALLENGE_MODE -- the player slots a keystone into the font of power to begin the run.
        // Wire (client serializer @68275): uint8 Bag; uint32 Slot; ObjectGuid GameObjectGUID (plain byte stream).
        class StartChallengeMode final : public ClientPacket
        {
        public:
            explicit StartChallengeMode(WorldPacket&& packet) : ClientPacket(CMSG_START_CHALLENGE_MODE, std::move(packet)) { }

            void Read() override;

            ObjectGuid GameObjectGUID;
            uint32 Slot = 0;
            uint8 Bag = 0;
        };

        // SMSG_MYTHIC_PLUS_SEASON_DATA -- whether the Mythic+ season is currently active.
        // Wire (client deserializer sub_7FF729091240 @68275): a single bit (bool, MSB-first) + FlushBits. Nothing else.
        class MythicPlusSeasonData final : public ServerPacket
        {
        public:
            explicit MythicPlusSeasonData() : ServerPacket(SMSG_MYTHIC_PLUS_SEASON_DATA, 1) { }

            WorldPacket const* Write() override;

            bool IsMythicPlusActive = false;
        };

        // One entry of SMSG_MYTHIC_PLUS_CURRENT_AFFIXES. Field layout is binary-confirmed as two uint32; the second
        // is the SeasonID, per C_MythicPlus.GetCurrentAffixes returning {id, seasonID}.
        struct CurrentAffix
        {
            int32 KeystoneAffixID = 0;
            int32 SeasonID = 0;
        };

        // SMSG_MYTHIC_PLUS_CURRENT_AFFIXES -- the affixes in effect this week.
        // Wire (client deserializer sub_7FF729091470 -> sub_7FF729091290 @68275):
        //   uint32 Count; Count x { uint32 KeystoneAffixID; uint32 SeasonID }   (all plain 4-byte LE).
        class MythicPlusCurrentAffixes final : public ServerPacket
        {
        public:
            explicit MythicPlusCurrentAffixes() : ServerPacket(SMSG_MYTHIC_PLUS_CURRENT_AFFIXES, 4 + 8 * 4) { }

            WorldPacket const* Write() override;

            std::vector<CurrentAffix> Affixes;
        };
    }
}

#endif // TRINITYCORE_CHALLENGE_MODE_PACKETS_H

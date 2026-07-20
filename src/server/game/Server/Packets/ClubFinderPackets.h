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

#ifndef TRINITYCORE_CLUB_FINDER_PACKETS_H
#define TRINITYCORE_CLUB_FINDER_PACKETS_H

#include "ObjectGuid.h"
#include "Packet.h"
#include "PacketUtilities.h"

namespace WorldPackets
{
    namespace ClubFinder
    {
        // Produced by Lua C_ClubFinder.PostClub(clubId, itemLevelRequirement, name, description,
        // avatarId, specs, type, crossFaction). Wire order is taken from the client's own body writer
        // sub_7FF72907DF50 (12.0.7 68275); see c:/dumps/CLUB_FINDER_SCOPING_68275.md.
        //
        // The client clamps Name to 96 characters and Description to 2048 before sending; both string
        // bodies are written raw at the end with no length prefix and no terminator, their lengths
        // living in the leading bit block.
        class ClubFinderPost final : public ClientPacket
        {
        public:
            explicit ClubFinderPost(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_POST, std::move(packet)) { }

            void Read() override;

            uint64 ClubId                = 0;
            uint64 RecruitingSpecs       = 0;
            uint32 RecruitmentFlags      = 0;   // bit-index-per-value ClubFinderSettingFlags mask,
                                                // locale packed as (locale + 1) in bits 21-25
            uint32 ItemLevelRequirement  = 0;
            uint32 AvatarId              = 0;
            uint8 Type                   = 0;   // ClubFinderRequestType, 3 bits on the wire
            bool CrossFaction            = false;
            std::string Name;
            std::string Description;
        };

        // Reader sub_7FF7290B42E0: a PackedGuid followed by one bit byte carrying two 3-bit fields
        // (stored at +48 = b >> 5 and +52 = (b >> 2) & 7).
        class ClubFinderResponsePostRecruitmentMessage final : public ServerPacket
        {
        public:
            explicit ClubFinderResponsePostRecruitmentMessage() : ServerPacket(SMSG_CLUB_FINDER_RESPONSE_POST_RECRUITMENT_MESSAGE, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid ClubFinderGUID;
            uint8 Type   = 0;   // 3 bits
            uint8 Status = 0;   // 3 bits - meaning unverified, this opcode is absent from all captures
        };

        // Reader sub_7FF7290B4020: one bit byte, a 3-bit field (+32 = b >> 5) and a 4-bit field
        // (+36 = (b >> 1) & 0xF). Widths and order are proven; the field VALUES are not, because the
        // opcode never appears in the 12.0.7 captures.
        class ClubFinderErrorMessage final : public ServerPacket
        {
        public:
            explicit ClubFinderErrorMessage() : ServerPacket(SMSG_CLUB_FINDER_ERROR_MESSAGE, 1) { }

            WorldPacket const* Write() override;

            uint8 Type  = 0;   // 3 bits
            uint8 Error = 0;   // 4 bits
        };
    }
}

#endif // TRINITYCORE_CLUB_FINDER_PACKETS_H

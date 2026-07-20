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
        // (+48 = b >> 5, +52 = (b >> 2) & 7).
        //
        // Handler sub_7FF72ACAB9D0 branches on the FIRST field only: anything other than 0 or 1 makes
        // it raise ERR_CLUB_FINDER_ERROR_POST_CLUB and drop the message. On 0/1 it refreshes the
        // posting cache and fires CLUB_FINDER_POST_UPDATED, which is what closes the posting dialog.
        // So the field is a post result code, not a request type.
        //
        // The second field is parsed and then never read by the handler - it reaches neither Lua nor
        // manager state. Its value does not affect client behaviour.
        class ClubFinderResponsePostRecruitmentMessage final : public ServerPacket
        {
        public:
            explicit ClubFinderResponsePostRecruitmentMessage() : ServerPacket(SMSG_CLUB_FINDER_RESPONSE_POST_RECRUITMENT_MESSAGE, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid ClubFinderGUID;
            uint8 Result = 0;   // 3 bits; 0 and 1 are success, >= 2 makes the client report a failure
            uint8 Unused = 0;   // 3 bits; parsed by the client and discarded
        };

        class ClubFinderRequestSubscribedClubPostingIds final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestSubscribedClubPostingIds(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_SUBSCRIBED_CLUB_POSTING_IDS, std::move(packet)) { }

            void Read() override;

            std::vector<uint64> ClubIds;
        };

        // Reader sub_7FF7290B4380 fills a stride-16 vector of { uint64, uint32, uint32 }, which is
        // exactly the client's own ClubFinderClubPostingClubIDMap reflection type.
        class ClubFinderGetClubPostingIdsResponse final : public ServerPacket
        {
        public:
            explicit ClubFinderGetClubPostingIdsResponse() : ServerPacket(SMSG_CLUB_FINDER_GET_CLUB_POSTING_IDS_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            struct ClubPostingClubIDMap
            {
                uint64 ClubID              = 0;
                uint32 ClubPostingID       = 0;
                uint32 PostingDisplayFlags = 0;
            };

            std::vector<ClubPostingClubIDMap> PostingIds;
        };

        // Serializer sub_7FF72907EAA0: two counts, then the posting ids, then the bit block, then any
        // filters. Both captured bodies (13B with one id, 17B with two) carry FilterCount == 0 and end
        // at the bit byte, which is what pins the second uint32 as the filter count.
        class ClubFinderRequestClubsData final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestClubsData(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_CLUBS_DATA, std::move(packet)) { }

            void Read() override;

            std::vector<uint32> ClubPostingIDs;
            uint32 FilterCount = 0;
            uint8 Type         = 0;   // 3 bits, ClubFinderRequestType
            bool Unknown       = false;
        };

        // The browse response. Envelope is uint32 Count plus one bit byte (Bits<3> request type,
        // Bits<1> flag), then Count records. The record layout is verified byte-for-byte against the
        // real 326-byte and 565-byte captures - see c:/dumps/CLUB_FINDER_SCOPING_68275.md.
        class ClubFinderLookupClubPostingsList final : public ServerPacket
        {
        public:
            explicit ClubFinderLookupClubPostingsList() : ServerPacket(SMSG_CLUB_FINDER_LOOKUP_CLUB_POSTINGS_LIST, 5) { }

            WorldPacket const* Write() override;

            struct ClubCacheData
            {
                std::string ClubName;
                std::string Comment;
                std::string GuildLeader;
                ObjectGuid ClubFinderGUID;
                ObjectGuid LastPosterGUID;
                uint64 RecruitingSpecs  = 0;
                uint64 ClubID           = 0;
                int64 LastUpdatedTime   = 0;
                uint32 NumActiveMembers = 0;
                uint32 TabardInfo       = 0;
                int32 RecruitmentFlags  = 0;
                int32 MinIlvl           = 0;
            };

            std::vector<ClubCacheData> Postings;
            uint8 Type     = 0;       // 3 bits; the captures echo the request's ClubFinderRequestType
            bool Unknown   = false;   // 1 bit; false in every capture
        };

        // Reader sub_7FF7290B4020: a 3-bit field (+32 = b >> 5) and a 4-bit field (+36 = (b >> 1) & 0xF).
        //
        // Handler sub_7FF72ACABB30 switches on the 4-bit field, mapping each value 1:1 onto an
        // ERR_CLUB_FINDER_* global string, and uses the 3-bit field as the ClubFinderRequestType of the
        // list re-request it issues for the recoverable cases.
        class ClubFinderErrorMessage final : public ServerPacket
        {
        public:
            explicit ClubFinderErrorMessage() : ServerPacket(SMSG_CLUB_FINDER_ERROR_MESSAGE, 1) { }

            WorldPacket const* Write() override;

            uint8 Type  = 0;   // 3 bits, ClubFinderRequestType
            uint8 Error = 0;   // 4 bits, ClubFinderErrorType
        };
    }
}

#endif // TRINITYCORE_CLUB_FINDER_PACKETS_H

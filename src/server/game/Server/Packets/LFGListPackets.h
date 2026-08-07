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

#ifndef TRINITYCORE_LFG_LIST_PACKETS_H
#define TRINITYCORE_LFG_LIST_PACKETS_H

#include "Packet.h"
#include "LFGPacketsCommon.h"       // WorldPackets::LFG::RideTicket (reused as the listing/application id)
#include "ObjectGuid.h"
#include "Optional.h"
#include <array>
#include <string>
#include <vector>

namespace WorldPackets
{
    namespace LFGList
    {
        // Playstyle enums (client Enum.LFGEntryPlaystyle / LFGEntryGeneralPlaystyle, from the generated API docs).
        enum class LFGEntryPlaystyle : uint8 { None = 0, Standard = 1, Casual = 2, Hardcore = 3 };
        enum class LFGEntryGeneralPlaystyle : uint8 { None = 0, Learning = 1, FunRelaxed = 2, FunSerious = 3, Expert = 4 };

        // One required-member score entry inside a listing (nested block, client serializer sub_7FF729167840).
        struct ListingMemberRequirement
        {
            uint32 Field0 = 0;
            float Field1 = 0.0f;
            uint32 Field2 = 0;
            uint32 Field3 = 0;
            uint8 Field4 = 0;
            bool Flag = false;
        };

        // The parameters a leader sets when publishing (JOIN) or editing (UPDATE_REQUEST) a premade listing.
        // RESOLVED against the 12.0.7.68275 premade-groups sniff + the client JOIN serializer (sub_7FF72914ABE0) +
        // the generated Lua API doc (LfgListingCreateData / LfgEntryData). The descriptor is BIT-PACKED, not byte-
        // aligned: a bit-packed header (activity group 5b, three bit-packed string lengths, member-requirement count,
        // and the boolean/optional presence bits) is flushed, then the member-requirement block, the fixed activity/
        // criteria fields, the string data, and the conditional (nilable) numeric fields follow. Full layout:
        // c:\dumps\LFG_LIST_WIRE_68275.md. Field names are authoritative (Lua); only ActivityID + RequiredItemLevel
        // drive server-side filtering, the rest are pass-through echo to searchers.
        struct ListingDescriptor
        {
            float HeaderFloat0 = 0.0f;          // nested block leading floats (sub_7FF729167840)
            float HeaderFloat1 = 0.0f;
            std::vector<ListingMemberRequirement> MemberRequirements;
            uint32 ActivityID = 0;              // GroupFinderActivity.db2 id (u32 @0x38; search/validation key)
            float RequiredDungeonScore = 0.0f;  // float @0x3c
            uint8 TrailingByte = 0;             // u8 @0x702
            std::vector<uint32> ActivityIDs;    // trailing uint32 vector (count from the 5-bit header field)
            bool IsAutoAccept = false;          // presence bits (client offsets 0x6c3..0x6c6)
            bool IsCrossFactionListing = false;
            bool IsPrivateGroup = false;
            bool NewPlayerFriendly = false;
            std::string Name;                   // bit-length-prefixed strings (client offsets 0x40 / 0x241 / 0x642)
            std::string VoiceChat;
            std::string Comment;                // listing title / comment ("crate" in the sniff)
            Optional<uint32> QuestID;           // nilable numeric fields, written only when their presence bit is set
            Optional<uint32> OptionalValue1;
            Optional<uint32> OptionalValue2;
            Optional<uint8> OptionalValue3;

            // Exact bytes consumed while reading this descriptor. The server echoes a listing back verbatim in
            // SMSG_LFG_LIST_UPDATE_STATUS (proven by sniff), so we replay these rather than re-serialize the
            // bit-packed descriptor (which is error-prone and was previously malformed).
            std::vector<uint8> RawBytes;
        };

        // The listing snapshot the server echoes to clients (UPDATE_STATUS / search rows). Mirrors ListingDescriptor
        // plus server-owned fields (leader name strings, member counts). Wire: u8x6 u32 u32 u8 u32 str str str u32 u32 u32 u8.
        struct ListingInfo
        {
            std::array<uint8, 6> Params = { };
            uint32 ActivityID = 0;
            uint32 Field1 = 0;
            uint8 Field2 = 0;
            uint32 RequiredItemLevel = 0;
            std::string Comment;
            std::string LeaderName;
            std::string VoiceChat;
            uint32 Field3 = 0;
            uint32 Field4 = 0;
            uint32 Field5 = 0;
            uint8 Field6 = 0;
        };

        // ---- CMSG (client -> server) ----

        class LFGListJoin final : public ClientPacket
        {
        public:
            explicit LFGListJoin(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_JOIN, std::move(packet)) { }
            void Read() override;

            ListingDescriptor Listing;
        };

        class LFGListUpdateRequest final : public ClientPacket
        {
        public:
            explicit LFGListUpdateRequest(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_UPDATE_REQUEST, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;
            ListingDescriptor Listing;
        };

        class LFGListLeave final : public ClientPacket
        {
        public:
            explicit LFGListLeave(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_LEAVE, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;
        };

        class LFGListGetStatus final : public ClientPacket
        {
        public:
            explicit LFGListGetStatus(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_GET_STATUS, std::move(packet)) { }
            // Sniff-verified (premandegroups 68275): empty payload — the client requests its own status blind.
            void Read() override { }
        };

        class LFGListSearch final : public ClientPacket
        {
        public:
            explicit LFGListSearch(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_SEARCH, std::move(packet)) { }
            void Read() override;

            uint8 CategoryId = 0;
            uint8 ActivityGroupId = 0;
            uint8 Field2 = 0;
            uint8 Field3 = 0;
            uint8 Field4 = 0;
            uint8 Field5 = 0;
            std::array<uint32, 9> Filters = { };
            uint8 Field6 = 0;
            uint8 Field7 = 0;
            std::array<uint32, 4> Filters2 = { };
            ObjectGuid SearchGuid;
        };

        class LFGListApplyToGroup final : public ClientPacket
        {
        public:
            explicit LFGListApplyToGroup(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_APPLY_TO_GROUP, std::move(packet)) { }
            void Read() override;

            // Sniff-verified 33B fixed: Ticket{groupGuid, ListingID, type 4, applyTime} + ActivityID + roles.
            LFG::RideTicket Ticket;         // the listing being applied to
            uint32 ActivityID = 0;          // GroupFinderActivity of the listing (was mislabeled ListingId)
            uint8 RoleMask = 0;
            uint8 Field2 = 0;
        };

        class LFGListCancelApplication final : public ClientPacket
        {
        public:
            explicit LFGListCancelApplication(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_CANCEL_APPLICATION, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;
        };

        class LFGListDeclineApplicant final : public ClientPacket
        {
        public:
            explicit LFGListDeclineApplicant(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_DECLINE_APPLICANT, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;         // the listing
            LFG::RideTicket ApplicantTicket;
        };

        class LFGListInviteApplicant final : public ClientPacket
        {
        public:
            explicit LFGListInviteApplicant(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_INVITE_APPLICANT, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;         // the listing
            uint32 ListingId = 0;
            ObjectGuid ApplicantGuid;
            uint8 RoleMask = 0;
            LFG::RideTicket ApplicantTicket;
        };

        class LFGListInviteResponse final : public ClientPacket
        {
        public:
            explicit LFGListInviteResponse(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_INVITE_RESPONSE, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;
            bool Accept = false;
        };

        class RequestLFGListBlacklist final : public ClientPacket
        {
        public:
            explicit RequestLFGListBlacklist(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_LFG_LIST_BLACKLIST, std::move(packet)) { }
            void Read() override { }
        };

        // ---- SMSG (server -> client) ----

        class LFGListJoinResult final : public ServerPacket
        {
        public:
            explicit LFGListJoinResult() : ServerPacket(SMSG_LFG_LIST_JOIN_RESULT, 5) { }
            WorldPacket const* Write() override;

            uint32 Status = 0;
            uint8 Result = 0;               // 0 = ok (exact enum needs a sniff)
        };

        class LFGListUpdateStatus final : public ServerPacket
        {
        public:
            explicit LFGListUpdateStatus() : ServerPacket(SMSG_LFG_LIST_UPDATE_STATUS, 64) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint8 Status = 0;
            std::vector<uint8> RawDescriptor;   // the listing's descriptor bytes, echoed verbatim (empty when not listed)
            bool Listed = true;
        };

        class LFGListUpdateExpiration final : public ServerPacket
        {
        public:
            explicit LFGListUpdateExpiration() : ServerPacket(SMSG_LFG_LIST_UPDATE_EXPIRATION, 24) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint8 Reason = 0;
        };

        class LFGListSearchStatus final : public ServerPacket
        {
        public:
            explicit LFGListSearchStatus() : ServerPacket(SMSG_LFG_LIST_SEARCH_STATUS, 2) { }
            WorldPacket const* Write() override;

            uint8 Status = 0;
            bool Complete = true;
        };

        // One row of a search result (2008-byte element): the listing + leader/member info.
        // Exact SMSG_LFG_LIST_SEARCH_RESULTS (0x560002) row. Layout reverse-engineered from the client
        // deserializers and validated byte-exact against a real 12.0.7.68275 sniff (see
        // c:\dumps\lfg_search_results_layout.md). The embedded descriptor is echoed verbatim from the bytes
        // the client sent in CMSG_LFG_LIST_JOIN (same replay strategy proven for SMSG_LFG_LIST_UPDATE_STATUS).
        struct SearchResultListing
        {
            ObjectGuid GroupGuid;                 // party/group guid (also echoed as LeaderGuidEcho)
            uint32 ListingId = 0;                 // stable id the client sends back in APPLY_TO_GROUP
            uint64 PostTime = 0;                  // listing creation unix seconds (emitted twice)
            ObjectGuid LeaderGuid;                // fills Guid_A..E
            std::vector<ObjectGuid> Members;      // group roster -> MemberCount + MemberDetail records
            std::vector<uint8> RawDescriptor;     // verbatim ListingDescriptor bytes
        };

        class LFGListSearchResults final : public ServerPacket
        {
        public:
            explicit LFGListSearchResults() : ServerPacket(SMSG_LFG_LIST_SEARCH_RESULTS, 8) { }
            WorldPacket const* Write() override;

            std::vector<SearchResultListing> Listings;
        };

        class LFGListSearchResultsUpdate final : public ServerPacket
        {
        public:
            explicit LFGListSearchResultsUpdate() : ServerPacket(SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE, 8) { }
            WorldPacket const* Write() override;

            std::vector<SearchResultListing> Listings;
        };

        // One applicant row (352-byte element): applicant ticket, roles/spec/ilvl, comment.
        struct ApplicantInfo
        {
            ObjectGuid ApplicantGuid;
            uint32 ApplicationId = 0;
            uint8 State = 0;
            uint8 RoleMask = 0;
            ObjectGuid PlayerGuid;
            uint32 SpecID = 0;
            uint32 ItemLevel = 0;
            uint32 Field3 = 0;
            uint8 Field4 = 0;
            uint8 Field5 = 0;
            std::array<uint32, 4> Fields = { };
            uint8 Field6 = 0;
            uint8 Field7 = 0;
            uint32 Field8 = 0;
            uint32 Field9 = 0;
            std::string Comment;
        };

        class LFGListApplicantListUpdate final : public ServerPacket
        {
        public:
            explicit LFGListApplicantListUpdate() : ServerPacket(SMSG_LFG_LIST_APPLICANT_LIST_UPDATE, 8) { }
            WorldPacket const* Write() override;

            uint32 ListingId = 0;
            std::vector<ApplicantInfo> Applicants;
        };

        class LFGListApplicationStatusUpdate final : public ServerPacket
        {
        public:
            explicit LFGListApplicationStatusUpdate() : ServerPacket(SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE, 24) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint32 ApplicationId = 0;
            uint8 State = 0;
            uint8 Field2 = 0;
        };

        class LFGListApplyToGroupResult final : public ServerPacket
        {
        public:
            explicit LFGListApplyToGroupResult() : ServerPacket(SMSG_LFG_LIST_APPLY_TO_GROUP_RESULT, 64) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint8 Result = 0;
            uint8 Field1 = 0;
            uint8 Field2 = 0;
            uint32 ListingId = 0;
            ObjectGuid LeaderGuid;
            ListingInfo Listing;
        };

        struct LFGListBlacklistEntry
        {
            uint32 ActivityID = 0;
            uint32 Reason = 0;      // exact semantics (cooldown reason/timestamp) NEEDS-SNIFF
        };

        class LFGListUpdateBlacklist final : public ServerPacket
        {
        public:
            explicit LFGListUpdateBlacklist() : ServerPacket(SMSG_LFG_LIST_UPDATE_BLACKLIST, 4) { }
            WorldPacket const* Write() override;

            std::vector<LFGListBlacklistEntry> Entries;
        };

        ByteBuffer& operator<<(ByteBuffer& data, ListingInfo const& listing);
    }
}

#endif // TRINITYCORE_LFG_LIST_PACKETS_H

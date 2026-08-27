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
#include "MythicPlusPacketsCommon.h"// WorldPackets::MythicPlus::DungeonScoreSummary (embedded in the descriptor)
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

        // ---------------------------------------------------------------------------------------------
        // ListingDescriptor - the shared core of six messages. Get it wrong and six messages are wrong.
        //
        // Client 12.1.0.69382: reader @ RVA 0x7572C0 (SMSG side), writer @ RVA 0x757660 (CMSG side). Both
        // directions were decoded independently and agree field for field, which is what makes a faithful
        // re-serialization (rather than the byte replay this used to do) safe.
        //
        // It rides in SMSG_LFG_LIST_UPDATE_STATUS (0x5A000A), every SMSG_LFG_LIST_SEARCH_RESULTS row
        // (0x5A0002), every SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE row (0x5A0010),
        // SMSG_LFG_LIST_APPLY_TO_GROUP_RESULT (0x5A000D, inside the embedded row),
        // SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE (0x5A0022), and on the client side in
        // CMSG_LFG_LIST_JOIN (0x3D0257) / CMSG_LFG_LIST_UPDATE_REQUEST (0x3D0258).
        //
        // Wire layout:
        //   bit header, MSB-first, 43 bits = 6 bytes, no explicit flush (the 5 spare bits are simply unread):
        //     bits<5>  ActivityIDs.Count
        //     bits<10> len(Name)        client buffer @desc+32   capacity 513
        //     bits<11> len(Comment)     client buffer @desc+545  capacity 1025
        //     bits<8>  len(VoiceChat)   client buffer @desc+1570 capacity 129
        //     bit IsAutoAccept(+1699) bit IsPrivateGroup(+1700) bit IsWarMode(+1701)
        //     bit IsCrossFactionListing(+1702)
        //     bit has(QuestID @+1704, flag @+1708)              bit has(RequiredDungeonScore @+1744, flag @+1748)
        //     bit has(RequiredPvpRating @+1752, flag @+1756)    bit has(Playstyle @+1760 (u8), flag @+1761)
        //     bit NewPlayerFriendly(+1763)
        //   byte aligned:
        //     uint32 CategoryID (@+24) ; uint32 RequiredItemLevel (@+28) ;
        //     DungeonScoreSummary LeaderScore (@+1712, sub-reader 0x6EC830) ; uint8 GeneralPlaystyle (@+1762)
        //   deferred:
        //     ActivityIDs[] (uint32 each) ; Name bytes ; Comment bytes ; VoiceChat bytes ;
        //     then the present optionals in order QuestID, RequiredDungeonScore, RequiredPvpRating, Playstyle
        //
        // Minimum size 27 bytes (6 header + 4 + 4 + 12 empty summary + 1). That is exactly the length of the
        // all-zero filler this file used to emit for an empty listing - the size was right, the position was not.
        //
        // The names below are NOT inferred. They come from the two request builders the obfuscated Lua stubs
        // tail-call into: CreateListing @ RVA 0x24E5830 (from stub 0x117F7E0) and UpdateListing @ RVA
        // 0x24E5D80 (from stub 0x119A7C0). Both assemble the descriptor on the stack, so the three character
        // arrays appear literally as _BYTE[513] / _BYTE[1025] / _BYTE[129] at stack deltas 0x20 / 0x221 /
        // 0x622 = 32 / 545 / 1570, and every store is fed straight from a named field of the Lua createData
        // struct parsed at 0x117A2A0 (activityIDs +0/+8, questID +88/+92, isAutoAccept +96,
        // isCrossFactionListing +97, isPrivateGroup +98, newPlayerFriendly +99, playstyle +100,
        // generalPlaystyle +101, requiredDungeonScore +104, requiredItemLevel +108, requiredPvpRating +112).
        // Cross-checked from the other direction through the active-entry mirror at manager+40
        // (copy-assign 0x757080 in 0x24DBE70): desc+32 -> manager+1808/2321 -> the name the creation UI
        // refills, desc+545 -> manager+2834 -> LFGListCreationDescription, desc+1570 -> manager+4884 ->
        // LFGListCreationVoiceChat. This settles the two questions the family brief carried as its biggest
        // open items, and it refutes the comment this file used to carry (Name / VoiceChat / Comment in
        // buffer order).
        //
        // Two results worth spelling out because they are not on anyone's candidate list:
        //  * +1701 is isWarMode, not privateGroup. It is fed by 0x21C6890, which reads bit 11 of the local
        //    player's field +0x2208 - the same bit the "ToggleWarMode" binding flips. It matches
        //    LfgSearchResultData.isWarMode.
        //  * `censored` is NOT in this descriptor at all. The client keeps raw and censored copies of the
        //    three strings side by side in its manager (raw at +2321/+3859, censored at +1808/+2834/+4884)
        //    and C_LFGList.RevealCensoredSearchResult just clears a byte at searchResult+2257. The censor
        //    state travels in its own opcode (SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE), not in here.
        //
        // Also not in the descriptor although LfgEntryData exposes them: requiredHonorLevel and duration
        // (duration comes from manager+5016, see 0x24DE410).
        //
        // LeaderScore is INBOUND-ONLY-EMPTY: both builders explicitly zero it before sending, so a CMSG
        // always carries an empty summary and it is the SERVER's job to fill it on the way out. That is
        // what puts a Mythic+ rating next to a listing in the group browser.
        struct ListingDescriptor
        {
            std::vector<uint32> ActivityIDs;    // bits<5> count; the selected GroupFinderActivity ids
            std::string Name;                   // bits<10> length, client capacity 513
            std::string Comment;                // bits<11> length, client capacity 1025
            std::string VoiceChat;              // bits<8>  length, client capacity 129
            bool IsAutoAccept = false;          // +1699
            bool IsPrivateGroup = false;        // +1700
            bool IsWarMode = false;             // +1701
            bool IsCrossFactionListing = false; // +1702
            bool NewPlayerFriendly = false;     // +1763
            Optional<uint32> QuestID;           // +1704
            Optional<uint32> RequiredDungeonScore;  // +1744 - raw dword, pass-through (see below)
            Optional<uint32> RequiredPvpRating;     // +1752 - raw dword, pass-through
            Optional<uint8> Playstyle;              // +1760, Enum.LFGEntryPlaystyle
            uint32 CategoryID = 0;              // +24, GroupFinderCategory.db2 id - the search key. The client
                                                // derives it from GroupFinderActivity[field 2] of every
                                                // selected activity and refuses a listing whose activities
                                                // disagree; UpdateListing echoes the active entry's value back
                                                // rather than recomputing it.
            uint32 RequiredItemLevel = 0;       // +28 - raw dword, pass-through
            MythicPlus::DungeonScoreSummary LeaderScore;    // +1712, the ADVERTISER's score, not an applicant's
            uint8 GeneralPlaystyle = 0;         // +1762, Enum.LFGEntryGeneralPlaystyle
        };
        // The three numeric fields marked "raw dword, pass-through" are carried as uint32 on purpose. The
        // client reads all three through the same 4-byte slot it uses for floats, and the server neither
        // interprets nor derives them - it stores what a leader published and hands it back to searchers
        // unchanged. A raw dword preserves the exact bits whichever way the client meant them.

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
            // Sniff-verified (premadegroups 68275): empty payload - the client requests its own status blind.
            void Read() override { }
        };

        // One JamLFGSearchTerm of CMSG_LFG_LIST_SEARCH. Client 12.1.0.69382 writer @ RVA 0x757D00,
        // in-memory stride 330 = 10 string slots of 32 bytes + 10 flag bytes at +320.
        // Wire: ten INTERLEAVED pairs of (bits<5> length, one presence bit) = 60 bits, then FlushBits,
        // then the ten strings back to back (empty slots contribute nothing).
        // The old reader here took ten bare bits<5> followed by 14 padding bits - same byte count, but
        // everything from the second slot on was shifted by one bit.
        struct LFGListSearchTerm
        {
            static constexpr std::size_t MAX_VALUES = 10;
            static constexpr std::size_t MAX_VALUE_LENGTH = 32;

            std::array<std::string, MAX_VALUES> Values;
            std::array<bool, MAX_VALUES> Flags = { };
        };

        // CMSG_LFG_LIST_SEARCH (0x43003A) - Client 12.1.0.69382, writer @ RVA 0x6A4570 -> 0x757EC0.
        //   bits<5> Terms.Count ; bit Flag ; FlushBits
        //   u32 CategoryID(+24) u32 Filter1(+28) u32 Filter2(+32) u32 LanguageMask(+36)
        //   u32 Size(Vec1)(+48) u32 Filter5(+68) u32 Size(Vec2)(+80) u32 Size(Vec3)(+104) u32 Filter8(+120)
        //   u8 FilterByte1(+124) ; u8 FilterByte2(+125) ; u32 Size(Guids)(+136)
        //   Terms[] ; Vec1[] (u32) ; Vec2[] (u32) ; Vec3[] (u32) ; Guids[] (PackedGuid)
        // Three separate defects were fixed here against the old nine-scalar model:
        //   - the term blocks are written AFTER all fixed scalars, not right behind the 5-bit count;
        //   - each term slot carries a presence bit next to its 5-bit length;
        //   - three of the nine "filters" (indices 4, 6 and 7 of the old array) are ARRAY COUNTS. Reading
        //     them as scalars made the guid list come out of the wrong bytes as soon as one was non-zero.
        // All three were invisible in the 143-byte sniff because that search had zero terms and empty lists.
        class LFGListSearch final : public ClientPacket
        {
        public:
            explicit LFGListSearch(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_SEARCH, std::move(packet)) { }
            void Read() override;

            std::vector<LFGListSearchTerm> Terms;
            bool Flag = false;
            uint32 CategoryID = 0;              // GroupFinderCategory id (68974: JOIN carried 1 = questing
                                                // and the follow-up SEARCH echoed the same 1 here)
            uint32 Filter1 = 0;
            uint32 Filter2 = 0;
            uint32 LanguageMask = 0;
            uint32 Filter5 = 0;
            uint32 Filter8 = 0;
            uint8 FilterByte1 = 0;              // observed 0xFF
            uint8 FilterByte2 = 0;              // observed 0x05
            std::vector<uint32> Values1;
            std::vector<uint32> Values2;
            std::vector<uint32> Values3;
            std::vector<ObjectGuid> Guids;

            uint32 GetCategoryId() const { return CategoryID; }
            // First non-empty search term, or an empty string. The server filters listings by keyword with it.
            std::string const& GetKeyword() const;
        };

        // CMSG_LFG_LIST_APPLY_TO_GROUP (0x43003B) - Client 12.1.0.69382, writer @ RVA 0x6A46A0:
        //   RideTicket ; u32 ActivityID ; u8 RoleMask ; u8 len(Comment) ; FlushBits ; Comment bytes
        // The byte after RoleMask is a length prefix (client buffer capacity 256, so ceil(log2(256)) = 8
        // bits, which the compiler emits as a whole byte), NOT a scalar. With an empty comment both models
        // produce the same 33 bytes, which is why the sniff never caught it.
        class LFGListApplyToGroup final : public ClientPacket
        {
        public:
            explicit LFGListApplyToGroup(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_APPLY_TO_GROUP, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;         // the listing being applied to
            uint32 ActivityID = 0;
            uint8 RoleMask = 0;
            std::string Comment;            // the applicant's note to the group leader
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

        struct LFGListInvitee
        {
            ObjectGuid Guid;
            uint8 RoleMask = 0;
        };

        // CMSG_LFG_LIST_INVITE_APPLICANT (0x43003E) - Client 12.1.0.69382, writer @ RVA 0x6A4A30:
        //   RideTicket Ticket ; RideTicket ApplicantTicket ; u32 Size(Invitees) ;
        //   Invitees[] { PackedGuid ; u8 RoleMask }        (in-memory stride 24)
        // The two tickets are ADJACENT. The old model read a u32 and a bare PackedGuid out of the second
        // ticket's first bytes, so everything after the first ticket was garbage on every single invite -
        // this was not a latent defect, it was always wrong.
        class LFGListInviteApplicant final : public ClientPacket
        {
        public:
            explicit LFGListInviteApplicant(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_INVITE_APPLICANT, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;         // the listing
            LFG::RideTicket ApplicantTicket;
            std::vector<LFGListInvitee> Invitees;
        };

        class LFGListInviteResponse final : public ClientPacket
        {
        public:
            explicit LFGListInviteResponse(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_INVITE_RESPONSE, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;
            bool Accept = false;
        };

        // CMSG_LFG_LIST_CONFIRM_CENSORED_ACTIVE_ENTRY (0x4301AB) - 12.1 newcomer.
        // Client 12.1.0.69382 writer @ RVA 0x6B3370: opcode, then exactly one RideTicket body and nothing
        // else. Sender: C_LFGList.ConfirmCensoredActiveEntry (RVA 0x117E560), which also sets the local
        // resolution state to 2 ("confirmed") before sending.
        // Meaning: "I keep my flagged listing as it is and decline to edit it."
        // There is deliberately NO reveal opcode - C_LFGList.RevealCensoredActiveEntry (0x11964F0) and
        // RevealCensoredSearchResult (0x1196940) only touch client state and fire a local event.
        class LFGListConfirmCensoredActiveEntry final : public ClientPacket
        {
        public:
            explicit LFGListConfirmCensoredActiveEntry(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LIST_CONFIRM_CENSORED_ACTIVE_ENTRY, std::move(packet)) { }
            void Read() override;

            LFG::RideTicket Ticket;
        };

        class RequestLFGListBlacklist final : public ClientPacket
        {
        public:
            explicit RequestLFGListBlacklist(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_LFG_LIST_BLACKLIST, std::move(packet)) { }
            void Read() override { }
        };

        // ---- SMSG (server -> client) ----

        // SMSG_LFG_LIST_JOIN_RESULT (0x5A0001) - Client 12.1.0.69382, dispatcher case @ RVA 0x75557D:
        //   RideTicket ; u32 Status ; u8 Result ; u8 ResultDetail
        // The ticket was missing entirely (a stock defect, not 12.1 drift - 68275 read it too), so the
        // client filled its ticket from the Status/Result bytes, ran off the end of the packet and zeroed
        // every field. A rejected listing creation then showed no error, or the wrong one.
        // The trailing u8 is genuinely new in 12.1 (68275 read Ticket, u32, u8).
        class LFGListJoinResult final : public ServerPacket
        {
        public:
            explicit LFGListJoinResult() : ServerPacket(SMSG_LFG_LIST_JOIN_RESULT, 16 + 4 + 4 + 8 + 1 + 4 + 1 + 1) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint32 Status = 0;
            uint8 Result = 0;               // UNVERIFIED: the exact enum. There is no Enum.LfgEntryResult in
                                            // the client, and LFG_LIST_ENTRY_CREATION_FAILED has no payload.
            uint8 ResultDetail = 0;         // UNVERIFIED: new in 12.1, no consumer evidence
        };

        // SMSG_LFG_LIST_UPDATE_STATUS (0x5A000A) - Client 12.1.0.69382, reader @ RVA 0x754910:
        //   RideTicket ; ListingDescriptor ; u64 ExpirationTime ; u8 Status ;
        //   one byte: bit7 Listed, bit6 has(Guid), bit5 has(u8) ; [PackedGuid] ; [u8]
        // 12.1 drift, and the worst one in the family: 68275 read Ticket, u64, u8, descriptor, bit - which
        // is what this used to write. With the old order the client parses the descriptor's bit header out
        // of the expiration timestamp's bytes, so the three string lengths (10/11/8 bits, copied by a
        // ReadBytes that bounds-checks only against the packet, never against the 513/1025/129-byte target
        // buffers) come out of effectively random data. On a large enough packet that overruns client memory.
        // The two trailing optionals are new in 12.1 and were missing completely. They are not guesses: the
        // three captured 12.1 payloads (69273, 81/74/81 bytes) round-trip byte-identically through the
        // layout below, and the first optional decodes to a HighGuid::Player guid that stays constant
        // across two different listings while the ticket's own guid is a HighGuid::Party guid that changes
        // with each one - so the ticket keys the party and this field names the leader.
        // The second optional appeared only in the delist payload (Status 0x08, Listed = 0) with value 0.
        class LFGListUpdateStatus final : public ServerPacket
        {
        public:
            explicit LFGListUpdateStatus() : ServerPacket(SMSG_LFG_LIST_UPDATE_STATUS, 96) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            ListingDescriptor Listing;
            uint64 ExpirationTime = 0;      // unix seconds the listing expires (sniff: post + 1800); 0 = not listed
            uint8 Status = 0;               // sniff codes: 0x06+0x38 create (twice), 0x38 steady, 0x19 member
                                            // join, 0x08 delist, 0x01 left group
            bool Listed = true;
            Optional<ObjectGuid> LeaderGuid;    // new in 12.1; sniff-decoded as the leader's player guid
            Optional<uint8> UnkByte;            // UNVERIFIED: new in 12.1, only seen on the delist payload,
                                                // value 0, and no consumer evidence for its meaning. Set by
                                                // LFGListMgr::DelistAndNotify (the only delist writer) and
                                                // deliberately left unset on the listed form, which is what
                                                // all three reference captures show.
        };

        // SMSG_LFG_LIST_UPDATE_EXPIRATION (0x5A000B) - dispatcher case @ RVA 0x755C5A:
        //   RideTicket ; u64 ; u8 Reason
        // Stock defect (68275 read the same): the u64 was missing, so the packet was 8 bytes short, the
        // client hit its overflow marker and dropped it - the expiry reason never arrived.
        class LFGListUpdateExpiration final : public ServerPacket
        {
        public:
            explicit LFGListUpdateExpiration() : ServerPacket(SMSG_LFG_LIST_UPDATE_EXPIRATION, 16 + 4 + 4 + 8 + 1 + 8 + 1) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint64 ExpirationTime = 0;      // UNVERIFIED: position measured, meaning inferred from the name
            uint8 Reason = 0;
        };

        // SMSG_LFG_LIST_SEARCH_STATUS (0x5A0003) - dispatcher case @ RVA 0x75568B:
        //   RideTicket ; u8 Status ; one byte with bit7 Complete
        // Stock defect: the ticket was missing (68275 read it too).
        // Note this class still has no sender, on purpose: the 68974 capture shows retail answering a
        // search with results only and never with a status message.
        class LFGListSearchStatus final : public ServerPacket
        {
        public:
            explicit LFGListSearchStatus() : ServerPacket(SMSG_LFG_LIST_SEARCH_STATUS, 16 + 4 + 4 + 8 + 1 + 1 + 1) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;
            uint8 Status = 0;
            bool Complete = true;
        };

        // One member record of a search-result row. Client 12.1.0.69382 reader @ RVA 0x7580F0
        // (in-memory stride 96), tail block reader @ RVA 0x6E7EA0.
        //   PackedGuid ; u8 Level ; u8 ClassID ; u8 Role ; u32 SpecID ; u8 ;
        //   tail { PackedGuid ; u32 x5 ; u64 ; u64 ; u32 ; one byte bit7 } ;
        //   one byte bit7 IsLeader
        // 12.1 drift: the head flag byte moved from BEFORE the tail block to AFTER it. Emitting it early
        // shifted the whole 45-byte tail by one byte.
        struct SearchResultMember
        {
            ObjectGuid Guid;
            uint8 Level = 0;
            uint8 ClassID = 0;
            uint8 Role = 0;                       // 0 tank / 1 healer / 2 dps
            uint32 SpecID = 0;
            bool IsLeader = false;
        };

        // One row of SMSG_LFG_LIST_SEARCH_RESULTS. Client 12.1.0.69382 reader @ RVA 0x758320,
        // in-memory stride 2168. Read order:
        //   RideTicket ; u32 Age(+40) ; ListingDescriptor(+48) ; u8(+1816) ; 5x PackedGuid(+1824..+1888) ;
        //   u32(+1904) u32(+1908) u32(+1912) ;
        //   u32 Count1 ; u32 Count2 ; u32 Count3 ; u32 MemberCount ;
        //   u32(+2016) ; u64 PostTime(+2024) ; u8(+2032) ; PackedGuid(+2040) ;
        //   DungeonScoreSummary(+2056) ; 9 x { u32, u8 } (+2088, reader 0x740020) ; u8(+2160) ; u8(+2161) ;
        //   Count1 x PackedGuid ; Count2 x PackedGuid ; Count3 x PackedGuid ; MemberCount x member ;
        //   one byte with bit7(+1916)
        // 12.1 drift, and the second dangerous one: in 68275 the descriptor sat at position 15, the score
        // block at 17 and the trailing bit before the members. With the old order the client reads
        // Count1..MemberCount out of descriptor bytes - four unchecked uint32 that go straight into vector
        // resizes. Practical effect ranges from an empty group browser to a client crash.
        struct SearchResultListing
        {
            ObjectGuid GroupGuid;                 // party/group guid (also echoed as the trailing guid)
            uint32 ListingId = 0;                 // stable id the client sends back in APPLY_TO_GROUP
            uint64 PostTime = 0;                  // listing creation unix seconds (emitted twice)
            uint32 Age = 0;                       // slow refresh/age counter
            ObjectGuid LeaderGuid;                // fills the five-guid block
            std::vector<SearchResultMember> Members;
            ListingDescriptor Listing;
        };

        class LFGListSearchResults final : public ServerPacket
        {
        public:
            explicit LFGListSearchResults() : ServerPacket(SMSG_LFG_LIST_SEARCH_RESULTS, 8) { }
            WorldPacket const* Write() override;

            std::vector<SearchResultListing> Listings;
        };

        // SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE (0x5A0010) - live refresh push for rows already returned.
        // Client 12.1.0.69382: u32 RowCount, then RowCount rows through reader 0x758640 (stride 2008):
        //   RideTicket ; u32 Age(+40) ; u32 MemberCount ; ListingDescriptor(+224) ; u8(+2002) ;
        //   MemberCount x member(0x7580F0) ;
        //   three bit bytes carrying 21 bits, then the present optionals in order: PackedGuid, u32, u32,
        //   PackedGuid, PackedGuid, PackedGuid, PackedGuid.
        // The 21 bits, counted off the reader MSB-first (two earlier comments itemized this differently and
        // both were wrong - one totalled 20, the other folded the conditional's presence flag in with the
        // plain bools; the real split is 7 + 12 + 1 + 1):
        //   byte 1: has(GuidA)+48 . has(U32A)+72 . has(CondBool)+81 . has(U32B)+84 . bool+120 . bool+121 .
        //           has(GuidB)+128 . has(GuidC)+152
        //   byte 2: has(GuidD)+176 . has(GuidE)+200 . bool+1992 . bool+1993 . bool+1994 . bool+1995 .
        //           bool+1996 . bool+1997
        //   byte 3: bool+1998 . bool+1999 . bool+2000 . bool+2001 . value of CondBool -> +80 (stored only
        //           when has(CondBool) is set) . 3 bits padding to the byte
        // So: 7 presence flags for the seven trailing optionals, 12 plain bools, and a pair (presence flag in
        // byte 1, value in byte 3) for one in-band bool. 7 + 12 + 1 + 1 = 21.
        // UNVERIFIED: what any of the 12 bools MEAN. The shape above is read off the reader; the semantics
        // are not, and every bit goes out as zero.
        // 12.1 drift: 68275 read Ticket, Age, MemberCount, u8, three bit bytes, descriptor, optionals,
        // members. The old writer emitted `u8 0, u32 8, 26 zero bytes` which happens to be exactly the 68275
        // shape (three bit bytes 08 00 00 plus a 27-byte zero descriptor) and is wrong for 12.1.
        // We emit all 21 bits as zero and therefore no optionals. The 0x08 bit the 68275 writer carried is
        // NOT reproduced: in 12.1 that bit position belongs to a different field of a re-laid-out struct, so
        // copying it forward would assert something we cannot support.
        class LFGListSearchResultsUpdate final : public ServerPacket
        {
        public:
            explicit LFGListSearchResultsUpdate() : ServerPacket(SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE, 8) { }
            WorldPacket const* Write() override;

            std::vector<SearchResultListing> Listings;
        };

        // SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE (0x5A0022) - 12.1 newcomer.
        // Client 12.1.0.69382, dispatcher case @ RVA 0x75699F, consumer @ RVA 0x24DEAA0 (registered by the
        // LFG-list registrar pair 0x2086FB0 / 0x24DF000, which owns every LFG_LIST slot and no classic one -
        // that is what places this opcode in the premade group finder rather than in Mythic+ scoring).
        //   ListingDescriptor Listing ; one byte with bit7 has(CensorCode) ; [u8 CensorCode]
        // Structurally this is SMSG_LFG_LIST_UPDATE_STATUS without the ticket and without the status tail:
        // both cases call the same descriptor reader.
        // Effect: the consumer stores {CensorCode, HasCensorCode} into the LFG-list manager at +0x13C1/+0x13C2,
        // sets the resolution state at +0x13E0 to (HasCensorCode && CensorCode != 0) and fires
        // LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE(isCensored) with that bool. The state is tri-valued:
        // 1 = flagged and unresolved (C_LFGList.IsCensoredActiveEntryUnresolved, 0x1194870),
        // 2 = the player confirmed it (C_LFGList.ConfirmCensoredActiveEntry),
        // 0 = revealed/cleared (C_LFGList.RevealCensoredActiveEntry).
        // So marking a listing as flagged needs BOTH the presence bit AND a non-zero code.
        // Note what that bool implies: this message can only ever drive the state to 0 or 1. The 2 is written
        // by the client alone, and any server-sent update after a confirmation would silently revoke it. The
        // senders therefore never repeat this message for a listing the player has already confirmed.
        // UNVERIFIED: the value range of CensorCode. The client only ever tests it against zero.
        class LFGListCensoredActiveEntryUpdate final : public ServerPacket
        {
        public:
            explicit LFGListCensoredActiveEntryUpdate() : ServerPacket(SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE, 32) { }
            WorldPacket const* Write() override;

            ListingDescriptor Listing;
            Optional<uint8> CensorCode;
        };

        // Wire state bits for application status (sniff MSB-first): 0x40 applied/pending, 0x20 invited,
        // 0xA0 invite accepted. Declined/cancelled bytes were not captured (best-effort 0x10).
        namespace ApplicationStateBits
        {
            constexpr uint8 Applied  = 0x40;
            constexpr uint8 Invited  = 0x20;
            constexpr uint8 Accepted = 0xA0;
            constexpr uint8 Declined = 0x10;    // UNVERIFIED (not in the capture)
        }

        // One applicant entry of SMSG_LFG_LIST_APPLICANT_LIST_UPDATE. Client 12.1.0.69382 reader @ RVA
        // 0x754CF0 (in-memory stride 352):
        //   RideTicket ; PackedGuid ; u32 MemberCount ; MemberCount x <member block, stride 248> ;
        //   bits<4> StateBits ; bit ; bits<8> len(Comment) ; FlushBits ; Comment bytes
        // 12.1 drift, currently latent: in 68275 the 13-bit block sat BEFORE the member array. We always
        // send MemberCount == 0, so the two orders coincide and the wire is identical today. It stops being
        // identical the moment anyone fills the member array - which is why the bits are now written
        // explicitly instead of as two hand-packed bytes.
        struct ApplicantInfo
        {
            LFG::RideTicket Ticket;             // application ticket (type 6, Id = ApplicationId)
            ObjectGuid PlayerGuid;
            uint8 StateBits = 0;                // occupies the top 4 bits of the wire
            bool Flag = false;
            std::string Comment;                // the applicant's note, bits<8> length
        };

        class LFGListApplicantListUpdate final : public ServerPacket
        {
        public:
            explicit LFGListApplicantListUpdate() : ServerPacket(SMSG_LFG_LIST_APPLICANT_LIST_UPDATE, 48) { }
            WorldPacket const* Write() override;

            LFG::RideTicket ListingTicket;      // listing ticket (type 4, Id = ListingId)
            uint32 Unknown = 0;                 // sniff values 25/60/6
            std::vector<ApplicantInfo> Applicants;
        };

        // SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE (0x5A000C) - dispatcher case @ RVA 0x755CE8:
        //   RideTicket Ticket ; RideTicket ListingTicket ; u64 ; u32 UnkResult ; u8 RoleGranted ;
        //   one byte whose top 4 bits are StateBits
        // 12.1 drift: 68275 had the second ticket after the three scalars. Total length is unchanged, so
        // nothing overflows - but the client read the listing ticket out of {u64, u32, u8, ...} and the
        // scalars out of the ticket's interior, i.e. application id, listing id and granted role were all
        // garbage on every status change.
        // StateBits is correctly modelled as an already-shifted value: the client keeps only bits 7..4.
        class LFGListApplicationStatusUpdate final : public ServerPacket
        {
        public:
            explicit LFGListApplicationStatusUpdate() : ServerPacket(SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE, 80) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;             // application ticket (type 6)
            LFG::RideTicket ListingTicket;      // listing ticket (type 4)
            uint64 Unknown = 0;
            uint32 UnkResult = 8;               // 8 while pending, 60 on invite (likely the invite window)
            uint8 RoleGranted = 0;
            uint8 StateBits = 0;
        };

        // SMSG_LFG_LIST_APPLY_TO_GROUP_RESULT (0x5A000D) - dispatcher case @ RVA 0x755DCC:
        //   RideTicket Ticket ; RideTicket ListingTicket ; <full search row, reader 0x758320> ;
        //   u64 ApplicationExpiration ; u8 Status ; u8 ; one byte whose top 4 bits are StateBits
        // 12.1 drift: the whole 14-byte scalar tail moved BEHIND the row and the second ticket moved 14
        // bytes forward. Inherits the search-row defect as well, so before this fix the embedded row put
        // unchecked counters into the client's allocator.
        // The row is written as a complete row (its header block IS a RideTicket in disguise: group guid,
        // listing id, type 4, post time, one bit), which is why the listing ticket appears to be sent twice.
        class LFGListApplyToGroupResult final : public ServerPacket
        {
        public:
            explicit LFGListApplyToGroupResult() : ServerPacket(SMSG_LFG_LIST_APPLY_TO_GROUP_RESULT, 128) { }
            WorldPacket const* Write() override;

            LFG::RideTicket Ticket;             // application ticket (type 6)
            LFG::RideTicket ListingTicket;      // listing ticket (type 4)
            SearchResultListing Row;
            uint64 ApplicationExpiration = 0;
            uint8 Status = 6;
            uint8 Unknown = 0;
            uint8 StateBits = 0x10;             // observed constant
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
    }
}

#endif // TRINITYCORE_LFG_LIST_PACKETS_H

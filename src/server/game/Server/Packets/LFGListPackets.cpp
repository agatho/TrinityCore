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

#include "LFGListPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::LFGList
{
namespace
{
    // Client buffer capacities of the three descriptor strings; they decide the bit width of each length
    // prefix (N = ceil(log2(capacity))) and, because the client appends its own NUL at buf[len], the
    // largest string that still fits.
    constexpr std::size_t DESCRIPTOR_NAME_CAPACITY = 513;        // bits<10>
    constexpr std::size_t DESCRIPTOR_COMMENT_CAPACITY = 1025;    // bits<11>
    constexpr std::size_t DESCRIPTOR_VOICECHAT_CAPACITY = 129;   // bits<8>

    // The trailing uint32 vector's count is a 5-bit field, so it can never legitimately exceed 31.
    constexpr uint32 DESCRIPTOR_MAX_ACTIVITY_IDS = 31;
}

// The embedded DungeonScoreSummary. MythicPlusPacketsCommon already provides operator<< for both this and
// its element type and both are byte-identical to the client's reader 0x6EC830 / writer 0x6EC980
// ({float, float, u32 count} then count x {i32, float, i32, i32, u8, one bit + flush}); only the read direction
// is missing there, so it lives here.
static ByteBuffer& operator>>(ByteBuffer& data, MythicPlus::DungeonScoreMapSummary& run)
{
    data >> run.ChallengeModeID;
    data >> run.MapScore;
    data >> run.BestRunLevel;
    data >> run.BestRunDurationMS;
    data >> run.Unknown1110;
    data >> Bits<1>(run.FinishedSuccess);
    data.ResetBitPos();
    return data;
}

static ByteBuffer& operator>>(ByteBuffer& data, MythicPlus::DungeonScoreSummary& summary)
{
    data >> summary.OverallScoreCurrentSeason;
    data >> summary.LadderScoreCurrentSeason;

    uint32 runCount = 0;
    data >> runCount;
    // 18 wire bytes per run; refuse a count the packet cannot possibly contain rather than trusting it.
    if (runCount <= (data.size() - data.rpos()) / 18)
    {
        summary.Runs.resize(runCount);
        for (MythicPlus::DungeonScoreMapSummary& run : summary.Runs)
            data >> run;
    }
    return data;
}

// ---------------------------------------------------------------------------------------------------
// ListingDescriptor. See the header for the full layout and for what is measured vs. inferred.
// Reads are size-guarded throughout: the descriptor is variable-length and largely pass-through, so a
// malformed tail must be tolerated, never fatal.
static ByteBuffer& operator>>(ByteBuffer& data, ListingDescriptor& d)
{
    auto remaining = [&]() -> std::size_t { return data.size() - data.rpos(); };

    // --- bit header, MSB-first, 43 bits (client reader 0x7572C0 / writer 0x757660) ---
    uint32 const activityCount = data.ReadBits(5);
    uint32 const nameLength = data.ReadBits(10);
    uint32 const commentLength = data.ReadBits(11);
    uint32 const voiceChatLength = data.ReadBits(8);
    d.IsAutoAccept = data.ReadBits(1) != 0;             // +1699
    d.IsPrivateGroup = data.ReadBits(1) != 0;           // +1700
    d.IsWarMode = data.ReadBits(1) != 0;                // +1701
    d.IsCrossFactionListing = data.ReadBits(1) != 0;    // +1702
    bool const hasQuestId = data.ReadBits(1) != 0;
    bool const hasRequiredDungeonScore = data.ReadBits(1) != 0;
    bool const hasRequiredPvpRating = data.ReadBits(1) != 0;
    bool const hasPlaystyle = data.ReadBits(1) != 0;
    d.NewPlayerFriendly = data.ReadBits(1) != 0;        // +1763
    data.ResetBitPos();                         // 5 spare bits; the client never reads them

    // --- byte-aligned body ---
    data >> d.CategoryID;
    data >> d.RequiredItemLevel;
    // The client zeroes this block on every CreateListing / UpdateListing, so an inbound descriptor always
    // carries an empty summary. Read it anyway - it is 12 bytes of wire either way.
    data >> d.LeaderScore;
    data >> d.GeneralPlaystyle;

    // --- deferred: the uint32 vector, then the three strings ---
    if (activityCount <= DESCRIPTOR_MAX_ACTIVITY_IDS && activityCount <= remaining() / 4)
    {
        d.ActivityIDs.resize(activityCount);
        for (uint32& activityId : d.ActivityIDs)
            data >> activityId;
    }

    if (nameLength <= remaining() && nameLength < DESCRIPTOR_NAME_CAPACITY)
        d.Name.assign(data.ReadString(nameLength));
    if (commentLength <= remaining() && commentLength < DESCRIPTOR_COMMENT_CAPACITY)
        d.Comment.assign(data.ReadString(commentLength));
    if (voiceChatLength <= remaining() && voiceChatLength < DESCRIPTOR_VOICECHAT_CAPACITY)
        d.VoiceChat.assign(data.ReadString(voiceChatLength));

    // --- deferred: the present optionals ---
    if (hasQuestId && remaining() >= 4)              { uint32 v; data >> v; d.QuestID = v; }
    if (hasRequiredDungeonScore && remaining() >= 4) { uint32 v; data >> v; d.RequiredDungeonScore = v; }
    if (hasRequiredPvpRating && remaining() >= 4)    { uint32 v; data >> v; d.RequiredPvpRating = v; }
    if (hasPlaystyle && remaining() >= 1)            { uint8 v;  data >> v; d.Playstyle = v; }
    return data;
}

static ByteBuffer& operator<<(ByteBuffer& data, ListingDescriptor const& d)
{
    // The client copies each string into a fixed-size buffer and writes its own NUL at buf[len], so
    // capacity-1 is the largest length it can survive. ReadBytes on that side bounds-checks against the
    // remaining packet only, never against the target buffer - an over-long string is a client-side
    // memory overrun, so clamp here.
    std::size_t const nameLength = std::min<std::size_t>(d.Name.length(), DESCRIPTOR_NAME_CAPACITY - 1);
    std::size_t const commentLength = std::min<std::size_t>(d.Comment.length(), DESCRIPTOR_COMMENT_CAPACITY - 1);
    std::size_t const voiceChatLength = std::min<std::size_t>(d.VoiceChat.length(), DESCRIPTOR_VOICECHAT_CAPACITY - 1);
    std::size_t const activityCount = std::min<std::size_t>(d.ActivityIDs.size(), DESCRIPTOR_MAX_ACTIVITY_IDS);

    data.WriteBits(activityCount, 5);
    data.WriteBits(nameLength, 10);
    data.WriteBits(commentLength, 11);
    data.WriteBits(voiceChatLength, 8);
    data << Bits<1>(d.IsAutoAccept);                    // +1699
    data << Bits<1>(d.IsPrivateGroup);                  // +1700
    data << Bits<1>(d.IsWarMode);                       // +1701
    data << Bits<1>(d.IsCrossFactionListing);           // +1702
    data << OptionalInit(d.QuestID);
    data << OptionalInit(d.RequiredDungeonScore);
    data << OptionalInit(d.RequiredPvpRating);
    data << OptionalInit(d.Playstyle);
    data << Bits<1>(d.NewPlayerFriendly);               // +1763
    data.FlushBits();

    data << uint32(d.CategoryID);
    data << uint32(d.RequiredItemLevel);
    data << d.LeaderScore;
    data << uint8(d.GeneralPlaystyle);

    for (std::size_t i = 0; i < activityCount; ++i)
        data << uint32(d.ActivityIDs[i]);

    data.WriteString(d.Name.c_str(), nameLength);
    data.WriteString(d.Comment.c_str(), commentLength);
    data.WriteString(d.VoiceChat.c_str(), voiceChatLength);

    if (d.QuestID)
        data << uint32(*d.QuestID);
    if (d.RequiredDungeonScore)
        data << uint32(*d.RequiredDungeonScore);
    if (d.RequiredPvpRating)
        data << uint32(*d.RequiredPvpRating);
    if (d.Playstyle)
        data << uint8(*d.Playstyle);
    return data;
}

// ---- CMSG Read ----

void LFGListJoin::Read()
{
    _worldPacket >> Listing;
}

void LFGListUpdateRequest::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> Listing;
}

void LFGListLeave::Read()
{
    _worldPacket >> Ticket;
}

std::vector<std::vector<std::string>> LFGListSearch::GetKeywords() const
{
    std::vector<std::vector<std::string>> keywords;
    keywords.reserve(Terms.size());
    for (LFGListSearchTerm const& term : Terms)
    {
        std::vector<std::string> alternatives;
        for (std::string const& value : term.Values)
            if (!value.empty())
                alternatives.push_back(value);

        // A block with nothing in it constrains nothing. Keeping it would turn every search that carries a
        // trailing empty block into a search no listing can satisfy.
        if (!alternatives.empty())
            keywords.push_back(std::move(alternatives));
    }
    return keywords;
}

void LFGListSearch::Read()
{
    // Client writer 0x757EC0; term blocks 0x757D00. See the header for the three defects this replaces.
    uint32 const termCount = _worldPacket.ReadBits(5);
    _worldPacket >> Bits<1>(CrossFaction);
    _worldPacket.ResetBitPos();

    _worldPacket >> CategoryID;
    _worldPacket >> Filter;
    _worldPacket >> PreferredFilters;
    _worldPacket >> LanguageMask;
    uint32 resolvedActivityCount = 0;
    _worldPacket >> resolvedActivityCount;
    _worldPacket >> AdvancedFilterMask;
    uint32 activityGroupCount = 0;
    _worldPacket >> activityGroupCount;
    uint32 activityCount = 0;
    _worldPacket >> activityCount;
    _worldPacket >> MinimumRating;
    _worldPacket >> FilterByte1;
    _worldPacket >> FilterByte2;
    uint32 guidCount = 0;
    _worldPacket >> guidCount;

    // A term block is at least 8 bytes (the interleaved 60-bit header) - refuse counts the packet cannot
    // hold before allocating.
    auto remaining = [&]() -> std::size_t { return _worldPacket.size() - _worldPacket.rpos(); };
    if (termCount && termCount <= remaining() / 8)
    {
        Terms.resize(termCount);
        for (LFGListSearchTerm& term : Terms)
        {
            std::array<uint32, LFGListSearchTerm::MAX_VALUES> lengths = { };
            for (std::size_t i = 0; i < LFGListSearchTerm::MAX_VALUES; ++i)
            {
                lengths[i] = _worldPacket.ReadBits(5);      // client buffer 32 -> ceil(log2(32)) = 5
                term.Flags[i] = _worldPacket.ReadBit();     // one presence bit per slot, interleaved
            }
            _worldPacket.ResetBitPos();                     // 60 bits used, 4 padding -> 8 bytes

            for (std::size_t i = 0; i < LFGListSearchTerm::MAX_VALUES; ++i)
                if (lengths[i] && lengths[i] < LFGListSearchTerm::MAX_VALUE_LENGTH && lengths[i] <= remaining())
                    term.Values[i] = _worldPacket.ReadString(lengths[i]);
        }
    }

    auto readValues = [&](std::vector<uint32>& out, uint32 count)
    {
        if (!count || count > remaining() / 4)
            return;

        out.resize(count);
        for (uint32& value : out)
            _worldPacket >> value;
    };
    // Order on the wire is fixed by the client writer and is NOT the order the counts appear in: the
    // count of the first list sits at struct +48, i.e. between LanguageMask and AdvancedFilterMask,
    // while the list itself follows the term blocks. See the header for what each one carries.
    readValues(ResolvedActivityIDs, resolvedActivityCount);
    readValues(ActivityGroupIDs, activityGroupCount);
    readValues(ActivityIDs, activityCount);

    if (guidCount && guidCount <= remaining() / 2)   // a PackedGuid is at least its 2-byte mask
    {
        Guids.resize(guidCount);
        for (ObjectGuid& guid : Guids)
            _worldPacket >> guid;
    }
}

void LFGListApplyToGroup::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> ActivityID;
    _worldPacket >> RoleMask;
    _worldPacket >> SizedString::BitsSize<8>(Comment);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::Data(Comment);
}

void LFGListCancelApplication::Read()
{
    _worldPacket >> Ticket;
}

void LFGListDeclineApplicant::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> ApplicantTicket;
}

void LFGListInviteApplicant::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> ApplicantTicket;

    uint32 inviteeCount = 0;
    _worldPacket >> inviteeCount;
    // Each invitee is at least 3 bytes (2-byte guid mask + role); a group cannot exceed MAX_RAID_SIZE
    // anyway, so a large count is malformed either way.
    if (inviteeCount && inviteeCount <= (_worldPacket.size() - _worldPacket.rpos()) / 3)
    {
        Invitees.resize(inviteeCount);
        for (LFGListInvitee& invitee : Invitees)
        {
            _worldPacket >> invitee.Guid;
            _worldPacket >> invitee.RoleMask;
        }
    }
}

void LFGListInviteResponse::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> Bits<1>(Accept);
    _worldPacket.ResetBitPos();
}

void LFGListConfirmCensoredActiveEntry::Read()
{
    _worldPacket >> Ticket;
}

// ---- SMSG Write ----

WorldPacket const* LFGListJoinResult::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint32(Status);
    _worldPacket << uint8(Result);
    _worldPacket << uint8(ResultDetail);

    return &_worldPacket;
}

WorldPacket const* LFGListUpdateStatus::Write()
{
    _worldPacket << Ticket;
    _worldPacket << Listing;                        // 12.1: directly behind the ticket, not at the end
    _worldPacket << uint64(Listed ? ExpirationTime : 0);
    _worldPacket << uint8(Status);
    _worldPacket << Bits<1>(Listed);
    _worldPacket << OptionalInit(LeaderGuid);
    _worldPacket << OptionalInit(UnkByte);
    _worldPacket.FlushBits();
    if (LeaderGuid)
        _worldPacket << *LeaderGuid;
    if (UnkByte)
        _worldPacket << uint8(*UnkByte);

    return &_worldPacket;
}

WorldPacket const* LFGListUpdateExpiration::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint64(ExpirationTime);
    _worldPacket << uint8(Reason);

    return &_worldPacket;
}

WorldPacket const* LFGListSearchStatus::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint8(Status);
    _worldPacket << Bits<1>(Complete);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

// One member record. Client reader 0x7580F0 + tail 0x6E7EA0; the head flag byte comes AFTER the tail.
static void WriteSearchResultMember(ByteBuffer& data, SearchResultMember const& member)
{
    data << member.Guid;
    data << uint8(member.Level);
    data << uint8(member.ClassID);
    data << uint8(member.Role);
    data << uint32(member.SpecID);
    data << uint8(0);                          // +24, zero-filled (no consumer evidence)

    // tail block (0x6E7EA0)
    data << member.Guid;
    data << uint32(0);                         // +20
    data << uint32(0);                         // +24
    data << uint32(0);                         // +28
    data << uint32(0);                         // +32
    data << uint32(0);                         // +36
    data << uint64(0);                         // +40
    data << uint64(0);                         // +48
    data << uint32(0);                         // +56
    data << Bits<1>(false);                    // tail flag (+16)
    data.FlushBits();

    // 12.1: the head flag byte is written here, behind the tail. Emitting it before the tail (the 68275
    // order) shifted every one of the tail's 45 bytes.
    data << Bits<1>(member.IsLeader);
    data.FlushBits();
}

// One SMSG_LFG_LIST_SEARCH_RESULTS row, client reader 0x758320. Order per the header comment.
static ByteBuffer& operator<<(ByteBuffer& data, SearchResultListing const& row)
{
    // The row header is a RideTicket in disguise: guid, id, type 4, time, one bit.
    data << row.GroupGuid;
    data << uint32(row.ListingId);
    data << uint32(4);                              // RideType::LfgListListing
    data << uint64(row.PostTime);
    data << Bits<1>(false);                         // IsCrossFaction
    data.FlushBits();

    data << uint32(row.Age);                        // +40
    data << row.Listing;                            // +48, 12.1: position 3, was position 15 in 68275
    // UNVERIFIED: constant taken from observation, not from a decoded consumer - 68974 carried 3 here,
    // 68275 carried 5, and no 12.1 capture of this message exists. Its meaning is unknown.
    data << uint8(3);                               // +1816
    data << row.LeaderGuid;                         // +1824
    data << row.LeaderGuid;                         // +1840
    data << row.LeaderGuid;                         // +1856
    data << row.LeaderGuid;                         // +1872
    data << row.LeaderGuid;                         // +1888
    data << uint32(0);                              // +1904
    data << uint32(0);                              // +1908
    data << uint32(0);                              // +1912
    data << uint32(0);                              // GuidList1 count
    data << uint32(0);                              // GuidList2 count
    data << uint32(0);                              // GuidList3 count
    data << uint32(uint32(row.Members.size()));     // MemberCount
    data << uint32(0);                              // +2016
    data << uint64(row.PostTime);                   // +2024
    data << uint8(0);                               // +2032
    data << row.GroupGuid;                          // +2040
    data << row.Listing.LeaderScore;                // +2056, 12.1: before the 9-entry table, was after it

    // Source for the VALUES, not just the shape - it was dropped when this block was rewritten for 12.1 and
    // is restored here verbatim: the 12.0.7.68974 capture carries every entry as {u32 0, u8 index}. An
    // earlier writer emitted {u32 index, u8 0}, which has the same byte count and put the running index into
    // the wrong client field. 68974 remains the only measurement of these values. The three 12.1 recordings
    // of this opcode that DO exist (c:\dumps\wpp_work\lfg_ref\69273_s69273_a_5A0002_0/1/2.bin) cannot
    // confirm them: each body is 6 bytes, 00 00 00 00 00 00, i.e. the empty form - u16 row count 0 followed
    // by the trailing u32 - so not one of them reaches a row, let alone this table. (An earlier version of
    // this note cited "0x3D0259" for the missing capture. That is CMSG_LFG_LOREWALKING_UPDATE_REQUEST, a
    // different opcode of this same unit; the message being written here is SMSG_LFG_LIST_SEARCH_RESULTS =
    // 0x5A0002.)
    // What 12.1 changed is the POSITION of the table relative to LeaderScore, and that is read from the
    // reader at RVA 0x740020, not from any capture.
    for (uint32 i = 0; i < 9; ++i)                  // +2088, fixed 9-entry {u32,u8} table (reader 0x740020)
    {
        data << uint32(0);
        data << uint8(i);
    }
    // UNVERIFIED: same class as +1816 - 68974 carried 3, meaning unknown, no 12.1 capture to confirm it.
    data << uint8(3);                               // +2160
    data << uint8(0);                               // +2161

    // the three PackedGuid lists are empty (counts written as 0 above) -> nothing to emit

    for (SearchResultMember const& member : row.Members)
        WriteSearchResultMember(data, member);

    // 12.1: the trailing bit belongs at the very end, behind the members.
    data << Bits<1>(false);                         // +1916
    data.FlushBits();

    return data;
}

WorldPacket const* LFGListSearchResults::Write()
{
    _worldPacket << uint16(Listings.size());        // duplicate row-count hint (== RowCount in every sniff)
    _worldPacket << uint32(Listings.size());
    for (SearchResultListing const& row : Listings)
        _worldPacket << row;

    return &_worldPacket;
}

WorldPacket const* LFGListSearchResultsUpdate::Write()
{
    _worldPacket << uint32(Listings.size());
    for (SearchResultListing const& row : Listings)
    {
        // header block: again a RideTicket in disguise
        _worldPacket << row.GroupGuid;
        _worldPacket << uint32(row.ListingId);
        _worldPacket << uint32(4);                  // RideType::LfgListListing
        _worldPacket << uint64(row.PostTime);
        _worldPacket << Bits<1>(false);
        _worldPacket.FlushBits();

        _worldPacket << uint32(row.Age);            // +40
        _worldPacket << uint32(row.Members.size()); // member count
        _worldPacket << row.Listing;                // +224
        _worldPacket << uint8(0);                   // +2002

        for (SearchResultMember const& member : row.Members)
            WriteSearchResultMember(_worldPacket, member);

        // 21 bits across three bytes (+3 padding): 7 presence flags for the seven trailing optionals,
        // 12 plain bools, and a presence/value pair for one in-band bool - the per-bit map is in the header.
        // All zero -> no optionals follow. UNVERIFIED: what the 12 bools mean.
        for (uint32 i = 0; i < 21; ++i)
            _worldPacket << Bits<1>(false);
        _worldPacket.FlushBits();
    }

    return &_worldPacket;
}

WorldPacket const* LFGListCensoredActiveEntryUpdate::Write()
{
    _worldPacket << Listing;
    _worldPacket << OptionalInit(CensorCode);
    _worldPacket.FlushBits();
    if (CensorCode)
        _worldPacket << uint8(*CensorCode);

    return &_worldPacket;
}

WorldPacket const* LFGListApplicantListUpdate::Write()
{
    _worldPacket << ListingTicket;
    _worldPacket << Size<uint32>(Applicants);
    _worldPacket << uint32(Unknown);            // UNVERIFIED: see the field's note in LFGListPackets.h
    for (ApplicantInfo const& applicant : Applicants)
    {
        _worldPacket << applicant.Ticket;
        _worldPacket << applicant.PlayerGuid;
        // Member snapshot list. Kept empty: the full form carries a 248-byte record per member whose
        // scalars are not resolved, and the client renders the status-only form fine.
        _worldPacket << uint32(0);
        // 12.1: the 13-bit block sits behind the member array. Written out bit by bit rather than as two
        // hand-packed bytes so it stays correct if the member list is ever filled.
        _worldPacket.WriteBits(applicant.StateBits >> 4, 4);
        _worldPacket << Bits<1>(applicant.Flag);
        _worldPacket << SizedString::BitsSize<8>(applicant.Comment);
        _worldPacket.FlushBits();
        _worldPacket << SizedString::Data(applicant.Comment);
    }

    return &_worldPacket;
}

WorldPacket const* LFGListApplicationStatusUpdate::Write()
{
    _worldPacket << Ticket;
    _worldPacket << ListingTicket;                  // 12.1: pulled forward, adjacent to the first ticket
    _worldPacket << uint64(ApplicationExpiration);
    _worldPacket << uint32(UnkResult);
    _worldPacket << uint8(RoleGranted);
    _worldPacket << uint8(StateBits);               // client keeps bits 7..4 only

    return &_worldPacket;
}

WorldPacket const* LFGListApplyToGroupResult::Write()
{
    _worldPacket << Ticket;
    _worldPacket << ListingTicket;
    _worldPacket << Row;                            // 12.1: the row moved in front of the scalar tail
    _worldPacket << uint64(ApplicationExpiration);
    _worldPacket << uint8(Status);
    _worldPacket << uint8(RoleGranted);
    _worldPacket << uint8(StateBits);               // client keeps bits 7..4 only

    return &_worldPacket;
}

WorldPacket const* LFGListUpdateBlacklist::Write()
{
    _worldPacket << Size<uint32>(Entries);
    for (LFGListBlacklistEntry const& entry : Entries)
    {
        _worldPacket << uint32(entry.ActivityID);
        _worldPacket << uint32(entry.Reason);
    }

    return &_worldPacket;
}
}

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

// One member record, reader 0x7FF7CD528140 (see SearchResultMember).
static void WriteSearchResultMember(ByteBuffer& data, SearchResultMember const& member)
{
    data << member.Guid;
    data << uint8(member.Level);
    data << uint8(member.ClassID);
    data << uint8(member.Role);
    data << uint32(member.SpecID);
    data << uint8(member.LfgRoles);

    // tail block (RVA 0x6E7EA0): leaver bookkeeping keyed by the Battle.net account
    data << member.BnetAccountGuid;
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint64(0);
    data << uint64(0);
    data << uint32(0);
    data << Bits<1>(member.IsLeaver);
    data.FlushBits();

    data << Bits<1>(member.IsLeader);
    data.FlushBits();
}

// The descriptor as a search row carries it: the leader's score rides in the row (+2056), the embedded copy is empty in
// every retail row.
static ListingDescriptor RowDescriptor(SearchResultListing const& row)
{
    ListingDescriptor descriptor = row.Listing;
    descriptor.LeaderScore = { };
    return descriptor;
}

// One SMSG_LFG_LIST_SEARCH_RESULTS row, reader 0x7FF7CD528370 (see SearchResultListing).
static ByteBuffer& operator<<(ByteBuffer& data, SearchResultListing const& row)
{
    data << row.GroupGuid;
    data << uint32(row.ListingId);
    data << uint32(4);                              // RideType::LfgListListing
    data << uint64(row.PostTime);
    data << Bits<1>(false);                         // IsCrossFaction
    data.FlushBits();

    data << uint32(row.Revision);
    data << RowDescriptor(row);
    // Not read by any 12.1 consumer of the row; 7 in all 105 retail 12.1 rows.
    data << uint8(7);
    data << row.LeaderGuid;
    data << row.LastEditorGuid;
    data << row.NameEditorGuid;
    data << row.CommentEditorGuid;
    data << row.VoiceChatEditorGuid;
    data << uint32(row.LeaderVirtualRealmAddress);
    data << uint32(row.LeaderAreaID);
    data << uint32(0);                              // +1912: only ever set by an update record's flag bit
    data << uint32(row.BNetFriendGuids.size());
    data << uint32(row.CharacterFriendGuids.size());
    data << uint32(row.GuildMateGuids.size());
    data << uint32(row.Members.size());
    data << uint32(0);                              // +2016: only ever set by an update record
    data << uint64(row.PostTime);
    data << uint8(0);                               // +2032: no 12.1 consumer; 0 in 93 of 105 retail rows
    data << row.GroupGuid;
    data << row.LeaderScore;
    for (uint32 bracket = 0; bracket < row.LeaderPvpRatings.size(); ++bracket)
    {
        data << uint32(row.LeaderPvpRatings[bracket]);
        data << uint8(bracket);
    }
    data << uint8(row.LeaderFactionMask);
    data << uint8(row.CensorFlags);

    for (ObjectGuid const& guid : row.BNetFriendGuids)
        data << guid;
    for (ObjectGuid const& guid : row.CharacterFriendGuids)
        data << guid;
    for (ObjectGuid const& guid : row.GuildMateGuids)
        data << guid;

    for (SearchResultMember const& member : row.Members)
        WriteSearchResultMember(data, member);

    data << Bits<1>(row.HasSelf);
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

// One update record, reader 0x7FF7CD528690 (see LFGListSearchResultsUpdate).
WorldPacket const* LFGListSearchResultsUpdate::Write()
{
    _worldPacket << uint32(Listings.size());
    for (SearchResultListing const& row : Listings)
    {
        _worldPacket << row.GroupGuid;
        _worldPacket << uint32(row.ListingId);
        _worldPacket << uint32(4);                  // RideType::LfgListListing
        _worldPacket << uint64(row.PostTime);
        _worldPacket << Bits<1>(false);
        _worldPacket.FlushBits();

        _worldPacket << uint32(row.Revision);
        _worldPacket << uint32(row.Members.size());
        _worldPacket << RowDescriptor(row);
        _worldPacket << uint8(0);

        for (SearchResultMember const& member : row.Members)
            WriteSearchResultMember(_worldPacket, member);

        uint32 const changes = row.Changes;
        bool const leader = (changes & SEARCH_RESULT_CHANGE_LEADER) != 0;
        bool const name = (changes & SEARCH_RESULT_CHANGE_NAME) != 0;
        bool const comment = (changes & SEARCH_RESULT_CHANGE_COMMENT) != 0;
        bool const voiceChat = (changes & SEARCH_RESULT_CHANGE_VOICE_CHAT) != 0;

        _worldPacket << Bits<1>(leader);
        _worldPacket << Bits<1>(leader);
        _worldPacket << Bits<1>(false);             // flag +1912 present
        _worldPacket << Bits<1>(false);             // u32 +2016 present
        _worldPacket << Bits<1>(row.Delisted);
        _worldPacket << Bits<1>(row.Delisted);
        _worldPacket << Bits<1>(false);             // guid the applier does not read
        _worldPacket << Bits<1>(name);
        _worldPacket << Bits<1>(comment);
        _worldPacket << Bits<1>(voiceChat);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_REQUIRED_ITEM_LEVEL) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_AUTO_ACCEPT) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_PRIVATE) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_REQUIRED_DUNGEON_SCORE) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_REQUIRED_PVP_RATING) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_PLAYSTYLE) != 0);
        _worldPacket << Bits<1>(false);             // not read by the applier
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_CROSS_FACTION) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_ACTIVITIES) != 0);
        _worldPacket << Bits<1>((changes & SEARCH_RESULT_CHANGE_NEW_PLAYER_FRIENDLY) != 0);
        _worldPacket << Bits<1>(false);             // flag +1912 value
        _worldPacket.FlushBits();

        if (leader)
        {
            _worldPacket << row.LeaderGuid;
            _worldPacket << uint32(row.LeaderVirtualRealmAddress);
        }
        if (name)
            _worldPacket << row.NameEditorGuid;
        if (comment)
            _worldPacket << row.CommentEditorGuid;
        if (voiceChat)
            _worldPacket << row.VoiceChatEditorGuid;
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
        _worldPacket << Bits<1>(applicant.CommentUpdated);
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

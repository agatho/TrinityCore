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
// The published-listing parameters. RESOLVED from the 12.0.7.68275 premade-groups sniff + the client JOIN
// serializer (sub_7FF72914ABE0) + the generated Lua API doc (LfgListingCreateData). The descriptor is BIT-PACKED:
// a bit-packed header (5-bit trailing-vector count; three bit-packed string lengths of 10/11/8 bits; four boolean
// flags; and presence bits for the nilable numeric fields), then FlushBits, then the member-requirement block, the
// fixed activity fields, the trailing uint32 vector, the three strings, and the present optional fields. Only
// CategoryID + the activity vector + item-level drive server filtering; the rest are pass-through echo. Reads are guarded against
// over-run (the descriptor is variable-length and pass-through, so a malformed tail is tolerated, never fatal).
// Full layout + bit-widths: c:\dumps\LFG_LIST_WIRE_68275.md.
static ByteBuffer& operator>>(ByteBuffer& data, ListingDescriptor& d)
{
    auto remaining = [&]() -> std::size_t { return data.size() - data.rpos(); };

    // --- bit-packed header (client bit-writer, MSB-first) ---
    uint32 vectorCount = data.ReadBits(5);      // sub_7FF729064C20: count of the trailing uint32 vector
    uint32 str0Len = data.ReadBits(10);         // string @0x40 length
    uint32 str1Len = data.ReadBits(11);         // string @0x241 length
    uint32 str2Len = data.ReadBits(8);          // string @0x642 length ("crate" in the sniff)
    d.IsAutoAccept = data.ReadBits(1) != 0;     // presence/flag bits (client offsets 0x6c3..0x703)
    d.IsCrossFactionListing = data.ReadBits(1) != 0;
    d.IsPrivateGroup = data.ReadBits(1) != 0;
    d.NewPlayerFriendly = data.ReadBits(1) != 0;
    bool hasQuestId = data.ReadBits(1) != 0;    // 0x6cc -> uint32 @0x6c8
    bool hasOpt1 = data.ReadBits(1) != 0;       // 0x6f4 -> uint32 @0x6f0
    bool hasOpt2 = data.ReadBits(1) != 0;       // 0x6fc -> uint32 @0x6f8
    bool hasOpt3 = data.ReadBits(1) != 0;       // 0x701 -> uint8  @0x700
    data.ReadBits(1);                           // 0x703 standalone flag (unused server-side)
    data.ResetBitPos();                         // FlushBits (sub_7FF729064E60)

    // --- member-requirement block (nested sub_7FF729167840) ---
    data >> d.HeaderFloat0 >> d.HeaderFloat1;
    uint32 memberCount = 0;
    data >> memberCount;
    if (memberCount <= remaining() / 0x11)      // 0x11 = min bytes per entry; guard against a bad count
    {
        d.MemberRequirements.resize(memberCount);
        for (ListingMemberRequirement& m : d.MemberRequirements)
        {
            data >> m.Field0 >> m.Field1 >> m.Field2 >> m.Field3 >> m.Field4;
            m.Flag = data.ReadBits(1) != 0;
            data.ResetBitPos();
        }
    }

    // --- fixed activity fields ---
    data >> d.CategoryID;               // uint32 @0x38: GroupFinderCategory id (68974: 1; the activities ride in the vector below)
    data >> d.RequiredDungeonScore;     // float  @0x3c
    data >> d.TrailingByte;             // uint8  @0x702

    // --- trailing uint32 vector ---
    if (vectorCount <= remaining() / 4)
    {
        d.ActivityIDs.resize(vectorCount);
        for (uint32& v : d.ActivityIDs)
            data >> v;
    }

    // --- string data (order matches the serializer: @0x40, @0x241, @0x642) ---
    if (str0Len <= remaining()) d.Name.assign(data.ReadString(str0Len));
    if (str1Len <= remaining()) d.VoiceChat.assign(data.ReadString(str1Len));
    if (str2Len <= remaining()) d.Comment.assign(data.ReadString(str2Len));

    // --- present optional (nilable) numeric fields ---
    if (hasQuestId && remaining() >= 4) { uint32 v; data >> v; d.QuestID = v; }
    if (hasOpt1 && remaining() >= 4)    { uint32 v; data >> v; d.OptionalValue1 = v; }
    if (hasOpt2 && remaining() >= 4)    { uint32 v; data >> v; d.OptionalValue2 = v; }
    if (hasOpt3 && remaining() >= 1)    { uint8 v;  data >> v; d.OptionalValue3 = v; }
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, ListingInfo const& listing)
{
    for (uint8 param : listing.Params)
        data << param;
    data << uint32(listing.ActivityID);
    data << uint32(listing.Field1);
    data << uint8(listing.Field2);
    data << uint32(listing.RequiredItemLevel);
    data << SizedString::BitsSize<10>(listing.Comment);
    data << SizedString::BitsSize<7>(listing.LeaderName);
    data << SizedString::BitsSize<7>(listing.VoiceChat);
    data.FlushBits();
    data << SizedString::Data(listing.Comment);
    data << SizedString::Data(listing.LeaderName);
    data << SizedString::Data(listing.VoiceChat);
    data << uint32(listing.Field3);
    data << uint32(listing.Field4);
    data << uint32(listing.Field5);
    data << uint8(listing.Field6);
    return data;
}

// ---- CMSG Read ----

void LFGListJoin::Read()
{
    std::size_t const start = _worldPacket.rpos();
    _worldPacket >> Listing;
    Listing.RawBytes.assign(_worldPacket.data() + start, _worldPacket.data() + _worldPacket.rpos());
}

void LFGListUpdateRequest::Read()
{
    _worldPacket >> Ticket;
    std::size_t const start = _worldPacket.rpos();
    _worldPacket >> Listing;
    Listing.RawBytes.assign(_worldPacket.data() + start, _worldPacket.data() + _worldPacket.rpos());
}

void LFGListLeave::Read()
{
    _worldPacket >> Ticket;
}

void LFGListSearch::Read()
{
    // Sniff-exact (43B no keyword / 56B with one): see header comment. All reads size-guarded.
    uint32 const termCount = _worldPacket.ReadBits(5);
    _worldPacket.ReadBit();                             // presence/flag bit (semantics approximate)
    _worldPacket.ResetBitPos();

    if (termCount)
    {
        std::array<uint32, 10> lengths = { };
        for (uint32& len : lengths)
            len = _worldPacket.ReadBits(5);             // ten bits(5) lengths packed into the 8-byte block
        _worldPacket.ReadBits(64 - 10 * 5);             // padding to the full 8 bytes
        _worldPacket.ResetBitPos();

        SearchTerms.resize(std::min<uint32>(termCount, 10));
        for (std::size_t i = 0; i < SearchTerms.size(); ++i)
            if (lengths[i] && _worldPacket.rpos() + lengths[i] <= _worldPacket.size())
                SearchTerms[i] = _worldPacket.ReadString(lengths[i]);
    }

    for (uint32& f : Filters)
        _worldPacket >> f;
    _worldPacket >> FilterByte1;                        // observed 0xFF
    _worldPacket >> FilterByte2;                        // observed 0x05

    uint32 guidCount = 0;
    _worldPacket >> guidCount;
    if (guidCount <= 50)
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
    _worldPacket >> Field2;
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
    _worldPacket >> ListingId;
    _worldPacket >> ApplicantGuid;
    _worldPacket >> RoleMask;
    _worldPacket >> ApplicantTicket;
}

void LFGListInviteResponse::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> Bits<1>(Accept);
    _worldPacket.ResetBitPos();
}

// ---- SMSG Write ----

WorldPacket const* LFGListJoinResult::Write()
{
    _worldPacket << uint32(Status);
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* LFGListUpdateStatus::Write()
{
    // Layout proven from the sniff: Ticket, u64 ExpirationTime, Status byte, the listing descriptor echoed
    // VERBATIM, then a single Listed bit (flushed). Not listed = expiration 0 + all-zero 27-byte descriptor.
    static constexpr std::size_t EMPTY_DESCRIPTOR_SIZE = 27;

    _worldPacket << Ticket;
    _worldPacket << uint64(Listed ? ExpirationTime : 0);
    _worldPacket << uint8(Status);
    if (Listed && !RawDescriptor.empty())
        _worldPacket.append(RawDescriptor.data(), RawDescriptor.size());
    else
        _worldPacket.append(std::vector<uint8>(EMPTY_DESCRIPTOR_SIZE, 0).data(), EMPTY_DESCRIPTOR_SIZE);
    _worldPacket << Bits<1>(Listed);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* LFGListUpdateExpiration::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint8(Reason);
    return &_worldPacket;
}

WorldPacket const* LFGListSearchStatus::Write()
{
    _worldPacket << uint8(Status);
    _worldPacket << Bits<1>(Complete);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

// One MemberDetail record (head sub_7FF7291DBF80 + tail sub_7FF729162CC0), shared between the full
// search-result row and the compact SEARCH_RESULTS_UPDATE row (68974: byte-identical in both).
// Head decoded from the 68974 capture: guid, level, class, role (0 tank/1 healer/2 dps), spec; the head
// flag bit was set on both retail members (each was the listing's leader).
static void WriteSearchResultMember(ByteBuffer& data, SearchResultMember const& member)
{
    data << member.Guid;                       // PackedGuid MemberGuid
    data << uint8(member.Level);
    data << uint8(member.ClassID);
    data << uint8(member.Role);
    data << uint32(member.SpecID);
    data << uint8(0);                          // Unk24
    data << uint8(member.IsLeader ? 0x80 : 0x00);   // head flag (bit-as-byte; 68974: 1 for the leader)
    // tail (sub_7FF729162CC0)
    data << member.Guid;                       // PackedGuid MemberGuid2
    data << uint32(0);                         // T20
    data << uint32(0);                         // T24 (68974 live values 17; semantics unknown, zero-filled)
    data << uint32(0);                         // T28
    data << uint32(0);                         // T32 (68974 live values 18)
    data << uint32(0);                         // T36 (68974 live values 18)
    data << uint64(0);                         // T40
    data << uint64(0);                         // T48
    data << uint32(0);                         // T56
    data << uint8(0);                          // T_flag (bit-as-byte)
}

// Emit one SMSG_LFG_LIST_SEARCH_RESULTS row per c:\dumps\lfg_search_results_layout.md, re-verified byte-exact
// against the 12.0.7.68974 capture (both bodies consume to exact end with the same layout — no structural
// drift 68275 -> 68974). Row-level "bit" fields are full wire bytes with the boolean in bit 7 (client reads
// x >> 7); PackedGuid uses the standard TrinityCore ObjectGuid operator<< (u16 mask + data bytes). Unknown
// scalars are zero-filled (the client parses them fine); observed retail constants are mirrored
// (Unk_b=4, Unk1816/Unk2160=3 at 68974 — they were 5 at 68275).
// The row body from the age counter onward — shared verbatim between SEARCH_RESULTS rows and the row
// snapshot embedded in SMSG_LFG_LIST_APPLY_TO_GROUP_RESULT (sniff: identical bytes in both containers).
static void WriteSearchResultRowBody(ByteBuffer& data, SearchResultListing const& row)
{
    data << uint32(row.Age);                        // Unk40 (age counter; 68974 rows: 3)
    data << uint8(3);                               // Unk1816 (68974: 3; was 5 at 68275)
    data << row.LeaderGuid;                         // Guid_A
    data << row.LeaderGuid;                         // Guid_B
    data << row.LeaderGuid;                         // Guid_C
    data << row.LeaderGuid;                         // Guid_D
    data << row.LeaderGuid;                         // Guid_E
    data << uint32(0);                              // Unk1904
    data << uint32(0);                              // Unk1908
    data << uint32(0);                              // Unk1912
    data << uint32(0);                              // Count1 (GuidList1 length)
    data << uint32(0);                              // Count2 (GuidList2 length)
    data << uint32(0);                              // Count3 (GuidList3 length)
    data << uint32(uint32(row.Members.size()));     // MemberCount
    data << uint32(0);                              // Unk2016
    data << uint64(row.PostTime);                   // PostTime2 (== PostTime)
    data << uint8(0);                               // Unk2032
    data << row.GroupGuid;                          // LeaderGuidEcho (== GroupGuid)
    for (uint32 i = 0; i < 9; ++i)                  // fixed 9-entry {u32,u8} table (sub_7FF729195220)
    {
        // 68974 capture: every entry is {u32 0, u8 index} — the previous writer emitted {u32 index, u8 0},
        // which put the index into the wrong client field (same byte count, wrong values).
        data << uint32(0);
        data << uint8(i);
    }
    data << uint8(3);                               // Unk2160 (68974: 3; was 5 at 68275)
    data << uint8(0);                               // Unk2161

    // === three PackedGuid lists (all empty, Count1/2/3 == 0) -> emit nothing ===

    // === embedded ListingDescriptor (echo verbatim) ===
    if (!row.RawDescriptor.empty())
        data.append(row.RawDescriptor.data(), row.RawDescriptor.size());
    else
        data.append(std::vector<uint8>(27, 0).data(), 27);  // minimal all-zero descriptor fallback

    // === trailing bit ===
    data << uint8(0);                               // Unk1916 (bit-as-byte, observed 0)

    // === block sub_7FF7291676F0 @2056 (empty) ===
    data << uint32(0);                              // Blk_f0
    data << uint32(0);                              // Blk_f1
    data << uint32(0);                              // Blk_count == 0

    // === member detail list x MemberCount (sub_7FF7291DBF80 + tail sub_7FF729162CC0) ===
    for (SearchResultMember const& member : row.Members)
        WriteSearchResultMember(data, member);
}

// Emit one full row: header block (sub_7FF7291CCDB0) + body.
static ByteBuffer& operator<<(ByteBuffer& data, SearchResultListing const& row)
{
    data << row.GroupGuid;                          // PackedGuid GroupGuid
    data << uint32(row.ListingId);                  // ListingId (APPLY_TO_GROUP key)
    data << uint32(4);                              // Unk_b   (observed constant 4)
    data << uint64(row.PostTime);                   // PostTime
    data << uint8(0);                               // Unk_hdrbit (bit-as-byte, observed 0)
    WriteSearchResultRowBody(data, row);
    return data;
}

WorldPacket const* LFGListSearchResults::Write()
{
    _worldPacket << uint16(Listings.size());        // Unk32 (duplicate row-count hint; == RowCount in sniffs)
    _worldPacket << uint32(Listings.size());        // RowCount (array length)
    for (SearchResultListing const& row : Listings)
        _worldPacket << row;
    return &_worldPacket;
}

WorldPacket const* LFGListSearchResultsUpdate::Write()
{
    // 68974 capture (bodies idx 16193 len=69 / idx 18313 len=136): the UPDATE row is NOT the full
    // search-result row (the previous writer emitted the full ~285B row — wrong wire). Observed compact row:
    //   PackedGuid GroupGuid, u32 ListingId, u32 4, u64 PostTime, bit(0),
    //   u32 Age (3 / 4), u32 MemberCount (0 / 1),
    //   u8 0, u32 8 (constant in both bodies), u8[26] zero,
    //   MemberDetail x MemberCount (identical 66B record as SEARCH_RESULTS).
    _worldPacket << uint32(Listings.size());
    for (SearchResultListing const& row : Listings)
    {
        _worldPacket << row.GroupGuid;
        _worldPacket << uint32(row.ListingId);
        _worldPacket << uint32(4);                  // header constant (== full-row Unk_b)
        _worldPacket << uint64(row.PostTime);
        _worldPacket << uint8(0);                   // header bit (bit-as-byte, observed 0)
        _worldPacket << uint32(row.Age);            // refresh/age counter (matches the row's Unk40)
        _worldPacket << uint32(row.Members.size());
        _worldPacket << uint8(0);
        _worldPacket << uint32(8);                  // observed constant 8 in both 68974 bodies
        for (uint32 i = 0; i < 26; ++i)
            _worldPacket << uint8(0);               // zero block (semantics unknown, all-zero in both bodies)
        for (SearchResultMember const& member : row.Members)
            WriteSearchResultMember(_worldPacket, member);
    }
    return &_worldPacket;
}

WorldPacket const* LFGListApplicantListUpdate::Write()
{
    // Sniff-exact: Ticket(listing) + u32 count + u32 unk + entries. Entry (status-only form, HasInfo=0):
    // Ticket(application) + PackedGuid player + u32 HasInfo(0) + u8 StateBits + u8 pad.
    _worldPacket << ListingTicket;
    _worldPacket << uint32(Applicants.size());
    _worldPacket << uint32(Unknown);
    for (ApplicantInfo const& a : Applicants)
    {
        _worldPacket << a.Ticket;
        _worldPacket << a.PlayerGuid;
        _worldPacket << uint32(0);          // HasInfo: 0 = status-only entry (full snapshot form documented, unresolved scalars)
        _worldPacket << uint8(a.StateBits);
        _worldPacket << uint8(0);           // pad
    }
    return &_worldPacket;
}

WorldPacket const* LFGListApplicationStatusUpdate::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint64(0);
    _worldPacket << uint32(UnkResult);
    _worldPacket << uint8(RoleGranted);
    _worldPacket << ListingTicket;
    _worldPacket << uint8(StateBits);
    return &_worldPacket;
}

WorldPacket const* LFGListApplyToGroupResult::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint64(ApplicationExpiration);
    _worldPacket << uint8(Status);
    _worldPacket << uint8(0);
    _worldPacket << ListingTicket;
    _worldPacket << uint8(0x10);            // observed constant
    _worldPacket << ListingTicket;
    WriteSearchResultRowBody(_worldPacket, Row);
    return &_worldPacket;
}

WorldPacket const* LFGListUpdateBlacklist::Write()
{
    _worldPacket << uint32(Entries.size());
    for (LFGListBlacklistEntry const& entry : Entries)
    {
        _worldPacket << uint32(entry.ActivityID);
        _worldPacket << uint32(entry.Reason);
    }
    return &_worldPacket;
}
}

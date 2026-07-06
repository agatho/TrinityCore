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
// ActivityID + item-level drive server filtering; the rest are pass-through echo. Reads are guarded against
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
    data >> d.ActivityID;               // uint32 @0x38 (GroupFinderActivity id)
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

void LFGListGetStatus::Read()
{
    _worldPacket >> Ticket;
}

void LFGListSearch::Read()
{
    _worldPacket >> CategoryId >> ActivityGroupId >> Field2 >> Field3 >> Field4 >> Field5;
    for (uint32& f : Filters)
        _worldPacket >> f;
    _worldPacket >> Field6 >> Field7;
    for (uint32& f : Filters2)
        _worldPacket >> f;
    _worldPacket >> SearchGuid;
}

void LFGListApplyToGroup::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> ListingId;
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
    _worldPacket << Ticket;
    _worldPacket << uint8(Status);
    _worldPacket << Listing;
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

static ByteBuffer& operator<<(ByteBuffer& data, SearchResultListing const& row)
{
    data << uint32(row.ListingId);
    data << uint32(row.ActivityID);
    data << uint8(row.Field2) << uint8(row.Field3) << uint8(row.Field4) << uint8(row.Field5);
    data << row.Listing;
    data << row.LeaderGuid;
    data << uint32(row.MemberCount);
    data << uint32(row.Field6);
    data << row.Field7 << row.Field8 << row.Field9 << row.Field10;
    return data;
}

WorldPacket const* LFGListSearchResults::Write()
{
    _worldPacket << uint32(Listings.size());
    for (SearchResultListing const& row : Listings)
        _worldPacket << row;
    return &_worldPacket;
}

WorldPacket const* LFGListSearchResultsUpdate::Write()
{
    _worldPacket << uint32(Listings.size());
    for (SearchResultListing const& row : Listings)
        _worldPacket << row;
    return &_worldPacket;
}

static ByteBuffer& operator<<(ByteBuffer& data, ApplicantInfo const& a)
{
    data << a.ApplicantGuid;
    data << uint32(a.ApplicationId);
    data << uint8(a.State);
    data << uint8(a.RoleMask);
    data << a.PlayerGuid;
    data << uint32(a.SpecID);
    data << uint32(a.ItemLevel);
    data << uint32(a.Field3);
    data << uint8(a.Field4) << uint8(a.Field5);
    for (uint32 f : a.Fields)
        data << uint32(f);
    data << uint8(a.Field6) << uint8(a.Field7);
    data << uint32(a.Field8) << uint32(a.Field9);
    data << SizedString::BitsSize<10>(a.Comment);
    data.FlushBits();
    data << SizedString::Data(a.Comment);
    return data;
}

WorldPacket const* LFGListApplicantListUpdate::Write()
{
    _worldPacket << uint32(ListingId);
    _worldPacket << uint32(Applicants.size());
    for (ApplicantInfo const& a : Applicants)
        _worldPacket << a;
    return &_worldPacket;
}

WorldPacket const* LFGListApplicationStatusUpdate::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint32(ApplicationId);
    _worldPacket << uint8(State);
    _worldPacket << uint8(Field2);
    return &_worldPacket;
}

WorldPacket const* LFGListApplyToGroupResult::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint8(Result) << uint8(Field1) << uint8(Field2);
    _worldPacket << uint32(ListingId);
    _worldPacket << LeaderGuid;
    _worldPacket << Listing;
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

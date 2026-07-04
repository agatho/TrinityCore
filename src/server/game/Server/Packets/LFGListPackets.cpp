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
// The published-listing parameters. Byte-aligned per the extracted deserialize layout; the trailing comment uses
// a bit-length prefix. NEEDS-SNIFF: a few small header fields are bit-packed in the client serializer (2/3-bit
// widths) and the exact comment length-prefix width is not offline-confirmable — verified structurally, values to
// be confirmed by a 12.0.7 capture (see c:\dumps\LFG_LIST_WIRE_68275.md). The activity/item-level fields the server
// filters on are read exactly.
static ByteBuffer& operator>>(ByteBuffer& data, ListingDescriptor& d)
{
    data >> d.ActivityGroupCategory >> d.ActivityGroupId;
    data >> d.PlaystyleCategory >> d.Playstyle;
    data >> d.QuestCategory >> d.QuestId >> d.Field6 >> d.Field7;
    data >> d.SubActivityCategory >> d.SubActivity;
    for (uint8& flag : d.Flags)
        data >> flag;
    data >> d.ActivityID;
    data >> d.RequiredRating;
    data >> d.Field2;
    data >> d.RequiredItemLevel;
    data >> d.RequiredHonorLevel;
    data >> d.Field3;
    data >> d.Field4;
    data >> d.Field5;
    data >> SizedString::BitsSize<10>(d.Comment);
    data.ResetBitPos();
    data >> SizedString::Data(d.Comment);
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
}

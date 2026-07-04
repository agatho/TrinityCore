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

#include "WorldSession.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "LFGListMgr.h"
#include "LFGListPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"

namespace
{
    // Fill the RideTicket that keys a listing on the client (Id = listing id, Requester = leader).
    void FillListingTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Listing const& listing)
    {
        ticket.RequesterGuid = listing.LeaderGuid;
        ticket.Id = listing.Id;
        ticket.Type = WorldPackets::LFG::RideType::Lfg;
        ticket.Time = int32(GameTime::GetGameTime());
        ticket.IsCrossFaction = false;
    }

    // Project a stored listing into the wire snapshot the client echoes in its UI.
    void FillListingInfo(WorldPackets::LFGList::ListingInfo& info, LFGList::Listing const& listing)
    {
        WorldPackets::LFGList::ListingDescriptor const& d = listing.Descriptor;
        info.ActivityID = d.ActivityID;
        info.RequiredItemLevel = d.RequiredItemLevel;
        info.Comment = d.Comment;
        if (Player* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid))
            info.LeaderName = leader->GetName();
    }

    // Build one search-result row for a listing.
    void FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing)
    {
        row.ListingId = listing.Id;
        row.ActivityID = listing.Descriptor.ActivityID;
        row.LeaderGuid = listing.LeaderGuid;
        row.MemberCount = 1;
        if (Group const* group = sGroupMgr->GetGroupByGUID(listing.GroupGuid))
            row.MemberCount = group->GetMembersCount();
        FillListingInfo(row.Listing, listing);
    }
}

// Send the current status of one of the player's listings (or "not listed" when it is gone).
void WorldSession::SendLFGListUpdateStatus(uint32 listingId)
{
    WorldPackets::LFGList::LFGListUpdateStatus packet;
    if (LFGList::Listing const* listing = sLFGListMgr.GetListing(listingId))
    {
        FillListingTicket(packet.Ticket, *listing);
        FillListingInfo(packet.Listing, *listing);
        packet.Listed = true;
    }
    else
    {
        packet.Ticket.Id = listingId;
        packet.Ticket.RequesterGuid = _player ? _player->GetGUID() : ObjectGuid::Empty;
        packet.Listed = false;
    }
    SendPacket(packet.Write());
}

void WorldSession::HandleLFGListJoin(WorldPackets::LFGList::LFGListJoin& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Only a group leader or an ungrouped player may publish a listing.
    if (Group* group = player->GetGroup())
    {
        if (group->GetLeaderGUID() != player->GetGUID())
        {
            WorldPackets::LFGList::LFGListJoinResult result;
            result.Result = 1; // not the leader (exact enum value NEEDS-SNIFF)
            SendPacket(result.Write());
            return;
        }
    }

    uint32 const id = sLFGListMgr.CreateListing(player, packet.Listing);

    WorldPackets::LFGList::LFGListJoinResult result;
    result.Status = 0;
    result.Result = id ? 0 : 1;
    SendPacket(result.Write());

    if (id)
        SendLFGListUpdateStatus(id);
}

void WorldSession::HandleLFGListUpdateRequest(WorldPackets::LFGList::LFGListUpdateRequest& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (sLFGListMgr.UpdateListing(packet.Ticket.Id, player->GetGUID(), packet.Listing))
        SendLFGListUpdateStatus(packet.Ticket.Id);
}

void WorldSession::HandleLFGListLeave(WorldPackets::LFGList::LFGListLeave& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 const listingId = packet.Ticket.Id;
    sLFGListMgr.RemoveListing(listingId, player->GetGUID());

    // Confirm delisting to the client.
    WorldPackets::LFGList::LFGListUpdateStatus status;
    status.Ticket = packet.Ticket;
    status.Listed = false;
    SendPacket(status.Write());
}

void WorldSession::HandleLFGListGetStatus(WorldPackets::LFGList::LFGListGetStatus& packet)
{
    SendLFGListUpdateStatus(packet.Ticket.Id);
}

void WorldSession::HandleLFGListSearch(WorldPackets::LFGList::LFGListSearch& packet)
{
    if (!GetPlayer())
        return;

    std::vector<LFGList::Listing const*> matches = sLFGListMgr.Search(packet.CategoryId, packet.ActivityGroupId, 0);

    WorldPackets::LFGList::LFGListSearchResults results;
    results.Listings.reserve(matches.size());
    for (LFGList::Listing const* listing : matches)
    {
        WorldPackets::LFGList::SearchResultListing row;
        FillSearchRow(row, *listing);
        results.Listings.push_back(std::move(row));
    }
    SendPacket(results.Write());

    // Tell the client the (single-shot) search is complete.
    WorldPackets::LFGList::LFGListSearchStatus status;
    status.Complete = true;
    SendPacket(status.Write());
}

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
#include "DB2Stores.h"
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
        info.RequiredItemLevel = d.OptionalValue1.value_or(0);   // nilable requiredItemLevel (LfgListingCreateData)
        info.Comment = d.Comment;
        if (Player* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid))
            info.LeaderName = leader->GetName();
    }

    // Build one search-result row for a listing.
    // Push the full applicant list of a listing to its (connected) leader.
    void SendApplicantList(LFGList::Listing const& listing)
    {
        Player* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid);
        if (!leader)
            return;

        WorldPackets::LFGList::LFGListApplicantListUpdate packet;
        packet.ListingId = listing.Id;
        for (LFGList::Application const& app : listing.Applications)
        {
            WorldPackets::LFGList::ApplicantInfo info;
            info.ApplicationId = app.Id;
            info.ApplicantGuid = app.ApplicantGuid;
            info.PlayerGuid = app.ApplicantGuid;
            info.RoleMask = app.RoleMask;
            info.State = uint8(app.State);
            info.SpecID = app.SpecID;
            info.ItemLevel = app.ItemLevel;
            info.Comment = app.Comment;
            packet.Applicants.push_back(std::move(info));
        }
        leader->SendDirectMessage(packet.Write());
    }

    // Notify one applicant that the state of its application changed. Application tickets are keyed on the
    // application id throughout, so both directions agree on which application a ticket refers to.
    void SendApplicationStatus(ObjectGuid applicantGuid, uint32 applicationId, LFGList::ApplicationState state)
    {
        Player* applicant = ObjectAccessor::FindConnectedPlayer(applicantGuid);
        if (!applicant)
            return;

        WorldPackets::LFGList::LFGListApplicationStatusUpdate packet;
        packet.Ticket.RequesterGuid = applicantGuid;
        packet.Ticket.Id = applicationId;
        packet.Ticket.Type = WorldPackets::LFG::RideType::Lfg;
        packet.Ticket.Time = int32(GameTime::GetGameTime());
        packet.ApplicationId = applicationId;
        packet.State = uint8(state);
        applicant->SendDirectMessage(packet.Write());
    }
}

// Send the current status of one of the player's listings (or "not listed" when it is gone).
void WorldSession::SendLFGListUpdateStatus(uint32 listingId)
{
    WorldPackets::LFGList::LFGListUpdateStatus packet;
    if (LFGList::Listing const* listing = sLFGListMgr.GetListing(listingId))
    {
        FillListingTicket(packet.Ticket, *listing);
        packet.RawDescriptor = listing->Descriptor.RawBytes;   // echo the client's descriptor verbatim
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

    // Reject listings for an activity the client made up. ActivityID is read from the (sniff-pending) listing
    // descriptor, so a non-zero id that isn't in GroupFinderActivity.db2 is treated as invalid.
    if (packet.Listing.ActivityID && !sGroupFinderActivityStore.LookupEntry(packet.Listing.ActivityID))
    {
        WorldPackets::LFGList::LFGListJoinResult result;
        result.Result = 1; // invalid activity (exact enum value NEEDS-SNIFF)
        SendPacket(result.Write());
        return;
    }

    uint32 const id = sLFGListMgr.CreateListing(player, packet.Listing);
    if (id)
    {
        // A successful create is signalled by UPDATE_STATUS alone - the sniff shows the retail server sends
        // no JOIN_RESULT on success (only UPDATE_STATUS echoing the listing back).
        SendLFGListUpdateStatus(id);
    }
    else
    {
        WorldPackets::LFGList::LFGListJoinResult result;
        result.Result = 1; // create failed (exact enum value NEEDS-SNIFF)
        SendPacket(result.Write());
    }
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

    // Keep this browser subscribed so listings published/edited from now on are pushed live via
    // SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE instead of the player having to re-search.
    sLFGListMgr.RegisterSearch(GetPlayer()->GetGUID(), packet.CategoryId, packet.ActivityGroupId);

    std::vector<LFGList::Listing const*> matches = sLFGListMgr.Search(packet.CategoryId, packet.ActivityGroupId, 0);

    WorldPackets::LFGList::LFGListSearchResults results;
    results.Listings.reserve(matches.size());
    for (LFGList::Listing const* listing : matches)
    {
        WorldPackets::LFGList::SearchResultListing row;
        sLFGListMgr.FillSearchRow(row, *listing);
        results.Listings.push_back(std::move(row));
    }
    SendPacket(results.Write());

    // Tell the client the (single-shot) search is complete.
    WorldPackets::LFGList::LFGListSearchStatus status;
    status.Complete = true;
    SendPacket(status.Write());
}

void WorldSession::HandleLFGListApplyToGroup(WorldPackets::LFGList::LFGListApplyToGroup& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    LFGList::Listing* listing = sLFGListMgr.GetListing(packet.Ticket.Id);
    if (!listing || listing->LeaderGuid == player->GetGUID())
        return;

    LFGList::Application* app = sLFGListMgr.AddApplication(listing->Id, player->GetGUID(), packet.RoleMask,
        uint32(player->GetPrimarySpecialization()), uint32(player->GetAverageItemLevel()), std::string());
    if (!app)
        return;

    // Confirm the application to the applicant.
    WorldPackets::LFGList::LFGListApplyToGroupResult result;
    result.Ticket.RequesterGuid = player->GetGUID();
    result.Ticket.Id = app->Id;
    result.Ticket.Type = WorldPackets::LFG::RideType::Lfg;
    result.Ticket.Time = int32(GameTime::GetGameTime());
    result.Result = 0;
    result.ListingId = listing->Id;
    result.LeaderGuid = listing->LeaderGuid;
    FillListingInfo(result.Listing, *listing);
    SendPacket(result.Write());

    SendApplicationStatus(player->GetGUID(), app->Id, LFGList::ApplicationState::Applied);
    SendApplicantList(*listing);
}

void WorldSession::HandleLFGListCancelApplication(WorldPackets::LFGList::LFGListCancelApplication& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 const applicationId = packet.Ticket.Id;
    LFGList::Application const* app = sLFGListMgr.GetApplication(applicationId);
    if (!app || app->ApplicantGuid != player->GetGUID())
        return;

    LFGList::Listing* listing = sLFGListMgr.GetListingByApplication(applicationId);
    sLFGListMgr.RemoveApplication(applicationId);
    if (listing)
        SendApplicantList(*listing);
}

void WorldSession::HandleLFGListDeclineApplicant(WorldPackets::LFGList::LFGListDeclineApplicant& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    LFGList::Listing* listing = sLFGListMgr.GetListing(packet.Ticket.Id);
    if (!listing || listing->LeaderGuid != player->GetGUID())
        return;

    uint32 const applicationId = packet.ApplicantTicket.Id;
    LFGList::Application const* app = sLFGListMgr.GetApplication(applicationId);
    if (!app)
        return;

    ObjectGuid const applicant = app->ApplicantGuid;
    SendApplicationStatus(applicant, applicationId, LFGList::ApplicationState::Declined);
    sLFGListMgr.RemoveApplication(applicationId);
    SendApplicantList(*listing);
}

void WorldSession::HandleLFGListInviteApplicant(WorldPackets::LFGList::LFGListInviteApplicant& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    LFGList::Listing* listing = sLFGListMgr.GetListing(packet.Ticket.Id);
    if (!listing || listing->LeaderGuid != player->GetGUID())
        return;

    uint32 const applicationId = packet.ApplicantTicket.Id;
    LFGList::Application* app = sLFGListMgr.GetApplication(applicationId);
    if (!app)
        return;

    sLFGListMgr.SetApplicationState(applicationId, LFGList::ApplicationState::Invited);
    SendApplicationStatus(app->ApplicantGuid, applicationId, LFGList::ApplicationState::Invited);
    SendApplicantList(*listing);
}

void WorldSession::HandleLFGListInviteResponse(WorldPackets::LFGList::LFGListInviteResponse& packet)
{
    Player* applicant = GetPlayer();
    if (!applicant)
        return;

    uint32 const applicationId = packet.Ticket.Id;
    LFGList::Application const* app = sLFGListMgr.GetApplication(applicationId);
    LFGList::Listing* listing = sLFGListMgr.GetListingByApplication(applicationId);
    if (!app || !listing || app->ApplicantGuid != applicant->GetGUID())
        return;

    if (!packet.Accept)
    {
        sLFGListMgr.RemoveApplication(applicationId);
        SendApplicantList(*listing);
        return;
    }

    Player* leader = ObjectAccessor::FindConnectedPlayer(listing->LeaderGuid);
    if (!leader)
        return;

    // Join (or form) the leader's party.
    Group* group = leader->GetGroup();
    if (!group)
    {
        group = new Group();
        if (!group->Create(leader))
        {
            delete group;
            return;
        }
        sGroupMgr->AddGroup(group);
        listing->GroupGuid = group->GetGUID();
    }

    if (group->IsFull() || applicant->GetGroup())
        return;

    group->AddMember(applicant);

    sLFGListMgr.RemoveApplication(applicationId);
    SendApplicantList(*listing);
}

void WorldSession::HandleRequestLFGListBlacklist(WorldPackets::LFGList::RequestLFGListBlacklist& /*packet*/)
{
    // The premade-finder blacklist (recently-declined groups the client hides) is not persisted server-side,
    // so a fresh request returns the current empty set. Entries would carry {activityId, reason}; populating
    // them requires a soft-blacklist model that a 12.0.7 sniff should confirm before it is added.
    //
    // Send-site DISABLED (2026-07): SMSG_LFG_LIST_UPDATE_BLACKLIST is currently parked on UNKNOWN_OPCODE
    // because 0x56000E was resolved by 12.0.7 sniff to belong to SMSG_HOUSING_CATALOG_STATE_SYNC. Transmitting
    // this packet would put it on a bogus/colliding opcode, so it is held until a dedicated LFG-list sniff
    // recovers its real wire value. Skipping the send is behaviorally identical to sending an empty blacklist.
    // WorldPackets::LFGList::LFGListUpdateBlacklist packet;
    // SendPacket(packet.Write());
}

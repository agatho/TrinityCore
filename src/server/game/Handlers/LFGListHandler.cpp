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
#include "Config.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "LFGListMgr.h"
#include "LFGListPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"

namespace
{
    // Fill the RideTicket that keys a listing on the client (sniff: type 4, Id = listing id, Time = post time).
    void FillListingTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Listing const& listing)
    {
        ticket.RequesterGuid = listing.LeaderGuid;
        ticket.Id = listing.Id;
        ticket.Type = WorldPackets::LFG::RideType::LfgListListing;
        ticket.Time = int32(listing.CreatedTime);
        ticket.IsCrossFaction = false;
    }

    // Fill an application RideTicket (sniff: type 6, Id = application id, Time = apply time).
    void FillApplicationTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Application const& app)
    {
        ticket.RequesterGuid = app.ApplicantGuid;
        ticket.Id = app.Id;
        ticket.Type = WorldPackets::LFG::RideType::LfgListApplication;
        ticket.Time = int32(app.AppliedTime);
        ticket.IsCrossFaction = false;
    }

    // Wire state bits for an application state (sniff: 0x40 applied, 0x20 invited, 0xA0 accepted).
    uint8 ApplicationStateToBits(LFGList::ApplicationState state)
    {
        switch (state)
        {
            case LFGList::ApplicationState::Applied:  return WorldPackets::LFGList::ApplicationStateBits::Applied;
            case LFGList::ApplicationState::Invited:  return WorldPackets::LFGList::ApplicationStateBits::Invited;
            case LFGList::ApplicationState::Accepted: return WorldPackets::LFGList::ApplicationStateBits::Accepted;
            default:                                  return WorldPackets::LFGList::ApplicationStateBits::Declined;
        }
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
    void FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing)
    {
        row.GroupGuid = !listing.GroupGuid.IsEmpty() ? listing.GroupGuid : listing.LeaderGuid;
        row.ListingId = listing.Id;
        row.PostTime = listing.CreatedTime;
        row.LeaderGuid = listing.LeaderGuid;
        row.RawDescriptor = listing.Descriptor.RawBytes;    // verbatim echo of the client's descriptor bytes

        row.Members.clear();
        if (Group const* group = sGroupMgr->GetGroupByGUID(listing.GroupGuid))
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
                row.Members.push_back(slot.guid);
        if (row.Members.empty())
            row.Members.push_back(listing.LeaderGuid);      // solo listing: the leader is the only member
    }

    // Push the full applicant list of a listing to every connected member of the listed group (sniff: the
    // packet goes to all members, not only the leader; solo listings notify just the leader).
    void SendApplicantList(LFGList::Listing const& listing)
    {
        WorldPackets::LFGList::LFGListApplicantListUpdate packet;
        FillListingTicket(packet.ListingTicket, listing);
        for (LFGList::Application const& app : listing.Applications)
        {
            WorldPackets::LFGList::ApplicantInfo& info = packet.Applicants.emplace_back();
            FillApplicationTicket(info.Ticket, app);
            info.PlayerGuid = app.ApplicantGuid;
            info.StateBits = ApplicationStateToBits(app.State);
        }
        WorldPacket const* data = packet.Write();

        bool leaderNotified = false;
        if (Group const* group = sGroupMgr->GetGroupByGUID(listing.GroupGuid))
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                if (Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid))
                {
                    member->SendDirectMessage(data);
                    leaderNotified = leaderNotified || slot.guid == listing.LeaderGuid;
                }
            }
        }
        if (!leaderNotified)
            if (Player* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid))
                leader->SendDirectMessage(data);
    }

    // Notify one applicant that the state of its application changed (sniff-exact 67/68B layout).
    void SendApplicationStatus(LFGList::Listing const& listing, LFGList::Application const& app)
    {
        Player* applicant = ObjectAccessor::FindConnectedPlayer(app.ApplicantGuid);
        if (!applicant)
            return;

        WorldPackets::LFGList::LFGListApplicationStatusUpdate packet;
        FillApplicationTicket(packet.Ticket, app);
        FillListingTicket(packet.ListingTicket, listing);
        packet.StateBits = ApplicationStateToBits(app.State);
        // Sniff: UnkResult 8 while pending, 60 on invite (possibly the invite-response window in seconds);
        // the granted role echoes the applied role only once invited.
        if (app.State == LFGList::ApplicationState::Invited || app.State == LFGList::ApplicationState::Accepted)
        {
            packet.UnkResult = 60;
            packet.RoleGranted = app.RoleMask;
        }
        else
            packet.UnkResult = 8;
        applicant->SendDirectMessage(packet.Write());
    }
}

// Send the current status of one of the player's listings (or "not listed" when it is gone).
void WorldSession::SendLFGListUpdateStatus(uint32 listingId, uint8 status /*= 0x38*/)
{
    WorldPackets::LFGList::LFGListUpdateStatus packet;
    packet.Status = status;
    if (LFGList::Listing const* listing = sLFGListMgr.GetListing(listingId))
    {
        FillListingTicket(packet.Ticket, *listing);
        packet.ExpirationTime = listing->ExpireTime;
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
        // A successful create is signalled by UPDATE_STATUS alone - no JOIN_RESULT on success. The sniff shows
        // the retail server sends it TWICE with status 0x06 then 0x38.
        SendLFGListUpdateStatus(id, 0x06);
        SendLFGListUpdateStatus(id, 0x38);
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

    // Confirm delisting to the client (sniff: status 0x08, expiration 0, zeroed descriptor).
    WorldPackets::LFGList::LFGListUpdateStatus status;
    status.Ticket = packet.Ticket;
    status.Status = 0x08;
    status.Listed = false;
    SendPacket(status.Write());
}

void WorldSession::HandleLFGListGetStatus(WorldPackets::LFGList::LFGListGetStatus& /*packet*/)
{
    // Empty payload (sniff-verified): the client asks for its own listing status blind; answer from the
    // leader index (0 = not listed).
    LFGList::Listing const* listing = GetPlayer() ? sLFGListMgr.GetListingByLeader(GetPlayer()->GetGUID()) : nullptr;
    SendLFGListUpdateStatus(listing ? listing->Id : 0);
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

void WorldSession::HandleLFGListApplyToGroup(WorldPackets::LFGList::LFGListApplyToGroup& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    LFGList::Listing* listing = sLFGListMgr.GetListing(packet.Ticket.Id);
    if (!listing || listing->LeaderGuid == player->GetGUID())
        return;

    // The client echoes the listing's activity id (sniff-verified field); a mismatch means a stale browse row.
    if (packet.ActivityID && listing->Descriptor.ActivityID && packet.ActivityID != listing->Descriptor.ActivityID)
        return;

    LFGList::Application* app = sLFGListMgr.AddApplication(listing->Id, player->GetGUID(), packet.RoleMask,
        uint32(player->GetPrimarySpecialization()), uint32(player->GetAverageItemLevel()), std::string());
    if (!app)
        return;

    // Confirm the application to the applicant (sniff-exact: app ticket + expiration + listing tickets +
    // the full row snapshot so the client renders the "applied" card without a re-search).
    WorldPackets::LFGList::LFGListApplyToGroupResult result;
    FillApplicationTicket(result.Ticket, *app);
    result.ApplicationExpiration = uint64(app->AppliedTime + sConfigMgr->GetIntDefault("LFGList.ApplicationTimeoutSeconds", 300));
    FillListingTicket(result.ListingTicket, *listing);
    FillSearchRow(result.Row, *listing);
    SendPacket(result.Write());

    SendApplicationStatus(*listing, *app);
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

    LFGList::Application declined = *app;
    declined.State = LFGList::ApplicationState::Declined;
    SendApplicationStatus(*listing, declined);
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
    SendApplicationStatus(*listing, *app);
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

    // Sniff-confirmed retail flow: accepting the invite adds the applicant to the party directly (no
    // SMSG_PARTY_INVITE dialog), the accepted state (0xA0) is echoed, and the joining member receives the
    // listing status (0x19).
    group->AddMember(applicant);

    LFGList::Application accepted = *app;
    accepted.State = LFGList::ApplicationState::Accepted;
    SendApplicationStatus(*listing, accepted);
    SendLFGListUpdateStatus(listing->Id, 0x19);

    sLFGListMgr.RemoveApplication(applicationId);
    SendApplicantList(*listing);
}

void WorldSession::HandleRequestLFGListBlacklist(WorldPackets::LFGList::RequestLFGListBlacklist& /*packet*/)
{
    // The premade-finder blacklist (recently-declined groups the client hides) is not persisted server-side,
    // so a fresh request returns the current empty set. Entries would carry {activityId, reason}; populating
    // them requires a soft-blacklist model that a 12.0.7 sniff should confirm before it is added.
    WorldPackets::LFGList::LFGListUpdateBlacklist packet;
    SendPacket(packet.Write());
}

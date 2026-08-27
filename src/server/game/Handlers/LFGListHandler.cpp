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
    // Both listing tickets are built by LFGListMgr and nowhere else - see the note on the declarations there.
    // These are the shorthands this file used before the two builders moved; they forward, they do not copy.
    void FillListingTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Listing const& listing)
    {
        LFGListMgr::FillListingTicket(ticket, listing);
    }

    void FillApplicationTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Application const& app)
    {
        LFGListMgr::FillApplicationTicket(ticket, app);
    }

    // Fill the RideTicket of a REJECTED listing creation. CMSG_LFG_LIST_JOIN carries a descriptor and no
    // ticket (reader: LFGListJoin::Read), and a rejected create leaves no listing behind, so there is no id
    // to name and nothing to echo - the only thing this ticket can identify is the party that tried. Same
    // requester rule as FillListingTicket: the party being listed, falling back to the leader when solo.
    // Type stays LfgListListing so the client files the answer with the listing flow rather than with an
    // application. UNVERIFIED: what the consumer does with the id. The hook slot of SMSG_LFG_LIST_JOIN_RESULT
    // (dispatcher case 5898241 in 0x755460, slot RVA 0x55FED90) is NULL in the retail image, so no consumer
    // could be decoded; only that the ticket is read at all is established.
    void FillRejectedListingTicket(WorldPackets::LFG::RideTicket& ticket, Player const* player)
    {
        Group const* group = player->GetGroup();
        ticket.RequesterGuid = group ? group->GetGUID() : player->GetGUID();
        ticket.Id = 0;                          // no listing was created
        ticket.Type = WorldPackets::LFG::RideType::LfgListListing;
        ticket.Time = int32(GameTime::GetGameTime());
        ticket.IsCrossFaction = false;
    }

    uint8 ApplicationStateToBits(LFGList::ApplicationState state)
    {
        return LFGListMgr::ApplicationStateToBits(state);
    }

    // NOTE: the search-result row builder lives in LFGListMgr (LFGListMgr::FillSearchRow) so the search reply,
    // the apply-result snapshot and the live SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE push all serialize a listing
    // through the exact same code.

    // Push the full applicant list of a listing to every connected member of the listed group (sniff: the
    // packet goes to all members, not only the leader; solo listings notify just the leader).
    void SendApplicantList(LFGList::Listing const& listing)
    {
        WorldPackets::LFGList::LFGListApplicantListUpdate packet;
        FillListingTicket(packet.ListingTicket, listing);
        for (LFGList::Application const& app : listing.Applications)
            LFGListMgr::FillApplicantInfo(packet.Applicants.emplace_back(), app);
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
        // Sniff: UnkResult 8 while pending, 60 on invite (possibly the invite-response window in seconds).
        // RoleGranted is the role the LEADER assigned in CMSG_LFG_LIST_INVITE_APPLICANT.Invitees[], not the
        // one the applicant asked for - that is why the invite message carries a per-invitee RoleMask at all
        // (client writer RVA 0x6A4A30). Falls back to the applied mask when the invite named no role for this
        // applicant, which is what an invite that simply accepts the application means.
        if (app.State == LFGList::ApplicationState::Invited || app.State == LFGList::ApplicationState::Accepted)
        {
            packet.UnkResult = 60;
            packet.RoleGranted = app.GrantedRoleMask ? app.GrantedRoleMask : app.RoleMask;
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
        packet.Listing = sLFGListMgr.GetPublicDescriptor(*listing);
        packet.Listed = true;
        packet.LeaderGuid = listing->LeaderGuid;    // present in every captured payload, listed or not
    }
    else
    {
        // A gone listing CANNOT be announced from here, and pretending otherwise was a live defect. The ticket
        // that keys the client's stored active entry is only reconstructible from the listing itself
        // (FillListingTicket: party guid, listing id, type 4, post time); assembled from the session's own
        // player guid it matched nothing, and the consumer (RVA 0x24DE410) dropped the delist in silence,
        // leaving the entry standing in the leader's group finder. Delisting is LFGListMgr::DelistAndNotify's
        // job, which sends BEFORE it erases. Nothing left to say here.
        return;
    }
    SendPacket(packet.Write());
}

// SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE (0x5A0022). Tells the owner of a listing that its text was
// flagged. The client only treats the entry as flagged when BOTH the presence bit is set AND the code is
// non-zero (consumer RVA 0x24DEAA0: state = HasCensorCode && CensorCode != 0), so an unflagged listing is
// signalled by omitting the code entirely.
void WorldSession::SendLFGListCensoredActiveEntryUpdate(uint32 listingId)
{
    LFGList::Listing const* listing = sLFGListMgr.GetListing(listingId);
    if (!listing)
        return;

    WorldPackets::LFGList::LFGListCensoredActiveEntryUpdate packet;
    // The censored entry carries the listing WITHOUT its flagged text - the same descriptor every other
    // client sees. That is what forces the edit dialog to compare server-side
    // (C_LFGList.DoesCensoredTextMatch) instead of against a local copy.
    packet.Listing = sLFGListMgr.GetPublicDescriptor(*listing);
    if (listing->IsCensored())
        packet.CensorCode = listing->CensorCode;

    SendPacket(packet.Write());
}

// CMSG_LFG_LIST_CONFIRM_CENSORED_ACTIVE_ENTRY (0x4301AB) - the player chose to keep the flagged listing
// rather than edit it. There is deliberately no reveal counterpart: C_LFGList.RevealCensoredActiveEntry
// and RevealCensoredSearchResult are purely client-side and send nothing.
void WorldSession::HandleLFGListConfirmCensoredActiveEntry(WorldPackets::LFGList::LFGListConfirmCensoredActiveEntry& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!sLFGListMgr.ConfirmCensoredListing(packet.Ticket.Id, player->GetGUID()))
        return;

    // No answer, on purpose. The confirmation is client-local by construction: C_LFGList.ConfirmCensoredActiveEntry
    // (RVA 0x117E560) writes the resolution state 2 itself before sending, and the only opcode that can write
    // that state again is SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE - whose consumer (RVA 0x24DEAA0) stores
    // `HasCensorCode && CensorCode != 0`, a bool that can only produce 0 or 1. Answering the confirmation with
    // that message would therefore push the client from 2 straight back to 1 and re-open the very dialog the
    // player just dismissed. The wire has no way to say "flagged and confirmed"; the state we keep here only
    // stops the server from nagging again (see HandleLFGListGetStatus).
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
            FillRejectedListingTicket(result.Ticket, player);
            result.Result = 1; // not the leader (exact enum value NEEDS-SNIFF)
            SendPacket(result.Write());
            return;
        }
    }

    // Reject listings for an activity the client made up. 68974: the real GroupFinderActivity ids ride in the
    // descriptor's trailing vector (JOIN vec=[1974]); the u32 @0x38 is the GroupFinderCategory id (1 in the
    // capture) and must NOT be looked up in GroupFinderActivity.db2 (the previous code did, wrongly).
    for (uint32 activityId : packet.Listing.ActivityIDs)
    {
        if (activityId && !sGroupFinderActivityStore.LookupEntry(activityId))
        {
            WorldPackets::LFGList::LFGListJoinResult result;
            FillRejectedListingTicket(result.Ticket, player);
            result.Result = 1; // invalid activity (exact enum value NEEDS-SNIFF)
            SendPacket(result.Write());
            return;
        }
    }

    // Retail puts a solo lister into a real party at listing time (sniff: PARTY_UPDATE burst precedes the
    // create UPDATE_STATUS) - applicants later join this group.
    if (!player->GetGroup())
    {
        Group* group = new Group();
        if (group->Create(player))
            sGroupMgr->AddGroup(group);
        else
            delete group;
    }

    uint32 const id = sLFGListMgr.CreateListing(player, packet.Listing);
    if (id)
    {
        // A successful create is signalled by UPDATE_STATUS alone - no JOIN_RESULT on success. The 68974
        // capture (idx 7573-7575) shows the retail server sends it THREE times: status 0x06, then 0x38 twice.
        SendLFGListUpdateStatus(id, 0x06);
        SendLFGListUpdateStatus(id, 0x38);
        SendLFGListUpdateStatus(id, 0x38);

        // A freshly published listing whose text was flagged raises LFG_LIST_CREATE_CENSOR_WARNING on the
        // client; an unflagged one clears any leftover state. Only send when there is something to say.
        if (LFGList::Listing const* listing = sLFGListMgr.GetListing(id))
            if (listing->IsCensored())
                SendLFGListCensoredActiveEntryUpdate(id);
    }
    else
    {
        WorldPackets::LFGList::LFGListJoinResult result;
        FillRejectedListingTicket(result.Ticket, player);
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
    {
        SendLFGListUpdateStatus(packet.Ticket.Id);
        // Editing is how a player clears a flag, so the new state has to go out either way - including the
        // "no longer flagged" case, which is what takes the warning off the client's listing.
        SendLFGListCensoredActiveEntryUpdate(packet.Ticket.Id);
    }
}

void WorldSession::HandleLFGListLeave(WorldPackets::LFGList::LFGListLeave& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Confirm delisting to the client (sniff: status 0x08, expiration 0, zeroed descriptor). Status 0x08 is
    // the no-popup arm of the consumer's switch and that is correct here: the player pressed the button, they
    // do not need to be told why. Through DelistAndNotify like every other delist - echoing the client's own
    // packet.Ticket happened to satisfy the field-by-field comparison, but it also answered requests for
    // listings the player does not own, and it was a fourth place where this ticket got assembled.
    sLFGListMgr.DelistAndNotify(packet.Ticket.Id, player->GetGUID(), 0x08);
}

void WorldSession::HandleLFGListGetStatus(WorldPackets::LFGList::LFGListGetStatus& /*packet*/)
{
    // Empty payload (sniff-verified): the client asks for its own listing status blind. 68974 capture: the
    // unlisted tester's GET_STATUS (idx 2938) received NO response — retail stays silent instead of pushing
    // a "not listed" UPDATE_STATUS, so only answer when a listing actually exists.
    LFGList::Listing const* listing = GetPlayer() ? sLFGListMgr.GetListingByLeader(GetPlayer()->GetGUID()) : nullptr;
    if (!listing)
        return;

    SendLFGListUpdateStatus(listing->Id);

    // The reload path. A UI reload throws away the client's censor state, and LFGList.lua:236-239 relies on
    // the server re-announcing it - without this the player sits on an invisibly flagged listing whose text
    // nobody can see and which no dialog ever mentions again. CMSG_LFG_LIST_GET_STATUS is the request the
    // client makes when the group finder comes back up, so this is the moment to repeat it.
    // Only for a listing the player has NOT dealt with yet. The consumer can write 0 or 1 and nothing else,
    // so repeating the message for a confirmed listing would re-raise the dialog on every reload. A confirmed
    // listing consequently comes back with no warning at all - that is the whole of what this wire can carry,
    // and it is the harmless direction of the two.
    if (listing->IsCensored() && listing->Censor == LFGList::CensorState::Unresolved)
        SendLFGListCensoredActiveEntryUpdate(listing->Id);
}

void WorldSession::HandleLFGListSearch(WorldPackets::LFGList::LFGListSearch& packet)
{
    if (!GetPlayer())
        return;

    std::string const keyword = packet.GetKeyword();

    // Keep this browser subscribed so listings published/edited from now on are pushed live via
    // SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE instead of the player having to re-search. The filters recorded are
    // exactly the ones handed to Search() below, so the push can only carry rows this reply would have carried.
    sLFGListMgr.RegisterSearch(GetPlayer()->GetGUID(), packet.GetCategoryId(), 0, keyword);

    std::vector<LFGList::Listing const*> matches = sLFGListMgr.Search(packet.GetCategoryId(), 0, 0, keyword);

    // 68974 capture: one CMSG_LFG_LIST_SEARCH (idx 8197) is answered by TWO SMSG_LFG_LIST_SEARCH_RESULTS —
    // an empty one first (idx 8215: u16 0 + u32 0) and then the populated one (idx 8224: 2 rows). No
    // SMSG_LFG_LIST_SEARCH_STATUS is sent anywhere in the capture — the previous handler sent one after the
    // results, which retail never does. Mirror retail exactly.
    WorldPackets::LFGList::LFGListSearchResults emptyResults;
    SendPacket(emptyResults.Write());

    WorldPackets::LFGList::LFGListSearchResults results;
    results.Listings.reserve(matches.size());
    for (LFGList::Listing const* listing : matches)
    {
        WorldPackets::LFGList::SearchResultListing row;
        sLFGListMgr.FillSearchRow(row, *listing);
        results.Listings.push_back(std::move(row));
    }
    SendPacket(results.Write());
}

void WorldSession::HandleLFGListApplyToGroup(WorldPackets::LFGList::LFGListApplyToGroup& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    LFGList::Listing* listing = sLFGListMgr.GetListing(packet.Ticket.Id);
    if (!listing || listing->LeaderGuid == player->GetGUID())
        return;

    // The client echoes the listing's descriptor @0x38 value (the category id per the 68974 capture;
    // sniff-verified to match at 68275 where it was 6); a mismatch means a stale browse row.
    if (packet.ActivityID && listing->Descriptor.CategoryID && packet.ActivityID != listing->Descriptor.CategoryID)
        return;

    LFGList::Application* app = sLFGListMgr.AddApplication(listing->Id, player->GetGUID(), packet.RoleMask,
        uint32(player->GetPrimarySpecialization()), uint32(player->GetAverageItemLevel()), packet.Comment);
    if (!app)
        return;

    // Confirm the application to the applicant (sniff-exact: app ticket + expiration + listing tickets +
    // the full row snapshot so the client renders the "applied" card without a re-search).
    WorldPackets::LFGList::LFGListApplyToGroupResult result;
    FillApplicationTicket(result.Ticket, *app);
    result.ApplicationExpiration = uint64(app->AppliedTime + sConfigMgr->GetIntDefault("LFGList.ApplicationTimeoutSeconds", 300));
    FillListingTicket(result.ListingTicket, *listing);
    sLFGListMgr.FillSearchRow(result.Row, *listing);
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

    // The application must belong to THIS leader's listing. Application ids are global, and the leader check above
    // only proves the player owns packet.Ticket's listing - without this a leader could pass their own listing
    // ticket together with an application id from someone else's listing and decline that stranger's applicant.
    if (sLFGListMgr.GetListingByApplication(applicationId) != listing)
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

    // The application must belong to this leader's own listing (see HandleLFGListDeclineApplicant) - otherwise a
    // leader could invite an applicant that applied to a different group's listing.
    if (sLFGListMgr.GetListingByApplication(applicationId) != listing)
        return;

    // Invitees[] is the leader's role assignment, and it is the reason this message carries a list at all:
    // { PackedGuid, u8 RoleMask } per entry (client writer RVA 0x6A4A30). The entry naming the applicant
    // decides SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE.RoleGranted; without it RoleGranted could only ever
    // repeat the role the applicant applied for and the leader's choice would never leave the client.
    // A list can name more than one guid because an applicant may apply as a group - only the entries that
    // match a live application of THIS listing are honoured, the rest are ignored rather than trusted.
    for (WorldPackets::LFGList::LFGListInvitee const& invitee : packet.Invitees)
    {
        for (LFGList::Application const& candidate : listing->Applications)
            if (candidate.ApplicantGuid == invitee.Guid)
                sLFGListMgr.SetApplicationGrantedRole(candidate.Id, invitee.RoleMask);
    }

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

    // Accept is only valid for an application the leader actually invited. Without this an applicant could send
    // CMSG_LFG_LIST_INVITE_RESPONSE{Accept} for its own still-pending (Applied) application and force-join a group
    // that never invited it.
    if (app->State != LFGList::ApplicationState::Invited)
        return;

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

    // Retail auto-delists when the group reaches the activity's player cap. Through DelistAndNotify, which
    // builds the ticket from the still-live listing - the previous order (remove, then announce) produced a
    // ticket from the leader's own player guid with Type 0 and Time 0, which the client discarded.
    // Status 0x2C, not 0x08: the consumer's switch (RVA 0x24DE410) maps 0x2C to the Lua event
    // LFG_LIST_ENTRY_EXPIRED_TOO_MANY_PLAYERS (hash 0xFF802F6F0E65A26D), which is exactly this reason.
    // 0x08 falls into the default arm and raises no popup, so the leader was never told why the entry went.
    if (group->IsFull())
        sLFGListMgr.DelistAndNotify(listing->Id, listing->LeaderGuid, 0x2C);
}

namespace
{
    // SMSG_LFG_LIST_UPDATE_BLACKLIST content, extracted verbatim from the 12.0.7.68974 capture
    // (dump_12.0.7.68974_2026-08-07_21-54-14.pkt): all three CMSG_REQUEST_LFG_LIST_BLACKLIST in the session
    // received the identical 3652-byte body - u32 count (456) + 456 x {u32 GroupFinderActivity id, u32 reason}.
    // Reason codes observed: 2 (x23), 3 (x22), 10 (x12), 18 (x143), 19 (x256) - exact semantics unresolved
    // (activity not listable: legacy/expansion/condition-gated). Retail answers EVERY blacklist request with
    // this table; serving the sniffed table verbatim reproduces the retail wire for this build.
    struct LFGListBlacklistRow { uint16 ActivityID; uint8 Reason; };
    constexpr LFGListBlacklistRow LFGListActivityBlacklist[] =
    {
        {14,3}, {15,3}, {22,19}, {25,19}, {30,19}, {33,19}, {132,19}, {143,19}, {179,2}, {180,19},
        {181,2}, {183,19}, {184,19}, {186,2}, {351,3}, {352,3}, {353,3}, {354,3}, {355,3}, {360,3},
        {363,2}, {364,2}, {389,3}, {390,3}, {391,3}, {392,3}, {393,3}, {731,3}, {733,3}, {394,3},
        {395,2}, {396,2}, {401,19}, {402,19}, {405,19}, {397,2}, {398,2}, {417,2}, {418,2}, {430,10},
        {460,19}, {433,2}, {434,2}, {435,19}, {436,19}, {438,19}, {440,19}, {441,19}, {458,2}, {459,19},
        {443,19}, {472,19}, {473,19}, {476,19}, {490,3}, {491,3}, {461,19}, {462,19}, {463,19}, {464,19},
        {465,19}, {466,19}, {467,19}, {470,19}, {471,19}, {530,19}, {532,2}, {533,10}, {534,19}, {535,19},
        {536,10}, {537,10}, {538,10}, {539,10}, {497,2}, {498,2}, {501,10}, {502,19}, {503,10}, {504,19},
        {506,10}, {507,19}, {509,10}, {510,19}, {512,2}, {513,10}, {514,10}, {518,19}, {522,19}, {526,19},
        {644,19}, {653,3}, {654,3}, {655,3}, {657,2}, {669,2}, {682,19}, {679,19}, {658,19}, {659,19},
        {661,19}, {662,3}, {673,19}, {683,19}, {684,19}, {724,19}, {725,19}, {726,19}, {727,19}, {728,19},
        {729,19}, {730,19}, {691,19}, {695,19}, {735,19}, {736,19}, {737,19}, {701,19}, {702,19}, {703,19},
        {709,19}, {723,19}, {713,19}, {715,19}, {717,19}, {699,19}, {705,19}, {714,19}, {1016,19}, {1017,19},
        {1025,19}, {1026,19}, {1027,19}, {1028,19}, {1029,19}, {1030,19}, {1031,19}, {1032,19}, {1033,19}, {1034,19},
        {940,19}, {941,19}, {942,19}, {943,19}, {944,19}, {945,19}, {946,19}, {947,19}, {948,19}, {949,19},
        {950,19}, {951,19}, {952,19}, {953,19}, {970,19}, {955,19}, {956,19}, {957,19}, {958,19}, {959,19},
        {960,19}, {961,19}, {962,19}, {963,19}, {964,19}, {965,19}, {966,19}, {967,19}, {968,19}, {969,19},
        {971,19}, {972,19}, {973,19}, {974,19}, {975,19}, {976,19}, {977,19}, {978,19}, {979,19}, {980,19},
        {981,19}, {982,19}, {983,19}, {984,19}, {985,19}, {986,19}, {987,19}, {988,19}, {989,19}, {990,19},
        {991,19}, {992,19}, {993,19}, {994,19}, {995,19}, {996,19}, {997,19}, {998,19}, {999,19}, {1000,19},
        {1001,19}, {1002,19}, {1003,19}, {1004,19}, {1005,19}, {1006,19}, {1007,19}, {1008,19}, {1009,19}, {1010,19},
        {1011,19}, {1012,19}, {1013,19}, {1014,19}, {1015,19}, {1024,19}, {1192,19}, {1193,19}, {1164,19}, {1168,19},
        {1172,19}, {1035,19}, {1036,19}, {1037,19}, {1038,19}, {1039,19}, {1040,19}, {1041,19}, {1042,19}, {1043,19},
        {1044,19}, {1045,19}, {1046,19}, {1047,19}, {1048,19}, {1049,19}, {1050,19}, {1051,19}, {1052,19}, {1053,19},
        {1054,19}, {1055,19}, {1056,19}, {1057,19}, {1194,19}, {1146,19}, {1176,19}, {1180,19}, {1184,19}, {1188,19},
        {1247,19}, {1248,19}, {1274,19}, {1281,19}, {1282,19}, {1283,19}, {1284,19}, {1285,19}, {1286,19}, {1287,19},
        {1288,19}, {1290,19}, {1311,18}, {1312,18}, {1313,18}, {1314,18}, {1315,18}, {1316,18}, {1289,2}, {1294,19},
        {1317,18}, {1318,18}, {1319,18}, {1320,18}, {1321,18}, {1322,18}, {1195,19}, {1370,18}, {1371,18}, {1372,18},
        {1373,18}, {1374,18}, {1375,18}, {1376,18}, {1377,18}, {1378,18}, {1323,18}, {1324,18}, {1325,18}, {1326,18},
        {1327,18}, {1328,18}, {1329,18}, {1330,18}, {1331,18}, {1332,18}, {1333,18}, {1334,18}, {1335,18}, {1336,18},
        {1337,18}, {1338,18}, {1339,18}, {1340,18}, {1341,18}, {1342,18}, {1343,18}, {1344,18}, {1345,18}, {1346,18},
        {1347,18}, {1348,18}, {1349,18}, {1350,18}, {1351,18}, {1356,18}, {1353,18}, {1354,18}, {1355,18}, {1357,18},
        {1358,18}, {1359,18}, {1360,18}, {1361,18}, {1362,18}, {1363,18}, {1364,18}, {1365,18}, {1366,18}, {1367,18},
        {1368,18}, {1369,18}, {1550,19}, {1616,19}, {1694,19}, {1695,19}, {1702,19}, {1700,19}, {1620,18}, {1621,18},
        {1622,18}, {1623,18}, {1624,18}, {1625,18}, {1626,18}, {1627,18}, {1628,18}, {1629,18}, {1630,18}, {1631,18},
        {1632,18}, {1633,18}, {1634,18}, {1635,18}, {1636,18}, {1637,18}, {1638,18}, {1639,18}, {1640,18}, {1641,18},
        {1642,18}, {1643,18}, {1644,18}, {1645,18}, {1646,18}, {1647,18}, {1648,18}, {1649,18}, {1650,18}, {1651,18},
        {1652,18}, {1653,18}, {1654,18}, {1655,18}, {1656,18}, {1657,18}, {1658,18}, {1659,18}, {1660,18}, {1661,18},
        {1662,18}, {1663,18}, {1664,18}, {1665,18}, {1666,18}, {1667,18}, {1668,18}, {1669,18}, {1670,18}, {1671,18},
        {1672,18}, {1673,18}, {1674,18}, {1675,18}, {1676,18}, {1677,18}, {1678,18}, {1679,18}, {1680,18}, {1681,18},
        {1682,18}, {1683,18}, {1701,19}, {1711,2}, {1551,19}, {1552,19}, {1750,19}, {1751,19}, {1755,19}, {1756,19},
        {1722,19}, {1723,19}, {1867,19}, {1868,19}, {1869,19}, {1878,19}, {1879,19}, {1880,19}, {1782,18}, {1783,18},
        {1785,18}, {1787,18}, {1788,18}, {1789,18}, {1790,18}, {1791,18}, {1793,18}, {1794,18}, {1795,18}, {1813,19},
        {1814,19}, {1815,19}, {1825,19}, {1834,19}, {1835,19}, {1836,19}, {1845,19}, {1846,19}, {1847,19}, {1856,19},
        {1857,19}, {1858,19}, {1900,19}, {1901,19}, {1902,19}, {1936,2}, {1911,19}, {1912,19}, {1913,19}, {1922,19},
        {1923,19}, {1924,19}, {1945,18}, {1889,19}, {1890,19}, {1891,19}
    };
}

void WorldSession::HandleRequestLFGListBlacklist(WorldPackets::LFGList::RequestLFGListBlacklist& /*packet*/)
{
    WorldPackets::LFGList::LFGListUpdateBlacklist packet;
    packet.Entries.reserve(std::size(LFGListActivityBlacklist));
    for (LFGListBlacklistRow const& row : LFGListActivityBlacklist)
    {
        WorldPackets::LFGList::LFGListBlacklistEntry& entry = packet.Entries.emplace_back();
        entry.ActivityID = row.ActivityID;
        entry.Reason = row.Reason;
    }
    SendPacket(packet.Write());
}

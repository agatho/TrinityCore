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
#include "Log.h"
#include "LFGListPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include <algorithm>
#include <unordered_set>

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

    // The one rejection answer both descriptor-writing paths owe the client, and the reason this is a
    // function: CMSG_LFG_LIST_JOIN and CMSG_LFG_LIST_UPDATE_REQUEST are the same dialog with the same two
    // outcomes, and the client waits for exactly one of two events after either of them -
    // LFG_LIST_ACTIVE_ENTRY_UPDATE (from SMSG_LFG_LIST_UPDATE_STATUS) or LFG_LIST_ENTRY_CREATION_FAILED
    // (from SMSG_LFG_LIST_JOIN_RESULT, see LFGListJoinResult in LFGListPackets.h). Returning without
    // sending either one leaves the dialog standing. Duplicating the send at each call site is what let the
    // edit path fall silent while the create path answered all three of its rejections.
    // `echo` is the ticket the client sent, where it sent one. UNVERIFIED: that echoing is what retail does -
    // the hook slot of SMSG_LFG_LIST_JOIN_RESULT is NULL, so its consumer could not be decoded. It is the
    // safer of the two options rather than a measurement: the one consumer of this family whose ticket
    // handling IS decoded (SMSG_LFG_LIST_UPDATE_STATUS, RVA 0x24DE410) compares the incoming ticket against
    // its stored active entry field for field and silently drops a mismatch, so on a rejection that names an
    // existing listing an echo can match where a zeroed ticket cannot. Where the client sent no ticket
    // (CMSG_LFG_LIST_JOIN), FillRejectedListingTicket applies and there is nothing to echo.
    void SendListingRejected(WorldSession* session, Player const* player, uint8 result,
        WorldPackets::LFG::RideTicket const* echo = nullptr)
    {
        WorldPackets::LFGList::LFGListJoinResult packet;
        if (echo)
            packet.Ticket = *echo;
        else
            FillRejectedListingTicket(packet.Ticket, player);
        packet.Result = result;
        session->SendPacket(packet.Write());
    }

    // Reject a descriptor naming an activity the client made up. 68974: the real GroupFinderActivity ids
    // ride in the descriptor's trailing vector (JOIN vec=[1974]); the u32 @0x38 is the GroupFinderCategory
    // id (1 in the capture) and must NOT be looked up in GroupFinderActivity.db2 (the previous code did,
    // wrongly). Shared by create and edit: the ids set here are what every open browser is served
    // (LFGListMgr::NotifyListingChanged -> FillSearchRow) and the comparison value HandleLFGListApplyToGroup
    // checks an application against, so a check on only one of the two writers is a check that is bypassed
    // in one step - publish valid, then edit.
    bool ValidateListingActivities(WorldSession* session, Player const* player,
        WorldPackets::LFGList::ListingDescriptor const& descriptor, WorldPackets::LFG::RideTicket const* echo = nullptr)
    {
        for (uint32 activityId : descriptor.ActivityIDs)
        {
            if (activityId && !sGroupFinderActivityStore.LookupEntry(activityId))
            {
                // UNVERIFIED: the value 1, the same guess the other rejections make - see
                // LFGListJoinResult::Result for why no consumer could settle the enum.
                SendListingRejected(session, player, 1, echo); // invalid activity
                return false;
            }
        }

        return true;
    }

    // NOTE: the search-result row builder lives in LFGListMgr (LFGListMgr::FillSearchRow) so the search reply,
    // the apply-result snapshot and the live SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE push all serialize a listing
    // through the exact same code.

    // Moved to LFGListMgr so that the logout cleanup and the application-timeout sweep - which live there -
    // reach the same recipients as the handler paths. See the contract on the declaration.
    void SendApplicantList(LFGList::Listing const& listing)
    {
        LFGListMgr::SendApplicantList(listing);
    }

    // Answer a REFUSED application.
    //
    // HandleLFGListApplyToGroup can refuse for four reasons, and until this was written every one of them
    // returned without sending anything: the player pressed "Apply" and nothing happened - no packet, no
    // error text, no log line. That is not a corner case. The most common of the four is the configured cap
    // (LFGList.MaxApplicationsPerPlayer, default 5): the sixth application is refused by
    // LFGListMgr::AddApplication, which is a decision the server makes deliberately and then used to keep
    // to itself.
    //
    // The wire has the answer and the client decodes it - the full trail is at the declaration of
    // LFGListApplyToGroupResult in LFGListPackets.h. In short: the consumer @ RVA 0x24DED40 hands the tail
    // to the state setter @ RVA 0x24DD190, whose only failure arm is
    //     else if (state == 3 && previous != 3) return present(reasonCode);
    // so state 3 ("failed", state->string mapper @ RVA 0x24DADA0) is the one state that puts a reason in
    // front of the player, and the reason is the FIRST u8 of the tail - the field named Status here. The
    // presenter @ RVA 0x24E25E0 looks it up in a closed table, so only the codes listed in
    // ApplicationFailureReason arrive anywhere; anything else is silently dropped by the client and would
    // leave us exactly where we started.
    //
    // ListingTicket is the ticket the CLIENT sent, echoed back byte for byte, and that is deliberate. The
    // consumer resolves the record by that ticket (lookup @ RVA 0x24E38D0, comparing guid, Id, Type and
    // Time), so echoing is the only construction guaranteed to name the record the client actually built
    // from its browse row - including on the one path where the listing is gone and the server has nothing
    // left to build a ticket from.
    //
    // UNVERIFIED: which reason code retail pairs with which refusal. The codes themselves and their texts
    // are measured (table @ RVA 0x44DD860, GameError keys resolved through the table @ RVA 0x43D55C0), and
    // no 12.1 or 12.0.7 capture of a refused application exists in c:\dumps\wpp_work\lfg_ref, so the
    // pairing below is chosen by matching the ERR_ text to the situation. A recording of a refusal would
    // settle it; see aufnahme_noetig in the status file.
    void SendApplyRefusal(WorldSession* session, Player const* player,
        WorldPackets::LFG::RideTicket const& listingTicket, LFGList::Listing const* listing,
        uint8 reason, uint8 stateBits = WorldPackets::LFGList::ApplicationStateBits::Failed)
    {
        WorldPackets::LFGList::LFGListApplyToGroupResult result;

        // No application was created, so the application ticket can only name the party that tried and
        // carry id 0 - the same rule FillRejectedListingTicket follows for a rejected listing, with the
        // type that belongs to an application.
        result.Ticket.RequesterGuid = player->GetGUID();
        result.Ticket.Id = 0;
        result.Ticket.Type = WorldPackets::LFG::RideType::LfgListApplication;
        result.Ticket.Time = int32(GameTime::GetGameTime());
        result.Ticket.IsCrossFaction = false;

        result.ListingTicket = listingTicket;
        result.ApplicationExpiration = 0;               // nothing is pending, so nothing expires
        result.Status = reason;
        result.RoleGranted = 0;
        result.StateBits = stateBits;

        if (listing)
            sLFGListMgr.FillSearchRow(result.Row, *listing);
        else
        {
            // The listing is gone, so there is no row to send. The consumer assigns whatever row arrives
            // into the client's record, which means the browse entry loses its name and members until the
            // next search - and that is the truth about a listing that no longer exists. The header is
            // still filled from the echoed ticket so that the create branch, if it runs, keys the record on
            // the right listing id instead of on 0.
            result.Row.GroupGuid = listingTicket.RequesterGuid;
            result.Row.ListingId = listingTicket.Id;
            result.Row.PostTime = uint64(time_t(listingTicket.Time));
        }

        TC_LOG_DEBUG("lfg.list", "Refused {} application to listing {}: reason {}, state bits 0x{:02X}",
            player->GetGUID().ToString(), listingTicket.Id, reason, stateBits);

        session->SendPacket(result.Write());
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
        // The applicant's deadline, restated on every status change. It is not decoration: the consumer
        // @ RVA 0x24DE9A0 passes it to the state setter @ 0x24DD190 as `wire - timebase` and the setter
        // stores it at applicant record +2248 unconditionally - the SAME slot the apply reply filled
        // through SMSG_LFG_LIST_APPLY_TO_GROUP_RESULT.ApplicationExpiration. Leaving it at 0 therefore did
        // not mean "unchanged", it overwrote the fresh deadline with 0 - timebase one packet later.
        packet.ApplicationExpiration = LFGListMgr::GetApplicationExpiration(app);
        packet.StateBits = ApplicationStateToBits(app.State);
        // UNVERIFIED: UnkResult 8 while pending / 60 on invite is copied from the 12.0.7.68974 capture; the
        // reading "invite-response window in seconds" is a guess, see the field's note in LFGListPackets.h.
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

    // The same message with the wire state given directly instead of derived from Application::State.
    //
    // It exists because the accept half of the invite flow can end in states the SERVER-side enum does not
    // have a member for: the party filled up between invite and accept, the leader logged out, the join
    // itself failed. Application::State has six values and none of them says "declined because full", but
    // the CLIENT has that state - nibble 6 "declined_full" of the state->string mapper @ RVA 0x24DADA0 -
    // and the UI reacts to it (LFGList.lua:1962-1968 groups the three decline strings). Without this, every
    // one of those endings was a bare `return`: the applicant pressed Accept and received nothing at all.
    // Same defect class as the four silent refusals in HandleLFGListApplyToGroup, on the sister handler.
    //
    // The reason field is a required argument and is forwarded verbatim - see the contract on
    // LFGListMgr::SendApplicationStatusBits. In short: the client presents it ONLY on the failed state, and
    // only if it is one of ApplicationFailureReason's table values. Runde 11 gave these endings a state but
    // left the reason at the pending filler 8, which is not in that table - so the four endings below still
    // put no text in front of the player. Naming the reason at every call site is what closes that.
    // Lives in LFGListMgr for the same reason SendApplicantList does: the delist path needs it too.
    void SendApplicationStatusBits(LFGList::Listing const& listing, LFGList::Application const& app,
        uint8 stateBits, uint8 failureReason)
    {
        LFGListMgr::SendApplicationStatusBits(listing, app, stateBits, failureReason);
    }
}

// Send the current status of one of the player's listings (or "not listed" when it is gone).
// UNVERIFIED: the default status 0x38. Its only provenance is 12.0.7.68974 idx 7574/7575, where it is what
// retail repeated for a listing that already existed - which is what the two callers that take the default do
// (an edit and the client's own status re-request), so it is the right shape of value for them. No 12.1
// capture contains an UPDATE_STATUS for an edit or a re-request, so the value itself is unconfirmed for the
// target build. It cannot mislead the client either way: the consumer at RVA 0x24DE410 only branches on the
// status when Listed == 0, and 0x38 sits in the no-popup arm of that switch alongside 0x06 and 0x08.
void WorldSession::SendLFGListUpdateStatus(uint32 listingId, uint8 status /*= 0x38*/)
{
    WorldPackets::LFGList::LFGListUpdateStatus packet;
    packet.Status = status;
    if (LFGList::Listing* listing = sLFGListMgr.GetListing(listingId))
    {
        FillListingTicket(packet.Ticket, *listing);
        packet.ExpirationTime = listing->ExpireTime;
        packet.Listing = sLFGListMgr.GetPublicDescriptor(*listing);
        packet.Listed = true;
        packet.LeaderGuid = listing->LeaderGuid;    // present in every captured payload, listed or not
        // This session's client now holds an active entry for the listing (consumer RVA 0x24DE410 creates
        // it from any Listed payload), so it has to be told when the listing goes. This is the ONLY place a
        // Listed payload is produced, which is what makes the bookkeeping complete; DelistAndNotify reads
        // the set back. Recording the receiver rather than the leader matters for exactly one caller -
        // HandleLFGListInviteResponse, which sends status 0x19 on the joining APPLICANT's session.
        if (Player const* receiver = GetPlayer())
            listing->StatusRecipients.insert(receiver->GetGUID());
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
    // IsCensored(), not IsTextWithheld(): this is the wire code, which says "flagged", while the withholding
    // of the text stops at the player's confirmation. The two only differ for a CONFIRMED listing, and no
    // caller can reach this function with one - creation and edit both run EvaluateCensorship first, which
    // can only land on None or Unresolved, and the reload path below tests Unresolved explicitly. That
    // matters, because sending this message for a confirmed listing would push the client's censor state
    // from 2 back to 1 and re-open the dialog the player just dismissed.
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

    // No CENSOR message in reply, on purpose - and ONLY that one. The censor state itself is client-local by
    // construction: C_LFGList.ConfirmCensoredActiveEntry (RVA 0x117E560) writes the resolution state 2 itself
    // before sending, and the only opcode that can write that state again is
    // SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE - whose consumer (RVA 0x24DEAA0) stores
    // `HasCensorCode && CensorCode != 0`, a bool that can only produce 0 or 1. Answering the confirmation with
    // that message would therefore push the client from 2 straight back to 1 and re-open the very dialog the
    // player just dismissed. The wire has no way to say "flagged and confirmed"; the state we keep here only
    // stops the server from nagging again (see HandleLFGListGetStatus).
    //
    // The DESCRIPTOR is a different matter, and leaving it unsent was a live defect. Confirmation is the third
    // mutation of the publicly visible descriptor after CreateListing and UpdateListing, and it was the only
    // one without a push. IsTextWithheld() goes false here, so from this line on GetPublicDescriptor stops
    // clearing Name and Comment and MatchesKeywords stops returning a hard false for this listing - the two
    // readers of that state. Both of the messages that carry a descriptor are unaffected by the censor
    // restriction above, because neither touches the client's censor state:
    //
    //   * the OWNER. His client set its own resolution state to 2 on send, so CENSORED_LFG_GROUP_NAME is gone
    //     from his header - but the last descriptor he actually received was the withheld one, with Name and
    //     Comment empty. Without this send he sits on a BLANK title until something else makes him ask
    //     (CMSG_LFG_LIST_GET_STATUS on a UI reload, HandleLFGListGetStatus). That is verbatim the damage the
    //     reload path was fixed for; this is the same damage on the confirmation path itself. Status 0x38 is
    //     the "listing you already know, nothing to pop up" value the edit path uses, which is what this is.
    //   * THIRD PARTIES with the browser open. Their subscribed rows still carry the withheld descriptor and
    //     would keep it until their next own search. NotifyListingChanged re-pushes the row through the same
    //     Matches() test Search() uses, so the listing also (re)appears for searchers whose keyword filter
    //     MatchesKeywords was rejecting while the text was withheld.
    SendLFGListUpdateStatus(packet.Ticket.Id);
    sLFGListMgr.NotifyListingChanged(packet.Ticket.Id);
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
            // UNVERIFIED: the value 1. "Rejected" is certain, the enum member is guessed - see
            // LFGListJoinResult::Result for why no consumer could settle it.
            SendListingRejected(this, player, 1); // not the leader
            return;
        }
    }

    if (!ValidateListingActivities(this, player, packet.Listing))
        return;

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
        // A successful create is signalled by UPDATE_STATUS alone - no JOIN_RESULT on success - and by
        // EXACTLY ONE message, status 0x06.
        //
        // Measured on the target wire, not inferred: C:/dumps/wpp_work/in/s69273_a.pkt (12.1.0.69273) holds
        // every SMSG_LFG_LIST_UPDATE_STATUS of the entire session in THREE packets, and they are three events
        // over TWO listings, not three sends of one create:
        //   #121376  81 B  status 0x06  party guid ..51690AC9  id 2657  post time 0x6A7CF6EE  -> create
        //   #124344  74 B  status 0x08  SAME guid, SAME id, SAME post time                    -> delist
        //   #159772  81 B  status 0x06  party guid ..5169132A  id 2709  post time 0x6A7CF8C7  -> create
        // Two creates, one message each. Between the create and the delist of listing 2657 there is no
        // further UPDATE_STATUS at all - the opcode occurs three times in the whole capture and zero times in
        // the other ten 12.1 sniffs. The create at #121376 answers CMSG_LFG_LIST_JOIN at #121344, 638 ms
        // earlier, with the party-creation burst (4 x SMSG_PARTY_UPDATE, #121368-121371) in between.
        //
        // The triple send this replaces was a correct reading of the WRONG BUILD. 12.0.7.68974 really does
        // send three (idx 7573-7575 of dump_12.0.7.68974_2026-08-07_21-54-14.pkt: one tick 151085, one
        // connection 0, one ticket, payloads byte-identical apart from the status, 0x06 then 0x38 then 0x38).
        // That opcode carried the value 0x56000A in 12.0.7 and carries 0x5A000A here, which is how a 12.0.7
        // observation kept its authority while 12.1 bytes for the same message sat in the reference set.
        SendLFGListUpdateStatus(id, 0x06);

        // A freshly published listing whose text was flagged raises LFG_LIST_CREATE_CENSOR_WARNING on the
        // client; an unflagged one clears any leftover state. Only send when there is something to say.
        if (LFGList::Listing const* listing = sLFGListMgr.GetListing(id))
            if (listing->IsCensored())
                SendLFGListCensoredActiveEntryUpdate(id);
    }
    else
    {
        // UNVERIFIED: the value 1, same guess as above (LFGListJoinResult::Result).
        SendListingRejected(this, player, 1); // create failed
    }
}

void WorldSession::HandleLFGListUpdateRequest(WorldPackets::LFGList::LFGListUpdateRequest& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Same check as the create path, and it has to be here too: LFGListMgr::UpdateListing overwrites the
    // descriptor wholesale (listing->Descriptor = descriptor), so without this an edit could write any
    // uint32 as a GroupFinderActivity into a listing that was published with a valid one.
    if (!ValidateListingActivities(this, player, packet.Listing, &packet.Ticket))
        return;

    if (sLFGListMgr.UpdateListing(packet.Ticket.Id, player->GetGUID(), packet.Listing))
    {
        SendLFGListUpdateStatus(packet.Ticket.Id);
        // Editing is how a player clears a flag, so the new state has to go out either way - including the
        // "no longer flagged" case, which is what takes the warning off the client's listing.
        SendLFGListCensoredActiveEntryUpdate(packet.Ticket.Id);
    }
    else
    {
        // UpdateListing has exactly two rejections (LFGListMgr::UpdateListing): the listing is gone, or the
        // sender is not its leader. "Gone" is reachable in normal play - the expiry sweep in
        // LFGListMgr::Update delists after LFGList.ListingExpiryMinutes (default 30) while the edit dialog
        // is still open - so this is not a can't-happen branch. Silence here left the player who pressed
        // "Update" with a dialog that never resolved: the client waits for LFG_LIST_ACTIVE_ENTRY_UPDATE or
        // LFG_LIST_ENTRY_CREATION_FAILED and gets neither.
        // UNVERIFIED: the value 1, the same guess the create path's rejections make - see
        // LFGListJoinResult::Result. What is certain is that an answer has to go out.
        SendListingRejected(this, player, 1, &packet.Ticket); // update failed
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
    // and it is the harmless direction of the two, but only because the UPDATE_STATUS just sent above now
    // carries that listing's real name and comment again (GetPublicDescriptor withholds text while the flag
    // is unresolved, not after the confirmation). Silence plus withheld text would have left the owner with
    // a blank title and nothing to explain it.
    if (listing->IsTextWithheld())
        SendLFGListCensoredActiveEntryUpdate(listing->Id);
}

void WorldSession::HandleLFGListSearch(WorldPackets::LFGList::LFGListSearch& packet)
{
    if (!GetPlayer())
        return;

    // Every filter the message carries that this server can act on, in one object. What each field means
    // and where that meaning comes from is documented on WorldPackets::LFGList::LFGListSearch; the three
    // activity lists used to be read into Values1/Values2/Values3 and thrown away, which made the search
    // ignore the only part of the query the client had already done the work for.
    //
    // ALL the terms the client sent, not just the first one. See LFGListSearch::GetKeywords for the shape
    // and for the AND/OR decision; the previous single-keyword form reduced the two-block reference payload
    // ("the", "nexus-captain") to a search for "the".
    LFGList::SearchFilter filter;
    filter.CategoryId = packet.GetCategoryId();
    filter.ResolvedActivityIds = packet.ResolvedActivityIDs;
    filter.ActivityIds = packet.ActivityIDs;
    filter.ActivityGroupIds = packet.ActivityGroupIDs;
    filter.Keywords = packet.GetKeywords();
    // Deliberately NOT acted on, each for a reason recorded at its declaration: Filter (already consumed
    // client-side before ResolvedActivityIDs was built), PreferredFilters and FilterByte2 (meaning
    // undecided), LanguageMask (no per-listing locale exists here), AdvancedFilterMask and MinimumRating
    // (the listing model carries neither role slots nor a per-activity difficulty), CrossFaction (nothing
    // to relax - every listing is already visible to both factions), FilterByte1 (a client constant) and
    // Guids (no client path fills it).

    // Keep this browser subscribed so listings published/edited from now on are pushed live via
    // SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE instead of the player having to re-search. It records the SAME
    // filter object the reply below is built from, and both run it through LFGListMgr::Matches, so the push
    // can only ever carry rows this reply would have carried.
    sLFGListMgr.RegisterSearch(GetPlayer()->GetGUID(), filter);

    std::vector<LFGList::Listing const*> matches = sLFGListMgr.Search(filter);

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
    if (!listing)
    {
        // The listing is gone - delisted, expired, or filled - while the browse row was still on screen.
        // This is the likeliest of the four refusals in practice, because an open browser is not refreshed
        // when a listing disappears (NotifyListingChanged runs on create and edit only).
        // "declined_delisted" rather than "failed": nibble 7 is a state of its own in the client's
        // vocabulary (mapper @ RVA 0x24DADA0) and the UI has a message for exactly this - LFGList.lua:388
        // maps it to LFG_LIST_APP_DECLINED_DELISTED_MESSAGE and :243 counts it as a decline, which greys
        // the stale row out instead of leaving it clickable. The failure-reason table has no entry that
        // says "delisted", so a "failed" state here would have to borrow a text about something else.
        // The reason byte goes out as 0 because the setter reads it only on state 3.
        SendApplyRefusal(this, player, packet.Ticket, nullptr, 0,
            WorldPackets::LFGList::ApplicationStateBits::DeclinedDelisted);
        return;
    }

    if (listing->LeaderGuid == player->GetGUID())
    {
        // Applying to one's own listing. The client never offers it (the browser hides the player's own
        // entry), so this is a hand-built or stale message rather than something a player can produce -
        // but it still gets an answer instead of silence. ERR_ALREADY_USING_LFG_LIST is the one code in the
        // table that describes the situation: this player is the lister.
        SendApplyRefusal(this, player, packet.Ticket, listing,
            WorldPackets::LFGList::ApplicationFailureReason::AlreadyUsingLfgList);
        return;
    }

    // The client echoes ActivityIDs[0] of the browse row it applied from (12.1 binding C_LFGList
    // .ApplyToGroup @ RVA 0x24EB2F0 reads `*(row+0x30)`, i.e. element 0 of the row descriptor's ActivityIDs
    // vector - the full source trail is in LFGListPackets.h). A value that is not among the listing's
    // activities means the player clicked a row that has since been edited, so the application is refused.
    // This used to be compared against Descriptor.CategoryID, and that killed the whole path without a
    // trace: the 12.1 reference listing carries CategoryID 3 with ActivityIDs [1735], so the guard fired on
    // a perfectly current row and the applicant received nothing at all.
    if (packet.ActivityID && !listing->Descriptor.ActivityIDs.empty()
        && std::find(listing->Descriptor.ActivityIDs.begin(), listing->Descriptor.ActivityIDs.end(),
            packet.ActivityID) == listing->Descriptor.ActivityIDs.end())
    {
        // The row the player clicked has been edited to a different activity since it was fetched.
        // ERR_LFG_INVALID_SLOT ("One or more dungeons was not valid.") is what the mismatch is.
        // Round 6 fixed the comparison itself here; the return stayed silent until now, which meant the
        // corrected guard still produced the same nothing-happens as the defect it replaced.
        SendApplyRefusal(this, player, packet.Ticket, listing,
            WorldPackets::LFGList::ApplicationFailureReason::InvalidSlot);
        return;
    }

    LFGList::Application* app = sLFGListMgr.AddApplication(listing->Id, player->GetGUID(), packet.RoleMask,
        uint32(player->GetPrimarySpecialization()), uint32(player->GetAverageItemLevel()), packet.Comment);
    if (!app)
    {
        // AddApplication refuses for exactly two reasons: the listing is gone, or the player is already at
        // LFGList.MaxApplicationsPerPlayer (default 5) counting only applications still in state Applied.
        // The listing was resolved five lines above and nothing between the two can drop it, so at this
        // point nullptr means the cap - and the cap has a code of its own: ERR_LFG_REASON_TOO_MANY_LFG,
        // "You are queued for too many instances."
        // This was the refusal that a player meets in normal play, and it was the quietest of the four.
        SendApplyRefusal(this, player, packet.Ticket, listing,
            WorldPackets::LFGList::ApplicationFailureReason::TooManyLfg);
        return;
    }

    // Confirm the application to the applicant (sniff-exact: app ticket + expiration + listing tickets +
    // the full row snapshot so the client renders the "applied" card without a re-search).
    WorldPackets::LFGList::LFGListApplyToGroupResult result;
    FillApplicationTicket(result.Ticket, *app);
    result.ApplicationExpiration = LFGListMgr::GetApplicationExpiration(*app);
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
    // that never invited it. This is the one ending here that stays silent on purpose: no legitimate client
    // can reach it - the Accept button only exists on an invited application - so anything arriving here is
    // forged, and answering it would confirm to the sender that the application id it guessed exists.
    if (app->State != LFGList::ApplicationState::Invited)
        return;

    // Every ending below this line answers the applicant. It pressed Accept; a bare `return` here means a
    // button that does nothing, which is the exact defect Runde 9 closed on HandleLFGListApplyToGroup.
    // `app` is a pointer into listing->Applications and RemoveApplication invalidates it, so the record the
    // answers are built from is copied out first.
    LFGList::Application const application = *app;

    Player* leader = ObjectAccessor::FindConnectedPlayer(listing->LeaderGuid);
    if (!leader)
    {
        // The inviting leader went offline between invite and accept. "failed" (nibble 3), not one of the
        // three decline states: nobody declined anything, the group is simply not reachable. The
        // application stays - the leader may come back, and the leader's own list is unchanged by this.
        // ERR_LFG_MEMBERS_NOT_PRESENT, "One or more group members are pending invites or disconnected", is
        // what the situation is: the one member that matters is disconnected.
        SendApplicationStatusBits(*listing, application, WorldPackets::LFGList::ApplicationStateBits::Failed,
            WorldPackets::LFGList::ApplicationFailureReason::MembersNotPresent);
        return;
    }

    // Join (or form) the leader's party.
    Group* group = leader->GetGroup();
    if (!group)
    {
        group = new Group();
        if (!group->Create(leader))
        {
            delete group;
            // Group::Create failed - a server-side fault with no player-facing cause, which is precisely
            // what ERR_LFG_NO_LFG_OBJECT ("Internal LFG Error.") is the table's word for.
            SendApplicationStatusBits(*listing, application, WorldPackets::LFGList::ApplicationStateBits::Failed,
                WorldPackets::LFGList::ApplicationFailureReason::NoLfgObject);
            return;
        }
        sGroupMgr->AddGroup(group);
        // GroupGuid is the LIVE group and is what enumerates members from here on (SendApplicantList,
        // FillSearchRow). It is deliberately NOT the listing's wire identity: that is Listing::TicketGuid,
        // frozen at CreateListing, so this assignment cannot re-key a listing the client already holds.
        listing->GroupGuid = group->GetGUID();
    }

    if (group->IsFull())
    {
        // The party filled up between the invite and this accept - reachable in ordinary play the moment
        // two invited applicants accept the same last seat. "declined_full" (nibble 6) is the client's own
        // word for it; the constant was measured and had no caller until now, which is what made this
        // ending silent. The application is dropped with it: the seat it was invited to no longer exists,
        // and leaving the row standing would give the leader an Invite button that cannot succeed.
        SendApplicationStatusBits(*listing, application, WorldPackets::LFGList::ApplicationStateBits::DeclinedFull,
            WorldPackets::LFGList::ApplicationFailureReason::NotAFailure);
        sLFGListMgr.RemoveApplication(applicationId);
        SendApplicantList(*listing);
        return;
    }

    if (applicant->GetGroup())
    {
        // The applicant joined some other party in the meantime. Its own doing, not a decline by the
        // leader, so "failed" - and the application goes, because it can never be honoured from here.
        //
        // UNVERIFIED: the reason code. The blocker is the applicant's OWN membership in another party, and
        // the presenter's closed table has no code that says so - it speaks about the group, the dungeon or
        // the queue. ERR_ALREADY_USING_LFG_LIST ("You can't do that while using Premade Groups.") is the one
        // entry that names the player's own conflicting state, and it is the code this unit already uses for
        // the other state conflict of the same kind (applying to one's own listing). A recording of this
        // ending would settle it; it is in the status file's aufnahme_noetig with the other refusals.
        SendApplicationStatusBits(*listing, application, WorldPackets::LFGList::ApplicationStateBits::Failed,
            WorldPackets::LFGList::ApplicationFailureReason::AlreadyUsingLfgList);
        sLFGListMgr.RemoveApplication(applicationId);
        SendApplicantList(*listing);
        return;
    }

    // Sniff-confirmed retail flow: accepting the invite adds the applicant to the party directly (no
    // SMSG_PARTY_INVITE dialog), the accepted state (0xA0) is echoed, and the joining member receives the
    // listing status (0x19).
    //
    // The return value is checked: the acceptance plus the 0x19 listing status used to go out regardless,
    // so the applicant was told it had joined a party it was not in.
    //
    // Group::AddMember has exactly ONE false return, and it is not a family of them: it fails only when the
    // group is a raid and no subgroup has a free slot (Group.cpp, the `if (!groupFound) return false` of the
    // MAX_RAID_SUBGROUPS scan) - everything after that point either succeeds or aborts. So the reason is not
    // a judgement call: ERR_LFG_GROUP_FULL, "Your group is already full." The IsFull() guard above already
    // covers the same ground by member count (MAX_RAID_SUBGROUPS * MAX_GROUP_SIZE == MAX_RAID_SIZE, so a
    // raid with every subgroup full IS full), which makes this a consistency backstop for a group whose
    // subgroup counters and slot vector have drifted apart - reachable only then, and answered when it is.
    if (!group->AddMember(applicant))
    {
        SendApplicationStatusBits(*listing, application, WorldPackets::LFGList::ApplicationStateBits::Failed,
            WorldPackets::LFGList::ApplicationFailureReason::GroupFull);
        return;
    }

    LFGList::Application accepted = application;
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
    // GroupFinderActivity.ExpansionID of the expansion that has not shipped yet. Not a named client constant -
    // read straight out of the data: the 205 activities the 12.1 captures blacklist with reason 1 are exactly
    // the ExpansionID == 11 rows of GroupFinderActivity, and no reason-1 row has any other ExpansionID.
    constexpr int32 EXPANSION_UNRELEASED_GROUP_FINDER = 11;

    // SMSG_LFG_LIST_UPDATE_BLACKLIST content, extracted verbatim from the TARGET build 12.1.0.69382
    // (C:/dumps/wpp_work/lfg_ref/69382_s69382_ws6_5A000E_{0,1}.bin, both 9100 byte and BYTE-IDENTICAL to each
    // other): u32 count (1137) + 1137 x {u32 GroupFinderActivity id, u32 reason}, and 4 + 1137*8 = 9100 exactly.
    // Row order is the observed order of the capture, unchanged - re-serialising this array reproduces both
    // capture files byte for byte, which is the D1 evidence for this message.
    //
    // This REPLACES a 456-row table that came from 12.0.7.68974 (0x56000E - the opcode carried a different
    // value in that build, which is why the old table survived the move to 12.1 unnoticed). That table was
    // measured against the same tooling and really is the 12.0.7 wire; it is simply the wrong build. It cost
    // the client 681 blacklisted activities and every single reason-1 row.
    //
    // MEASURED, and the reason this table is a snapshot and not a truth: the blacklist is per character and
    // per moment, not a constant. Row counts across the four 12.1 captures: 69273 = 533, 69382 = 1137,
    // 69404 (horde) = 1121 and, from the SAME character later in the SAME session, 1088. Between those two
    // 69404 captures 33 rows vanished and 26 changed reason 2 -> 19. Across different characters the table is
    // ~96% stable (69382 vs 69404: 40 of ~1130 rows differ), so a snapshot is a good approximation and
    // nothing more.
    //
    // What the reasons are, as far as the captures decide it:
    //   reason  1 - EXACT RULE, see below: ExpansionID == 11 (the unreleased expansion). 205 activities,
    //               and the set is byte-for-byte identical in all three captures that carry it.
    //   reason 18 - 143 rows in ALL FOUR captures, identical set: legacy content permanently off the group
    //               finder (MoP/WoD zones and the 10-man raid tiers - "The Jade Forest", "Mogu'shan Vaults
    //               (10 Heroic)"). Constant, therefore derivable, but the deriving field is not yet found.
    //   reason  2/10/19 - condition-gated as well (880 of the 900 activities carrying a PlayerConditionID are
    //               blacklisted for this character); the reason -> cause mapping is NOT established.
    //   reason  3 - DOES NOT OCCUR IN THIS TABLE, and it is worth saying why, because it occurs in the other
    //               two captures. Counted over the shipped 1137 rows the reasons are exactly
    //               {1: 205, 2: 171, 10: 9, 18: 143, 19: 609}. Reason 3 is the PvP level brackets
    //               (activities 14/15/351-355/389-394, PlayerConditionID 86425..86430, one per bracket): in
    //               69273 all thirteen of them carry reason 3, in 69404 only 15/393/394 do - and 15/393/394
    //               are precisely the three that are ABSENT from the 69382 table, i.e. not blacklisted for
    //               this character at all. What the 69382 character does get is reason 2 on the other ten
    //               (14, 351-355, 389-392). So reason 3 is a per-character bracket verdict that this
    //               particular snapshot happens not to contain, and a server serving this array never emits
    //               it. Do not read the table as evidence that reason 3 is gone in 12.1.
    // UNVERIFIED: reasons 2, 10, 18 and 19 are served from this snapshot instead of being computed from
    // the requesting player. A character that differs from the 69382 tester sees at most ~40 wrong rows out
    // of 1137, and the 26 rows that moved 2 -> 19 mid-session show the reason itself can be wrong. Deriving
    // them needs the reason -> PlayerCondition mapping out of the client, which no capture gives.
    struct LFGListBlacklistRow { uint16 ActivityID; uint8 Reason; };
    constexpr LFGListBlacklistRow LFGListActivityBlacklist[] =
    {
        {14,2}, {19,19}, {23,19}, {24,19}, {25,19}, {26,19}, {28,19}, {29,19}, {30,19}, {31,19},
        {32,19}, {33,19}, {34,19}, {35,19}, {36,19}, {37,2}, {38,2}, {39,2}, {40,2}, {41,19},
        {42,19}, {6,19}, {7,19}, {9,19}, {43,19}, {44,19}, {63,19}, {64,19}, {72,19}, {45,19},
        {46,19}, {47,19}, {48,19}, {49,19}, {76,19}, {80,19}, {81,19}, {82,19}, {83,19}, {84,19},
        {85,19}, {86,19}, {4,19}, {87,19}, {88,19}, {89,19}, {90,19}, {91,19}, {92,19}, {93,19},
        {94,19}, {96,19}, {97,19}, {98,19}, {99,19}, {100,19}, {62,19}, {122,19}, {123,19}, {124,19},
        {125,19}, {126,19}, {127,19}, {128,19}, {129,19}, {130,19}, {131,19}, {132,19}, {133,19}, {134,19},
        {135,19}, {136,19}, {137,19}, {138,19}, {139,19}, {140,19}, {141,19}, {142,19}, {143,19}, {144,19},
        {148,19}, {146,19}, {147,19}, {149,19}, {150,19}, {151,19}, {152,19}, {153,19}, {154,19}, {157,19},
        {158,19}, {159,19}, {160,19}, {163,19}, {164,19}, {165,19}, {166,19}, {167,19}, {169,19}, {170,19},
        {171,19}, {179,2}, {180,2}, {181,2}, {182,19}, {183,2}, {184,19}, {186,2}, {293,19}, {294,19},
        {295,19}, {102,19}, {104,19}, {105,19}, {106,19}, {107,19}, {109,19}, {113,19}, {114,19}, {115,19},
        {116,19}, {117,19}, {118,19}, {119,19}, {120,19}, {121,19}, {316,19}, {317,19}, {318,19}, {319,19},
        {320,19}, {321,19}, {322,19}, {323,19}, {324,19}, {325,19}, {326,19}, {298,19}, {296,19}, {297,19},
        {299,19}, {300,19}, {301,19}, {303,19}, {304,19}, {305,19}, {306,19}, {307,19}, {308,19}, {309,19},
        {310,19}, {311,19}, {313,19}, {351,2}, {352,2}, {353,2}, {354,2}, {355,2}, {356,2}, {746,2},
        {360,2}, {363,2}, {364,2}, {389,2}, {390,2}, {391,2}, {392,2}, {731,2}, {732,2}, {733,2},
        {734,2}, {395,2}, {331,19}, {332,19}, {333,19}, {334,19}, {335,19}, {336,19}, {337,19}, {338,19},
        {339,19}, {340,19}, {341,19}, {342,19}, {343,19}, {344,19}, {345,19}, {346,19}, {347,19}, {348,19},
        {349,19}, {350,19}, {399,2}, {400,2}, {409,2}, {410,2}, {396,2}, {401,2}, {402,19}, {403,19},
        {404,19}, {405,19}, {406,19}, {407,19}, {408,19}, {397,2}, {398,2}, {412,2}, {413,19}, {414,19},
        {415,19}, {416,19}, {417,2}, {418,2}, {430,10}, {457,19}, {460,19}, {433,2}, {434,2}, {435,19},
        {436,19}, {437,19}, {438,19}, {439,19}, {440,19}, {441,19}, {442,19}, {458,2}, {459,19}, {443,19},
        {444,19}, {445,19}, {446,19}, {447,19}, {448,19}, {449,19}, {450,19}, {451,19}, {452,19}, {455,19},
        {456,19}, {453,19}, {454,19}, {482,19}, {483,19}, {484,2}, {485,2}, {486,19}, {468,19}, {472,19},
        {473,19}, {474,19}, {475,19}, {476,19}, {480,19}, {481,19}, {490,2}, {491,2}, {478,19}, {479,19},
        {461,19}, {462,19}, {463,19}, {464,19}, {465,19}, {466,19}, {467,19}, {470,19}, {471,19}, {527,19},
        {530,19}, {531,19}, {532,2}, {533,10}, {534,19}, {535,19}, {536,10}, {537,10}, {538,10}, {539,10},
        {542,2}, {494,2}, {495,2}, {496,2}, {497,2}, {498,2}, {500,19}, {501,10}, {502,19}, {504,19},
        {492,19}, {493,19}, {505,2}, {506,10}, {507,19}, {508,19}, {509,10}, {510,19}, {511,2}, {512,2},
        {513,2}, {514,19}, {515,2}, {517,19}, {518,19}, {519,19}, {521,19}, {522,19}, {523,19}, {525,19},
        {526,19}, {644,19}, {653,19}, {654,2}, {655,2}, {645,2}, {646,2}, {657,2}, {669,2}, {663,19},
        {664,19}, {665,19}, {682,19}, {679,19}, {658,19}, {659,19}, {660,2}, {661,2}, {662,2}, {666,19},
        {667,19}, {668,19}, {670,19}, {671,19}, {672,19}, {673,2}, {676,19}, {677,19}, {683,19}, {684,2},
        {685,19}, {686,19}, {687,19}, {724,19}, {725,19}, {726,19}, {727,19}, {728,19}, {729,19}, {730,19},
        {722,2}, {689,2}, {691,19}, {693,2}, {695,19}, {735,19}, {736,19}, {737,19}, {697,2}, {708,2},
        {688,2}, {700,2}, {692,2}, {698,2}, {701,19}, {702,19}, {703,19}, {706,2}, {707,2}, {709,19},
        {723,19}, {743,2}, {744,2}, {745,2}, {711,2}, {713,19}, {715,19}, {717,19}, {719,2}, {690,2},
        {694,2}, {696,2}, {699,19}, {704,2}, {705,19}, {710,2}, {712,2}, {714,19}, {716,2}, {718,2},
        {720,2}, {721,2}, {1016,19}, {1017,19}, {1018,2}, {1019,2}, {1020,19}, {1021,19}, {1022,2}, {1025,19},
        {1026,19}, {1027,19}, {1028,19}, {1029,19}, {1030,19}, {1031,19}, {1032,19}, {1033,19}, {1034,19}, {940,19},
        {941,19}, {942,19}, {943,19}, {944,19}, {945,19}, {946,19}, {947,19}, {948,19}, {949,19}, {950,19},
        {951,19}, {952,19}, {953,19}, {970,19}, {955,19}, {956,19}, {957,19}, {958,19}, {959,19}, {960,19},
        {961,19}, {962,19}, {963,19}, {964,19}, {965,19}, {966,19}, {967,19}, {968,19}, {969,19}, {971,19},
        {972,19}, {973,19}, {974,19}, {975,19}, {976,19}, {977,19}, {978,19}, {979,19}, {980,19}, {981,19},
        {982,19}, {983,19}, {984,19}, {985,19}, {986,19}, {987,19}, {988,19}, {989,19}, {990,19}, {991,19},
        {992,19}, {993,19}, {994,19}, {995,19}, {996,19}, {997,19}, {998,19}, {999,19}, {1000,19}, {1001,19},
        {1002,19}, {1003,19}, {1004,19}, {1005,19}, {1006,19}, {1007,19}, {1008,19}, {1009,19}, {1010,19}, {1011,19},
        {1012,19}, {1013,19}, {1014,19}, {1015,19}, {1024,19}, {1192,19}, {1193,19}, {1158,2}, {1159,2}, {1160,19},
        {1161,2}, {1162,2}, {1163,2}, {1164,19}, {1165,2}, {1166,2}, {1167,2}, {1168,19}, {1169,2}, {1170,2},
        {1171,2}, {1172,19}, {1174,2}, {1175,2}, {1035,19}, {1036,19}, {1037,19}, {1038,19}, {1039,19}, {1040,19},
        {1041,19}, {1042,19}, {1043,19}, {1044,19}, {1045,19}, {1046,19}, {1047,19}, {1048,19}, {1049,19}, {1050,19},
        {1051,19}, {1052,19}, {1053,19}, {1054,19}, {1055,19}, {1056,19}, {1057,19}, {1194,2}, {1146,19}, {1176,19},
        {1177,2}, {1178,2}, {1179,2}, {1180,19}, {1181,2}, {1182,2}, {1183,2}, {1184,19}, {1185,2}, {1186,2},
        {1187,2}, {1188,19}, {1189,2}, {1190,2}, {1191,2}, {1244,2}, {1245,2}, {1246,2}, {1247,19}, {1248,19},
        {1279,2}, {1278,2}, {1280,2}, {1303,19}, {1304,19}, {1305,19}, {1306,19}, {1307,19}, {1308,2}, {1309,2},
        {1310,2}, {1274,19}, {1275,2}, {1276,2}, {1277,2}, {1281,19}, {1282,19}, {1283,19}, {1284,19}, {1285,19},
        {1286,19}, {1287,19}, {1288,19}, {1290,19}, {1311,18}, {1312,18}, {1313,18}, {1314,18}, {1315,18}, {1316,18},
        {1251,2}, {1252,2}, {1253,2}, {1289,2}, {1291,2}, {1292,2}, {1293,2}, {1294,19}, {1317,18}, {1318,18},
        {1319,18}, {1320,18}, {1321,18}, {1322,18}, {1195,19}, {1235,2}, {1236,2}, {1237,2}, {1295,19}, {1296,19},
        {1297,19}, {1299,19}, {1300,19}, {1301,19}, {1302,19}, {1370,18}, {1371,18}, {1372,18}, {1373,18}, {1374,18},
        {1375,18}, {1376,18}, {1377,18}, {1378,18}, {1382,19}, {1383,2}, {1425,19}, {1426,19}, {1427,19}, {1428,19},
        {1429,19}, {1430,19}, {1431,19}, {1432,19}, {1433,19}, {1434,19}, {1435,19}, {1436,19}, {1437,19}, {1438,19},
        {1439,19}, {1440,19}, {1441,19}, {1442,19}, {1443,19}, {1384,19}, {1385,19}, {1386,19}, {1387,19}, {1388,19},
        {1389,19}, {1390,19}, {1391,19}, {1392,19}, {1393,19}, {1394,19}, {1395,19}, {1396,19}, {1397,19}, {1398,19},
        {1399,19}, {1400,19}, {1401,19}, {1402,19}, {1403,19}, {1404,19}, {1405,19}, {1406,19}, {1407,19}, {1408,19},
        {1409,19}, {1410,19}, {1411,19}, {1412,19}, {1413,19}, {1414,19}, {1415,19}, {1416,19}, {1417,19}, {1418,19},
        {1419,19}, {1420,19}, {1421,19}, {1422,19}, {1423,19}, {1424,19}, {1323,18}, {1324,18}, {1325,18}, {1326,18},
        {1327,18}, {1328,18}, {1329,18}, {1330,18}, {1331,18}, {1332,18}, {1333,18}, {1334,18}, {1335,18}, {1336,18},
        {1337,18}, {1338,18}, {1339,18}, {1340,18}, {1341,18}, {1342,18}, {1343,18}, {1344,18}, {1345,18}, {1346,18},
        {1347,18}, {1348,18}, {1349,18}, {1350,18}, {1351,18}, {1356,18}, {1353,18}, {1354,18}, {1355,18}, {1357,18},
        {1358,18}, {1359,18}, {1360,18}, {1361,18}, {1362,18}, {1363,18}, {1364,18}, {1365,18}, {1366,18}, {1367,18},
        {1368,18}, {1369,18}, {1518,19}, {1519,2}, {1547,19}, {1548,2}, {1549,2}, {1550,19}, {1553,19}, {1554,19},
        {1555,19}, {1556,19}, {1557,19}, {1558,19}, {1616,2}, {1617,2}, {1618,2}, {1619,2}, {1694,19}, {1564,19},
        {1565,19}, {1695,19}, {1702,19}, {1521,2}, {1566,19}, {1567,19}, {1568,19}, {1569,19}, {1458,19}, {1459,19},
        {1456,19}, {1457,19}, {1462,19}, {1463,19}, {1460,19}, {1461,19}, {1466,19}, {1467,19}, {1464,19}, {1465,19},
        {1470,19}, {1471,19}, {1468,19}, {1469,19}, {1446,19}, {1447,19}, {1444,19}, {1445,19}, {1450,19}, {1451,19},
        {1448,19}, {1449,19}, {1454,19}, {1455,19}, {1452,19}, {1453,19}, {1520,19}, {1490,19}, {1491,19}, {1488,19},
        {1489,19}, {1507,19}, {1508,2}, {1509,2}, {1494,19}, {1495,19}, {1492,19}, {1493,19}, {1498,19}, {1499,19},
        {1496,19}, {1497,19}, {1502,19}, {1503,19}, {1500,19}, {1501,19}, {1474,19}, {1475,19}, {1472,19}, {1473,19},
        {1478,19}, {1479,19}, {1476,19}, {1477,19}, {1482,19}, {1483,19}, {1480,19}, {1481,19}, {1486,19}, {1487,19},
        {1484,19}, {1485,19}, {1298,19}, {1585,19}, {1570,19}, {1571,19}, {1574,19}, {1572,19}, {1573,19}, {1582,19},
        {1700,1}, {1559,19}, {1562,19}, {1563,19}, {1560,19}, {1561,19}, {1600,2}, {1601,2}, {1602,2}, {1620,18},
        {1621,18}, {1622,18}, {1623,18}, {1624,18}, {1625,18}, {1626,18}, {1627,18}, {1628,18}, {1629,18}, {1630,18},
        {1631,18}, {1632,18}, {1633,18}, {1634,18}, {1635,18}, {1636,18}, {1637,18}, {1638,18}, {1639,18}, {1640,18},
        {1641,18}, {1642,18}, {1643,18}, {1699,1}, {1504,2}, {1505,2}, {1506,2}, {1510,2}, {1511,2}, {1512,2},
        {1644,18}, {1645,18}, {1646,18}, {1647,18}, {1648,18}, {1649,18}, {1650,18}, {1651,18}, {1652,18}, {1653,18},
        {1654,18}, {1655,18}, {1656,18}, {1657,18}, {1658,18}, {1659,18}, {1660,18}, {1661,18}, {1662,18}, {1663,18},
        {1664,18}, {1665,18}, {1666,18}, {1667,18}, {1668,18}, {1535,19}, {1539,1}, {1540,1}, {1541,1}, {1542,1},
        {1669,18}, {1670,18}, {1671,18}, {1672,18}, {1673,18}, {1674,18}, {1675,18}, {1676,18}, {1677,18}, {1678,18},
        {1679,18}, {1680,18}, {1681,18}, {1682,18}, {1683,18}, {1701,1}, {1551,19}, {1552,19}, {1735,1}, {1744,19},
        {1745,19}, {1746,19}, {1747,19}, {1748,19}, {1749,1}, {1750,1}, {1751,1}, {1754,1}, {1753,1}, {1755,1},
        {1756,1}, {1757,1}, {1758,1}, {1759,1}, {1760,1}, {1761,1}, {1762,1}, {1763,1}, {1764,1}, {1765,1},
        {1707,19}, {1708,2}, {1709,2}, {1711,2}, {1721,1}, {1722,1}, {1723,1}, {1736,19}, {1737,19}, {1738,19},
        {1739,19}, {1740,19}, {1741,19}, {1742,19}, {1743,19}, {1858,1}, {1859,1}, {1860,1}, {1861,1}, {1862,1},
        {1863,1}, {1864,1}, {1865,1}, {1866,1}, {1867,1}, {1868,1}, {1869,1}, {1778,1}, {1779,1}, {1780,1},
        {1870,1}, {1871,1}, {1872,1}, {1873,1}, {1874,1}, {1875,1}, {1876,1}, {1877,1}, {1878,1}, {1879,1},
        {1880,1}, {1955,1}, {1782,18}, {1783,18}, {1785,18}, {1787,18}, {1788,18}, {1789,18}, {1790,18}, {1791,18},
        {1793,18}, {1794,18}, {1795,18}, {1813,1}, {1814,1}, {1815,1}, {1816,1}, {1817,1}, {1766,1}, {1767,1},
        {1768,1}, {1769,2}, {1770,19}, {1818,1}, {1819,1}, {1820,1}, {1821,1}, {1822,1}, {1823,1}, {1824,1},
        {1825,1}, {1826,1}, {1827,1}, {1772,1}, {1773,1}, {1774,1}, {1775,1}, {1776,1}, {1777,1}, {1828,1},
        {1829,1}, {1830,1}, {1831,1}, {1832,1}, {1833,1}, {1834,1}, {1835,1}, {1836,1}, {1837,1}, {1838,1},
        {1839,1}, {1840,1}, {1841,1}, {1842,1}, {1843,1}, {1844,1}, {1845,1}, {1846,1}, {1847,1}, {1848,1},
        {1849,1}, {1850,1}, {1851,1}, {1852,1}, {1853,1}, {1854,1}, {1855,1}, {1856,1}, {1857,1}, {1893,1},
        {1894,1}, {1895,1}, {1896,1}, {1897,1}, {1898,1}, {1899,1}, {1900,1}, {1901,1}, {1902,1}, {1903,1},
        {1904,1}, {1905,1}, {1930,1}, {1931,1}, {1932,1}, {1933,1}, {1949,1}, {1950,1}, {1951,1}, {1952,1},
        {1936,1}, {1946,1}, {1947,1}, {1948,1}, {1906,1}, {1907,1}, {1908,1}, {1909,1}, {1910,1}, {1911,1},
        {1912,1}, {1913,1}, {1914,1}, {1915,1}, {1916,1}, {1917,1}, {1918,1}, {1919,1}, {1920,1}, {1921,1},
        {1922,1}, {1923,1}, {1924,1}, {1942,1}, {1945,18}, {1944,1}, {1943,1}, {1881,1}, {1882,1}, {1883,1},
        {1884,1}, {1885,1}, {1886,1}, {1887,1}, {1888,1}, {1889,1}, {1890,1}, {1891,1}, {1892,1}, {1972,1},
        {2024,1}, {2025,1}, {2026,1}, {2027,1}, {2028,1}, {2029,1}, {2002,1}, {2030,1}, {2031,1}, {1956,1},
        {1957,1}, {1964,1}, {1965,1}, {1968,1}, {2003,1}, {2004,1}, {2005,1}, {2006,1}, {2007,1}, {2008,1},
        {2009,1}, {2010,1}, {2011,1}, {2012,1}, {2013,1}, {1970,1}, {2014,1}, {1974,1}, {2015,1}, {2016,1},
        {2017,1}, {2018,1}, {2019,1}, {2020,1}, {2021,1}, {2022,1}, {2023,1}
    };
}

void WorldSession::HandleRequestLFGListBlacklist(WorldPackets::LFGList::RequestLFGListBlacklist& /*packet*/)
{
    WorldPackets::LFGList::LFGListUpdateBlacklist packet;
    packet.Entries.reserve(std::size(LFGListActivityBlacklist));

    std::unordered_set<uint32> sent;
    sent.reserve(std::size(LFGListActivityBlacklist));
    for (LFGListBlacklistRow const& row : LFGListActivityBlacklist)
    {
        WorldPackets::LFGList::LFGListBlacklistEntry& entry = packet.Entries.emplace_back();
        entry.ActivityID = row.ActivityID;
        entry.Reason = row.Reason;
        sent.insert(row.ActivityID);
    }

    // Reason 1 is the one rule the captures settle exactly, so it is DERIVED and not trusted to the snapshot
    // above. In all three 12.1 captures that carry reason 1 (69382 x1, 69404 x2 - different build, different
    // faction, different character) the reason-1 set is EXACTLY the set of GroupFinderActivity rows with
    // ExpansionID == 11, 205 of 205, no row missing and no row extra. 69273 carries no reason 1 at all, which
    // fits: expansion 11 is the unreleased one and its activities entered the client data later.
    // Deriving it matters because the alternative rots on a schedule. Every build adds ExpansionID 11
    // activities, retail blacklists each of them on sight, and a frozen list of 205 ids would keep answering
    // with yesterday's set - the exact failure that put the 12.0.7 table into a 12.1 branch in the first
    // place. Rows already present in the snapshot are left alone so the observed 1137 keep their observed
    // order and no activity is ever sent twice (the retail payload has no duplicate ids either).
    for (GroupFinderActivityEntry const* activity : sGroupFinderActivityStore)
    {
        if (activity->ExpansionID != EXPANSION_UNRELEASED_GROUP_FINDER || sent.count(activity->ID))
            continue;

        WorldPackets::LFGList::LFGListBlacklistEntry& entry = packet.Entries.emplace_back();
        entry.ActivityID = activity->ID;
        entry.Reason = 1;
    }

    SendPacket(packet.Write());
}

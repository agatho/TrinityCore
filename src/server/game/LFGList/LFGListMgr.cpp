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

#include "LFGListMgr.h"
#include "Config.h"
#include <algorithm>
#include "DB2Stores.h"
#include "GameTime.h"
#include "LFGListPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Util.h"
#include <cctype>
#include <utility>

namespace
{
    constexpr uint32 EXPIRE_CHECK_INTERVAL_MS = 10 * IN_MILLISECONDS;
    // How long an open Premade Groups browser keeps receiving live pushes without re-searching. There is
    // no "stopped searching" opcode, so the subscription lapses instead of leaking.
    constexpr uint32 SEARCH_SUBSCRIPTION_TTL_SECONDS = 5 * MINUTE;
}

LFGListMgr& LFGListMgr::Instance()
{
    static LFGListMgr instance;
    return instance;
}

void LFGListMgr::Update(uint32 diff)
{
    _expireTimer += diff;
    if (_expireTimer < EXPIRE_CHECK_INTERVAL_MS)
        return;
    _expireTimer = 0;

    uint32 const now = GameTime::GetGameTime();

    // Application timeout sweep (retail: 300s, sniff-verified AppExpiration = applyTime + 300): expired
    // applications become soft-declined (removed; the applicant is told, the leader's list refreshes).
    uint32 const applicationTimeout = uint32(sConfigMgr->GetIntDefault("LFGList.ApplicationTimeoutSeconds", 300));
    if (applicationTimeout)
    {
        for (auto& [listingId, listing] : _listings)
        {
            bool changed = false;
            for (auto appItr = listing.Applications.begin(); appItr != listing.Applications.end(); )
            {
                if (appItr->State == LFGList::ApplicationState::Applied && now >= appItr->AppliedTime + applicationTimeout)
                {
                    if (Player* applicant = ObjectAccessor::FindConnectedPlayer(appItr->ApplicantGuid))
                    {
                        WorldPackets::LFGList::LFGListApplicationStatusUpdate statusUpdate;
                        // Both tickets through the shared builders. Hand-building them here put the LEADER
                        // guid into the listing ticket where every other path sends the PARTY guid, so the
                        // two disagreed on the same listing - the same defect class as the delist paths.
                        FillApplicationTicket(statusUpdate.Ticket, *appItr);
                        FillListingTicket(statusUpdate.ListingTicket, listing);
                        statusUpdate.UnkResult = 8;
                        // "timedout", not a generic decline: nibble 8 is a state of its own in the client's
                        // vocabulary (mapper 0x24DADA0) and the UI treats it separately from the three
                        // decline states (LFGList.lua:1962-1968). This path IS the timeout, so it says so.
                        statusUpdate.StateBits = WorldPackets::LFGList::ApplicationStateBits::TimedOut;
                        applicant->SendDirectMessage(statusUpdate.Write());
                    }
                    _applicationIndex.erase(appItr->Id);
                    appItr = listing.Applications.erase(appItr);
                    changed = true;
                }
                else
                    ++appItr;
            }

            // Through the shared producer, like every other path. Hand-built here, it reached the LEADER
            // ONLY, so a timeout left the expired applicant standing in every other group member's list.
            if (changed)
                SendApplicantList(listing);
        }
    }

    // Collected first, delisted after: DelistAndNotify sends from the live listing and then erases it, which
    // would invalidate an iterator held across the call. Worth the extra pass - it leaves exactly ONE producer
    // of SMSG_LFG_LIST_UPDATE_STATUS's delist form in the tree, which is the whole point after four separate
    // hand-built copies of it drifted apart.
    std::vector<std::pair<uint32, ObjectGuid>> expired;
    for (auto const& [listingId, listing] : _listings)
        if (listing.ExpireTime && now >= listing.ExpireTime)
            expired.emplace_back(listingId, listing.LeaderGuid);

    for (auto const& [listingId, leaderGuid] : expired)
    {
        // Tell the leader (if online) the listing expired, then delist it.
        //
        // What actually reaches the player is the STATUS byte of the second message, not the Reason byte
        // of the first. Consumer of SMSG_LFG_LIST_UPDATE_STATUS @ RVA 0x24DE410: with Listed = 0, and only
        // once the incoming ticket matches the stored active entry field for field, it clears the entry,
        // fires LFG_LIST_ACTIVE_ENTRY_UPDATE (hash 0xF03C06AFDFAA0FB0) and then switches on Status:
        //   0x2C -> LFG_LIST_ENTRY_EXPIRED_TOO_MANY_PLAYERS   0x3B -> LFG_LIST_ENTRY_EXPIRED_TIMEOUT
        //   0x4B -> GameError 0x291                            anything else -> no popup at all
        // (both event hashes verified against the 12.1 image; the two Lua events carry no payload, which
        // is why the byte has to select between them). This sweep is the timeout, so 0x3B it is - the
        // previous Status 0 landed in the default arm and the player was never told anything.
        if (LFGList::Listing const* listing = GetListing(listingId))
        {
            if (Player* leader = ObjectAccessor::FindConnectedPlayer(leaderGuid))
            {
                WorldPackets::LFGList::LFGListUpdateExpiration expiration;
                // Same builder as every other listing message. Hand-building it here put the LEADER guid
                // where the create path had sent the PARTY guid and left Time at 0, so the consumer's
                // field-by-field comparison failed and the delist was dropped without a trace.
                FillListingTicket(expiration.Ticket, *listing);
                expiration.ExpirationTime = listing->ExpireTime;
                // UNVERIFIED: the Reason enum of this message. Its own hook slot (RVA 0x55FECF0, dispatcher
                // case 5898251) is NULL in the retail image, so no consumer could be decoded. We send the
                // same code the STATUS switch above uses for a timeout, so the two messages cannot disagree.
                expiration.Reason = 0x3B;
                leader->SendDirectMessage(expiration.Write());
            }
        }

        DelistAndNotify(listingId, leaderGuid, 0x3B);   // -> LFG_LIST_ENTRY_EXPIRED_TIMEOUT
    }
}

uint32 LFGListMgr::CreateListing(Player* leader, WorldPackets::LFGList::ListingDescriptor const& descriptor)
{
    if (!leader)
        return 0;

    // One active listing per leader; re-publishing replaces the old one.
    RemoveListingsBy(leader->GetGUID());

    uint32 const expireMinutes = uint32(sConfigMgr->GetIntDefault("LFGList.ListingExpiryMinutes", 30)); // sniff: 1800s, activity-refreshed

    uint32 const id = _nextListingId++;
    LFGList::Listing& listing = _listings[id];
    listing.Id = id;
    listing.LeaderGuid = leader->GetGUID();
    if (Group* group = leader->GetGroup())
        listing.GroupGuid = group->GetGUID();
    // The ONLY write to TicketGuid in the tree. Everything the client keys on is decided here, once, and
    // stays put for the life of the listing - see the field's comment in LFGListMgr.h.
    listing.TicketGuid = !listing.GroupGuid.IsEmpty() ? listing.GroupGuid : listing.LeaderGuid;
    listing.Descriptor = descriptor;
    listing.CreatedTime = GameTime::GetGameTime();
    listing.ExpireTime = expireMinutes ? listing.CreatedTime + expireMinutes * MINUTE : 0;
    EvaluateCensorship(listing);

    _listingByLeader[leader->GetGUID()] = id;

    // A newly published listing must appear in every open browser it matches.
    NotifyListingChanged(id);
    return id;
}

bool LFGListMgr::UpdateListing(uint32 listingId, ObjectGuid leader, WorldPackets::LFGList::ListingDescriptor const& descriptor)
{
    LFGList::Listing* listing = GetListing(listingId);
    if (!listing || listing->LeaderGuid != leader)
        return false;

    listing->Descriptor = descriptor;
    // Editing is exactly how a player clears a flag: re-run the check on the new text.
    EvaluateCensorship(*listing);
    TouchListing(*listing);

    // Edited listings are pushed so open browsers show the new title/activity without re-searching.
    NotifyListingChanged(listingId);
    return true;
}

void LFGListMgr::TouchListing(LFGList::Listing& listing)
{
    // Listing activity refreshes the expiry window (sniff: ExpirationTime moves to now + 1800 on edits/applies).
    if (uint32 const expireMinutes = uint32(sConfigMgr->GetIntDefault("LFGList.ListingExpiryMinutes", 30)))
        listing.ExpireTime = uint32(GameTime::GetGameTime()) + expireMinutes * MINUTE;
}

void LFGListMgr::RemoveListing(uint32 listingId, ObjectGuid leader)
{
    auto itr = _listings.find(listingId);
    if (itr == _listings.end() || itr->second.LeaderGuid != leader)
        return;

    for (LFGList::Application const& app : itr->second.Applications)
        _applicationIndex.erase(app.Id);
    _listingByLeader.erase(itr->second.LeaderGuid);
    _listings.erase(itr);
}

// Every server-initiated delist. Order matters and is the whole point of this function existing: the
// message is built from the LIVE listing and only then is the listing erased. The client compares the
// incoming ticket against its stored active entry field for field (consumer RVA 0x24DE410, six comparisons
// against LFG-list manager +0..+24) and drops a delist whose ticket differs, without a word in any log.
// Three separate paths used to assemble that ticket by hand; two of them got it wrong.
void LFGListMgr::DelistAndNotify(uint32 listingId, ObjectGuid leader, uint8 status)
{
    LFGList::Listing const* listing = GetListing(listingId);
    if (!listing || listing->LeaderGuid != leader)
        return;

    // Every addressee that was handed this listing with Listed = true holds an active entry keyed on the
    // ticket below, so every one of them needs the delist. The leader is included explicitly because a
    // listing that was never re-announced (no GET_STATUS, no edit) has an empty recipient set.
    std::unordered_set<ObjectGuid> addressees = listing->StatusRecipients;
    addressees.insert(leader);

    {
        WorldPackets::LFGList::LFGListUpdateStatus packet;
        FillListingTicket(packet.Ticket, *listing);
        packet.Status = status;
        packet.Listed = false;
        // The 69273 delist reference payload (69273_s69273_a_5A000A_1.bin) carries ExpirationTime 0 and a
        // zeroed descriptor with Listed = 0 - a delisted entry has no expiry left to report.
        packet.ExpirationTime = 0;
        packet.LeaderGuid = listing->LeaderGuid;     // present in every captured payload, listed or not
        // The second optional is present on the delist payload and nowhere else. Measured, not guessed:
        // 69273_s69273_a_5A000A_1.bin (74 bytes, the delist) ends on
        //   08 60 | 0f e0 88 10 7f 0e 44 16 08 | 00
        // = Status 0x08, bit byte 0x60 (Listed 0, has(Guid) 1, has(u8) 1), the 9-byte PackedGuid, then the
        // payload byte 0x00. The two listed payloads _0 and _2 (81 bytes each) carry 0xC0 instead, i.e.
        // has(u8) 0 - which is why WorldSession::SendLFGListUpdateStatus leaves it unset. Without this line
        // the delist went out one byte short and with 0x40 in the bit byte, so it did not match the
        // reference capture this layout is derived from.
        // UNVERIFIED: the meaning of the byte. Its only observed value is 0 and no consumer reads it in a
        // way that distinguishes values (see LFGListPackets.h).
        packet.UnkByte = 0;
        WorldPacket const* built = packet.Write();
        for (ObjectGuid const& addressee : addressees)
            if (Player* recipient = ObjectAccessor::FindConnectedPlayer(addressee))
                recipient->SendDirectMessage(built);
    }

    // The addressees above are the holders of an ACTIVE ENTRY. A player whose application is still pending
    // never received a Listed payload and is not among them, yet its application dies here with the listing -
    // and used to die in silence, so the applicant kept a live-looking application row for a listing that no
    // longer existed, until relog. Third flank of the same defect class as the silent RemoveListingsBy and
    // the silent RemoveApplicationsBy; this closes it. The notification has to go out before RemoveListing,
    // for the same reason the delist above does: the ticket cannot be built once the listing is gone.
    //
    // "declined_delisted" (nibble 7) is the client's own word for exactly this situation and is already what
    // HandleLFGListApplyToGroup answers when the listing vanished before the application arrived; LFGList.lua
    // :388 maps it to LFG_LIST_APP_DECLINED_DELISTED_MESSAGE and :243 counts it as a decline.
    // UNVERIFIED: that retail sends this message at delist time at all. The STATE is measured (mapper @ RVA
    // 0x24DADA0) and so is the UI's handling of it; what no capture in c:\dumps\wpp_work\lfg_ref shows is a
    // listing being delisted while an application is pending, because none of them contains an application.
    // See aufnahme_noetig in the status file. Silence is the one answer that is certainly wrong: the client
    // has no other way to learn that the application is over.
    for (LFGList::Application const& app : listing->Applications)
        SendApplicationStatusBits(*listing, app, WorldPackets::LFGList::ApplicationStateBits::DeclinedDelisted,
            WorldPackets::LFGList::ApplicationFailureReason::NotAFailure);

    RemoveListing(listingId, leader);
}

// The leader's listing goes away for a reason that is nobody's fault - they logged out, or they published
// a new listing over the old one. Both are still DELISTS, and both used to erase the three maps in silence.
// That broke the promise DelistAndNotify and Listing::StatusRecipients exist to keep: an applicant who had
// accepted an invite was handed a Listed payload (WorldSession::SendLFGListUpdateStatus, status 0x19) and
// the consumer at RVA 0x24DE410 built an active entry from it. With no delist reaching that player, the
// entry stood in their group finder until they relogged - the exact damage the header of this class
// describes, and it survived on the two flanks that did not go through DelistAndNotify.
//
// Status 0x08 is the no-popup arm of that consumer's switch (only 0x2C, 0x3B and 0x4B raise anything; see
// the Update() sweep above for the decoded switch). That is the right choice for both callers: the entry
// has to disappear, but neither a logout nor a re-publish is an expiry or an error the recipients need to
// be told about - the same reasoning as HandleLFGListLeave, which sends 0x08 for the deliberate delist.
void LFGListMgr::RemoveListingsBy(ObjectGuid leader)
{
    auto itr = _listingByLeader.find(leader);
    if (itr == _listingByLeader.end())
        return;

    // Copied out before the call: DelistAndNotify erases the very entry this iterator points at.
    uint32 const listingId = itr->second;
    DelistAndNotify(listingId, leader, 0x08);
    // DelistAndNotify bails out if the listing is already gone from _listings, which would leave this index
    // entry behind; the erase this function used to do unconditionally is kept for that case. A no-op on
    // the normal path, because RemoveListing has already removed it.
    _listingByLeader.erase(leader);
}

LFGList::Listing* LFGListMgr::GetListing(uint32 listingId)
{
    auto itr = _listings.find(listingId);
    return itr != _listings.end() ? &itr->second : nullptr;
}

LFGList::Listing* LFGListMgr::GetListingByLeader(ObjectGuid leader)
{
    auto itr = _listingByLeader.find(leader);
    return itr != _listingByLeader.end() ? GetListing(itr->second) : nullptr;
}

LFGList::Application* LFGListMgr::AddApplication(uint32 listingId, ObjectGuid applicant, uint8 roleMask, uint32 specId, uint32 itemLevel, std::string const& comment)
{
    LFGList::Listing* listing = GetListing(listingId);
    if (!listing)
        return nullptr;

    // Re-applying replaces the previous application from the same player.
    for (LFGList::Application& existing : listing->Applications)
    {
        if (existing.ApplicantGuid == applicant)
        {
            existing.RoleMask = roleMask;
            existing.SpecID = specId;
            existing.ItemLevel = itemLevel;
            existing.Comment = comment;
            existing.State = LFGList::ApplicationState::Applied;
            existing.AppliedTime = uint32(GameTime::GetGameTime());
            return &existing;
        }
    }

    LFGList::Application app;
    app.Id = _nextApplicationId++;
    app.ApplicantGuid = applicant;
    app.RoleMask = roleMask;
    app.SpecID = specId;
    app.ItemLevel = itemLevel;
    app.Comment = comment;
    app.State = LFGList::ApplicationState::Applied;
    app.AppliedTime = uint32(GameTime::GetGameTime());

    // Retail cap: at most 5 concurrent applications per player (MAX_LFG_LIST_APPLICATIONS, unchanged in 12.x).
    uint32 const maxApplications = uint32(sConfigMgr->GetIntDefault("LFGList.MaxApplicationsPerPlayer", 5));
    if (maxApplications)
    {
        uint32 active = 0;
        for (auto const& [otherId, otherListing] : _listings)
            for (LFGList::Application const& otherApp : otherListing.Applications)
                if (otherApp.ApplicantGuid == applicant && otherApp.State == LFGList::ApplicationState::Applied)
                    ++active;
        if (active >= maxApplications)
            return nullptr;
    }

    listing->Applications.push_back(app);
    TouchListing(*listing);
    _applicationIndex[app.Id] = listingId;
    return &listing->Applications.back();
}

LFGList::Listing* LFGListMgr::GetListingByApplication(uint32 applicationId)
{
    auto itr = _applicationIndex.find(applicationId);
    return itr != _applicationIndex.end() ? GetListing(itr->second) : nullptr;
}

LFGList::Application* LFGListMgr::GetApplication(uint32 applicationId)
{
    LFGList::Listing* listing = GetListingByApplication(applicationId);
    if (!listing)
        return nullptr;
    for (LFGList::Application& app : listing->Applications)
        if (app.Id == applicationId)
            return &app;
    return nullptr;
}

// The role the leader granted in CMSG_LFG_LIST_INVITE_APPLICANT.Invitees[]. Kept apart from the applied
// RoleMask because SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE.RoleGranted is a distinct field: echoing the
// applied mask back would make the two structurally indistinguishable and the leader's assignment lost.
bool LFGListMgr::SetApplicationGrantedRole(uint32 applicationId, uint8 roleMask)
{
    LFGList::Listing* listing = GetListingByApplication(applicationId);
    if (!listing)
        return false;

    for (LFGList::Application& app : listing->Applications)
    {
        if (app.Id != applicationId)
            continue;

        app.GrantedRoleMask = roleMask;
        return true;
    }
    return false;
}

bool LFGListMgr::SetApplicationState(uint32 applicationId, LFGList::ApplicationState state)
{
    if (LFGList::Application* app = GetApplication(applicationId))
    {
        app->State = state;
        return true;
    }
    return false;
}

void LFGListMgr::RemoveApplication(uint32 applicationId)
{
    LFGList::Listing* listing = GetListingByApplication(applicationId);
    if (!listing)
        return;

    std::erase_if(listing->Applications, [applicationId](LFGList::Application const& a) { return a.Id == applicationId; });
    _applicationIndex.erase(applicationId);
}

// Withdraw everything this player applied for - logout cleanup. Every OTHER removal path announces the new
// applicant list (cancel, decline, invite response, and the timeout sweep in Update); this one erased in
// silence, so a leader kept a ghost row for a player who had already logged out, and the Invite button on
// that row ran HandleLFGListInviteApplicant into GetApplication == nullptr and returned without a packet -
// a dead button with no error text. Same defect class RemoveListingsBy had on the listing side.
void LFGListMgr::RemoveApplicationsBy(ObjectGuid applicant)
{
    for (auto& [listingId, listing] : _listings)
    {
        bool changed = false;
        for (auto itr = listing.Applications.begin(); itr != listing.Applications.end(); )
        {
            if (itr->ApplicantGuid == applicant)
            {
                _applicationIndex.erase(itr->Id);
                itr = listing.Applications.erase(itr);
                changed = true;
            }
            else
                ++itr;
        }

        // After the erase, so the list that goes out is the one that remains. The departing applicant is
        // logging out and gets nothing: its own SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE would race the
        // session teardown, and the client discards the state of a character it is leaving anyway.
        if (changed)
            SendApplicantList(listing);
    }
}

LFGList::Listing const* LFGListMgr::GetListing(uint32 listingId) const
{
    auto itr = _listings.find(listingId);
    return itr != _listings.end() ? &itr->second : nullptr;
}

// The keyword half of a search. See the contract on the declaration for why a withheld listing is matched
// against an empty name rather than against the text it is hiding.
bool LFGListMgr::MatchesKeywords(LFGList::Listing const& listing, LFGList::SearchKeywords const& keywords)
{
    // An empty request constrains nothing. Matches(), the only caller today, no longer reaches this: it
    // decides the both-halves-empty case itself, because a wildcard inside an OR is a wildcard for the whole
    // condition. The guard stays because it is this function's contract - dropping it would make a future
    // caller's empty request match NOTHING, which is the opposite of what an empty request means.
    if (keywords.empty())
        return true;

    // Match against what a searcher would actually RECEIVE. GetPublicDescriptor clears Name for a withheld
    // listing, so there is no delivered name left to match and the listing cannot be a keyword hit.
    if (listing.IsTextWithheld())
        return false;

    std::string const& name = listing.Descriptor.Name;
    if (name.empty())
        return false;

    // Every block has to be satisfied by at least one of its own values (case-insensitive substring, as
    // before). The AND/OR split is LFGListSearch::GetKeywords' documented and marked decision.
    for (std::vector<std::string> const& alternatives : keywords)
    {
        bool any = false;
        for (std::string const& value : alternatives)
            if (StringContainsStringI(name, value))
            {
                any = true;
                break;
            }
        if (!any)
            return false;
    }
    return true;
}

// Does the listing offer at least one of the requested GroupFinderActivity ids? An empty request is a
// wildcard. A listing that names no activity at all can satisfy no activity filter, which is why the loop
// and not a separate empty-listing case decides it.
static bool OffersAnyActivity(std::vector<uint32> const& listedActivities, std::vector<uint32> const& wanted)
{
    if (wanted.empty())
        return true;

    for (uint32 listed : listedActivities)
        if (std::find(wanted.begin(), wanted.end(), listed) != wanted.end())
            return true;
    return false;
}

bool LFGListMgr::Matches(LFGList::Listing const& listing, LFGList::SearchFilter const& filter)
{
    WorldPackets::LFGList::ListingDescriptor const& d = listing.Descriptor;

    // 68974 capture: the descriptor u32 @0x38 is the GroupFinderCategory id itself (JOIN carried 1 and the
    // search echoed 1 as Filters[0]); the selected GroupFinderActivity ids ride in the trailing vector.
    // The previous code looked the category value up in GroupFinderActivity.db2 and compared that entry's
    // GroupFinderCategoryID against the search category - that excluded every listing (empty browse pane).
    if (filter.CategoryId && d.CategoryID != filter.CategoryId)
        return false;

    // The advanced filter's activity GROUPS: resolve each id of the listing's activity vector in
    // GroupFinderActivity.db2 and match if ANY of them sits in one of the requested groups. Ids that do not
    // resolve are skipped rather than treated as a mismatch.
    if (!filter.ActivityGroupIds.empty())
    {
        bool inGroup = false;
        for (uint32 listedActivity : d.ActivityIDs)
            if (GroupFinderActivityEntry const* activity = sGroupFinderActivityStore.LookupEntry(listedActivity))
                inGroup = inGroup || std::find(filter.ActivityGroupIds.begin(), filter.ActivityGroupIds.end(),
                    uint32(activity->GroupFinderActivityGrpID)) != filter.ActivityGroupIds.end();
        if (!inGroup)
            return false;
    }

    // C_LFGList.Search's activityIDsFilter - an explicit narrowing the player asked for, so it ANDs.
    if (!OffersAnyActivity(d.ActivityIDs, filter.ActivityIds))
        return false;

    // The client's own resolution of the search box against GroupFinderActivity, ORed with the title match.
    // Both halves come out of the SAME typed words: the client turns them into an activity set (it alone has
    // the activity names) and the server matches them against the listing title (it alone has the titles).
    //
    // WHAT THE CLIENT ACTUALLY SENDS, decompiled in Runde 12 instead of inferred from the field list. The
    // resolver @ RVA 0x24E26D0 walks GroupFinderActivity and drops a record on two tests before it looks at
    // any name: `categoryOf(record) != CategoryID` and `(~mask(record) & Filter) != 0`, i.e. keep only what
    // carries ALL bits of Filter. It then emits an id at two places, and BOTH are gated on the term matcher
    // @ RVA 0x24E2630 - whose very first statement is `if (terms.begin() == terms.end()) return 0`.
    // So an empty search box yields an EMPTY ResolvedActivityIDs. The note that stood here claimed the
    // opposite ("the client sends every activity of the category"); it reached the right behaviour by a
    // route the client does not take, and Runde 12 called that out.
    //
    // Two consequences, and they decide the shape of this predicate:
    //   - ResolvedActivityIds is non-empty ONLY when the player typed something. It is never the lone
    //     carrier of a request, and the union below can never be reached with a wildcard on one side.
    //   - The Filter narrowing therefore does not reach the server at all when the box is empty, and this
    //     is a REAL GAP, stated here rather than smoothed over. mask() is the client's computed
    //     Enum.LFGListFilter mask (RVA 0x24DA960), built from the SEARCHING PLAYER's level against the
    //     activity's suggestion (Recommended 1 / NotRecommended 2), the record's own PvE 4 / PvP 8 /
    //     Timerunning 16 flag bits, an expansion test (CurrentExpansion 32) and membership in a client-side
    //     store (CurrentSeason 64 / NotCurrentSeason 128). The last three are not server state at all, and
    //     the first two need a level/suggestion model this unit's listing does not carry.
    //     The client does NOT make up for it either - that was checked rather than hoped. The UI displays
    //     C_LFGList.GetFilteredSearchResults() (LFGList.lua 12.1.0.69404,
    //     LFGListSearchPanel_UpdateResultList), whose implementation @ RVA 0x24E3A40 walks the received
    //     rows and drops exactly those whose key at row+0x10 is present in a client-side map
    //     (qword_7FF7853F17F0) - what fills that map was not identified. It re-applies neither the filter
    //     mask nor the category nor the activity set. So a search with a Filter and an empty box really does
    //     show the whole category, and closing it needs work outside this unit.
    //     Reproducing the mask PARTIALLY would be worse than not reproducing it: the test is
    //     `(mask & Filter) == Filter`, so a server mask that cannot carry Recommended would fail every
    //     record for the one Filter value actually measured (1, which ResolveCategoryFilters forces for the
    //     dungeon category) and empty the browse pane. Delivering too much is the recoverable error here.
    //
    // Hence: a request with BOTH halves empty is the wildcard - that is the search box left blank, and the
    // whole category is the honest answer to it. Anything else has to satisfy at least one half, and each
    // half only counts when it was actually asked for. That is the fix for Runde 12's second finding: a
    // request carrying resolved activities and no keywords now filters by them instead of riding in an OR
    // that an empty keyword set satisfies on its own.
    if (filter.Keywords.empty() && filter.ResolvedActivityIds.empty())
        return true;

    // Retail shows a group whose activity is what you typed even when its title says something else, and a
    // group whose title says what you typed even when its activity is called otherwise, so the union is the
    // only reading that does not silently drop one of the two halves.
    //
    // UNVERIFIED: that retail unions them rather than intersecting. Only the server can decide it and there
    // is no retail server to read; what IS measured is that the client sends both halves for one query
    // (69273_s69273_a_43003A_{0,1,2}.bin: Terms "the"/"nexus-captain" AND 17 resolved activity ids). The
    // union is also the non-regressive choice - intersecting would empty the browse pane for every listing
    // whose title does not repeat the dungeon name.
    bool const keywordHit = !filter.Keywords.empty() && MatchesKeywords(listing, filter.Keywords);
    bool const activityHit = !filter.ResolvedActivityIds.empty()
        && OffersAnyActivity(d.ActivityIDs, filter.ResolvedActivityIds);
    return keywordHit || activityHit;
}

std::vector<LFGList::Listing const*> LFGListMgr::Search(LFGList::SearchFilter const& filter) const
{
    uint32 const maxResults = uint32(sConfigMgr->GetIntDefault("LFGList.MaxSearchResults", 100));

    std::vector<LFGList::Listing const*> results;
    for (auto const& [id, listing] : _listings)
    {
        if (!Matches(listing, filter))
            continue;

        results.push_back(&listing);
        if (maxResults && results.size() >= maxResults)
            break;
    }
    return results;
}

// Fill the RideTicket that keys a listing on the client (sniff: type 4, Id = listing id, Time = post time).
// RequesterGuid is the listed PARTY, not the leader: decoding the 69273 SMSG_LFG_LIST_UPDATE_STATUS payloads
// gives a HighGuid::Party guid here that changes from one listing to the next, while the leader's
// HighGuid::Player guid stays constant and rides in the message's own optional field. A solo listing has no
// group, so the leader's own guid takes that place.
// It reads Listing::TicketGuid and NOT the live GroupGuid, and that distinction is the whole point: the
// choice is made once, in CreateListing, and frozen. Deriving it here from GroupGuid meant a solo listing
// re-keyed itself the moment an applicant's acceptance created the leader's group, and every later message
// about that listing - delist, expiry, application status, search-row update - was dropped unseen by the
// client, whose stored entry still carried the old guid.
// Every path that names a listing has to come through here. The client stores this ticket when the listing is
// created and compares the incoming one against it field by field before it will act on a delist; a path that
// builds the ticket its own way produces a message the client drops without a word.
void LFGListMgr::FillListingTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Listing const& listing)
{
    ticket.RequesterGuid = listing.TicketGuid;
    ticket.Id = listing.Id;
    ticket.Type = WorldPackets::LFG::RideType::LfgListListing;
    ticket.Time = int32(listing.CreatedTime);
    ticket.IsCrossFaction = false;
}

// Fill an application RideTicket (sniff: type 6, Id = application id, Time = apply time).
void LFGListMgr::FillApplicationTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Application const& app)
{
    ticket.RequesterGuid = app.ApplicantGuid;
    ticket.Id = app.Id;
    ticket.Type = WorldPackets::LFG::RideType::LfgListApplication;
    ticket.Time = int32(app.AppliedTime);
    ticket.IsCrossFaction = false;
}

// Wire state bits for an application state. The values are measured off the client's state->string mapper
// @ RVA 0x24DADA0 (see ApplicationStateBits) - the previous comment here claimed "sniff: 0x40 applied", for
// which no capture exists: there is no 12.1 recording of 0x5A000C, 0x5A000D or 0x5A000F at all. Nibble 4 is
// "cancelled", so 0x40 marked every fresh applicant as cancelled and the leader's Invite button, which the
// UI shows only for "applied" (LFGList.lua:1984-1985), never appeared.
// Every arm is now an exact hit rather than a collective fallback, so the default arm is a compile-safe
// floor instead of the code path every rejection took. Two honest gaps remain, both server-side and both
// recorded under D3 in the unit status file: ApplicationState::Cancelled is declared but never assigned
// anywhere in the tree, and the invite-decline path (HandleLFGListInviteResponse, !packet.Accept) deletes
// the application instead of moving it to a state, so the client never learns "invitedeclined" - the
// applicant simply vanishes from the leader's list.
uint8 LFGListMgr::ApplicationStateToBits(LFGList::ApplicationState state)
{
    switch (state)
    {
        case LFGList::ApplicationState::None:      return WorldPackets::LFGList::ApplicationStateBits::None;
        case LFGList::ApplicationState::Applied:   return WorldPackets::LFGList::ApplicationStateBits::Applied;
        case LFGList::ApplicationState::Invited:   return WorldPackets::LFGList::ApplicationStateBits::Invited;
        case LFGList::ApplicationState::Cancelled: return WorldPackets::LFGList::ApplicationStateBits::Cancelled;
        case LFGList::ApplicationState::Declined:  return WorldPackets::LFGList::ApplicationStateBits::Declined;
        case LFGList::ApplicationState::Accepted:  return WorldPackets::LFGList::ApplicationStateBits::InviteAccepted;
        default:                                   return WorldPackets::LFGList::ApplicationStateBits::None;
    }
}

// One applicant record as the leader's list must carry it - ticket, guid, real state and the note the
// applicant wrote. The comment is the reason this exists: the timeout sweep used to build the record by hand
// and omit it, so every sweep stripped the notes off the applicants that were still waiting.
void LFGListMgr::FillApplicantInfo(WorldPackets::LFGList::ApplicantInfo& info, LFGList::Application const& app)
{
    FillApplicationTicket(info.Ticket, app);
    info.PlayerGuid = app.ApplicantGuid;
    info.StateBits = ApplicationStateToBits(app.State);
    info.Comment = app.Comment;
}

// See the contract on the declaration for why this is the only producer of the message and why the
// recipient circle is the whole group rather than the leader.
void LFGListMgr::SendApplicantList(LFGList::Listing const& listing)
{
    WorldPackets::LFGList::LFGListApplicantListUpdate packet;
    FillListingTicket(packet.ListingTicket, listing);
    for (LFGList::Application const& app : listing.Applications)
        FillApplicantInfo(packet.Applicants.emplace_back(), app);
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

// See the contract on the declaration for why failureReason is a required argument and what it may hold.
// It goes into the message unchanged: on a failed state it IS the reason the client looks up, and on every
// other state it is the non-failure filler the capture carries. The 60 of the invite branch is the
// invite-response window, and none of the endings this serves has a window left to count down.
void LFGListMgr::SendApplicationStatusBits(LFGList::Listing const& listing, LFGList::Application const& app,
    uint8 stateBits, uint8 failureReason)
{
    Player* applicant = ObjectAccessor::FindConnectedPlayer(app.ApplicantGuid);
    if (!applicant)
        return;

    WorldPackets::LFGList::LFGListApplicationStatusUpdate packet;
    FillApplicationTicket(packet.Ticket, app);
    FillListingTicket(packet.ListingTicket, listing);
    packet.StateBits = stateBits;
    packet.UnkResult = failureReason;
    packet.RoleGranted = 0;
    applicant->SendDirectMessage(packet.Write());
}

void LFGListMgr::FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing) const
{
    // The frozen ticket guid, not the live GroupGuid: the row header IS a RideTicket (see operator<<
    // SearchResultListing), so a browser identifies the row by it. Same freeze, same reason as
    // FillListingTicket - otherwise the push that follows a group being formed reads as a different row.
    row.GroupGuid = listing.TicketGuid;
    row.ListingId = listing.Id;
    row.PostTime = listing.CreatedTime;
    row.LeaderGuid = listing.LeaderGuid;
    // UNVERIFIED: the row's Age field is served as a constant. 3 is what every row of the 12.0.7.68974
    // capture carried (some carried 4); there is no 12.1 recording of SMSG_LFG_LIST_SEARCH_RESULTS to
    // check it against, and no client consumer was found that distinguishes its values, so neither the
    // meaning of the field nor the right way to compute it is established. It is not a real age -
    // listing.CreatedTime already rides in PostTime one field earlier.
    row.Age = 3;
    row.Listing = GetPublicDescriptor(listing);

    row.Members.clear();
    auto addMember = [&row, &listing](ObjectGuid guid)
    {
        WorldPackets::LFGList::SearchResultMember& member = row.Members.emplace_back();
        member.Guid = guid;
        member.IsLeader = guid == listing.LeaderGuid;   // 68974: the head flag bit is set on the leader
        if (Player const* player = ObjectAccessor::FindConnectedPlayer(guid))
        {
            member.Level = uint8(player->GetLevel());
            member.ClassID = uint8(player->GetClass());
            member.SpecID = uint32(player->GetPrimarySpecialization());
            // 68974: third head byte is the spec role (Outlaw rogue = 2 dps, Brewmaster monk = 0 tank).
            if (ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry())
                member.Role = uint8(spec->Role);
        }
    };
    if (Group const* group = sGroupMgr->GetGroupByGUID(listing.GroupGuid))
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
            addMember(slot.guid);
    if (row.Members.empty())
        addMember(listing.LeaderGuid);                  // solo listing: the leader is the only member
}

WorldPackets::LFGList::ListingDescriptor LFGListMgr::GetPublicDescriptor(LFGList::Listing const& listing) const
{
    WorldPackets::LFGList::ListingDescriptor descriptor = listing.Descriptor;

    // The client zeroes the score block on the way in (both CreateListing and UpdateListing explicitly
    // clear it), so what a leader published carries nothing here and filling it is the server's job. This
    // is what puts a Mythic+ rating on the listing in the group browser.
    if (Player const* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid))
        descriptor.LeaderScore = *leader->m_playerData->DungeonScore;
    else
        descriptor.LeaderScore = { };

    if (listing.IsTextWithheld())
    {
        // Withhold the flagged text - but only while the flag is UNRESOLVED, which is exactly what this
        // function's contract in LFGListMgr.h says. The client expects the withholding: with the entry
        // flagged it draws CENSORED_LFG_GROUP_NAME in the header and CENSORED_LFG_GROUP_HEADER_WARNNG (sic)
        // instead of the description, in the search row, the context menu, the tooltip and the queue status
        // alike. A CONFIRMED listing gets its text back, and testing IsCensored() here instead was a defect:
        // the confirmation is deliberately never answered and never repeated (the wire can only carry 0 or
        // 1), so the client's local 2 is all that keeps the placeholder on screen. Lose it to a UI reload
        // and a still-withheld listing shows an empty title with nothing left to explain it.
        descriptor.Name.clear();
        descriptor.Comment.clear();
    }
    return descriptor;
}

// The listing-text word list, parsed from LFGList.CensorWords: comma separated, surrounding blanks
// trimmed, empty entries dropped, stored lower-cased. Re-parsed when the configured string changes so a
// `reload config` is picked up without a reload hook of this manager's own.
// This list is deliberately NOT the reserved_name table - see ContainsCensoredWord in LFGListMgr.h for
// why that table is the wrong policy in both directions.
std::vector<std::string> const& LFGListMgr::GetCensorWords()
{
    std::string configured = sConfigMgr->GetStringDefault("LFGList.CensorWords", "");
    if (_censorWordsLoaded && configured == _censorWordsConfig)
        return _censorWords;

    _censorWordsConfig = std::move(configured);
    _censorWordsLoaded = true;
    _censorWords.clear();
    for (std::string_view token : Trinity::Tokenize(_censorWordsConfig, ',', false))
    {
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())) != 0)
            token.remove_prefix(1);
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())) != 0)
            token.remove_suffix(1);
        if (token.empty())
            continue;

        std::string word(token);
        strToLower(word);
        _censorWords.push_back(std::move(word));
    }
    return _censorWords;
}

// A listing name or comment is free text, so the whole string is tested and so is every word in it.
// Tokenising on anything that is not a letter or digit also defeats the obvious "bad.word" / "bad-word"
// dodges. Matching is exact per word rather than substring on purpose: an admin-curated list of whole
// words must not turn every listing containing that sequence of letters inside a longer, harmless word
// into a flagged one.
// UNVERIFIED: retail's actual criteria. The wire only ever exposes a single "code" byte and the client
// does nothing with its value beyond testing it against zero, so no client evidence exists either way.
bool LFGListMgr::ContainsCensoredWord(std::string const& text)
{
    if (text.empty())
        return false;

    std::vector<std::string> const& words = GetCensorWords();
    if (words.empty())
        return false;

    auto matches = [&words](std::string_view candidate)
    {
        return std::any_of(words.begin(), words.end(),
            [candidate](std::string const& word) { return StringEqualI(candidate, word); });
    };

    if (matches(text))
        return true;

    std::size_t start = std::string::npos;
    for (std::size_t i = 0; i <= text.length(); ++i)
    {
        bool const isWordChar = i < text.length()
            && (std::isalnum(static_cast<unsigned char>(text[i])) != 0 || static_cast<unsigned char>(text[i]) >= 0x80);
        if (isWordChar)
        {
            if (start == std::string::npos)
                start = i;
            continue;
        }

        if (start != std::string::npos)
        {
            if (matches(std::string_view(text).substr(start, i - start)))
                return true;
            start = std::string::npos;
        }
    }
    return false;
}

bool LFGListMgr::EvaluateCensorship(LFGList::Listing& listing)
{
    LFGList::CensorState const previous = listing.Censor;
    uint8 const previousCode = listing.CensorCode;

    // Server-side text check against this system's OWN word list (LFGList.CensorWords), which ships empty.
    // With an empty list nothing is ever flagged and the censor pair stays silent - a deliberate off state,
    // recorded as such: SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE and
    // CMSG_LFG_LIST_CONFIRM_CENSORED_ACTIVE_ENTRY are carried as D2 "teil" in the unit status file for
    // exactly this reason, the same grade SMSG_SET_DF_FAST_LAUNCH_RESULT carries for the same situation.
    bool const flagged = ContainsCensoredWord(listing.Descriptor.Name)
        || ContainsCensoredWord(listing.Descriptor.Comment);

    if (flagged)
    {
        // Re-flagging an edited listing resets the decision: the player has to deal with it again. This runs
        // only on create and on an actual player edit, so a previous "confirmed" always refers to text that
        // no longer exists. Keeping the decision here would also desync the client: the update-request path
        // answers an edit with SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE, whose consumer can only store
        // 0 or 1, so the client lands on "unresolved" either way and the server has to agree.
        listing.Censor = LFGList::CensorState::Unresolved;
        listing.CensorCode = 1;
    }
    else
    {
        listing.Censor = LFGList::CensorState::None;
        listing.CensorCode = 0;
    }

    return listing.Censor != previous || listing.CensorCode != previousCode;
}

bool LFGListMgr::ConfirmCensoredListing(uint32 listingId, ObjectGuid leader)
{
    LFGList::Listing* listing = GetListing(listingId);
    if (!listing || listing->LeaderGuid != leader)
        return false;

    if (listing->Censor != LFGList::CensorState::Unresolved)
        return false;

    listing->Censor = LFGList::CensorState::Confirmed;
    return true;
}

void LFGListMgr::RegisterSearch(ObjectGuid player, LFGList::SearchFilter filter)
{
    SearchSubscription& sub = _searchSubscriptions[player];
    sub.Filter = std::move(filter);
    sub.ExpireTime = uint32(GameTime::GetGameTime()) + SEARCH_SUBSCRIPTION_TTL_SECONDS;
}

void LFGListMgr::UnregisterSearch(ObjectGuid player)
{
    _searchSubscriptions.erase(player);
}

void LFGListMgr::NotifyListingChanged(uint32 listingId)
{
    if (_searchSubscriptions.empty())
        return;

    LFGList::Listing const* listing = GetListing(listingId);
    if (!listing)
        return;

    // The filter test is LFGListMgr::Matches - the same call Search() makes, not a second copy of the
    // conditions - so a pushed row can never reach a browser whose filters would have excluded it from the
    // search reply. Keeping the two in step by hand is exactly what this used to get wrong.
    WorldPackets::LFGList::LFGListSearchResultsUpdate update;
    WorldPackets::LFGList::SearchResultListing row;
    FillSearchRow(row, *listing);
    update.Listings.push_back(std::move(row));
    WorldPacket const* packet = update.Write();

    uint32 const now = uint32(GameTime::GetGameTime());
    for (auto itr = _searchSubscriptions.begin(); itr != _searchSubscriptions.end(); )
    {
        SearchSubscription const& sub = itr->second;
        if (sub.ExpireTime && now >= sub.ExpireTime)
        {
            itr = _searchSubscriptions.erase(itr);
            continue;
        }

        Player* searcher = ObjectAccessor::FindConnectedPlayer(itr->first);
        if (!searcher)
        {
            itr = _searchSubscriptions.erase(itr);
            continue;
        }

        if (Matches(*listing, sub.Filter))
            searcher->SendDirectMessage(packet);

        ++itr;
    }
}

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

#ifndef LFGListMgr_h__
#define LFGListMgr_h__

#include "Define.h"
#include "ObjectGuid.h"
#include "LFGListPackets.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Player;

// Premade Group Finder (the "Premade Groups" tab). Server-side registry of player-published group listings + the
// application/invite flow. Ephemeral (no DB persistence — like the classic LFG queue); listings auto-expire.
// Distinct from the classic auto-matchmaking Dungeon Finder (LFGMgr / CMSG_DF_*), which TC already implements.
namespace LFGList
{
    enum class ApplicationState : uint8
    {
        None        = 0,
        Applied     = 1,
        Invited     = 2,
        Cancelled   = 3,
        Declined    = 4,
        Accepted    = 5,
    };

    // One applicant to a listing.
    struct Application
    {
        uint32 Id = 0;
        ObjectGuid ApplicantGuid;           // the applying player (or group leader)
        uint8 RoleMask = 0;                 // the role the applicant asked for
        // The role the LEADER assigned when inviting. CMSG_LFG_LIST_INVITE_APPLICANT carries an Invitees[]
        // list of { PackedGuid, u8 RoleMask } (client writer RVA 0x6A4A30) precisely so the leader can grant
        // a role other than the one applied for - a healer applicant slotted as damage, say. Zero until an
        // invite names it; SMSG_LFG_LIST_APPLICATION_STATUS_UPDATE.RoleGranted then carries this, not RoleMask.
        uint8 GrantedRoleMask = 0;
        uint32 SpecID = 0;
        uint32 ItemLevel = 0;
        std::string Comment;
        ApplicationState State = ApplicationState::Applied;
        uint32 AppliedTime = 0;             // drives the retail 300s application timeout (sniff-verified)
    };

    // How the player has dealt with a flagged (censored) listing. Mirrors the tri-state the client keeps
    // at LFG-list manager offset +0x13E0: 1 = flagged and undecided, 2 = the player confirmed it as is,
    // 0 = not flagged / cleared. C_LFGList.IsCensoredActiveEntryUnresolved tests for exactly the 1.
    enum class CensorState : uint8
    {
        None        = 0,
        Unresolved  = 1,
        Confirmed   = 2,
    };

    // The search terms of one CMSG_LFG_LIST_SEARCH, as the packet hands them over: one inner vector per
    // term block, holding that block's non-empty values. Blocks are ANDed, values inside a block ORed -
    // see LFGListSearch::GetKeywords, which owns that decision and marks it.
    using SearchKeywords = std::vector<std::vector<std::string>>;

    // One published group listing.
    struct Listing
    {
        uint32 Id = 0;                      // server-issued listing id (RideTicket.Id)
        ObjectGuid LeaderGuid;
        ObjectGuid GroupGuid;               // the leader's group (empty = solo listing)
        WorldPackets::LFGList::ListingDescriptor Descriptor;
        uint32 CreatedTime = 0;
        uint32 ExpireTime = 0;
        std::vector<Application> Applications;
        // Every client that has been handed this listing with Listed = true, and therefore holds an active
        // entry for it. The delist has to reach ALL of them, not just the leader: the consumer of
        // SMSG_LFG_LIST_UPDATE_STATUS (RVA 0x24DE410) creates the entry from any Listed payload whose ticket
        // it does not know yet, and an applicant who accepts an invite is sent exactly that (status 0x19).
        // Notifying only the leader left the joined member's group finder showing the entry until relog -
        // the very damage DelistAndNotify exists to prevent, one flank short. Maintained by
        // WorldSession::SendLFGListUpdateStatus, which is the only producer of a Listed payload.
        std::unordered_set<ObjectGuid> StatusRecipients;
        CensorState Censor = CensorState::None;
        uint8 CensorCode = 0;               // non-zero is what makes the client treat the entry as flagged

        uint32 GetCategoryID() const { return Descriptor.CategoryID; }
        // Flagged at all - this is what the wire carries, and what decides whether
        // SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE goes out with a code.
        bool IsCensored() const { return Censor != CensorState::None && CensorCode != 0; }
        // Flagged AND still undecided - this, and only this, is what withholds the listing's text.
        // Once the player has confirmed the listing (CensorState::Confirmed, set by
        // CMSG_LFG_LIST_CONFIRM_CENSORED_ACTIVE_ENTRY) the text goes out again. It has to: this wire can
        // only ever push the client's censor state to 0 or 1, so a confirmed listing is never re-announced,
        // and a client that lost its local 2 (any UI reload) would otherwise render an EMPTY title with no
        // dialog and no explanation left to account for it. Withholding past the confirmation buys nothing
        // and costs the owner their listing's name. See GetPublicDescriptor.
        bool IsTextWithheld() const { return Censor == CensorState::Unresolved && CensorCode != 0; }
    };
}

class TC_GAME_API LFGListMgr
{
public:
    static LFGListMgr& Instance();

    void Update(uint32 diff);               // expiration ticker

    // Publish/edit/delist. Returns the listing id (0 on failure).
    uint32 CreateListing(Player* leader, WorldPackets::LFGList::ListingDescriptor const& descriptor);
    bool UpdateListing(uint32 listingId, ObjectGuid leader, WorldPackets::LFGList::ListingDescriptor const& descriptor);
    void RemoveListing(uint32 listingId, ObjectGuid leader);
    void RemoveListingsBy(ObjectGuid leader); // logout cleanup
    // Delist AND tell everyone holding an active entry for the listing, in that order, from one place. The
    // notification has to be built while the listing still exists: the client only accepts a delist whose
    // ticket matches its stored active entry field for field (see FillListingTicket below), and a ticket
    // cannot be reconstructed once the listing is gone. Every server-initiated delist goes through here; a
    // hand-built "not listed" message is silently dropped by the client and leaves the entry standing in the
    // recipient's group finder.
    // "Everyone" is the leader plus Listing::StatusRecipients - an applicant who accepted an invite was sent
    // the listing with Listed = true (status 0x19) and holds an entry of its own. Addressing the leader alone
    // fixed the create/edit flank and left that one standing until relog.
    // `status` selects the client's popup (consumer RVA 0x24DE410): 0x2C too many players, 0x3B timeout,
    // 0x4B GameError 0x291, anything else no popup at all.
    void DelistAndNotify(uint32 listingId, ObjectGuid leader, uint8 status);

    LFGList::Listing* GetListing(uint32 listingId);
    LFGList::Listing const* GetListing(uint32 listingId) const;
    LFGList::Listing* GetListingByLeader(ObjectGuid leader);

    // Search the registry. Any argument left 0 acts as a wildcard, an empty keyword list matches every
    // listing. Results are capped by config.
    std::vector<LFGList::Listing const*> Search(uint32 category, uint32 activityGroup, uint32 activityId,
        LFGList::SearchKeywords const& keywords = LFGList::SearchKeywords()) const;

    // Does this listing satisfy the keyword part of a search? The ONE place that answers it, because the
    // search reply and the live push have to agree - and because it has to agree with what
    // GetPublicDescriptor actually delivers. A listing whose text is withheld is matched against an EMPTY
    // name, i.e. it is never a keyword hit: matching the STORED name would have let a searcher confirm a
    // word inside the withheld text from a result row that renders as CENSORED_LFG_GROUP_NAME, which is the
    // one thing withholding the text is supposed to prevent. Such a listing still shows up in an unfiltered
    // browse, exactly as in retail.
    static bool MatchesKeywords(LFGList::Listing const& listing, LFGList::SearchKeywords const& keywords);

    // Fills one search-result row for a listing. Shared by the search reply, the apply-result snapshot and the
    // live update push so all three serialize a listing identically.
    void FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing) const;

    // The two RideTickets of the listing system. These MUST be built in exactly one place: the client keys
    // its stored active entry on the whole 32-byte ticket and compares it field by field before it accepts a
    // delist (SMSG_LFG_LIST_UPDATE_STATUS consumer @ RVA 0x24DE410, six comparisons against LFG-list manager
    // +0..+24). A ticket assembled differently on one path than on another is silently ignored by the client.
    static void FillListingTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Listing const& listing);
    static void FillApplicationTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Application const& app);

    // The wire state bits of an application, and one whole applicant record of SMSG_LFG_LIST_APPLICANT_LIST_UPDATE.
    // Shared for the same reason as the tickets: the message has two producers (the handler's push and the
    // application-timeout sweep) and every field one of them forgets is a field the leader's applicant list loses.
    static uint8 ApplicationStateToBits(LFGList::ApplicationState state);
    static void FillApplicantInfo(WorldPackets::LFGList::ApplicantInfo& info, LFGList::Application const& app);

    // The descriptor as it must go out to a client. Identical to the stored one, except that a listing
    // whose text was flagged and not yet resolved goes out WITHOUT its name and comment: retail withholds
    // them (that is why the edit dialog has to ask the server via C_LFGList.DoesCensoredTextMatch instead
    // of comparing locally), and the client renders CENSORED_LFG_GROUP_NAME in their place.
    WorldPackets::LFGList::ListingDescriptor GetPublicDescriptor(LFGList::Listing const& listing) const;

    // Runs the listing's text through the server-side check and updates its censor state. Returns true if
    // the state changed, i.e. if SMSG_LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE has to go out.
    bool EvaluateCensorship(LFGList::Listing& listing);
    // CMSG_LFG_LIST_CONFIRM_CENSORED_ACTIVE_ENTRY: the player keeps the flagged listing as it is.
    bool ConfirmCensoredListing(uint32 listingId, ObjectGuid leader);

    // Live search updates. While a player has the Premade Groups browser open, retail keeps pushing
    // SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE as listings appear/change. A search registers the player's filters
    // here; listing mutations then push the affected row to every subscriber whose filters still match.
    void RegisterSearch(ObjectGuid player, uint32 category, uint32 activityGroup, LFGList::SearchKeywords keywords);
    void UnregisterSearch(ObjectGuid player);
    void NotifyListingChanged(uint32 listingId);

    // Applications. An application gets a globally-unique id the client keys on via a RideTicket.
    LFGList::Application* AddApplication(uint32 listingId, ObjectGuid applicant, uint8 roleMask, uint32 specId, uint32 itemLevel, std::string const& comment);
    LFGList::Listing* GetListingByApplication(uint32 applicationId);
    LFGList::Application* GetApplication(uint32 applicationId);
    bool SetApplicationState(uint32 applicationId, LFGList::ApplicationState state);
    // Records the role the leader granted in CMSG_LFG_LIST_INVITE_APPLICANT.Invitees[].
    bool SetApplicationGrantedRole(uint32 applicationId, uint8 roleMask);
    void RemoveApplication(uint32 applicationId);
    // Drops every application this player has outstanding (logout cleanup).
    void RemoveApplicationsBy(ObjectGuid applicant);
    // Refreshes the listing's expiry window (retail: activity extends the 30-minute lifetime).
    void TouchListing(LFGList::Listing& listing);

private:
    LFGListMgr() = default;

    // An open Premade Groups browser: the filters the player last searched with. 0 / empty = wildcard, matching
    // Search(). Refreshed by every search; expires so a client that closed the browser (there is no "stopped
    // searching" opcode) stops receiving pushes.
    struct SearchSubscription
    {
        uint32 CategoryId = 0;
        uint32 ActivityGroupId = 0;
        LFGList::SearchKeywords Keywords;
        uint32 ExpireTime = 0;
    };

    uint32 _nextListingId = 1;
    uint32 _nextApplicationId = 1;
    uint32 _expireTimer = 0;
    std::unordered_map<uint32 /*listingId*/, LFGList::Listing> _listings;
    std::unordered_map<ObjectGuid /*leader*/, uint32 /*listingId*/> _listingByLeader;
    std::unordered_map<uint32 /*applicationId*/, uint32 /*listingId*/> _applicationIndex;
    std::unordered_map<ObjectGuid /*searcher*/, SearchSubscription> _searchSubscriptions;
};

#define sLFGListMgr LFGListMgr::Instance()

#endif // LFGListMgr_h__

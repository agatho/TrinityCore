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
#include <unordered_map>
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
        uint8 RoleMask = 0;
        uint32 SpecID = 0;
        uint32 ItemLevel = 0;
        std::string Comment;
        ApplicationState State = ApplicationState::Applied;
        uint32 AppliedTime = 0;             // drives the retail 300s application timeout (sniff-verified)
    };

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

        uint32 GetActivityID() const { return Descriptor.ActivityID; }
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

    LFGList::Listing* GetListing(uint32 listingId);
    LFGList::Listing const* GetListing(uint32 listingId) const;
    LFGList::Listing* GetListingByLeader(ObjectGuid leader);

    // Search the registry. Any argument left 0 acts as a wildcard. Results are capped by config.
    std::vector<LFGList::Listing const*> Search(uint32 category, uint32 activityGroup, uint32 activityId, std::string const& keyword = std::string()) const;

    // Fills one search-result row for a listing (shared by the search reply and the live update push).
    void FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing) const;

    // Live search updates. While a player has the Premade Groups browser open, retail keeps pushing
    // SMSG_LFG_LIST_SEARCH_RESULTS_UPDATE as listings appear/change. A search registers the player's
    // filters here; listing mutations then push the affected row to every matching subscriber.
    void RegisterSearch(ObjectGuid player, uint8 category, uint8 activityGroup);
    void UnregisterSearch(ObjectGuid player);
    void NotifyListingChanged(uint32 listingId);

    // Applications. An application gets a globally-unique id the client keys on via a RideTicket.
    LFGList::Application* AddApplication(uint32 listingId, ObjectGuid applicant, uint8 roleMask, uint32 specId, uint32 itemLevel, std::string const& comment);
    LFGList::Listing* GetListingByApplication(uint32 applicationId);
    LFGList::Application* GetApplication(uint32 applicationId);
    bool SetApplicationState(uint32 applicationId, LFGList::ApplicationState state);
    void RemoveApplication(uint32 applicationId);
    // Drops every application this player has outstanding (logout cleanup).
    void RemoveApplicationsBy(ObjectGuid applicant);
    // Refreshes the listing's expiry window (retail: activity extends the 30-minute lifetime).
    void TouchListing(LFGList::Listing& listing);

private:
    LFGListMgr() = default;

    // An open Premade Groups browser: the filters the player last searched with. 0 = wildcard, matching
    // Search(). Refreshed by every search; expires so a client that closed the browser (there is no
    // "stopped searching" opcode) stops receiving pushes.
    struct SearchSubscription
    {
        uint8 CategoryId = 0;
        uint8 ActivityGroupId = 0;
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

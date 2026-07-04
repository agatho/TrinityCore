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

    // Search the registry for listings matching an activity (0 = any).
    std::vector<LFGList::Listing const*> Search(uint32 activityId) const;

private:
    LFGListMgr() = default;

    uint32 _nextListingId = 1;
    uint32 _expireTimer = 0;
    std::unordered_map<uint32 /*listingId*/, LFGList::Listing> _listings;
    std::unordered_map<ObjectGuid /*leader*/, uint32 /*listingId*/> _listingByLeader;
};

#define sLFGListMgr LFGListMgr::Instance()

#endif // LFGListMgr_h__

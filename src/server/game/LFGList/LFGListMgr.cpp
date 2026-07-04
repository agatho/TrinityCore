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
#include "GameTime.h"
#include "LFGListPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Group.h"

namespace
{
    constexpr uint32 EXPIRE_CHECK_INTERVAL_MS = 10 * IN_MILLISECONDS;
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
    for (auto itr = _listings.begin(); itr != _listings.end(); )
    {
        LFGList::Listing const& listing = itr->second;
        if (listing.ExpireTime && now >= listing.ExpireTime)
        {
            // Tell the leader (if online) the listing expired and is no longer listed.
            if (Player* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid))
            {
                WorldPackets::LFGList::LFGListUpdateExpiration expiration;
                expiration.Ticket.RequesterGuid = listing.LeaderGuid;
                expiration.Ticket.Id = listing.Id;
                expiration.Ticket.Type = WorldPackets::LFG::RideType::Lfg;
                leader->SendDirectMessage(expiration.Write());

                WorldPackets::LFGList::LFGListUpdateStatus status;
                status.Ticket = expiration.Ticket;
                status.Listed = false;
                leader->SendDirectMessage(status.Write());
            }

            for (LFGList::Application const& app : listing.Applications)
                _applicationIndex.erase(app.Id);
            _listingByLeader.erase(listing.LeaderGuid);
            itr = _listings.erase(itr);
        }
        else
            ++itr;
    }
}

uint32 LFGListMgr::CreateListing(Player* leader, WorldPackets::LFGList::ListingDescriptor const& descriptor)
{
    if (!leader)
        return 0;

    // One active listing per leader; re-publishing replaces the old one.
    RemoveListingsBy(leader->GetGUID());

    uint32 const expireMinutes = uint32(sConfigMgr->GetIntDefault("LFGList.ListingExpiryMinutes", 120));

    uint32 const id = _nextListingId++;
    LFGList::Listing& listing = _listings[id];
    listing.Id = id;
    listing.LeaderGuid = leader->GetGUID();
    if (Group* group = leader->GetGroup())
        listing.GroupGuid = group->GetGUID();
    listing.Descriptor = descriptor;
    listing.CreatedTime = GameTime::GetGameTime();
    listing.ExpireTime = expireMinutes ? listing.CreatedTime + expireMinutes * MINUTE : 0;

    _listingByLeader[leader->GetGUID()] = id;
    return id;
}

bool LFGListMgr::UpdateListing(uint32 listingId, ObjectGuid leader, WorldPackets::LFGList::ListingDescriptor const& descriptor)
{
    LFGList::Listing* listing = GetListing(listingId);
    if (!listing || listing->LeaderGuid != leader)
        return false;

    listing->Descriptor = descriptor;
    return true;
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

void LFGListMgr::RemoveListingsBy(ObjectGuid leader)
{
    auto itr = _listingByLeader.find(leader);
    if (itr == _listingByLeader.end())
        return;

    if (LFGList::Listing const* listing = GetListing(itr->second))
        for (LFGList::Application const& app : listing->Applications)
            _applicationIndex.erase(app.Id);
    _listings.erase(itr->second);
    _listingByLeader.erase(itr);
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
    listing->Applications.push_back(app);
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

LFGList::Listing const* LFGListMgr::GetListing(uint32 listingId) const
{
    auto itr = _listings.find(listingId);
    return itr != _listings.end() ? &itr->second : nullptr;
}

std::vector<LFGList::Listing const*> LFGListMgr::Search(uint8 category, uint8 activityGroup, uint32 activityId) const
{
    uint32 const maxResults = uint32(sConfigMgr->GetIntDefault("LFGList.MaxSearchResults", 100));

    std::vector<LFGList::Listing const*> results;
    for (auto const& [id, listing] : _listings)
    {
        WorldPackets::LFGList::ListingDescriptor const& d = listing.Descriptor;
        if (category && d.ActivityGroupCategory != category)
            continue;
        if (activityGroup && d.ActivityGroupId != activityGroup)
            continue;
        if (activityId && d.ActivityID != activityId)
            continue;

        results.push_back(&listing);
        if (maxResults && results.size() >= maxResults)
            break;
    }
    return results;
}

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
#include "DB2Stores.h"
#include "GameTime.h"
#include "LFGListPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Group.h"
#include "GroupMgr.h"

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
                        statusUpdate.Ticket.RequesterGuid = appItr->ApplicantGuid;
                        statusUpdate.Ticket.Id = appItr->Id;
                        statusUpdate.Ticket.Type = WorldPackets::LFG::RideType::LfgListApplication;
                        statusUpdate.Ticket.Time = int32(appItr->AppliedTime);
                        statusUpdate.ListingTicket.RequesterGuid = listing.LeaderGuid;
                        statusUpdate.ListingTicket.Id = listing.Id;
                        statusUpdate.ListingTicket.Type = WorldPackets::LFG::RideType::LfgListListing;
                        statusUpdate.ListingTicket.Time = int32(listing.CreatedTime);
                        statusUpdate.UnkResult = 8;
                        statusUpdate.StateBits = WorldPackets::LFGList::ApplicationStateBits::Declined;
                        applicant->SendDirectMessage(statusUpdate.Write());
                    }
                    _applicationIndex.erase(appItr->Id);
                    appItr = listing.Applications.erase(appItr);
                    changed = true;
                }
                else
                    ++appItr;
            }

            if (changed)
            {
                if (Player* leader = ObjectAccessor::FindConnectedPlayer(listing.LeaderGuid))
                {
                    WorldPackets::LFGList::LFGListApplicantListUpdate applicantList;
                    applicantList.ListingTicket.RequesterGuid = listing.LeaderGuid;
                    applicantList.ListingTicket.Id = listing.Id;
                    applicantList.ListingTicket.Type = WorldPackets::LFG::RideType::LfgListListing;
                    applicantList.ListingTicket.Time = int32(listing.CreatedTime);
                    for (LFGList::Application const& app : listing.Applications)
                    {
                        WorldPackets::LFGList::ApplicantInfo& info = applicantList.Applicants.emplace_back();
                        info.Ticket.RequesterGuid = app.ApplicantGuid;
                        info.Ticket.Id = app.Id;
                        info.Ticket.Type = WorldPackets::LFG::RideType::LfgListApplication;
                        info.Ticket.Time = int32(app.AppliedTime);
                        info.PlayerGuid = app.ApplicantGuid;
                        info.StateBits = WorldPackets::LFGList::ApplicationStateBits::Applied;
                    }
                    leader->SendDirectMessage(applicantList.Write());
                }
            }
        }
    }

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

    uint32 const expireMinutes = uint32(sConfigMgr->GetIntDefault("LFGList.ListingExpiryMinutes", 30)); // sniff: 1800s, activity-refreshed

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

void LFGListMgr::RemoveApplicationsBy(ObjectGuid applicant)
{
    for (auto& [listingId, listing] : _listings)
    {
        for (auto itr = listing.Applications.begin(); itr != listing.Applications.end(); )
        {
            if (itr->ApplicantGuid == applicant)
            {
                _applicationIndex.erase(itr->Id);
                itr = listing.Applications.erase(itr);
            }
            else
                ++itr;
        }
    }
}

LFGList::Listing const* LFGListMgr::GetListing(uint32 listingId) const
{
    auto itr = _listings.find(listingId);
    return itr != _listings.end() ? &itr->second : nullptr;
}

std::vector<LFGList::Listing const*> LFGListMgr::Search(uint32 category, uint32 activityGroup, uint32 activityId, std::string const& keyword) const
{
    uint32 const maxResults = uint32(sConfigMgr->GetIntDefault("LFGList.MaxSearchResults", 100));

    std::vector<LFGList::Listing const*> results;
    for (auto const& [id, listing] : _listings)
    {
        WorldPackets::LFGList::ListingDescriptor const& d = listing.Descriptor;
        // Category + activity group are derived from the listing's activity (GroupFinderActivity.db2), which is the
        // authoritative source — the descriptor carries the ActivityID, not the group ids.
        GroupFinderActivityEntry const* activity = sGroupFinderActivityStore.LookupEntry(d.ActivityID);
        if (category && (!activity || uint32(activity->GroupFinderCategoryID) != category))
            continue;
        if (activityGroup && (!activity || uint32(activity->GroupFinderActivityGrpID) != activityGroup))
            continue;
        if (activityId && d.ActivityID != activityId)
            continue;

        // Keyword search matches the listing title (case-insensitive substring, retail behaviour).
        if (!keyword.empty() && !StringContainsStringI(d.Name, keyword))
            continue;

        results.push_back(&listing);
        if (maxResults && results.size() >= maxResults)
            break;
    }
    return results;
}

void LFGListMgr::FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing) const
{
    row.GroupGuid = !listing.GroupGuid.IsEmpty() ? listing.GroupGuid : listing.LeaderGuid;
    row.ListingId = listing.Id;
    row.PostTime = listing.CreatedTime;
    row.LeaderGuid = listing.LeaderGuid;
    row.RawDescriptor = listing.Descriptor.RawBytes;    // verbatim echo of the client's descriptor bytes

    row.Members.clear();
    auto addMember = [&row](ObjectGuid guid)
    {
        WorldPackets::LFGList::SearchResultMember& member = row.Members.emplace_back();
        member.Guid = guid;
        if (Player const* player = ObjectAccessor::FindConnectedPlayer(guid))
        {
            member.Level = uint8(player->GetLevel());
            member.ClassID = uint8(player->GetClass());
            member.SpecID = uint32(player->GetPrimarySpecialization());
        }
    };
    if (Group const* group = sGroupMgr->GetGroupByGUID(listing.GroupGuid))
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
            addMember(slot.guid);
    if (row.Members.empty())
        addMember(listing.LeaderGuid);                  // solo listing: the leader is the only member
}

void LFGListMgr::RegisterSearch(ObjectGuid player, uint8 category, uint8 activityGroup)
{
    SearchSubscription& sub = _searchSubscriptions[player];
    sub.CategoryId = category;
    sub.ActivityGroupId = activityGroup;
    sub.ExpireTime = GameTime::GetGameTime() + SEARCH_SUBSCRIPTION_TTL_SECONDS;
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

    // Category/activity group come from GroupFinderActivity.db2 - the same authority Search() uses, so a
    // pushed row can never reach a browser whose filters would have excluded it.
    GroupFinderActivityEntry const* activity = sGroupFinderActivityStore.LookupEntry(listing->GetActivityID());

    WorldPackets::LFGList::LFGListSearchResultsUpdate update;
    WorldPackets::LFGList::SearchResultListing row;
    FillSearchRow(row, *listing);
    update.Listings.push_back(std::move(row));
    WorldPacket const* packet = update.Write();

    uint32 const now = GameTime::GetGameTime();
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

        bool const matches =
            (!sub.CategoryId || (activity && activity->GroupFinderCategoryID == sub.CategoryId)) &&
            (!sub.ActivityGroupId || (activity && activity->GroupFinderActivityGrpID == sub.ActivityGroupId));
        if (matches)
            searcher->SendDirectMessage(packet);

        ++itr;
    }
}

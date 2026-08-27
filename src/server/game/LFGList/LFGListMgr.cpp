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
#include "ObjectMgr.h"
#include "Player.h"
#include "Group.h"
#include "GroupMgr.h"
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
                    FillListingTicket(applicantList.ListingTicket, listing);
                    for (LFGList::Application const& app : listing.Applications)
                        FillApplicantInfo(applicantList.Applicants.emplace_back(), app);
                    leader->SendDirectMessage(applicantList.Write());
                }
            }
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

    if (Player* leaderPlayer = ObjectAccessor::FindConnectedPlayer(leader))
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
        leaderPlayer->SendDirectMessage(packet.Write());
    }

    RemoveListing(listingId, leader);
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
        // 68974 capture: the descriptor u32 @0x38 is the GroupFinderCategory id itself (JOIN carried 1 and the
        // search echoed 1 as Filters[0]); the selected GroupFinderActivity ids ride in the trailing vector.
        // The previous code looked the category value up in GroupFinderActivity.db2 and compared that entry's
        // GroupFinderCategoryID against the search category — that excluded every listing (empty browse pane).
        if (category && d.CategoryID != category)
            continue;
        if (activityGroup)
        {
            bool inGroup = false;
            for (uint32 listedActivity : d.ActivityIDs)
                if (GroupFinderActivityEntry const* activity = sGroupFinderActivityStore.LookupEntry(listedActivity))
                    inGroup = inGroup || uint32(activity->GroupFinderActivityGrpID) == activityGroup;
            if (!inGroup)
                continue;
        }
        if (activityId && std::find(d.ActivityIDs.begin(), d.ActivityIDs.end(), activityId) == d.ActivityIDs.end())
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

// Fill the RideTicket that keys a listing on the client (sniff: type 4, Id = listing id, Time = post time).
// RequesterGuid is the listed PARTY, not the leader: decoding the 69273 SMSG_LFG_LIST_UPDATE_STATUS payloads
// gives a HighGuid::Party guid here that changes from one listing to the next, while the leader's
// HighGuid::Player guid stays constant and rides in the message's own optional field. A solo listing has no
// group yet, so fall back to the leader.
// Every path that names a listing has to come through here. The client stores this ticket when the listing is
// created and compares the incoming one against it field by field before it will act on a delist; a path that
// builds the ticket its own way produces a message the client drops without a word.
void LFGListMgr::FillListingTicket(WorldPackets::LFG::RideTicket& ticket, LFGList::Listing const& listing)
{
    ticket.RequesterGuid = !listing.GroupGuid.IsEmpty() ? listing.GroupGuid : listing.LeaderGuid;
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

// Wire state bits for an application state (sniff: 0x40 applied, 0x20 invited, 0xA0 accepted).
uint8 LFGListMgr::ApplicationStateToBits(LFGList::ApplicationState state)
{
    switch (state)
    {
        case LFGList::ApplicationState::Applied:  return WorldPackets::LFGList::ApplicationStateBits::Applied;
        case LFGList::ApplicationState::Invited:  return WorldPackets::LFGList::ApplicationStateBits::Invited;
        case LFGList::ApplicationState::Accepted: return WorldPackets::LFGList::ApplicationStateBits::Accepted;
        default:                                  return WorldPackets::LFGList::ApplicationStateBits::Declined;
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

void LFGListMgr::FillSearchRow(WorldPackets::LFGList::SearchResultListing& row, LFGList::Listing const& listing) const
{
    row.GroupGuid = !listing.GroupGuid.IsEmpty() ? listing.GroupGuid : listing.LeaderGuid;
    row.ListingId = listing.Id;
    row.PostTime = listing.CreatedTime;
    row.LeaderGuid = listing.LeaderGuid;
    row.Age = 3;                                        // 68974: every observed row carried 3 or 4
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

namespace
{
    // A listing name or comment is free text, while ObjectMgr::IsReservedName is an EXACT lookup built for
    // single character names - handing it a whole sentence would make the check unreachable in practice.
    // So test the whole string and every word in it. Tokenising on anything that is not a letter or digit
    // also defeats the obvious "bad.word" / "bad-word" dodges.
    bool ContainsReservedWord(std::string const& text)
    {
        if (text.empty())
            return false;

        if (sObjectMgr->IsReservedName(text))
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
                if (sObjectMgr->IsReservedName(std::string_view(text).substr(start, i - start)))
                    return true;
                start = std::string::npos;
            }
        }
        return false;
    }
}

bool LFGListMgr::EvaluateCensorship(LFGList::Listing& listing)
{
    LFGList::CensorState const previous = listing.Censor;
    uint8 const previousCode = listing.CensorCode;

    // Server-side text check. TrinityCore has no listing-text blacklist of its own; the reserved-name
    // table is the only curated word list in the tree, and it is already the one an admin fills to keep
    // words off the realm, so that is what this applies.
    // UNVERIFIED: retail's actual criteria. The wire only ever exposes a single "code" byte and the client
    // does nothing with its value beyond testing it against zero, so no client evidence exists either way.
    bool const flagged = ContainsReservedWord(listing.Descriptor.Name)
        || ContainsReservedWord(listing.Descriptor.Comment);

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

void LFGListMgr::RegisterSearch(ObjectGuid player, uint32 category, uint32 activityGroup, std::string const& keyword)
{
    SearchSubscription& sub = _searchSubscriptions[player];
    sub.CategoryId = category;
    sub.ActivityGroupId = activityGroup;
    sub.Keyword = keyword;
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

    WorldPackets::LFGList::ListingDescriptor const& d = listing->Descriptor;

    // The filter test below mirrors Search() field for field, so a pushed row can never reach a browser whose
    // filters would have excluded it from the search reply:
    //  - category: the descriptor u32 @0x38 IS the GroupFinderCategory id, compared directly. It must NOT be
    //    looked up in GroupFinderActivity.db2 (that was the empty-browse-pane bug fixed by the 68974 pass).
    //  - activity group: resolve each id of the descriptor's trailing activity vector in GroupFinderActivity.db2
    //    and match if ANY of them sits in the requested group. Unresolvable ids are skipped, as in Search().
    //  - keyword: case-insensitive substring of the listing title, as in Search().
    std::vector<uint32> listingActivityGroups;
    listingActivityGroups.reserve(d.ActivityIDs.size());
    for (uint32 listedActivity : d.ActivityIDs)
        if (GroupFinderActivityEntry const* activity = sGroupFinderActivityStore.LookupEntry(listedActivity))
            listingActivityGroups.push_back(uint32(activity->GroupFinderActivityGrpID));

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

        bool const matches =
            (!sub.CategoryId || d.CategoryID == sub.CategoryId) &&
            (!sub.ActivityGroupId || std::find(listingActivityGroups.begin(), listingActivityGroups.end(), sub.ActivityGroupId) != listingActivityGroups.end()) &&
            (sub.Keyword.empty() || StringContainsStringI(d.Name, sub.Keyword));
        if (matches)
            searcher->SendDirectMessage(packet);

        ++itr;
    }
}

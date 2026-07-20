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

#include "ClubFinderPackets.h"
#include "ClubFinderMgr.h"
#include "CharacterCache.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"

// Club Finder P0: a guild advertises itself for recruitment.
//
// The client clamps Name to 96 and Description to 2048 before sending, and the wire encodes those
// lengths in 7 and 12 bits respectively, so anything that arrives is already within range - the checks
// below are defensive against a hand-crafted packet, not against the real client.
void WorldSession::HandleClubFinderPost(WorldPackets::ClubFinder::ClubFinderPost& clubFinderPost)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Failures are answered with SMSG_CLUB_FINDER_ERROR_MESSAGE. The 4-bit error selector maps 1:1 onto
    // the client's ERR_CLUB_FINDER_* strings (decompiled from handler sub_7FF72ACABB30), so these are
    // real, correctly-worded messages rather than a guessed code.
    auto sendError = [&](uint8 error)
    {
        WorldPackets::ClubFinder::ClubFinderErrorMessage errorMessage;
        errorMessage.Type = clubFinderPost.Type;
        errorMessage.Error = error;
        SendPacket(errorMessage.Write());
    };

    // Only a guild can be posted, and only by someone who speaks for it. The client's own UI gates the
    // button on guild permissions, so a failure here means the request did not come from that UI.
    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    if (!guild)
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} is not in a guild.", GetPlayerInfo());
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    if (clubFinderPost.ClubId != guild->GetId())
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} tried to post for club {} but is in guild {}.",
            GetPlayerInfo(), clubFinderPost.ClubId, guild->GetId());
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    if (guild->GetLeaderGUID() != player->GetGUID())
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} is not the leader of guild {}.",
            GetPlayerInfo(), guild->GetId());
        sendError(CLUB_FINDER_ERROR_NO_POSTING_PERMISSIONS);
        return;
    }

    ClubFinderPosting posting;
    posting.ClubId               = clubFinderPost.ClubId;
    posting.Name                 = clubFinderPost.Name;
    posting.Description          = clubFinderPost.Description;
    posting.RecruitingSpecs      = clubFinderPost.RecruitingSpecs;
    posting.RecruitmentFlags     = clubFinderPost.RecruitmentFlags;
    posting.ItemLevelRequirement = clubFinderPost.ItemLevelRequirement;
    posting.AvatarId             = clubFinderPost.AvatarId;
    posting.Type                 = clubFinderPost.Type;
    posting.CrossFaction         = clubFinderPost.CrossFaction;
    posting.LastPosterGUID       = player->GetGUID();

    ClubFinderPosting const* stored = sClubFinderMgr->SavePosting(std::move(posting));
    if (!stored)
    {
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    WorldPackets::ClubFinder::ClubFinderResponsePostRecruitmentMessage response;
    response.ClubFinderGUID = stored->GetClubFinderGUID();

    // The client's handler rejects anything but 0 or 1 here, raising ERR_CLUB_FINDER_ERROR_POST_CLUB
    // and discarding the update. 0 is the success path that closes the posting dialog.
    response.Result = CLUB_FINDER_POST_RESULT_OK;

    // The client parses this second field and never reads it again, so its value cannot affect
    // behaviour either way. Left at 0 rather than filled with a guess.
    response.Unused = 0;

    SendPacket(response.Write());

    TC_LOG_INFO("network", "ClubFinder: {} posted guild {} as posting {} (\"{}\").",
        GetPlayerInfo(), stored->ClubId, stored->PostingId, stored->Name);
}

// Fills one browse record from a stored posting. The counts and the leader name are read from the live
// guild rather than cached on the posting, so a browsing player sees the guild's real current state.
static bool BuildClubCacheData(ClubFinderPosting const& posting, WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList::ClubCacheData& data)
{
    Guild* guild = sGuildMgr->GetGuildById(posting.ClubId);
    if (!guild)
        return false;

    data.ClubName         = posting.Name;
    data.Comment          = posting.Description;
    data.ClubFinderGUID   = posting.GetClubFinderGUID();
    data.LastPosterGUID   = posting.LastPosterGUID;
    data.RecruitingSpecs  = posting.RecruitingSpecs;
    data.ClubID           = posting.ClubId;
    data.LastUpdatedTime  = posting.LastUpdatedTime;
    data.NumActiveMembers = guild->GetMembersCount();
    data.TabardInfo       = posting.AvatarId;
    data.RecruitmentFlags = int32(posting.RecruitmentFlags);
    data.MinIlvl          = int32(posting.ItemLevelRequirement);

    // The wire field is the guild leader's name; the posting only stores who last edited it.
    sCharacterCache->GetCharacterNameByGuid(guild->GetLeaderGUID(), data.GuildLeader);

    return true;
}

// The client asks which of the clubs it knows about currently have a posting.
void WorldSession::HandleClubFinderRequestSubscribedClubPostingIds(WorldPackets::ClubFinder::ClubFinderRequestSubscribedClubPostingIds& request)
{
    WorldPackets::ClubFinder::ClubFinderGetClubPostingIdsResponse response;

    for (uint64 clubId : request.ClubIds)
    {
        ClubFinderPosting const* posting = sClubFinderMgr->GetPostingForClub(clubId);
        if (!posting)
            continue;

        WorldPackets::ClubFinder::ClubFinderGetClubPostingIdsResponse::ClubPostingClubIDMap& entry = response.PostingIds.emplace_back();
        entry.ClubID = clubId;
        entry.ClubPostingID = posting->PostingId;

        // Real moderation state. The client decodes this as a mask of (1 << ClubFinderClubPostingStatusFlags)
        // in C_ClubFinder.GetStatusOfPostingFromClubId, and PostClub tests bits 2 and 3 of it to force a
        // description or name change before it will let the guild re-post.
        entry.PostingDisplayFlags = posting->DisplayFlags;
    }

    SendPacket(response.Write());
}

// The client asks for the full posting records behind a set of posting ids.
void WorldSession::HandleClubFinderRequestClubsData(WorldPackets::ClubFinder::ClubFinderRequestClubsData& request)
{
    WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList response;

    // The captured response echoes the request's type in the envelope's 3-bit field.
    response.Type = request.Type;

    for (uint32 clubPostingId : request.ClubPostingIDs)
    {
        ClubFinderPosting const* posting = sClubFinderMgr->GetPosting(clubPostingId);
        if (!posting)
            continue;

        WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList::ClubCacheData& data = response.Postings.emplace_back();
        if (!BuildClubCacheData(*posting, data))
            response.Postings.pop_back();
    }

    SendPacket(response.Write());
}

// Decodes the client's filter list into search criteria. Filter types 1, 2, 3 and 5 map directly onto
// data the posting carries; 4 (class) and 6 (locale) are parsed but not yet matched - see below.
static void ApplySearchFilters(std::vector<WorldPackets::ClubFinder::ClubFinderPostingFilter> const& filters,
    ClubFinderMgr::SearchCriteria& criteria)
{
    for (WorldPackets::ClubFinder::ClubFinderPostingFilter const& filter : filters)
    {
        switch (filter.Type)
        {
            case 1:     // focus flags, same bit space as the posting's recruitmentFlags
                criteria.FocusFlags = filter.UintValue;
                break;
            case 2:     // guild size flags, likewise
                criteria.SizeFlags = filter.UintValue;
                break;
            case 3:     // the searching player's average item level
                criteria.ItemLevel = filter.UintValue;
                break;
            case 5:     // specialization bitmask
                criteria.Specs = filter.Uint64Value;
                break;
            case 4:     // the searching player class id
                criteria.ClassId = uint8(filter.UintValue);
                break;
            case 6:     // applicant locale flags, a bitmask of (1 << WowLocale). The client applies no
                        // validation to this value, so it is masked to the legal locale set here.
                criteria.LocaleFlags = filter.UintValue & CLUB_FINDER_LOCALE_FLAGS_ALL;
                break;
            default:
                break;
        }
    }
}

// The Club Finder search: "show me guilds recruiting".
void WorldSession::HandleClubFinderRequestClubsList(WorldPackets::ClubFinder::ClubFinderRequestClubsList& request)
{
    ClubFinderMgr::SearchCriteria criteria;
    criteria.SearchString = request.SearchString;
    criteria.Type = request.Type;
    ApplySearchFilters(request.Filters, criteria);

    WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList response;
    response.Type = request.Type;

    for (ClubFinderPosting const* posting : sClubFinderMgr->Search(criteria))
    {
        WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList::ClubCacheData& data = response.Postings.emplace_back();
        if (!BuildClubCacheData(*posting, data))
            response.Postings.pop_back();
    }

    SendPacket(response.Write());
}

// ---------------------------------------------------------------------------------------------
// P2: applications
// ---------------------------------------------------------------------------------------------

// The posting a clubFinderGUID refers to: the posting id is the low 32 bits of the GUID high qword.
static ClubFinderPosting const* GetPostingFromGUID(ObjectGuid const& clubFinderGUID)
{
    return sClubFinderMgr->GetPosting(uint32(clubFinderGUID.GetRawValue(0) & 0xFFFFFFFF));
}

static void FillApplicationList(WorldPackets::ClubFinder::ClubFinderApplicationList& packet,
    std::vector<ClubFinderApplication const*> const& applications)
{
    for (ClubFinderApplication const* application : applications)
    {
        ClubFinderPosting const* posting = sClubFinderMgr->GetPosting(application->PostingId);
        if (!posting)
            continue;

        WorldPackets::ClubFinder::ClubFinderApplicationList::PendingApplication& entry = packet.Applications.emplace_back();
        entry.ClubFinderGUID    = posting->GetClubFinderGUID();
        entry.PlayerGUID        = application->PlayerGuid;
        entry.LastUpdatedTime   = application->LastUpdatedTime;
        entry.ApplicationStatus = application->Status;

        // closed marks an application that is no longer actionable.
        entry.Closed = (application->Status == CLUB_FINDER_APPLICATION_PENDING) ? 0 : 1;
    }
}

void WorldSession::SendClubFinderPendingApplications(uint8 type)
{
    WorldPackets::ClubFinder::ClubFinderApplicationList response(SMSG_CLUB_FINDER_RESPONSE_CHARACTER_APPLICATION_LIST);
    response.Type = type;
    FillApplicationList(response, sClubFinderMgr->GetApplicationsForPlayer(GetPlayer()->GetGUID()));
    SendPacket(response.Write());
}

// A player applies to a guild posting.
void WorldSession::HandleClubFinderRequestMembershipToClub(WorldPackets::ClubFinder::ClubFinderRequestMembershipToClub& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    auto sendError = [&](uint8 error)
    {
        WorldPackets::ClubFinder::ClubFinderErrorMessage errorMessage;
        errorMessage.Type = CLUB_FINDER_REQUEST_TYPE_GUILD;
        errorMessage.Error = error;
        SendPacket(errorMessage.Write());
    };

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    if (!posting)
    {
        sendError(CLUB_FINDER_ERROR_APPLY_CLUB);
        return;
    }

    // Applying to your own guild is meaningless, and a guild that stopped listing is not accepting.
    if (player->GetGuildId() == posting->ClubId || !(posting->RecruitmentFlags & CLUB_FINDER_SETTING_ENABLE_LISTING))
    {
        sendError(CLUB_FINDER_ERROR_APPLY_CLUB);
        return;
    }

    ClubFinderApplication application;
    application.PostingId  = posting->PostingId;
    application.PlayerGuid = player->GetGUID();
    application.Comment    = request.Comment.substr(0, 512);
    application.Specs      = request.RecruitingSpecs;

    // A guild that auto-accepts admits the applicant without the officer step.
    application.Status = (posting->RecruitmentFlags & CLUB_FINDER_SETTING_AUTO_ACCEPT)
        ? CLUB_FINDER_APPLICATION_AUTO_APPROVED : CLUB_FINDER_APPLICATION_PENDING;

    sClubFinderMgr->SaveApplication(std::move(application));

    // Echo the pending list back so the applicant UI reflects the new application.
    SendClubFinderPendingApplications(CLUB_FINDER_REQUEST_TYPE_GUILD);

    TC_LOG_INFO("network", "ClubFinder: {} applied to posting {} (guild {}).",
        GetPlayerInfo(), posting->PostingId, posting->ClubId);
}

// A guild officer asks for the applicants to their own posting. The request carries no club GUID, so
// the posting is resolved from the sender guild membership.
void WorldSession::HandleClubFinderGetApplicantsList(WorldPackets::ClubFinder::ClubFinderGetApplicantsList& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::ClubFinder::ClubFinderApplicationList response(SMSG_CLUB_FINDER_RESPONSE_CHARACTER_APPLICATION_LIST);
    response.Type = request.Type;

    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    ClubFinderPosting const* posting = guild ? sClubFinderMgr->GetPostingForClub(guild->GetId()) : nullptr;

    // No posting, or no authority over it: an empty list is the truthful answer, and never another
    // guild applicants.
    if (!posting || guild->GetLeaderGUID() != player->GetGUID())
    {
        SendPacket(response.Write());
        return;
    }

    FillApplicationList(response, sClubFinderMgr->GetApplicationsForPosting(posting->PostingId));
    SendPacket(response.Write());
}

void WorldSession::HandleClubFinderRequestPendingClubsList(WorldPackets::ClubFinder::ClubFinderRequestPendingClubsList& request)
{
    if (!GetPlayer())
        return;

    SendClubFinderPendingApplications(request.Type);
}

// A guild officer accepts or declines an applicant.
void WorldSession::HandleClubFinderRespondToApplicant(WorldPackets::ClubFinder::ClubFinderRespondToApplicant& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    auto sendError = [&](uint8 error)
    {
        WorldPackets::ClubFinder::ClubFinderErrorMessage errorMessage;
        errorMessage.Type = request.Type;
        errorMessage.Error = error;
        SendPacket(errorMessage.Write());
    };

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    Guild* guild = posting ? sGuildMgr->GetGuildById(posting->ClubId) : nullptr;
    if (!posting || !guild)
    {
        sendError(CLUB_FINDER_ERROR_RESPOND_APPLICANT);
        return;
    }

    if (guild->GetLeaderGUID() != player->GetGUID())
    {
        sendError(CLUB_FINDER_ERROR_NO_INVITE_PERMISSIONS);
        return;
    }

    ClubFinderApplication const* existing = sClubFinderMgr->GetApplication(posting->PostingId, request.PlayerGUID);
    if (!existing)
    {
        sendError(CLUB_FINDER_ERROR_RESPOND_APPLICANT);
        return;
    }

    ClubFinderApplication updated = *existing;
    updated.Status = request.ShouldAccept ? CLUB_FINDER_APPLICATION_APPROVED : CLUB_FINDER_APPLICATION_DECLINED;
    sClubFinderMgr->SaveApplication(std::move(updated));

    // Refresh the officer applicant list so the decision shows immediately.
    WorldPackets::ClubFinder::ClubFinderApplicationList response(SMSG_CLUB_FINDER_UPDATE_APPLICATIONS);
    response.Type = request.Type;
    FillApplicationList(response, sClubFinderMgr->GetApplicationsForPosting(posting->PostingId));
    SendPacket(response.Write());

    TC_LOG_INFO("network", "ClubFinder: {} responded to applicant {} for posting {} (accepted: {}).",
        GetPlayerInfo(), request.PlayerGUID.ToString(), posting->PostingId, request.ShouldAccept);
}

// The applicant accepts an invite, or withdraws. DeclineInvite is never emitted by the client - a
// declined invite arrives as Cancel - so both are handled as a withdrawal.
void WorldSession::HandleClubFinderApplicationResponse(WorldPackets::ClubFinder::ClubFinderApplicationResponse& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    if (!posting)
        return;

    ClubFinderApplication const* existing = sClubFinderMgr->GetApplication(posting->PostingId, player->GetGUID());
    if (!existing)
        return;

    ClubFinderApplication updated = *existing;
    switch (request.UpdateType)
    {
        case CLUB_FINDER_APPLICATION_UPDATE_ACCEPT_INVITE:
            updated.Status = CLUB_FINDER_APPLICATION_JOINED;
            break;
        case CLUB_FINDER_APPLICATION_UPDATE_DECLINE_INVITE:
        case CLUB_FINDER_APPLICATION_UPDATE_CANCEL:
            updated.Status = CLUB_FINDER_APPLICATION_CANCELED;
            break;
        default:
            return;
    }

    sClubFinderMgr->SaveApplication(std::move(updated));
    SendClubFinderPendingApplications(request.Type);
}

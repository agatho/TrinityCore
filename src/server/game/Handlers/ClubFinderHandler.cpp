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

        // postingDisplayFlags is 0 in the capture and its bit meanings are not derived, so it is left
        // at 0 rather than populated with a guess.
        entry.PostingDisplayFlags = 0;
    }

    SendPacket(response.Write());
}

// The client asks for the full posting records behind a set of posting ids.
void WorldSession::HandleClubFinderRequestClubsData(WorldPackets::ClubFinder::ClubFinderRequestClubsData& request)
{
    if (request.FilterCount)
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_REQUEST_CLUBS_DATA from {} carries {} filters, which are not parsed.",
            GetPlayerInfo(), request.FilterCount);

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

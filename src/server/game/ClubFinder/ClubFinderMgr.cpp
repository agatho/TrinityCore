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

#include "ClubFinderMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>

ObjectGuid ClubFinderPosting::GetClubFinderGUID() const
{
    return ObjectGuid::Create<HighGuid::ClubFinder>(Type, PostingId, PostingId);
}

ClubFinderMgr* ClubFinderMgr::instance()
{
    static ClubFinderMgr instance;
    return &instance;
}

void ClubFinderMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    _postings.clear();
    _postingsByClub.clear();
    _maxPostingId = 0;

    //                                                       0          1      2            3
    QueryResult result = CharacterDatabase.Query("SELECT postingId, clubId, name, description, "
    //   4                5                 6                     7         8      9             10
        "recruitingSpecs, recruitmentFlags, itemLevelRequirement, avatarId, type, crossFaction, lastPosterGuid, "
    //   11
        "lastUpdatedTime FROM club_finder_posting");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 club finder postings. The table is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ClubFinderPosting posting;
        posting.PostingId            = fields[0].GetUInt32();
        posting.ClubId               = fields[1].GetUInt64();
        posting.Name                 = fields[2].GetString();
        posting.Description          = fields[3].GetString();
        posting.RecruitingSpecs      = fields[4].GetUInt64();
        posting.RecruitmentFlags     = fields[5].GetUInt32();
        posting.ItemLevelRequirement = fields[6].GetUInt32();
        posting.AvatarId             = fields[7].GetUInt32();
        posting.Type                 = fields[8].GetUInt8();
        posting.CrossFaction         = fields[9].GetBool();
        posting.LastPosterGUID       = ObjectGuid::Create<HighGuid::Player>(fields[10].GetUInt64());
        posting.LastUpdatedTime      = fields[11].GetInt64();

        _maxPostingId = std::max(_maxPostingId, posting.PostingId);
        _postingsByClub[posting.ClubId] = posting.PostingId;
        _postings[posting.PostingId] = std::move(posting);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} club finder postings in {} ms", _postings.size(), GetMSTimeDiffToNow(oldMSTime));
}

ClubFinderPosting const* ClubFinderMgr::GetPosting(uint32 postingId) const
{
    auto itr = _postings.find(postingId);
    return itr != _postings.end() ? &itr->second : nullptr;
}

ClubFinderPosting const* ClubFinderMgr::GetPostingForClub(uint64 clubId) const
{
    auto itr = _postingsByClub.find(clubId);
    return itr != _postingsByClub.end() ? GetPosting(itr->second) : nullptr;
}

std::vector<ClubFinderPosting const*> ClubFinderMgr::GetAllPostings() const
{
    std::vector<ClubFinderPosting const*> postings;
    postings.reserve(_postings.size());
    for (auto const& [postingId, posting] : _postings)
        postings.push_back(&posting);

    return postings;
}

ClubFinderPosting const* ClubFinderMgr::SavePosting(ClubFinderPosting posting)
{
    // One posting per club: re-posting updates the existing entry rather than stacking duplicates,
    // which is what the client's single "post/update" button expects.
    if (ClubFinderPosting const* existing = GetPostingForClub(posting.ClubId))
        posting.PostingId = existing->PostingId;
    else
        posting.PostingId = ++_maxPostingId;

    posting.LastUpdatedTime = GameTime::GetGameTime();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CLUB_FINDER_POSTING);
    stmt->setUInt32(0, posting.PostingId);
    stmt->setUInt64(1, posting.ClubId);
    stmt->setString(2, posting.Name);
    stmt->setString(3, posting.Description);
    stmt->setUInt64(4, posting.RecruitingSpecs);
    stmt->setUInt32(5, posting.RecruitmentFlags);
    stmt->setUInt32(6, posting.ItemLevelRequirement);
    stmt->setUInt32(7, posting.AvatarId);
    stmt->setUInt8(8, posting.Type);
    stmt->setBool(9, posting.CrossFaction);
    stmt->setUInt64(10, posting.LastPosterGUID.GetCounter());
    stmt->setInt64(11, posting.LastUpdatedTime);
    CharacterDatabase.Execute(stmt);

    uint32 const postingId = posting.PostingId;
    uint64 const clubId = posting.ClubId;

    _postings[postingId] = std::move(posting);
    _postingsByClub[clubId] = postingId;

    return &_postings[postingId];
}

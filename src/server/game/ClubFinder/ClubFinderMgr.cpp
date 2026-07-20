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
#include "DB2Stores.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>
#include <cctype>

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
        "recruitingSpecs, recruitmentFlags, itemLevelRequirement, avatarId, displayFlags, type, crossFaction, lastPosterGuid, "
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
        posting.DisplayFlags         = fields[8].GetUInt32();
        posting.Type                 = fields[9].GetUInt8();
        posting.CrossFaction         = fields[10].GetBool();
        posting.LastPosterGUID       = ObjectGuid::Create<HighGuid::Player>(fields[11].GetUInt64());
        posting.LastUpdatedTime      = fields[12].GetInt64();

        _maxPostingId = std::max(_maxPostingId, posting.PostingId);
        _postingsByClub[posting.ClubId] = posting.PostingId;
        _postings[posting.PostingId] = std::move(posting);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} club finder postings in {} ms", _postings.size(), GetMSTimeDiffToNow(oldMSTime));

    LoadApplications();
    BuildSpecBitIndex();
}

void ClubFinderMgr::LoadApplications()
{
    _applications.clear();

    QueryResult result = CharacterDatabase.Query("SELECT postingId, playerGuid, comment, specs, status, lastUpdatedTime FROM club_finder_application");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        ClubFinderApplication application;
        application.PostingId       = fields[0].GetUInt32();
        application.PlayerGuid      = ObjectGuid::Create<HighGuid::Player>(fields[1].GetUInt64());
        application.Comment         = fields[2].GetString();
        application.Specs           = fields[3].GetUInt64();
        application.Status          = fields[4].GetUInt8();
        application.LastUpdatedTime = fields[5].GetInt64();

        _applications.push_back(std::move(application));
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} club finder applications", _applications.size());
}

// The client's bit index for a specialisation is its rank in the ascending list of every
// ChrSpecialization id whose ClassID is non-zero (builder 0x7FF72ACAB250: filter ClassID != 0, sort
// ascending, assign running index). Reproduced here so a class filter can be tested against the
// recruitingSpecs mask a client sent us.
void ClubFinderMgr::BuildSpecBitIndex()
{
    _specMaskByClass.clear();

    std::vector<ChrSpecializationEntry const*> specs;
    for (ChrSpecializationEntry const* spec : sChrSpecializationStore)
        if (spec->ClassID)
            specs.push_back(spec);

    std::sort(specs.begin(), specs.end(), [](ChrSpecializationEntry const* left, ChrSpecializationEntry const* right)
    {
        return left->ID < right->ID;
    });

    if (specs.size() > 64)
    {
        // The client shifts with `bts`, which masks the count to 63 and would silently alias specs
        // onto each other. Refuse to build a mapping we know is wrong rather than mismatch quietly.
        TC_LOG_ERROR("server.loading", "ClubFinder: {} class specialisations exceed the 64 bit recruiting mask; spec filters disabled.", specs.size());
        return;
    }

    for (std::size_t bitIndex = 0; bitIndex < specs.size(); ++bitIndex)
        _specMaskByClass[specs[bitIndex]->ClassID] |= UI64LIT(1) << bitIndex;
}

uint64 ClubFinderMgr::GetSpecMaskForClass(uint8 classId) const
{
    auto itr = _specMaskByClass.find(classId);
    return itr != _specMaskByClass.end() ? itr->second : UI64LIT(0);
}

std::vector<ClubFinderApplication const*> ClubFinderMgr::GetApplicationsForPosting(uint32 postingId) const
{
    std::vector<ClubFinderApplication const*> applications;
    for (ClubFinderApplication const& application : _applications)
        if (application.PostingId == postingId)
            applications.push_back(&application);

    return applications;
}

std::vector<ClubFinderApplication const*> ClubFinderMgr::GetApplicationsForPlayer(ObjectGuid playerGuid) const
{
    std::vector<ClubFinderApplication const*> applications;
    for (ClubFinderApplication const& application : _applications)
        if (application.PlayerGuid == playerGuid)
            applications.push_back(&application);

    return applications;
}

ClubFinderApplication const* ClubFinderMgr::GetApplication(uint32 postingId, ObjectGuid playerGuid) const
{
    for (ClubFinderApplication const& application : _applications)
        if (application.PostingId == postingId && application.PlayerGuid == playerGuid)
            return &application;

    return nullptr;
}

ClubFinderApplication const* ClubFinderMgr::SaveApplication(ClubFinderApplication application)
{
    application.LastUpdatedTime = GameTime::GetGameTime();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CLUB_FINDER_APPLICATION);
    stmt->setUInt32(0, application.PostingId);
    stmt->setUInt64(1, application.PlayerGuid.GetCounter());
    stmt->setString(2, application.Comment);
    stmt->setUInt64(3, application.Specs);
    stmt->setUInt8(4, application.Status);
    stmt->setInt64(5, application.LastUpdatedTime);
    CharacterDatabase.Execute(stmt);

    for (ClubFinderApplication& existing : _applications)
    {
        if (existing.PostingId == application.PostingId && existing.PlayerGuid == application.PlayerGuid)
        {
            existing = std::move(application);
            return &existing;
        }
    }

    _applications.push_back(std::move(application));
    return &_applications.back();
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
    {
        posting.PostingId = existing->PostingId;

        // Moderation state belongs to the posting, not to whoever last edited it, so a re-post must
        // not clear it.
        posting.DisplayFlags = existing->DisplayFlags;
    }
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
    stmt->setUInt32(8, posting.DisplayFlags);
    stmt->setUInt8(9, posting.Type);
    stmt->setBool(10, posting.CrossFaction);
    stmt->setUInt64(11, posting.LastPosterGUID.GetCounter());
    stmt->setInt64(12, posting.LastUpdatedTime);
    CharacterDatabase.Execute(stmt);

    uint32 const postingId = posting.PostingId;
    uint64 const clubId = posting.ClubId;

    _postings[postingId] = std::move(posting);
    _postingsByClub[clubId] = postingId;

    return &_postings[postingId];
}

std::vector<ClubFinderPosting const*> ClubFinderMgr::Search(SearchCriteria const& criteria) const
{
    std::string needle = criteria.SearchString;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    std::vector<ClubFinderPosting const*> results;
    for (auto const& [postingId, posting] : _postings)
    {
        if (criteria.Type != CLUB_FINDER_REQUEST_TYPE_ALL && posting.Type != criteria.Type)
            continue;

        // A delisted or removed posting must not surface in a search.
        if (posting.DisplayFlags & (CLUB_FINDER_POSTING_FLAG_POST_DELISTED | CLUB_FINDER_POSTING_FLAG_PENDING_DELETE | CLUB_FINDER_POSTING_FLAG_BANNED))
            continue;

        // A guild that has not enabled its listing is not advertising.
        if (!(posting.RecruitmentFlags & CLUB_FINDER_SETTING_ENABLE_LISTING))
            continue;

        // The posting only advertises to players who meet its own item level requirement.
        if (criteria.ItemLevel && posting.ItemLevelRequirement > criteria.ItemLevel)
            continue;

        // Focus and size filters live in the same ClubFinderSettingFlags bit space as the posting's
        // own recruitmentFlags, so they match directly: the guild must share at least one of the
        // requested focuses / sizes.
        if (criteria.FocusFlags && !(posting.RecruitmentFlags & criteria.FocusFlags & CLUB_FINDER_SETTING_MASK_FOCUS))
            continue;

        if (criteria.SizeFlags && !(posting.RecruitmentFlags & criteria.SizeFlags & CLUB_FINDER_SETTING_MASK_SIZE))
            continue;

        // A spec filter matches when the guild recruits at least one of the requested specs.
        if (criteria.Specs && posting.RecruitingSpecs && !(posting.RecruitingSpecs & criteria.Specs))
            continue;

        // A class filter matches when the guild recruits any specialisation of that class. The spec
        // mask uses the client bit ordering, rebuilt in BuildSpecBitIndex.
        if (criteria.ClassId && posting.RecruitingSpecs)
            if (uint64 classMask = GetSpecMaskForClass(criteria.ClassId); classMask && !(posting.RecruitingSpecs & classMask))
                continue;

        // Locale: the posting packs (locale + 1) into bits 21-25 of its flags, the applicant sends a
        // bitmask of (1 << locale). An unset posting locale (packed 0) or an empty applicant mask is
        // treated as "no constraint" - the client gives no evidence either way, so the permissive
        // reading is used rather than silently hiding postings.
        if (criteria.LocaleFlags)
        {
            uint32 const packedLocale = (posting.RecruitmentFlags >> CLUB_FINDER_LOCALE_SHIFT) & CLUB_FINDER_LOCALE_MASK;
            if (packedLocale)
            {
                uint32 const localeId = packedLocale - 1;
                // Bit 9 is a hole in the locale table and anything above 11 is unused.
                if (localeId == 9 || localeId > 11 || !((criteria.LocaleFlags >> localeId) & 1))
                    continue;
            }
        }

        if (!needle.empty())
        {
            std::string name = posting.Name;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (name.find(needle) == std::string::npos)
                continue;
        }

        results.push_back(&posting);
    }

    return results;
}

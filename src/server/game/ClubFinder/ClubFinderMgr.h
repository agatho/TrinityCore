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

#ifndef TRINITYCORE_CLUB_FINDER_MGR_H
#define TRINITYCORE_CLUB_FINDER_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

// ClubFinderRequestType. Every captured posting is type 3, which is also the value the client encodes
// into the clubFinderGUID's type field for guild postings - see c:/dumps/CLUB_FINDER_SCOPING_68275.md.
enum ClubFinderRequestType : uint8
{
    CLUB_FINDER_REQUEST_TYPE_NONE   = 0,
    CLUB_FINDER_REQUEST_TYPE_GUILD  = 3
};

// A guild's recruitment posting, as the client posts it via C_ClubFinder.PostClub.
struct ClubFinderPosting
{
    uint32 PostingId            = 0;
    uint64 ClubId               = 0;    // the guild id this posting advertises
    std::string Name;
    std::string Description;
    uint64 RecruitingSpecs      = 0;
    uint32 RecruitmentFlags     = 0;    // ClubFinderSettingFlags bit-index mask; locale in bits 21-25
    uint32 ItemLevelRequirement = 0;
    uint32 AvatarId             = 0;
    uint8 Type                  = CLUB_FINDER_REQUEST_TYPE_GUILD;
    bool CrossFaction           = false;
    ObjectGuid LastPosterGUID;
    time_t LastUpdatedTime      = 0;

    // The posting id is not a wire field of its own: the client reads it back out of the low 32 bits
    // of the clubFinderGUID's high qword. This mints the GUID the client expects.
    ObjectGuid GetClubFinderGUID() const;
};

// Registry of guild recruitment postings (Club Finder P0).
//
// Postings are guild-backed: one posting per guild, created and updated by CMSG_CLUB_FINDER_POST.
// Persisted in the character database because guilds live there.
class TC_GAME_API ClubFinderMgr
{
public:
    static ClubFinderMgr* instance();

    void Load();

    ClubFinderPosting const* GetPosting(uint32 postingId) const;
    ClubFinderPosting const* GetPostingForClub(uint64 clubId) const;

    // Creates or updates the posting for a club and persists it. Returns the stored posting.
    ClubFinderPosting const* SavePosting(ClubFinderPosting posting);

    // All currently listed postings, for the browse responses built on top of this in P1.
    std::vector<ClubFinderPosting const*> GetAllPostings() const;

private:
    ClubFinderMgr() = default;
    ~ClubFinderMgr() = default;
    ClubFinderMgr(ClubFinderMgr const&) = delete;
    ClubFinderMgr& operator=(ClubFinderMgr const&) = delete;

    std::unordered_map<uint32, ClubFinderPosting> _postings;    // by posting id
    std::unordered_map<uint64, uint32> _postingsByClub;         // club id -> posting id
    uint32 _maxPostingId = 0;
};

#define sClubFinderMgr ClubFinderMgr::instance()

#endif // TRINITYCORE_CLUB_FINDER_MGR_H

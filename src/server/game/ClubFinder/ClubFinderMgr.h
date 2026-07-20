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

// ClubFinderRequestType, from the client's own Lua API documentation. Carried as 3 bits on the wire.
enum ClubFinderRequestType : uint8
{
    CLUB_FINDER_REQUEST_TYPE_NONE      = 0,
    CLUB_FINDER_REQUEST_TYPE_GUILD     = 1,
    CLUB_FINDER_REQUEST_TYPE_COMMUNITY = 2,
    CLUB_FINDER_REQUEST_TYPE_ALL       = 3
};

// The 4-bit error selector of SMSG_CLUB_FINDER_ERROR_MESSAGE. Recovered by decompiling the client's
// handler sub_7FF72ACABB30, whose switch maps each value 1:1 onto an ERR_CLUB_FINDER_* global string.
// Values 12, 13 and 15 fall through to a no-op in the client.
enum ClubFinderErrorType : uint8
{
    CLUB_FINDER_ERROR_POST_CLUB                 = 0,
    CLUB_FINDER_ERROR_RESPOND_APPLICANT         = 1,    // client also re-requests the applicant list
    CLUB_FINDER_ERROR_APPLY_CLUB                = 2,
    CLUB_FINDER_ERROR_CANCEL_APPLICATION        = 3,    // client also re-requests the pending list
    CLUB_FINDER_ERROR_ACCEPT_APPLICATION        = 4,    // client also re-requests the pending list
    CLUB_FINDER_ERROR_NO_INVITE_PERMISSIONS     = 5,
    CLUB_FINDER_ERROR_NO_POSTING_PERMISSIONS    = 6,
    CLUB_FINDER_ERROR_APPLICANT_LIST            = 7,
    CLUB_FINDER_ERROR_APPLICANT_LIST_NO_PERM    = 8,
    CLUB_FINDER_ERROR_FINDER_NOT_AVAILABLE      = 9,
    CLUB_FINDER_ERROR_GET_POSTING_IDS           = 10,
    CLUB_FINDER_ERROR_JOIN_APPLICATION          = 11,
    CLUB_FINDER_ERROR_REALM_NOT_ELIGIBLE        = 14
};

// ClubFinderClubPostingStatusFlags. The wire field postingDisplayFlags is a mask of (1 << value):
// C_ClubFinder.GetStatusOfPostingFromClubId walks bits 1..8 and returns the set bit indices, and
// PostClub's validation tests the same stored u32 with & 4 and & 8 for the two forced-change flags.
enum ClubFinderPostingStatusFlag : uint32
{
    CLUB_FINDER_POSTING_FLAG_NEEDS_CACHE_UPDATE       = 1 << 1,
    CLUB_FINDER_POSTING_FLAG_FORCE_DESCRIPTION_CHANGE = 1 << 2,
    CLUB_FINDER_POSTING_FLAG_FORCE_NAME_CHANGE        = 1 << 3,
    CLUB_FINDER_POSTING_FLAG_UNDER_REVIEW             = 1 << 4,
    CLUB_FINDER_POSTING_FLAG_BANNED                   = 1 << 5,
    CLUB_FINDER_POSTING_FLAG_FAKE_POST                = 1 << 6,
    CLUB_FINDER_POSTING_FLAG_PENDING_DELETE           = 1 << 7,
    CLUB_FINDER_POSTING_FLAG_POST_DELISTED            = 1 << 8
};

// ClubFinderSettingFlags, recovered from the client's enum registrar. Both the posting's
// recruitmentFlags and the searcher's applicantSettings are bit-index masks over these values, which
// is why a focus or size filter can be matched directly against a posting's flags.
enum ClubFinderSettingFlag : uint32
{
    CLUB_FINDER_SETTING_DUNGEONS         = 1 << 1,
    CLUB_FINDER_SETTING_RAIDS            = 1 << 2,
    CLUB_FINDER_SETTING_PVP              = 1 << 3,
    CLUB_FINDER_SETTING_RP               = 1 << 4,
    CLUB_FINDER_SETTING_SOCIAL           = 1 << 5,
    CLUB_FINDER_SETTING_SMALL            = 1 << 6,
    CLUB_FINDER_SETTING_MEDIUM           = 1 << 7,
    CLUB_FINDER_SETTING_LARGE            = 1 << 8,
    CLUB_FINDER_SETTING_TANK             = 1 << 9,
    CLUB_FINDER_SETTING_HEALER           = 1 << 10,
    CLUB_FINDER_SETTING_DAMAGE           = 1 << 11,
    CLUB_FINDER_SETTING_ENABLE_LISTING   = 1 << 12,
    CLUB_FINDER_SETTING_MAX_LEVEL_ONLY   = 1 << 13,
    CLUB_FINDER_SETTING_AUTO_ACCEPT      = 1 << 14,
    CLUB_FINDER_SETTING_FACTION_HORDE    = 1 << 15,
    CLUB_FINDER_SETTING_FACTION_ALLIANCE = 1 << 16,
    CLUB_FINDER_SETTING_FACTION_NEUTRAL  = 1 << 17,

    // The masks the client itself slices out when building filters 1 and 2.
    CLUB_FINDER_SETTING_MASK_FOCUS       = 0x3E,    // Dungeons .. Social
    CLUB_FINDER_SETTING_MASK_SIZE        = 0x1C0    // Small / Medium / Large
};

// Locale is packed as (locale + 1) into bits 21-25 of a posting's recruitmentFlags.
constexpr uint32 CLUB_FINDER_LOCALE_SHIFT = 21;
constexpr uint32 CLUB_FINDER_LOCALE_MASK  = 0x3E00000;

// The client accepts 0 and 1 as success in the post response and treats everything else as a failure.
enum ClubFinderPostResult : uint8
{
    CLUB_FINDER_POST_RESULT_OK = 0
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
    uint32 DisplayFlags         = 0;    // mask of ClubFinderPostingStatusFlag; moderation state
    uint8 Type                  = CLUB_FINDER_REQUEST_TYPE_GUILD;
    bool CrossFaction           = false;
    ObjectGuid LastPosterGUID;
    time_t LastUpdatedTime      = 0;

    // The posting id is not a wire field of its own: the client reads it back out of the low 32 bits
    // of the clubFinderGUID's high qword. This mints the GUID the client expects.
    //
    // The GUID's type field is an intrinsic club type, NOT an echo of the request that produced it:
    // C_ClubFinder.GetClubTypeFromFinderGUID decodes it as `hi >> 33` and accepts only 1 (Guild) and
    // 2 (Community), returning nothing for anything else. Echoing a request type of All (3) would
    // therefore make the client's own getter fail.
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

    // The search criteria the client sends, already decoded out of its filter list.
    struct SearchCriteria
    {
        std::string SearchString;
        uint64 Specs        = 0;    // filter 5: recruiting-spec bitmask
        uint32 ItemLevel    = 0;    // filter 3: the searcher's average item level
        uint32 FocusFlags   = 0;    // filter 1: Dungeons / Raids / PvP / RP / Social
        uint32 SizeFlags    = 0;    // filter 2: Small / Medium / Large
        uint8 Type          = CLUB_FINDER_REQUEST_TYPE_ALL;
    };

    std::vector<ClubFinderPosting const*> Search(SearchCriteria const& criteria) const;

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

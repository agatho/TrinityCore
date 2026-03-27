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

#ifndef TRINITY_DELVE_MGR_H
#define TRINITY_DELVE_MGR_H

#include "Define.h"
#include "DelvesDefines.h"
#include <unordered_map>
#include <vector>

struct DelvesSeasonEntry;
struct PlayerCompanionInfoEntry;

namespace Delves
{

class TC_GAME_API DelveMgr
{
    DelveMgr();
    ~DelveMgr();

public:
    DelveMgr(DelveMgr const&) = delete;
    DelveMgr(DelveMgr&&) = delete;
    DelveMgr& operator=(DelveMgr const&) = delete;
    DelveMgr& operator=(DelveMgr&&) = delete;

    static DelveMgr* Instance();

    void Initialize();

    // Season
    uint32 GetActiveSeasonId() const { return _activeSeasonId; }
    DelvesSeasonEntry const* GetActiveSeason() const;

    // Templates
    DelveTemplate const* GetDelveTemplate(uint32 mapId) const;
    DelveTemplate const* GetDelveTemplateByChallengeModeId(uint32 mapChallengeModeId) const;
    std::vector<DelveTemplate> const& GetAllDelveTemplates() const { return _delveTemplatesList; }

    // Tier rewards
    DelveTierReward const* GetTierReward(uint8 tier) const;

    // Bountiful
    bool IsDelveCurrentlyBountiful(uint32 mapChallengeModeId) const;
    std::vector<uint32> GetTodaysBountifulDelves() const;

private:
    void LoadDelveTemplates();
    void LoadTierRewards();
    void DetermineActiveSeason();

    // Templates indexed by mapId
    std::unordered_map<uint32, DelveTemplate> _delveTemplatesByMap;
    // Templates indexed by MapChallengeModeId
    std::unordered_map<uint32, DelveTemplate const*> _delveTemplatesByChallengeModeId;
    // Ordered list of all templates
    std::vector<DelveTemplate> _delveTemplatesList;
    // Tier rewards
    std::unordered_map<uint8, DelveTierReward> _tierRewards;
    // Active season
    uint32 _activeSeasonId = 0;
};

} // namespace Delves

#define sDelveMgr Delves::DelveMgr::Instance()

#endif // TRINITY_DELVE_MGR_H

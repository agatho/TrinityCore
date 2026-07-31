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

#ifndef TRINITYCORE_WEEKLY_REWARDS_MGR_H
#define TRINITYCORE_WEEKLY_REWARDS_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <vector>
#include <array>
#include <unordered_map>

class Player;

namespace WeeklyRewards
{
    // The three Great Vault activity rows. Values match the client's WeeklyRewardChestThreshold activity grouping
    // (the exact DB2 activity-type ids are configured server-side; these are the internal row indices).
    enum class ActivityType : uint8
    {
        Dungeon = 0,    // Mythic+ / dungeon runs
        Raid    = 1,    // raid boss kills
        World   = 2,    // world / PvP activity

        Max
    };

    // The three reward slots per row unlock at these completion counts (retail Great Vault layout). A row with
    // >= thresholds[i] qualifying completions this week has earned reward slot i.
    inline constexpr std::array<uint32, 3> DUNGEON_THRESHOLDS = { 1, 4, 8 };
    inline constexpr std::array<uint32, 3> RAID_THRESHOLDS    = { 2, 4, 6 };
    inline constexpr std::array<uint32, 3> WORLD_THRESHOLDS   = { 1, 4, 8 };

    std::array<uint32, 3> const& ThresholdsFor(ActivityType type);

    // A character's accumulated activity for one row in the current week.
    struct ActivityRow
    {
        uint32 Count = 0;       // qualifying completions this period
        uint32 BestLevel = 0;   // best key level / difficulty / tier seen (kept for legacy rows / fallback)
        // Individual run levels this period, sorted high->low and capped at the highest slot threshold. Each Great
        // Vault slot rewards the level of the Nth-best run (N = the slot's threshold), so slot 2 (4 runs) uses
        // Levels[3], not the single BestLevel. Empty for legacy rows saved before this field existed.
        std::vector<uint32> Levels;
    };

    struct CharacterVault
    {
        uint32 Period = 0;                                  // week index this data belongs to
        std::array<ActivityRow, uint8(ActivityType::Max)> Rows = {};
        uint32 ClaimedPeriod = 0;                            // the last period the player claimed a reward
    };
}

class TC_GAME_API WeeklyRewardsMgr
{
public:
    static WeeklyRewardsMgr& Instance();

    // The current weekly-reward period (week index). Rolls over every reset; used to expire last week's activity.
    static uint32 GetCurrentPeriod();

    // Record one qualifying activity completion for a player (bumps the row count, tracks the best level). Rolls the
    // stored data to the current period first if it is stale.
    void RecordActivity(Player* player, WeeklyRewards::ActivityType type, uint32 level);

    WeeklyRewards::CharacterVault& GetVault(ObjectGuid guid);
    WeeklyRewards::CharacterVault const* FindVault(ObjectGuid guid) const;

    // Whether the player still has an unclaimed reward this period (any row reached its first threshold).
    bool HasUnclaimedReward(ObjectGuid guid);

    // Mark this period claimed for the player (persisted). Returns false if already claimed / nothing to claim.
    bool MarkClaimed(ObjectGuid guid);

    void LoadCharacter(ObjectGuid guid);        // lazy load on demand
    void SaveVault(ObjectGuid guid);

private:
    WeeklyRewardsMgr() = default;

    // Reset a vault's rows if its stored period is older than the current one.
    void RollPeriod(WeeklyRewards::CharacterVault& vault);

    std::unordered_map<ObjectGuid, WeeklyRewards::CharacterVault> _vaults;
};

#define sWeeklyRewardsMgr WeeklyRewardsMgr::Instance()

#endif // TRINITYCORE_WEEKLY_REWARDS_MGR_H

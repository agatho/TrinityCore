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

#include "WeeklyRewardsMgr.h"
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "Common.h"
#include "GameTime.h"
#include "Optional.h"
#include "Player.h"
#include "StringConvert.h"
#include "Timer.h"
#include "Util.h"
#include <algorithm>
#include <sstream>

namespace WeeklyRewards
{
std::array<uint32, 3> const& ThresholdsFor(ActivityType type)
{
    switch (type)
    {
        case ActivityType::Raid:  return RAID_THRESHOLDS;
        case ActivityType::World: return WORLD_THRESHOLDS;
        case ActivityType::Dungeon:
        default:                  return DUNGEON_THRESHOLDS;
    }
}
}

WeeklyRewardsMgr& WeeklyRewardsMgr::Instance()
{
    static WeeklyRewardsMgr instance;
    return instance;
}

uint32 WeeklyRewardsMgr::GetCurrentPeriod()
{
    // Week index since the unix epoch. Monotonic and identical for every character on the realm, so all vaults
    // roll over together (the real client tracks weeks since Cfg_RegionsEntry::ChallengeOrigin; the absolute base
    // does not matter here, only that the index advances once per week).
    return uint32(GameTime::GetGameTime() / (7 * DAY));
}

void WeeklyRewardsMgr::RollPeriod(WeeklyRewards::CharacterVault& vault)
{
    uint32 const current = GetCurrentPeriod();
    if (vault.Period != current)
    {
        vault.Period = current;
        vault.Rows = {};
    }
}

WeeklyRewards::CharacterVault& WeeklyRewardsMgr::GetVault(ObjectGuid guid)
{
    auto itr = _vaults.find(guid);
    if (itr == _vaults.end())
    {
        LoadCharacter(guid);
        itr = _vaults.find(guid);
    }

    WeeklyRewards::CharacterVault& vault = itr->second;
    RollPeriod(vault);
    return vault;
}

WeeklyRewards::CharacterVault const* WeeklyRewardsMgr::FindVault(ObjectGuid guid) const
{
    auto itr = _vaults.find(guid);
    return itr != _vaults.end() ? &itr->second : nullptr;
}

void WeeklyRewardsMgr::LoadCharacter(ObjectGuid guid)
{
    WeeklyRewards::CharacterVault& vault = _vaults[guid];
    vault.Period = GetCurrentPeriod();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_WEEKLY_REWARD_ACTIVITY);
    stmt->setUInt64(0, guid.GetCounter());
    if (PreparedQueryResult res = CharacterDatabase.Query(stmt))
    {
        do
        {
            Field* f = res->Fetch();
            uint32 const period = f[0].GetUInt32();
            uint8 const type = f[1].GetUInt8();
            if (type >= uint8(WeeklyRewards::ActivityType::Max))
                continue;
            // Only adopt stored rows that belong to the current period; older rows are stale and stay empty.
            if (period == vault.Period)
            {
                vault.Rows[type].Count = f[2].GetUInt32();
                vault.Rows[type].BestLevel = f[3].GetUInt32();
                // Restore the serialized per-run levels (comma-separated, high->low). Empty for legacy rows.
                vault.Rows[type].Levels.clear();
                for (std::string_view tok : Trinity::Tokenize(f[4].GetStringView(), ',', false))
                    if (Optional<uint32> lvl = Trinity::StringTo<uint32>(tok))
                        vault.Rows[type].Levels.push_back(*lvl);
            }
        } while (res->NextRow());
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_WEEKLY_REWARD_STATE);
    stmt->setUInt64(0, guid.GetCounter());
    if (PreparedQueryResult res = CharacterDatabase.Query(stmt))
        vault.ClaimedPeriod = res->Fetch()[0].GetUInt32();
}

void WeeklyRewardsMgr::SaveVault(ObjectGuid guid)
{
    WeeklyRewards::CharacterVault const* vault = FindVault(guid);
    if (!vault)
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (uint8 type = 0; type < uint8(WeeklyRewards::ActivityType::Max); ++type)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_WEEKLY_REWARD_ACTIVITY);
        stmt->setUInt64(0, guid.GetCounter());
        stmt->setUInt8(1, type);
        stmt->setUInt32(2, vault->Period);
        stmt->setUInt32(3, vault->Rows[type].Count);
        stmt->setUInt32(4, vault->Rows[type].BestLevel);
        std::ostringstream levelsStr;
        for (size_t i = 0; i < vault->Rows[type].Levels.size(); ++i)
            levelsStr << (i ? "," : "") << vault->Rows[type].Levels[i];
        stmt->setString(5, levelsStr.str());
        trans->Append(stmt);
    }

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_WEEKLY_REWARD_STATE);
    stmt->setUInt64(0, guid.GetCounter());
    stmt->setUInt32(1, vault->ClaimedPeriod);
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

void WeeklyRewardsMgr::RecordActivity(Player* player, WeeklyRewards::ActivityType type, uint32 level)
{
    if (!player || type >= WeeklyRewards::ActivityType::Max)
        return;

    WeeklyRewards::CharacterVault& vault = GetVault(player->GetGUID());
    WeeklyRewards::ActivityRow& row = vault.Rows[uint8(type)];
    ++row.Count;
    row.BestLevel = std::max(row.BestLevel, level);

    // Track each run's level (sorted high->low, capped at this activity's highest slot threshold) so the Great Vault
    // can award the Nth-best run per slot instead of the single best for all three slots.
    row.Levels.push_back(level);
    std::sort(row.Levels.begin(), row.Levels.end(), std::greater<uint32>());
    if (uint32 const maxRuns = WeeklyRewards::ThresholdsFor(type).back(); row.Levels.size() > maxRuns)
        row.Levels.resize(maxRuns);

    SaveVault(player->GetGUID());
}

bool WeeklyRewardsMgr::HasUnclaimedReward(ObjectGuid guid)
{
    WeeklyRewards::CharacterVault& vault = GetVault(guid);
    if (vault.ClaimedPeriod == vault.Period)
        return false;

    for (uint8 type = 0; type < uint8(WeeklyRewards::ActivityType::Max); ++type)
        if (vault.Rows[type].Count >= WeeklyRewards::ThresholdsFor(WeeklyRewards::ActivityType(type))[0])
            return true;
    return false;
}

bool WeeklyRewardsMgr::MarkClaimed(ObjectGuid guid)
{
    if (!HasUnclaimedReward(guid))
        return false;

    WeeklyRewards::CharacterVault& vault = GetVault(guid);
    vault.ClaimedPeriod = vault.Period;
    SaveVault(guid);
    return true;
}

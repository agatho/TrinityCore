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

#include "MythicPlusData.h"
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "World.h"
#include <algorithm>

MythicPlusData::MythicPlusData(Player* owner) : _owner(owner) { }

void MythicPlusData::LoadFromDB(PreparedQueryResult result)
{
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        MythicPlusRunRecord run;
        run.ChallengeModeID = fields[0].GetUInt32();
        run.Level = fields[1].GetUInt32();
        run.DurationMs = fields[2].GetUInt32();
        run.Deaths = fields[3].GetUInt32();
        run.CompletionDate = fields[4].GetInt64();
        run.Score = fields[5].GetFloat();
        run.Affixes[0] = fields[6].GetUInt32();
        run.Affixes[1] = fields[7].GetUInt32();
        run.Affixes[2] = fields[8].GetUInt32();
        run.Affixes[3] = fields[9].GetUInt32();

        _bestRuns[run.ChallengeModeID] = run;
    } while (result->NextRow());
}

void MythicPlusData::LoadWeeklyFromDB(PreparedQueryResult result)
{
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        MythicPlusWeeklyRun run;
        run.ChallengeModeID = fields[0].GetUInt32();
        run.Level = fields[1].GetUInt32();
        run.CompletionDate = fields[2].GetInt64();
        _weeklyResetTime = fields[3].GetInt64();

        _weeklyRuns.push_back(run);
    } while (result->NextRow());

    // Drop the list if it belongs to a week that has already reset.
    PruneStaleWeek();
}

void MythicPlusData::SaveToDB(CharacterDatabaseTransaction trans)
{
    ObjectGuid::LowType guid = _owner->GetGUID().GetCounter();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    for (auto const& [challengeModeId, run] : _bestRuns)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS);
        stmt->setUInt64(0, guid);
        stmt->setUInt32(1, run.ChallengeModeID);
        stmt->setUInt32(2, run.Level);
        stmt->setUInt32(3, run.DurationMs);
        stmt->setUInt32(4, run.Deaths);
        stmt->setInt64(5, run.CompletionDate);
        stmt->setFloat(6, run.Score);
        stmt->setUInt32(7, run.Affixes[0]);
        stmt->setUInt32(8, run.Affixes[1]);
        stmt->setUInt32(9, run.Affixes[2]);
        stmt->setUInt32(10, run.Affixes[3]);
        trans->Append(stmt);
    }

    // Weekly Great Vault runs.
    PruneStaleWeek();

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS_WEEKLY);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    for (MythicPlusWeeklyRun const& run : _weeklyRuns)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS_WEEKLY);
        stmt->setUInt64(0, guid);
        stmt->setUInt32(1, run.ChallengeModeID);
        stmt->setUInt32(2, run.Level);
        stmt->setInt64(3, run.CompletionDate);
        stmt->setInt64(4, _weeklyResetTime);
        trans->Append(stmt);
    }
}

bool MythicPlusData::RecordRun(MythicPlusRunRecord const& run)
{
    auto itr = _bestRuns.find(run.ChallengeModeID);
    if (itr != _bestRuns.end())
    {
        MythicPlusRunRecord const& best = itr->second;
        // Best run is the one that awards the most rating, which is the Score - NOT the keystone level. A higher
        // keystone completed over time (depleted) scores lower than a lower keystone finished in time, so ranking
        // by Level would let such a run overwrite the real best and LOWER the player's overall rating. Score already
        // folds in level, timing and affixes.
        if (run.Score <= best.Score)
            return false;
    }

    _bestRuns[run.ChallengeModeID] = run;
    return true;
}

MythicPlusRunRecord const* MythicPlusData::GetBestRun(uint32 challengeModeId) const
{
    auto itr = _bestRuns.find(challengeModeId);
    return itr != _bestRuns.end() ? &itr->second : nullptr;
}

float MythicPlusData::GetOverallScore() const
{
    float total = 0.0f;
    for (auto const& [challengeModeId, run] : _bestRuns)
        total += run.Score;
    return total;
}

void MythicPlusData::PruneStaleWeek() const
{
    int64 const currentReset = int64(sWorld->GetNextWeeklyQuestsResetTime());
    if (_weeklyResetTime != currentReset)
    {
        _weeklyRuns.clear();
        _weeklyResetTime = currentReset;
    }
}

void MythicPlusData::RecordWeeklyRun(uint32 challengeModeId, uint32 level, int64 date)
{
    PruneStaleWeek();
    _weeklyRuns.push_back({ challengeModeId, level, date });
}

std::vector<MythicPlusWeeklyRun> MythicPlusData::GetWeeklyRunsByLevel() const
{
    PruneStaleWeek();
    std::vector<MythicPlusWeeklyRun> runs = _weeklyRuns;
    std::sort(runs.begin(), runs.end(), [](MythicPlusWeeklyRun const& a, MythicPlusWeeklyRun const& b)
    {
        return a.Level > b.Level;
    });
    return runs;
}

uint32 MythicPlusData::GetVaultSlotLevel(uint32 slotIndex) const
{
    if (slotIndex >= 3)
        return 0;

    PruneStaleWeek();
    uint32 const threshold = VAULT_SLOT_THRESHOLDS[slotIndex];
    if (_weeklyRuns.size() < threshold)
        return 0;   // slot not yet unlocked

    // The slot rewards the level of the threshold-th best run (1st / 4th / 8th).
    std::vector<MythicPlusWeeklyRun> const runs = GetWeeklyRunsByLevel();
    return runs[threshold - 1].Level;
}

uint32 MythicPlusData::GetWeeklyRunCount() const
{
    PruneStaleWeek();
    return uint32(_weeklyRuns.size());
}

void MythicPlusData::LoadVaultFromDB(PreparedQueryResult result)
{
    if (!result)
        return;

    _vaultClaimedResetTime = result->Fetch()[0].GetInt64();
}

bool MythicPlusData::IsVaultClaimedThisWeek() const
{
    return _vaultClaimedResetTime == int64(sWorld->GetNextWeeklyQuestsResetTime());
}

void MythicPlusData::SetVaultClaimed()
{
    _vaultClaimedResetTime = int64(sWorld->GetNextWeeklyQuestsResetTime());

    ObjectGuid::LowType guid = _owner->GetGUID().GetCounter();

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS_VAULT);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS_VAULT);
    stmt->setUInt64(0, guid);
    stmt->setInt64(1, _vaultClaimedResetTime);
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);
}

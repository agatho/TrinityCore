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
        run.Timed = fields[2].GetUInt8() != 0;
        run.CompletionDate = fields[3].GetInt64();
        _weeklyResetTime = fields[4].GetInt64();

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
        stmt->setUInt8(3, run.Timed ? 1 : 0);
        stmt->setInt64(4, run.CompletionDate);
        stmt->setInt64(5, _weeklyResetTime);
        trans->Append(stmt);
    }
}

bool MythicPlusData::RecordRun(MythicPlusRunRecord const& run)
{
    auto itr = _bestRuns.find(run.ChallengeModeID);
    if (itr != _bestRuns.end())
    {
        MythicPlusRunRecord const& best = itr->second;
        if (run.Level < best.Level || (run.Level == best.Level && run.DurationMs >= best.DurationMs))
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
        // Capture last week's best-run summary before discarding it: this feeds the weekly keystone adjustment
        // (retail derives the new key level from the previous week's runs).
        if (!_weeklyRuns.empty())
        {
            _prunedWeekResetTime = _weeklyResetTime;
            _prunedWeekBestTimedLevel = 0;
            _prunedWeekBestLevel = 0;
            for (MythicPlusWeeklyRun const& run : _weeklyRuns)
            {
                _prunedWeekBestLevel = std::max(_prunedWeekBestLevel, run.Level);
                if (run.Timed)
                    _prunedWeekBestTimedLevel = std::max(_prunedWeekBestTimedLevel, run.Level);
            }
        }

        _weeklyRuns.clear();
        _weeklyResetTime = currentReset;
    }
}

void MythicPlusData::RecordWeeklyRun(uint32 challengeModeId, uint32 level, bool timed, int64 date)
{
    PruneStaleWeek();
    _weeklyRuns.push_back({ challengeModeId, level, timed, date });
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

    Field* fields = result->Fetch();
    _vaultClaimedResetTime = fields[0].GetInt64();
    _keystoneResetTime = fields[1].GetInt64();
}

bool MythicPlusData::IsVaultClaimedThisWeek() const
{
    return _vaultClaimedResetTime == int64(sWorld->GetNextWeeklyQuestsResetTime());
}

void MythicPlusData::SetVaultClaimed()
{
    _vaultClaimedResetTime = int64(sWorld->GetNextWeeklyQuestsResetTime());
    SaveVaultToDB();
}

bool MythicPlusData::NeedsKeystoneAdjustment() const
{
    return _keystoneResetTime != int64(sWorld->GetNextWeeklyQuestsResetTime());
}

void MythicPlusData::SetKeystoneAdjusted()
{
    _keystoneResetTime = int64(sWorld->GetNextWeeklyQuestsResetTime());
    SaveVaultToDB();
}

uint32 MythicPlusData::ComputeNewWeekKeystoneLevel(uint32 currentLevel, uint32 minLevel) const
{
    // Make sure last week's summary has been captured if the boundary just rolled over.
    PruneStaleWeek();

    // Retail rule: new key = best timed level of last week, or one below the best run if it was untimed.
    // With no runs last week the key decays one level from what it is now, and one further per fully idle week.
    uint32 level;
    int64 lastWeekBoundary = _weeklyResetTime - int64(WEEK);
    if (_prunedWeekResetTime == lastWeekBoundary && _prunedWeekBestLevel)
        level = std::max(_prunedWeekBestTimedLevel, _prunedWeekBestLevel - 1);
    else
    {
        level = currentLevel > 0 ? currentLevel - 1 : 0;
        // Additional decay for weeks fully skipped since the last recorded week (when known).
        if (_prunedWeekResetTime && _prunedWeekResetTime < lastWeekBoundary)
        {
            int64 idleWeeks = (lastWeekBoundary - _prunedWeekResetTime) / int64(WEEK);
            level -= uint32(std::min<int64>(idleWeeks, level));
        }
    }

    return std::max(level, minLevel);
}

void MythicPlusData::SaveVaultToDB() const
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS_VAULT);
    stmt->setUInt64(0, _owner->GetGUID().GetCounter());
    stmt->setInt64(1, _vaultClaimedResetTime);
    stmt->setInt64(2, _keystoneResetTime);
    CharacterDatabase.Execute(stmt);
}

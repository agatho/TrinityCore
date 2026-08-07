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
#include "ChallengeModeMgr.h"
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "MythicPlusPacketsCommon.h"
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

namespace
{
    // A run counts as timed when its (death-penalty-inclusive) duration beat the dungeon's par time.
    bool IsRunTimed(MythicPlusRunRecord const& run)
    {
        uint32 const parSeconds = sChallengeModeMgr.GetTimeLimit(run.ChallengeModeID);
        return parSeconds && run.DurationMs <= parSeconds * IN_MILLISECONDS;
    }
}

void MythicPlusData::BuildDungeonScoreSummary(WorldPackets::MythicPlus::DungeonScoreSummary& summary) const
{
    summary.OverallScoreCurrentSeason = GetOverallScore();
    summary.LadderScoreCurrentSeason = 0.0f;

    summary.Runs.reserve(_bestRuns.size());
    for (auto const& [challengeModeId, run] : _bestRuns)
    {
        WorldPackets::MythicPlus::DungeonScoreMapSummary& mapSummary = summary.Runs.emplace_back();
        mapSummary.ChallengeModeID = int32(challengeModeId);
        mapSummary.MapScore = run.Score;
        mapSummary.BestRunLevel = int32(run.Level);
        mapSummary.BestRunDurationMS = int32(run.DurationMs);
        mapSummary.FinishedSuccess = IsRunTimed(run);
    }
}

void MythicPlusData::BuildDungeonScoreData(WorldPackets::MythicPlus::DungeonScoreData& data) const
{
    int32 const seasonId = int32(sChallengeModeMgr.GetActiveSeasonId());

    WorldPackets::MythicPlus::DungeonScoreSeasonData& season = data.Seasons.emplace_back();
    season.Season = seasonId;
    season.SeasonScore = GetOverallScore();
    season.LadderScore = 0.0f;

    season.SeasonMaps.reserve(_bestRuns.size());
    for (auto const& [challengeModeId, run] : _bestRuns)
    {
        WorldPackets::MythicPlus::DungeonScoreMapData& map = season.SeasonMaps.emplace_back();
        map.MapChallengeModeID = int32(challengeModeId);
        map.OverAllScore = run.Score;

        // Scoring has been single-best-run since TWW, but the wire still keys best runs per affix; a single
        // entry keyed by the run's first affix is what the client expects for the one tracked run.
        WorldPackets::MythicPlus::DungeonScoreBestRunForAffix& bestRun = map.BestRuns.emplace_back();
        bestRun.KeystoneAffixID = int32(run.Affixes[0]);
        bestRun.Score = run.Score;

        bestRun.Run.MapChallengeModeID = int32(challengeModeId);
        bestRun.Run.Completed = IsRunTimed(run);
        bestRun.Run.Level = run.Level;
        bestRun.Run.DurationMs = int32(run.DurationMs);
        bestRun.Run.StartDate = run.CompletionDate - int64(run.DurationMs / IN_MILLISECONDS);
        bestRun.Run.CompletionDate = run.CompletionDate;
        bestRun.Run.Season = seasonId;
        bestRun.Run.RunScore = run.Score;
        for (std::size_t i = 0; i < run.Affixes.size(); ++i)
            bestRun.Run.KeystoneAffixIDs[i] = int32(run.Affixes[i]);
    }

    data.TotalRuns = int32(_bestRuns.size());
}

void MythicPlusData::PruneStaleWeek() const
{
    int64 const currentReset = int64(sWorld->GetNextWeeklyQuestsResetTime());
    if (_weeklyResetTime == currentReset)
        return;

    // Capture last week's best-run summary before discarding it: this feeds the weekly keystone adjustment
    // (retail derives the new key level from the previous week's runs).
    bool captured = false;
    if (!_weeklyRuns.empty() && _weeklyResetTime)
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
        captured = true;
    }

    _weeklyRuns.clear();
    _weeklyResetTime = currentReset;

    // The run rows are gone from here on, so the summary has to be durable before anything else can save.
    if (captured)
        SaveVaultToDB();
}

void MythicPlusData::ResetWeeklyRuns()
{
    PruneStaleWeek();
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
    _prunedWeekResetTime = fields[2].GetInt64();
    _prunedWeekBestLevel = fields[3].GetUInt32();
    _prunedWeekBestTimedLevel = fields[4].GetUInt32();
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
    // Make sure the finished week's summary has been captured if the boundary just rolled over.
    PruneStaleWeek();

    // _weeklyResetTime is the current boundary after the prune above. A captured summary belongs to the week
    // that ENDED at _prunedWeekResetTime, so exactly one boundary between the two means "last week".
    int64 const currentBoundary = _weeklyResetTime;
    uint32 level;

    if (_prunedWeekResetTime && _prunedWeekBestLevel && _prunedWeekResetTime <= currentBoundary)
    {
        // Rules 1 + 2: the highest dungeon completed that week, at its own level when it was timed and one
        // level lower when it was not. (bestTimed <= best always, so the max() collapses to exactly that.)
        level = std::max(_prunedWeekBestTimedLevel, _prunedWeekBestLevel - 1);

        // Rule 3: one level lower for every FURTHER consecutive week without a completion.
        int64 const idleWeeks = (currentBoundary - _prunedWeekResetTime) / int64(WEEK) - 1;
        if (idleWeeks > 0)
            level -= uint32(std::min<int64>(idleWeeks, int64(level)));
    }
    else
    {
        // Rule 4: nothing on record -> one level below the key carried, once per weekly boundary missed since
        // the last adjustment (a character offline for three weeks drops three levels, not one).
        int64 missedWeeks = 1;
        if (_keystoneResetTime && currentBoundary > _keystoneResetTime)
            missedWeeks = std::max<int64>((currentBoundary - _keystoneResetTime) / int64(WEEK), 1);

        level = currentLevel;
        level -= uint32(std::min<int64>(missedWeeks, int64(level)));
    }

    return std::max(level, minLevel);
}

void MythicPlusData::SaveVaultToDB() const
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS_VAULT);
    stmt->setUInt64(0, _owner->GetGUID().GetCounter());
    stmt->setInt64(1, _vaultClaimedResetTime);
    stmt->setInt64(2, _keystoneResetTime);
    stmt->setInt64(3, _prunedWeekResetTime);
    stmt->setUInt32(4, _prunedWeekBestLevel);
    stmt->setUInt32(5, _prunedWeekBestTimedLevel);
    CharacterDatabase.Execute(stmt);
}

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

#ifndef MythicPlusData_h__
#define MythicPlusData_h__

#include "DatabaseEnvFwd.h"
#include "Define.h"
#include <array>
#include <unordered_map>
#include <vector>

class Player;

struct MythicPlusRunRecord
{
    uint32 ChallengeModeID = 0;
    uint32 Level = 0;
    uint32 DurationMs = 0;
    uint32 Deaths = 0;
    int64 CompletionDate = 0;               // unix timestamp of the run
    float Score = 0.0f;
    std::array<uint32, 4> Affixes = { };
};

// One completed run this Great Vault week (every run counts toward the 1/4/8-run slots, not just the best).
struct MythicPlusWeeklyRun
{
    uint32 ChallengeModeID = 0;
    uint32 Level = 0;
    int64 CompletionDate = 0;
};

// Per-player Mythic+ progression: the best run recorded for each dungeon and the overall rating derived from them.
// Attached to Player (see Player::GetMythicPlusData), loaded/saved with the character.
class TC_GAME_API MythicPlusData
{
public:
    // Great Vault unlocks a reward slot at 1 / 4 / 8 completed runs in a week.
    static constexpr uint32 VAULT_SLOT_THRESHOLDS[3] = { 1, 4, 8 };

    explicit MythicPlusData(Player* owner);

    void LoadFromDB(PreparedQueryResult result);
    void LoadWeeklyFromDB(PreparedQueryResult result);
    void SaveToDB(CharacterDatabaseTransaction trans);

    // Records a completed run, keeping the best per dungeon (higher level wins, ties broken by faster time).
    // Returns true if it became the new best for that dungeon.
    bool RecordRun(MythicPlusRunRecord const& run);
    MythicPlusRunRecord const* GetBestRun(uint32 challengeModeId) const;
    std::unordered_map<uint32, MythicPlusRunRecord> const& GetBestRuns() const { return _bestRuns; }

    // Sum of the best-run scores across all dungeons (the client's overall Mythic+ Rating).
    float GetOverallScore() const;

    // --- Great Vault weekly tracking ---
    // Records a run toward this week's vault (all runs count). Auto-resets the list when the weekly reset passes.
    void RecordWeeklyRun(uint32 challengeModeId, uint32 level, int64 date);
    // This week's runs sorted by keystone level, highest first (what the vault slots draw from).
    std::vector<MythicPlusWeeklyRun> GetWeeklyRunsByLevel() const;
    // Keystone level rewarded at vault slot 0/1/2 (unlocked at 1/4/8 runs); 0 if the slot is still locked.
    uint32 GetVaultSlotLevel(uint32 slotIndex) const;
    uint32 GetWeeklyRunCount() const;

private:
    // Clears the weekly list when the stored weekly-reset boundary no longer matches the world's next reset.
    void PruneStaleWeek() const;

    Player* _owner;
    std::unordered_map<uint32 /*challengeModeId*/, MythicPlusRunRecord> _bestRuns;

    mutable std::vector<MythicPlusWeeklyRun> _weeklyRuns;
    mutable int64 _weeklyResetTime = 0;     // the GetNextWeeklyQuestsResetTime these runs belong to
};

#endif // MythicPlusData_h__

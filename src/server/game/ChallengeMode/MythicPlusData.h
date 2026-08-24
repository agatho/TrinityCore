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

namespace WorldPackets::MythicPlus
{
    struct DungeonScoreData;
    struct DungeonScoreSummary;
}

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
    bool Timed = false;                     // beat the par time (drives the weekly keystone adjustment)
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

    // --- client rating surfacing (update fields) ---
    // Fills the public roster summary (PlayerData::DungeonScore -- what party/inspect UIs read).
    void BuildDungeonScoreSummary(WorldPackets::MythicPlus::DungeonScoreSummary& summary) const;
    // Fills the owner's full score tree (ActivePlayerData::DungeonScore -- what the Mythic+ UI reads).
    void BuildDungeonScoreData(WorldPackets::MythicPlus::DungeonScoreData& data) const;

    // --- Great Vault weekly tracking ---
    // Records a run toward this week's vault (all runs count). Auto-resets the list when the weekly reset passes.
    void RecordWeeklyRun(uint32 challengeModeId, uint32 level, bool timed, int64 date);
    // Drops the finished week's run list (the vault-progress reset), capturing its best/best-timed summary for
    // the weekly keystone rule first. Driven by the real weekly-reset event for online characters
    // (ChallengeModeMgr::OnWeeklyReset) and lazily by any weekly accessor for characters that were offline.
    void ResetWeeklyRuns();
    // This week's runs sorted by keystone level, highest first (what the vault slots draw from).
    std::vector<MythicPlusWeeklyRun> GetWeeklyRunsByLevel() const;
    // Keystone level rewarded at vault slot 0/1/2 (unlocked at 1/4/8 runs); 0 if the slot is still locked.
    uint32 GetVaultSlotLevel(uint32 slotIndex) const;
    uint32 GetWeeklyRunCount() const;

    // --- Great Vault claim (one reward per weekly reset) ---
    void LoadVaultFromDB(PreparedQueryResult result);
    // True once this week's Great Vault reward has been collected (blocks a second claim until the weekly reset).
    bool IsVaultClaimedThisWeek() const;
    // Marks this week's vault reward as claimed and persists it immediately.
    void SetVaultClaimed();

    // --- weekly keystone adjustment ---
    // True until the keystone has been adjusted for the current reset boundary.
    bool NeedsKeystoneAdjustment() const;
    // Marks the keystone adjusted for this week's boundary and persists it immediately.
    void SetKeystoneAdjusted();
    // The keystone level the player should carry this week, derived from the previous week's runs (captured
    // before the weekly list is pruned). currentLevel is the level of the keystone being adjusted; the result is
    // floored at minLevel (the Resilient Keystone floor, or the season minimum).
    //
    // Retail rule (Maxroll "Mythic+ Dungeon Guide: Keystone Basics, Rewards, Affixes & Scaling 12.0.7",
    // https://maxroll.gg/wow/resources/mythic-dungeon-mechanics -- corroborated by Wowpedia "Mythic Keystone"):
    //   1. "Same level as the highest Mythic+ Dungeon you completed in time previous week."
    //   2. "One level lower than the highest Mythic+ Dungeon you completed the previous week which wasn't in time."
    //   3. "One level lower for each consecutive week that you didn't complete a Mythic+ Dungeon."
    //   4. "One level lower than the Keystone you received the previous week." (no recorded history at all)
    uint32 ComputeNewWeekKeystoneLevel(uint32 currentLevel, uint32 minLevel) const;

private:
    // Clears the weekly list when the stored weekly-reset boundary no longer matches the world's next reset,
    // capturing last week's best-run summary first (feeds ComputeNewWeekKeystoneLevel).
    void PruneStaleWeek() const;
    // Persists the vault row (claim boundary + keystone-adjustment boundary).
    void SaveVaultToDB() const;

    Player* _owner;
    std::unordered_map<uint32 /*challengeModeId*/, MythicPlusRunRecord> _bestRuns;

    mutable std::vector<MythicPlusWeeklyRun> _weeklyRuns;
    mutable int64 _weeklyResetTime = 0;     // the GetNextWeeklyQuestsResetTime these runs belong to
    int64 _vaultClaimedResetTime = 0;       // weekly-reset boundary the vault was last claimed for (0 = never)
    int64 _keystoneResetTime = 0;           // weekly-reset boundary the keystone was last adjusted for (0 = never)

    // Summary of the week that was pruned last (the "previous" week relative to the current boundary). Persisted
    // with the vault row: the weekly run rows are deleted the moment the week is pruned, so without this the
    // summary would be lost if the character saved (or the server restarted) between the weekly reset and the
    // keystone adjustment, silently degrading the key to the "no history" rule.
    mutable int64 _prunedWeekResetTime = 0;
    mutable uint32 _prunedWeekBestTimedLevel = 0;
    mutable uint32 _prunedWeekBestLevel = 0;
};

#endif // MythicPlusData_h__

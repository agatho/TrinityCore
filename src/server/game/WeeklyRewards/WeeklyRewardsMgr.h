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
#include "Optional.h"
#include <vector>
#include <array>
#include <unordered_map>

class Player;
enum class ItemContext : uint8;

namespace WeeklyRewards
{
    // The three Great Vault activity rows. These are the internal row indices; each maps onto a
    // WeeklyRewardChestThreshold.db2 `Type` (see DB2ThresholdType below).
    enum class ActivityType : uint8
    {
        Dungeon = 0,    // Mythic+ / dungeon runs      (DB2 Type 1 = Activities)
        Raid    = 1,    // raid boss kills             (DB2 Type 3 = Raid)
        World   = 2,    // world activity: delves, PvP (DB2 Type 6 = World)

        Max
    };

    // WeeklyRewardChestThreshold.db2 `Type` for a row. The client's WeeklyRewardChestThresholdType enum is
    // None=0, Activities=1, RankedPvP=2, Raid=3, AlsoReceive=4, Concession=5, World=6 - read out of the
    // 12.0.7.68275 client's own enum-reflection registrar (sub_7FF72A0F6780 registers the members in that
    // order and WeeklyRewardChestThresholdTypeMeta gives NumValues=7, MinValue=0, MaxValue=6, i.e. a
    // contiguous 0..6 range, so the registration order IS the value order). World = 6 is NOT a guess and
    // NOT the value WoWDBDefs documents (its comment stops at 5 = Concession).
    uint8 DB2ThresholdType(ActivityType type);

    // One live Great Vault slot of a row, straight out of WeeklyRewardChestThreshold.db2.
    struct VaultSlot
    {
        uint32 ThresholdID = 0;     // the DB2 row id - what goes on the wire; the client resolves Type/Index from it
        uint32 Index = 0;           // slot index 0/1/2 within the row
        uint32 Threshold = 0;       // completions required to unlock the slot
    };

    // The live slots of a row, in Index order. The DB2 keeps every past season's rows, so the live set is the
    // highest-ID row per (Type, Index) - the same rule the client uses ("these are not unique. It appears that
    // the *last* entry is used in game", WoWDBDefs WeeklyRewardChestThreshold.dbd).
    // For 12.0.7.68275 that resolves to ids 202/203/204 (Type 1) = 1/4/8, 199/200/201 (Type 3) = 2/4/6 and
    // 196/197/198 (Type 6) = 2/4/8 - byte-for-byte what the client is sent in the wild
    // (C:\dumps\MPLUS_SNIFF_DEEP_68275.md section 5 lists exactly those nine row ids).
    // EMPTY when the DB2 has no rows of that type: the vault then has no such row at all, rather than a
    // fabricated one.
    std::vector<VaultSlot> const& SlotsFor(ActivityType type);

    // The three completion counts of a row (0 in a slot the DB2 does not define). Convenience view over
    // SlotsFor() for callers that only need the ladder (e.g. `.delve info`).
    std::array<uint32, 3> const& ThresholdsFor(ActivityType type);

    // The item the vault would hand out for one slot: a roll from the row's reward pool, scaled to the item
    // level the client's own DB2 chain derives for (context, level).
    struct SlotReward
    {
        uint32 ItemID = 0;
        std::vector<int32> BonusListIDs;
        ItemContext Context = ItemContext(0);
    };

    // Whether a row can generate a reward item at all, i.e. whether an ItemContext is known for it. False for the
    // Raid row: the client has no raid "jackpot" context and that row records a difficulty, not a reward level,
    // so it shows progress only. A row that returns true but still produces no reward is a content/config problem
    // (unset or empty loot pool) and is logged as an error.
    bool HasRewardContext(ActivityType type);

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

    // --- Great Vault slots ---

    // Resolve a WeeklyRewardChestThreshold.db2 id (what CMSG_CLAIM_WEEKLY_REWARD carries) back to its row and
    // slot. Returns false for an id that is not one of this season's live slots.
    bool FindSlot(uint32 thresholdId, WeeklyRewards::ActivityType& type, WeeklyRewards::VaultSlot& slot) const;

    // The level a slot of `row` is worth: the level of the Nth-best run of the week, N = the slot's threshold.
    // 0 while the slot is still locked.
    static uint32 GetSlotLevel(WeeklyRewards::ActivityRow const& row, WeeklyRewards::VaultSlot const& slot);

    // Roll the item a slot would hand out, scaled to `level`. Empty when the row has no reward ItemContext or
    // its reward pool is unconfigured/empty - the caller must then leave the slot without a reward rather than
    // advertise one that pays nothing.
    Optional<WeeklyRewards::SlotReward> BuildSlotReward(Player* player, WeeklyRewards::ActivityType type, uint32 level) const;

    // Hand a rolled reward to the player (mails it when the bags are full, so a claim is never swallowed).
    void GrantSlotReward(Player* player, WeeklyRewards::SlotReward const& reward) const;

private:
    WeeklyRewardsMgr() = default;

    // Reset a vault's rows if its stored period is older than the current one.
    void RollPeriod(WeeklyRewards::CharacterVault& vault);

    std::unordered_map<ObjectGuid, WeeklyRewards::CharacterVault> _vaults;
};

#define sWeeklyRewardsMgr WeeklyRewardsMgr::Instance()

#endif // TRINITYCORE_WEEKLY_REWARDS_MGR_H

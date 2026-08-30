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
#include "ChallengeModeMgr.h"
#include "CharacterDatabase.h"
#include "Common.h"
#include "Config.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemBonusMgr.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "Optional.h"
#include "Player.h"
#include "StringConvert.h"
#include "Timer.h"
#include "Util.h"
#include <algorithm>
#include <sstream>

namespace WeeklyRewards
{
uint8 DB2ThresholdType(ActivityType type)
{
    // WeeklyRewardChestThresholdType, client 12.0.7.68275 (enum-reflection registrar sub_7FF72A0F6780):
    // None=0, Activities=1, RankedPvP=2, Raid=3, AlsoReceive=4, Concession=5, World=6.
    switch (type)
    {
        case ActivityType::Raid:  return 3;
        case ActivityType::World: return 6;
        case ActivityType::Dungeon:
        default:                  return 1;
    }
}

namespace
{
    // The live slot ladder of all three rows, read once out of WeeklyRewardChestThreshold.db2.
    struct VaultSlotTable
    {
        std::array<std::vector<VaultSlot>, uint8(ActivityType::Max)> Slots;
        std::array<std::array<uint32, 3>, uint8(ActivityType::Max)> Counts = { };

        VaultSlotTable()
        {
            // Per (Type, Index) keep the highest-ID row: the DB2 retains every past season's rows and the
            // client uses the last one ("for some reason, these are not unique. It appears that the *last*
            // entry is used in game" - WoWDBDefs WeeklyRewardChestThreshold.dbd).
            std::array<std::array<WeeklyRewardChestThresholdEntry const*, 3>, uint8(ActivityType::Max)> live = { };

            for (WeeklyRewardChestThresholdEntry const* entry : sWeeklyRewardChestThresholdStore)
            {
                if (entry->Index < 0 || entry->Index > 2)
                    continue;

                for (uint8 t = 0; t < uint8(ActivityType::Max); ++t)
                {
                    if (uint8(entry->Type) != DB2ThresholdType(ActivityType(t)))
                        continue;

                    WeeklyRewardChestThresholdEntry const*& current = live[t][entry->Index];
                    if (!current || entry->ID > current->ID)
                        current = entry;
                }
            }

            for (uint8 t = 0; t < uint8(ActivityType::Max); ++t)
            {
                for (uint8 index = 0; index < 3; ++index)
                {
                    WeeklyRewardChestThresholdEntry const* entry = live[t][index];
                    if (!entry)
                        continue;

                    uint32 const threshold = uint32(std::max(entry->Threshold, 0));
                    if (!threshold)     // a 0-completion row is AlsoReceive/Concession bookkeeping, not a slot
                        continue;

                    Slots[t].push_back({ entry->ID, index, threshold });
                    Counts[t][index] = threshold;
                }

                if (Slots[t].empty())
                    TC_LOG_INFO("misc", "WeeklyRewards: no live WeeklyRewardChestThreshold.db2 rows of Type {} - "
                        "the Great Vault will not show that activity row.", DB2ThresholdType(ActivityType(t)));
            }
        }
    };

    VaultSlotTable const& GetSlotTable()
    {
        // DB2 stores are fully loaded (and hotfixes applied) long before anything can ask the vault for a slot.
        static VaultSlotTable const table;
        return table;
    }
}

std::vector<VaultSlot> const& SlotsFor(ActivityType type)
{
    static std::vector<VaultSlot> const empty;
    if (type >= ActivityType::Max)
        return empty;

    return GetSlotTable().Slots[uint8(type)];
}

std::array<uint32, 3> const& ThresholdsFor(ActivityType type)
{
    static std::array<uint32, 3> const empty = { };
    if (type >= ActivityType::Max)
        return empty;

    return GetSlotTable().Counts[uint8(type)];
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
    if (std::vector<WeeklyRewards::VaultSlot> const& slots = WeeklyRewards::SlotsFor(type); !slots.empty())
        if (size_t const maxRuns = slots.back().Threshold; row.Levels.size() > maxRuns)
            row.Levels.resize(maxRuns);

    SaveVault(player->GetGUID());
}

bool WeeklyRewardsMgr::HasUnclaimedReward(ObjectGuid guid)
{
    WeeklyRewards::CharacterVault& vault = GetVault(guid);
    if (vault.ClaimedPeriod == vault.Period)
        return false;

    // A row with no live DB2 slots has nothing to claim (never "0 completions required"), and neither has a row
    // that cannot generate a reward at all - claiming it would consume the week for nothing.
    for (uint8 type = 0; type < uint8(WeeklyRewards::ActivityType::Max); ++type)
    {
        if (!WeeklyRewards::HasRewardContext(WeeklyRewards::ActivityType(type)))
            continue;

        for (WeeklyRewards::VaultSlot const& slot : WeeklyRewards::SlotsFor(WeeklyRewards::ActivityType(type)))
            if (vault.Rows[type].Count >= slot.Threshold)
                return true;
    }

    return false;
}

bool WeeklyRewardsMgr::FindSlot(uint32 thresholdId, WeeklyRewards::ActivityType& type, WeeklyRewards::VaultSlot& slot) const
{
    for (uint8 t = 0; t < uint8(WeeklyRewards::ActivityType::Max); ++t)
    {
        for (WeeklyRewards::VaultSlot const& candidate : WeeklyRewards::SlotsFor(WeeklyRewards::ActivityType(t)))
        {
            if (candidate.ThresholdID != thresholdId)
                continue;

            type = WeeklyRewards::ActivityType(t);
            slot = candidate;
            return true;
        }
    }

    return false;
}

uint32 WeeklyRewardsMgr::GetSlotLevel(WeeklyRewards::ActivityRow const& row, WeeklyRewards::VaultSlot const& slot)
{
    if (!slot.Threshold || row.Count < slot.Threshold)
        return 0;

    // Each slot rewards the level of the Nth-best run (N = the slot's threshold), not the single best for every
    // slot. Levels is sorted high->low; BestLevel is only the fallback for rows saved before Levels existed.
    return slot.Threshold <= row.Levels.size() ? row.Levels[slot.Threshold - 1] : row.BestLevel;
}

namespace WeeklyRewards
{
    // The ItemContext a row's vault reward is generated in. The item LEVEL is never a number this server picks:
    // ItemBonusMgr walks the client's own ItemBonusTreeNode -> ItemBonusListGroup -> ItemLevelSelector chain for
    // (context, level), exactly as it already does for the Mythic+ vault.
    //   Dungeon -> MythicPlus_Jackpot (35), level = keystone level.
    //   World   -> Delves_Jackpot (108), level = delve tier. ItemBonusTreeNode.db2 carries live Delves_Jackpot
    //              nodes gated on MinMythicPlusLevel/MaxMythicPlusLevel = the delve tier bands (ids 23321/23322/
    //              23323 => tiers 0-4 / 5-7 / 8-11 in 12.0.7.68275), so the T1..T11 ladder resolves from the DB2
    //              and no ilvl table has to be hardcoded.
    //   Raid    -> none: there is no Raid jackpot context and the row stores a difficulty, not a level, so no
    //              reward is generated for it (the row still shows progress). See the handler.
    static Optional<ItemContext> VaultRewardContext(ActivityType type)
    {
        switch (type)
        {
            case ActivityType::Dungeon: return ItemContext::MythicPlus_Jackpot;
            case ActivityType::World:   return ItemContext::Delves_Jackpot;
            default:                    return { };
        }
    }

    bool HasRewardContext(ActivityType type)
    {
        return VaultRewardContext(type).has_value();
    }

    // The reference_loot_template pool a row's vault reward is rolled from. Pools are content, not something the
    // core can derive, so they are configuration.
    static uint32 VaultRewardLootId(ActivityType type)
    {
        switch (type)
        {
            case ActivityType::Dungeon:
                // Falls back to the Mythic+ vault pool, which is the same pool under its existing name.
                if (uint32 lootId = uint32(sConfigMgr->GetIntDefault("WeeklyRewards.Vault.Dungeon.LootId", 0)))
                    return lootId;
                return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Vault.LootId", 0));
            case ActivityType::World:
                // Falls back to the delve gear pool, which is the pool the delve content ships.
                if (uint32 lootId = uint32(sConfigMgr->GetIntDefault("WeeklyRewards.Vault.World.LootId", 0)))
                    return lootId;
                return uint32(sConfigMgr->GetIntDefault("Delves.Reward.LootId", 0));
            default:
                return 0;
        }
    }
}

Optional<WeeklyRewards::SlotReward> WeeklyRewardsMgr::BuildSlotReward(Player* player, WeeklyRewards::ActivityType type, uint32 level) const
{
    if (!player || !level)
        return { };

    Optional<ItemContext> const context = WeeklyRewards::VaultRewardContext(type);
    if (!context)
        return { };

    // Mythic+ reward scaling stops at the active season's highest MythicPlusSeasonRewardLevels row; higher keys
    // are score-only. Same cap ChallengeModeMgr::GetMythicPlusVaultSlotRewardLevel applies, so the two vault
    // paths cannot drift. 0 = the DB2 has no rows for the season -> uncapped rather than invented.
    if (type == WeeklyRewards::ActivityType::Dungeon)
        if (uint32 const cap = sChallengeModeMgr.GetVaultRewardLevelCap())
            level = std::min(level, cap);

    uint32 const lootId = WeeklyRewards::VaultRewardLootId(type);
    if (!lootId || !LootTemplates_Reference.HaveLootFor(lootId))
        return { };

    Loot loot(player->GetMap(), ObjectGuid::Empty, LOOT_NONE, nullptr);
    loot.FillLoot(lootId, LootTemplates_Reference, player, true /*personal*/, true /*noEmptyError*/,
        LOOT_MODE_DEFAULT, *context);

    uint32 itemId = 0;
    for (LootItem const& item : loot.items)
    {
        if (item.itemid)
        {
            itemId = item.itemid;
            break;
        }
    }

    if (!itemId)
        return { };

    WeeklyRewards::SlotReward reward;
    reward.ItemID = itemId;
    reward.Context = *context;
    reward.BonusListIDs = ItemBonusMgr::GetBonusListsForItem(itemId,
        ItemBonusMgr::ItemBonusGenerationParams(*context, int32(level)));
    return reward;
}

void WeeklyRewardsMgr::GrantSlotReward(Player* player, WeeklyRewards::SlotReward const& reward) const
{
    ItemPosCountVec dest;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, reward.ItemID, 1) == EQUIP_ERR_OK)
    {
        player->StoreNewItem(dest, reward.ItemID, true, 0, GuidSet(), reward.Context, &reward.BonusListIDs);
        return;
    }

    // Bags full: mail it rather than swallow a once-a-week reward.
    if (Item* item = Item::CreateItem(reward.ItemID, 1, reward.Context, player, false))
    {
        item->SetBonuses(reward.BonusListIDs);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        item->SaveToDB(trans);
        MailDraft("Great Vault Reward", "Your Great Vault reward.")
            .AddItem(item)
            .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
        CharacterDatabase.CommitTransaction(trans);
    }
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

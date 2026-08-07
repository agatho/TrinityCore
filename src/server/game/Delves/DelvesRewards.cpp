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

#include "DelvesRewards.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DelveMgr.h"
#include "DelvesCompanion.h"
#include "DelvesSeason.h"
#include "ItemBonusMgr.h"
#include "Log.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Player.h"
#include "WorldSession.h"

namespace Delves
{

namespace
{
    // Rolls a reference_loot_template pool as personal loot at the delve item context and grants every item,
    // scaled through ItemBonusMgr (the same authentic ilvl path the M+ rewards use). Mails on full bags.
    void GrantDelveLoot(Player* player, uint32 lootId, uint8 itemContext)
    {
        if (!lootId || !LootTemplates_Reference.HaveLootFor(lootId))
            return;

        Loot loot(player->GetMap(), ObjectGuid::Empty, LOOT_NONE, nullptr);
        loot.FillLoot(lootId, LootTemplates_Reference, player, true /*personal*/, true /*noEmptyError*/,
            LOOT_MODE_DEFAULT, ItemContext(itemContext));

        for (LootItem const& lootItem : loot.items)
        {
            if (!lootItem.itemid || !lootItem.count)
                continue;

            std::vector<int32> bonuses = ItemBonusMgr::GetBonusListsForItem(lootItem.itemid,
                ItemBonusMgr::ItemBonusGenerationParams(ItemContext(itemContext)));

            ItemPosCountVec dest;
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, lootItem.itemid, lootItem.count) == EQUIP_ERR_OK)
                player->StoreNewItem(dest, lootItem.itemid, true, 0, GuidSet(), ItemContext(itemContext), &bonuses);
            else if (Item* item = Item::CreateItem(lootItem.itemid, lootItem.count, ItemContext(itemContext), player, false))
            {
                item->SetBonuses(bonuses);
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                item->SaveToDB(trans);
                MailDraft("Delve Reward", "Your delve reward.")
                    .AddItem(item)
                    .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
                CharacterDatabase.CommitTransaction(trans);
            }
        }
    }
}

void DelvesRewards::AwardDelveCompletion(Player* player, uint8 tier, bool bountiful, bool hasRevivesRemaining)
{
    uint32 accountId = player->GetSession()->GetBattlenetAccountId();

    // Load current progress
    DelveProgress progress;
    LoadProgress(accountId, progress);

    // Track completion
    progress.WeeklyCompletions++;
    if (tier > progress.HighestTierThisWeek)
        progress.HighestTierThisWeek = tier;

    // Award crests based on tier
    AwardCrests(player, tier);

    // Award companion XP
    AwardCompanionXP(player, tier);

    // End-of-run gear (non-bountiful context; retail caps this at the tier-3 track). The item POOL is
    // server content (reference_loot_template, Delves.Reward.LootId); the LEVEL comes from the item context.
    GrantDelveLoot(player, uint32(sConfigMgr->GetIntDefault("Delves.Reward.LootId", 0)),
        GetItemContextForTier(std::min<uint8>(tier, 3), false));

    // Handle bountiful rewards
    if (bountiful)
    {
        progress.WeeklyBountifulCount++;

        if (hasRevivesRemaining && HasCofferKey(player))
        {
            ConsumeCofferKey(player);
            // Bountiful Coffer: the enhanced chest loot at the tier's bounty item context.
            GrantDelveLoot(player, uint32(sConfigMgr->GetIntDefault("Delves.Coffer.LootId", 0)),
                GetItemContextForTier(tier, true));
            TC_LOG_DEBUG("scripts.delves", "Player {} opened Bountiful Coffer (tier {}, ItemContext {})",
                player->GetName(), tier, GetItemContextForTier(tier, true));
        }
    }

    // Update Great Vault progress
    UpdateGreatVaultProgress(player, tier);

    // Check for tier unlock (must have revives remaining)
    if (hasRevivesRemaining && CanUnlockNextTier(progress.HighestTierUnlocked, tier, hasRevivesRemaining))
    {
        progress.HighestTierUnlocked = tier + 1;
        TC_LOG_DEBUG("scripts.delves", "Player {} unlocked delve tier {} (account {})",
            player->GetName(), progress.HighestTierUnlocked, accountId);
    }

    // Save progress and mirror it to the client UI
    SaveProgress(accountId, progress);
    PublishProgress(player, progress);

    TC_LOG_DEBUG("scripts.delves", "Awarded delve completion to {}: tier={}, bountiful={}, revives={}, weekly={}/{}",
        player->GetName(), tier, bountiful, hasRevivesRemaining,
        progress.WeeklyCompletions, progress.HighestTierThisWeek);
}

void DelvesRewards::AwardCrests(Player* player, uint8 tier)
{
    DelveTierReward const* reward = sDelveMgr->GetTierReward(tier);
    if (!reward || reward->CrestType == CREST_NONE || reward->CrestCount == 0)
        return;

    // CrestType -> crest currency (Midnight S1 Dawncrest ladder; config-tunable, guarded on CurrencyTypes.db2
    // so a wrong/absent id is a safe no-op). Delves top out at Hero crests (T11), matching retail.
    uint32 currencyId = 0;
    switch (reward->CrestType)
    {
        case CREST_WEATHERED: currencyId = uint32(sConfigMgr->GetIntDefault("Delves.Crest.Tier1.CurrencyId", 3383)); break; // Adventurer Dawncrest
        case CREST_CARVED:    currencyId = uint32(sConfigMgr->GetIntDefault("Delves.Crest.Tier2.CurrencyId", 3341)); break; // Veteran Dawncrest
        case CREST_RUNED:     currencyId = uint32(sConfigMgr->GetIntDefault("Delves.Crest.Tier3.CurrencyId", 3343)); break; // Champion Dawncrest
        case CREST_GILDED:    currencyId = uint32(sConfigMgr->GetIntDefault("Delves.Crest.Tier4.CurrencyId", 3345)); break; // Hero Dawncrest
        default: break;
    }

    if (currencyId && sCurrencyTypesStore.LookupEntry(currencyId))
        player->AddCurrency(currencyId, reward->CrestCount, CurrencyGainSource::Loot);

    TC_LOG_DEBUG("scripts.delves", "Awarded {} crests (currency {}) to {} for tier {}",
        reward->CrestCount, currencyId, player->GetName(), tier);
}

void DelvesRewards::AwardCompanionXP(Player* player, uint8 tier)
{
    // Base XP scales with tier
    uint32 xpAmount = 1000 + (tier * 500);

    CompanionState state;
    DelvesCompanion::LoadFromDB(player->GetSession()->GetBattlenetAccountId(), state);
    DelvesCompanion::AwardCompanionXP(player->GetSession()->GetBattlenetAccountId(), state, xpAmount);

    TC_LOG_DEBUG("scripts.delves", "Awarded {} companion XP to {} (level {} -> {})",
        xpAmount, player->GetName(), state.Level, state.Level);
}

bool DelvesRewards::HasCofferKey(Player* player)
{
    // Check for Restored Coffer Key currency (ID 3028)
    return player->HasCurrency(CURRENCY_RESTORED_COFFER_KEY, 1);
}

void DelvesRewards::ConsumeCofferKey(Player* player)
{
    player->RemoveCurrency(CURRENCY_RESTORED_COFFER_KEY, 1);
    TC_LOG_DEBUG("scripts.delves", "Consumed Coffer Key for {}", player->GetName());
}

void DelvesRewards::AwardCofferKeyShards(Player* player, uint32 amount)
{
    uint32 accountId = player->GetSession()->GetBattlenetAccountId();

    DelveProgress progress;
    LoadProgress(accountId, progress);

    // Enforce weekly cap
    uint32 remaining = MAX_COFFER_KEY_SHARDS_PER_WEEK > progress.WeeklyCofferShards
        ? MAX_COFFER_KEY_SHARDS_PER_WEEK - progress.WeeklyCofferShards
        : 0;

    uint32 actualAmount = std::min(amount, remaining);
    if (actualAmount == 0)
        return;

    progress.WeeklyCofferShards += actualAmount;
    SaveProgress(accountId, progress);
    PublishProgress(player, progress);

    // Auto-convert shards to keys (100 shards = 1 key)
    // This happens when entering a delve, but we track the shards here
    TC_LOG_DEBUG("scripts.delves", "Awarded {} coffer key shards to {} (total: {}/{})",
        actualAmount, player->GetName(), progress.WeeklyCofferShards, MAX_COFFER_KEY_SHARDS_PER_WEEK);
}

void DelvesRewards::UpdateGreatVaultProgress(Player* player, uint8 tier)
{
    if (tier < DELVE_TIER_ENDGAME_START) // Only tier 4+ counts for vault
        return;

    // Great Vault tracking uses the existing WeeklyRewardChest system
    // Completions at 2/4/8 unlock vault slots
    // The highest tier completed determines the vault reward ilvl
    // TODO: Integrate with existing WeeklyRewardChestActivity system
    TC_LOG_DEBUG("scripts.delves", "Updated Great Vault progress for {} (tier {})", player->GetName(), tier);
}

uint8 DelvesRewards::GetGreatVaultSlotCount(uint32 weeklyCompletions)
{
    if (weeklyCompletions >= VAULT_SLOT_3_COMPLETIONS) // 8
        return 3;
    if (weeklyCompletions >= VAULT_SLOT_2_COMPLETIONS) // 4
        return 2;
    if (weeklyCompletions >= VAULT_SLOT_1_COMPLETIONS) // 2
        return 1;
    return 0;
}

bool DelvesRewards::CanUnlockNextTier(uint8 currentHighest, uint8 completedTier, bool hasRevivesRemaining)
{
    // Must complete at current highest tier with revives remaining to unlock next
    if (!hasRevivesRemaining)
        return false;
    if (completedTier < currentHighest)
        return false;
    if (completedTier >= MAX_DELVE_TIER)
        return false;
    return completedTier == currentHighest;
}

void DelvesRewards::LoadProgress(uint32 battlenetAccountId, DelveProgress& progress)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_DELVE_PROGRESS);
    stmt->setUInt32(0, battlenetAccountId);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        Field* fields = result->Fetch();
        progress.HighestTierUnlocked  = fields[0].GetUInt8();
        progress.WeeklyCompletions    = fields[1].GetUInt32();
        progress.HighestTierThisWeek  = fields[2].GetUInt8();
        progress.WeeklyBountifulCount = fields[3].GetUInt32();
        progress.WeeklyCofferShards   = fields[4].GetUInt32();
    }
}

void DelvesRewards::SaveProgress(uint32 battlenetAccountId, DelveProgress const& progress)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_DELVE_PROGRESS);
    stmt->setUInt32(0, battlenetAccountId);
    stmt->setUInt8(1, progress.HighestTierUnlocked);
    stmt->setUInt32(2, progress.WeeklyCompletions);
    stmt->setUInt8(3, progress.HighestTierThisWeek);
    stmt->setUInt32(4, progress.WeeklyBountifulCount);
    stmt->setUInt32(5, progress.WeeklyCofferShards);
    CharacterDatabase.Execute(stmt);
}

void DelvesRewards::ResetWeeklyProgress(uint32 battlenetAccountId, DelveProgress& progress)
{
    progress.WeeklyCompletions = 0;
    progress.HighestTierThisWeek = 0;
    progress.WeeklyBountifulCount = 0;
    progress.WeeklyCofferShards = 0;
    SaveProgress(battlenetAccountId, progress);
}

void DelvesRewards::ResetAllWeeklyProgress()
{
    // Weekly rollover for every account at once; online players' cached progress reloads on next use.
    CharacterDatabase.Execute("UPDATE delve_progress SET weeklyCompletions = 0, highestTierThisWeek = 0, "
        "weeklyBountifulCount = 0, weeklyCofferShards = 0");
    TC_LOG_INFO("delves", "DelvesRewards: weekly delve progress reset.");
}

void DelvesRewards::PublishProgress(Player* player)
{
    DelveProgress progress;
    LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
    PublishProgress(player, progress);
}

void DelvesRewards::PublishProgress(Player* player, DelveProgress const& progress)
{
    // Push account progression into the client's JamDelveData mirror (the map at
    // CGActivePlayer_C+0x1F08, delivered inside SMSG_UPDATE_OBJECT). Without this
    // the delve UI never sees highest-unlocked / last-selected state from us.
    //
    // // UNVERIFIED — needs sniff: entry KEY (we use the active delves season ID —
    // the 68275 RE says "season or scenario id") and the progression-entry field
    // semantics. Wire widths/order are byte-exact — see Player::SetDelveProgressData.
    uint32 seasonId = DelvesSeason::GetCurrentSeasonNumber();
    if (!seasonId)
    {
        TC_LOG_DEBUG("scripts.delves", "PublishProgress: no active delves season, skipping publish for {}",
            player->GetName());
        return;
    }

    player->SetDelveProgressData(int32(seasonId), int32(player->m_delveSelectedMapId),
        int32(progress.HighestTierUnlocked),
        { int32(progress.WeeklyCompletions), int32(progress.HighestTierThisWeek),
          int32(progress.WeeklyBountifulCount), int32(progress.WeeklyCofferShards) });
}

uint8 DelvesRewards::GetItemContextForTier(uint8 tier, bool bountiful)
{
    // Maps tier to ItemContext enum values (from DBCEnums.h)
    // Delves_1=104, Delves_2=106, Delves_3=107
    // Delves_Key_1..8 = 109..116 (bountiful with key)
    // Delves_Bounty_1..8 = 117..124 (bountiful)

    if (bountiful && tier >= 1 && tier <= 8)
        return 116 + tier; // Delves_Bounty_1(117) through Delves_Bounty_8(124)

    switch (tier)
    {
        case 1: case 2: case 3:
            return 104; // Delves_1 (Explorer)
        case 4: case 5:
            return 106; // Delves_2 (Adventurer/Veteran)
        case 6:
            return 106; // Delves_2
        case 7: case 8: case 9: case 10: case 11:
            return 107; // Delves_3 (Champion)
        default:
            return 104;
    }
}

DelvesRewards::CrestType DelvesRewards::GetCrestTypeForTier(uint8 tier)
{
    if (tier <= 2)  return CREST_NONE;
    if (tier <= 5)  return CREST_WEATHERED;
    if (tier <= 7)  return CREST_CARVED;
    if (tier <= 10) return CREST_RUNED;
    return CREST_GILDED; // Tier 11
}

} // namespace Delves

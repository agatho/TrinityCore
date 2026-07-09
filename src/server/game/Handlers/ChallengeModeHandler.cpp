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

#include "WorldSession.h"
#include "ChallengeMode.h"
#include "ChallengeModeMgr.h"
#include "ChallengeModePackets.h"
#include "CharacterDatabase.h"
#include "Item.h"
#include "ItemBonusMgr.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "MythicPlusData.h"
#include "Player.h"

namespace
{
    // Rolls the configured reward pool once (personal loot, tagged with the given context) and returns a single
    // item id, or 0 if nothing rolled / the pool is empty.
    uint32 RollMythicPlusRewardItem(Player* player, uint32 lootId, ItemContext context)
    {
        Loot loot(player->GetMap(), ObjectGuid::Empty, LOOT_NONE, nullptr);
        loot.FillLoot(lootId, LootTemplates_Reference, player, true /*personal*/, true /*noEmptyError*/, LOOT_MODE_DEFAULT, context);
        for (LootItem const& item : loot.items)
            if (item.itemid)
                return item.itemid;
        return 0;
    }

    // Item bonuses that scale a reward item to the Mythic+ item level for the given context + keystone level.
    std::vector<int32> MythicPlusRewardBonuses(uint32 itemId, ItemContext context, int32 keystoneLevel)
    {
        return ItemBonusMgr::GetBonusListsForItem(itemId, ItemBonusMgr::ItemBonusGenerationParams(context, keystoneLevel));
    }

    // Grants one item (bags, or mail on a full bag) carrying the given scaled bonuses.
    void GrantMythicPlusItem(Player* player, uint32 itemId, ItemContext context, std::vector<int32> const& bonuses)
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1) == EQUIP_ERR_OK)
        {
            player->StoreNewItem(dest, itemId, true, 0, GuidSet(), context, &bonuses);
        }
        else if (Item* item = Item::CreateItem(itemId, 1, context, player, false))
        {
            item->SetBonuses(bonuses);
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            item->SaveToDB(trans);
            MailDraft("Great Vault Reward", "Your Great Vault reward.")
                .AddItem(item)
                .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
            CharacterDatabase.CommitTransaction(trans);
        }
    }
}

void WorldSession::HandleRequestMythicPlusSeasonData(WorldPackets::ChallengeMode::RequestMythicPlusSeasonData& /*requestMythicPlusSeasonData*/)
{
    WorldPackets::ChallengeMode::MythicPlusSeasonData response;
    response.IsMythicPlusActive = sChallengeModeMgr.GetActiveSeasonId() != 0;
    SendPacket(response.Write());
}

void WorldSession::HandleRequestMythicPlusAffixes(WorldPackets::ChallengeMode::RequestMythicPlusAffixes& /*requestMythicPlusAffixes*/)
{
    WorldPackets::ChallengeMode::MythicPlusCurrentAffixes response;

    int32 const seasonId = int32(sChallengeModeMgr.GetActiveSeasonId());
    for (uint32 affixId : sChallengeModeMgr.GetWeeklyAffixes())
    {
        WorldPackets::ChallengeMode::CurrentAffix& affix = response.Affixes.emplace_back();
        affix.KeystoneAffixID = int32(affixId);
        affix.SeasonID = seasonId;
    }

    SendPacket(response.Write());
}

void WorldSession::HandleStartChallengeMode(WorldPackets::ChallengeMode::StartChallengeMode& startChallengeMode)
{
    Player* player = GetPlayer();

    // The keystone the player slotted into the font of power.
    Item* keystone = player->GetItemByPos(startChallengeMode.Bag, startChallengeMode.Slot);
    if (!keystone)
    {
        player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    uint32 const mapChallengeModeId = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID);
    uint32 const keystoneLevel = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL);
    if (!mapChallengeModeId || !keystoneLevel)
        return;

    // The player must be standing in the matching Mythic Keystone instance for that dungeon.
    Map* map = player->GetMap();
    InstanceMap* instanceMap = map->ToInstanceMap();
    if (!instanceMap || !map->IsMythicPlus())
        return;

    if (sChallengeModeMgr.GetMapIdForChallengeMode(mapChallengeModeId) != map->GetId())
        return;

    ChallengeMode* challenge = instanceMap->GetChallengeMode();
    if (!challenge || challenge->IsActive() || challenge->IsCompleted())
        return;

    std::array<uint32, 4> const affixes =
    {
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_1),
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_2),
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_3),
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_4)
    };

    challenge->Start(mapChallengeModeId, keystoneLevel, affixes, player->GetGUID(), keystone->GetGUID());
}

void WorldSession::HandleMythicPlusRequestMapStats(WorldPackets::ChallengeMode::MythicPlusRequestMapStats& /*request*/)
{
    Player* player = GetPlayer();

    WorldPackets::ChallengeMode::MythicPlusAllMapStats response;
    response.Field80 = sChallengeModeMgr.GetActiveSeasonId();

    // One map-stat row per dungeon the player has a recorded best run for. The requester is the sole member; the
    // full party roster is not persisted server-side. MapChallengeModeID/BestLevel/DurationMs/Affixes are populated;
    // the remaining scalar slots await a live sniff to map (the wire is exact, so a zero there is not a desync).
    if (MythicPlusData* data = player->GetMythicPlusData())
    {
        for (auto const& [challengeModeId, run] : data->GetBestRuns())
        {
            WorldPackets::ChallengeMode::MythicPlusMapStat& mapStat = response.MapStats.emplace_back();
            mapStat.MapChallengeModeID = challengeModeId;
            mapStat.BestLevel = run.Level;
            mapStat.DurationMs = run.DurationMs;
            mapStat.Affixes = run.Affixes;

            WorldPackets::ChallengeMode::MythicPlusMapStatMember& member = mapStat.Members.emplace_back();
            member.PlayerGUID = player->GetGUID();
        }
    }

    SendPacket(response.Write());
}

void WorldSession::HandleRequestWeeklyRewards(WorldPackets::ChallengeMode::RequestWeeklyRewards& /*request*/)
{
    Player* player = GetPlayer();

    WorldPackets::ChallengeMode::WeeklyRewardsProgressResult response;

    MythicPlusData* data = player->GetMythicPlusData();
    uint32 const runCount = data ? data->GetWeeklyRunCount() : 0;

    // The live Mythic+ vault thresholds (WeeklyRewardChestThreshold.db2, Type=MythicPlus): slots 0/1/2 unlock at
    // 1/4/8 completed runs this week. Missing DB2 -> empty -> no M+ row (safe).
    std::vector<ChallengeModeMgr::VaultThreshold> const thresholds = sChallengeModeMgr.GetMythicPlusVaultThresholds();

    // A single M+ activity tier so the vault shows the dungeon row. Type=1 (MythicPlus) is authoritative; Level is
    // the best slot's keystone level, Points the run count. ActivityTierID is left 0 (the WeeklyRewardChestActivityTier
    // field semantics are opaque offline and not fabricated); the client categorises each slot via thresholdID.
    if (!thresholds.empty())
    {
        WorldPackets::ChallengeMode::WeeklyRewardActivityTier& tier = response.ActivityTiers.emplace_back();
        tier.Type = 1;
        tier.Level = data ? int32(data->GetVaultSlotLevel(0)) : 0;
        tier.Points = int32(runCount);
    }

    for (ChallengeModeMgr::VaultThreshold const& threshold : thresholds)
    {
        WorldPackets::ChallengeMode::WeeklyRewardThresholdProgress& progress = response.Progress.emplace_back();
        progress.ThresholdID = int32(threshold.ThresholdID);
        progress.Amount = int32(runCount);
        progress.Level = data ? int32(data->GetVaultSlotLevel(threshold.Index)) : 0;
        progress.Earned = runCount >= threshold.Count;
    }

    SendPacket(response.Write());

    // Reward options: one previewed item per unlocked slot, rolled from the vault pool at that slot's Jackpot
    // item level. The preview is an example (the granted item is rolled fresh on claim); empty when the vault
    // reward pool (ChallengeMode.Vault.LootId) is not configured.
    WorldPackets::ChallengeMode::WeeklyRewardsResult result;
    uint32 const vaultLootId = sChallengeModeMgr.GetVaultRewardLootId();
    bool const poolReady = vaultLootId && LootTemplates_Reference.HaveLootFor(vaultLootId);

    for (ChallengeModeMgr::VaultThreshold const& threshold : thresholds)
    {
        uint32 const slotLevel = data ? data->GetVaultSlotLevel(threshold.Index) : 0;
        if (!slotLevel)
            continue;   // locked slot -> no reward option

        WorldPackets::ChallengeMode::WeeklyRewardActivity& activity = result.Activities.emplace_back();
        activity.ThresholdID = threshold.ThresholdID;

        if (poolReady)
        {
            if (uint32 itemId = RollMythicPlusRewardItem(player, vaultLootId, ItemContext::MythicPlus_Jackpot))
            {
                WorldPackets::ChallengeMode::WeeklyReward& reward = activity.Rewards.emplace_back();
                reward.HasItem = true;
                reward.Item.ItemID = itemId;

                std::vector<int32> bonuses = MythicPlusRewardBonuses(itemId, ItemContext::MythicPlus_Jackpot, int32(slotLevel));
                if (!bonuses.empty())
                {
                    WorldPackets::Item::ItemBonuses& itemBonus = reward.Item.ItemBonus.emplace();
                    itemBonus.Context = ItemContext::MythicPlus_Jackpot;
                    itemBonus.BonusListIDs = std::move(bonuses);
                }
            }
        }
    }

    SendPacket(result.Write());
}

void WorldSession::HandleClaimWeeklyReward(WorldPackets::ChallengeMode::ClaimWeeklyReward& claim)
{
    Player* player = GetPlayer();
    MythicPlusData* data = player->GetMythicPlusData();

    WorldPackets::ChallengeMode::WeeklyRewardClaimResult result;

    // Server-authoritative validation: needs data, an unclaimed week, and the requested slot actually unlocked.
    // RewardID is assumed to be the WeeklyRewardChestThreshold.ID of the slot (not yet sniff-confirmed); an id
    // that doesn't match an unlocked slot is rejected, so a wrong id never yields a reward.
    uint32 rewardLevel = 0;
    if (data && !data->IsVaultClaimedThisWeek())
    {
        for (ChallengeModeMgr::VaultThreshold const& threshold : sChallengeModeMgr.GetMythicPlusVaultThresholds())
        {
            if (threshold.ThresholdID != claim.RewardID)
                continue;
            rewardLevel = data->GetVaultSlotLevel(threshold.Index);
            break;
        }
    }

    if (!rewardLevel)
    {
        result.Result = 1;      // not claimable: already claimed, locked slot, or unknown id
        SendPacket(result.Write());
        return;
    }

    // Grant one vault item at the slot's Jackpot item level, then lock the vault for the rest of the week.
    if (uint32 vaultLootId = sChallengeModeMgr.GetVaultRewardLootId())
        if (LootTemplates_Reference.HaveLootFor(vaultLootId))
            if (uint32 itemId = RollMythicPlusRewardItem(player, vaultLootId, ItemContext::MythicPlus_Jackpot))
                GrantMythicPlusItem(player, itemId, ItemContext::MythicPlus_Jackpot,
                    MythicPlusRewardBonuses(itemId, ItemContext::MythicPlus_Jackpot, int32(rewardLevel)));

    data->SetVaultClaimed();
    result.Result = 0;          // success
    SendPacket(result.Write());
}

void WorldSession::HandleResetChallengeMode(WorldPackets::ChallengeMode::ResetChallengeMode& /*resetChallengeMode*/)
{
    InstanceMap* instanceMap = GetPlayer()->GetMap()->ToInstanceMap();
    if (!instanceMap)
        return;

    // Abort the active run and stop the timer. Trash/boss respawn goes through the standard instance reset path.
    if (ChallengeMode* challenge = instanceMap->GetChallengeMode())
    {
        if (challenge->IsActive())
        {
            challenge->Reset();

            // Notify the party UI that the keystone was reset (SMSG_CHALLENGE_MODE_RESET carries the instance MapID).
            WorldPackets::ChallengeMode::ChallengeModeReset resetPacket;
            resetPacket.MapID = instanceMap->GetId();
            instanceMap->SendToPlayers(resetPacket.Write());
        }
    }
}

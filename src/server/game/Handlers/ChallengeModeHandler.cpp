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
#include "Config.h"
#include "Item.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Map.h"
#include "MythicPlusData.h"
#include "Player.h"

// NOTE (assembly): the Great Vault reward-item logic for the Mythic+ row deliberately does NOT live in this file.
// It is a service on ChallengeModeMgr (BuildMythicPlusVaultOptions / ClaimMythicPlusVaultReward /
// GetMythicPlusVaultSlotForThreshold) so that whichever handler an assembly binds to CMSG_REQUEST_WEEKLY_REWARDS /
// CMSG_CLAIM_WEEKLY_REWARD can drive it. On this branch that is the pair below, over the ChallengeMode packet
// family; integration/all-systems binds WeeklyRewardHandler.cpp over the WeeklyRewards packet family (it also
// serves the Raid and World vault rows) and calls the same service for the Mythic+ row. There is exactly one
// handler bound per opcode in either assembly - the granting rules are shared, never duplicated.

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

    // Only the actual Mythic Keystone item may start a run (config-tunable; 0 disables the check).
    if (uint32 keystoneItemId = sChallengeModeMgr.GetKeystoneItemId())
        if (keystone->GetEntry() != keystoneItemId)
            return;

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

    // Retail activates the run from the Font of Power pedestal. When enforced, require the pedestal gameobject
    // near the player; lenient by default because the GO spawn is world-DB content.
    if (sConfigMgr->GetBoolDefault("ChallengeMode.RequireFontOfPower", false))
        if (!player->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_CHALLENGE_MODE_REWARD, 40.0f))
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

    // Opening the Great Vault after a reset grants a fresh keystone when the player has none (retail rule) and
    // applies the pending weekly level adjustment / affix restamp.
    sChallengeModeMgr.UpdateKeystoneForNewWeek(player, true /*createIfMissing*/);

    WorldPackets::ChallengeMode::WeeklyRewardsProgressResult response;

    MythicPlusData* data = player->GetMythicPlusData();
    uint32 const runCount = data ? data->GetWeeklyRunCount() : 0;

    // The live Mythic+ vault thresholds (WeeklyRewardChestThreshold.db2, Type=MythicPlus): slots 0/1/2 unlock at
    // 1/4/8 completed runs this week. Missing DB2 -> empty -> no M+ row (safe).
    std::vector<ChallengeModeMgr::VaultThreshold> const thresholds = sChallengeModeMgr.GetMythicPlusVaultThresholds();

    // A single M+ activity tier so the vault shows the dungeon row. Type=1 (MythicPlus) is authoritative; Level is
    // the best slot's keystone level, Points the run count. ActivityTierID comes from the active season's
    // MythicPlusSeasonRewardLevels rows (0 when the DB2 has no data for the season).
    if (!thresholds.empty())
    {
        WorldPackets::ChallengeMode::WeeklyRewardActivityTier& tier = response.ActivityTiers.emplace_back();
        tier.ActivityTierID = sChallengeModeMgr.GetVaultActivityTierId();
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
    // item level. The preview is an example (the granted item is rolled fresh on claim); the option carries no
    // item when the vault reward pool (ChallengeMode.Vault.LootId) is not configured.
    WorldPackets::ChallengeMode::WeeklyRewardsResult result;

    for (ChallengeModeMgr::VaultRewardOption& option : sChallengeModeMgr.BuildMythicPlusVaultOptions(player))
    {
        WorldPackets::ChallengeMode::WeeklyRewardActivity& activity = result.Activities.emplace_back();
        activity.ThresholdID = option.ThresholdID;

        if (!option.ItemID)
            continue;

        WorldPackets::ChallengeMode::WeeklyReward& reward = activity.Rewards.emplace_back();
        reward.HasItem = true;
        reward.Item.ItemID = option.ItemID;

        if (!option.BonusListIDs.empty())
        {
            WorldPackets::Item::ItemBonuses& itemBonus = reward.Item.ItemBonus.emplace();
            itemBonus.Context = ItemContext::MythicPlus_Jackpot;
            itemBonus.BonusListIDs = std::move(option.BonusListIDs);
        }
    }

    SendPacket(result.Write());
}

void WorldSession::HandleClaimWeeklyReward(WorldPackets::ChallengeMode::ClaimWeeklyReward& claim)
{
    Player* player = GetPlayer();

    // RewardID is the WeeklyRewardChestThreshold.ID of the chosen slot (not yet sniff-confirmed); an id that is
    // not a live Mythic+ slot is rejected outright, so a wrong id can never yield a reward.
    uint32 const slotIndex = sChallengeModeMgr.GetMythicPlusVaultSlotForThreshold(claim.RewardID);

    // All validation + granting lives in the shared service (see the note at the top of this file).
    ChallengeModeMgr::VaultClaimResult const claimResult = slotIndex != ChallengeModeMgr::VAULT_SLOT_NONE
        ? sChallengeModeMgr.ClaimMythicPlusVaultReward(player, slotIndex)
        : ChallengeModeMgr::VaultClaimResult::NotClaimable;

    WorldPackets::ChallengeMode::WeeklyRewardClaimResult result;
    result.Result = claimResult == ChallengeModeMgr::VaultClaimResult::Success ? 0 : 1;
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

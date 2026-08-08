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
#include "ChallengeModeMgr.h"
#include "Log.h"
#include "Player.h"
#include "WeeklyRewardsMgr.h"
#include "WeeklyRewardsPackets.h"

namespace
{
    // SMSG_WEEKLY_REWARD_CLAIM_RESULT payload, the client's WeeklyRewardChestClaimRewardResult enum
    // (12.0.7.68275 enum-reflection registrar sub_7FF72A0F6780, Meta NumValues=8 MinValue=0 MaxValue=7).
    enum class ClaimResult : uint8
    {
        Success             = 0,
        InvalidThreshold    = 1,
        PlayerNotFound      = 2,
        InvalidSlot         = 3,
        TooManyItems        = 4,
        DbError             = 5,
        LockFailure         = 6,
        CountExceeded       = 7
    };
}

// Projects the player's tracked weekly activity into the vault packets. Each of the three activity rows
// (Dungeon / Raid / World) contributes its live WeeklyRewardChestThreshold.db2 slots; a slot is earned once the
// row's completion count reaches the slot's threshold.
//
// The ThresholdID on the wire is the real DB2 row id, because that is the ONLY thing that tells the client which
// row a progress record belongs to: it resolves ThresholdID -> WeeklyRewardChestThreshold.db2 -> Type, and
// Blizzard_WeeklyRewards.lua:SetUpConditionalActivities only shows the World row when at least one activity of
// Enum.WeeklyRewardChestThresholdType.World (= 6) came back (otherwise it shows the PvP row instead).
void WorldSession::HandleRequestWeeklyRewards(WorldPackets::WeeklyRewards::RequestWeeklyRewards& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Mythic+ (feature/mythic-plus): opening the vault after a weekly reset is also where the carried keystone is
    // refreshed - a fresh key when the player has none, plus the pending level adjustment and affix restamp. It
    // lives here because this is the handler bound to CMSG_REQUEST_WEEKLY_REWARDS.
    sChallengeModeMgr.UpdateKeystoneForNewWeek(player, true /*createIfMissing*/);

    WeeklyRewards::CharacterVault const& vault = sWeeklyRewardsMgr.GetVault(player->GetGUID());

    WorldPackets::WeeklyRewards::WeeklyRewardsProgressResult progress;
    progress.CanClaim = sWeeklyRewardsMgr.HasUnclaimedReward(player->GetGUID());

    WorldPackets::WeeklyRewards::WeeklyRewardsResult rewards;

    for (uint8 t = 0; t < uint8(WeeklyRewards::ActivityType::Max); ++t)
    {
        WeeklyRewards::ActivityType const type = WeeklyRewards::ActivityType(t);
        WeeklyRewards::ActivityRow const& row = vault.Rows[t];

        // No live DB2 slots -> the row is simply absent from the vault, rather than shown with invented ones.
        for (WeeklyRewards::VaultSlot const& slot : WeeklyRewards::SlotsFor(type))
        {
            uint32 const slotLevel = WeeklyRewardsMgr::GetSlotLevel(row, slot);

            WorldPackets::WeeklyRewards::WeeklyRewardProgress p;
            p.ThresholdID = slot.ThresholdID;
            p.Amount = row.Count;
            // ActivityTierID indexes WeeklyRewardChestActivityTier.db2 (the client feeds it to
            // GetDifficultyIDForActivityTier / GetNextActivitiesIncrease). Nothing server-side maps a row+level
            // onto a tier id yet, so it stays 0 (unmapped) instead of a made-up id; both client call sites
            // degrade gracefully - the World row falls back to "next tier = level + 1".
            p.ActivityTierID = 0;
            // Level is what the row displays: keystone level for Dungeon, delve TIER for World
            // (Blizzard_WeeklyRewards.lua renders GREAT_VAULT_WORLD_TIER:format(activityInfo.level)).
            p.Level = slotLevel;
            p.Earned = slotLevel != 0;

            // The item level the client shows comes from the preview item's bonus lists, not from any number in
            // this packet (WEEKLY_REWARDS_ITEM_LEVEL_WORLD is filled from GetExampleRewardItemHyperlinks). So an
            // earned slot only claims to pay something when there is a real, DB2-scaled item behind it.
            if (p.Earned)
            {
                if (Optional<WeeklyRewards::SlotReward> reward = sWeeklyRewardsMgr.BuildSlotReward(player, type, slotLevel))
                {
                    WorldPackets::Item::ItemInstance& example = p.ExampleItem.emplace();
                    example.ItemID = reward->ItemID;
                    if (!reward->BonusListIDs.empty())
                    {
                        WorldPackets::Item::ItemBonuses& bonuses = example.ItemBonus.emplace();
                        bonuses.Context = reward->Context;
                        bonuses.BonusListIDs = reward->BonusListIDs;
                    }

                    WorldPackets::WeeklyRewards::WeeklyRewardThreshold& threshold = rewards.Thresholds.emplace_back();
                    threshold.ThresholdID = slot.ThresholdID;
                    WorldPackets::WeeklyRewards::WeeklyRewardItem& choice = threshold.Rewards.emplace_back();
                    // Field mapping proven by the client-side builder of WeeklyRewardActivityRewardInfo
                    // (sub_7FF72AEEA3A0, 12.0.7.68275): it emits {type = JamWeeklyReward.type,
                    // id = item.ItemID for type 1 / currencyType for type 2, quantity = JamWeeklyReward.value,
                    // itemDBID = JamWeeklyReward.itemDBID} - so Value is a QUANTITY, not the item id, and the id
                    // is read out of the embedded ItemInstance. The same code SKIPS a type-1 reward unless BOTH
                    // the itemDBID and the item are present, and a slot whose rewards list ends up empty is
                    // treated by Blizzard_WeeklyRewards.lua as having no reward at all.
                    choice.Type = 1;                    // CachedRewardType::Item (registrar: None=0, Item=1, Currency=2, Quest=3)
                    choice.Value = 1;                   // quantity
                    choice.Item = example;
                    // The client only ever uses itemDBID as an opaque handle (GetDisplayedItemDBID ->
                    // GetItemHyperlink, and the confirm popup), and it must be present for the reward to show.
                    // Retail's is the db id of the pre-generated vault item; nothing is pre-generated here, so a
                    // stable per-character/per-slot handle stands in. It is never used as a reward value.
                    choice.ItemDBID = (uint64(player->GetGUID().GetCounter()) << 32) | slot.ThresholdID;
                }
                else if (WeeklyRewards::HasRewardContext(type))
                {
                    // The row CAN pay - its loot pool is unset or empty. That is an operator-visible content gap,
                    // so it is an error: show the progress, offer no reward, and refuse the claim in
                    // HandleClaimWeeklyReward rather than silently consuming the week.
                    TC_LOG_ERROR("network", "Great Vault: {} earned threshold {} (row {}, level {}) but its reward "
                        "pool is unconfigured or empty (WeeklyRewards.Vault.{}.LootId). The slot is shown without "
                        "a reward and cannot be claimed.", player->GetGUID().ToString(), slot.ThresholdID, t,
                        slotLevel, type == WeeklyRewards::ActivityType::World ? "World" : "Dungeon");
                }
                // Rows with no reward ItemContext at all (Raid) show progress only - a known structural gap, not
                // a misconfiguration, so it is not logged per vault open.
            }

            progress.Progress.push_back(std::move(p));
        }
    }

    SendPacket(progress.Write());
    SendPacket(rewards.Write());
}

void WorldSession::HandleClaimWeeklyReward(WorldPackets::WeeklyRewards::ClaimWeeklyReward& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::WeeklyRewards::WeeklyRewardClaimResult result;

    // RewardIndex is the chosen activity's WeeklyRewardChestThreshold.db2 id: the client's
    // C_WeeklyRewards.ClaimReward() is called with WeeklyRewardActivityInfo.id, which is the row id it was sent.
    WeeklyRewards::ActivityType type = WeeklyRewards::ActivityType::Dungeon;
    WeeklyRewards::VaultSlot slot;
    if (!sWeeklyRewardsMgr.FindSlot(packet.RewardIndex, type, slot))
    {
        result.Result = uint8(ClaimResult::InvalidThreshold);
        SendPacket(result.Write());
        return;
    }

    WeeklyRewards::CharacterVault const& vault = sWeeklyRewardsMgr.GetVault(player->GetGUID());
    uint32 const slotLevel = WeeklyRewardsMgr::GetSlotLevel(vault.Rows[uint8(type)], slot);
    if (!slotLevel || vault.ClaimedPeriod == vault.Period)
    {
        result.Result = uint8(ClaimResult::InvalidSlot);
        SendPacket(result.Write());
        return;
    }

    // A row that cannot generate a reward at all (Raid) is never offered as a choice; refuse it outright.
    if (!WeeklyRewards::HasRewardContext(type))
    {
        result.Result = uint8(ClaimResult::InvalidSlot);
        SendPacket(result.Write());
        return;
    }

    // Roll the reward BEFORE consuming the week, so an unconfigured pool refuses the claim instead of burning it.
    Optional<WeeklyRewards::SlotReward> reward = sWeeklyRewardsMgr.BuildSlotReward(player, type, slotLevel);
    if (!reward)
    {
        TC_LOG_ERROR("network", "Great Vault: {} tried to claim threshold {} (row {}, level {}) but its reward pool "
            "is unconfigured or empty. The week is left unclaimed.",
            player->GetGUID().ToString(), packet.RewardIndex, uint32(type), slotLevel);
        result.Result = uint8(ClaimResult::DbError);
        SendPacket(result.Write());
        return;
    }

    if (!sWeeklyRewardsMgr.MarkClaimed(player->GetGUID()))
    {
        result.Result = uint8(ClaimResult::InvalidSlot);
        SendPacket(result.Write());
        return;
    }

    sWeeklyRewardsMgr.GrantSlotReward(player, *reward);

    result.Result = uint8(ClaimResult::Success);
    SendPacket(result.Write());

    TC_LOG_DEBUG("network", "CMSG_CLAIM_WEEKLY_REWARD: {} claimed threshold {} (row {}, level {}) -> item {}",
        player->GetGUID().ToString(), packet.RewardIndex, uint32(type), slotLevel, reward->ItemID);
}

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
#include "Log.h"
#include "Player.h"
#include "WeeklyRewardsMgr.h"
#include "WeeklyRewardsPackets.h"

// Projects the player's tracked weekly activity into the vault progress packet: three reward slots per activity row
// (Dungeon / Raid / World), each earned once its completion count reaches the slot threshold.
void WorldSession::HandleRequestWeeklyRewards(WorldPackets::WeeklyRewards::RequestWeeklyRewards& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WeeklyRewards::CharacterVault const& vault = sWeeklyRewardsMgr.GetVault(player->GetGUID());

    WorldPackets::WeeklyRewards::WeeklyRewardsProgressResult progress;
    progress.CanClaim = sWeeklyRewardsMgr.HasUnclaimedReward(player->GetGUID());

    WorldPackets::WeeklyRewards::WeeklyRewardsResult rewards;

    for (uint8 t = 0; t < uint8(WeeklyRewards::ActivityType::Max); ++t)
    {
        WeeklyRewards::ActivityType const type = WeeklyRewards::ActivityType(t);
        WeeklyRewards::ActivityRow const& row = vault.Rows[t];
        std::array<uint32, 3> const& thresholds = WeeklyRewards::ThresholdsFor(type);

        for (uint8 slot = 0; slot < thresholds.size(); ++slot)
        {
            uint32 const thresholdId = uint32(t) * 3 + slot;   // synthetic per-row/slot id

            WorldPackets::WeeklyRewards::WeeklyRewardProgress p;
            p.ThresholdID = thresholdId;
            p.Amount = row.Count;
            p.ActivityTierID = uint32(t);
            p.Level = row.BestLevel;
            p.Earned = row.Count >= thresholds[slot];
            progress.Progress.push_back(p);

            // Earned slots also appear as choices in the vault result. The concrete reward item is content-driven
            // (the reward pool DB2 / loot table), so the slot is advertised with its tier (Type = activity row,
            // Value = the best level that seeded it) and the item is left absent rather than fabricated.
            if (p.Earned)
            {
                WorldPackets::WeeklyRewards::WeeklyRewardThreshold threshold;
                threshold.ThresholdID = thresholdId;
                WorldPackets::WeeklyRewards::WeeklyRewardItem& reward = threshold.Rewards.emplace_back();
                reward.Type = uint32(t);
                reward.Value = row.BestLevel;
                rewards.Thresholds.push_back(std::move(threshold));
            }
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

    // The claim is gated to once per weekly period and only when a reward was actually earned. The chosen slot
    // (packet.RewardIndex) selects which item to receive; resolving/granting that concrete item is content-driven
    // (reward pool) and handled in a later phase, but the weekly claim gate itself is authoritative here.
    bool const claimed = sWeeklyRewardsMgr.MarkClaimed(player->GetGUID());

    WorldPackets::WeeklyRewards::WeeklyRewardClaimResult result;
    result.Result = claimed ? 0 /*Success*/ : 1 /*NoRewardAvailable*/;
    SendPacket(result.Write());

    if (claimed)
        TC_LOG_DEBUG("network", "CMSG_CLAIM_WEEKLY_REWARD: {} claimed weekly reward slot {}",
            player->GetGUID().ToString(), packet.RewardIndex);
}

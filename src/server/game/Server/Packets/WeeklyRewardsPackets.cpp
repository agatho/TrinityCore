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

#include "WeeklyRewardsPackets.h"

namespace WorldPackets::WeeklyRewards
{
void ClaimWeeklyReward::Read()
{
    _worldPacket >> RewardIndex;
}

// One reward option, mirror of client reader sub_7FF72915E170. Presence byte bits: 7=ItemDBID, 6=ItemDateCreated,
// 5=CurrencyType, 4=Item; the Item (ItemInstance) is written first, then the optional scalars, matching the reader.
static ByteBuffer& operator<<(ByteBuffer& data, WeeklyRewardItem const& reward)
{
    data << uint32(reward.Type);
    data << uint32(reward.Value);

    uint8 presence = (reward.ItemDBID ? 0x80 : 0) | (reward.ItemDateCreated ? 0x40 : 0)
        | (reward.CurrencyType ? 0x20 : 0) | (reward.Item ? 0x10 : 0);
    data << uint8(presence);

    if (reward.Item)
        data << *reward.Item;                       // WorldPackets::Item::ItemInstance operator<<
    if (reward.ItemDBID)
        data << uint64(*reward.ItemDBID);
    if (reward.ItemDateCreated)
        data << uint64(*reward.ItemDateCreated);
    if (reward.CurrencyType)
        data << uint32(*reward.CurrencyType);

    return data;
}

WorldPacket const* WeeklyRewardsResult::Write()
{
    _worldPacket << uint32(Thresholds.size());
    _worldPacket << uint32(Field56);
    for (WeeklyRewardThreshold const& threshold : Thresholds)
    {
        _worldPacket << uint32(threshold.ThresholdID);
        _worldPacket << uint32(threshold.Rewards.size());
        for (WeeklyRewardItem const& reward : threshold.Rewards)
            _worldPacket << reward;
    }
    return &_worldPacket;
}

WorldPacket const* WeeklyRewardClaimResult::Write()
{
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

// One threshold-progress record, mirror of the loop in client reader body sub_7FF7290B69B0. Presence byte bits:
// 7=Earned(bool), 6=ExampleItem, 5=UpgradeExampleItem.
static ByteBuffer& operator<<(ByteBuffer& data, WeeklyRewardProgress const& progress)
{
    data << uint32(progress.ThresholdID);
    data << uint32(progress.Amount);
    data << uint32(progress.ActivityTierID);
    data << uint32(progress.Level);

    data << uint32(progress.RaidEncounters.size());
    for (WeeklyRewardEncounter const& enc : progress.RaidEncounters)
    {
        data << uint32(enc.EncounterID);
        data << uint16(enc.BestDifficultyID);
    }

    uint8 presence = (progress.Earned ? 0x80 : 0) | (progress.ExampleItem ? 0x40 : 0)
        | (progress.UpgradeExampleItem ? 0x20 : 0);
    data << uint8(presence);

    if (progress.ExampleItem)
        data << *progress.ExampleItem;
    if (progress.UpgradeExampleItem)
        data << *progress.UpgradeExampleItem;

    return data;
}

WorldPacket const* WeeklyRewardsProgressResult::Write()
{
    _worldPacket << uint8(CanClaim ? 1 : 0);
    _worldPacket << uint32(Progress.size());        // reserve threshold-progress vector
    _worldPacket << uint32(Activities.size());      // reserve activity vector
    _worldPacket << uint32(Field58);

    for (WeeklyRewardActivity const& activity : Activities)
    {
        _worldPacket << uint32(activity.Field0);
        _worldPacket << uint32(activity.Field4);
        _worldPacket << uint32(activity.Field8);
        _worldPacket << uint8(activity.FieldC);
    }

    for (WeeklyRewardProgress const& progress : Progress)
        _worldPacket << progress;

    return &_worldPacket;
}
}

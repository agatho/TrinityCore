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

#ifndef TRINITYCORE_WEEKLY_REWARDS_PACKETS_H
#define TRINITYCORE_WEEKLY_REWARDS_PACKETS_H

#include "Packet.h"
#include "ItemPacketsCommon.h"
#include "Optional.h"
#include <vector>

// The "Great Vault": each week, activity in three categories (Mythic+ dungeons, raids, PvP) fills reward slots;
// the player then picks one item. Wire recovered byte-exact from the client dispatch readers (0x42-family switch
// sub_7FF729103660) -> full layout in c:\dumps\GREAT_VAULT_WIRE_68275.md.
namespace WorldPackets
{
namespace WeeklyRewards
{
    // CMSG_REQUEST_WEEKLY_REWARDS (0x3A026C): empty request to open/refresh the vault (client sub_7FF72914B090).
    class RequestWeeklyRewards final : public ClientPacket
    {
    public:
        explicit RequestWeeklyRewards(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_WEEKLY_REWARDS, std::move(packet)) { }

        void Read() override { }
    };

    // CMSG_CLAIM_WEEKLY_REWARD (0x3A026B): pick a reward slot (client sub_7FF72914B040 = one uint32).
    class ClaimWeeklyReward final : public ClientPacket
    {
    public:
        explicit ClaimWeeklyReward(WorldPacket&& packet) : ClientPacket(CMSG_CLAIM_WEEKLY_REWARD, std::move(packet)) { }

        void Read() override;

        uint32 RewardIndex = 0;
    };

    // One reward option (JamWeeklyReward, client reader sub_7FF72915E170). The four trailing fields are presence-gated.
    struct WeeklyRewardItem
    {
        uint32 Type = 0;
        uint32 Value = 0;
        Optional<Item::ItemInstance> Item;
        Optional<uint64> ItemDBID;
        Optional<uint64> ItemDateCreated;
        Optional<uint32> CurrencyType;
    };

    // One activity's reward slot (JamWeeklyRewardThreshold): a threshold id and the item(s) it grants.
    struct WeeklyRewardThreshold
    {
        uint32 ThresholdID = 0;
        std::vector<WeeklyRewardItem> Rewards;
    };

    // SMSG_WEEKLY_REWARDS_RESULT (0x420305, reader sub_7FF7290B6800): the reward choices in the vault.
    class WeeklyRewardsResult final : public ServerPacket
    {
    public:
        WeeklyRewardsResult() : ServerPacket(SMSG_WEEKLY_REWARDS_RESULT) { }

        WorldPacket const* Write() override;

        uint32 Field56 = 0;                         // header scalar, semantics unconfirmed offline -> 0
        std::vector<WeeklyRewardThreshold> Thresholds;
    };

    // SMSG_WEEKLY_REWARD_CLAIM_RESULT (0x420306, reader sub_7FF7290B6960): { uint8 Result }.
    class WeeklyRewardClaimResult final : public ServerPacket
    {
    public:
        WeeklyRewardClaimResult() : ServerPacket(SMSG_WEEKLY_REWARD_CLAIM_RESULT, 1) { }

        WorldPacket const* Write() override;

        uint8 Result = 0;
    };

    // One activity tier line on the progress screen (16-byte {u32,u32,u32,u8}; leaf semantics opaque -> honest 0s
    // for the unmapped fields).
    struct WeeklyRewardActivity
    {
        uint32 Field0 = 0;
        uint32 Field4 = 0;
        uint32 Field8 = 0;
        uint8 FieldC = 0;
    };

    // One threshold-progress record (JamWeeklyRewardThresholdProgress, part of reader body sub_7FF7290B69B0).
    struct WeeklyRewardEncounter
    {
        uint32 EncounterID = 0;
        uint16 BestDifficultyID = 0;
    };

    struct WeeklyRewardProgress
    {
        uint32 ThresholdID = 0;
        uint32 Amount = 0;
        uint32 ActivityTierID = 0;
        uint32 Level = 0;
        std::vector<WeeklyRewardEncounter> RaidEncounters;
        bool Earned = false;
        Optional<Item::ItemInstance> ExampleItem;
        Optional<Item::ItemInstance> UpgradeExampleItem;
    };

    // SMSG_WEEKLY_REWARDS_PROGRESS_RESULT (0x420307, reader body sub_7FF7290B69B0): the vault progress bars.
    class WeeklyRewardsProgressResult final : public ServerPacket
    {
    public:
        WeeklyRewardsProgressResult() : ServerPacket(SMSG_WEEKLY_REWARDS_PROGRESS_RESULT) { }

        WorldPacket const* Write() override;

        bool CanClaim = false;
        uint32 Field58 = 0;                         // header scalar, semantics unconfirmed offline -> 0
        std::vector<WeeklyRewardActivity> Activities;
        std::vector<WeeklyRewardProgress> Progress;
    };
}
}

#endif // TRINITYCORE_WEEKLY_REWARDS_PACKETS_H

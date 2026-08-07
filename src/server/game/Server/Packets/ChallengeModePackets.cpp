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

#include "ChallengeModePackets.h"
#include "PacketOperators.h"

namespace WorldPackets::ChallengeMode
{
void StartChallengeMode::Read()
{
    _worldPacket >> Bag;
    _worldPacket >> Slot;
    _worldPacket >> GameObjectGUID;
}

WorldPacket const* MythicPlusSeasonData::Write()
{
    _worldPacket << Bits<1>(IsMythicPlusActive);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* MythicPlusCurrentAffixes::Write()
{
    _worldPacket << Size<uint32>(Affixes);
    for (CurrentAffix const& affix : Affixes)
    {
        _worldPacket << int32(affix.KeystoneAffixID);
        _worldPacket << int32(affix.SeasonID);
    }

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, MythicPlusMapStatMember const& member)
{
    data << uint64(member.Field0);
    data << member.PlayerGUID;
    data << member.OwnerGUID;
    data << uint32(member.Field56);
    data << uint32(member.Field60);
    data << uint32(member.Field64);
    data << uint8(member.Flag);
    data << uint32(member.Field72);
    data << uint32(member.Field76);
    data << uint32(member.Field80);
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, MythicPlusMapStat const& mapStat)
{
    data << uint32(mapStat.MapChallengeModeID);
    data << uint32(mapStat.BestLevel);
    data << uint32(mapStat.DurationMs);
    data << uint64(mapStat.Field16);
    data << uint64(mapStat.Field24);
    data << uint32(mapStat.Field32);
    for (uint32 affix : mapStat.Affixes)
        data << uint32(affix);
    data << Size<uint32>(mapStat.Members);
    data << uint32(mapStat.Field64);
    data << uint32(mapStat.Field68);
    for (MythicPlusMapStatMember const& member : mapStat.Members)
        data << member;
    return data;
}

WorldPacket const* MythicPlusAllMapStats::Write()
{
    _worldPacket << Size<uint32>(MapStats);
    _worldPacket << Size<uint32>(SeasonBests);
    _worldPacket << uint32(Field80);
    _worldPacket << uint32(Field84);

    for (MythicPlusMapStat const& mapStat : MapStats)
        _worldPacket << mapStat;

    for (MythicPlusSeasonBest const& best : SeasonBests)
    {
        _worldPacket << uint64(best.Field0);
        _worldPacket << uint32(best.Field8);
        _worldPacket << uint32(best.Field12);
        _worldPacket << uint64(best.Field16);
        _worldPacket << uint64(best.Field24);
        _worldPacket << uint8(best.Flag);
    }

    return &_worldPacket;
}

WorldPacket const* ChallengeModeReset::Write()
{
    _worldPacket << uint32(MapID);
    return &_worldPacket;
}

WorldPacket const* ChallengeModeUpdateDeathCount::Write()
{
    _worldPacket << uint32(DeathCount);
    return &_worldPacket;
}

WorldPacket const* MythicPlusNewWeekRecord::Write()
{
    _worldPacket << uint32(MapChallengeModeID);
    _worldPacket << uint32(CompletionMs);
    _worldPacket << uint32(KeystoneLevel);
    return &_worldPacket;
}

WorldPacket const* ChallengeModeNewPlayerRecord::Write()
{
    _worldPacket << uint32(MapChallengeModeID);
    _worldPacket << uint32(CompletionMs);
    _worldPacket << uint32(KeystoneLevel);
    return &_worldPacket;
}

WorldPacket const* ChallengeModeStart::Write()
{
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(MapChallengeModeID);
    _worldPacket << uint32(KeystoneLevel);
    _worldPacket << uint32(Field44);
    _worldPacket << uint64(DeployedTime);
    for (uint32 affix : Affixes)
        _worldPacket << uint32(affix);
    _worldPacket << uint32(0);          // MemberCount: party roster (720-byte specs/talents element) not populated yet
    _worldPacket << uint8(Flags);

    return &_worldPacket;
}

WorldPacket const* ChallengeModeComplete::Write()
{
    // Byte-for-byte the retail 68275 layout (m+ run12.0.7.pkt): head, score pair, then guid+name members.
    _worldPacket << uint32(MapChallengeModeID);
    _worldPacket << uint32(KeystoneLevel);
    _worldPacket << uint64(CompletionMs);
    _worldPacket << uint32(0);
    _worldPacket << int64(CompletionDate);
    _worldPacket << uint32(0);
    for (uint32 affix : Affixes)
        _worldPacket << uint32(affix);
    _worldPacket << float(Score);
    _worldPacket << uint32(0);
    _worldPacket << uint8(Flags1);
    _worldPacket << float(BestScore);
    _worldPacket << uint32(Names.size());
    _worldPacket << uint32(0);
    _worldPacket << uint8(Flags2);
    for (uint32 i = 0; i < 4; ++i)
        _worldPacket << uint32(0);      // 16-byte zero block (unidentified; zero in the capture)

    for (MemberName const& member : Names)
    {
        _worldPacket << member.PlayerGUID;
        _worldPacket << uint8((member.Name.length() << 2) | (member.IsEligibleForScore ? 2 : 0));
        _worldPacket.append(member.Name.data(), member.Name.length());
    }

    return &_worldPacket;
}

WorldPacket const* WeeklyRewardsProgressResult::Write()
{
    _worldPacket << uint8(Header);
    _worldPacket << uint32(Progress.size());        // thresholdProgressCount (client reserves vector first)
    _worldPacket << uint32(ActivityTiers.size());   // activityTierCount
    _worldPacket << uint32(Unused);                 // header scalar the client reads and discards

    for (WeeklyRewardActivityTier const& tier : ActivityTiers)
    {
        _worldPacket << int32(tier.ActivityTierID);
        _worldPacket << int32(tier.Level);
        _worldPacket << int32(tier.Points);
        _worldPacket << uint8(tier.Type);
    }

    for (WeeklyRewardThresholdProgress const& progress : Progress)
    {
        _worldPacket << int32(progress.ThresholdID);
        _worldPacket << int32(progress.Amount);
        _worldPacket << int32(progress.ActivityTierID);
        _worldPacket << int32(progress.Level);
        _worldPacket << uint32(progress.RaidEncounters.size());
        for (WeeklyRewardRaidEncounter const& enc : progress.RaidEncounters)
        {
            _worldPacket << int32(enc.EncounterID);
            _worldPacket << int16(enc.BestDifficultyID);
        }
        // Flags byte: bit7 earned, bit6/bit5 = example/upgrade item preview present (both 0 -> no item structs follow).
        _worldPacket << uint8(progress.Earned ? 0x80 : 0x00);
    }

    return &_worldPacket;
}

void ClaimWeeklyReward::Read()
{
    _worldPacket >> RewardID;
}

ByteBuffer& operator<<(ByteBuffer& data, WeeklyReward const& reward)
{
    data << int32(reward.Type);
    data << int32(reward.Value);
    // Presence byte: bit6 (0x40) = item preview present; itemDBID/date/currency (bit7/others) all absent.
    data << uint8(reward.HasItem ? 0x40 : 0x00);
    if (reward.HasItem)
        data << reward.Item;    // present optionals are read item-first (sub_7FF72915E170)
    return data;
}

WorldPacket const* WeeklyRewardsResult::Write()
{
    _worldPacket << uint32(Activities.size());
    _worldPacket << uint32(Field56);

    for (WeeklyRewardActivity const& activity : Activities)
    {
        _worldPacket << uint32(activity.ThresholdID);
        _worldPacket << uint32(activity.Rewards.size());
        for (WeeklyReward const& reward : activity.Rewards)
            _worldPacket << reward;
    }

    return &_worldPacket;
}

WorldPacket const* WeeklyRewardClaimResult::Write()
{
    _worldPacket << uint8(Result);
    return &_worldPacket;
}
}

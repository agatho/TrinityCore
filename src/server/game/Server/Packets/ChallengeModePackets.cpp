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

WorldPacket const* MythicPlusAllMapStats::Write()
{
    _worldPacket << Size<uint32>(MapStats);
    _worldPacket << Size<uint32>(SeasonBests);
    _worldPacket << uint32(Field80);
    _worldPacket << uint32(Field84);

    for (MythicPlusMapStat const& mapStat : MapStats)
    {
        _worldPacket << uint32(mapStat.MapChallengeModeID);
        _worldPacket << uint32(mapStat.BestLevel);
        _worldPacket << uint32(mapStat.DurationMs);
        _worldPacket << uint64(mapStat.Field16);
        _worldPacket << uint64(mapStat.Field24);
        _worldPacket << uint32(mapStat.Field32);
        for (uint32 affix : mapStat.Affixes)
            _worldPacket << uint32(affix);
        _worldPacket << Size<uint32>(mapStat.Members);
        _worldPacket << uint32(mapStat.Field64);
        _worldPacket << uint32(mapStat.Field68);
        for (MythicPlusMapStatMember const& member : mapStat.Members)
        {
            _worldPacket << uint64(member.Field0);
            _worldPacket << member.PlayerGUID;
            _worldPacket << member.OwnerGUID;
            _worldPacket << uint32(member.Field56);
            _worldPacket << uint32(member.Field60);
            _worldPacket << uint32(member.Field64);
            _worldPacket << uint8(member.Flag);
            _worldPacket << uint32(member.Field72);
            _worldPacket << uint32(member.Field76);
            _worldPacket << uint32(member.Field80);
        }
    }

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

WorldPacket const* ChallengeModeStart::Write()
{
    _worldPacket << uint32(MapChallengeModeID);
    _worldPacket << uint32(KeystoneLevel);
    _worldPacket << uint32(Field40);
    _worldPacket << uint32(Field44);
    _worldPacket << uint64(DeployedTime);
    for (uint32 affix : Affixes)
        _worldPacket << uint32(affix);
    _worldPacket << uint32(0);          // MemberCount: party roster (720-byte specs/talents element) not populated yet
    _worldPacket << uint8(Flags);

    return &_worldPacket;
}
}

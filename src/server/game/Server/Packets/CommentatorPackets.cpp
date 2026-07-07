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

#include "CommentatorPackets.h"

void WorldPackets::Commentator::CommentatorEnable::Read()
{
    _worldPacket >> Enable;
}

void WorldPackets::Commentator::CommentatorGetMapInfo::Read()
{
    // The player name is length-prefixed by a 6-bit count in the bit stream (matches the client serializer).
    TargetPlayer = _worldPacket.ReadString(_worldPacket.ReadBits(6));
}

void WorldPackets::Commentator::CommentatorEnterInstance::Read()
{
    _worldPacket >> MapID;
    _worldPacket >> InstanceIDLow;
    _worldPacket >> InstanceIDHigh;
    Field3 = _worldPacket.ReadBit();
}

void WorldPackets::Commentator::CommentatorSpectate::Read()
{
    TargetName = _worldPacket.ReadString(_worldPacket.ReadBits(6));
}

WorldPacket const* WorldPackets::Commentator::CommentatorMapInfo::Write()
{
    _worldPacket << uint64(DirectoryId);
    _worldPacket << uint32(Maps.size());
    for (MapInfo const& map : Maps)
    {
        _worldPacket << uint32(map.TeamSize);
        _worldPacket << uint32(map.MinLevel);
        _worldPacket << uint32(map.MaxLevel);
        _worldPacket << uint16(map.Field3);
        _worldPacket << uint32(map.Instances.size());
        for (InstanceInfo const& instance : map.Instances)
        {
            _worldPacket << uint32(instance.MapID);
            _worldPacket << uint32(instance.Field1);
            _worldPacket << uint32(instance.Field2);
            _worldPacket << uint8(instance.Field3);
            _worldPacket << uint64(instance.InstanceID);
            _worldPacket << uint32(instance.Status);
            for (TeamInfo const& team : instance.Teams)
            {
                _worldPacket << team.TeamGUID;
                _worldPacket << uint32(team.Players.size());
                for (PlayerInfo const& player : team.Players)
                {
                    _worldPacket << player.PlayerGUID;
                    _worldPacket << uint32(player.Field1);
                    _worldPacket << uint32(player.Field2);
                    _worldPacket << uint8(player.Field3);
                }
            }
        }
    }

    return &_worldPacket;
}

WorldPacket const* WorldPackets::Commentator::CommentatorStateChanged::Write()
{
    _worldPacket << MatchGUID;
    _worldPacket.WriteBit(Enabled);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

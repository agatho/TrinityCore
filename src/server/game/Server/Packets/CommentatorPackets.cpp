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

void WorldPackets::Commentator::CommentatorGetPlayerInfo::Read()
{
    _worldPacket >> Field0;
    _worldPacket >> Field1;
    _worldPacket >> Field2;
    Field3 = _worldPacket.ReadBit();
}

void WorldPackets::Commentator::CommentatorGetPlayerCooldowns::Read()
{
    _worldPacket >> Player;
    uint32 count;
    _worldPacket >> count;
    TrackedSpells.resize(count);
    for (TrackedSpell& spell : TrackedSpells)
    {
        _worldPacket >> spell.SpellID;
        _worldPacket >> spell.Category;
    }
}

void WorldPackets::Commentator::CommentatorStartWargame::Read()
{
    // Bit block first: two 6-bit captain-name lengths + the tournament-rules flag, then a byte-aligned u64
    // packing { ListID (low 32) | TeamSize (high 32) }, then the two captain names as raw bytes.
    uint32 const lenOne = _worldPacket.ReadBits(6);
    uint32 const lenTwo = _worldPacket.ReadBits(6);
    TournamentRules = _worldPacket.ReadBit();

    uint64 packed;
    _worldPacket >> packed;                                 // read<T> resets the bit cursor, so this is byte-aligned
    ListID = uint32(packed & 0xFFFFFFFFu);
    TeamSize = uint32(packed >> 32);

    TeamOneCaptain = _worldPacket.ReadString(lenOne);
    TeamTwoCaptain = _worldPacket.ReadString(lenTwo);
}

WorldPacket const* WorldPackets::Commentator::CommentatorPlayerInfo::Write()
{
    _worldPacket << uint32(LeadingId);
    _worldPacket << uint32(SpellTuple1);
    _worldPacket << uint32(SpellTuple2);
    _worldPacket << uint8(SpellTuple3);
    _worldPacket << uint64(PackedId);
    _worldPacket << uint32(Players.size());
    _worldPacket << uint8(Flag ? 0x80 : 0x00);              // the client takes bit 7 of this byte

    for (PlayerData const& player : Players)
    {
        _worldPacket << player.UnitGUID;
        _worldPacket << uint8(player.Faction);
        _worldPacket << uint32(player.Specialization);
        _worldPacket << uint8(player.Field3);
        _worldPacket << uint8(player.Field4);
        _worldPacket << uint16(player.Kills);
        _worldPacket << uint16(player.Deaths);
        _worldPacket << uint32(player.DamageDone);
        _worldPacket << uint32(player.DamageTaken);
        _worldPacket << uint32(player.HealingDone);
        _worldPacket << uint32(player.HealingTaken);
        _worldPacket << uint8(player.SoloShuffleRoundWins);
        _worldPacket << uint8(player.SoloShuffleRoundLosses);
        // Four array counts are written up front in order A,B,C,D; the client then reads the bodies in the
        // order B,C,D,A (confirmed from the deserializer sub_7FF72906EFA0).
        _worldPacket << uint32(player.Cooldowns.size());        // count A - cooldowns
        _worldPacket << uint32(player.Charges.size());          // count B - charges
        _worldPacket << uint32(player.Auras.size());            // count C - auras
        _worldPacket << uint32(player.TrackedSpellIds.size());  // count D - tracked spell ids

        // Bodies in the client's read order: B (charges), C (auras), D (ids), then A (cooldowns) last.
        for (Spells::SpellChargeEntry const& charge : player.Charges)
            _worldPacket << charge;
        for (PlayerData::AuraState const& aura : player.Auras)
        {
            _worldPacket << uint32(aura.SpellID);
            _worldPacket << uint32(aura.Duration);
        }
        for (uint32 trackedSpellId : player.TrackedSpellIds)
            _worldPacket << uint32(trackedSpellId);
        for (Spells::SpellHistoryEntry const& cooldown : player.Cooldowns)
            _worldPacket << cooldown;
    }

    return &_worldPacket;
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

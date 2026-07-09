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

#include "ReferAFriendPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::RaF
{
WorldPacket const* RecruitAFriendFailure::Write()
{
    _worldPacket << int32(Reason);
    // Client uses this string only if Reason == ERR_REFER_A_FRIEND_NOT_IN_GROUP || Reason == ERR_REFER_A_FRIEND_SUMMON_OFFLINE_S
    // but always reads it from packet
    _worldPacket << SizedString::BitsSize<6>(Str);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(Str);

    return &_worldPacket;
}

void GetRafAccountInfo::Read()
{
    _worldPacket >> Field;
}

void RafGenerateRecruitmentLink::Read()
{
    _worldPacket >> Field;
}

void RafUpdateRecruitmentInfo::Read()
{
    _worldPacket >> Field;
}

void RafClaimNextReward::Read()
{
    _worldPacket >> FieldA;
    _worldPacket >> FieldB;
}

void RemoveRafRecruit::Read()
{
    _worldPacket >> RecruitId;
}

// Serializes one recruit (client reader sub_7FF729139460) with an empty character roster.
static void WriteRecruit(ByteBuffer& data, RafRecruit const& recruit)
{
    for (uint32 field : recruit.Fields)
        data << uint32(field);

    uint8 nameLen = uint8(std::min<std::size_t>(recruit.Name.length(), 0xFF));
    data << uint8(nameLen);   // NameLen (plain uint8 length)
    data << uint8(0);         // presence bits + high bits of the 7-bit char-roster count (all 0 -> empty, no optional)
    data << uint8(0);         // low bit of the char-roster count + trailing optional-presence (0)
    // empty character roster -> no entries
    data.append(recruit.Name.data(), nameLen);   // Name bytes (no terminator on the wire)
}

void RafClaimActivityReward::Read()
{
    _worldPacket >> FieldA;
    _worldPacket >> ActivityID;
}

WorldPacket const* ClaimRafRewardResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(0);   // RecruitCount (no per-claim recruit deltas)
    _worldPacket << uint8(0);    // packed 3-bit enum @ bits 5-7

    // nested block sub_7FF729195730 (8 x uint32 + 1 packed byte), zero form
    for (int i = 0; i < 8; ++i)
        _worldPacket << uint32(0);
    _worldPacket << uint8(0);

    // nested block sub_7FF729139200 (uint32, uint64, uint64, uint32 count1, uint32, uint32, uint32,
    // uint32 count2, uint32, uint64, count1 x uint32, count2 x uint32, uint8 presence), zero form
    _worldPacket << uint32(0);
    _worldPacket << uint64(0);
    _worldPacket << uint64(0);
    _worldPacket << uint32(0);   // count1
    _worldPacket << uint32(0);
    _worldPacket << uint32(0);
    _worldPacket << uint32(0);
    _worldPacket << uint32(0);   // count2
    _worldPacket << uint32(0);
    _worldPacket << uint64(0);
    _worldPacket << uint8(0);    // presence

    // no recruit descriptors
    return &_worldPacket;
}

WorldPacket const* RafAccountInfo::Write()
{
    // Byte-exact top-level layout (client body sub_7FF7290B46F0). The activity/reward vectors and optional blocks
    // stay empty; the recruit vector is populated from the recruitment backend.
    _worldPacket << uint32(Field20);
    _worldPacket << uint32(0);                   // Count1 (activity vec @+0x28)
    _worldPacket << uint32(0);                   // Count2 (vec @+0x40)
    _worldPacket << uint32(Recruits.size());     // Count3 (recruit descriptors @+0x58)
    _worldPacket << uint32(0);                   // Count4 (vec @+0x70)
    // vec1 loop empty
    _worldPacket << uint8(FieldBit24 ? 0x80 : 0x00);   // presence byte: bit7=FieldBit24, bit6/bit5 (optional blocks) = 0
    // vec2 loop empty
    for (RafRecruit const& recruit : Recruits)
        WriteRecruit(_worldPacket, recruit);
    // vec4 loop empty; no optional blocks
    return &_worldPacket;
}
}

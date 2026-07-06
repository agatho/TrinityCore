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

WorldPacket const* RafAccountInfo::Write()
{
    // Byte-exact top-level layout (client body sub_7FF7290B46F0). Recruit/reward vectors are emitted empty -
    // an account with no recruits is exactly this shape; population comes from the recruitment backend later.
    _worldPacket << uint32(Field20);
    _worldPacket << uint32(0);   // Count1 (vec @+0x28)
    _worldPacket << uint32(0);   // Count2 (vec @+0x40)
    _worldPacket << uint32(0);   // Count3 (recruit descriptors @+0x58)
    _worldPacket << uint32(0);   // Count4 (vec @+0x70)
    // vec1 loop empty
    _worldPacket << uint8(FieldBit24 ? 0x80 : 0x00);   // presence byte: bit7=FieldBit24, bit6/bit5 (optional blocks) = 0
    // vec2/vec3/vec4 loops empty; no optional blocks
    return &_worldPacket;
}
}

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

#include "ClubFinderPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::ClubFinder
{
void ClubFinderPost::Read()
{
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::BitsSize<7>(Name);
    _worldPacket >> SizedString::BitsSize<12>(Description);
    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);

    // The first byte-aligned read below flushes the remaining bits of the block for us
    // (ByteBuffer::read<T> calls ResetBitPos), matching the client's explicit FlushBits.
    _worldPacket >> ClubId;
    _worldPacket >> RecruitingSpecs;
    _worldPacket >> RecruitmentFlags;
    _worldPacket >> ItemLevelRequirement;
    _worldPacket >> AvatarId;
    _worldPacket >> SizedString::Data(Name);
    _worldPacket >> SizedString::Data(Description);
}

WorldPacket const* ClubFinderResponsePostRecruitmentMessage::Write()
{
    _worldPacket << ClubFinderGUID;
    _worldPacket << Bits<3>(Type);
    _worldPacket << Bits<3>(Status);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* ClubFinderErrorMessage::Write()
{
    _worldPacket << Bits<3>(Type);
    _worldPacket << Bits<4>(Error);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
}

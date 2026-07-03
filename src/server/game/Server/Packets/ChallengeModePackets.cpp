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
}

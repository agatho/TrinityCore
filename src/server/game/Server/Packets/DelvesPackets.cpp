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

#include "DelvesPackets.h"

namespace WorldPackets
{
namespace Delves
{

void RequestPartyEligibilityForDelveTiers::Read()
{
    _worldPacket >> MapID;
}

void SelectDelveEntranceTier::Read()
{
    _worldPacket >> MapID;
    _worldPacket >> Tier;
}

WorldPacket const* ShowDelvesDisplayUI::Write()
{
    return &_worldPacket;
}

WorldPacket const* DelvesAccountDataElementChanged::Write()
{
    _worldPacket << uint32(DataElementID);
    _worldPacket << uint32(Value);
    return &_worldPacket;
}

WorldPacket const* ShowDelvesCompanionConfigurationUI::Write()
{
    _worldPacket << uint32(CreatureOrSpellID);
    return &_worldPacket;
}

WorldPacket const* PartyEligibilityForDelveTiersResponse::Write()
{
    _worldPacket.WriteBits(PlayerName.size(), 6);
    _worldPacket.FlushBits();
    _worldPacket.WriteString(PlayerName);
    _worldPacket << uint8(MaxEligibleTier);
    return &_worldPacket;
}

} // namespace Delves
} // namespace WorldPackets

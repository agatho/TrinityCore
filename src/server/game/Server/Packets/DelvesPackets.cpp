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

// DelvesAccountDataElementChanged intentionally has no class — PDE state is
// delivered to the client via ActivePlayer UpdateFields, not a dedicated SMSG.
// See DelvesPackets.h for the IDA-traced reasoning.

WorldPacket const* ShowDelvesCompanionConfigurationUI::Write()
{
    _worldPacket << uint32(CreatureOrSpellID);
    return &_worldPacket;
}

WorldPacket const* PartyEligibilityForDelveTiersResponse::Write()
{
    // Sniff-confirmed for empty case. Per-entry layout is INFERRED — see header.
    _worldPacket << uint32(Members.size());
    for (EligibleMember const& member : Members)
    {
        _worldPacket.WriteBits(member.PlayerName.size(), 6);
        _worldPacket.FlushBits();
        _worldPacket.WriteString(member.PlayerName);
        _worldPacket << uint8(member.MaxEligibleTier);
    }
    return &_worldPacket;
}

} // namespace Delves
} // namespace WorldPackets

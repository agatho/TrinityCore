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
    // 68275 wire: PackedGUID entranceGuid + uint32 tier (sender 0x7FF729155A10).
    _worldPacket >> EntranceGUID;
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
    // 68275: empty body — the client read ctor (0x7FF7290BB940) takes no fields.
    return &_worldPacket;
}

WorldPacket const* PartyEligibilityForDelveTiersResponse::Write()
{
    // 68275 wire (read ctor 0x7FF7290BBA40): PackedGUID + uint32 + uint32 + bool(MSB).
    // One member per packet — no count framing. Field semantics UNVERIFIED — see header.
    _worldPacket << PlayerGUID;
    _worldPacket << uint32(MaxEligibleTier);
    _worldPacket << uint32(ReasonOrFlags);
    _worldPacket.WriteBit(IsEligible);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

} // namespace Delves
} // namespace WorldPackets

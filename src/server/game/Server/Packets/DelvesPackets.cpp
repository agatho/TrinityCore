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

void TieredEntranceOpen::Read()
{
    // 68275 wire: PackedGuid only (12B observed; sender 0x7FF7291559C0).
    _worldPacket >> EntranceGUID;
}

WorldPacket const* TieredEntranceOpenResponse::Write()
{
    // Byte-exact reproduction of the 579B sniff body (rated BG 12.0.7.pkt).
    // See C:\dumps\TIERED_ENTRANCE_RE_68275.md for the field table + evidence.
    _worldPacket << EntranceGUID;
    _worldPacket << uint32(EntranceType);
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(Unknown3);
    _worldPacket << uint32(Unknown4);
    _worldPacket << uint32(Tiers.size());
    _worldPacket << uint32(Unknown6);
    _worldPacket << uint32(Unknown7);
    _worldPacket << uint32(Unknown8);

    // 12-bit description length, flushed with 4 pad bits (sniff: len 17 → `01 10`).
    _worldPacket.WriteBits(EntranceDescription.length(), 12);
    _worldPacket.FlushBits();

    for (TieredEntranceTier const& tier : Tiers)
    {
        _worldPacket << uint32(tier.TieredEntranceTierID);
        _worldPacket << uint32(tier.Tier);
        _worldPacket << uint32(tier.SuggestedILvl);
        _worldPacket << uint32(tier.UnlockPlayerConditionID);
        _worldPacket << uint32(tier.DynamicUnlockPlayerConditionID);
        _worldPacket << uint32(tier.ModifierUIWidgetSetID);

        // unlocked bit + 12-bit tierDescription length, flushed with 3 pad bits
        // (sniff: unlocked=1,len=6 → `80 30`; unlocked=0,len=21 → `00 a8`).
        _worldPacket.WriteBit(tier.Unlocked);
        _worldPacket.WriteBits(tier.TierDescription.length(), 12);
        _worldPacket.FlushBits();

        _worldPacket << uint32(tier.PreviewTreasureList.size());
        for (TieredEntranceReward const& reward : tier.PreviewTreasureList)
        {
            _worldPacket << uint8(reward.RewardType);
            _worldPacket << uint32(reward.Id);
            _worldPacket << uint32(reward.Quantity);
            _worldPacket << uint8(reward.Context);
        }

        // Description chars at record end, no NUL terminator.
        _worldPacket.append(tier.TierDescription.data(), tier.TierDescription.length());
    }

    // Entrance description chars form the packet tail, no NUL terminator.
    _worldPacket.append(EntranceDescription.data(), EntranceDescription.length());

    return &_worldPacket;
}

} // namespace Delves
} // namespace WorldPackets

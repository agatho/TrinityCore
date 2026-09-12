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
    // 12.1.0.69497 wire (midnightintro 4704067 / 4707529): one uint32.
    _worldPacket << uint32(Unknown);

    return &_worldPacket;
}

// DelvesAccountDataElementChanged intentionally has no class — PDE state is
// delivered to the client via ActivePlayer UpdateFields, not a dedicated SMSG.
// See DelvesPackets.h for the IDA-traced reasoning.

WorldPacket const* ShowDelvesCompanionConfigurationUI::Write()
{
    // 12.1.0.69497 wire (gulf 1096575, eversong 2688539; 12.0.1 deatholme 75171): one uint32.
    _worldPacket << uint32(Unknown);

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
    // 12.1.0.69497 layout, byte-exact on every 12.1 frame (Gulf of Memory 672 B x2, Shadow Enclave
    // 669 B, Naigtal 200 B; tools/delve_wire_check.py in the sniff rig). Differences to the 12.0.7
    // layout this was first reconstructed from: the tier record has a 4th uint32
    // (OverrideTooltipSpellID), the reward list precedes the Unlocked/length bits, a uint16 sits
    // between them, and the entrance description length is written at the packet tail.
    _worldPacket << EntranceGUID;
    _worldPacket << uint32(EntranceType);
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(Unknown3);
    _worldPacket << uint32(Unknown4);
    _worldPacket << uint32(Tiers.size());
    _worldPacket << uint32(Unknown6);
    _worldPacket << uint32(Unknown7);
    _worldPacket << uint32(Unknown8);

    for (TieredEntranceTier const& tier : Tiers)
    {
        _worldPacket << uint32(tier.TieredEntranceTierID);
        _worldPacket << uint32(tier.Tier);
        _worldPacket << uint32(tier.SuggestedILvl);
        _worldPacket << uint32(tier.OverrideTooltipSpellID);
        _worldPacket << uint32(tier.UnlockPlayerConditionID);
        _worldPacket << uint32(tier.DynamicUnlockPlayerConditionID);
        _worldPacket << uint32(tier.ModifierUIWidgetSetID);

        _worldPacket << uint32(tier.PreviewTreasureList.size());
        for (TieredEntranceReward const& reward : tier.PreviewTreasureList)
        {
            _worldPacket << uint8(reward.RewardType);
            _worldPacket << uint32(reward.Id);
            _worldPacket << uint32(reward.Quantity);
            _worldPacket << uint8(reward.Context);
        }

        _worldPacket << uint16(tier.Unknown);

        _worldPacket.WriteBit(tier.Unlocked);
        _worldPacket.WriteBits(tier.TierDescription.length(), 12);
        _worldPacket.FlushBits();

        _worldPacket.append(tier.TierDescription.data(), tier.TierDescription.length());
    }

    _worldPacket.WriteBits(EntranceDescription.length(), 12);
    _worldPacket.FlushBits();
    _worldPacket.append(EntranceDescription.data(), EntranceDescription.length());

    return &_worldPacket;
}

} // namespace Delves
} // namespace WorldPackets

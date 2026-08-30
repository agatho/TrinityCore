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

#include "ArtifactPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Artifact
{
ByteBuffer& operator>>(ByteBuffer& data, ArtifactPowerChoice& artifactPowerChoice)
{
    data >> artifactPowerChoice.ArtifactPowerID;
    data >> artifactPowerChoice.Rank;
    return data;
}

void ArtifactAddPower::Read()
{
    _worldPacket >> ArtifactGUID;
    _worldPacket >> ForgeGUID;
    _worldPacket >> Size<uint32>(PowerChoices);
    for (ArtifactPowerChoice& artifactPowerChoice : PowerChoices)
        _worldPacket >> artifactPowerChoice;
}

void ArtifactSetAppearance::Read()
{
    _worldPacket >> ArtifactGUID;
    _worldPacket >> ForgeGUID;
    _worldPacket >> ArtifactAppearanceID;
}

void ConfirmArtifactRespec::Read()
{
    _worldPacket >> ArtifactGUID;
    _worldPacket >> NpcGUID;
}

WorldPacket const* OpenArtifactForge::Write()
{
    _worldPacket << ArtifactGUID;
    _worldPacket << ForgeGUID;

    return &_worldPacket;
}

WorldPacket const* ArtifactRespecPrompt::Write()
{
    _worldPacket << ArtifactGUID;
    _worldPacket << NpcGUID;

    return &_worldPacket;
}

WorldPacket const* CloseArtifactForge::Write()
{
    return &_worldPacket;
}

WorldPacket const* ArtifactForgeError::Write()
{
    return &_worldPacket;
}

// Length 2..18 + 1 + 4 = 7..23 bytes.
// UNVERIFIED: no capture exists (0 packets in 72 sniffs). The PackedGuid leg is calibrated against
// SMSG_XP_GAIN_ABORTED (0x45006D, 284 captured packets, 14..29 bytes for pguid + 3 x uint32).
WorldPacket const* ArtifactEndgamePowersRefunded::Write()
{
    _worldPacket << ArtifactGUID;
    _worldPacket << uint8(RefundedTier);
    _worldPacket << uint32(NumRefundedPowers);

    return &_worldPacket;
}

WorldPacket const* ArtifactXpGain::Write()
{
    _worldPacket << ArtifactGUID;
    _worldPacket << uint64(Amount);

    return &_worldPacket;
}
}

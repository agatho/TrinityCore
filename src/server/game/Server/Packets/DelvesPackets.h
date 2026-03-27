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

#ifndef DelvesPackets_h__
#define DelvesPackets_h__

#include "Packet.h"

namespace WorldPackets
{
namespace Delves
{

// CMSG_DELVE_TELEPORT_OUT (0x3B012C)
class DelveTeleportOut final : public ClientPacket
{
public:
    explicit DelveTeleportOut(WorldPacket&& packet) : ClientPacket(CMSG_DELVE_TELEPORT_OUT, std::move(packet)) { }

    void Read() override { }
};

// CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS (0x3A02F3)
class RequestPartyEligibilityForDelveTiers final : public ClientPacket
{
public:
    explicit RequestPartyEligibilityForDelveTiers(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS, std::move(packet)) { }

    void Read() override;

    uint32 GossipOptionOrMapChallengeID = 0;
};

// SMSG_SHOW_DELVES_DISPLAY_UI (0x420356)
class ShowDelvesDisplayUI final : public ServerPacket
{
public:
    explicit ShowDelvesDisplayUI() : ServerPacket(SMSG_SHOW_DELVES_DISPLAY_UI, 0) { }

    WorldPacket const* Write() override;
};

// SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED (0x420357)
class DelvesAccountDataElementChanged final : public ServerPacket
{
public:
    explicit DelvesAccountDataElementChanged() : ServerPacket(SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED, 0) { }

    WorldPacket const* Write() override;
};

// SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI (0x420358)
class ShowDelvesCompanionConfigurationUI final : public ServerPacket
{
public:
    explicit ShowDelvesCompanionConfigurationUI() : ServerPacket(SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI, 4) { }

    WorldPacket const* Write() override;

    uint32 CreatureID = 0;
};

// SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE (0x42035A)
class PartyEligibilityForDelveTiersResponse final : public ServerPacket
{
public:
    explicit PartyEligibilityForDelveTiersResponse() : ServerPacket(SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE, 0) { }

    WorldPacket const* Write() override;
};

} // namespace Delves
} // namespace WorldPackets

#endif // DelvesPackets_h__

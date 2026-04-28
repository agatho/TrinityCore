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

// CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS (build 67186 = 0x3A02F4)
// Lua signature: C_DelvesUI.RequestPartyEligibilityForDelveTiers(mapID)
// Sniff (build 66562) confirms 4-byte payload (uint32 MapID only).
class RequestPartyEligibilityForDelveTiers final : public ClientPacket
{
public:
    explicit RequestPartyEligibilityForDelveTiers(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS, std::move(packet)) { }

    void Read() override;

    uint32 MapID = 0;
};

// CMSG_SELECT_DELVE_ENTRANCE_TIER (0x3B0134)
// Lua: C_DelvesUI.SelectDelveEntranceTier(tier). Client wraps with active PDE MapID.
class SelectDelveEntranceTier final : public ClientPacket
{
public:
    explicit SelectDelveEntranceTier(WorldPacket&& packet) : ClientPacket(CMSG_SELECT_DELVE_ENTRANCE_TIER, std::move(packet)) { }

    void Read() override;

    uint32 MapID = 0;
    uint8 Tier = 0;
};

// SMSG_SHOW_DELVES_DISPLAY_UI (build 67186 = 0x420359)
class ShowDelvesDisplayUI final : public ServerPacket
{
public:
    explicit ShowDelvesDisplayUI() : ServerPacket(SMSG_SHOW_DELVES_DISPLAY_UI, 0) { }

    WorldPacket const* Write() override;
};

// SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED (build 67186 = 0x42035A)
// Wire: uint32 DataElementID, uint32 Value (per IDA-decoded JamSMsgDelvesAccountDataElementChanged).
class DelvesAccountDataElementChanged final : public ServerPacket
{
public:
    explicit DelvesAccountDataElementChanged() : ServerPacket(SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED, 8) { }

    WorldPacket const* Write() override;

    uint32 DataElementID = 0;
    uint32 Value = 0;
};

// SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI (build 67186 = 0x42035B)
// Sniff confirms 4-byte payload — value matches a creature/spell ID.
// Lua doc: "Signaled when SpellScript indicates that a curio has been learned or upgraded."
class ShowDelvesCompanionConfigurationUI final : public ServerPacket
{
public:
    explicit ShowDelvesCompanionConfigurationUI() : ServerPacket(SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI, 4) { }

    WorldPacket const* Write() override;

    uint32 CreatureOrSpellID = 0;
};

// SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE (build 67186 = 0x42035D)
// Lua event payload: (playerName: string, maxEligibleLevel: number) — one event firing per packet.
// Wire: TC strings use bit-length prefix, then bytes. Sent once per evaluated party member.
class PartyEligibilityForDelveTiersResponse final : public ServerPacket
{
public:
    explicit PartyEligibilityForDelveTiersResponse() : ServerPacket(SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE, 64) { }

    WorldPacket const* Write() override;

    std::string PlayerName;
    uint8 MaxEligibleTier = 0;
};

} // namespace Delves
} // namespace WorldPackets

#endif // DelvesPackets_h__

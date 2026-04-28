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

// CMSG_SELECT_DELVE_ENTRANCE_TIER (build 67186 = 0x3B0134)
// Lua signature: C_DelvesUI.SelectDelveEntranceTier(tier) — single Lua arg.
//
// The client wrapper at IDA `0x7FF75C96F170` (Delve_Entrance_SelectDelveEntranceTier)
// looks the tier up against a 40-entry table at g_Data_4198798, then dispatches
// via ai_Handle_LuaNetworkMessage with a v-table-typed packet whose payload is
// a uint32 plus a 16-byte struct copied from the matched entry's owner+0x10.
// The exact serialized layout has NOT been observed in a sniff. Our (MapID,
// Tier) read below is a best-effort placeholder — server-side validation in
// HandleSelectDelveEntranceTier still gates on tier eligibility, so a wrong
// MapID just falls through harmlessly. Replace with the verified shape once
// alliance_deatholme_delve sniffs (or any later sniff) capture this opcode.
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
//
// Intentionally NO packet class. PlayerDataElement (PDE) state on the client is
// stored on CGActivePlayer_C in two `(vector<PlayerDataElement>, vector<uint32>)`
// fields (decompiled from `sub_7FF75C204150`, the CGActivePlayer destructor;
// fields at qword offsets 651 and 658 — Account and Character respectively). The
// Lua event `DELVES_ACCOUNT_DATA_ELEMENT_CHANGED` is broadcast by mirror handlers
// registered against those vector fields with the signature
//   void(CGActivePlayer_C&, PlayerDataElement const& oldElem,
//        PlayerDataElement const& newElem, unsigned int idx)
// (typename string at `0x7FF75F3A9AE0` in IDA build 66198). Mirror handlers fire
// from UpdateField changes — the data is delivered inside SMSG_UPDATE_OBJECT, not
// via a dedicated SMSG. Server-side, the corresponding work is to populate the
// ActivePlayer UpdateField that holds the PDE list, which then drives the client
// event automatically. The opcode value (0x42035A) is retained in Opcodes.h as a
// known marker only.

// SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI (build 67186 = 0x42035B)
// Sniff (66709, op 0x420358) shows 4-byte payload, value 0x0003EEDB. Single uint32.
// Field semantics not symbolicated in IDA 66198 — likely a creature/companion entry.
class ShowDelvesCompanionConfigurationUI final : public ServerPacket
{
public:
    explicit ShowDelvesCompanionConfigurationUI() : ServerPacket(SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI, 4) { }

    WorldPacket const* Write() override;

    uint32 CreatureOrSpellID = 0;
};

// SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE (build 67186 = 0x42035D)
// Sniff (66709, op 0x42035D) shows a single 4-byte payload `00 00 00 00` for the
// no-eligible-member case. The Lua event PARTY_ELIGIBILITY_FOR_DELVE_TIERS_CHANGED
// fires with (playerName: string, maxEligibleLevel: number) per entry, suggesting
// the wire format is `uint32 Count` followed by `Count` entries. The element layout
// (bit-prefixed name + uint8 tier) is INFERRED from TrinityCore conventions and is
// NOT confirmed by IDA (build-66198 db has no SMSG decoder symbols) or by a
// populated-list sniff. The current Write() emits only `uint32 Count` to remain
// byte-exact with the observed empty case.
class PartyEligibilityForDelveTiersResponse final : public ServerPacket
{
public:
    explicit PartyEligibilityForDelveTiersResponse() : ServerPacket(SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE, 4) { }

    WorldPacket const* Write() override;

    struct EligibleMember
    {
        std::string PlayerName;
        uint8 MaxEligibleTier = 0;
    };

    std::vector<EligibleMember> Members;
};

} // namespace Delves
} // namespace WorldPackets

#endif // DelvesPackets_h__

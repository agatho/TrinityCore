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
#include "ObjectGuid.h"

namespace WorldPackets
{
namespace Delves
{

// CMSG_DELVE_TELEPORT_OUT (0x3B012E @ 12.0.7.68275)
// 68275 binary (sender 0x7FF7291558F0): empty body, opcode only.
class DelveTeleportOut final : public ClientPacket
{
public:
    explicit DelveTeleportOut(WorldPacket&& packet) : ClientPacket(CMSG_DELVE_TELEPORT_OUT, std::move(packet)) { }

    void Read() override { }
};

// CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS (0x3A02F0 @ 12.0.7.68275; was 0x3A02F4 at 67186)
// Lua signature: C_DelvesUI.RequestPartyEligibilityForDelveTiers(mapID)
// 68275 binary (sender 0x7FF72914CAB0) confirms 4-byte payload (uint32 MapID only);
// matches the build-66562 sniff.
class RequestPartyEligibilityForDelveTiers final : public ClientPacket
{
public:
    explicit RequestPartyEligibilityForDelveTiers(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_PARTY_ELIGIBILITY_FOR_DELVE_TIERS, std::move(packet)) { }

    void Read() override;

    uint32 MapID = 0;
};

// CMSG_SELECT_DELVE_ENTRANCE_TIER (0x3B0134 @ 12.0.7.68275)
// Lua signature: C_DelvesUI.SelectDelveEntranceTier(tier) — single Lua arg.
//
// Wire resolved from the 68275 binary (sender 0x7FF729155A10, opcode immediate
// at 0x7FF729155A23): PackedGUID entranceGuid + uint32 tier. The "16-byte struct
// copied from the 40-entry table" of the earlier 67186 read is that ObjectGuid.
// Tier is uint32, not uint8. The GUID is believed to be the delve entrance/POI
// object the picker is bound to — // UNVERIFIED — needs sniff (the field widths
// and order ARE certain; only the GUID's referent is inferred). The server
// re-derives the delve MapID from this GUID in HandleSelectDelveEntranceTier.
class SelectDelveEntranceTier final : public ClientPacket
{
public:
    explicit SelectDelveEntranceTier(WorldPacket&& packet) : ClientPacket(CMSG_SELECT_DELVE_ENTRANCE_TIER, std::move(packet)) { }

    void Read() override;

    ObjectGuid EntranceGUID;
    uint32 Tier = 0;
};

// SMSG_SHOW_DELVES_DISPLAY_UI (0x420359 @ 12.0.7.68275)
// 68275 binary (read ctor 0x7FF7290BB840): empty body (remaining-span, 0-length in practice).
class ShowDelvesDisplayUI final : public ServerPacket
{
public:
    explicit ShowDelvesDisplayUI() : ServerPacket(SMSG_SHOW_DELVES_DISPLAY_UI, 0) { }

    WorldPacket const* Write() override;
};

// SMSG_DELVES_ACCOUNT_DATA_ELEMENT_CHANGED (0x42035A @ 12.0.7.68275)
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
// from UpdateField changes — server-side we populate the ActivePlayer UpdateFields
// (AccountDataElements / CharacterDataElements / DelveData) inside SMSG_UPDATE_OBJECT,
// which drives the client event automatically.
// 68275 note: the dedicated SMSG's read ctor (0x7FF7290BB8C0) captures the entire
// remaining payload as an opaque account-data element blob that is fed to the same
// CGActivePlayer mirror deserializer (0x7FF72920BCF0) — i.e. it is an alternative
// runtime delta channel for the same mirror stream. The exact blob framing is
// // UNVERIFIED — needs sniff; we deliver via SMSG_UPDATE_OBJECT instead.

// SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI (0x42035B @ 12.0.7.68275)
// 68275 binary (read ctor 0x7FF7290BB940): the client reads an EMPTY body (a
// remaining-bytes span expected to be 0-length). The earlier 66709 sniff's 4-byte
// payload is ignored by the 68275 reader (trailing bytes are benign), so the
// packet carries no fields. UI-trigger only.
class ShowDelvesCompanionConfigurationUI final : public ServerPacket
{
public:
    explicit ShowDelvesCompanionConfigurationUI() : ServerPacket(SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI, 0) { }

    WorldPacket const* Write() override;
};

// SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE (0x42035D @ 12.0.7.68275)
// 68275 binary (read ctor 0x7FF7290BBA40) reads exactly, in order:
//   PackedGUID + uint32 + uint32 + uint8 (bool = byte>>7)
// There is NO count/array framing — the packet carries a single member entry, so
// the server sends one packet per party member. The Lua event
// PARTY_ELIGIBILITY_FOR_DELVE_TIERS_CHANGED carries (playerName, maxEligibleLevel);
// the name is resolved client-side from the GUID.
// Semantics of the two uint32s and the bool are // UNVERIFIED — needs sniff.
// Best-hypothesis mapping used here: first uint32 = max eligible tier (matches the
// Lua event's maxEligibleLevel), second uint32 = ineligibility reason/flags (0 when
// eligible), bool = is-eligible. Wire widths/order ARE certain.
class PartyEligibilityForDelveTiersResponse final : public ServerPacket
{
public:
    explicit PartyEligibilityForDelveTiersResponse() : ServerPacket(SMSG_PARTY_ELIGIBILITY_FOR_DELVE_TIERS_RESPONSE, 16 + 2 + 4 + 4 + 1) { }

    WorldPacket const* Write() override;

    ObjectGuid PlayerGUID;
    uint32 MaxEligibleTier = 0;   // UNVERIFIED — needs sniff (client field +0x30)
    uint32 ReasonOrFlags = 0;     // UNVERIFIED — needs sniff (client field +0x34)
    bool IsEligible = false;      // UNVERIFIED — needs sniff (client field +0x38, wire byte MSB)
};

} // namespace Delves
} // namespace WorldPackets

#endif // DelvesPackets_h__

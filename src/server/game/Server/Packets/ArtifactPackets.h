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

#ifndef TRINITYCORE_ARTIFACT_PACKETS_H
#define TRINITYCORE_ARTIFACT_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"

namespace WorldPackets
{
    namespace Artifact
    {
        struct ArtifactPowerChoice
        {
            int32 ArtifactPowerID = 0;
            uint8 Rank = 0;
        };

        class ArtifactAddPower final : public ClientPacket
        {
        public:
            explicit ArtifactAddPower(WorldPacket&& packet) : ClientPacket(CMSG_ARTIFACT_ADD_POWER, std::move(packet)) { }

            void Read() override;

            ObjectGuid ArtifactGUID;
            ObjectGuid ForgeGUID;
            Array<ArtifactPowerChoice, 1 /*lua allows only 1 power per call*/> PowerChoices;
        };

        class ArtifactSetAppearance final : public ClientPacket
        {
        public:
            explicit ArtifactSetAppearance(WorldPacket&& packet) : ClientPacket(CMSG_ARTIFACT_SET_APPEARANCE, std::move(packet)) { }

            void Read() override;

            ObjectGuid ArtifactGUID;
            ObjectGuid ForgeGUID;
            int32 ArtifactAppearanceID = 0;
        };

        class ConfirmArtifactRespec final : public ClientPacket
        {
        public:
            explicit ConfirmArtifactRespec(WorldPacket&& packet) : ClientPacket(CMSG_CONFIRM_ARTIFACT_RESPEC, std::move(packet)) { }

            void Read() override;

            ObjectGuid ArtifactGUID;
            ObjectGuid NpcGUID;
        };

        class OpenArtifactForge final : public ServerPacket
        {
        public:
            explicit OpenArtifactForge() : ServerPacket(SMSG_OPEN_ARTIFACT_FORGE, 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid ArtifactGUID;
            ObjectGuid ForgeGUID;
        };

        class ArtifactRespecPrompt final : public ServerPacket
        {
        public:
            explicit ArtifactRespecPrompt() : ServerPacket(SMSG_ARTIFACT_RESPEC_PROMPT, 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid ArtifactGUID;
            ObjectGuid NpcGUID;
        };

        // SMSG_CLOSE_ARTIFACT_FORGE (0x45024F) and SMSG_ARTIFACT_FORGE_ERROR (0x450250) share one
        // consumer (0x2329870). Both readers (0x5FF780 / 0x5FF800) only take a raw pointer to the
        // remaining span, and the consumer never touches its argument:
        //     mov  byte ptr [rip+0x2971b6d], 0   ; forge state flag byte_7FF785C6B3E8 = 0
        //     call 0x7FF781274DC0                ; PlayerInteractionManager singleton
        //     mov  dword ptr [rsp+0x38], 0x26    ; 38 == PlayerInteractionType::ArtifactForge
        //     mov  byte  ptr [rsp+0x3c], 1       ; only clear if that type is the active one
        //     call 0x7FF78323A4D0                ; ClearInteraction(type, checkType)
        // Both messages therefore carry no payload the client can use, and both are idempotent: the
        // checkType byte makes the client ignore them unless the artifact forge is the open frame.
        class CloseArtifactForge final : public ServerPacket
        {
        public:
            explicit CloseArtifactForge() : ServerPacket(SMSG_CLOSE_ARTIFACT_FORGE, 0) { }

            WorldPacket const* Write() override;
        };

        // See CloseArtifactForge. The client has no reader, no enum and no Lua event for an error code
        // here - a code on the wire would not reach the UI. The only matching string,
        // ARTIFACT_TRAITS_NO_FORGE_ERROR (GlobalStrings 35246), is set client side in
        // Blizzard_ArtifactPowerButton.lua:66.
        // UNVERIFIED: whether retail nevertheless puts bytes on the wire cannot be decided offline
        // (0 captured packets). Sent empty; a later capture can add fields without changing the effect.
        class ArtifactForgeError final : public ServerPacket
        {
        public:
            explicit ArtifactForgeError() : ServerPacket(SMSG_ARTIFACT_FORGE_ERROR, 0) { }

            WorldPacket const* Write() override;
        };

        // SMSG_ARTIFACT_ENDGAME_POWERS_REFUNDED (0x450252), reader 0x5FF910:
        //     ReadPackedGuid, Read<uint8> (no shift - a real byte aligned uint8), Read<uint32>
        // Consumer 0x23299E0 resolves ArtifactGUID with type mask 2 (TYPEMASK_ITEM), derives bag and
        // slot index from that item and fires Lua ARTIFACT_ENDGAME_REFUND (0x21B4401823260C60,
        // ArtifactUIDocumentation.lua:832-844) - the event name does not match the opcode name.
        // NO SENDER. This is not the ordinary paid artifact respec. The effect the message actually
        // produces is the tier 2 upgrade sequence: ArtifactPerksMixin:OnTraitsRefunded stores
        // numArtifactTraitsRefunded and sets perksDirty (Blizzard_ArtifactPerks.lua:893-895), and the
        // next Refresh dispatches to AnimateTraitRefund (:415-417). That function plays
        // Tier2ForgingScene.ForgingEffectAnimIn (:923) and Model.ForgingEffectAnimIn (:928), and for
        // NumRefundedPowers == 0 it does nothing but PrepTierTwoReveal (:931-933); the same Refresh
        // also suppresses the normal tier display while numArtifactTraitsRefunded is set (:392).
        // Together with the opcode name and RefundedTier (an ArtifactTiers value,
        // ArtifactUIDocumentation.lua:840) that places the message at the endgame tier upgrade - the
        // point at which the previously purchased endgame powers are given back so they can be
        // re-spent in the new tier. TrinityCore has no such upgrade: MAX_ARTIFACT_TIER is 1
        // (DBCEnums.h:225), so tier 2 is never reached, and nothing in the tree refunds powers on a
        // tier change. A sender was therefore deliberately not built - hanging it off
        // CMSG_CONFIRM_ARTIFACT_RESPEC (the paid respec) would make every tier 0/1 respec play the
        // tier 2 reveal on the next opening of the artifact frame. See HandleConfirmArtifactRespec.
        // NumRefundedPowers drives the tick count of the PointsRemainingLabel animation
        // (Blizzard_ArtifactPerks.lua:943), so it counts refunded artifact *points*, i.e. purchased
        // ranks - not distinct powers.
        // UNVERIFIED: the exact trigger. 0 captured packets; the tier upgrade reading is derived from
        // the consumer and the opcode name, no source names the sending event.
        // UNVERIFIED: whether RefundedTier carries the tier that was refunded or the tier now reached.
        class ArtifactEndgamePowersRefunded final : public ServerPacket
        {
        public:
            explicit ArtifactEndgamePowersRefunded() : ServerPacket(SMSG_ARTIFACT_ENDGAME_POWERS_REFUNDED, 16 + 1 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid ArtifactGUID;
            uint8 RefundedTier = 0;             ///< ArtifactTier::ArtifactTier of the refunded tier
            uint32 NumRefundedPowers = 0;       ///< purchased ranks given back
        };

        class ArtifactXpGain final : public ServerPacket
        {
        public:
            explicit ArtifactXpGain() : ServerPacket(SMSG_ARTIFACT_XP_GAIN, 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid ArtifactGUID;
            uint64 Amount = 0;
        };
    }
}

#endif // TRINITYCORE_ARTIFACT_PACKETS_H

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

#ifndef TRINITYCORE_TALENT_PACKETS_H
#define TRINITYCORE_TALENT_PACKETS_H

#include "Packet.h"
#include "DBCEnums.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"

namespace WorldPackets
{
    namespace Talent
    {
        struct PvPTalent
        {
            uint16 PvPTalentID = 0;
            uint8 Slot = 0;
        };

        struct TalentGroupInfo
        {
            uint32 SpecID = 0;
            std::vector<uint16> TalentIDs;
            std::vector<PvPTalent> PvPTalents;
            std::vector<uint32> GlyphIDs;
        };

        struct TalentInfoUpdate
        {
            uint8 ActiveGroup = 0;
            uint32 PrimarySpecialization = 0;
            std::vector<TalentGroupInfo> TalentGroups;
        };

        struct ClassicTalentEntry
        {
            int32 TalentID = 0;
            int32 Rank = 0;
        };

        struct ClassicTalentGroupInfo
        {
            uint8 NumTalents = 0;
            std::vector<ClassicTalentEntry> Talents;
            uint8 NumGlyphs = 0;
            std::vector<uint16> GlyphIDs;
            int8 Role = 0;
            int32 PrimarySpecialization = 0;
        };

        struct ClassicTalentInfoUpdate
        {
            int32 UnspentTalentPoints = 0;
            uint8 ActiveGroup = 0;
            bool IsPetTalents = false;
            std::vector<ClassicTalentGroupInfo> Talents;
        };

        class UpdateTalentData final : public ServerPacket
        {
        public:
            explicit UpdateTalentData() : ServerPacket(SMSG_UPDATE_TALENT_DATA, 2+4+4+4+12) { }

            WorldPacket const* Write() override;

            TalentInfoUpdate Info;
        };

        class LearnTalents final : public ClientPacket
        {
        public:
            explicit LearnTalents(WorldPacket&& packet) : ClientPacket(CMSG_LEARN_TALENTS, std::move(packet)) { }

            void Read() override;
            Array<uint16, MAX_TALENT_TIERS> Talents;
        };

        class RespecWipeConfirm final : public ServerPacket
        {
        public:
            explicit RespecWipeConfirm() : ServerPacket(SMSG_RESPEC_WIPE_CONFIRM, 16 + 4 +1) { }

            WorldPacket const* Write() override;

            ObjectGuid RespecMaster;
            uint32 Cost = 0;
            int8 RespecType = 0;
        };

        class ConfirmRespecWipe final : public ClientPacket
        {
        public:
            explicit ConfirmRespecWipe(WorldPacket&& packet) : ClientPacket(CMSG_CONFIRM_RESPEC_WIPE, std::move(packet)) { }

            void Read() override;

            ObjectGuid RespecMaster;
            uint8 RespecType = 0;
        };

        class TalentsInvoluntarilyReset final : public ServerPacket
        {
        public:
            explicit TalentsInvoluntarilyReset(bool isPetTalents) : ServerPacket(SMSG_TALENTS_INVOLUNTARILY_RESET, 1), IsPetTalents(isPetTalents) { }

            WorldPacket const* Write() override;

            bool IsPetTalents = false;
        };

        // SMSG_SPEC_INVOLUNTARILY_CHANGED (0x4501C4)
        // Reader 0x5F72F0 reads one uint8 and takes ">> 7" from it: that is a one-bit section, not a
        // uint8 field. Consumer 0x207F770 fires the Lua event SPEC_INVOLUNTARILY_CHANGED
        // (murmur3_x64_128 low 64 = 0xF9C8D4DC09EFDBC4), documented as { isPet : bool } in
        // Blizzard_APIDocumentationGenerated/SpecializationInfoDocumentation.lua:448.
        class SpecInvoluntarilyChanged final : public ServerPacket
        {
        public:
            explicit SpecInvoluntarilyChanged(bool isPet = false) : ServerPacket(SMSG_SPEC_INVOLUNTARILY_CHANGED, 1), IsPet(isPet) { }

            WorldPacket const* Write() override;

            bool IsPet;
        };

        // SMSG_UPDATE_PRIMARY_SPEC (0x45007B)
        // The client reader (0x5E2330) does not decompose the payload at all - it stores a raw pointer
        // to the remaining span. The field list therefore comes from the consumer (0x23A2290), which is
        // four instructions long:
        //     48 8b 41 20                mov   rax, [rcx+0x20]      ; raw span pointer
        //     0f b7 08                   movzx ecx, word ptr [rax]  ; uint16
        //     89 0d 1f f8 46 04          mov   [rip+0x0446F81F], ecx ; -> dword_7FF7877E1ABC
        // dword_7FF7877E1ABC is the client's primary specialization cache. Its other writer is the
        // consumer of SMSG_UPDATE_TALENT_DATA (0x45007A, consumer 0x23A18D0), which stores
        // *(uint32*)(event+36) there; by that message's sub reader (0x5E2080) offset +36 is field 2 of
        // TalentInfoUpdate, i.e. UpdateTalentData::Info.PrimarySpecialization - and TrinityCore fills
        // that from Player::GetPrimarySpecialization(), a ChrSpecialization::ID.
        // Both writers therefore feed the same cache with the same quantity: SpecID is a
        // ChrSpecialization::ID, not a 1-based specialization index.
        class UpdatePrimarySpec final : public ServerPacket
        {
        public:
            explicit UpdatePrimarySpec(uint16 specId = 0) : ServerPacket(SMSG_UPDATE_PRIMARY_SPEC, 2), SpecID(specId) { }

            WorldPacket const* Write() override;

            uint16 SpecID;
        };

        class LearnTalentFailed final : public ServerPacket
        {
        public:
            explicit LearnTalentFailed() : ServerPacket(SMSG_LEARN_TALENT_FAILED, 1 + 4 + 4 + 2 * MAX_TALENT_TIERS) { }

            WorldPacket const* Write() override;

            uint32 Reason = 0;
            int32 SpellID = 0;
            std::vector<uint16> Talents;
        };

        struct GlyphBinding
        {
            GlyphBinding(uint32 spellId = 0, uint16 glyphId = 0) : SpellID(spellId), GlyphID(glyphId) { }

            uint32 SpellID;
            uint16 GlyphID;
        };

        class ActiveGlyphs final : public ServerPacket
        {
        public:
            explicit ActiveGlyphs() : ServerPacket(SMSG_ACTIVE_GLYPHS) { }

            WorldPacket const* Write() override;

            std::vector<GlyphBinding> Glyphs;
            bool IsFullUpdate = false;
        };

        class LearnPvpTalents final : public ClientPacket
        {
        public:
            explicit LearnPvpTalents(WorldPacket&& packet) : ClientPacket(CMSG_LEARN_PVP_TALENTS, std::move(packet)) { }

            void Read() override;

            Array<PvPTalent, 4> Talents;
        };

        class LearnPvpTalentFailed final : public ServerPacket
        {
        public:
            explicit LearnPvpTalentFailed() : ServerPacket(SMSG_LEARN_PVP_TALENT_FAILED, 1 + 4 + 4 + (2 + 1) * MAX_PVP_TALENT_SLOTS) { }

            WorldPacket const* Write() override;

            uint32 Reason = 0;
            int32 SpellID = 0;
            std::vector<PvPTalent> Talents;
        };

        ByteBuffer& operator<<(ByteBuffer& data, ClassicTalentInfoUpdate const& talentInfoInfo);
    }
}

#endif // TRINITYCORE_TALENT_PACKETS_H

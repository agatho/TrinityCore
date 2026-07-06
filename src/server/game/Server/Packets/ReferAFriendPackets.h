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

#ifndef TRINITYCORE_REFER_A_FRIEND_PACKETS_H
#define TRINITYCORE_REFER_A_FRIEND_PACKETS_H

#include "Packet.h"

namespace WorldPackets
{
    namespace RaF
    {
        class RecruitAFriendFailure final : public ServerPacket
        {
        public:
            RecruitAFriendFailure() : ServerPacket(SMSG_RECRUIT_A_FRIEND_FAILURE, 1 + 4) { }

            WorldPacket const* Write() override;

            std::string Str;
            int32 Reason = 0;
        };

        // CMSG_GET_RAF_ACCOUNT_INFO (0x400150) wire (client serializer sub_7FF72907ECA0): { uint32 Field }.
        // Field is echoed back in SMSG_RAF_ACCOUNT_INFO.Field20 (leading id/version; exact semantics unconfirmed).
        class GetRafAccountInfo final : public ClientPacket
        {
        public:
            explicit GetRafAccountInfo(WorldPacket&& packet) : ClientPacket(CMSG_GET_RAF_ACCOUNT_INFO, std::move(packet)) { }

            void Read() override;

            uint32 Field = 0;
        };

        // CMSG_RAF_GENERATE_RECRUITMENT_LINK (0x400153) wire (sub_7FF72907EDA0): { uint32 Field }.
        class RafGenerateRecruitmentLink final : public ClientPacket
        {
        public:
            explicit RafGenerateRecruitmentLink(WorldPacket&& packet) : ClientPacket(CMSG_RAF_GENERATE_RECRUITMENT_LINK, std::move(packet)) { }

            void Read() override;

            uint32 Field = 0;
        };

        // CMSG_RAF_UPDATE_RECRUITMENT_INFO (0x400152) wire (sub_7FF72907ED50): { uint32 Field }.
        class RafUpdateRecruitmentInfo final : public ClientPacket
        {
        public:
            explicit RafUpdateRecruitmentInfo(WorldPacket&& packet) : ClientPacket(CMSG_RAF_UPDATE_RECRUITMENT_INFO, std::move(packet)) { }

            void Read() override;

            uint32 Field = 0;
        };

        // CMSG_RAF_CLAIM_NEXT_REWARD (0x400151) wire (sub_7FF72907ECF0): { uint32 FieldA, uint32 FieldB }.
        class RafClaimNextReward final : public ClientPacket
        {
        public:
            explicit RafClaimNextReward(WorldPacket&& packet) : ClientPacket(CMSG_RAF_CLAIM_NEXT_REWARD, std::move(packet)) { }

            void Read() override;

            uint32 FieldA = 0;
            uint32 FieldB = 0;
        };

        // CMSG_REMOVE_RAF_RECRUIT (0x400154) wire (sub_7FF72907EDF0): { uint64 RecruitId }.
        class RemoveRafRecruit final : public ClientPacket
        {
        public:
            explicit RemoveRafRecruit(WorldPacket&& packet) : ClientPacket(CMSG_REMOVE_RAF_RECRUIT, std::move(packet)) { }

            void Read() override;

            uint64 RecruitId = 0;
        };

        // SMSG_RAF_ACCOUNT_INFO (0x4202E9) wire (client deserializer body sub_7FF7290B46F0):
        //   uint32 Field20, uint32 Count1, uint32 Count2, uint32 Count3(recruits), uint32 Count4,
        //   Count1 x {6 x uint32}, uint8 presence(bit7=FieldBit24, bit6=optBlockA, bit5=optBlockB),
        //   Count2 x <sub_195730>, Count3 x <recruit descriptor sub_139460>, Count4 x <sub_195510>,
        //   [optBlockA], [optBlockB]. The four vectors + optional blocks carry the recruit roster and reward
        //   state; they are emitted empty here (an account with no recruits is exactly this) and populated in
        //   later phases from the recruitment backend. Full sub-reader layouts: RAF_WIRE_DOSSIER_68275.md.
        class RafAccountInfo final : public ServerPacket
        {
        public:
            RafAccountInfo() : ServerPacket(SMSG_RAF_ACCOUNT_INFO) { }

            WorldPacket const* Write() override;

            uint32 Field20 = 0;
            bool FieldBit24 = false;
        };
    }
}

#endif // TRINITYCORE_REFER_A_FRIEND_PACKETS_H

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
#include <array>
#include <vector>

namespace WorldPackets
{
    namespace RaF
    {
        // One recruit in SMSG_RAF_ACCOUNT_INFO's recruit vector (client reader sub_7FF729139460). On the wire:
        //   13 x uint32 scalars, uint8 NameLen, uint8 0, uint8 0 (presence + 7-bit nested char-roster count),
        //   [char roster entries], NameLen bytes of Name. The per-recruit character roster is server-unknown
        //   offline, so it is emitted empty (count 0) - which zeroes the two flag bytes - and the two extra
        //   bytes carry no roster/optional. Per-scalar semantics are unconfirmed (named Fields[]); Fields[0] is
        //   populated with the recruit's account id.
        struct RafRecruit
        {
            std::array<uint32, 13> Fields = { };
            std::string Name;
        };
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

        // CMSG_RAF_CLAIM_ACTIVITY_REWARD (0x3B009D) wire (client serializer sub_7FF729152B70): { uint64 FieldA, uint32 ActivityID }.
        class RafClaimActivityReward final : public ClientPacket
        {
        public:
            explicit RafClaimActivityReward(WorldPacket&& packet) : ClientPacket(CMSG_RAF_CLAIM_ACTIVITY_REWARD, std::move(packet)) { }

            void Read() override;

            uint64 FieldA = 0;
            uint32 ActivityID = 0;
        };

        // SMSG_CLAIM_RAF_REWARD_RESPONSE (0x4202EA) wire (client deserializer sub_7FF7290B4C60):
        //   uint32 Result, uint32 RecruitCount, uint8 (bits 5-7 = a 3-bit enum), <sub_195730>, <sub_139200>,
        //   RecruitCount x <recruit descriptor>. The two nested blocks + recruit list are emitted in their
        //   zero form for a plain claim result (no per-claim recruit deltas needed).
        class ClaimRafRewardResponse final : public ServerPacket
        {
        public:
            ClaimRafRewardResponse() : ServerPacket(SMSG_CLAIM_RAF_REWARD_RESPONSE) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
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
            std::vector<RafRecruit> Recruits;
        };
    }
}

#endif // TRINITYCORE_REFER_A_FRIEND_PACKETS_H

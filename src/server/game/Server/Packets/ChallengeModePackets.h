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

#ifndef TRINITYCORE_CHALLENGE_MODE_PACKETS_H
#define TRINITYCORE_CHALLENGE_MODE_PACKETS_H

#include "Packet.h"
#include "MythicPlusPacketsCommon.h"
#include "ObjectGuid.h"
#include <array>
#include <vector>

namespace WorldPackets
{
    namespace ChallengeMode
    {
        // CMSG_REQUEST_MYTHIC_PLUS_SEASON_DATA -- empty request (client serializer carries no payload @68275).
        class RequestMythicPlusSeasonData final : public ClientPacket
        {
        public:
            explicit RequestMythicPlusSeasonData(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_MYTHIC_PLUS_SEASON_DATA, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_REQUEST_MYTHIC_PLUS_AFFIXES -- empty request (client serializer carries no payload @68275).
        class RequestMythicPlusAffixes final : public ClientPacket
        {
        public:
            explicit RequestMythicPlusAffixes(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_MYTHIC_PLUS_AFFIXES, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_RESET_CHALLENGE_MODE -- player requests to abort/reset the active run (empty payload).
        class ResetChallengeMode final : public ClientPacket
        {
        public:
            explicit ResetChallengeMode(WorldPacket&& packet) : ClientPacket(CMSG_RESET_CHALLENGE_MODE, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_START_CHALLENGE_MODE -- the player slots a keystone into the font of power to begin the run.
        // Wire (client serializer @68275): uint8 Bag; uint32 Slot; ObjectGuid GameObjectGUID (plain byte stream).
        class StartChallengeMode final : public ClientPacket
        {
        public:
            explicit StartChallengeMode(WorldPacket&& packet) : ClientPacket(CMSG_START_CHALLENGE_MODE, std::move(packet)) { }

            void Read() override;

            ObjectGuid GameObjectGUID;
            uint32 Slot = 0;
            uint8 Bag = 0;
        };

        // SMSG_MYTHIC_PLUS_SEASON_DATA -- whether the Mythic+ season is currently active.
        // Wire (client deserializer sub_7FF729091240 @68275): a single bit (bool, MSB-first) + FlushBits. Nothing else.
        class MythicPlusSeasonData final : public ServerPacket
        {
        public:
            explicit MythicPlusSeasonData() : ServerPacket(SMSG_MYTHIC_PLUS_SEASON_DATA, 1) { }

            WorldPacket const* Write() override;

            bool IsMythicPlusActive = false;
        };

        // One entry of SMSG_MYTHIC_PLUS_CURRENT_AFFIXES. Field layout is binary-confirmed as two uint32; the second
        // is the SeasonID, per C_MythicPlus.GetCurrentAffixes returning {id, seasonID}.
        struct CurrentAffix
        {
            int32 KeystoneAffixID = 0;
            int32 SeasonID = 0;
        };

        // SMSG_MYTHIC_PLUS_CURRENT_AFFIXES -- the affixes in effect this week.
        // Wire (client deserializer sub_7FF729091470 -> sub_7FF729091290 @68275):
        //   uint32 Count; Count x { uint32 KeystoneAffixID; uint32 SeasonID }   (all plain 4-byte LE).
        class MythicPlusCurrentAffixes final : public ServerPacket
        {
        public:
            explicit MythicPlusCurrentAffixes() : ServerPacket(SMSG_MYTHIC_PLUS_CURRENT_AFFIXES, 4 + 8 * 4) { }

            WorldPacket const* Write() override;

            std::vector<CurrentAffix> Affixes;
        };

        // CMSG_MYTHIC_PLUS_REQUEST_MAP_STATS -- client asks for the player's per-dungeon best-run scores (empty payload).
        class MythicPlusRequestMapStats final : public ClientPacket
        {
        public:
            explicit MythicPlusRequestMapStats(WorldPacket&& packet) : ClientPacket(CMSG_MYTHIC_PLUS_REQUEST_MAP_STATS, std::move(packet)) { }

            void Read() override { }
        };

        // One member row of a dungeon best run. Wire (client deserializer sub_7FF729166F60 @68275, byte-aligned):
        //   uint64 Field0; PackedGuid PlayerGUID; PackedGuid OwnerGUID; uint32 x3; uint8 Flag; uint32 x3.
        struct MythicPlusMapStatMember
        {
            uint64 Field0 = 0;
            ObjectGuid PlayerGUID;
            ObjectGuid OwnerGUID;            // bnet/guild guid; unused by our populate
            uint32 Field56 = 0;
            uint32 Field60 = 0;
            uint32 Field64 = 0;
            uint8 Flag = 0;
            uint32 Field72 = 0;
            uint32 Field76 = 0;
            uint32 Field80 = 0;
        };

        // Per-dungeon best-run summary. Wire (client deserializer sub_7FF729167070 @68275):
        //   uint32 MapChallengeModeID; uint32 BestLevel; uint32 DurationMs; uint64 x2; uint32; uint32[4] Affixes;
        //   uint32 MemberCount; uint32 x2; MemberCount x MythicPlusMapStatMember.
        // Field semantics beyond MapChallengeModeID/Affixes/BestLevel/DurationMs are not yet sniff-confirmed.
        struct MythicPlusMapStat
        {
            uint32 MapChallengeModeID = 0;
            uint32 BestLevel = 0;               // Field8 (UNVERIFIED slot)
            uint32 DurationMs = 0;              // Field12 (UNVERIFIED slot)
            uint64 Field16 = 0;
            uint64 Field24 = 0;
            uint32 Field32 = 0;
            std::array<uint32, 4> Affixes = { };
            uint32 Field64 = 0;
            uint32 Field68 = 0;
            std::vector<MythicPlusMapStatMember> Members;
        };

        // A season best-run entry (second top-level vector). Wire (40-byte element in sub_7FF729091040):
        //   uint64; uint32; uint32; uint64; uint64; uint8.
        struct MythicPlusSeasonBest
        {
            uint64 Field0 = 0;
            uint32 Field8 = 0;
            uint32 Field12 = 0;
            uint64 Field16 = 0;
            uint64 Field24 = 0;
            uint8 Flag = 0;
        };

        // SMSG_MYTHIC_PLUS_ALL_MAP_STATS -- the player's dungeon-score list. Wire (client deserializer
        // sub_7FF729091040 @68275, byte-aligned, no bit-packing):
        //   uint32 MapCount; uint32 SeasonBestCount; uint32 Field80; uint32 Field84;
        //   MapCount x MythicPlusMapStat; SeasonBestCount x MythicPlusSeasonBest.
        class MythicPlusAllMapStats final : public ServerPacket
        {
        public:
            explicit MythicPlusAllMapStats() : ServerPacket(SMSG_MYTHIC_PLUS_ALL_MAP_STATS, 16) { }

            WorldPacket const* Write() override;

            std::vector<MythicPlusMapStat> MapStats;
            std::vector<MythicPlusSeasonBest> SeasonBests;
            uint32 Field80 = 0;
            uint32 Field84 = 0;
        };

        // SMSG_CHALLENGE_MODE_START -- announces a keystone run to the party. Wire (client deserializer
        // sub_7FF729090970 @68275, byte-aligned):
        //   uint32 x4; uint64; uint32[4] Affixes; uint32 MemberCount; uint8 Flags (3 bit-flags in one byte);
        //   MemberCount x member (720-byte specs/talents element).
        // We send MemberCount = 0: the member element is not populated yet (its nested talent vectors are deep).
        // Scalar-field semantics beyond Affixes are not sniff-confirmed; the wire framing is exact (no desync).
        class ChallengeModeStart final : public ServerPacket
        {
        public:
            explicit ChallengeModeStart() : ServerPacket(SMSG_CHALLENGE_MODE_START, 41) { }

            WorldPacket const* Write() override;

            uint32 MapChallengeModeID = 0;      // field @32 (UNVERIFIED slot)
            uint32 KeystoneLevel = 0;           // field @36 (UNVERIFIED slot)
            uint32 Field40 = 0;
            uint32 Field44 = 0;
            uint64 DeployedTime = 0;            // field @48 (UNVERIFIED slot)
            std::array<uint32, 4> Affixes = { };
            uint8 Flags = 0;
        };

        // Shared writers for the map-summary sub-struct (sub_7FF729167070), reused by ALL_MAP_STATS and COMPLETE.
        ByteBuffer& operator<<(ByteBuffer& data, MythicPlusMapStatMember const& member);
        ByteBuffer& operator<<(ByteBuffer& data, MythicPlusMapStat const& mapStat);

        // SMSG_CHALLENGE_MODE_COMPLETE -- run result, sent to the party on completion. Full wire idat-traced
        // (outer sub_7FF729090EE0 -> inner sub_7FF729090C10; byte-aligned; see c:\dumps\COMPLETE_PACKET_WIRE_68275.md):
        //   MapSummary(sub_7FF729167070); uint32 F124; uint32 NamesCount; uint32 F216; uint8 Flags(3 bits);
        //   uint32 RunsCount; uint32 PairsCount; uint64 F208;
        //   PairsCount x {uint32; uint32};
        //   RunsCount  x { RunElement(sub_7FF7291CBD40): uint32; uint8 optBit; SubC{uint8 cnt; cnt x {uint8;uint32}};
        //                  opt[uint8; uint32 n; n x uint32];  ...then trailing uint32 };
        //   NamesCount x { PackedGuid; uint8 packed(bit1=flag, value>>2=len); byte[len] name }.
        // The three trailing lists are sent empty (0 count); the DungeonScoreData/run tree is not persisted
        // server-side. MapSummary carries the completed run (map/level/affixes + present players as members).
        class ChallengeModeComplete final : public ServerPacket
        {
        public:
            explicit ChallengeModeComplete() : ServerPacket(SMSG_CHALLENGE_MODE_COMPLETE, 64) { }

            WorldPacket const* Write() override;

            MythicPlusMapStat MapSummary;
            uint32 Field124 = 0;
            uint32 Field216 = 0;
            uint64 Field208 = 0;
            uint8 Flags = 0;
        };
    }
}

#endif // TRINITYCORE_CHALLENGE_MODE_PACKETS_H

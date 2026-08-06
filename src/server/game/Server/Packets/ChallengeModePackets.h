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
#include "ItemPacketsCommon.h"
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
        // SMSG_CHALLENGE_MODE_RESET (0x4200AF) — sent when a keystone run is reset. Wire (m+ run12.0.7.pkt, 4B): { uint32 MapID }.
        class ChallengeModeReset final : public ServerPacket
        {
        public:
            explicit ChallengeModeReset() : ServerPacket(SMSG_CHALLENGE_MODE_RESET, 4) { }
            WorldPacket const* Write() override;

            uint32 MapID = 0;
        };

        // SMSG_CHALLENGE_MODE_UPDATE_DEATH_COUNT — refreshes the client's live death counter / timer penalty
        // display during a run. Wire: { uint32 DeathCount } (long-stable community layout; not present in the
        // 68275 sniff because it was never sent before - verify on capture).
        class ChallengeModeUpdateDeathCount final : public ServerPacket
        {
        public:
            explicit ChallengeModeUpdateDeathCount() : ServerPacket(SMSG_CHALLENGE_MODE_UPDATE_DEATH_COUNT, 4) { }
            WorldPacket const* Write() override;

            uint32 DeathCount = 0;
        };

        // SMSG_MYTHIC_PLUS_NEW_WEEK_RECORD (0x4200BA) / SMSG_CHALLENGE_MODE_NEW_PLAYER_RECORD (0x4200B1) — sent when a
        // new weekly/personal best is set. Wire (m+ run12.0.7.pkt, 12B each): { uint32 MapChallengeModeID; uint32 CompletionMs; uint32 KeystoneLevel }.
        class MythicPlusNewWeekRecord final : public ServerPacket
        {
        public:
            explicit MythicPlusNewWeekRecord() : ServerPacket(SMSG_MYTHIC_PLUS_NEW_WEEK_RECORD, 12) { }
            WorldPacket const* Write() override;

            uint32 MapChallengeModeID = 0;
            uint32 CompletionMs = 0;
            uint32 KeystoneLevel = 0;
        };

        class ChallengeModeNewPlayerRecord final : public ServerPacket
        {
        public:
            explicit ChallengeModeNewPlayerRecord() : ServerPacket(SMSG_CHALLENGE_MODE_NEW_PLAYER_RECORD, 12) { }
            WorldPacket const* Write() override;

            uint32 MapChallengeModeID = 0;
            uint32 CompletionMs = 0;
            uint32 KeystoneLevel = 0;
        };

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

            // Wire order VERIFIED from m+ run12.0.7.pkt (SMSG_CHALLENGE_MODE_START, 45B body): MapID, then the
            // MapChallengeMode.db2 id, then the keystone level. The old code mislabeled these (sent the challenge id
            // in the MapID slot and the level in the challenge-id slot).
            uint32 MapID = 0;                   // instance MapID (sniff: 2526)
            uint32 MapChallengeModeID = 0;      // MapChallengeMode.db2 id (sniff: 402)
            uint32 KeystoneLevel = 0;           // sniff: 2
            uint32 Field44 = 0;
            uint64 DeployedTime = 0;
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

            // Names (record-holder / eligible-player) list element, as the client deserializer
            // (sub_7FF729090EE0) reads it: PackedGuid, then one packed byte (Name length in the high
            // 6 bits, IsEligibleForScore in bit 1), then the raw name bytes.
            struct MemberName
            {
                ObjectGuid PlayerGUID;
                bool IsEligibleForScore = false;
                std::string Name;
            };

            MythicPlusMapStat MapSummary;
            uint32 Field124 = 0;
            uint32 Field216 = 0;
            uint64 Field208 = 0;
            uint8 Flags = 0;
            std::vector<MemberName> Names;
        };

        // CMSG_REQUEST_WEEKLY_REWARDS -- client opens the Great Vault UI (empty payload @68275).
        class RequestWeeklyRewards final : public ClientPacket
        {
        public:
            explicit RequestWeeklyRewards(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_WEEKLY_REWARDS, std::move(packet)) { }

            void Read() override { }
        };

        // JamWeeklyRewardActivityTier (reflection-named): one Great Vault activity tier (M+/raid/pvp).
        struct WeeklyRewardActivityTier
        {
            int32 ActivityTierID = 0;
            int32 Level = 0;
            int32 Points = 0;
            uint8 Type = 0;                     // WeeklyRewardChestThresholdType (1 = MythicPlus)
        };

        // JamWeeklyRewardRaidEncounters (reflection-named): a raid boss credited toward a raid vault slot.
        struct WeeklyRewardRaidEncounter
        {
            int32 EncounterID = 0;
            int16 BestDifficultyID = 0;
        };

        // JamWeeklyRewardThresholdProgress (reflection-named): progress toward one vault reward slot.
        //   thresholdID -> WeeklyRewardChestThreshold.db2 (client resolves Type/Index/required-count);
        //   amount = runs completed, level = keystone level rewarded, earned = slot unlocked.
        // exampleItem/upgradeExampleItem (reward previews) are optional and sent absent (need the loot pool).
        struct WeeklyRewardThresholdProgress
        {
            int32 ThresholdID = 0;
            int32 Amount = 0;
            int32 ActivityTierID = 0;
            int32 Level = 0;
            std::vector<WeeklyRewardRaidEncounter> RaidEncounters;
            bool Earned = false;
        };

        // SMSG_WEEKLY_REWARDS_PROGRESS_RESULT -- the Great Vault progress display. Wire (client deserializer
        // sub_7FF7290B6DB0 -> JamHandler_JamWeeklyRewardActivityTier @68275, byte-aligned; field names from the
        // JAM reflection descriptors; see c:\dumps\VAULT_PACKET_WIRE_68275.md):
        //   uint8 Header; uint32 ThresholdProgressCount; uint32 ActivityTierCount; uint32 Unused;
        //   ActivityTierCount x { int32 ActivityTierID; int32 Level; int32 Points; uint8 Type };
        //   ThresholdProgressCount x { int32 ThresholdID; int32 Amount; int32 ActivityTierID; int32 Level;
        //       uint32 RaidEncCount; RaidEncCount x { int32 EncounterID; int16 BestDifficultyID };
        //       uint8 Flags(bit7=Earned, bit6=exampleItemPresent, bit5=upgradeItemPresent);
        //       [exampleItem][upgradeExampleItem] }.
        // We send both item-preview presence bits 0 (no loot preview) and no raid encounters (M+ only).
        class WeeklyRewardsProgressResult final : public ServerPacket
        {
        public:
            explicit WeeklyRewardsProgressResult() : ServerPacket(SMSG_WEEKLY_REWARDS_PROGRESS_RESULT, 16) { }

            WorldPacket const* Write() override;

            std::vector<WeeklyRewardActivityTier> ActivityTiers;
            std::vector<WeeklyRewardThresholdProgress> Progress;
            uint8 Header = 0;
            uint32 Unused = 0;
        };

        // CMSG_CLAIM_WEEKLY_REWARD -- player collects a Great Vault reward. Wire (@68275): uint32 RewardID.
        // The RewardID references the vault slot being claimed (WeeklyRewardChestThreshold.ID); not yet
        // sniff-confirmed, but the claim is validated + granted server-side, so a wrong id is a safe no-op.
        class ClaimWeeklyReward final : public ClientPacket
        {
        public:
            explicit ClaimWeeklyReward(WorldPacket&& packet) : ClientPacket(CMSG_CLAIM_WEEKLY_REWARD, std::move(packet)) { }

            void Read() override;

            uint32 RewardID = 0;
        };

        // JamWeeklyReward (reflection-named): one selectable Great Vault reward option. Wire (sub_7FF72915E170):
        //   uint32 Type; uint32 Value; uint8 flags(bit7=itemDBID, bit6=item, others date/currency); then each
        //   present optional in order [ItemInstance][u64 itemDBID][u64 itemDateCreated][u32 currencyType].
        // We emit item rewards only (item present, the rest absent). Type/Value are not sniff-confirmed (sent 0);
        // the ItemInstance preview is the authoritative reward content shown by the client.
        struct WeeklyReward
        {
            Item::ItemInstance Item;
            int32 Type = 0;
            int32 Value = 0;
            bool HasItem = false;
        };

        // One vault slot's reward options (outer element of SMSG_WEEKLY_REWARDS_RESULT).
        struct WeeklyRewardActivity
        {
            uint32 ThresholdID = 0;
            std::vector<WeeklyReward> Rewards;
        };

        ByteBuffer& operator<<(ByteBuffer& data, WeeklyReward const& reward);

        // SMSG_WEEKLY_REWARDS_RESULT -- the reward options shown in the Great Vault. Wire (client deserializer
        // sub_7FF7290B6800 @68275, byte-aligned):
        //   uint32 ActivityCount; uint32 Field56; ActivityCount x { uint32 ThresholdID; uint32 RewardCount;
        //     RewardCount x JamWeeklyReward }.
        class WeeklyRewardsResult final : public ServerPacket
        {
        public:
            explicit WeeklyRewardsResult() : ServerPacket(SMSG_WEEKLY_REWARDS_RESULT, 16) { }

            WorldPacket const* Write() override;

            std::vector<WeeklyRewardActivity> Activities;
            uint32 Field56 = 0;
        };

        // SMSG_WEEKLY_REWARD_CLAIM_RESULT -- outcome of a claim. Wire (sub_7FF7290B6960): a single uint8 (0 = ok).
        class WeeklyRewardClaimResult final : public ServerPacket
        {
        public:
            explicit WeeklyRewardClaimResult() : ServerPacket(SMSG_WEEKLY_REWARD_CLAIM_RESULT, 1) { }

            WorldPacket const* Write() override;

            uint8 Result = 0;
        };
    }
}

#endif // TRINITYCORE_CHALLENGE_MODE_PACKETS_H

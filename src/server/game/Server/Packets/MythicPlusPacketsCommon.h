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

#ifndef TRINITYCORE_MYTHIC_PLUS_PACKETS_COMMON_H
#define TRINITYCORE_MYTHIC_PLUS_PACKETS_COMMON_H

#include "ObjectGuid.h"
#include "PacketUtilities.h"

namespace WorldPackets
{
    namespace MythicPlus
    {
        struct DungeonScoreMapSummary
        {
            bool operator==(DungeonScoreMapSummary const&) const = default;
            int32 ChallengeModeID = 0;
            float MapScore = 0.0f;
            int32 BestRunLevel = 0;
            int32 BestRunDurationMS = 0;
            bool FinishedSuccess = false;
            uint8 Unknown1110 = 0;
        };

        struct DungeonScoreSummary
        {
            bool operator==(DungeonScoreSummary const&) const = default;
            float OverallScoreCurrentSeason = 0.0f;
            float LadderScoreCurrentSeason = 0.0f;
            std::vector<DungeonScoreMapSummary> Runs;
        };

        struct MythicPlusMember
        {
            bool operator==(MythicPlusMember const&) const = default;
            ObjectGuid BnetAccountGUID;
            uint64 GuildClubMemberID = 0;
            ObjectGuid GUID;
            ObjectGuid GuildGUID;
            uint32 NativeRealmAddress = 0;
            uint32 VirtualRealmAddress = 0;
            int32 ChrSpecializationID = 0;
            int8 RaceID = 0;
            int32 ItemLevel = 0;
            int32 CovenantID = 0;
            int32 SoulbindID = 0;
        };

        struct MythicPlusRun
        {
            bool operator==(MythicPlusRun const&) const = default;
            int32 MapChallengeModeID = 0;
            bool Completed = false;
            uint32 Level = 0;
            int32 DurationMs = 0;
            Timestamp<> StartDate;
            Timestamp<> CompletionDate;
            int32 Season = 0;
            std::vector<MythicPlusMember> Members;
            float RunScore = 0.0f;
            int32 Unknown_1120 = 0;
            std::array<int32, 4> KeystoneAffixIDs;
        };

        struct DungeonScoreBestRunForAffix
        {
            bool operator==(DungeonScoreBestRunForAffix const&) const = default;
            int32 KeystoneAffixID = 0;
            MythicPlusRun Run;
            float Score = 0.0f;
        };

        struct DungeonScoreMapData
        {
            bool operator==(DungeonScoreMapData const&) const = default;
            int32 MapChallengeModeID = 0;
            std::vector<DungeonScoreBestRunForAffix> BestRuns;
            float OverAllScore = 0.0f;
        };

        struct DungeonScoreSeasonData
        {
            bool operator==(DungeonScoreSeasonData const&) const = default;
            int32 Season = 0;
            std::vector<DungeonScoreMapData> SeasonMaps;
            std::vector<DungeonScoreMapData> LadderMaps;
            float SeasonScore = 0.0f;
            float LadderScore = 0.0f;
        };

        struct DungeonScoreData
        {
            bool operator==(DungeonScoreData const&) const = default;
            std::vector<DungeonScoreSeasonData> Seasons;
            int32 TotalRuns = 0;
        };

        ByteBuffer& operator<<(ByteBuffer& data, DungeonScoreSummary const& dungeonScoreSummary);
        ByteBuffer& operator<<(ByteBuffer& data, DungeonScoreData const& dungeonScoreData);
    }
}

#endif // TRINITYCORE_MYTHIC_PLUS_PACKETS_COMMON_H

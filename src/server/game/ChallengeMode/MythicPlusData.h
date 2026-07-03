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

#ifndef MythicPlusData_h__
#define MythicPlusData_h__

#include "DatabaseEnvFwd.h"
#include "Define.h"
#include <array>
#include <unordered_map>

class Player;

struct MythicPlusRunRecord
{
    uint32 ChallengeModeID = 0;
    uint32 Level = 0;
    uint32 DurationMs = 0;
    uint32 Deaths = 0;
    int64 CompletionDate = 0;               // unix timestamp of the run
    float Score = 0.0f;
    std::array<uint32, 4> Affixes = { };
};

// Per-player Mythic+ progression: the best run recorded for each dungeon and the overall rating derived from them.
// Attached to Player (see Player::GetMythicPlusData), loaded/saved with the character.
class TC_GAME_API MythicPlusData
{
public:
    explicit MythicPlusData(Player* owner);

    void LoadFromDB(PreparedQueryResult result);
    void SaveToDB(CharacterDatabaseTransaction trans);

    // Records a completed run, keeping the best per dungeon (higher level wins, ties broken by faster time).
    // Returns true if it became the new best for that dungeon.
    bool RecordRun(MythicPlusRunRecord const& run);
    MythicPlusRunRecord const* GetBestRun(uint32 challengeModeId) const;
    std::unordered_map<uint32, MythicPlusRunRecord> const& GetBestRuns() const { return _bestRuns; }

    // Sum of the best-run scores across all dungeons (the client's overall Mythic+ Rating).
    float GetOverallScore() const;

private:
    Player* _owner;
    std::unordered_map<uint32 /*challengeModeId*/, MythicPlusRunRecord> _bestRuns;
};

#endif // MythicPlusData_h__

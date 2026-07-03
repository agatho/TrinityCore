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

#include "MythicPlusData.h"
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "Player.h"

MythicPlusData::MythicPlusData(Player* owner) : _owner(owner) { }

void MythicPlusData::LoadFromDB(PreparedQueryResult result)
{
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        MythicPlusRunRecord run;
        run.ChallengeModeID = fields[0].GetUInt32();
        run.Level = fields[1].GetUInt32();
        run.DurationMs = fields[2].GetUInt32();
        run.Deaths = fields[3].GetUInt32();
        run.CompletionDate = fields[4].GetInt64();
        run.Score = fields[5].GetFloat();
        run.Affixes[0] = fields[6].GetUInt32();
        run.Affixes[1] = fields[7].GetUInt32();
        run.Affixes[2] = fields[8].GetUInt32();
        run.Affixes[3] = fields[9].GetUInt32();

        _bestRuns[run.ChallengeModeID] = run;
    } while (result->NextRow());
}

void MythicPlusData::SaveToDB(CharacterDatabaseTransaction trans)
{
    ObjectGuid::LowType guid = _owner->GetGUID().GetCounter();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    for (auto const& [challengeModeId, run] : _bestRuns)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS);
        stmt->setUInt64(0, guid);
        stmt->setUInt32(1, run.ChallengeModeID);
        stmt->setUInt32(2, run.Level);
        stmt->setUInt32(3, run.DurationMs);
        stmt->setUInt32(4, run.Deaths);
        stmt->setInt64(5, run.CompletionDate);
        stmt->setFloat(6, run.Score);
        stmt->setUInt32(7, run.Affixes[0]);
        stmt->setUInt32(8, run.Affixes[1]);
        stmt->setUInt32(9, run.Affixes[2]);
        stmt->setUInt32(10, run.Affixes[3]);
        trans->Append(stmt);
    }
}

bool MythicPlusData::RecordRun(MythicPlusRunRecord const& run)
{
    auto itr = _bestRuns.find(run.ChallengeModeID);
    if (itr != _bestRuns.end())
    {
        MythicPlusRunRecord const& best = itr->second;
        if (run.Level < best.Level || (run.Level == best.Level && run.DurationMs >= best.DurationMs))
            return false;
    }

    _bestRuns[run.ChallengeModeID] = run;
    return true;
}

MythicPlusRunRecord const* MythicPlusData::GetBestRun(uint32 challengeModeId) const
{
    auto itr = _bestRuns.find(challengeModeId);
    return itr != _bestRuns.end() ? &itr->second : nullptr;
}

float MythicPlusData::GetOverallScore() const
{
    float total = 0.0f;
    for (auto const& [challengeModeId, run] : _bestRuns)
        total += run.Score;
    return total;
}

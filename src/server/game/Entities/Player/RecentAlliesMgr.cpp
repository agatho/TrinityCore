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

#include "RecentAlliesMgr.h"
#include "CharacterDatabase.h"
#include "GameTime.h"
#include "Player.h"
#include "WorldSession.h"

namespace RecentAllies
{
static constexpr std::size_t MAX_RECENT_ALLIES = 100;

static void RecordOne(ObjectGuid owner, ObjectGuid ally, uint32 allyAccount)
{
    // Upsert: refresh the timestamp (and account) if we already grouped with them; a pre-existing note is kept.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_RECENT_ALLY);
    stmt->setUInt64(0, owner.GetCounter());
    stmt->setUInt64(1, ally.GetCounter());
    stmt->setUInt32(2, allyAccount);
    stmt->setUInt32(3, uint32(GameTime::GetGameTime()));
    CharacterDatabase.Execute(stmt);
}

void RecordGrouping(Player const* a, Player const* b)
{
    if (!a || !b || a == b || a->GetGUID() == b->GetGUID())
        return;

    RecordOne(a->GetGUID(), b->GetGUID(), b->GetSession()->GetAccountId());
    RecordOne(b->GetGUID(), a->GetGUID(), a->GetSession()->GetAccountId());
}

std::vector<AllyRecord> GetAllies(ObjectGuid owner)
{
    std::vector<AllyRecord> result;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_RECENT_ALLIES);
    stmt->setUInt64(0, owner.GetCounter());
    if (PreparedQueryResult res = CharacterDatabase.Query(stmt))
    {
        do
        {
            Field* f = res->Fetch();
            AllyRecord& record = result.emplace_back();
            record.Guid = ObjectGuid::Create<HighGuid::Player>(f[0].GetUInt64());
            record.WowAccount = f[1].GetUInt32();
            record.Note = f[2].GetString();
        } while (res->NextRow() && result.size() < MAX_RECENT_ALLIES);
    }

    return result;
}

void SetNote(ObjectGuid owner, ObjectGuid ally, std::string const& note)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_RECENT_ALLY_NOTE);
    stmt->setString(0, note);
    stmt->setUInt64(1, owner.GetCounter());
    stmt->setUInt64(2, ally.GetCounter());
    CharacterDatabase.Execute(stmt);
}

void SetAllowSeeLocation(ObjectGuid owner, bool allow)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_RECENT_ALLY_SETTING);
    stmt->setUInt64(0, owner.GetCounter());
    stmt->setBool(1, allow);
    CharacterDatabase.Execute(stmt);
}

bool GetAllowSeeLocation(ObjectGuid owner)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_RECENT_ALLY_SETTING);
    stmt->setUInt64(0, owner.GetCounter());
    if (PreparedQueryResult res = CharacterDatabase.Query(stmt))
        return res->Fetch()[0].GetBool();
    return true;    // default: allow (opt-out toggle)
}
}

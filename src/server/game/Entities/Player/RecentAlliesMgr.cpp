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
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Player.h"
#include "RecentAllyPackets.h"
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

// Push a recent-player delta to one online player. Only the "added" direction is ever populated. The client erases
// on the "removed" list, and the only removals observed on retail retract LFG-list applicants that an earlier packet
// had added - in the premade capture the same two GUIDs are added when they apply and removed when the applications
// go away. That is the group-finder listing's transient display cache, not recent allies: recent-ally records are
// persistent and nothing on the server ever means "forget this player". A removal we cannot justify would tell the
// client to drop someone it should still know, so we send none.
static void SendRecentPlayerAdds(Player const* to, std::vector<ObjectGuid> added)
{
    if (!to || added.empty())
        return;

    WorldSession* session = to->GetSession();
    if (!session)
        return;

    WorldPackets::Social::UpdateRecentPlayerGuids update;
    update.Added = std::move(added);
    session->SendPacket(update.Write());
}

void RecordGroupJoin(Player const* joiner, std::span<Player const* const> existing)
{
    if (!joiner)
        return;

    std::vector<ObjectGuid> addedForJoiner;
    addedForJoiner.reserve(existing.size());

    for (Player const* other : existing)
    {
        if (!other || other == joiner || other->GetGUID() == joiner->GetGUID())
            continue;

        if (!other->GetSession() || !joiner->GetSession())
            continue;

        RecordOne(joiner->GetGUID(), other->GetGUID(), other->GetSession()->GetAccountId());
        RecordOne(other->GetGUID(), joiner->GetGUID(), joiner->GetSession()->GetAccountId());

        addedForJoiner.push_back(other->GetGUID());

        // Everyone already in the group learns about exactly one newcomer.
        SendRecentPlayerAdds(other, { joiner->GetGUID() });
    }

    // The joiner learns about everyone already present, in one packet.
    SendRecentPlayerAdds(joiner, std::move(addedForJoiner));
}

void RecordGrouping(Player const* a, Player const* b)
{
    Player const* single[] = { b };
    RecordGroupJoin(a, single);
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

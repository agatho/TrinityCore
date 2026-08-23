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

#include "PreyMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>

namespace
{
    // A table that does not exist is NOT a recoverable error in this core.
    //
    // MySQLConnection::_HandleMySQLErrno() classifies ER_NO_SUCH_TABLE (1146) and
    // ER_BAD_FIELD_ERROR (1054) as "your database structure is not up to date",
    // sleeps ten seconds and calls ABORT() -> Trinity::Abort(), which is [[noreturn]]
    // and takes the whole process down. There is no result code to inspect and no
    // null QueryResult to branch on: control never returns to the caller.
    //
    // This is equally true of the ASYNCHRONOUS path. DatabaseWorkerPool<T>::Execute()
    // only posts the statement to a worker thread, which then calls the very same
    // MySQLConnection::Execute() -> _HandleMySQLErrno(). An async write against a
    // missing table kills the process just as dead, merely from a database worker
    // thread instead of the world thread. "Async, therefore safe" is false.
    //
    // So core code may never probe an optional table by simply running a query
    // against it. information_schema always exists, so asking it cannot raise 1146
    // and cannot abort. LoginRESTService uses the same technique to test for an
    // optional column.
    bool PreyHuntTemplateExists()
    {
        return WorldDatabase.Query("SELECT 1 FROM information_schema.TABLES"
            " WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'prey_hunt_template'") != nullptr;
    }

    bool CharacterPreyHuntExists()
    {
        return CharacterDatabase.Query("SELECT 1 FROM information_schema.TABLES"
            " WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_prey_hunt'") != nullptr;
    }
}

PreyMgr::PreyMgr() = default;
PreyMgr::~PreyMgr() = default;

/*static*/ PreyMgr* PreyMgr::instance()
{
    static PreyMgr instance;
    return &instance;
}

void PreyMgr::LoadFromDB()
{
    _huntTemplates.clear();
    _huntsByBucket.clear();
    // World::InitQuestResetTimes() has not run yet at this point in
    // SetInitialWorldSettings, so the week index is still 0 here. The first
    // Update tick establishes the real one and logs the opening rotation.
    _weekIndex = GetCurrentWeekIndex();
    _rotationCheckTimer = 0;
    _enabled = false;
    _characterHuntTableReady = false;

    // Existence is established against information_schema BEFORE the table is read.
    // Reading it to "see whether it is there" does not yield a null result on a realm
    // that has not applied the migration - it aborts the worldserver during startup.
    if (!PreyHuntTemplateExists())
    {
        TC_LOG_INFO("server.loading", ">> Prey: prey_hunt_template is not present in the world database; Prey system idle.");
        return;
    }

    QueryResult result = WorldDatabase.Query("SELECT Id, ZoneId, Difficulty, ContentTuningId, VaultActivityId FROM prey_hunt_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Prey: prey_hunt_template is empty; Prey system idle.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        PreyHuntTemplate tmpl;
        tmpl.Id              = fields[0].GetUInt32();
        tmpl.ZoneId          = fields[1].GetUInt32();
        uint8 diff           = fields[2].GetUInt8();
        tmpl.Difficulty      = diff < uint8(PreyDifficulty::Max) ? PreyDifficulty(diff) : PreyDifficulty::Normal;
        tmpl.ContentTuningId = fields[3].GetUInt32();
        tmpl.VaultActivityId = fields[4].GetUInt32();
        _huntTemplates.emplace(tmpl.Id, tmpl);
        _huntsByBucket[{ uint8(tmpl.Difficulty), tmpl.ZoneId }].push_back(tmpl.Id);
        ++count;
    } while (result->NextRow());

    // Sort every bucket so the weekly pick depends only on the week index and
    // the zone, never on row order coming back from MySQL.
    for (auto& bucket : _huntsByBucket)
        std::sort(bucket.second.begin(), bucket.second.end());

    _enabled = count != 0;

    // The per-character state lives in a DIFFERENT DATABASE behind a DIFFERENT
    // migration (sql/updates/characters/master/2026_08_12_00_characters_prey_voidforge.sql).
    // _enabled therefore says nothing whatsoever about whether character_prey_hunt
    // exists, so it is probed separately and every access to that table is gated on
    // this flag instead. Treating the world-side flag as authority over a
    // characters-side table is what made a realm with the world SQL applied and the
    // characters SQL missing abort on every single login.
    _characterHuntTableReady = CharacterPreyHuntExists();
    if (_enabled && !_characterHuntTableReady)
        TC_LOG_WARN("server.loading", ">> Prey: prey_hunt_template is loaded but character_prey_hunt is absent from the "
            "characters database - apply 2026_08_12_00_characters_prey_voidforge.sql. Per-character hunt state is disabled "
            "for this run; the rotation and the reward grants are unaffected.");

    TC_LOG_INFO("server.loading", ">> Prey: loaded {} hunt template(s) in {} rotation bucket(s).", count, _huntsByBucket.size());
}

void PreyMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    // The rotation only ever moves on a weekly boundary, so poll cheaply rather
    // than on every world tick.
    if (_rotationCheckTimer > diff)
    {
        _rotationCheckTimer -= diff;
        return;
    }

    _rotationCheckTimer = MINUTE * IN_MILLISECONDS;

    uint32 weekIndex = GetCurrentWeekIndex();
    if (weekIndex == _weekIndex)
        return;

    _weekIndex = weekIndex;
    TC_LOG_INFO("misc", "Prey: hunt rotation advanced to week {}; {} hunt(s) now active.", _weekIndex, GetWeeklyHuntQuests().size());
}

void PreyMgr::OnPlayerLogin(Player* player)
{
    // Gated on _characterHuntTableReady, NOT on IsEnabled(). IsEnabled() reflects
    // prey_hunt_template in the WORLD database; the query below reads
    // character_prey_hunt in the CHARACTERS database. Two databases, two migrations,
    // and a realm can trivially have one without the other - which is precisely what
    // happened: the world SQL was applied, _enabled came up true, and the resulting
    // query against the missing characters table aborted the worldserver on every
    // login until the characters migration was applied.
    if (!_enabled || !_characterHuntTableReady || !player)
        return;

    WorldSession* session = player->GetSession();
    if (!session)
        return;

    time_t const now = GameTime::GetGameTime();
    uint32 const weekStart = uint32(now - (now % WEEK));

    // Read this week's completed hunts so future logic (weekly cap / Journey rank UI)
    // can consult them. Asynchronous on purpose: this runs inside Player::LoadFromDB on
    // the world thread and nothing here is needed synchronously, so it must not stall
    // the world update for a MySQL round-trip once per login. The callback belongs to
    // the session's own QueryCallbackProcessor, so it dies with the session; the guid
    // re-check covers the character being swapped out before the result lands.
    session->GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(
        Trinity::StringFormat("SELECT HuntId, Difficulty, Status FROM character_prey_hunt WHERE guid = {} AND WeekStart = {}",
            player->GetGUID().GetCounter(), weekStart).c_str())
        .WithCallback([session, playerGuid = player->GetGUID()](QueryResult result)
    {
        if (!result)
            return;

        Player* thisPlayer = session->GetPlayer();
        if (!thisPlayer || thisPlayer->GetGUID() != playerGuid)
            return;

        uint32 rows = 0;
        do { ++rows; } while (result->NextRow());

        TC_LOG_DEBUG("entities.player", "Prey: player {} has {} hunt record(s) this week.",
            playerGuid.ToString(), rows);
    }));
}

PreyHuntTemplate const* PreyMgr::GetHuntTemplate(uint32 huntId) const
{
    auto it = _huntTemplates.find(huntId);
    return it != _huntTemplates.end() ? &it->second : nullptr;
}

/*static*/ uint32 PreyMgr::GetCurrentWeekIndex()
{
    // Anchor to the world's own weekly quest reset rather than to a private
    // timer, so a Prey hunt turns over at the same instant as everything else
    // the realm resets weekly. The stored stamp is the *end* of the current
    // week, so step back one period to get an index that is stable all week.
    time_t nextReset = sWorld->GetNextWeeklyQuestsResetTime();
    if (nextReset <= time_t(WEEK))
        return 0;

    return uint32((uint64(nextReset) - WEEK) / WEEK);
}

/*static*/ std::span<uint32 const> PreyMgr::GetStaticHuntPool(PreyDifficulty difficulty)
{
    switch (difficulty)
    {
        case PreyDifficulty::Normal: return Prey::HUNT_QUESTS_NORMAL;
        case PreyDifficulty::Hard:   return Prey::HUNT_QUESTS_HARD;
        // Nightmare has no captured quest ids. Returning empty is deliberate:
        // an invented id would be worse than an advertised gap.
        default:                     return {};
    }
}

std::vector<uint32> const* PreyMgr::GetRegisteredHuntPool(PreyDifficulty difficulty, uint32 zoneId) const
{
    auto it = _huntsByBucket.find({ uint8(difficulty), zoneId });
    if (it != _huntsByBucket.end() && !it->second.empty())
        return &it->second;

    // ZoneId 0 is the "unscoped" bucket every shipped row currently lands in.
    // A zone with no hunts of its own falls back to it rather than going dark.
    if (zoneId != 0)
    {
        it = _huntsByBucket.find({ uint8(difficulty), 0u });
        if (it != _huntsByBucket.end() && !it->second.empty())
            return &it->second;
    }

    return nullptr;
}

uint32 PreyMgr::GetWeeklyHuntQuest(PreyDifficulty difficulty, uint32 zoneId) const
{
    if (difficulty >= PreyDifficulty::Max)
        return 0;

    std::span<uint32 const> pool;
    if (std::vector<uint32> const* registered = GetRegisteredHuntPool(difficulty, zoneId))
        pool = *registered;
    else
        pool = GetStaticHuntPool(difficulty);

    if (pool.empty())
        return 0;

    // Offsetting by the zone keeps two zones from advertising the same hunt in
    // the same week. With every row at ZoneId 0 this is a no-op, which is the
    // correct behaviour for the data we actually have.
    uint64 index = uint64(GetCurrentWeekIndex()) + uint64(zoneId);
    return pool[index % pool.size()];
}

std::vector<uint32> PreyMgr::GetWeeklyHuntQuests() const
{
    std::vector<uint32> active;

    if (!_huntsByBucket.empty())
    {
        for (auto const& [bucket, hunts] : _huntsByBucket)
        {
            if (hunts.empty())
                continue;

            uint64 index = uint64(GetCurrentWeekIndex()) + uint64(bucket.second);
            active.push_back(hunts[index % hunts.size()]);
        }
    }
    else
    {
        for (uint8 difficulty = 0; difficulty < uint8(PreyDifficulty::Max); ++difficulty)
            if (uint32 questId = GetWeeklyHuntQuest(PreyDifficulty(difficulty), 0))
                active.push_back(questId);
    }

    std::sort(active.begin(), active.end());
    active.erase(std::unique(active.begin(), active.end()), active.end());
    return active;
}

bool PreyMgr::IsHuntQuest(uint32 questId) const
{
    return GetHuntDifficulty(questId) != PreyDifficulty::Max;
}

PreyDifficulty PreyMgr::GetHuntDifficulty(uint32 questId) const
{
    // Registry first — it is the shipped source of truth once the SQL is
    // applied. The static pools answer for a realm that has not applied it.
    if (PreyHuntTemplate const* tmpl = GetHuntTemplate(questId))
        return tmpl->Difficulty;

    for (uint8 difficulty = 0; difficulty < uint8(PreyDifficulty::Max); ++difficulty)
    {
        std::span<uint32 const> pool = GetStaticHuntPool(PreyDifficulty(difficulty));
        if (std::find(pool.begin(), pool.end(), questId) != pool.end())
            return PreyDifficulty(difficulty);
    }

    return PreyDifficulty::Max;
}

void PreyMgr::CreditHuntProgress(Player* player)
{
    if (!player)
        return;

    // Objective 0 of every hunt: ObjectID 246472 "Credit: Hunt your Prey",
    // Amount 1, no flags. Filling it reveals the SEQUENCED second objective.
    player->KilledMonsterCredit(Prey::NPC_CREDIT_HUNT_PROGRESS);
}

void PreyMgr::CreditHuntTargetSlain(Player* player)
{
    if (!player)
        return;

    // Objective 1 of every hunt: ObjectID 253450 "Credit: Multiple Credit",
    // Amount 1, Flags 2 SEQUENCED. This is the one the named target's death
    // must drive; the target creature entries are not in the capture, so the
    // call has to come from that creature's script once it is imported.
    player->KilledMonsterCredit(Prey::NPC_CREDIT_TARGET_SLAIN);
}

void PreyMgr::GrantJourneyProgress(Player* player, PreyDifficulty difficulty)
{
    // Gated on IsEnabled() so this is a hard no-op on the shared realm.
    if (!_enabled || !player)
        return;

    // --- Preyseeker's Journey track (currency 3387, FactionID 2764) ---
    // Plain CurrencyTypes track; ModifyCurrency clamps to the DB2 cap. Uses the stock
    // helper — no hand-rolled currency math. Amount is PLACEHOLDER (CAPTURE-BLOCKED).
    uint32 journeyPoints = 0;
    // --- faction-2764 reputation that drives the 3386 renown display currency ---
    int32 renownRep = 0;
    switch (difficulty)
    {
        case PreyDifficulty::Normal:
            journeyPoints = Prey::PLACEHOLDER_JOURNEY_POINTS_NORMAL;
            renownRep     = Prey::PLACEHOLDER_RENOWN_REP_NORMAL;
            break;
        case PreyDifficulty::Hard:
            journeyPoints = Prey::PLACEHOLDER_JOURNEY_POINTS_HARD;
            renownRep     = Prey::PLACEHOLDER_RENOWN_REP_HARD;
            break;
        case PreyDifficulty::Nightmare:
            journeyPoints = Prey::PLACEHOLDER_JOURNEY_POINTS_NIGHTMARE;
            renownRep     = Prey::PLACEHOLDER_RENOWN_REP_NIGHTMARE;
            break;
        default:
            return;
    }

    if (journeyPoints)
        player->ModifyCurrency(Prey::CURRENCY_PREYSEEKERS_JOURNEY, int32(journeyPoints), CurrencyGainSource::Script);

    // Renown display currency 3386 is faction 2764's RenownCurrencyID: it is NEVER
    // written directly. Feeding reputation through ReputationMgr crosses the per-level
    // thresholds, which bumps 3386 by level (CurrencyGainSource::RenownRepGain) inside
    // the stock path. This is the correct, non-reinvented renown grant.
    if (renownRep)
        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(Prey::FACTION_PREY_SEASON_1))
            player->GetReputationMgr().ModifyReputation(factionEntry, renownRep);
}

void PreyMgr::StartHunt(Player* /*player*/, uint32 /*huntId*/, PreyDifficulty /*difficulty*/)
{
    // STILL CAPTURE-BLOCKED: the Hunt Table (npc 245824, subname "missions")
    // activation is a mission-table opcode flow not present in any capture we
    // hold, and the 12.1.0.69273 sniff did not change that — its opcode space is
    // renumbered, so even a matching packet there could not be trusted here.
    // The hunt *content* now exists (quests, objectives, credit ids), but the
    // handler that hands a player a hunt does not. This remains the seam.
}

void PreyMgr::CompleteHunt(Player* player, PreyDifficulty difficulty, uint32 huntId)
{
    // Grant the per-difficulty DIRECT rewards. Gated on IsEnabled() (shared-realm-safe).
    // The reward MECHANISM (ModifyCurrency onto DB2-confirmed ids) is LIVE; the AMOUNTS
    // are PLACEHOLDER — see Prey::PLACEHOLDER_* / TODO(CAPTURE-BLOCKED). The exact reward
    // packet was never captured (blueprint §7 ask #2).
    if (!_enabled || !player)
        return;

    // Kill credit runs only when we know WHICH hunt finished. A hunt is an ordinary quest
    // whose two objectives are kill credits on shared bunnies, so completing one is these two
    // calls and the stock quest system does the rest. The `.prey grant` debug driver passes no
    // hunt id: it exercises the economy without pretending a quest was completed.
    if (huntId && IsHuntQuest(huntId))
    {
        CreditHuntProgress(player);
        CreditHuntTargetSlain(player);
    }

    // Journey points and faction-2764 reputation are deliberately NOT granted here:
    // GrantJourneyProgress is a separate public call and the debug driver invokes it
    // alongside this one. Granting in both places would double every hunt's Journey.

    // Clean difficulty -> direct-reward dispatch.
    switch (difficulty)
    {
        case PreyDifficulty::Normal:
            // Adventurer Dawncrest (3383).
            player->ModifyCurrency(Prey::CURRENCY_DAWNCREST_ADVENTURER,
                Prey::PLACEHOLDER_DAWNCREST_COUNT_NORMAL, CurrencyGainSource::Script);
            break;
        case PreyDifficulty::Hard:
            // Veteran Dawncrest (3341).
            player->ModifyCurrency(Prey::CURRENCY_DAWNCREST_VETERAN,
                Prey::PLACEHOLDER_DAWNCREST_COUNT_HARD, CurrencyGainSource::Script);
            break;
        case PreyDifficulty::Nightmare:
            // Champion (3343) + Hero (3345) Dawncrests, plus the Voidforge bonus-roll
            // currency Nebulous Voidcore (3418).
            player->ModifyCurrency(Prey::CURRENCY_DAWNCREST_CHAMPION,
                Prey::PLACEHOLDER_DAWNCREST_COUNT_NIGHTMARE, CurrencyGainSource::Script);
            player->ModifyCurrency(Prey::CURRENCY_DAWNCREST_HERO,
                Prey::PLACEHOLDER_DAWNCREST_COUNT_NIGHTMARE, CurrencyGainSource::Script);
            player->ModifyCurrency(Prey::CURRENCY_NEBULOUS_VOIDCORE,
                Prey::PLACEHOLDER_NEBULOUS_VOIDCORE_COUNT, CurrencyGainSource::Script);
            break;
        default:
            return;
    }

    // TODO(CAPTURE-BLOCKED — vault dependency): Great Vault credit is a deliberate
    // no-op here. It rides the fork's WeeklyRewardsMgr::RecordActivity, which lives on
    // feature/mythic-plus and is ABSENT from this baseline (blueprint §5). Once that
    // branch is merged, credit the row here, e.g.:
    //   sWeeklyRewardsMgr->RecordActivity(player, ActivityType::Prey, preyLevel);

    // Persist the weekly hunt state (skipped inside when character_prey_hunt is absent).
    RecordHuntCompletion(player, difficulty);
}

void PreyMgr::RecordHuntCompletion(Player* player, PreyDifficulty difficulty)
{
    // Same gate, same reason as OnPlayerLogin: this is a CHARACTERS-database table and
    // the world-database _enabled flag cannot vouch for it.
    if (!_enabled || !_characterHuntTableReady || !player)
        return;

    // Weekly-reset bucket. NOTE: a coarse UTC-week truncation, not the live weekly
    // reset alignment (that lands with the hunt-lifecycle wire, blueprint Phase 3).
    time_t const now = GameTime::GetGameTime();
    uint32 const weekStart = uint32(now - (now % WEEK));

    // Debug/economy slice: there is no real hunt template (activation is CAPTURE-BLOCKED),
    // so key the row by difficulty. This preserves the intended 1-record-per-difficulty
    // -per-week shape via PK(guid, HuntId, WeekStart). REPLACE keeps re-grants idempotent.
    //
    // NOTE: being asynchronous buys this statement NOTHING in safety. PExecute posts the
    // statement to a database worker thread which runs the identical
    // MySQLConnection::Execute -> _HandleMySQLErrno, and ER_NO_SUCH_TABLE ABORT()s the
    // process from that thread. The existence probe above is the only thing making this
    // call safe on a realm without the characters migration.
    CharacterDatabase.PExecute(
        "REPLACE INTO character_prey_hunt (guid, HuntId, Difficulty, Status, WeekStart) VALUES ({}, {}, {}, {}, {})",
        player->GetGUID().GetCounter(), uint32(difficulty), uint32(difficulty),
        uint32(Prey::HUNT_STATUS_COMPLETED), weekStart);
}

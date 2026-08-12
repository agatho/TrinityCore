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
    _enabled = false;

    // Realm-safe: the shipped table may not be applied on the shared realm.
    // A missing table yields a null result (logged, non-fatal) -> silent no-op.
    QueryResult result = WorldDatabase.Query("SELECT Id, ZoneId, Difficulty, ContentTuningId, VaultActivityId FROM prey_hunt_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Prey: prey_hunt_template absent or empty; Prey system idle.");
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
        ++count;
    } while (result->NextRow());

    _enabled = count != 0;
    TC_LOG_INFO("server.loading", ">> Prey: loaded {} hunt template(s).", count);
}

void PreyMgr::Update(uint32 /*diff*/)
{
    // Reserved for weekly-reset / hunt-lifecycle timers. No-op until content lands.
    if (!_enabled)
        return;
}

void PreyMgr::OnPlayerLogin(Player* player)
{
    // Gated on IsEnabled(): the shared realm has no prey_hunt_template, so we never
    // even touch character_prey_hunt there (realm-safe). On a seeded test DB, read
    // back this week's completed hunts so future logic (weekly cap / Journey rank UI)
    // can consult them. Tolerant of an absent table: a null result is a clean no-op.
    if (!_enabled || !player)
        return;

    time_t const now = GameTime::GetGameTime();
    uint32 const weekStart = uint32(now - (now % WEEK));

    QueryResult result = CharacterDatabase.PQuery(
        "SELECT HuntId, Difficulty, Status FROM character_prey_hunt WHERE guid = {} AND WeekStart = {}",
        player->GetGUID().GetCounter(), weekStart);
    if (!result)
        return;

    uint32 rows = 0;
    do { ++rows; } while (result->NextRow());

    TC_LOG_DEBUG("entities.player", "Prey: player {} has {} hunt record(s) this week.",
        player->GetGUID().ToString(), rows);
}

PreyHuntTemplate const* PreyMgr::GetHuntTemplate(uint32 huntId) const
{
    auto it = _huntTemplates.find(huntId);
    return it != _huntTemplates.end() ? &it->second : nullptr;
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

void PreyMgr::CompleteHunt(Player* player, PreyDifficulty difficulty)
{
    // Grant the per-difficulty DIRECT rewards. Gated on IsEnabled() (shared-realm-safe).
    // The reward MECHANISM (ModifyCurrency onto DB2-confirmed ids) is LIVE; the AMOUNTS
    // are PLACEHOLDER — see Prey::PLACEHOLDER_* / TODO(CAPTURE-BLOCKED). The exact reward
    // packet was never captured (blueprint §7 ask #2).
    if (!_enabled || !player)
        return;

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

    // Persist the weekly hunt state (gated + table-tolerant inside).
    RecordHuntCompletion(player, difficulty);
}

void PreyMgr::RecordHuntCompletion(Player* player, PreyDifficulty difficulty)
{
    if (!_enabled || !player)
        return;

    // Weekly-reset bucket. NOTE: a coarse UTC-week truncation, not the live weekly
    // reset alignment (that lands with the hunt-lifecycle wire, blueprint Phase 3).
    time_t const now = GameTime::GetGameTime();
    uint32 const weekStart = uint32(now - (now % WEEK));

    // Debug/economy slice: there is no real hunt template (activation is CAPTURE-BLOCKED),
    // so key the row by difficulty. This preserves the intended 1-record-per-difficulty
    // -per-week shape via PK(guid, HuntId, WeekStart). REPLACE keeps re-grants idempotent.
    // Async PExecute is tolerant of an absent table (logs a DB error, never crashes).
    CharacterDatabase.PExecute(
        "REPLACE INTO character_prey_hunt (guid, HuntId, Difficulty, Status, WeekStart) VALUES ({}, {}, {}, {}, {})",
        player->GetGUID().GetCounter(), uint32(difficulty), uint32(difficulty),
        uint32(Prey::HUNT_STATUS_COMPLETED), weekStart);
}

void PreyMgr::StartHunt(Player* /*player*/, uint32 /*huntId*/, PreyDifficulty /*difficulty*/)
{
    // CAPTURE-BLOCKED: the Hunt Table (npc 245824, subname "missions") activation
    // is a mission-table opcode flow not present in captures. This is the future
    // entry point for that handler. See blueprint §7 tester-capture ask #1. The
    // temporary `.prey grant` debug command stands in for it in this slice.
}

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
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"

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

void PreyMgr::OnPlayerLogin(Player* /*player*/)
{
    // Reserved: restore in-flight hunt state / Journey rank UI on login.
    // No-op until hunt persistence (character_prey_hunt) is populated.
}

PreyHuntTemplate const* PreyMgr::GetHuntTemplate(uint32 huntId) const
{
    auto it = _huntTemplates.find(huntId);
    return it != _huntTemplates.end() ? &it->second : nullptr;
}

void PreyMgr::GrantJourneyProgress(Player* player, uint32 points)
{
    if (!player || !points)
        return;

    // Preyseeker's Journey (currency 3387, FactionID 2764) is a plain currency track;
    // ModifyCurrency clamps to the DB2 cap.
    player->ModifyCurrency(Prey::CURRENCY_PREYSEEKERS_JOURNEY, int32(points));

    // NOTE: the renown *level* (currency 3386, faction 2764's RenownCurrencyID) is
    // NOT written directly. Per ReputationMgr::SetOneFactionReputation, renown level
    // is derived from reputation crossing per-level thresholds, which bumps 3386 via
    // ModifyCurrency(..., CurrencyGainSource::RenownRepGain). The correct grant is
    //   player->GetReputationMgr().ModifyReputation(factionEntry, <rep>, ...);
    // Wired in a later phase once the per-hunt reputation amount is DB2-confirmed.
}

void PreyMgr::StartHunt(Player* /*player*/, uint32 /*huntId*/, PreyDifficulty /*difficulty*/)
{
    // CAPTURE-BLOCKED: the Hunt Table (npc 245824, subname "missions") activation
    // is a mission-table opcode flow not present in captures. This is the future
    // entry point for that handler. See blueprint §6 tester-capture ask #1.
}

void PreyMgr::CompleteHunt(Player* player, uint32 huntId, PreyDifficulty difficulty)
{
    // CAPTURE-BLOCKED for the exact reward packet, but the *design* rewards are
    // DB2-anchored. This seam is intentionally inert until the completion wire and
    // the fork's weekly-reward vault-credit call are wired (blueprint Phase 2/3).
    if (!player)
        return;

    PreyHuntTemplate const* tmpl = GetHuntTemplate(huntId);
    if (!tmpl)
        return;

    // Journey progress (design cadence — [RESEARCH], not DB2-confirmed).
    GrantJourneyProgress(player, Prey::JOURNEY_POINTS_PER_HUNT);

    // Per-difficulty Dawncrest + (Nightmare) Nebulous Voidcore are granted here in a
    // later phase; Great Vault row credit (VaultActivityId) rides the weekly-reward
    // framework. Left as documented TODO to avoid inventing the reward amounts.
    (void)difficulty;
}

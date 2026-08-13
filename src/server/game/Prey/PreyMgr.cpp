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
#include "Log.h"
#include "Player.h"
#include "World.h"
#include <algorithm>

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
        _huntsByBucket[{ uint8(tmpl.Difficulty), tmpl.ZoneId }].push_back(tmpl.Id);
        ++count;
    } while (result->NextRow());

    // Sort every bucket so the weekly pick depends only on the week index and
    // the zone, never on row order coming back from MySQL.
    for (auto& bucket : _huntsByBucket)
        std::sort(bucket.second.begin(), bucket.second.end());

    _enabled = count != 0;
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
    // STILL CAPTURE-BLOCKED: the Hunt Table (npc 245824, subname "missions")
    // activation is a mission-table opcode flow not present in any capture we
    // hold, and the 12.1.0.69273 sniff did not change that — its opcode space is
    // renumbered, so even a matching packet there could not be trusted here.
    // The hunt *content* now exists (quests, objectives, credit ids), but the
    // handler that hands a player a hunt does not. This remains the seam.
}

void PreyMgr::CompleteHunt(Player* player, uint32 huntId, PreyDifficulty difficulty)
{
    // The completion *packet* is still unknown, but the completion *mechanic* is
    // not: a hunt is an ordinary quest whose two objectives are kill credits on
    // shared bunnies, so finishing one is two KilledMonsterCredit calls and the
    // stock quest system does the rest.
    if (!player)
        return;

    if (!IsHuntQuest(huntId))
        return;

    CreditHuntProgress(player);
    CreditHuntTargetSlain(player);

    // Journey progress (design cadence — [RESEARCH], not DB2-confirmed).
    GrantJourneyProgress(player, Prey::JOURNEY_POINTS_PER_HUNT);

    // Per-difficulty Dawncrest + (Nightmare) Nebulous Voidcore are granted here in a
    // later phase; Great Vault row credit (VaultActivityId) rides the weekly-reward
    // framework. Left as documented TODO to avoid inventing the reward amounts.
    (void)difficulty;
}

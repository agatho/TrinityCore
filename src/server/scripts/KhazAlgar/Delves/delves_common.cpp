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

#include "delves_common.h"
#include "Config.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DelveMgr.h"
#include "GameObject.h"
#include "InstanceScenario.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Optional.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ScenarioMgr.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include <list>

namespace Delves
{

namespace
{

enum DelveInstanceEvents
{
    EVENT_SUMMON_COMPANION = 1,
    EVENT_SPAWN_HEAVY_TRUNKS,
    EVENT_START_REWARD_SCENARIO,
};

// Retail timings, REPORT.md "Timeline tables" / 5.3 (gulf): SMSG_BOSS_KILL 1062320 -> Fragment of Revelation
// 1062329 -> SCENARIO_COMPLETED 1062360 -> Heavy Trunks 1062703 (+343 ms) -> SCENARIO_STATE 3424 1063114 (+754 ms).
constexpr Milliseconds HEAVY_TRUNK_SPAWN_DELAY = 350ms;
constexpr Milliseconds REWARD_SCENARIO_START_DELAY = 750ms;
// REPORT 3.1: the player casts 1247560 ~2.1 s after SMSG_NEW_WORLD (gulf 101829 -> 103987; eversong 1846873 -> 1848985)
constexpr Milliseconds COMPANION_SUMMON_DELAY = 2s;
// Grid search radius when looking for a player's companion summon (delve instances are a few hundred yards across)
constexpr float COMPANION_SEARCH_RANGE = 250.0f;

// Reward object placements read off the SMSG_UPDATE_OBJECT create blocks (delve_research/_cache, GameObject
// position quad at movement-block offset 7, creature/vehicle quad self-validated against the imported 2952
// spawns to < 0.001 yd). Map 2952 is byte-identical between 12.0.1 (deatholme) and 12.1 (eversong).
struct DelveCompletionSpawns
{
    uint32 MapId;
    Optional<Position> Fragment;    // 572806 - only The Gulf of Memory spawned one (gulf 1062329; census: absent in 2952 runs)
    Position HeavyTrunkA;           // 584517
    Position HeavyTrunkB;           // 584519
    Position LeaveOBot;             // 205496
};

DelveCompletionSpawns const CompletionSpawns[] =
{
    // The Gulf of Memory (gulf 1062329 / 1062703 / 1077344)
    { 2964, Position(-159.903f, 662.439f, 176.859f, 4.6662f), Position(-168.431f, 660.127f, 176.731f, 4.5728f),
            Position(-160.913f, 657.670f, 176.765f, 4.7822f), Position(-85.042f, 591.574f, 197.508f, 3.7092f) },
    // The Shadow Enclave (eversong 2612261 / 2621018 == deatholme 742784 / 757442)
    { 2952, {}, Position(166.078f, -101.342f, 218.528f, 1.5184f),
            Position(173.568f, -97.356f, 218.654f, 2.2864f), Position(151.821f, -65.059f, 226.183f, 4.6739f) },
    // The Darkway (3003): the shadowmoon capture ends before the boss - no placements; the fallbacks below use the
    // boss's death position instead.
};

DelveCompletionSpawns const* GetCompletionSpawns(uint32 mapId)
{
    for (DelveCompletionSpawns const& spawns : CompletionSpawns)
        if (spawns.MapId == mapId)
            return &spawns;
    return nullptr;
}

Position OffsetPosition(Position const& base, float dx, float dy)
{
    return Position(base.GetPositionX() + dx, base.GetPositionY() + dy, base.GetPositionZ(), base.GetOrientation());
}

} // anonymous namespace

Creature* FindDelveCompanionOf(Player const* player)
{
    if (!player || !player->IsInWorld())
        return nullptr;

    // 248567 is the retail 12.x companion (REPORT 3.1); the config entry is the branch's own companion switch
    // (DelvesCompanion::SpawnCompanion) - honour both so a configured companion is never doubled.
    uint32 entries[2] = { NPC_DELVE_COMPANION_VALEERA, uint32(sConfigMgr->GetIntDefault("Delves.Companion.CreatureId", 0)) };
    for (uint32 entry : entries)
    {
        if (!entry)
            continue;

        std::list<Creature*> candidates;
        player->GetCreatureListWithEntryInGrid(candidates, entry, COMPANION_SEARCH_RANGE);
        for (Creature* candidate : candidates)
            if (TempSummon const* summon = candidate->ToTempSummon())
                if (summon->GetSummonerGUID() == player->GetGUID())
                    return candidate;
    }

    return nullptr;
}

DelveInstanceScript::DelveInstanceScript(InstanceMap* map, uint8 tier)
    : InstanceScript(map)
{
    DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplate(map->GetId());
    _delveInstance = std::make_unique<DelveInstance>(map, tier, tmpl);
}

DelveInstanceScript::~DelveInstanceScript() = default;

void DelveInstanceScript::OnPlayerEnter(Player* player)
{
    InstanceScript::OnPlayerEnter(player);

    if (!_delveInstance)
        return;

    // First entrant locks the run's tier from their tier selection.
    if (_delveInstance->GetState() == DelveState::Entering &&
        player->m_delveSelectedTier > 0 && player->m_delveSelectedTier <= MAX_DELVE_TIER)
    {
        _delveInstance->SetTier(player->m_delveSelectedTier);
    }

    _delveInstance->OnPlayerEnter(player);

    // REPORT 6.2: the `scenarios` rows (2964,208,3177) / (2952,208,3154) / (3003,208,3184) only bind through
    // ScenarioMgr::CreateInstanceScenarioForTeam when the map was created with difficulty 208. Attach the delve
    // scenario from the template if the map came up without one, so completion never depends on the difficulty path.
    EnsureScenarioAttached();

    // Apply the tier scaling/affix aura. The TIER_SPELL_IDS auras are the
    // retail mechanism that drives mob HP/damage scaling and tier-specific
    // affixes inside the instance (verified via sniff 12.0.1.66527).
    uint8 tier = _delveInstance->GetTier();
    if (uint32 spellId = GetTierSpellId(tier))
    {
        // Refresh — strip any prior tier aura before applying the current one.
        for (uint32 sid : TIER_SPELL_IDS)
            if (sid && sid != spellId && player->HasAura(sid))
                player->RemoveAurasDueToSpell(sid);

        if (!player->HasAura(spellId))
            player->CastSpell(player, spellId, true);
    }

    // REPORT 1.6 (world states 24430/26345/26423/26931/26903/5029/25316/24836 set on the delve map) and REPORT 4
    // (gulf 108474, deatholme 105836: 25x 3310 Coffer Key Shards + 100x 3316 Voidlight Marl + item 263488 through
    // hidden quest 96612 / 93943 ~6.6 s after entry) - both handled by DelveMgr; called once per player per run.
    if (_grantedEntrants.insert(player->GetGUID()).second)
        sDelveMgr->OnPlayerEnteredDelve(player);

    // REPORT 3.1: the companion summon (player-cast 1247560) follows the entry by ~2 s
    _pendingCompanionSummons.push_back(player->GetGUID());
    _events.ScheduleEvent(EVENT_SUMMON_COMPANION, COMPANION_SUMMON_DELAY);
}

void DelveInstanceScript::OnPlayerLeave(Player* player)
{
    // The companion is bound to her summoner: she leaves the map with the player (REPORT 3.1 - three creates in
    // gulf, one per (re-)entry; never seen without her player).
    DespawnCompanionOf(player);

    if (_delveInstance)
        _delveInstance->OnPlayerExit(player);

    InstanceScript::OnPlayerLeave(player);
}

void DelveInstanceScript::Update(uint32 diff)
{
    InstanceScript::Update(diff);

    if (_delveInstance)
        _delveInstance->Update(diff);

    _events.Update(diff);
    while (uint32 eventId = _events.ExecuteEvent())
    {
        switch (eventId)
        {
            case EVENT_SUMMON_COMPANION:
            {
                std::vector<ObjectGuid> pending = std::move(_pendingCompanionSummons);
                _pendingCompanionSummons.clear();
                for (ObjectGuid const& guid : pending)
                    if (Player* player = ObjectAccessor::GetPlayer(instance, guid))
                        SummonCompanionFor(player);
                break;
            }
            case EVENT_SPAWN_HEAVY_TRUNKS:
                SpawnHeavyTrunks();
                break;
            case EVENT_START_REWARD_SCENARIO:
                StartRewardScenario();
                break;
            default:
                break;
        }
    }

    // Completion is the SCENARIO completing (REPORT 2.2 / 5.2: PROGRESS 60399 -> SCENARIO_STATE complete ->
    // SMSG_SCENARIO_COMPLETED 3177/3154), not a hardcoded boss entry. The core exposes no completion callback to the
    // instance script, so the current step and completion flag are observed here.
    InstanceScenario* scenario = instance->GetInstanceScenario();
    if (!scenario)
        return;

    ScenarioStepEntry const* step = scenario->GetStep();
    if (!_scenarioObserved || step != _lastObservedStep)
    {
        _scenarioObserved = true;
        _lastObservedStep = step;
        OnScenarioStepChanged(step);
    }

    if (!_completionHandled && scenario->GetEntry()->ID != DELVE_REWARD_SCENARIO_ID && scenario->IsComplete())
        HandleScenarioCompleted();
}

void DelveInstanceScript::EnsureScenarioAttached()
{
    if (instance->GetInstanceScenario())
        return;

    DelveTemplate const* tmpl = _delveInstance ? _delveInstance->GetTemplate() : nullptr;
    if (!tmpl)
        return;

    uint32 scenarioId = tmpl->ActiveScenarioId ? tmpl->ActiveScenarioId : tmpl->ScenarioId;
    if (!scenarioId)
        return;

    if (InstanceScenario* scenario = sScenarioMgr->CreateInstanceScenario(instance, scenarioId))
    {
        TC_LOG_DEBUG("scripts.delves", "Delve map {} instance {} came up without a scenario (difficulty {}); attaching scenario {} from delve_template.",
            instance->GetId(), instance->GetInstanceId(), uint32(instance->GetDifficultyID()), scenarioId);
        instance->SetInstanceScenario(scenario);
    }
}

void DelveInstanceScript::OnScenarioComplete()
{
    if (_delveInstance)
    {
        _delveInstance->OnScenarioComplete();

        if (_delveInstance->GetState() == DelveState::Completed)
            OnDelveComplete();
        else if (_delveInstance->GetState() == DelveState::Failed)
            OnDelveFailed();
    }
}

void DelveInstanceScript::HandleScenarioCompleted()
{
    if (_completionHandled)
        return;
    _completionHandled = true;

    // REPORT 5.2: ws 25316 -> 1 at the SMSG_SCENARIO_COMPLETED tick (gulf 1062360)
    DoUpdateWorldState(WS_DELVE_COMPLETE, 1);

    // REPORT 5.2: SMSG_ACHIEVEMENT_EARNED 40436 "You're Getting a Delve!" at the same tick (eversong 2611916; the
    // achievement is first-completion only, CompletedAchievement is a no-op for players who already have it)
    if (AchievementEntry const* achievement = sAchievementStore.LookupEntry(ACHIEVEMENT_YOURE_GETTING_A_DELVE))
        instance->DoOnPlayers([achievement](Player* player)
        {
            player->CompletedAchievement(achievement);
        });

    // REPORT 5.3: "Fragment of Revelation" immediately (gulf 1062329), the two Heavy Trunks ~350 ms later, then
    // scenario 3424 on a fresh ScenarioGUID (~750 ms).
    if (DelveCompletionSpawns const* spawns = GetCompletionSpawns(instance->GetId()))
        if (spawns->Fragment)
            SpawnDelveGameObject(GO_FRAGMENT_OF_REVELATION, *spawns->Fragment);

    _events.ScheduleEvent(EVENT_SPAWN_HEAVY_TRUNKS, HEAVY_TRUNK_SPAWN_DELAY);
    _events.ScheduleEvent(EVENT_START_REWARD_SCENARIO, REWARD_SCENARIO_START_DELAY);

    TC_LOG_DEBUG("scripts.delves", "Delve map {} instance {} completed (tier {}).",
        instance->GetId(), instance->GetInstanceId(), _delveInstance ? _delveInstance->GetTier() : 0);

    OnScenarioComplete();
}

void DelveInstanceScript::SpawnHeavyTrunks()
{
    if (DelveCompletionSpawns const* spawns = GetCompletionSpawns(instance->GetId()))
    {
        SpawnDelveGameObject(GO_HEAVY_TRUNK_A, spawns->HeavyTrunkA);
        SpawnDelveGameObject(GO_HEAVY_TRUNK_B, spawns->HeavyTrunkB);
        return;
    }

    // No captured placement for this map: put the trunks a few yards in front of the boss's corpse (REPORT 6.5:
    // The Darkway's capture ends before the boss). A REVIEW approximation, not a measured position.
    if (_hasFinalBossDeathPos)
    {
        SpawnDelveGameObject(GO_HEAVY_TRUNK_A, OffsetPosition(_finalBossDeathPos, 4.0f, 3.0f));
        SpawnDelveGameObject(GO_HEAVY_TRUNK_B, OffsetPosition(_finalBossDeathPos, 4.0f, -3.0f));
        return;
    }

    TC_LOG_WARN("scripts.delves", "Delve map {}: no Heavy Trunk placement known and no boss death position recorded; trunks not spawned.",
        instance->GetId());
}

void DelveInstanceScript::StartRewardScenario()
{
    // 3424 has no `scenarios` row (no map of its own) - REPORT 2.3 / work item 10. CreateInstanceScenario only needs
    // the Scenario/ScenarioStep DB2 rows, which ScenarioMgr::LoadDB2Data loads for every scenario.
    InstanceScenario* reward = sScenarioMgr->CreateInstanceScenario(instance, DELVE_REWARD_SCENARIO_ID);
    if (!reward)
    {
        TC_LOG_ERROR("scripts.delves", "Delve map {}: reward scenario {} could not be created (missing Scenario.db2 data?).",
            instance->GetId(), DELVE_REWARD_SCENARIO_ID);
        return;
    }

    // InstanceMap::SetInstanceScenario boots the finished delve scenario (SMSG_SCENARIO_VACATE) before sending the
    // new SCENARIO_STATE; retail went straight to the new ScenarioGUID (gulf 1063114). Harmless difference.
    instance->SetInstanceScenario(reward);
    _lastObservedStep = nullptr;
    _scenarioObserved = false;
}

GameObject* DelveInstanceScript::SpawnDelveGameObject(uint32 entry, Position const& pos)
{
    GameObject* go = GameObject::CreateGameObject(entry, instance, pos, QuaternionData::fromEulerAnglesZYX(pos.GetOrientation(), 0.0f, 0.0f), 255, GO_STATE_READY);
    if (!go)
    {
        TC_LOG_ERROR("scripts.delves", "Delve map {}: could not create gameobject {} (missing gameobject_template?).", instance->GetId(), entry);
        return nullptr;
    }

    if (!instance->AddToMap(go))
    {
        delete go;
        return nullptr;
    }

    return go;
}

Creature* DelveInstanceScript::SpawnLeaveOBot()
{
    if (_leaveOBotSpawned)
        return nullptr;

    Optional<Position> pos;
    if (DelveCompletionSpawns const* spawns = GetCompletionSpawns(instance->GetId()))
        pos = spawns->LeaveOBot;
    else if (_hasFinalBossDeathPos)
        pos = OffsetPosition(_finalBossDeathPos, 8.0f, 0.0f);   // REVIEW approximation (no capture for this map)

    if (!pos)
        return nullptr;

    _leaveOBotSpawned = true;

    // REPORT 2.3: Vehicle 205496 "Leave-O-Bot 7000" created +790 ms after 3424 reached step 17120 (gulf 1076552 -> 1077344)
    Creature* bot = instance->SummonCreature(NPC_LEAVE_O_BOT, *pos);
    if (!bot)
        TC_LOG_ERROR("scripts.delves", "Delve map {}: could not summon Leave-O-Bot {}.", instance->GetId(), NPC_LEAVE_O_BOT);
    return bot;
}

void DelveInstanceScript::SetData(uint32 type, uint32 /*data*/)
{
    switch (type)
    {
        case DATA_TREASURE_ROOM_OPENED:
            SpawnLeaveOBot();
            break;
        default:
            break;
    }
}

void DelveInstanceScript::ProcessEvent(WorldObject* /*obj*/, uint32 eventId, WorldObject* /*invoker*/)
{
    // GameEvents::Trigger(88888, player, trunk) reaches the instance script here and the 3424 criteria through
    // TriggerForPlayer (delve maps are InstanceType 5 = scenario, Map.db2).
    if (eventId == GAME_EVENT_TREASURE_FOUND)
        SpawnLeaveOBot();
}

void DelveInstanceScript::SummonCompanionFor(Player* player)
{
    if (player->GetMap() != instance || !player->IsAlive())
        return;

    // Already out (DelveInstance's configured companion, or a re-entry) - never two companions per player.
    if (FindDelveCompanionOf(player))
        return;

    // REPORT 3.1: PLAYER casts 1247560 "Summon" (12.1) / 1247562 (12.0.7); SpellEffect 28 SPELL_EFFECT_SUMMON with
    // MiscValue 248567 -> the create of Valeera Sanguinar lands in the same tick (gulf 103987).
    uint32 summonSpellId = 0;
    for (uint32 candidate : { SPELL_SUMMON_DELVE_COMPANION, SPELL_SUMMON_DELVE_COMPANION_12_0_7 })
    {
        if (sSpellMgr->GetSpellInfo(candidate, DIFFICULTY_NONE))
        {
            summonSpellId = candidate;
            break;
        }
    }

    if (summonSpellId)
        player->CastSpell(player, summonSpellId, true);

    if (Creature* companion = FindDelveCompanionOf(player))
    {
        TC_LOG_DEBUG("scripts.delves", "Delve companion {} summoned for {} by spell {}.", companion->GetGUID().ToString(), player->GetName(), summonSpellId);
        return;
    }

    // The summon effect did not produce 248567 (spell absent, SummonProperties 6425 missing, or the effect refused):
    // summon her directly beside the player. Retail's first create sat 15-22 yd from the entry point in every run
    // (she walks in), so "beside the player" is an approximation of the arrival, not of a spawn point.
    Position pos = player->GetNearPosition(3.0f, float(M_PI) / 4.0f);
    if (TempSummon* companion = player->SummonCreature(NPC_DELVE_COMPANION_VALEERA, pos, TEMPSUMMON_MANUAL_DESPAWN))
        TC_LOG_DEBUG("scripts.delves", "Delve companion {} summoned directly for {} (summon spell {} did not produce it).",
            companion->GetGUID().ToString(), player->GetName(), summonSpellId);
    else
        TC_LOG_ERROR("scripts.delves", "Delve map {}: could not summon companion {} for {}.", instance->GetId(), NPC_DELVE_COMPANION_VALEERA, player->GetName());
}

void DelveInstanceScript::DespawnCompanionOf(Player const* player)
{
    if (Creature* companion = FindDelveCompanionOf(player))
        companion->DespawnOrUnsummon();
}

void DelveInstanceScript::OnPlayerDeath(Player* player)
{
    if (_delveInstance)
        _delveInstance->OnPlayerDeath(player);
}

bool DelveInstanceScript::IsFinalBoss(Creature const* creature) const
{
    DelveTemplate const* tmpl = _delveInstance ? _delveInstance->GetTemplate() : nullptr;
    return tmpl && tmpl->FinalBossEntry && creature->GetEntry() == tmpl->FinalBossEntry;
}

bool DelveInstanceScript::SetBossState(uint32 id, EncounterState state)
{
    if (!InstanceScript::SetBossState(id, state))
        return false;

    // REPORT 1.6: ws 24836 = 1 at SMSG_ENCOUNTER_START (gulf 969466), 0 at SMSG_ENCOUNTER_END (1062320)
    if (state == IN_PROGRESS)
        DoUpdateWorldState(WS_DELVE_ENCOUNTER_IN_PROGRESS, 1);
    else if (state == DONE || state == FAIL || state == NOT_STARTED)
        DoUpdateWorldState(WS_DELVE_ENCOUNTER_IN_PROGRESS, 0);

    return true;
}

void DelveInstanceScript::OnUnitDeath(Unit* unit)
{
    InstanceScript::OnUnitDeath(unit);

    if (!_delveInstance || !unit)
        return;

    // Player death: decrement the shared revive pool; at 0 the run fails (rewards become unreachable and
    // subclasses may despawn objectives via OnDelveFailed). This override is what actually connects the
    // death-limit machinery — Unit::setDeathState routes every death in the instance through here.
    if (Player* player = unit->ToPlayer())
    {
        if (_delveInstance->GetState() != DelveState::InProgress)
            return;

        _delveInstance->OnPlayerDeath(player);
        if (_delveInstance->GetState() == DelveState::Failed)
        {
            TC_LOG_DEBUG("delves", "Delve instance {} (map {}) failed: revive pool exhausted.",
                instance->GetInstanceId(), instance->GetId());
            OnDelveFailed();
        }
        return;
    }

    Creature* creature = unit->ToCreature();
    if (!creature || _completionHandled || !IsFinalBoss(creature))
        return;

    _finalBossDeathPos = creature->GetPosition();
    _hasFinalBossDeathPos = true;

    // REPORT 2.2 / 5.1: the boss kill satisfies the scenario's last step through GameEvent 85913 (criteria 60399,
    // type 92 AnyoneTriggerGameEventScenario) - SMSG_BOSS_KILL 3359/3368 -> PROGRESS 60399 -> SCENARIO_COMPLETED.
    DoUpdateCriteria(CriteriaType::AnyoneTriggerGameEventScenario, GAME_EVENT_DELVE_BOSS_SLAIN, 0, creature);

    InstanceScenario* scenario = instance->GetInstanceScenario();
    if (!scenario)
    {
        // No scenario bound at all - the boss kill is the only completion signal we have.
        HandleScenarioCompleted();
    }
    else if (!scenario->IsComplete())
    {
        // The scenario is still on an earlier step (objective scripts for it are not written yet - work item 8).
        // Retail completion is the scenario's; finish it explicitly rather than leaving a dead boss and no rewards.
        TC_LOG_WARN("scripts.delves", "Delve map {}: final boss {} died while scenario {} was still on step {}; completing the scenario explicitly.",
            instance->GetId(), creature->GetEntry(), scenario->GetEntry()->ID, scenario->GetStep() ? scenario->GetStep()->ID : 0);
        scenario->CompleteScenario();
        HandleScenarioCompleted();
    }
    // else: Update() observes IsComplete() on the next tick and runs HandleScenarioCompleted()
}

} // namespace Delves

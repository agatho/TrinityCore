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

#include "ZoneEventMgr.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "Position.h"
#include "QuaternionData.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "WorldStateMgr.h"
#include <algorithm>

ZoneEventMgr::ZoneEventMgr() : _updateAccumulator(0) { }
ZoneEventMgr::~ZoneEventMgr() = default;

ZoneEventMgr* ZoneEventMgr::instance()
{
    static ZoneEventMgr instance;
    return &instance;
}

void ZoneEventMgr::LoadFromDB()
{
    uint32 const oldMSTime = getMSTime();

    _templates.clear();
    _states.clear();

    //                                                   0   1     2       3      4                  5                  6                      7              8                9
    QueryResult result = WorldDatabase.Query("SELECT Id, Type, ZoneId, MapId, StateWorldStateId, TimerWorldStateId, CountdownWorldStateId, PeriodSeconds, DurationSeconds, MeterCap FROM zone_event_template");
    if (!result)
    {
        // Table is intentionally NOT applied to the shared realm; a missing table
        // is a valid no-op for the skeleton. The SQL ships on this branch.
        TC_LOG_INFO("server.loading", ">> Loaded 0 Quel'Thalas zone events (table zone_event_template absent or empty)");
        return;
    }

    time_t const now = GameTime::GetGameTime();
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ZoneEventTemplate tmpl;
        tmpl.Id                    = fields[0].GetUInt32();
        tmpl.Type                  = static_cast<ZoneEventType>(fields[1].GetUInt8());
        tmpl.ZoneId                = fields[2].GetUInt32();
        tmpl.MapId                 = fields[3].GetUInt32();
        tmpl.StateWorldStateId     = fields[4].GetInt32();
        tmpl.TimerWorldStateId     = fields[5].GetInt32();
        tmpl.CountdownWorldStateId = fields[6].GetInt32();
        tmpl.PeriodSeconds         = fields[7].GetUInt32();
        tmpl.DurationSeconds       = fields[8].GetUInt32();
        tmpl.MeterCap              = fields[9].GetInt32();

        if (tmpl.Type >= ZoneEventType::Max)
        {
            TC_LOG_ERROR("sql.sql", "zone_event_template Id %u has invalid Type %u, skipped", tmpl.Id, uint32(fields[1].GetUInt8()));
            continue;
        }
        if (tmpl.PeriodSeconds == 0)
        {
            TC_LOG_ERROR("sql.sql", "zone_event_template Id %u has PeriodSeconds 0, skipped", tmpl.Id);
            continue;
        }

        _templates[tmpl.Id] = tmpl;

        ZoneEventState& state = _states[tmpl.Id];
        state.TemplateId = tmpl.Id;
        // Schedule the first activation at the next period boundary from server start.
        state.NextStart  = now + static_cast<time_t>(tmpl.PeriodSeconds);
        state.Active     = false;

        ++count;
    }
    while (result->NextRow());

    LoadSpawns();
    LoadScenarioWaves();

    TC_LOG_INFO("server.loading", ">> Loaded %u Quel'Thalas zone events in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// zone_event_spawn: event actor + destructibles. Ships with NO rows (spawn
// coordinates are CAPTURE-BLOCKED); a missing/empty table is a valid no-op.
void ZoneEventMgr::LoadSpawns()
{
    _spawns.clear();

    //                                                    0        1     2      3      4     5     6     7
    QueryResult result = WorldDatabase.Query("SELECT EventId, Kind, Entry, MapId, PosX, PosY, PosZ, Orientation FROM zone_event_spawn");
    if (!result)
        return; // table absent or empty -> no spawns (safe)

    do
    {
        Field* fields = result->Fetch();

        ZoneEventSpawn spawn;
        spawn.EventId     = fields[0].GetUInt32();
        spawn.Kind        = fields[1].GetUInt8();
        spawn.Entry       = fields[2].GetUInt32();
        spawn.MapId       = fields[3].GetUInt32();
        spawn.PosX        = fields[4].GetFloat();
        spawn.PosY        = fields[5].GetFloat();
        spawn.PosZ        = fields[6].GetFloat();
        spawn.Orientation = fields[7].GetFloat();

        if (!_templates.contains(spawn.EventId))
        {
            TC_LOG_ERROR("sql.sql", "zone_event_spawn references unknown EventId {}, skipped", spawn.EventId);
            continue;
        }
        if (spawn.Kind > 1)
        {
            TC_LOG_ERROR("sql.sql", "zone_event_spawn EventId {} has invalid Kind {}, skipped", spawn.EventId, uint32(spawn.Kind));
            continue;
        }

        _spawns.push_back(spawn);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} Quel'Thalas zone-event spawns", _spawns.size());
}

// zone_event_scenario_step: Scenario 3021 waves for Stormarion. Feeds the assault
// meter on wave completion. Ships with the three DB2-confirmed Stormarion rows.
void ZoneEventMgr::LoadScenarioWaves()
{
    _wavesByCriteriaTree.clear();

    //                                                    0        1           2          3               4              5
    QueryResult result = WorldDatabase.Query("SELECT EventId, ScenarioId, WaveIndex, CriteriaTreeId, RewardQuestId, MeterStep FROM zone_event_scenario_step");
    if (!result)
        return; // table absent -> scenario waves simply do not feed the meter (safe)

    do
    {
        Field* fields = result->Fetch();

        ZoneEventWave wave;
        wave.EventId        = fields[0].GetUInt32();
        wave.ScenarioId     = fields[1].GetUInt32();
        wave.WaveIndex      = fields[2].GetUInt8();
        wave.CriteriaTreeId = fields[3].GetUInt32();
        wave.RewardQuestId  = fields[4].GetUInt32();
        wave.MeterStep      = fields[5].GetInt32();

        if (!_templates.contains(wave.EventId))
        {
            TC_LOG_ERROR("sql.sql", "zone_event_scenario_step references unknown EventId {}, skipped", wave.EventId);
            continue;
        }
        if (!wave.CriteriaTreeId)
        {
            TC_LOG_ERROR("sql.sql", "zone_event_scenario_step EventId {} wave {} has no CriteriaTreeId, skipped", wave.EventId, uint32(wave.WaveIndex));
            continue;
        }

        _wavesByCriteriaTree[wave.CriteriaTreeId] = wave;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} Quel'Thalas zone-event scenario waves", _wavesByCriteriaTree.size());
}

void ZoneEventMgr::Update(uint32 diff)
{
    // Throttle: 1 Hz is sufficient for the second-resolution countdown worldstate.
    _updateAccumulator += diff;
    if (_updateAccumulator < ZONE_EVENT_UPDATE_INTERVAL)
        return;
    _updateAccumulator = 0;

    if (_templates.empty())
        return;

    time_t const now = GameTime::GetGameTime();

    for (auto& [id, state] : _states)
    {
        auto itr = _templates.find(id);
        if (itr == _templates.end())
            continue;
        ZoneEventTemplate const& tmpl = itr->second;

        if (!state.Active)
        {
            if (now >= state.NextStart)
                ActivateEvent(state, tmpl, now);
        }
        else
        {
            if (now >= state.EndTime)
                DeactivateEvent(state, tmpl, now);
            else
                BroadcastTimers(tmpl, state);
        }
    }
}

void ZoneEventMgr::ActivateEvent(ZoneEventState& state, ZoneEventTemplate const& tmpl, time_t now)
{
    state.Active    = true;
    state.EndTime   = now + static_cast<time_t>(tmpl.DurationSeconds);
    state.Meter     = 0;
    state.WavesDone = 0;

    // Advance rotation phase (mirrors the 3-phase 5207/5208 cycle observed on the wire).
    state.Phase = static_cast<uint8>((state.Phase + 1) % 3);

    // Publish the "next event" timer + rotation phase to clients in the zone.
    // world_state rows for these ids carry the AreaIDs so WorldStateMgr scopes
    // the broadcast automatically; pass nullptr for the realm-wide case.
    BroadcastTimers(tmpl, state);
    if (tmpl.StateWorldStateId)
    {
        // For the assault meter (Stormarion, MeterCap>0) the StateWorldStateId is
        // the fill meter (WS 29616): open the window at 0. For the rotation events
        // (5207/5208) it carries the rotation phase index.
        int32 const openValue = tmpl.MeterCap ? 0 : state.Phase;
        WorldStateMgr::SetValue(tmpl.StateWorldStateId, openValue, false, nullptr);
    }

    TC_LOG_DEBUG("misc", "ZoneEventMgr: activated event %u (type %u) in zone %u until %ld",
        tmpl.Id, uint32(tmpl.Type), tmpl.ZoneId, static_cast<long>(state.EndTime));

    OnEventStart(tmpl, state);
}

void ZoneEventMgr::DeactivateEvent(ZoneEventState& state, ZoneEventTemplate const& tmpl, time_t now)
{
    state.Active    = false;
    state.NextStart = now + static_cast<time_t>(tmpl.PeriodSeconds);
    state.Meter     = 0;

    if (tmpl.CountdownWorldStateId)
        WorldStateMgr::SetValue(tmpl.CountdownWorldStateId, 0, false, nullptr);
    if (tmpl.StateWorldStateId && tmpl.MeterCap)
        WorldStateMgr::SetValue(tmpl.StateWorldStateId, 0, false, nullptr); // meter reset (WS 29616 -> 0)

    BroadcastTimers(tmpl, state);

    TC_LOG_DEBUG("misc", "ZoneEventMgr: deactivated event %u (type %u); next start %ld",
        tmpl.Id, uint32(tmpl.Type), static_cast<long>(state.NextStart));

    OnEventComplete(tmpl, state);
}

void ZoneEventMgr::BroadcastTimers(ZoneEventTemplate const& tmpl, ZoneEventState const& state)
{
    // WS 22984 carries the next-event unix timestamp (confirmed on wire).
    if (tmpl.TimerWorldStateId)
        WorldStateMgr::SetValue(tmpl.TimerWorldStateId, static_cast<int32>(state.NextStart), false, nullptr);

    // WS 28763 carries the 1 Hz countdown while the event is active.
    if (tmpl.CountdownWorldStateId && state.Active)
    {
        time_t const now = GameTime::GetGameTime();
        int32 const remaining = state.EndTime > now ? static_cast<int32>(state.EndTime - now) : 0;
        WorldStateMgr::SetValue(tmpl.CountdownWorldStateId, remaining, false, nullptr);
    }
}

ZoneEventTemplate const* ZoneEventMgr::GetTemplate(uint32 id) const
{
    auto itr = _templates.find(id);
    return itr != _templates.end() ? &itr->second : nullptr;
}

bool ZoneEventMgr::IsEventActive(uint32 id) const
{
    auto itr = _states.find(id);
    return itr != _states.end() && itr->second.Active;
}

int32 ZoneEventMgr::GetMeter(uint32 id) const
{
    auto itr = _states.find(id);
    return itr != _states.end() ? itr->second.Meter : 0;
}

// ---------------------------------------------------------------------------
// Assault-meter mechanism (WS 29616). LIVE. The +500 step and the 1,000,000 cap
// are confirmed on the wire; the completion FLIP semantics (what exactly happens
// at cap besides the meter reset) are CAPTURE-BLOCKED, so at cap we run the
// documented default: fire the reward hook and end the window (reschedule).
// ---------------------------------------------------------------------------
void ZoneEventMgr::AddAssaultProgress(uint32 templateId, int32 amount)
{
    auto stateItr = _states.find(templateId);
    if (stateItr == _states.end())
        return;
    ZoneEventState& state = stateItr->second;

    auto tmplItr = _templates.find(templateId);
    if (tmplItr == _templates.end())
        return;
    ZoneEventTemplate const& tmpl = tmplItr->second;

    if (!state.Active || tmpl.MeterCap <= 0 || !tmpl.StateWorldStateId || amount <= 0)
        return;

    state.Meter = std::min<int32>(state.Meter + amount, tmpl.MeterCap);
    WorldStateMgr::SetValue(tmpl.StateWorldStateId, state.Meter, false, nullptr);

    TC_LOG_DEBUG("misc", "ZoneEventMgr: event {} assault meter -> {} / {}", tmpl.Id, state.Meter, tmpl.MeterCap);

    if (state.Meter >= tmpl.MeterCap)
        OnAssaultComplete(tmpl, state, GameTime::GetGameTime());
}

void ZoneEventMgr::OnScenarioCriteriaCompleted(uint32 criteriaTreeId)
{
    if (_wavesByCriteriaTree.empty())
        return;

    auto itr = _wavesByCriteriaTree.find(criteriaTreeId);
    if (itr == _wavesByCriteriaTree.end())
        return; // not one of our event waves -> no-op for all other scenarios

    ZoneEventWave const& wave = itr->second;

    auto stateItr = _states.find(wave.EventId);
    if (stateItr == _states.end() || !stateItr->second.Active)
        return; // wave completed outside its scheduled window -> ignore

    stateItr->second.WavesDone = std::max<uint8>(stateItr->second.WavesDone, wave.WaveIndex);

    TC_LOG_DEBUG("misc", "ZoneEventMgr: event {} scenario {} wave {} (tree {}) complete; +{} meter",
        wave.EventId, wave.ScenarioId, uint32(wave.WaveIndex), wave.CriteriaTreeId, wave.MeterStep);

    // Reward quest (91464/91465/90943) is granted by the stock scenario system
    // (Scenario::CompleteStep). Here we only feed the zone assault meter.
    AddAssaultProgress(wave.EventId, wave.MeterStep);
}

void ZoneEventMgr::OnAssaultComplete(ZoneEventTemplate const& tmpl, ZoneEventState& state, time_t now)
{
    TC_LOG_DEBUG("misc", "ZoneEventMgr: event {} assault meter reached cap ({}); running completion",
        tmpl.Id, tmpl.MeterCap);

    // TODO(CAPTURE-BLOCKED): grant the completion reward tail (renown/currency ids
    // unknown -- SCENARIO_STATE was n=0 in every sniff on hand). The mechanism is
    // wired: OnEventComplete() is the single reward-grant seam. Left as a no-op so
    // the completion path is realm-safe until the reward packet is captured.

    // End the active window immediately on cap and reschedule the next one. This
    // resets the meter (WS 29616 -> 0) and the countdown via DeactivateEvent.
    DeactivateEvent(state, tmpl, now);
}

// ---------------------------------------------------------------------------
// Per-event content hooks.
//
// The spawn/despawn MECHANISM is live: it reads `zone_event_spawn` and summons /
// despawns the event actor (256697) + destructibles (241418/241419). The spawn
// COORDINATES are CAPTURE-BLOCKED, so the table ships with no rows and the whole
// thing is a safe no-op until the coordinates are captured. Do NOT hardcode
// coordinates or spell effects here without a sniff.
// ---------------------------------------------------------------------------
void ZoneEventMgr::OnEventStart(ZoneEventTemplate const& tmpl, ZoneEventState& state)
{
    SpawnEventActors(tmpl, state);
}

void ZoneEventMgr::OnEventComplete(ZoneEventTemplate const& tmpl, ZoneEventState& state)
{
    DespawnEventActors(tmpl, state);

    // TODO(CAPTURE-BLOCKED): grant renown/currency/loot on completion. Needs the
    // completion reward packet + currency ids from a full-run capture
    // (SCENARIO_STATE was n=0 in the sniffs on hand). Reward-grant seam is here.
}

// Summon every zone_event_spawn row for this event on its target map. With no
// rows shipped this loops zero times (safe). Instance maps that are not currently
// loaded are skipped cleanly (FindMap returns nullptr).
void ZoneEventMgr::SpawnEventActors(ZoneEventTemplate const& tmpl, ZoneEventState& state)
{
    if (_spawns.empty())
        return;

    Creature* lastSummoner = nullptr;
    for (ZoneEventSpawn const& spawn : _spawns)
    {
        if (spawn.EventId != tmpl.Id)
            continue;

        uint32 const mapId = spawn.MapId ? spawn.MapId : tmpl.MapId;
        Map* map = sMapMgr->FindMap(mapId, 0);
        if (!map)
        {
            // Continent not loaded / instance not spun up yet -> skip (spawns are
            // re-attempted next activation; on a live realm the map is resident).
            TC_LOG_DEBUG("misc", "ZoneEventMgr: event {} spawn entry {} skipped, map {} not loaded",
                tmpl.Id, spawn.Entry, mapId);
            continue;
        }

        Position const pos(spawn.PosX, spawn.PosY, spawn.PosZ, spawn.Orientation);

        if (spawn.Kind == 0) // creature (event actor 256697, destructibles-as-creatures)
        {
            if (TempSummon* summon = map->SummonCreature(spawn.Entry, pos))
            {
                state.SpawnedCreatures.push_back(summon->GetGUID());
                lastSummoner = summon->ToCreature();
            }
        }
        else // Kind == 1: gameobject (destructibles-as-GOs). Needs a WorldObject
        {    // summoner; reuse a creature summoned above for this event.
            if (!lastSummoner)
            {
                TC_LOG_DEBUG("misc", "ZoneEventMgr: event {} gameobject {} skipped, no summoner creature spawned first",
                    tmpl.Id, spawn.Entry);
                continue;
            }
            if (GameObject* go = lastSummoner->SummonGameObject(spawn.Entry, pos, QuaternionData::fromEulerAnglesZYX(spawn.Orientation, 0.0f, 0.0f), 0s))
                state.SpawnedGameObjects.push_back(go->GetGUID());
        }
    }

    if (!state.SpawnedCreatures.empty() || !state.SpawnedGameObjects.empty())
        TC_LOG_DEBUG("misc", "ZoneEventMgr: event {} spawned {} creatures, {} gameobjects",
            tmpl.Id, state.SpawnedCreatures.size(), state.SpawnedGameObjects.size());
}

void ZoneEventMgr::DespawnEventActors(ZoneEventTemplate const& tmpl, ZoneEventState& state)
{
    uint32 const mapId = tmpl.MapId;
    if (Map* map = sMapMgr->FindMap(mapId, 0))
    {
        for (ObjectGuid const& guid : state.SpawnedCreatures)
            if (Creature* creature = map->GetCreature(guid))
                creature->DespawnOrUnsummon();

        for (ObjectGuid const& guid : state.SpawnedGameObjects)
            if (GameObject* go = map->GetGameObject(guid))
                go->Delete();
    }

    state.SpawnedCreatures.clear();
    state.SpawnedGameObjects.clear();
}

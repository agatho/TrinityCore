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

#include "VoidAssaultMgr.h"
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

VoidAssaultMgr::VoidAssaultMgr() : _updateAccumulator(0) { }
VoidAssaultMgr::~VoidAssaultMgr() = default;

VoidAssaultMgr* VoidAssaultMgr::instance()
{
    static VoidAssaultMgr instance;
    return &instance;
}

void VoidAssaultMgr::LoadFromDB()
{
    uint32 const oldMSTime = getMSTime();

    _templates.clear();
    _states.clear();

    //                                                   0   1     2       3      4                  5                  6                      7              8                9         10                   11               12               13
    QueryResult result = WorldDatabase.Query("SELECT Id, Type, ZoneId, MapId, StateWorldStateId, TimerWorldStateId, CountdownWorldStateId, PeriodSeconds, DurationSeconds, MeterCap, StrikesPerIncursion, PortalWorldMapA, PortalWorldMapB, HeroicContentTuningId FROM void_assault_template");
    if (!result)
    {
        // Table is intentionally NOT applied to the shared realm; a missing table
        // is a valid no-op for the skeleton. The SQL ships on this branch.
        TC_LOG_INFO("server.loading", ">> Loaded 0 Void assaults (table void_assault_template absent or empty)");
        return;
    }

    time_t const now = GameTime::GetGameTime();
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        VoidAssaultTemplate tmpl;
        tmpl.Id                    = fields[0].GetUInt32();
        tmpl.Type                  = static_cast<VoidAssaultType>(fields[1].GetUInt8());
        tmpl.ZoneId                = fields[2].GetUInt32();
        tmpl.MapId                 = fields[3].GetUInt32();
        tmpl.StateWorldStateId     = fields[4].GetInt32();
        tmpl.TimerWorldStateId     = fields[5].GetInt32();
        tmpl.CountdownWorldStateId = fields[6].GetInt32();
        tmpl.PeriodSeconds         = fields[7].GetUInt32();
        tmpl.DurationSeconds       = fields[8].GetUInt32();
        tmpl.MeterCap              = fields[9].GetInt32();
        tmpl.StrikesPerIncursion   = fields[10].GetUInt32();
        tmpl.PortalWorldMapA       = fields[11].GetUInt32();
        tmpl.PortalWorldMapB       = fields[12].GetUInt32();
        tmpl.HeroicContentTuningId = fields[13].GetUInt32();

        if (tmpl.Type >= VoidAssaultType::Max)
        {
            TC_LOG_ERROR("sql.sql", "void_assault_template Id {} has invalid Type {}, skipped", tmpl.Id, uint32(fields[1].GetUInt8()));
            continue;
        }
        if (tmpl.PeriodSeconds == 0)
        {
            TC_LOG_ERROR("sql.sql", "void_assault_template Id {} has PeriodSeconds 0, skipped", tmpl.Id);
            continue;
        }

        _templates[tmpl.Id] = tmpl;

        VoidAssaultState& state = _states[tmpl.Id];
        state.TemplateId = tmpl.Id;
        // Schedule the first activation at the next period boundary from server start.
        state.NextStart  = now + static_cast<time_t>(tmpl.PeriodSeconds);
        state.Active     = false;

        ++count;
    }
    while (result->NextRow());

    LoadSpawns();

    TC_LOG_INFO("server.loading", ">> Loaded {} Void assaults in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// void_assault_spawn: event actor / world boss / destructibles. Ships with NO
// rows (spawn coordinates are CAPTURE-BLOCKED); a missing/empty table is a valid
// no-op.
void VoidAssaultMgr::LoadSpawns()
{
    _spawns.clear();

    //                                                    0          1     2      3      4     5     6     7
    QueryResult result = WorldDatabase.Query("SELECT AssaultId, Kind, Entry, MapId, PosX, PosY, PosZ, Orientation FROM void_assault_spawn");
    if (!result)
        return; // table absent or empty -> no spawns (safe)

    do
    {
        Field* fields = result->Fetch();

        VoidAssaultSpawn spawn;
        spawn.AssaultId   = fields[0].GetUInt32();
        spawn.Kind        = fields[1].GetUInt8();
        spawn.Entry       = fields[2].GetUInt32();
        spawn.MapId       = fields[3].GetUInt32();
        spawn.PosX        = fields[4].GetFloat();
        spawn.PosY        = fields[5].GetFloat();
        spawn.PosZ        = fields[6].GetFloat();
        spawn.Orientation = fields[7].GetFloat();

        if (!_templates.contains(spawn.AssaultId))
        {
            TC_LOG_ERROR("sql.sql", "void_assault_spawn references unknown AssaultId {}, skipped", spawn.AssaultId);
            continue;
        }
        if (spawn.Kind > 1)
        {
            TC_LOG_ERROR("sql.sql", "void_assault_spawn AssaultId {} has invalid Kind {}, skipped", spawn.AssaultId, uint32(spawn.Kind));
            continue;
        }

        _spawns.push_back(spawn);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} Void assault spawns", _spawns.size());
}

void VoidAssaultMgr::Update(uint32 diff)
{
    // Throttle: 1 Hz is sufficient for the second-resolution countdown worldstate.
    _updateAccumulator += diff;
    if (_updateAccumulator < VOID_ASSAULT_UPDATE_INTERVAL)
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
        VoidAssaultTemplate const& tmpl = itr->second;

        if (!state.Active)
        {
            if (now >= state.NextStart)
                ActivateAssault(state, tmpl, now);
        }
        else
        {
            if (now >= state.EndTime)
                DeactivateAssault(state, tmpl, now);
            else
                BroadcastTimers(tmpl, state);
        }
    }
}

void VoidAssaultMgr::ActivateAssault(VoidAssaultState& state, VoidAssaultTemplate const& tmpl, time_t now)
{
    state.Active      = true;
    state.EndTime     = now + static_cast<time_t>(tmpl.DurationSeconds);
    state.Meter       = 0;
    state.StrikesDone = 0;

    // Advance rotation phase (mirrors the 3-phase 5207/5208 cycle observed on the wire).
    state.Phase = static_cast<uint8>((state.Phase + 1) % 3);

    // Pick the active portal world for a 12.0.7 escalation window.
    if (tmpl.Type == VoidAssaultType::VoidEscalation)
        RotatePortalWorld(state, tmpl);

    // Publish the "next event" clock + rotation phase / open the meter.
    BroadcastTimers(tmpl, state);
    if (tmpl.StateWorldStateId)
    {
        // MeterCap>0 -> StateWorldStateId is the fill meter (WS 29616): open at 0.
        // Otherwise it carries the rotation phase index (WS 5207/5208).
        int32 const openValue = tmpl.MeterCap ? 0 : state.Phase;
        WorldStateMgr::SetValue(tmpl.StateWorldStateId, openValue, false, nullptr);
    }

    TC_LOG_DEBUG("misc", "VoidAssaultMgr: activated assault {} (type {}) in zone {} until {}",
        tmpl.Id, uint32(tmpl.Type), tmpl.ZoneId, static_cast<long long>(state.EndTime));

    OnAssaultStart(tmpl, state);
}

void VoidAssaultMgr::DeactivateAssault(VoidAssaultState& state, VoidAssaultTemplate const& tmpl, time_t now)
{
    state.Active      = false;
    state.NextStart   = now + static_cast<time_t>(tmpl.PeriodSeconds);
    state.Meter       = 0;
    state.StrikesDone = 0;

    if (tmpl.CountdownWorldStateId)
        WorldStateMgr::SetValue(tmpl.CountdownWorldStateId, 0, false, nullptr);
    if (tmpl.StateWorldStateId && tmpl.MeterCap)
        WorldStateMgr::SetValue(tmpl.StateWorldStateId, 0, false, nullptr); // meter reset (WS 29616 -> 0)

    BroadcastTimers(tmpl, state);

    TC_LOG_DEBUG("misc", "VoidAssaultMgr: deactivated assault {} (type {}); next start {}",
        tmpl.Id, uint32(tmpl.Type), static_cast<long long>(state.NextStart));

    OnWindowExpired(tmpl, state);
}

void VoidAssaultMgr::BroadcastTimers(VoidAssaultTemplate const& tmpl, VoidAssaultState const& state)
{
    // WS 22984 carries the next-event unix timestamp (confirmed on wire).
    if (tmpl.TimerWorldStateId)
        WorldStateMgr::SetValue(tmpl.TimerWorldStateId, static_cast<int32>(state.NextStart), false, nullptr);

    // WS 28763 carries the 1 Hz countdown while the window is active.
    if (tmpl.CountdownWorldStateId && state.Active)
    {
        time_t const now = GameTime::GetGameTime();
        int32 const remaining = state.EndTime > now ? static_cast<int32>(state.EndTime - now) : 0;
        WorldStateMgr::SetValue(tmpl.CountdownWorldStateId, remaining, false, nullptr);
    }
}

// Portal-world rotation for a 12.0.7 Void Escalation window. Alternates the two
// configured worlds by rotation phase. RESEARCH: retail uses a weekly-reset
// odd/even cadence between Naigtal and Val -- the exact schedule is not
// DB2-confirmed, so the skeleton uses a simple phase-parity swap that a later
// pass can replace with the real calendar.
void VoidAssaultMgr::RotatePortalWorld(VoidAssaultState& state, VoidAssaultTemplate const& tmpl)
{
    if (!tmpl.PortalWorldMapA && !tmpl.PortalWorldMapB)
    {
        state.ActiveWorld = VoidPortalWorld::None;
        return;
    }

    state.ActiveWorld = (state.Phase % 2 == 0) ? VoidPortalWorld::Naigtal : VoidPortalWorld::Val;

    // Heroic World Tier: CAPTURE-BLOCKED. The retail toggle is a per-player
    // Normal/Heroic choice at the portal that re-scales the world via a
    // ContentTuning row and is gated behind achievement 63323 "Heroic
    // Tendencies". The server-side flag lives here; applying HeroicContentTuningId
    // to the phased instance is deferred until the toggle wire + tuning row are
    // captured. Skeleton keeps it off.
    state.HeroicTier = false;

    TC_LOG_DEBUG("misc", "VoidAssaultMgr: escalation {} portal world -> {} (heroic {})",
        tmpl.Id, uint32(state.ActiveWorld), state.HeroicTier);
}

VoidAssaultTemplate const* VoidAssaultMgr::GetTemplate(uint32 id) const
{
    auto itr = _templates.find(id);
    return itr != _templates.end() ? &itr->second : nullptr;
}

bool VoidAssaultMgr::IsAssaultActive(uint32 id) const
{
    auto itr = _states.find(id);
    return itr != _states.end() && itr->second.Active;
}

int32 VoidAssaultMgr::GetMeter(uint32 id) const
{
    auto itr = _states.find(id);
    return itr != _states.end() ? itr->second.Meter : 0;
}

VoidPortalWorld VoidAssaultMgr::GetActiveWorld(uint32 id) const
{
    auto itr = _states.find(id);
    return itr != _states.end() ? itr->second.ActiveWorld : VoidPortalWorld::None;
}

bool VoidAssaultMgr::IsHeroicWorldTier(uint32 id) const
{
    auto itr = _states.find(id);
    return itr != _states.end() && itr->second.HeroicTier;
}

// ---------------------------------------------------------------------------
// Escalation-meter mechanism (WS 29616). LIVE. The +500 step and the 1,000,000
// cap are confirmed on the wire; the completion FLIP semantics at cap (what runs
// besides the meter reset) are CAPTURE-BLOCKED, so at cap we run the documented
// default: fire the reward hook and end the window (reschedule).
// ---------------------------------------------------------------------------
void VoidAssaultMgr::AddAssaultProgress(uint32 templateId, int32 amount)
{
    auto stateItr = _states.find(templateId);
    if (stateItr == _states.end())
        return;
    VoidAssaultState& state = stateItr->second;

    auto tmplItr = _templates.find(templateId);
    if (tmplItr == _templates.end())
        return;
    VoidAssaultTemplate const& tmpl = tmplItr->second;

    if (!state.Active || tmpl.MeterCap <= 0 || !tmpl.StateWorldStateId || amount <= 0)
        return;

    state.Meter = std::min<int32>(state.Meter + amount, tmpl.MeterCap);
    WorldStateMgr::SetValue(tmpl.StateWorldStateId, state.Meter, false, nullptr);

    TC_LOG_DEBUG("misc", "VoidAssaultMgr: assault {} meter -> {} / {}", tmpl.Id, state.Meter, tmpl.MeterCap);

    if (state.Meter >= tmpl.MeterCap)
        OnAssaultComplete(tmpl, state, GameTime::GetGameTime());
}

void VoidAssaultMgr::OnVoidStrikeCompleted(uint32 templateId)
{
    auto stateItr = _states.find(templateId);
    if (stateItr == _states.end() || !stateItr->second.Active)
        return;
    VoidAssaultState& state = stateItr->second;

    auto tmplItr = _templates.find(templateId);
    if (tmplItr == _templates.end())
        return;
    VoidAssaultTemplate const& tmpl = tmplItr->second;

    // Feed the fill-meter directly for every strike (drives the 29616 UI bar).
    AddAssaultProgress(templateId, tmpl.MeterCap ? VoidAssault::METER_STEP : 0);

    if (tmpl.StrikesPerIncursion == 0)
        return; // this window is not strike-driven

    ++state.StrikesDone;
    TC_LOG_DEBUG("misc", "VoidAssaultMgr: assault {} strikes {}/{}", tmpl.Id, state.StrikesDone, tmpl.StrikesPerIncursion);

    // Enough Strikes -> flip the meter to cap to trigger the Incursion completion.
    if (state.StrikesDone >= tmpl.StrikesPerIncursion && tmpl.MeterCap > 0)
        AddAssaultProgress(templateId, tmpl.MeterCap);
}

void VoidAssaultMgr::OnAssaultComplete(VoidAssaultTemplate const& tmpl, VoidAssaultState& state, time_t now)
{
    TC_LOG_DEBUG("misc", "VoidAssaultMgr: assault {} meter reached cap ({}); running Incursion completion",
        tmpl.Id, tmpl.MeterCap);

    // TODO(CAPTURE-BLOCKED): grant the Incursion completion reward tail. The
    // economy ids are DB2-confirmed (Field Accolade 3405, Voidlight Marl 3316)
    // but the per-completion AMOUNTS + Great-Vault "World Content" credit packet
    // are not captured (SCENARIO_STATE was n=0 in every sniff on hand). The
    // reward-grant seam is OnWindowExpired(); left as a no-op so the completion
    // path is realm-safe until the reward packet is captured.

    // End the active window immediately on cap and reschedule the next one. This
    // resets the meter (WS 29616 -> 0) and the countdown via DeactivateAssault.
    DeactivateAssault(state, tmpl, now);
}

// ---------------------------------------------------------------------------
// Per-window content hooks.
//
// The spawn/despawn MECHANISM is live: it reads `void_assault_spawn` and summons
// / despawns the event actor, world boss (Nexus-Captain Leth'ir 260875 /
// Imperator Pertinax 261072) and destructibles. Spawn COORDINATES are
// CAPTURE-BLOCKED, so the table ships with no rows and the whole thing is a safe
// no-op until captured. Do NOT hardcode coordinates or spell effects here
// without a sniff.
// ---------------------------------------------------------------------------
void VoidAssaultMgr::OnAssaultStart(VoidAssaultTemplate const& tmpl, VoidAssaultState& state)
{
    SpawnAssaultActors(tmpl, state);
}

void VoidAssaultMgr::OnWindowExpired(VoidAssaultTemplate const& tmpl, VoidAssaultState& state)
{
    DespawnAssaultActors(tmpl, state);

    // TODO(CAPTURE-BLOCKED): grant renown/currency/loot on completion. Needs the
    // completion reward packet + amounts from a full-run capture (SCENARIO_STATE
    // was n=0 in the sniffs on hand). Reward-grant seam is here.
}

// Summon every void_assault_spawn row for this window on its target map. With no
// rows shipped this loops zero times (safe). Maps that are not currently loaded
// are skipped cleanly (FindMap returns nullptr).
void VoidAssaultMgr::SpawnAssaultActors(VoidAssaultTemplate const& tmpl, VoidAssaultState& state)
{
    if (_spawns.empty())
        return;

    Creature* lastSummoner = nullptr;
    for (VoidAssaultSpawn const& spawn : _spawns)
    {
        if (spawn.AssaultId != tmpl.Id)
            continue;

        uint32 const mapId = spawn.MapId ? spawn.MapId : tmpl.MapId;
        Map* map = sMapMgr->FindMap(mapId, 0);
        if (!map)
        {
            TC_LOG_DEBUG("misc", "VoidAssaultMgr: assault {} spawn entry {} skipped, map {} not loaded",
                tmpl.Id, spawn.Entry, mapId);
            continue;
        }

        Position const pos(spawn.PosX, spawn.PosY, spawn.PosZ, spawn.Orientation);

        if (spawn.Kind == 0) // creature (event actor, world boss, destructibles-as-creatures)
        {
            if (TempSummon* summon = map->SummonCreature(spawn.Entry, pos))
            {
                state.SpawnedCreatures.push_back(summon->GetGUID());
                lastSummoner = summon->ToCreature();
            }
        }
        else // Kind == 1: gameobject. Needs a WorldObject summoner; reuse a
        {    // creature summoned above for this window.
            if (!lastSummoner)
            {
                TC_LOG_DEBUG("misc", "VoidAssaultMgr: assault {} gameobject {} skipped, no summoner creature spawned first",
                    tmpl.Id, spawn.Entry);
                continue;
            }
            if (GameObject* go = lastSummoner->SummonGameObject(spawn.Entry, pos, QuaternionData::fromEulerAnglesZYX(spawn.Orientation, 0.0f, 0.0f), 0s))
                state.SpawnedGameObjects.push_back(go->GetGUID());
        }
    }

    if (!state.SpawnedCreatures.empty() || !state.SpawnedGameObjects.empty())
        TC_LOG_DEBUG("misc", "VoidAssaultMgr: assault {} spawned {} creatures, {} gameobjects",
            tmpl.Id, state.SpawnedCreatures.size(), state.SpawnedGameObjects.size());
}

void VoidAssaultMgr::DespawnAssaultActors(VoidAssaultTemplate const& tmpl, VoidAssaultState& state)
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

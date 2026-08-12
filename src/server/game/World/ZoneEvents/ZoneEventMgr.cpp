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
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include "WorldStateMgr.h"

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

    TC_LOG_INFO("server.loading", ">> Loaded %u Quel'Thalas zone events in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
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
    state.Active  = true;
    state.EndTime = now + static_cast<time_t>(tmpl.DurationSeconds);
    state.Meter   = 0;

    // Advance rotation phase (mirrors the 3-phase 5207/5208 cycle observed on the wire).
    state.Phase = static_cast<uint8>((state.Phase + 1) % 3);

    // Publish the "next event" timer + rotation phase to clients in the zone.
    // world_state rows for these ids carry the AreaIDs so WorldStateMgr scopes
    // the broadcast automatically; pass nullptr for the realm-wide case.
    BroadcastTimers(tmpl, state);
    if (tmpl.StateWorldStateId)
        WorldStateMgr::SetValue(tmpl.StateWorldStateId, state.Phase, false, nullptr);

    TC_LOG_DEBUG("misc", "ZoneEventMgr: activated event %u (type %u) in zone %u until %ld",
        tmpl.Id, uint32(tmpl.Type), tmpl.ZoneId, static_cast<long>(state.EndTime));

    OnEventStart(tmpl);
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

    OnEventComplete(tmpl);
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

// ---------------------------------------------------------------------------
// CAPTURE-BLOCKED per-event content hooks.
//
// These require evidence not yet in hand (see the "needed from testers" list in
// C:\dumps\QUELTHALAS_EVENTS_BLUEPRINT.md). They are intentionally no-ops so the
// framework compiles and schedules cleanly; the rotation/worldstate spine above
// is fully live. Do NOT invent spawn tables / spell effects here without a sniff.
// ---------------------------------------------------------------------------
void ZoneEventMgr::OnEventStart(ZoneEventTemplate const& /*tmpl*/)
{
    // TODO(CAPTURE-BLOCKED): spawn event actor 256697 (+ vignette 7698, cast 1253107),
    // Stormarion destructibles Conjured Defense 241418 / Ward Fragment 241419,
    // and per-type creatures/GOs. Needs AreaTrigger/SceneObject + spawn-point capture.
}

void ZoneEventMgr::OnEventComplete(ZoneEventTemplate const& /*tmpl*/)
{
    // TODO(CAPTURE-BLOCKED): grant renown/currency/loot on completion and despawn
    // event objects. Needs the completion reward packet + currency ids from a
    // full-run capture (SCENARIO_STATE was n=0 in the sniffs on hand).
}

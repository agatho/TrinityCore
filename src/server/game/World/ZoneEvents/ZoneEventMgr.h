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

#ifndef TRINITYCORE_ZONE_EVENT_MGR_H
#define TRINITYCORE_ZONE_EVENT_MGR_H

// Quel'Thalas (Midnight, Season 1) endgame zone-event scheduler.
//
// Drives the four Quel'Thalas zone events off server-side rotation timers and
// broadcasts the client-recognised worldstates the retail UI reacts to
// (assault meter 29616, next-event unix stamps 22984, 3-phase rotation
// 5207/5208, countdown 28763). Modelled directly on WorldQuestMgr / AreaPoiMgr
// (this fork's house pattern for timed world content): a lazy-static singleton
// with LoadFromDB() + Update(diff), wired into World::SetInitialWorldSettings /
// World::Update.
//
// SCOPE: framework skeleton only. Event-specific spawning / completion / reward
// logic is stubbed and marked CAPTURE-BLOCKED where wire/DB2 evidence is not yet
// in hand. See C:\dumps\QUELTHALAS_EVENTS_BLUEPRINT.md.

#include "Define.h"
#include <ctime>
#include <unordered_map>
#include <vector>

// Broad classification of a Quel'Thalas zone event. The buildability order in
// the blueprint follows the evidence strength: Stormarion (meter+scenario),
// Abundance (DB2 POIs+scenarios), Saltheril (quest+POI), Haranir (quest;
// scenario capture-blocked).
enum class ZoneEventType : uint8
{
    AbundanceCaves    = 0, // 8h rotation of harvest caves (Skinning/Enchanting/Mining/Herbalism)
    SaltherilSoiree   = 1, // weekly faction-favour soiree (quest 89289 "Favor of the Court")
    StormarionAssault = 2, // 30-min tower defence, assault meter WS 29616, Scenario 3021
    LegendsOfHaranir  = 3, // weekly warband scenario (quest 93932 "Legendary Prosperity")

    Max
};

// One configured zone event. Populated from world table `zone_event_template`
// (ships on this branch, NOT applied to any shared realm — see blueprint).
struct ZoneEventTemplate
{
    uint32       Id                    = 0;
    ZoneEventType Type                 = ZoneEventType::Max;
    uint32       ZoneId                = 0; // 15968 (Eversong) / 15969 (Silvermoon), Quel'Thalas
    uint32       MapId                 = 0; // 0 Eastern Kingdoms, 2694 Har'alnor, 2771 Stormarion Keep

    // Client-recognised worldstates this event broadcasts (0 = unused).
    int32        StateWorldStateId     = 0; // rotation / assault-meter driver (5207, 5208, 29616)
    int32        TimerWorldStateId     = 0; // next-event unix timestamp (22984)
    int32        CountdownWorldStateId = 0; // 1 Hz countdown while active (28763)

    // Rotation cadence.
    uint32       PeriodSeconds         = 0; // 28800 (8h) / 604800 (weekly) / 1800 (30m)
    uint32       DurationSeconds       = 0; // how long the event stays "active" once triggered
    int32        MeterCap              = 0; // assault fill cap (1000000 for Stormarion), 0 = none
};

// Live runtime state for one active/scheduled event instance.
struct ZoneEventState
{
    uint32 TemplateId = 0;
    time_t NextStart  = 0; // unix time of next activation (mirrors WS 22984)
    time_t EndTime    = 0; // unix time this active window closes
    int32  Meter      = 0; // current assault-meter value (mirrors WS 29616)
    uint8  Phase      = 0; // rotation phase index (mirrors WS 5207/5208)
    bool   Active     = false;
};

class TC_GAME_API ZoneEventMgr
{
private:
    ZoneEventMgr();
    ~ZoneEventMgr();

public:
    ZoneEventMgr(ZoneEventMgr const&) = delete;
    ZoneEventMgr(ZoneEventMgr&&) = delete;
    ZoneEventMgr& operator=(ZoneEventMgr const&) = delete;
    ZoneEventMgr& operator=(ZoneEventMgr&&) = delete;

    static ZoneEventMgr* instance();

    // Called once from World::SetInitialWorldSettings (after WorldStateMgr::LoadFromDB).
    void LoadFromDB();

    // Called every World::Update tick; throttled internally to ZONE_EVENT_UPDATE_INTERVAL.
    void Update(uint32 diff);

    // --- accessors (used by scripts / debug commands) ---
    ZoneEventTemplate const* GetTemplate(uint32 id) const;
    bool IsEventActive(uint32 id) const;

private:
    // Rotation / scheduling core (implemented in skeleton).
    void ActivateEvent(ZoneEventState& state, ZoneEventTemplate const& tmpl, time_t now);
    void DeactivateEvent(ZoneEventState& state, ZoneEventTemplate const& tmpl, time_t now);
    void BroadcastTimers(ZoneEventTemplate const& tmpl, ZoneEventState const& state);

    // --- CAPTURE-BLOCKED per-event hooks (stubbed; see blueprint) ---
    // Spawn/despawn of event creatures/GOs + destructibles (Conjured Defense
    // 241418 / Ward Fragment 241419), and completion -> renown/currency/loot.
    void OnEventStart(ZoneEventTemplate const& tmpl);   // TODO CAPTURE-BLOCKED
    void OnEventComplete(ZoneEventTemplate const& tmpl); // TODO CAPTURE-BLOCKED

    static constexpr uint32 ZONE_EVENT_UPDATE_INTERVAL = 1000; // ms; 1 Hz is enough for the countdown WS

    uint32 _updateAccumulator;
    std::unordered_map<uint32, ZoneEventTemplate> _templates;
    std::unordered_map<uint32, ZoneEventState>    _states;
};

#define sZoneEventMgr ZoneEventMgr::instance()

#endif // TRINITYCORE_ZONE_EVENT_MGR_H

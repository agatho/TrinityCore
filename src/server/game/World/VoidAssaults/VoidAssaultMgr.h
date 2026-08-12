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

#ifndef TRINITYCORE_VOID_ASSAULT_MGR_H
#define TRINITYCORE_VOID_ASSAULT_MGR_H

// Void Assaults (Midnight 12.0.5) + Void Escalations (12.0.7) open-world
// invasion scheduler.
//
// -------------------------------------------------------------------------
// ARCHITECTURE / REUSE DECISION (see C:\dumps\VOID_ASSAULTS_BLUEPRINT.md §0)
// -------------------------------------------------------------------------
// Void Assaults are the SAME machinery as feature/quelthalas-zone-events'
// ZoneEventMgr: a zone-wide fill-meter (WS 29616-family), a rotation phase
// (WS 5207/5208-family), a next-event unix clock (WS 22984-family), a live
// countdown (WS 28763-family) and a table-driven spawn mechanism. The design
// decision is therefore to EXTEND ZoneEventMgr, NOT to build a parallel
// scheduler: at integration Void Assaults become new ZoneEventType values plus
// two additive mechanisms ZoneEventMgr does not yet have -- an escalation
// counter (Strikes -> Incursion) and a two-world portal rotation with a
// Heroic World Tier flag.
//
// BUT this branch is cut from golden-source baseline 560165c0a6, which does NOT
// contain ZoneEventMgr (that lives on feature/quelthalas-zone-events). Per the
// project rule we do NOT cherry-pick ZoneEventMgr here. Instead this branch
// ships a self-contained VoidAssaultMgr that MIRRORS the ZoneEventMgr idiom
// (lazy-static singleton, LoadFromDB() + Update(diff), tolerant loaders) so the
// branch compiles standalone and green. At the integration merge (after
// quelthalas-zone-events lands) VoidAssaultMgr is folded into ZoneEventMgr:
// the two escalation-specific structs below become ZoneEventMgr members and the
// ZoneEventType enum gains VoidStrike / VoidIncursion / VoidEscalation. See the
// blueprint merge-order section.
//
// SCOPE: framework skeleton only. Meter/rotation/portal spine is LIVE; per-world
// spawning, world-boss handoff, Heroic World Tier scaling and completion rewards
// are stubbed and marked CAPTURE-BLOCKED where wire/DB2 evidence is not in hand.

#include "Define.h"
#include "ObjectGuid.h"
#include <ctime>
#include <unordered_map>
#include <vector>

// Classification of a Void invasion, ordered by evidence strength / buildability.
enum class VoidAssaultType : uint8
{
    VoidStrike     = 0, // 12.0.5 small rotating world-objective that feeds the escalation meter
    VoidIncursion  = 1, // 12.0.5 climactic communal scenario triggered when the meter fills
    VoidEscalation = 2, // 12.0.7 portal-world window (Naigtal / Val) carrying the world boss + HWT

    Max
};

// Which 12.0.7 portal world is currently active. Escalations alternate ONE of
// these two per weekly reset (RESEARCH: cadence not DB2-confirmed).
enum class VoidPortalWorld : uint8
{
    None    = 0,
    Naigtal = 1, // fungal/arcane world, Hal'hadar ethereals, world boss Nexus-Captain Leth'ir
    Val     = 2, // frozen Legion waste, Domanaar under Imperator Pertinax
};

// Client-recognised worldstate ids. The 29616/22984/5207/5208/28763 family was
// captured LIVE in the Eversong/Silvermoon (area 15968/15969) sniffs -- the same
// zone Void Assaults 12.0.5 occupy -- so they are reused here as the assault
// meter / rotation / clock / countdown drivers. See blueprint §1 for the
// Stormarion-vs-VoidAssault attribution discussion (shared machinery).
namespace VoidAssault
{
    // Fill-meter for the active assault (Strikes -> Incursion). Cap 1,000,000,
    // +500 step, resets to 0 on completion. [SNIFF ws 29616]
    constexpr int32 WS_ASSAULT_METER      = 29616;
    // Next-event unix timestamp. [SNIFF ws 22984]
    constexpr int32 WS_NEXT_EVENT_CLOCK   = 22984;
    // 3-phase rotation pair. [SNIFF ws 5207 / 5208]
    constexpr int32 WS_ROTATION_A         = 5207;
    constexpr int32 WS_ROTATION_B         = 5208;
    // 1 Hz countdown while an assault window is active. [SNIFF ws 28763]
    constexpr int32 WS_COUNTDOWN          = 28763;

    // Assault meter step + cap (both confirmed on wire).
    constexpr int32 METER_STEP            = 500;
    constexpr int32 METER_CAP             = 1000000;

    // --- DB2-anchored economy (build 12.0.7.68887) ---
    constexpr uint32 CURRENCY_FIELD_ACCOLADE = 3405; // [DB2 CurrencyTypes] "Field Accolade" -- CONFIRMED, named in the
                                                     // Void Assaults POI reward text; Cat 264, Quality 4.
    constexpr uint32 CURRENCY_VOIDLIGHT_MARL = 3316; // [DB2 CurrencyTypes] "Voidlight Marl" -- CONFIRMED
                                                     // ("Created at the confluence of Light and Void..."), Cat/cosmetic sink.

    // --- Portal-world identity (authoritative DB2 anchors) ---
    constexpr uint32 MAP_NAIGTAL       = 3075;  // [DB2 Map] MapName "Naigtal", ExpansionID 11 (Midnight)
    constexpr uint32 MAP_VAL           = 3047;  // [DB2 Map] MapName "Val", ExpansionID 11
    constexpr uint32 AREA_NAIGTAL_ROOT = 16943; // [DB2 AreaTable] "Naigtal" (ShowdownNaigtal), ContinentID 3075
    constexpr uint32 AREA_VAL_ROOT     = 16900; // [DB2 AreaTable] "Val", ContinentID 3047
    constexpr uint32 CONTENT_TUNING_PORTAL_WORLDS = 3321; // [DB2 ContentTuning] shared Naigtal/Val tuning
                                                          // (HP curve 90579, DMG curve 90578, lvl 80-90).
                                                          // Heroic World Tier scaling row is CAPTURE-BLOCKED:
                                                          // no DB2 row is NAMED "Heroic World Tier" @68887.

    // --- Void Assault open-world markers (DB2) ---
    constexpr uint32 AREAPOI_VOID_ASSAULT_A = 8697; // [DB2 AreaPOI] "Void Assaults", PoiData 94385, cond 153246
    constexpr uint32 AREAPOI_VOID_ASSAULT_B = 8698; // [DB2 AreaPOI] "Void Assaults", PoiData 94386, cond 153247
    constexpr uint32 VIGNETTE_VOID_ASSAULT_A = 7174; // [DB2 Vignette] "Void Assault"
    constexpr uint32 VIGNETTE_VOID_ASSAULT_B = 7314; // [DB2 Vignette] "Void Assault"

    // --- World bosses (per portal world) -- RESEARCH only, NOT DB2-verified ---
    // wowhead lists these npc ids; NOT confirmed against Creature.db2 @68887.
    // Do NOT hardcode into spawns until verified -- ship via void_assault_spawn.
    constexpr uint32 NPC_NAIGTAL_WORLD_BOSS = 260875; // "Nexus-Captain Leth'ir" [RESEARCH -- verify]
    constexpr uint32 NPC_VAL_WORLD_BOSS     = 261072; // "Imperator Pertinax"    [RESEARCH -- verify]

    // --- Heroic World Tier unlock -- RESEARCH only, ZERO DB2 hits for the name ---
    constexpr uint32 ACHIEVEMENT_HWT_UNLOCK = 63323; // "Heroic Tendencies" [RESEARCH -- verify]
}

// One configured Void assault window. Populated from world table
// `void_assault_template` (ships on this branch, NOT applied to any shared
// realm). Mirrors ZoneEventTemplate + escalation/portal fields.
struct VoidAssaultTemplate
{
    uint32          Id                    = 0;
    VoidAssaultType Type                  = VoidAssaultType::Max;
    uint32          ZoneId                = 0; // 15968 Eversong / Zul'Aman (12.0.5); portal-world zone (12.0.7)
    uint32          MapId                 = 0; // continent / portal-world map

    // Client-recognised worldstates this window broadcasts (0 = unused).
    int32           StateWorldStateId     = 0; // assault fill-meter (29616) or rotation phase (5207/5208)
    int32           TimerWorldStateId     = 0; // next-event unix timestamp (22984)
    int32           CountdownWorldStateId = 0; // 1 Hz countdown while active (28763)

    // Rotation cadence.
    uint32          PeriodSeconds         = 0; // 604800 weekly assault swap, shorter for strike cadence
    uint32          DurationSeconds       = 0; // how long the window stays active once triggered
    int32           MeterCap              = 0; // escalation fill cap (1000000), 0 = none

    // Escalation counter: how many Void Strikes complete a Void Incursion. When
    // the strike counter reaches this, the incursion child window is triggered.
    // 0 = this window is not driven by a strike counter.
    uint32          StrikesPerIncursion   = 0;

    // Portal-world rotation (VoidEscalation only). The two worlds this window
    // alternates between and the ContentTuning row applied when Heroic World
    // Tier is toggled on. 0 = not a portal-world window.
    uint32          PortalWorldMapA       = 0; // Naigtal map
    uint32          PortalWorldMapB       = 0; // Val map
    uint32          HeroicContentTuningId = 0; // Heroic World Tier scaling row [DB2 ContentTuning -- CAPTURE-BLOCKED]
};

// One spawn row (`void_assault_spawn`, ships on this branch). Drives the event
// actor / world-boss / destructible spawn mechanism. Spawn COORDINATES are
// CAPTURE-BLOCKED, so the table ships with NO data rows; the mechanism iterates
// whatever rows exist and is a safe no-op when empty.
struct VoidAssaultSpawn
{
    uint32 AssaultId   = 0;  // FK -> void_assault_template.Id
    uint8  Kind        = 0;  // 0 = creature, 1 = gameobject
    uint32 Entry       = 0;  // creature_template / gameobject_template entry
    uint32 MapId       = 0;  // map to summon on (0 = use the assault's MapId)
    float  PosX        = 0.f;
    float  PosY        = 0.f;
    float  PosZ        = 0.f;
    float  Orientation = 0.f;
};

// Live runtime state for one active/scheduled assault window.
struct VoidAssaultState
{
    uint32          TemplateId  = 0;
    time_t          NextStart   = 0; // unix time of next activation (mirrors WS 22984)
    time_t          EndTime     = 0; // unix time this active window closes
    int32           Meter       = 0; // current assault-meter value (mirrors WS 29616)
    uint8           Phase       = 0; // rotation phase index (mirrors WS 5207/5208)
    uint32          StrikesDone = 0; // Void Strikes completed toward the current Incursion
    VoidPortalWorld ActiveWorld = VoidPortalWorld::None; // which portal world is live this window
    bool            HeroicTier  = false; // Heroic World Tier active for this window
    bool            Active      = false;

    // Objects summoned by the spawn mechanism this window (despawned on end).
    std::vector<ObjectGuid> SpawnedCreatures;
    std::vector<ObjectGuid> SpawnedGameObjects;
};

class TC_GAME_API VoidAssaultMgr
{
private:
    VoidAssaultMgr();
    ~VoidAssaultMgr();

public:
    VoidAssaultMgr(VoidAssaultMgr const&) = delete;
    VoidAssaultMgr(VoidAssaultMgr&&) = delete;
    VoidAssaultMgr& operator=(VoidAssaultMgr const&) = delete;
    VoidAssaultMgr& operator=(VoidAssaultMgr&&) = delete;

    static VoidAssaultMgr* instance();

    // Called once from World::SetInitialWorldSettings (after WorldStateMgr::LoadFromDB).
    void LoadFromDB();

    // Called every World::Update tick; throttled internally to VOID_ASSAULT_UPDATE_INTERVAL.
    void Update(uint32 diff);

    // --- accessors (used by scripts / debug commands) ---
    VoidAssaultTemplate const* GetTemplate(uint32 id) const;
    bool IsAssaultActive(uint32 id) const;
    int32 GetMeter(uint32 id) const;
    VoidPortalWorld GetActiveWorld(uint32 id) const;
    bool IsHeroicWorldTier(uint32 id) const;

    // --- escalation-meter mechanism (WS 29616) ---
    // Advance the assault meter of window `templateId` by `amount` (defaults to
    // the +500 step). Broadcasts WS 29616 and, on reaching MeterCap, drives the
    // Incursion completion path. Safe no-op if inactive / no meter cap.
    void AddAssaultProgress(uint32 templateId, int32 amount = VoidAssault::METER_STEP);

    // Void Strike completion hook. Increments the strike counter; when it reaches
    // StrikesPerIncursion the meter is flipped to cap (triggering the Incursion).
    // Called from the (future) Void Strike quest-turn-in script. No-op for
    // windows without a strike counter.
    void OnVoidStrikeCompleted(uint32 templateId);

    static constexpr int32 ASSAULT_METER_STEP = VoidAssault::METER_STEP;

private:
    // Rotation / scheduling core (implemented in skeleton).
    void ActivateAssault(VoidAssaultState& state, VoidAssaultTemplate const& tmpl, time_t now);
    void DeactivateAssault(VoidAssaultState& state, VoidAssaultTemplate const& tmpl, time_t now);
    void BroadcastTimers(VoidAssaultTemplate const& tmpl, VoidAssaultState const& state);

    // Portal-world rotation (VoidEscalation): pick Naigtal/Val for this window.
    void RotatePortalWorld(VoidAssaultState& state, VoidAssaultTemplate const& tmpl);

    // Per-window start/end content hooks. Spawn/despawn MECHANISM is live (reads
    // `void_assault_spawn`; no-op when empty). Reward tail is CAPTURE-BLOCKED.
    void OnAssaultStart(VoidAssaultTemplate const& tmpl, VoidAssaultState& state);
    void OnAssaultComplete(VoidAssaultTemplate const& tmpl, VoidAssaultState& state, time_t now);
    void OnWindowExpired(VoidAssaultTemplate const& tmpl, VoidAssaultState& state);

    // Spawn mechanism (reads `void_assault_spawn`).
    void SpawnAssaultActors(VoidAssaultTemplate const& tmpl, VoidAssaultState& state);
    void DespawnAssaultActors(VoidAssaultTemplate const& tmpl, VoidAssaultState& state);

    // Loader for the shipped spawn table.
    void LoadSpawns();

    static constexpr uint32 VOID_ASSAULT_UPDATE_INTERVAL = 1000; // ms; 1 Hz for the countdown WS

    uint32 _updateAccumulator;
    std::unordered_map<uint32, VoidAssaultTemplate> _templates;
    std::unordered_map<uint32, VoidAssaultState>    _states;
    std::vector<VoidAssaultSpawn>                   _spawns;
};

#define sVoidAssaultMgr VoidAssaultMgr::instance()

#endif // TRINITYCORE_VOID_ASSAULT_MGR_H

// ElevatorIndex — auto-detected catalog of every multi-floor elevator
// (GAMEOBJECT_TYPE_TRANSPORT, type 11) on every map.
//
// Built once at worldserver boot from authoritative data:
//
//   1. ObjectMgr::GetAllGameObjectData() — every spawned GO row from the
//      `gameobject` table.
//   2. GameObjectTemplate.transport.Timeto2ndfloor..Timeto9thfloor —
//      the time indices (ms) at which the platform pauses at each
//      stop above the spawn point.
//   3. sTransportMgr->GetTransportAnimInfo(entry)->Path — animation
//      frames mapping (timeIndex → local-space xyz position).
//
// For each spawned elevator we emit one ElevatorStop record per
// physical stop (start position + each Timeto*floor stop). World
// coordinates derived by rotating the local-space animation offset
// by the spawn's orientation and translating by the spawn position.
//
// Consumers (ElevatorRules.cpp idle:elevator_step_on / step_off) use
// the same APIs the WorldMetadataStore exposed for operator-placed
// annotations — but ElevatorIndex auto-detects everything without
// any operator effort.

#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

class Player;

namespace Playerbot::V2::Travel {

struct ElevatorStop
{
    uint32_t map_id    = 0;
    uint32_t entry     = 0;     // gameobject_template.entry
    uint64_t spawn_id  = 0;     // gameobject.guid (one per spawned platform)
    float    x         = 0.f;
    float    y         = 0.f;
    float    z         = 0.f;   // world-space at this stop
    uint8_t  stop_idx  = 0;     // 0 = base/spawn pos, 1..N = Timetonth-floor
};

class ElevatorIndex
{
public:
    static ElevatorIndex& Instance();

    // One-shot boot-time scan. Idempotent; subsequent calls clear and
    // rebuild the index (useful for future dynamic spawn additions).
    // Reads from ObjectMgr + sTransportMgr; safe to call after
    // ObjectMgr::LoadGameObjects() and TransportMgr::LoadTransport
    // AnimationAndRotation() complete.
    void LoadFromGameObjects();

    // Closest stop on the given map whose Z is within `z_range` of the
    // query Z AND horizontal distance ≤ `xy_range`. Returns nullptr
    // when nothing matches. Used by idle:elevator_step_on to find the
    // boarding spot for the bot's current floor.
    ElevatorStop const* NearestStopOnFloor(uint32_t map_id, float x, float y, float z,
                                            float xy_range, float z_range) const;

    // Closest stop on the given map regardless of Z. Used by
    // idle:elevator_step_off to find the destination floor's safe
    // off-platform spot after the platform has carried the bot
    // upward / downward.
    ElevatorStop const* NearestStopAnyZ(uint32_t map_id, float x, float y,
                                         float xy_range) const;

    // LOWEST-Z (boarding-floor) stop on the given map within `xy_range`
    // of (x,y), regardless of the query Z. Used by the cross-map dock
    // hand-off (State_Idle walk_to_known_dock → Fix B): a bot standing at
    // ground level beneath a deck-level zeppelin dock anchor needs to be
    // routed to the BASE of the nearest elevator shaft so it can ride up.
    // NearestStopOnFloor can't be used there because the bot's Z (ground)
    // is far below every indexed stop once the shaft's lowest floor sits
    // above the bot's footing; this resolves the shaft by X/Y proximity
    // and then returns its lowest floor.
    ElevatorStop const* LowestStopNear(uint32_t map_id, float x, float y,
                                       float xy_range) const;

    // Like LowestStopNear, but PREFERS a shaft whose boarding (lowest) stop has
    // a navmesh-derived ledge — i.e. a lift the bot can actually board from solid
    // footing. Falls back to the plain nearest shaft when none in range has a
    // derived ledge yet. Used by the cross-map dock hand-off so a bot bound for a
    // tower reachable by EITHER of two lifts is routed to the one it can board,
    // not merely the nearest one — which may drop it in an unboardable sub-ground
    // pit (Orgrimmar lift 206609's pit @z29.5 vs the boardable 206610). Both Org
    // zeppelin lifts reach the tower, so falling back to the boardable one is
    // always valid.
    ElevatorStop const* LowestStopNearBoardable(uint32_t map_id, float x, float y,
                                                float xy_range) const;

    // ---- Walkable board/disembark LEDGES (navmesh-derived) ----------------
    // Every indexed stop is the platform CENTRE — the point the platform's
    // origin occupies at that floor. But a bot can only stand/board/disembark
    // on the surrounding walkable FLOOR (the rim ledge), which sits ~9-11y to
    // the side of the centre because the moving platform footprint is carved
    // OUT of the static navmesh. The ledge is derived once, lazily, from the
    // live navmesh (nearest walkable poly to the centre) the first time a bot
    // is near the elevator — at which point the map tile is guaranteed loaded.
    //
    // EnsureLedges runs on the WORLD THREAD ONLY (it queries the live navmesh
    // via `p`). Call it from the snapshot builder when a type-11 elevator is
    // pushed into nearby_objects. Cheap after the first call per stop (the
    // result, including a "no ledge found, use centre" verdict, is cached).
    void EnsureLedges(uint64_t spawn_id, Player* p);

    // Derive+cache ledges for EVERY shaft within `range` of `p` (not just the one
    // the bot is standing at), so the boardable-lift preference
    // (LowestStopNearBoardable) can compare sibling lifts the moment a bot reaches
    // the cluster — instead of waiting for a bot to physically visit each lift
    // before its boarding floor is known. Without this, on a fresh boot the
    // nearest lift's ledge derives (often has_ledge=false for a pit lift) while
    // the boardable sibling 150y away stays unknown, so the preference can't fire.
    // World-thread only (queries the live navmesh via `p`). Cheap + cached.
    void EnsureLedgesNear(Player* p, float range);

    // Read the cached ledge for a stop. Returns true and fills (lx,ly,lz) with
    // the walkable ledge when one was derived; false when no ledge has been
    // derived yet OR the derivation found none (caller should fall back to the
    // platform centre). Thread-safe (taken under the index mutex), so the AI
    // worker may call it while reading a published snapshot's stop.
    bool LedgeFor(uint64_t spawn_id, uint8_t stop_idx,
                  float& lx, float& ly, float& lz) const;

    // Among the stops of the shaft NEAREST (x,y) on `map_id`, return the Z of
    // the stop closest to `target_z` (writes it to `out_z`). Returns false when
    // no shaft is within `xy_range`. Used by idle:elevator_step_on to refuse a
    // lift that can't carry the bot toward its target floor: a bot whose goal is
    // ABOVE the lift's highest stop (the Orgrimmar zeppelin DECK z135 vs the
    // rim-lift top z109) must NOT board, ride to the top, disembark, still want
    // vertical, and re-board in a loop — boarding is only worthwhile when the
    // lift has a stop materially closer to the target than the bot's current Z.
    bool BestStopZToward(uint32_t map_id, float x, float y, float target_z,
                         float xy_range, float& out_z) const;

    // Stop count by map — exposed for /diag.
    size_t StopsForMapCount(uint32_t map_id) const;
    size_t TotalStops() const { return stops_.size(); }

    // Read-only access for graph builders (UnifiedTravelGraph::LoadElevators).
    // The returned span is stable for the lifetime of the process; callers
    // must not mutate. Acquires the index mutex internally is unnecessary
    // because the graph build happens single-threaded at boot, AFTER
    // LoadFromGameObjects has finalised the vector.
    std::vector<ElevatorStop> const& Stops() const { return stops_; }

private:
    ElevatorIndex() = default;

    // Per-elevator stop source, for boot-time coverage logging.
    enum class StopSource { None, Timeto, Animation };

    // Append every stop for one spawned elevator. Derives the stop floors
    // from the world-DB Timeto*floor columns when present, otherwise from
    // the client TransportAnimation DB2 path's local-space Z extrema.
    // Computes world coordinates by rotating the local-space offset by the
    // spawn orientation and translating by the spawn position. Returns which
    // source produced the stops (None if the elevator was degenerate/skipped).
    StopSource AppendStopsFor(uint32_t map_id, uint32_t entry, uint64_t spawn_id,
                              float spawn_x, float spawn_y, float spawn_z, float spawn_o);

    // Cached navmesh-derived ledge for one stop. has_ledge==false records a
    // negative result (derivation ran, found no walkable poly nearby → use the
    // platform centre) so we never re-query a hopeless stop every tick.
    struct LedgeRec
    {
        float x = 0.f, y = 0.f, z = 0.f;
        bool  has_ledge = false;
    };
    // Key = (spawn_id << 8) | stop_idx. spawn_id fits 56 bits in practice.
    static uint64_t LedgeKey(uint64_t spawn_id, uint8_t stop_idx)
    { return (spawn_id << 8) | stop_idx; }

    std::vector<ElevatorStop>              stops_;
    std::unordered_map<uint64_t, LedgeRec> ledges_;   // navmesh-derived, lazy
    mutable std::mutex                     mtx_;   // protects stops_ + ledges_
};

} // namespace Playerbot::V2::Travel

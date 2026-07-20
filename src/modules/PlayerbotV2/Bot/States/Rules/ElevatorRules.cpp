// ElevatorRules — board and exit Thunder Bluff / Orgrimmar / Undercity /
// Stormwind / Ironforge / Dornogal / etc. style elevators (GAMEOBJECT_TYPE
// _TRANSPORT, type 11).
//
// Architecture:
//
//   * Operator (or future hardcoded catalog) places
//     `WorldMetadataKind::Elevator` annotations at every floor of every
//     elevator a bot might want to use. Each point's (x, y, z) is the
//     safe stand-on-platform spot.
//
//   * `idle:elevator_step_on` (this file): when the bot is standing near
//     an Elevator metadata point AND a GameObject of type 11 is within
//     ~5y of the metadata point at the metadata's Z, walks 2y into the
//     platform's collision so the server attaches the bot as a
//     passenger. `Player::GetTransport()` flips non-null next tick,
//     which trips the existing snapshot field `on_transport` and the
//     existing `idle:on_transport_wait` (priority 999) freezes
//     movement for the duration of the ride.
//
//   * `idle:elevator_step_off`: when the bot is on a transport AND the
//     transport is stopped, scans for the nearest Elevator metadata
//     point on this map within ~12y horizontally. If found AND that
//     metadata point's Z is materially different (>=15y delta) from
//     the boarding Z (latched in BotAI::elevator_boarding_z_ when the
//     bot first attached), step off toward the metadata's
//     (x, y, z) — that's the safe off-platform spot for this floor.
//
// Risk profile: rules gate on world-metadata presence. Without
// operator annotations they no-op silently. They never fire in
// combat / while casting / during BG or dungeon scripted content.
//
// Future work (deferred — see docs/ELEVATOR_PLAN.md):
//   * Hardcoded boot-time catalog of major-city elevators so users
//     don't need to annotate them manually.
//   * UnifiedTravelGraph integration so cross-floor routes are
//     planned through elevators automatically.
//   * Multi-stop elevators (TB spoke lifts pass three floors) get a
//     metadata point per intermediate stop.

#include "Bot/IdleRule.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Group/GroupSnapshot.h"
#include "../../World/WorldMetadata.h"
#include "../../Travel/ElevatorIndex.h"

#include <cmath>
#include <cstdint>

namespace Playerbot {

namespace {

using ::Playerbot::V2::World::WorldMetadataKind;
using ::Playerbot::V2::World::WorldMetadataStore;
using ::Playerbot::V2::World::WorldMetadataRecord;
using ::Playerbot::V2::Travel::ElevatorIndex;
using ::Playerbot::V2::Travel::ElevatorStop;

constexpr uint8 GO_TYPE_TRANSPORT = 11;   // matches GAMEOBJECT_TYPE_TRANSPORT

// Generic elevator stop point — abstracts over the two sources
// (ElevatorIndex auto-detection vs WorldMetadata operator annotation).
//
// Two coordinates, because the platform CENTRE and the walkable footing differ:
//   * (cx,cy,cz) = platform CENTRE. The shaft identity (the moving platform's
//     X/Y is fixed; only its Z animates) and the spot a bot must stand ON to be
//     attached as a passenger.
//   * (wx,wy,wz) = walkable WAIT/stand spot = the navmesh-derived board ledge
//     (~9-11y to the side of the centre) when available, else the centre. A bot
//     waits HERE for the platform; the centre is often a pit (bottom, platform
//     up) or open air (an upper stop) and can't be stood on until the platform
//     is present.
struct ElevatorPoint
{
    float cx, cy, cz;
    float wx, wy, wz;
    bool  is_auto;     // true: ElevatorIndex; false: WorldMetadata
    bool  has_ledge;   // a real navmesh ledge was derived (wait spot != centre)
    bool  valid() const { return std::isfinite(cx); }
};
inline ElevatorPoint MakeInvalid() { return {std::nanf(""), 0.f, 0.f, 0.f, 0.f, 0.f, false, false}; }

// Build an ElevatorPoint from an auto-detected stop, substituting the navmesh-
// derived board/disembark ledge for the wait spot when one has been derived
// (ElevatorIndex::LedgeFor, populated on the world thread by the snapshot
// builder). Falls back to the platform centre when no ledge is cached yet.
inline ElevatorPoint FromStop(ElevatorStop const& st)
{
    ElevatorPoint ep;
    ep.cx = st.x; ep.cy = st.y; ep.cz = st.z;
    ep.is_auto = true;
    float lx, ly, lz;
    if (ElevatorIndex::Instance().LedgeFor(st.spawn_id, st.stop_idx, lx, ly, lz))
    { ep.wx = lx; ep.wy = ly; ep.wz = lz; ep.has_ledge = true; }
    else
    { ep.wx = st.x; ep.wy = st.y; ep.wz = st.z; ep.has_ledge = false; }
    return ep;
}

// Common gate for both rules — bots that are in combat / casting /
// dead / in BG stay out of the elevator system. We do NOT gate on
// is_in_dungeon: raid instances like Karazhan (Curator → Shade lift),
// Black Temple, Ulduar (Razorscale ascent), Throne of the Four Winds,
// and Mogu'shan Vaults all have type-11 elevators that ARE part of
// the normal traversal path. Dungeon scripts coordinate the high-
// level "is the group ready to advance" decision; the elevator rules
// just handle the physical boarding once the bot is already at a stop.
bool ElevatorGateCommon(BotSnapshotView const& s)
{
    if (!s.is_alive()) return false;
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    if (s.in_battleground()) return false;
    return true;
}

// Nearest elevator stop on the bot's map within `range` yards horizontal
// AND `dz` yards vertical. Consults the auto-detected ElevatorIndex
// first; falls back to WorldMetadataKind::Elevator operator annotations.
// Returns an ElevatorPoint with .valid()==false when no stop is in range.
ElevatorPoint NearestElevatorOnFloor(
    BotSnapshotView const& s, float bz, float range, float dz)
{
    float bx, by, bz_unused; s.position(bx, by, bz_unused);
    (void)bz_unused;
    // 1) Auto-detected stops.
    if (auto const* stop = ElevatorIndex::Instance().NearestStopOnFloor(
            s.map_id(), bx, by, bz, range, dz))
        return FromStop(*stop);
    // 2) Operator annotations (fallback / overrides where auto-detection
    //    fails — e.g. scripted lifts that don't expose stop times). The
    //    annotation already IS the safe stand-on spot, so centre == wait.
    auto rows = WorldMetadataStore::Instance().RecordsForMapAndKind(
        s.map_id(), WorldMetadataKind::Elevator);
    if (rows.empty()) return MakeInvalid();
    float const r2 = range * range;
    WorldMetadataRecord const* best = nullptr;
    float best_dsq = r2;
    for (auto const& r : rows)
    {
        const float dxh = r.x - bx, dyh = r.y - by;
        const float dsq = dxh*dxh + dyh*dyh;
        if (dsq > best_dsq) continue;
        if (std::fabs(r.z - bz) > dz) continue;
        best = &r;
        best_dsq = dsq;
    }
    if (!best) return MakeInvalid();
    return {best->x, best->y, best->z, best->x, best->y, best->z, false, true};
}

// Nearest elevator stop on the bot's map regardless of Z — used by
// step_off to find the destination floor's stop position when the bot
// has been carried away from boarding Z. Same source precedence as
// NearestElevatorOnFloor.
ElevatorPoint NearestElevatorAnyZ(
    BotSnapshotView const& s, float range)
{
    float bx, by, bz; s.position(bx, by, bz);
    (void)bz;
    if (auto const* stop = ElevatorIndex::Instance().NearestStopAnyZ(
            s.map_id(), bx, by, range))
        return FromStop(*stop);
    auto rows = WorldMetadataStore::Instance().RecordsForMapAndKind(
        s.map_id(), WorldMetadataKind::Elevator);
    if (rows.empty()) return MakeInvalid();
    float const r2 = range * range;
    WorldMetadataRecord const* best = nullptr;
    float best_dsq = r2;
    for (auto const& r : rows)
    {
        const float dxh = r.x - bx, dyh = r.y - by;
        const float dsq = dxh*dxh + dyh*dyh;
        if (dsq < best_dsq) { best_dsq = dsq; best = &r; }
    }
    if (!best) return MakeInvalid();
    return {best->x, best->y, best->z, best->x, best->y, best->z, false, true};
}

// Resolve the stop a bot should BOARD. Two cases:
//   1. SAME FLOOR — the bot is already standing at a stop's Z (within 6y): the
//      normal boarding case (and multi-floor lifts).
//   2. BELOW THE BOARDING FLOOR — the bot reached the shaft's X/Y but is sitting
//      BELOW its lowest stop (it approached the lift from under its boarding
//      landing). Classic case: Somi pathed to (1902,-4373,z29.5) directly under
//      lift 206609 whose boarding floor is z44 — 14.5y above — so the dz=6 same-
//      floor match never engaged and the bot waited forever at the base. Resolve
//      the shaft by X/Y proximity (LowestStopNear, Z-agnostic) and return its
//      lowest stop so step_on walks the bot UP the last few yards to the boarding
//      ledge. Bounded to ≤20y below + ~12y planar so a bot merely standing under
//      an unrelated elevated platform isn't swept in (the step_on gate's
//      wants_vertical + vertical-progress checks gate intent on top of this).
ElevatorPoint ResolveBoardingStop(BotSnapshotView const& s, float bz)
{
    ElevatorPoint ep = NearestElevatorOnFloor(s, bz, /*range*/ 8.0f, /*dz*/ 6.0f);
    if (ep.valid()) return ep;
    float bx, by, bz_unused; s.position(bx, by, bz_unused); (void)bz_unused;
    if (auto const* low = ElevatorIndex::Instance().LowestStopNear(
            s.map_id(), bx, by, /*xy_range*/ 12.0f))
    {
        const float dz = low->z - bz;        // >0 = bot is below the boarding floor
        // Only a SMALL step-up (<=8y): a genuine "the bot reached the boarding
        // landing but its feet settled a few yards low" case. A large gap (Somi
        // 14.5y under lift 206609's z44 ledge, in a disconnected sub-ground pit)
        // is NOT a step the bot can take — the boarding ground is reached by
        // walking, not levitating up a shaft — so don't engage the climb and
        // hammer an Incomplete path; leave it to normal navigation / stuck rescue.
        if (dz >= -6.0f && dz <= 8.0f)
        {
            ElevatorPoint ep = FromStop(*low);
            // Only attempt the climb-from-below if a real navmesh LEDGE was
            // derived at the boarding floor. Without one the wait spot IS the
            // platform centre — which is carved out of the navmesh — so the climb
            // can never complete and the bot would hammer move_to forever (the
            // partial 1.3y forward-progress keeps resetting the stuck-rescue
            // counter, soft-locking it: observed live with Somi trapped in the
            // z29.5 sub-ground pit under lift 206609, whose z44 boarding ground
            // sits ~24y away — outside the ledge-derivation box — so no ledge
            // exists and the climb is hopeless). Bail to let stuck-rescue /
            // re-routing pull the bot out of the pocket instead.
            if (ep.has_ledge)
                return ep;
        }
    }
    return MakeInvalid();
}

// ---------- idle:elevator_step_on ----------
// Bot is near an Elevator metadata point AND a transport GO is at the
// same Z within ~5y → step onto it. The 2y push into the platform
// reliably attaches the player as a passenger server-side.
bool ElevatorStepOnGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!ElevatorGateCommon(s)) return false;
    if (s.on_transport()) return false;        // already boarded
    // NOTE: do NOT exclude mounted bots here. Flight bots auto-mount for the
    // long walk to an elevated flight master, so excluding `is_mounted` meant a
    // mounted bot reached the lift base, this gate failed, and walk_to_flightmaster
    // kept shoving it into the shaft forever (4.6M path-Locked blocks across 255
    // bots in the 4-day run — the FM-blacklist was masking THIS). The Fire
    // dismounts the bot on the boarding spot before attaching (you can't ride a
    // city lift mounted), then boards.
    float bx, by, bz; s.position(bx, by, bz);
    auto stop = ResolveBoardingStop(s, bz);
    if (!stop.valid()) return false;
    // Confirm the elevator's shaft is actually here by finding a type-11
    // GO whose HORIZONTAL position coincides with the stop. We deliberately
    // do NOT gate on the platform's instantaneous Z.
    //
    // Why: the ~126 anim-driven city lifts (Orgrimmar/Thunder Bluff/
    // Undercity towers, etc.) carry NO Timeto*floor stop frames, so the
    // server runs them as a *continuous* cycle (GameObject.cpp Transport::
    // Update → newProgress = now % period). The platform is therefore only
    // momentarily at the boarding floor's Z each cycle, and a 3D-distance
    // snapshot scan (30y) drops it from nearby_objects entirely whenever it
    // is near the top of the shaft. The previous |obj.z - stop.z| <= 4y
    // gate could only ever pass during the brief instant the platform was
    // parked at the bottom AND captured in that tick's snapshot — so in
    // practice it fired ~never (0 boardings fleet-wide). The correct model
    // is: the bot walks onto the boarding spot and *waits*; the descending
    // platform overlaps its collision and the server attaches it as a
    // passenger (on_transport flips, idle:on_transport_wait freezes it for
    // the ride). All we must verify here is that the shaft is the bot's:
    // a type-11 GO whose X/Y sits over the stop (the shaft X/Y is fixed;
    // only Z animates). 6y horizontal covers platform half-extent + the
    // synthesized-stop / GO-origin offset.
    // Intent gate. Because we no longer require the platform to be parked at
    // the bot's exact Z, an idle bot merely loitering within 8y of a city lift
    // would be swept away involuntarily. Only engage the lift when the bot has a
    // travel target on a MATERIALLY DIFFERENT floor (>=15y vertical):
    //   (a) a cross-map dock anchor (walk_to_known_dock routed it to this lift
    //       base to reach a deck-level dock — the Org-zeppelin case); or
    //   (b) a taxi/flight-master node (an elevated FM like Orgrimmar's Doras on
    //       the z103 rim — walk_to_flightmaster routed it here); or
    //   (c) its current same-map objective POI sits on another floor.
    //
    // `on_spot` proximity ALONE is deliberately NOT sufficient. A bot that
    // pathfind-failed and drifted within 3y of a lift base while its real goal
    // sits on THIS floor (Gorois at the Org zeppelin tower, ground-level goal 6y
    // away) was swept UP the 90y shaft and stranded at the top (step-off lands on
    // non-navmesh → FarFromPolyEnd), then the inline guard spammed disembarks on a
    // nearby zeppelin. Requiring a genuine off-floor destination keeps the legit
    // elevated-dock / elevated-FM / vertical-objective rides while refusing the
    // accidental sweep. (Checks are bidirectional via fabs so a bot at the TOP
    // can ride DOWN to a lower target too.)
    bool wants_vertical = false;
    float target_z = bz;
    if (s.has_nearest_portal_anchor() &&
        std::fabs(s.nearest_portal_anchor_z() - bz) >= 15.0f)
    { wants_vertical = true; target_z = s.nearest_portal_anchor_z(); }
    if (!wants_vertical && s.has_recommended_taxi_route() &&
        std::fabs(s.recommended_taxi_start_z() - bz) >= 15.0f)
    { wants_vertical = true; target_z = s.recommended_taxi_start_z(); }
    if (!wants_vertical && s.has_current_objective() &&
        s.current_objective_poi().valid &&
        s.current_objective_poi().map_id == s.map_id() &&
        std::fabs(s.current_objective_poi().z - bz) >= 15.0f)
    { wants_vertical = true; target_z = s.current_objective_poi().z; }
    if (!wants_vertical) return false;

    // Only board a lift that carries the bot materially CLOSER to the target
    // floor. Without this, a bot whose goal sits ABOVE the lift's highest stop
    // (the Orgrimmar zeppelin DECK z135 vs the rim-lift top z109) would ride to
    // the top, disembark at the rim, still want vertical, and re-board forever.
    // Requiring strict vertical progress (the shaft has a stop >5y nearer the
    // target than the bot's current Z) breaks that loop AND refuses the wrong
    // lift for an off-deck goal — the bot disembarks at the rim and walks the
    // rest. (When no shaft resolves, fall through: the stop was already valid.)
    float best_stop_z = bz;
    if (ElevatorIndex::Instance().BestStopZToward(
            s.map_id(), bx, by, target_z, /*xy_range*/ 12.0f, best_stop_z))
    {
        if (std::fabs(bz - target_z) - std::fabs(best_stop_z - target_z) < 5.0f)
            return false;
    }

    // Fire whenever the bot wants vertical travel AND is at the boarding
    // floor. We do NOT additionally require the platform GO to be visible
    // this tick: a tall shaft (Org tower base→deck ≈ 90y) pushes the
    // platform outside the 30y/3D nearby_objects scan for most of its
    // cycle, so requiring platform-presence here would let wander/idle pull
    // the bot off the boarding spot between sightings. Instead the Fire
    // re-issues a hold on the spot every tick (priority 710, above wander),
    // keeping the bot planted until the cycling platform descends through
    // this floor and the server auto-attaches it. The shaft-here check (a
    // type-11 GO horizontally over the stop, when visible) is folded into
    // the Fire as a cheap sanity log only — not a gate.
    return true;
}

bool ElevatorStepOnFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    auto stop = ResolveBoardingStop(s, bz);
    if (!stop.valid()) return false;

    // Boarding is a two-phase manoeuvre that mirrors how a real player boards a
    // city lift, because a clientless bot is NOT auto-attached when a moving
    // platform overlaps it (that needs a client packet — the reason step_on used
    // to fire but nobody ever rode):
    //
    //   PHASE 1 — WAIT on the walkable board LEDGE (stop.w*). The platform CENTRE
    //   is frequently un-standable: a pit at the bottom while the platform is up,
    //   or open air at an upper stop. The navmesh-derived ledge (~9-11y to the
    //   side) is always solid footing. Hold here until the platform arrives.
    //
    //   PHASE 2 — when the platform GO is at THIS floor (its animated Z within
    //   ~4y of the stop centre Z), step from the ledge ONTO the centre and call
    //   use_game_object; the API's on-platform fast path attaches the instant the
    //   bot is within 6y/3y of the platform. on_transport then flips and
    //   idle:on_transport_wait freezes the bot for the ride; idle:elevator_step_off
    //   releases it at the destination floor.
    //
    // Stepping onto the centre ONLY while the platform is present means the bot
    // never walks into the open shaft / pit.
    // on_ledge must be 3D: a bot standing directly UNDER the shaft (Somi at the
    // lift 206609 base, z29.5, 14.5y below the z44 boarding ledge) is planar-on-
    // top of the wait spot but must still CLIMB to the floor before it can board.
    // Requiring |bz - wz| <= 5y forces the move_to(... wz) climb up the ramp/stairs
    // to the boarding floor; without it the bot would sit below the platform
    // emitting a board that the API refuses (its attach needs |dz| <= 3y).
    const float wdx = stop.wx - bx, wdy = stop.wy - by;
    const bool on_ledge = (wdx*wdx + wdy*wdy) <= (3.5f * 3.5f) &&
                          std::fabs(stop.wz - bz) <= 5.0f;
    if (!on_ledge)
    {
        if (!emit.move_to(stop.wx, stop.wy, stop.wz, /*run*/ false)) return false;
        ai.set_last_rule_fired("idle:elevator_step_on");
        return true;
    }

    // On the ledge. Dismount before boarding — a city lift won't carry a mounted
    // bot, and an auto-mounted flight bot would otherwise sit on the spot forever.
    if (s.raw().movement.is_mounted)
    {
        emit.dismount();
        ai.set_last_rule_fired("idle:elevator_dismount");
        return true;
    }

    // Is the platform of THIS shaft at our floor right now? Find a type-11 GO
    // whose X/Y sits over the stop centre (the shaft X/Y is fixed; only Z
    // animates) and whose current Z is within ~4y of the stop centre Z.
    bool platform_here = false;
    ObjectGuid platform_guid;
    for (auto const& o : s.raw().world_objects.nearby_objects)
    {
        if (o.go_type != GO_TYPE_TRANSPORT) continue;
        const float odx = o.x - stop.cx, ody = o.y - stop.cy;
        if (odx*odx + ody*ody > 12.0f * 12.0f) continue;   // not this shaft
        platform_guid = o.guid;
        if (std::fabs(o.z - stop.cz) <= 4.0f) platform_here = true;
        break;
    }

    if (platform_here)
    {
        // Platform is at our floor → step onto the centre and attach. The API
        // only completes the attach when the bot is actually over the platform
        // (xy<=6y, |dz|<=3y), so emitting both the close-in move and the board is
        // safe; if still on the ledge edge, the move closes the last few yards
        // and the next tick attaches.
        const float cdx = stop.cx - bx, cdy = stop.cy - by;
        if (cdx*cdx + cdy*cdy <= 6.0f * 6.0f && !platform_guid.IsEmpty())
        {
            emit.use_game_object(platform_guid);
            ai.set_last_rule_fired("idle:elevator_step_on");
            return true;
        }
        emit.move_to(stop.cx, stop.cy, stop.cz, /*run*/ false);
        ai.set_last_rule_fired("idle:elevator_step_on");
        return true;
    }

    // Platform not at our floor (or not in this tick's scan) — hold on the ledge
    // until it cycles back. Re-issuing the hold every tick (priority 710, above
    // wander) keeps the bot planted on solid footing between platform sightings.
    emit.move_to(stop.wx, stop.wy, stop.wz, /*run*/ false);
    ai.set_last_rule_fired("idle:elevator_step_on");
    return true;
}

// ---------- idle:elevator_step_off ----------
// Bot is on a stopped transport AND there's an Elevator metadata
// point near the bot's current Z (different from boarding Z) →
// step off the platform onto the destination floor's annotated spot.
bool ElevatorStepOffGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!ElevatorGateCommon(s)) return false;
    if (!s.on_transport()) return false;
    // Step off once the platform has carried us clear of the boarding floor
    // (>=15y Z delta) AND we are vertically ALIGNED with a destination stop
    // (within ~5y). We deliberately do NOT require the platform to be parked /
    // Z-stable: 126 of the 146 indexed city lifts are continuous-cycle (anim-
    // driven, NO Timeto stop frames) and so never hold a stable Z — the old
    // `transport_stopped && transport_z_stable_ms>=500` gate only ever passed
    // for the 4 Timeto lifts that physically park, and stranded a bot aboard
    // every continuous shaft forever (it would ride up frozen and never get an
    // exit signal). Vertical alignment with a stop is itself the "arrived at
    // this floor" signal for BOTH lift kinds; the brief alignment window on a
    // continuous lift is caught by the frequent snapshot cadence, and the Fire
    // detaches via use_game_object (RemovePassenger) + the API snaps the bot onto
    // the floor's navmesh ledge. Ships (type-15) never reach the stop test: they ride
    // at sea level so |bz-bdz| stays < 15y, and even if not, no elevator stop
    // sits near their Z — NearestElevatorOnFloor returns invalid.
    float bx, by, bz; s.position(bx, by, bz);
    // boarding_z=0 means we just attached this tick (latch hasn't run yet) or
    // we boarded on a ship at sea level — either way wait.
    const float bdz = ai.elevator_boarding_z();
    if (bdz == 0.f) return false;
    if (std::fabs(bz - bdz) < 15.0f) return false;
    // There must be an elevator stop aligned with our current Z (within 5y).
    return NearestElevatorOnFloor(s, bz, /*range*/ 12.0f, /*dz*/ 5.0f).valid();
}

bool ElevatorStepOffFire(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const&,
                         BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);

    // We've arrived at a destination floor (gate verified carried >=15y AND
    // aligned with a stop). DETACH cleanly via the platform GO: emitting
    // use_game_object on the ridden transport RemovePassenger's us server-side
    // AND the API immediately relocates the bot from the platform centre (open
    // air at an upper stop) onto the navmesh-derived disembark LEDGE at this
    // floor. This replaces the old move_to(centre): a world-space move issued
    // while still attached fought the passenger link (it detached the bot mid-
    // ride, leaving it riding the platform collision un-attached — Pyrethel/
    // Gorois on lift 206610), and the centre is un-standable open air anyway, so
    // the bot rode forever. The ridden platform is the type-11 GO nearest the
    // bot (it moves WITH us, so it sits at dist ~0 in nearby_objects).
    ObjectGuid platform_guid;
    float best_d2 = 8.0f * 8.0f;
    for (auto const& o : s.raw().world_objects.nearby_objects)
    {
        if (o.go_type != GO_TYPE_TRANSPORT) continue;
        const float odx = o.x - bx, ody = o.y - by;
        const float d2  = odx*odx + ody*ody;
        if (d2 < best_d2) { best_d2 = d2; platform_guid = o.guid; }
    }
    if (platform_guid.IsEmpty()) return false;

    emit.use_game_object(platform_guid);   // → RemovePassenger + ledge relocate (API)
    ai.set_last_rule_fired("idle:elevator_step_off");
    return true;
}

} // anonymous namespace

void RegisterElevatorRules(IdleRuleRegistry& r)
{
    // step_off is priority 1000 — it MUST outrank idle:on_transport_wait (999).
    // on_transport_wait freezes the bot whenever the platform's Z isn't stable,
    // which for the 126 continuous-cycle city lifts is EVERY tick — so at 712 it
    // never got a turn and the bot rode aligned past its destination floor forever.
    // Its gate only fires in the narrow "carried >=15y AND aligned with a
    // destination stop" window and otherwise returns false, yielding to the
    // freeze; so ranking it first lets it pop the bot off at the target floor
    // without disturbing the mid-ride freeze. (Still above step_on so a bot
    // already aboard at the right floor never re-attempts a boarding.)
    {
        IdleRule rule;
        rule.name     = "idle:elevator_step_off";
        rule.priority = 1000;
        rule.gate     = &ElevatorStepOffGate;
        rule.fire     = &ElevatorStepOffFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:elevator_step_on";
        rule.priority = 710;
        rule.gate     = &ElevatorStepOnGate;
        rule.fire     = &ElevatorStepOnFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

// PlayerbotMovement.cpp
//
// See PlayerbotMovement.h for the contract.

#include "PlayerbotMovement.h"

#include "Map.h"
#include "MapDefines.h"
#include "MapManager.h"
#include "MMapManager.h"
#include "MMapDefines.h"
#include "MotionMaster.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Unit.h"
#include "VMapDefinitions.h"
#include "Log.h"

#include "DetourCommon.h"
#include "DetourNavMeshQuery.h"

#include <G3D/Vector3.h>

#include <algorithm>
#include <cmath>
#include <exception>

#ifdef _MSC_VER
#include <windows.h>   // EXCEPTION_EXECUTE_HANDLER for SehSafeCalculatePath
#endif

namespace Playerbot::BotMovement {

namespace {

// Same threshold BotSetupPipeline used (audit 2026-05-17 found 'Howling Fjord
// (Valgarde)' at Z=205 — 130y above ground; bots spawned on a cliff with no
// navmesh under their feet and emitted ~60k path-fail telemetry lines).
constexpr float kTeleportZSnapThresholdY = 50.0f;

// Distance above the queried ground that we drop the bot when snapping —
// matches the 2y lift BotSetupPipeline used so unit collision height never
// places the bot half-inside terrain on the first frame.
constexpr float kGroundLiftY = 2.0f;

} // namespace

// ---- SehSafeCalculatePath ---------------------------------------------------

#ifdef _MSC_VER
bool SehSafeCalculatePath(PathGenerator& path, float x, float y, float z) noexcept
{
    __try
    {
        return path.CalculatePath(x, y, z, /*forceDest=*/false);
    }
    // TC ASSERT/ABORT (EXCEPTION_ASSERTION_FAILURE) means corrupt state —
    // let it reach the crash handler for a dump at the fault site instead
    // of masking it as a failed path (same policy as SehSafeMapUpdate).
    __except (GetExceptionCode() == EXCEPTION_ASSERTION_FAILURE
              ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int SehSafeNearestPolyProbe(void const* query, float const* detourPt,
                            float const* extents, void const* filter,
                            float* nearestOut) noexcept
{
    __try
    {
        dtPolyRef ref = 0;
        static_cast<dtNavMeshQuery const*>(query)->findNearestPoly(
            detourPt, extents, static_cast<dtQueryFilter const*>(filter),
            &ref, nearestOut);
        return ref ? 1 : 0;
    }
    __except (GetExceptionCode() == EXCEPTION_ASSERTION_FAILURE
              ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}
#else
bool SehSafeCalculatePath(PathGenerator& path, float x, float y, float z) noexcept
{
    return path.CalculatePath(x, y, z, /*forceDest=*/false);
}
#endif

// ---- SnapToGround -----------------------------------------------------------

bool SnapToGround(Player* p, float x, float y, float& z)
{
    if (!p)
        return false;

    Map* m = p->GetMap();
    if (!m)
        return false;

    // Pre-load the destination grid. UpdateAllowedPositionZ samples vmap
    // through the local grid; if the grid hasn't streamed in yet the
    // sampler returns INVALID_HEIGHT and we'd silently keep the bad Z.
    // LoadGrid is a no-op when the grid is already resident.
    m->LoadGrid(x, y);

    float const original = z;

    // Step 1: UpdateAllowedPositionZ — same vmap/mmap composite TC uses
    // in Player::SetPosition. Wrap in try/catch because BIH::subdivide
    // throws "invalid node overlap" during dynamic GO churn (BG flags,
    // doors); crash 2026-05-13.
    try
    {
        p->UpdateAllowedPositionZ(x, y, z);
    }
    catch (std::exception const& e)
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotMovement::SnapToGround] {} BIH exception at ({:.1f},{:.1f}): {}",
            p->GetName(), x, y, e.what());
        z = original;
    }
    catch (...)
    {
        z = original;
    }

    // UpdateAllowedPositionZ returns the input unchanged when it can't
    // find any ground/water. Detect that and fall through to the explicit
    // probes; otherwise we're done.
    if (std::fabs(z - original) > 0.05f)
        return true;

    // Step 2: GetWaterOrGroundLevel — swim-aware lookup with the player's
    // collision height accounted for. Best for shoreline / waterfall cases
    // where UpdateAllowedPositionZ picked the wrong surface.
    float ground = INVALID_HEIGHT;
    float const wog = m->GetWaterOrGroundLevel(p->GetPhaseShift(), x, y, original,
                                               &ground, /*swim*/ false);
    if (wog > INVALID_HEIGHT)
    {
        z = wog;
        return true;
    }

    // Step 3: top-down vmap probe. Last resort — finds any walkable
    // surface under (x, y, MAX_HEIGHT). If this returns INVALID_HEIGHT
    // there's genuinely no ground at this XY (off-map, hole in mesh).
    float const topdown = m->GetHeight(p->GetPhaseShift(), x, y, MAX_HEIGHT,
                                       /*vmap*/ true);
    if (topdown > INVALID_HEIGHT)
    {
        // Reject top-down results that are far ABOVE the input z. The
        // input z is often a player's actual in-game position (group
        // member, nearby creature) and is already valid. A large upward
        // result means the probe found outdoor terrain sitting above a
        // dungeon room — e.g., the Westfall hillside at z≈358 above the
        // Deadmines floor at z≈62. Accepting it corrupts the pathfinder
        // destination so that both src and dst map to the same entrance
        // poly, producing a perpetually-incomplete stub path and
        // [move_blocked] for every follow attempt.
        // 30y is generous: legitimate SnapToGround use cases only need a
        // few yards of upward correction; a 30y buried floor also corrects.
        constexpr float kMaxUpwardSnapY = 30.0f;
        if (topdown <= original + kMaxUpwardSnapY)
        {
            z = topdown;
            return true;
        }
        // Outdoor terrain far above a dungeon — keep the caller's z.
        z = original;
        return false;
    }

    // No surface found. Restore the input so the caller can decide what
    // to do (refuse the move, log, retry with a different XY).
    z = original;
    return false;
}

// ---- NearestNavPoint --------------------------------------------------------

bool NearestNavPoint(Player* p, float tx, float ty, float tz,
                     float hxy, float hz, Position& out)
{
    if (!p)
        return false;
    Map* m = p->GetMap();
    if (!m)
        return false;

    // The tile under the target may not be resident (a tall lift shaft can
    // span into a grid the bot hasn't streamed). LoadGrid is a no-op when the
    // grid is already loaded; it keeps the navmesh tile alive for the query.
    m->LoadGrid(tx, ty);

    // Resolve the query the same way PathGenerator does: the *terrain* map id
    // (phased maps share a parent terrain) drives the navmesh selection, while
    // the instance map id + instance id pick the per-instance mesh.
    uint32 const terrainMapId = PhasingHandler::GetTerrainMapId(
        p->GetPhaseShift(), p->GetMapId(), m->GetTerrain(), tx, ty);
    dtNavMeshQuery const* query = MMAP::MMapManager::instance()->GetNavMeshQuery(
        terrainMapId, p->GetMapId(), p->GetInstanceId());
    if (!query)
        return false;

    // Walkable surfaces only (ground + steep ground + road — city ledges and
    // bridges beside lifts are frequently road-tagged by the road-aware mmaps,
    // so a GROUND-only filter would miss them). We deliberately do NOT include
    // WATER / MAGMA_SLIME: a disembark ledge must be dry footing, and a boarding
    // query must not snap a bot into a lava moat beside a raid lift.
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_ROAD);
    filter.setExcludeFlags(0);

    // Detour coordinate convention: Recast is Y-up, so WoW (x,y,z) maps to
    // Recast (y,z,x); the snapped result maps back (x=pt[2], y=pt[0], z=pt[1]).
    float const center[3]  = { ty, tz, tx };
    float const extents[3] = { hxy, hz, hxy };
    float nearest[3] = { 0.f, 0.f, 0.f };
    // SEH-guarded probe — NearestNavPoint runs from the snapshot builder
    // (ElevatorIndex lazy ledge cache) on the world thread; same Detour
    // tile-race AV hazard as every other raw query (2026-06-12 sweep).
    if (SehSafeNearestPolyProbe(query, center, extents, &filter, nearest) <= 0)
        return false;

    out.Relocate(nearest[2], nearest[0], nearest[1]);
    return true;
}

// ---- LineIsClear ------------------------------------------------------------

bool LineIsClear(Player* p, float sx, float sy, float sz,
                            float dx, float dy, float dz)
{
    if (!p)
        return false;
    Map* m = p->GetMap();
    if (!m)
        return false;
    // VMAP::ModelIgnoreFlags::M2 lets us ignore decorative doodads (trees,
    // crates) that would otherwise block a jump's hop arc. Walls, doors,
    // ground geometry remain blocking — exactly what we want.
    // LineOfSightChecks is a bitflag-defined enum; `|` of two enumerators
    // promotes to int, so we re-cast to the enum type the signature wants.
    return m->isInLineOfSight(p->GetPhaseShift(), sx, sy, sz, dx, dy, dz,
                              static_cast<LineOfSightChecks>(
                                  LINEOFSIGHT_CHECK_VMAP |
                                  LINEOFSIGHT_CHECK_GOBJECT),
                              VMAP::ModelIgnoreFlags::M2);
}

// ---- SafeNearTeleport -------------------------------------------------------

bool SafeNearTeleport(Player* p, float x, float y, float z, float o)
{
    if (!p)
        return false;

    // Remember the caller's intended Z. We need this to reject the
    // "snapped to wrong surface" case below (cave interior teleport
    // landing on the mountain roof above).
    float const requested_z = z;

    // Snap Z first. If there's no ground at all, refuse — the caller will
    // either retry with a different XY or escalate to stuck recovery.
    if (!SnapToGround(p, x, y, z))
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotMovement::SafeNearTeleport] {} no ground at ({:.1f},{:.1f}); refusing",
            p->GetName(), x, y);
        return false;
    }

    // Cave-interior safety check: when a bot is inside a cave / overhang
    // and the random XY offset lands on the mountain ABOVE the cave roof,
    // UpdateAllowedPositionZ's GetMapHeight picks the *topmost* walkable
    // surface (the mountain) rather than the cave floor. The bot teleports
    // out of the cave onto a hillside it can't path back down from
    // (Shadowthread Cave 2026-05-21, Uraimus). Reject when the snap moved
    // Z by more than 20y — that magnitude only happens when crossing into
    // a different surface stratum, and the right answer is "don't
    // teleport" rather than "land on the wrong floor".
    constexpr float kMaxSnapDeltaY = 20.0f;
    if (std::fabs(z - requested_z) > kMaxSnapDeltaY)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotMovement::SafeNearTeleport] {} Z snap delta {:.1f}y exceeds {:.0f}y "
            "(input z={:.1f} snapped z={:.1f}) at ({:.1f},{:.1f}); refusing",
            p->GetName(), z - requested_z, kMaxSnapDeltaY,
            requested_z, z, x, y);
        return false;
    }

    // Off-mesh rescue: if (x,y,z) is still FARFROMPOLY, ask Detour for
    // the closest poly and use that instead. Prevents landing in a wall
    // that vmap considers "ground" but Detour can't path away from.
    PathGenerator probe(p);
    if (SehSafeCalculatePath(probe, x, y, z))
    {
        PathType pt = probe.GetPathType();
        if (pt & PATHFIND_FARFROMPOLY_END)
        {
            G3D::Vector3 const& nearest = probe.GetActualEndPosition();
            TC_LOG_INFO("playerbot.v2",
                "[BotMovement::SafeNearTeleport] {} dest off-mesh, snapping ({:.1f},{:.1f},{:.1f}) -> ({:.1f},{:.1f},{:.1f})",
                p->GetName(), x, y, z, nearest.x, nearest.y, nearest.z);
            x = nearest.x;
            y = nearest.y;
            z = nearest.z;
        }
    }

    p->NearTeleportTo(x, y, z, o);
    return true;
}

// ---- SafeTeleport -----------------------------------------------------------

bool SafeTeleport(Player* p, uint32 map_id, float x, float y, float z, float o,
                  uint32 options)
{
    if (!p)
        return false;

    // Probe the destination ground if the target map is already loaded.
    // World maps stream in lazily; if the map isn't resident we accept
    // the teleport as-is (the map loads on Player::TeleportTo and we'd
    // need a post-teleport hook to validate after arrival).
    if (Map* destMap = sMapMgr->FindMap(map_id, 0))
    {
        float const ground = destMap->GetHeight(PhasingHandler::GetEmptyPhaseShift(),
                                                x, y, MAX_HEIGHT, /*vmap*/ true);
        if (ground > INVALID_HEIGHT && (z - ground) > kTeleportZSnapThresholdY)
        {
            float const snapped = ground + kGroundLiftY;
            TC_LOG_WARN("playerbot.v2",
                "[BotMovement::SafeTeleport] {} dest map={} ({:.1f},{:.1f},{:.1f}) is "
                "{:.1f}y above ground ({:.1f}); snapping to ground+{} = {:.1f}",
                p->GetName(), map_id, x, y, z, z - ground, ground,
                kGroundLiftY, snapped);
            z = snapped;
        }
    }

    return p->TeleportTo(map_id, x, y, z, o,
                         static_cast<TeleportToOptions>(options));
}

bool SafeTeleport(Player* p, WorldLocation const& loc, uint32 options)
{
    return SafeTeleport(p, loc.GetMapId(), loc.GetPositionX(), loc.GetPositionY(),
                        loc.GetPositionZ(), loc.GetOrientation(), options);
}

// ---- TryUnstickJump ---------------------------------------------------------

bool TryUnstickJump(Player* p, float forward)
{
    if (!p)
        return false;

    float const sx = p->GetPositionX();
    float const sy = p->GetPositionY();
    float const sz = p->GetPositionZ();
    float const facing = p->GetOrientation();

    // M-P2c: an unstick hop emitted at the bot's CURRENT facing nearly
    // always launches INTO the obstacle that wedged it (the bot was walking
    // toward its goal when it jammed, so it's still pointing at the wall /
    // ledge / closed door). Try a fan of bearings and take the first that
    // both lands on real ground (SnapToGround) and has a clear arc chord
    // (LineIsClear). Order: current facing first (cheapest unstick when the
    // jam is a low ledge directly ahead), then ±90° sidesteps, then 180°
    // (back the way we came — the guaranteed-open escape from a dead-end
    // pocket). Face the chosen bearing before MoveJump so the parabolic arc
    // actually travels that way.
    constexpr float kPi = float(M_PI);
    float const bearings[] = {
        facing,                 // straight ahead
        facing + kPi * 0.5f,    // +90° (left)
        facing - kPi * 0.5f,    // -90° (right)
        facing + kPi,           // 180° (reverse out of the pocket)
    };

    for (float bearing : bearings)
    {
        float const dx = sx + std::cos(bearing) * forward;
        float const dy = sy + std::sin(bearing) * forward;

        float dz = sz;
        if (!SnapToGround(p, dx, dy, dz))
            continue;   // no ground under this landing — try the next bearing

        // Strict LoS gate on the arc chord at chest height. Hopping into a
        // closed door / wall / cliff face wedges the bot harder, so a bearing
        // whose chord is blocked is rejected.
        if (!LineIsClear(p, sx, sy, sz + 1.5f, dx, dy, dz + 1.5f))
            continue;

        // Face the bearing first; MoveJump launches along the unit's facing
        // and we want the arc to travel toward the open direction we found.
        p->SetFacingTo(bearing);

        Position dest = p->GetPosition();
        dest.m_positionX = dx;
        dest.m_positionY = dy;
        dest.m_positionZ = dz;
        p->GetMotionMaster()->MoveJump(/*id*/ 0, dest, /*speedXY*/ 7.0f);
        return true;
    }

    // Every bearing was blocked or had no ground — refuse rather than launch
    // the bot into geometry / the void.
    return false;
}

// ---- PostTeleportSnap -------------------------------------------------------

void PostTeleportSnap(Player* p)
{
    if (!p || !p->IsInWorld())
        return;
    Map* m = p->GetMap();
    if (!m)
        return;

    float const x = p->GetPositionX();
    float const y = p->GetPositionY();
    float const z = p->GetPositionZ();

    // Cheap probe: if Detour can reach the bot's current position, the
    // landing was fine and we exit silently.
    PathGenerator probe(p);
    if (!SehSafeCalculatePath(probe, x, y, z))
        return;
    PathType pt = probe.GetPathType();
    if (!(pt & (PATHFIND_FARFROMPOLY_START | PATHFIND_FARFROMPOLY_END | PATHFIND_NOPATH)))
        return;

    // Off-mesh landing. Try to recover via Detour's nearest poly first;
    // fall back to a top-down vmap probe if Detour gives nothing useful.
    G3D::Vector3 const& nearest = probe.GetActualEndPosition();
    float nx = nearest.x;
    float ny = nearest.y;
    float nz = nearest.z;

    if (!SnapToGround(p, nx, ny, nz))
    {
        // Last resort: probe directly under the current XY. If even that
        // fails the map has no ground here and we leave the bot alone —
        // the stuck-recovery escalation will take over.
        float top = m->GetHeight(p->GetPhaseShift(), x, y, MAX_HEIGHT, /*vmap*/ true);
        if (top <= INVALID_HEIGHT)
            return;
        nx = x;
        ny = y;
        nz = top + kGroundLiftY;
    }

    TC_LOG_INFO("playerbot.v2",
        "[BotMovement::PostTeleportSnap] {} off-mesh landing ({:.1f},{:.1f},{:.1f}) -> ({:.1f},{:.1f},{:.1f})",
        p->GetName(), x, y, z, nx, ny, nz);
    p->NearTeleportTo(nx, ny, nz, p->GetOrientation());
}

// ---- ClassifyDestinationLiquid ----------------------------------------------

LiquidVerdict ClassifyDestinationLiquid(Player* p, float x, float y, float z,
                                        LiquidData* outData /*= nullptr*/)
{
    if (!p)
        return LiquidVerdict::Dry;

    Map* m = p->GetMap();
    if (!m)
        return LiquidVerdict::Dry;

    // Decide the wade<->swim boundary against the bot's REAL collision height
    // (matches the client's per-model depth check), but floor it at
    // DEFAULT_COLLISION_HEIGHT. A smaller height would classify shallower water
    // as UNDER_WATER and start refusing the shoreline wading starter-zone hubs
    // depend on — flooring keeps the gate at-least-as-permissive as the
    // historical default while letting taller / mounted bots wade deeper.
    float const collisionHeight =
        std::max(p->GetCollisionHeight(), DEFAULT_COLLISION_HEIGHT);

    LiquidData liq{};
    ZLiquidStatus const zs = m->GetLiquidStatus(p->GetPhaseShift(), x, y, z,
                                                map_liquidHeaderTypeFlags::AllLiquids,
                                                &liq, collisionHeight);
    if (outData)
        *outData = liq;

    if (zs == LIQUID_MAP_NO_WATER)
        return LiquidVerdict::Dry;

    // Magma / slime is lethal on contact regardless of depth.
    if (liq.type_flags.HasFlag(map_liquidHeaderTypeFlags::Magma) ||
        liq.type_flags.HasFlag(map_liquidHeaderTypeFlags::Slime))
        return LiquidVerdict::Hazard;

    // Fully submerged for this bot's collision height => must swim: a
    // non-aquatic bot drowns, and the navmesh ends at the surface so there is
    // no walkable path below it anyway.
    //
    // Bitmask test, NOT exact-equality: GridMap/TerrainMgr OR in
    // LIQUID_MAP_OCEAN_FLOOR (0x10) whenever the probe sits within
    // GROUND_HEIGHT_TOLERANCE of the floor, so a bot on the BOTTOM of deep
    // water returns UNDER_WATER|OCEAN_FLOOR (0x18). Because routing probes the
    // ground-level Z, that floor case is the COMMON one — an `== UNDER_WATER`
    // test would miss it and wrongly allow walking into deep water. `& UNDER_
    // WATER` catches both 0x08 and 0x18; shallow IN_WATER|OCEAN_FLOOR (0x14)
    // and WATER_WALK|OCEAN_FLOOR (0x12) correctly stay Wadeable.
    if ((zs & LIQUID_MAP_UNDER_WATER) != 0)
        return LiquidVerdict::Swim;

    // ABOVE_WATER / WATER_WALK / IN_WATER / OCEAN_FLOOR: the bot's head stays
    // above the surface — wading depth, allowed (PathGenerator wades through).
    return LiquidVerdict::Wadeable;
}

} // namespace Playerbot::BotMovement

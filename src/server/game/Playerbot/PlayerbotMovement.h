// PlayerbotMovement.h
//
// Unified bot-movement helpers used by PlayerbotAPI and by the V2 module's
// setup pipeline. Centralises the "treat the bot like a smart creature"
// invariants we want everywhere a bot is repositioned:
//
//   * Z is always snapped to a real walkable surface (no falling through
//     the world, no spawning inside geometry).
//   * Cross-map and same-map teleports probe the destination ground before
//     committing, so a stale POI / hand-curated coord doesn't drop the bot
//     onto a navmesh hole.
//   * Jumps and other ballistic moves refuse to launch into walls / closed
//     doors / cliff faces.
//   * Off-mesh recovery (FARFROMPOLY_START) re-snaps the bot to the nearest
//     valid navmesh poly before the AI re-emits movement.
//
// Inspiration: mod-playerbots' SearchForBestPath / CheckCollisionAndGetValidCoords
// (AzerothCore module), mapped onto TrinityCore's PathGenerator + vmap/mmap
// APIs. The helpers operate purely on TrinityCore primitives (no module
// includes) so they live next to PlayerbotAPI in the core-side surface.
//
// All entry points run on the world thread.

#pragma once

#include "Define.h"
#include "Position.h"

class Player;
class Unit;
class PathGenerator;
struct LiquidData;   // declared in MapDefines.h (included transitively in the .cpp)
// WorldLocation is declared in Position.h (included above) as `class WorldLocation : public Position`.

namespace Playerbot::BotMovement {

// Detour `dtNavMeshQuery` can ACCESS_VIOLATION when a navmesh tile gets
// unloaded mid-query (e.g. grid stream-out racing the path build). MSVC
// /EHsc can't translate SEH to a C++ exception, so the only safe option
// is __try/__except in a helper whose stack frame has no objects with
// destructors. Returns false on SEH fault (treat as path-failed).
bool SehSafeCalculatePath(PathGenerator& path, float x, float y, float z) noexcept;

// SEH-guarded raw dtNavMeshQuery::findNearestPoly probe for diagnostics
// ([path_fail_ep] endpoint introspection). Same tile-race AV hazard as
// CalculatePath — the diag probe shipped WITHOUT this guard and crashed
// the world thread twice within minutes of boot (2026-06-12, full WER
// dumps: dtVlerp <- closestPointOnDetailEdges <- findNearestPoly <-
// API::move_to log_path_fail; boot-time mass logins maximize path
// failures so the probe hammered half-loaded tiles). Returns 1 when a
// poly resolves, 0 when none, -1 on SEH fault (assertion failures
// continue-search per the SehSafeMapUpdate policy). The dtQueryFilter
// and float arrays live in the CALLER's frame — this helper's frame
// must stay destructor-free for __try.
// nearestOut (optional, float[3] in Detour order) receives the snapped
// point when a poly resolves — NearestNavPoint consumes it.
int SehSafeNearestPolyProbe(void const* query, float const* detourPt,
                            float const* extents, void const* filter,
                            float* nearestOut = nullptr) noexcept;

// Snap (x, y, z) to a safe walkable Z. Layered fallbacks:
//   1. Player::UpdateAllowedPositionZ — vmap/mmap composite (wrapped in
//      try/catch for the BIH "invalid node overlap" race).
//   2. Map::GetWaterOrGroundLevel — swim-aware ground for the player's
//      collision height.
//   3. Map::GetHeight(MAX_HEIGHT) — top-down vmap probe.
// Mutates `z` in place. Returns true if a real surface was found; false
// means caller should refuse the move (no ground at this XY).
bool SnapToGround(Player* p, float x, float y, float& z);

// Snap target (tx,ty,tz) to the nearest WALKABLE navmesh polygon point as
// seen from `p` (its map / instance / phase), searching a box of half-extents
// `hxy` horizontal and `hz` vertical around the target. Unlike SnapToGround
// (which only adjusts Z straight DOWN at the same X/Y) this finds a laterally
// OFFSET walkable point — e.g. the rim ledge ~10y beside an elevator platform-
// centre that hovers over the open shaft at the top of its travel. Writes the
// snapped world point to `out` and returns true; false when no walkable poly
// is within the box, or the map has no navmesh loaded for the tile.
//
// The horizontal extent must be wide enough to clear the moving-platform
// footprint (which is carved OUT of the static navmesh) and reach the
// surrounding floor: ~14-16y for city lifts. Detour caps a findNearestPoly
// box at ~128 overlapped polys, so keep `hz` modest (≤8y) to avoid grabbing a
// poly from a different elevator floor. Runs on the world thread (queries the
// live navmesh); never call from an AI worker.
bool NearestNavPoint(Player* p, float tx, float ty, float tz,
                     float hxy, float hz, Position& out);

// Strict line-of-sight check used by ballistic moves (jump, charge).
// Returns true if the straight line from src to dst is clear of static
// vmap collision (walls, closed doors, terrain). Cheap (single vmap
// raycast).
bool LineIsClear(Player* p, float sx, float sy, float sz,
                            float dx, float dy, float dz);

// Wrap Player::NearTeleportTo with full safety: SnapToGround + a
// PathGenerator FARFROMPOLY_START rescue (if the snapped position still
// sits off the navmesh, jump to the nearest poly Detour can find).
// Refuses in combat / mid-cast like the underlying TC primitive.
// Returns true on success.
bool SafeNearTeleport(Player* p, float x, float y, float z, float o);

// Wrap Player::TeleportTo (cross-map) with destination ground probe via
// Map::GetHeight when the target map is already loaded. If the requested
// Z is significantly above the queried ground (>50y — the same threshold
// BotSetupPipeline used before this helper existed), snap to ground+2
// and WARN-log so the upstream data table can be fixed.
//
// Caller passes the same TELE_TO_* flag set they'd give to
// Player::TeleportTo (typically TELE_TO_GM_MODE for bots).
// Returns true if Player::TeleportTo returned true.
bool SafeTeleport(Player* p, uint32 map_id, float x, float y, float z, float o,
                  uint32 options);

// WorldLocation overload — convenience wrapper for callers that already
// have a packed location (BG entry point, GlobalStuckRescue homebind /
// capital fallback, owner-issued .playerbot teleport).
bool SafeTeleport(Player* p, WorldLocation const& loc, uint32 options);

// Unstick hop with obstacle-aware bearing selection. An unstick jump fired
// at the bot's current facing almost always launches INTO whatever wedged
// it (the bot jammed while walking toward its goal, so it's still pointing
// at the wall). Tries a fan of bearings — current facing, ±90°, then 180°
// (reverse out of a dead-end pocket) — and takes the first whose landing
// has real ground (SnapToGround) and a clear arc chord (LineIsClear). Faces
// the chosen bearing, then MoveJump `forward` yards. Returns true if a hop
// was launched; false if every bearing was blocked / groundless (caller
// should escalate recovery rather than launch into geometry).
bool TryUnstickJump(Player* p, float forward);

// Validate the bot's CURRENT position after a server-side teleport (BG
// port, accept_summon, hearth post-cast). If the bot is significantly
// off-mesh / sitting in geometry / floating in mid-air, SafeNearTeleport
// it onto the nearest valid surface. Cheap when the landing is fine
// (single PathGenerator probe).
void PostTeleportSnap(Player* p);

// Per-point liquid classification for the routing layer. Mirrors the WoW
// client's per-cell + depth liquid model:
// the destination POINT is probed (never the whole tile/chunk — a chunk is
// routinely half-water / half-dry), and the wade<->swim boundary is decided
// against the bot's REAL collision height (Unit::GetCollisionHeight()), so a
// tall / mounted bot correctly wades deeper than a gnome.
//
// The collision height is floored at DEFAULT_COLLISION_HEIGHT (2.03128) so the
// gate is never MORE restrictive than the historically-tuned default: a smaller
// collision height would flip shallow points to "submerged" and start refusing
// the shoreline wading that starter-zone quest hubs (Ratchet, Booty Bay, Echo
// Isles) depend on. Flooring keeps it at-least-as-permissive while letting
// taller bots wade deeper (more accurate, never stricter).
enum class LiquidVerdict : uint8
{
    Dry,       // no liquid at the point — plain walkable ground
    Wadeable,  // liquid present but the bot's head stays above it (wade / water-walk)
    Swim,      // bot would be fully submerged for its collision height (drowns)
    Hazard,    // magma or slime — lethal on contact
};

// Classify the liquid at destination point (x,y,z) for `p`. See LiquidVerdict.
// Optionally returns the raw LiquidData (level / depth_level / type_flags /
// entry) via `outData` for diagnostics. Returns Dry for a null player/map.
// This is the single source of truth for the routing layer's water policy;
// PlayerbotAPI::move_to and TryTerrainWalkFallback both go through it.
LiquidVerdict ClassifyDestinationLiquid(Player* p, float x, float y, float z,
                                        LiquidData* outData = nullptr);

// True when a verdict means the routing layer must NOT issue a ground
// MovePoint to the point (Swim or Hazard). Dry / Wadeable are passable.
constexpr bool IsLiquidImpassableForWalk(LiquidVerdict v)
{
    return v == LiquidVerdict::Swim || v == LiquidVerdict::Hazard;
}

} // namespace Playerbot::BotMovement

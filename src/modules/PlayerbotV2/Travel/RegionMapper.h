// RegionMapper — classifies a world position into a connected "region" within
// a map. Several WoW maps are ONE mapId but physically DISCONNECTED into
// multiple unreachable landmasses (you cannot walk between them):
//   - map 530 "Expansion01": Outland mainland / Azuremyst+Bloodmyst (Draenei) /
//     Eversong+Ghostlands (Blood Elf)
//   - map 1 Kalimdor: mainland / Teldrassil island (Night Elf)
// The travel graph keys reachability on mapId alone, so it treats "reach map
// 530" as one thing — and would route a Bloodmyst-bound bot through the
// Shattrath portal (lands in Outland, unreachable from Bloodmyst). Tagging each
// position with a region id lets the router target the CORRECT region's
// portal/dock (e.g. the Exodar portal, which lands on Azuremyst).
//
// Region id convention: 0 = the map's main/contiguous region (and every
// single-landmass map). >0 = a specific disconnected landmass within a split
// map. Two positions are mutually walkable only if same map AND same region id.
//
// The region table is DB-derived (creature/gameobject coordinate clusters) and
// covers the maps that are actually split; all other maps return 0. This is a
// pure, allocation-free coordinate lookup (a handful of bbox tests) — safe to
// call per-anchor at graph-build time and per-bot in the snapshot builder.

#pragma once

#include <cstdint>

namespace Playerbot::V2::Travel {

// Returns the region id for (map_id, x, y). 0 = main/global region.
uint32 RegionForPosition(uint32 map_id, float x, float y);

// True if the given map has more than one disconnected region (i.e. region ids
// other than 0 are possible). Lets callers skip region logic on normal maps.
bool MapHasMultipleRegions(uint32 map_id);

} // namespace Playerbot::V2::Travel

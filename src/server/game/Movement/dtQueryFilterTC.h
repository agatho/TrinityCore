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

#ifndef TRINITYCORE_DT_QUERY_FILTER_TC_H
#define TRINITYCORE_DT_QUERY_FILTER_TC_H

#include "Define.h"
#include "DetourNavMeshQuery.h"

#include <cstdint>
#include <vector>

// ----------------------------------------------------------------------------
// dtQueryFilterTC — TC's custom Detour cost filter.
//
// Extends the standard dtQueryFilter with two cost modifiers stacked on top
// of the base `dist * areaCost`:
//
//   1. Slope penalty — segments that climb steeply cost more.  A 30° slope
//      gets a 1.30× multiplier; 60° gets 1.60×.  Tunable via slope-multiplier
//      knob.  Reason: a road that switchbacks up a mountain isn't always
//      worth taking over a flat dirt cut; the slope term tempers the bare
//      area-cost preference.
//
//   2. Per-instance road-bonus disable — when a bot is in a context where
//      detouring for a road is wrong (e.g. tank pulling toward a mob, healer
//      following a moving raid), CallerSets DisableRoadBonus(true) → road
//      cost reverts to base 1.0 for THIS filter only.  Other bots' filters
//      keep the road preference.
//
// Inherits dtQueryFilter via public inheritance.  Override is getCost().
// AzerothCore's dtQueryFilterExt is the precedent for this pattern (slope
// penalty only; we add the road-disable bit on top).
// ----------------------------------------------------------------------------

class TC_GAME_API dtQueryFilterTC : public dtQueryFilter
{
public:
    dtQueryFilterTC();

    // Per-instance road-bonus disable. When true, getCost() ignores the
    // installed road areaCost and treats road polygons as base cost 1.0.
    // Other area costs are unaffected. Defaults to false (road preference
    // active).
    void SetDisableRoadBonus(bool disable) { _disableRoadBonus = disable; }
    bool GetDisableRoadBonus() const { return _disableRoadBonus; }

    // Slope penalty multiplier coefficient. Final slope cost factor is:
    //   1.0 + slopeDegrees * _slopeCoefficient / 100.0
    // Default 1.0 (AC-compatible). Set to 0.0 to disable slope penalty
    // entirely (back to plain dtQueryFilter semantics on slope).
    void  SetSlopeCoefficient(float c) { _slopeCoefficient = c; }
    float GetSlopeCoefficient() const  { return _slopeCoefficient; }

    // Override Detour's per-segment cost computation. Signature must match
    // dtQueryFilter::getCost exactly so the dynamic dispatch works.
    float getCost(float const* pa, float const* pb,
        dtPolyRef prevRef, dtMeshTile const* prevTile, dtPoly const* prevPoly,
        dtPolyRef curRef,  dtMeshTile const* curTile,  dtPoly const* curPoly,
        dtPolyRef nextRef, dtMeshTile const* nextTile, dtPoly const* nextPoly) const override;

    // Pure helper exposed for testing. Computes the slope multiplier
    // factor from a 3D segment (pa, pb).
    // Returns 1.0 for horizontal segments; > 1.0 for any positive slope.
    static float ComputeSlopeMultiplier(float const* pa, float const* pb,
                                         float slopeCoefficient);

    // ----- Per-pathfind telemetry (slope) -----------------------------------
    // Reset the max-slope tracker before issuing a pathfind. getCost()
    // updates _maxSlopeFactorThisPath internally; PathGenerator reads it
    // after CalculatePath and feeds it to TallyPath().
    void BeginPathStats() const;
    float GetMaxSlopeFactorThisPath() const { return _maxSlopeFactorThisPath; }

    // ----- Global road-aware-pathfinding telemetry --------------------------
    // Aggregated across all PathGenerator pathfinds running in this
    // worldserver process. Read via SampleStats(); reset via ResetStats().
    // Driven by TallyPath() which inspects the final path's polygons.
    struct RoadStats
    {
        uint64_t pathsRun = 0;                // total CalculatePath successes
        uint64_t pathsWithRoadPoly = 0;       // paths touching ≥1 NAV_AREA_ROAD
        uint64_t pathsRoadBonusDisabled = 0;  // paths where road bonus was disabled
        uint64_t pathsWithSlopePenalty = 0;   // paths with maxSlopeFactor > 1.10
        uint64_t roadPolysVisited = 0;        // sum of NAV_AREA_ROAD polys across all paths
        uint64_t totalPolysVisited = 0;       // sum of all polys across all paths
        uint64_t pathsInInstance = 0;         // paths on dungeon/raid maps (road bias auto-disabled)
    };
    static RoadStats SampleStats();
    static void ResetStats();

    // Sentinel for "no map id available" — used by callers that can't
    // resolve a map (shouldn't happen in normal PathGenerator flow but
    // exists for defensive null-safety). Map 0 is the legitimate WoW
    // Eastern Kingdoms map id and IS tracked per-map.
    static constexpr uint32 kNoMapId = 0xFFFFFFFFu;

    // Walk the resulting path polygons and tally road/total counts plus
    // the path-level outcome flags. Called by PathGenerator at the end
    // of CalculatePath. Safe to call with null mesh / empty path — those
    // still increment pathsRun.
    //
    // mapId is the source unit's map id. kNoMapId means "no per-map
    // tracking"; otherwise the path is tallied both globally AND into
    // the per-map table (accessed via SampleStatsForMap).
    static void TallyPath(class dtNavMesh const* mesh,
                          dtPolyRef const* polys, uint32 polyCount,
                          bool roadBonusDisabled,
                          float maxSlopeFactor,
                          bool inInstance,
                          uint32 mapId = kNoMapId);

    // Per-map stats — fragmented by source-unit map id at tally time.
    // Returns the stats for the named map, or a zero-initialised
    // RoadStats if no path has been tallied for that map.
    static RoadStats SampleStatsForMap(uint32 mapId);

    // List the map ids that have at least one tallied path. Used by
    // /roadstats listing mode to print a table of all known maps.
    static std::vector<uint32> ListMapsWithStats();

private:
    bool  _disableRoadBonus = false;
    float _slopeCoefficient = 1.0f;
    mutable float _maxSlopeFactorThisPath = 1.0f;  // updated by getCost(); reset by BeginPathStats()
};

#endif // TRINITYCORE_DT_QUERY_FILTER_TC_H

// RoadOverrides — operator-curated road waypoints loaded at mmaps_generator
// startup, applied to navmesh polygons by TileBuilder's road post-pass.
//
// Purpose: Bridge the gap between the texture-based road classifier and
// the actual roads that the classifier misses. The classic example is
// Teldrassil: the `tileset/kalidar/*road*` textures resolve in the
// listfile and the classifier's substring matcher tags them as road,
// but `RoadConfidenceFromEffectId() > 0.3` rejects them (the doodad-
// density signal labels Teldrassil's road MCNKs as "vegetated" because
// the cobblestone is rendered as a thin alpha-blended layer on top of
// ferns/wood ground). The result: 0 road bits in .road files for every
// Teldrassil tile despite road textures being referenced.
//
// Solution: a GM in-game uses `.playerbot meta add road` to capture
// waypoints along the actual visible road. The command writes both DB
// (characters.playerbot_v2_world_metadata) and an export CSV that
// mmaps_generator reads at regen time. For each waypoint, polygons
// whose centroid lies within `radius` get tagged NAV_AREA_ROAD — exactly
// the same area tag the texture classifier would have set.
//
// CSV format (one row per metadata point; lines starting with # ignored):
//   id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes
//
// mmaps_generator reads ONLY `kind=1` (Road) rows. Other kinds are
// ignored by the generator but persist in the same file because the
// editor exports the whole metadata table.
//
// Auto-discovery: the consumer looks for `<input>/world_metadata.csv`
// at startup (no CLI flag needed). Operators can also pass
// `--roadOverrides <path>` to point at a different file.

#pragma once

#include "Define.h"
#include <cstdint>
#include <string>
#include <vector>

namespace MMAP
{
    struct RoadOverridePoint
    {
        uint32 map_id;
        float  x;
        float  y;
        float  z;
        float  radius;
    };

    class RoadOverrides
    {
    public:
        static RoadOverrides& Instance();

        // Load from CSV. Returns count of road-kind rows loaded.
        // Returns 0 if the file is absent (not an error — overrides
        // are opt-in). Returns -1 on malformed file.
        int LoadFromFile(std::string const& csv_path);

        bool Empty() const { return road_points_.empty(); }
        size_t Size() const { return road_points_.size(); }

        // Filter to points within the bounding rect of a tile, expanded
        // by the maximum override radius so points just outside the
        // rect whose radius reaches into it are still considered. The
        // caller (TileBuilder) does per-polygon distance check.
        std::vector<RoadOverridePoint>
        PointsOverlappingTile(uint32 map_id,
                              float minX, float minZ,
                              float maxX, float maxZ) const;

    private:
        RoadOverrides() = default;
        RoadOverrides(RoadOverrides const&) = delete;
        RoadOverrides& operator=(RoadOverrides const&) = delete;

        // All kind=1 (Road) points; other kinds dropped on load.
        // Hundreds-of-points scale max, so a linear scan per tile is
        // fine (only runs at mmaps_generator regen, not at runtime).
        std::vector<RoadOverridePoint> road_points_;
    };
}

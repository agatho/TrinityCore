#include "RegionMapper.h"

namespace Playerbot::V2::Travel {

namespace {

// A disconnected landmass carve-out: positions inside the bbox on `map_id`
// belong to region `region_id`. Anything on a split map NOT matching any
// carve-out falls to region 0 (the mainland). bboxes are DB-derived from
// creature+gameobject coordinate clusters (see CROSS_MAP_TRAVEL_REBUILD.md /
// the region-geography research) with ~50-100u margin, verified non-overlapping.
struct RegionBox
{
    uint32 map_id;
    uint32 region_id;
    float  xmin, xmax, ymin, ymax;
};

constexpr RegionBox kRegionBoxes[] = {
    // --- Map 530 ("Expansion01") : Outland(0) / Azuremyst+Bloodmyst(1) / Eversong(2) ---
    // Azuremyst + Bloodmyst isles + Exodar (Draenei start, far south).
    // Landmarks: Exodar/Azuremyst ~(-4000,-11500), Bloodmyst ~(-4200,-13600),
    // Azuremyst↔Exodar boat dock (-4265,-11317).
    { 530, 1, -5450.f,  -850.f, -14200.f, -10500.f },
    // Eversong Woods + Ghostlands + Silvermoon (Blood Elf start, far NE).
    { 530, 2,  6250.f, 13400.f,  -8100.f,  -5350.f },

    // --- Map 1 (Kalimdor) : mainland(0) / Teldrassil(1) ---
    // Teldrassil island incl. Darnassus + Rut'theran Village dock (8181,1005).
    {   1, 1,  8000.f, 11100.f,    450.f,   2750.f },
};

} // namespace

uint32 RegionForPosition(uint32 map_id, float x, float y)
{
    for (RegionBox const& b : kRegionBoxes)
    {
        if (b.map_id != map_id) continue;
        if (x >= b.xmin && x <= b.xmax && y >= b.ymin && y <= b.ymax)
            return b.region_id;
    }
    return 0; // main/contiguous region (or a single-landmass map)
}

bool MapHasMultipleRegions(uint32 map_id)
{
    for (RegionBox const& b : kRegionBoxes)
        if (b.map_id == map_id)
            return true;
    return false;
}

} // namespace Playerbot::V2::Travel

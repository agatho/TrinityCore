#include "NavmeshPrewarm.h"
#include "CapitalsTable.h"
#include "DB2Stores.h"       // sBattlemasterListXMapStore — BG map id source
#include "MMapManager.h"     // isRebuildingTilesEnabledOnMap — per-instance-mesh gate
#include "MapManager.h"
#include "Map.h"
#include "Log.h"
#include "SharedDefines.h"   // RACE_* constants for kRaceProbes init
#include "RaceMask.h"        // RACE_HUMAN..RACE_BLOODELF enum values
#include "TerrainMgr.h"
#include <chrono>
#include <memory>
#include <set>
#include <vector>

namespace Playerbot::V2::World {

namespace {

// (map_id, x, y) of high-traffic bot zones. Capitals come from CapitalsTable.
// Class-starter maps + entry coords:
//   - Acherus       (map 609)  - DK starter, every Death Knight begins here.
//   - Mardum        (map 1481) - DH starter (Legion).
//   - Forbidden Reach (map 2444) - Evoker starter (Dragonflight).
struct PrewarmPoint
{
    uint32 map_id;
    float  x, y;
    char const* label;
};

constexpr PrewarmPoint kClassStarters[] = {
    { 609,  2380.0f,  -5901.0f, "Acherus (DK starter)" },
    { 1481, -1814.0f,   3791.0f, "Mardum (DH starter)" },
    { 2444, -2024.0f,   6618.0f, "Forbidden Reach (Evoker starter)" },
};

} // anonymous

void PrewarmCommonZones()
{
    using clock = std::chrono::steady_clock;
    auto t_start = clock::now();
    uint32 maps_loaded = 0;
    uint32 grids_loaded = 0;

    auto warm_one = [&](uint32 map_id, float x, float y, char const* label)
    {
        Map* m = sMapMgr->PrewarmContinentMap(map_id);
        if (!m)
        {
            TC_LOG_DEBUG("playerbot.v2",
                "[NavmeshPrewarm] skip {} (map {}): not a continent base map",
                label, map_id);
            return;
        }
        ++maps_loaded;
        // LoadGrid loads the 3x3 cell block centered on (x, y), which pulls
        // the underlying nav-tile + creature spawns + GO data into memory.
        m->LoadGrid(x, y);
        ++grids_loaded;
    };

    // Capitals: CapitalsTable has 8 entries (Stormwind/Ironforge/Darnassus/
    // Exodar + Orgrimmar/Thunder Bluff/Undercity/Silvermoon).
    constexpr uint32 kRaceProbes[] = {
        RACE_HUMAN, RACE_DWARF, RACE_NIGHTELF, RACE_DRAENEI,
        RACE_ORC, RACE_TAUREN, RACE_UNDEAD_PLAYER, RACE_BLOODELF,
    };
    for (uint32 race : kRaceProbes)
    {
        if (CapitalEntry const* cap = CapitalForRace(race))
            warm_one(cap->map_id, cap->x, cap->y, cap->name);
    }

    for (auto const& s : kClassStarters)
        warm_one(s.map_id, s.x, s.y, s.label);

    auto t_end = clock::now();
    auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    TC_LOG_INFO("playerbot.v2",
        "[NavmeshPrewarm] warmed {} maps / {} center grids in {} ms",
        maps_loaded, grids_loaded, dur_ms);
}

namespace {

// Held for the server's lifetime — each shared_ptr keeps its TerrainInfo
// alive (TerrainMgr stores weak_ptrs), and the grid references taken in
// PinBattlegroundTerrain keep every BG grid's terrain + vmap + mmap tile
// resident across battleground-map create/destroy churn.
std::vector<std::shared_ptr<TerrainInfo>> s_pinnedBgTerrain;

} // anonymous

void PinBattlegroundTerrain()
{
    using clock = std::chrono::steady_clock;
    auto t_start = clock::now();

    // Every map a battleground can run on, straight from the client data
    // that the BG system itself uses — covers brawls/variants/epics without
    // a hand-maintained list.
    std::set<uint32> bg_maps;
    for (BattlemasterListXMapEntry const* xmap : sBattlemasterListXMapStore)
        if (xmap->MapID >= 0)
            bg_maps.insert(uint32(xmap->MapID));

    uint32 maps_pinned = 0, grids_pinned = 0;
    for (uint32 map_id : bg_maps)
    {
        std::shared_ptr<TerrainInfo> terrain = sTerrainMgr.LoadTerrain(map_id);
        if (!terrain)
            continue;
        // Destructible-building BGs (SotA / IoC / Wintergrasp ...) use
        // PER-INSTANCE dynamic meshes keyed by instanceId — an instance-0
        // mmap pin would never be hit by a real match, just waste memory.
        // Their terrain + vmaps still pin below (those ARE shared).
        const bool shared_mesh =
            !MMAP::MMapManager::isRebuildingTilesEnabledOnMap(map_id);
        bool any = false;
        for (int32 gx = 0; gx < MAX_NUMBER_OF_GRIDS; ++gx)
        {
            for (int32 gy = 0; gy < MAX_NUMBER_OF_GRIDS; ++gy)
            {
                // Cheap existence probe (fopen + header check, instant
                // ENOENT for the ~97% of cells a BG map doesn't cover).
                if (!TerrainInfo::ExistMap(map_id, gx, gy, /*log*/ false))
                    continue;
                // Takes a grid reference we never release: terrain +
                // vmap tile load here, mmap tile right after (instance 0
                // mesh — all matches on a shared-mesh BG map use it).
                terrain->LoadMapAndVMap(gx, gy);
                if (shared_mesh)
                    terrain->LoadMMap(0, gx, gy);
                ++grids_pinned;
                any = true;
            }
        }
        if (any)
        {
            s_pinnedBgTerrain.push_back(std::move(terrain));
            ++maps_pinned;
        }
    }

    auto dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t_start).count();
    TC_LOG_INFO("playerbot.v2",
        "[NavmeshPrewarm] pinned {} battleground maps / {} grids resident in {} ms",
        maps_pinned, grids_pinned, dur_ms);
}

} // namespace Playerbot::V2::World

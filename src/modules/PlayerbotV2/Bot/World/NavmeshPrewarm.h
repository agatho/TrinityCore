// NavmeshPrewarm - Force-load grids + nav tiles for high-traffic bot zones
// at module init so the first bot teleporting in doesn't pay the cold-load
// cost on the map worker thread.
//
// Background: distribution-spawned bots cluster in a small set of maps
// (8 faction capitals + DK Acherus + DH Mardum + Evoker Forbidden Reach).
// When the population manager logs in many bots simultaneously, each one
// arriving in a fresh grid triggers a navmesh tile load on whichever map
// worker is processing that map's tick. With the original 20-logins-per-cycle
// throttle, this saturated single map workers and pushed tick time past the
// 60s freeze-detector limit.
//
// Pre-warming amortizes the cost into one batched server-startup load, so
// runtime spawns hit warm grids and avoid the cascade. Pairs with the
// kLoginRatePerTick=5 throttle (which we may raise back up after this lands).

#pragma once

namespace Playerbot::V2::World {

// Walk the capital + class-starter map list, force-load each map's center
// grids on the calling thread. Sync; takes ~1-3s of disk I/O on cold cache.
// Idempotent - already-loaded maps/grids are no-ops. Safe to call multiple
// times though only the first call does real work.
//
// Called from PlayerbotV2 module Initialize() after sMapMgr is ready.
void PrewarmCommonZones();

// Pin battleground terrain (grid maps + vmap tiles + mmap tiles) resident
// for the server's lifetime. BG maps are INSTANCED: when the last match on
// a map ends, the map's grid references drop to zero and TerrainMgr unloads
// every vmap/mmap tile — the next match then re-reads them from disk
// SYNCHRONOUSLY on map-update threads as 10-40 players port in at once.
// Since the 2026-06-12 nav regen those tiles are real (BG maps previously
// had NO vmaps at all), and the reload storms showed up as multi-second
// world-tick spikes (observed: update diff 4-10s around match boundaries
// with ~150 bots in BGs).
//
// Mechanism (zero core changes): TerrainMgr::LoadTerrain returns a
// shared_ptr<TerrainInfo> (the manager itself only holds weak_ptrs), and
// TerrainInfo::LoadMapAndVMap/LoadMMap take per-grid references that only
// unload at refcount zero. We acquire one reference per existing grid of
// every BattlemasterListXMap map id at boot and hold them forever.
//
// Called from PlayerbotV2 module Initialize(), after DB2 stores are loaded.
void PinBattlegroundTerrain();

} // namespace Playerbot::V2::World

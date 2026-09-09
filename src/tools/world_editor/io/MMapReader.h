/*
 * MMapReader - standalone loader for TrinityCore .mmap / .mmtile files.
 *
 * Mirrors the binary layout in src/common/mmaps_common/MMapDefines.h and
 * the read pattern used by src/tools/mmap_world_dump/mmap_world_dump.cpp
 * and src/tools/mmap_probe/mmap_probe.cpp. No worldserver runtime
 * dependency - the editor needs to load navmesh data for any map without
 * standing up a Map or MMapManager instance.
 *
 * Phase 0: declarations only. Phase 1 implements load() against Detour.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class dtNavMesh;

namespace world_editor::io
{

struct MMapLoadStats
{
    uint32_t tilesLoaded   = 0;
    uint32_t tilesFailed   = 0;
    uint32_t tilesSkipped  = 0;
    uint32_t mmapVersion   = 0;
    uint32_t dtVersion     = 0;
    uint64_t polyCount     = 0;
    uint64_t bytesLoaded   = 0;
};

// RAII handle for a loaded navmesh. Owns its dtNavMesh allocation and
// frees it via dtFreeNavMesh on destruction.
class LoadedMMap
{
public:
    LoadedMMap() = default;
    LoadedMMap(dtNavMesh* mesh, MMapLoadStats stats);
    LoadedMMap(LoadedMMap const&)            = delete;
    LoadedMMap& operator=(LoadedMMap const&) = delete;
    LoadedMMap(LoadedMMap&& other) noexcept;
    LoadedMMap& operator=(LoadedMMap&& other) noexcept;
    ~LoadedMMap();

    [[nodiscard]] dtNavMesh* navmesh() const noexcept           { return m_mesh; }
    [[nodiscard]] MMapLoadStats const& stats() const noexcept   { return m_stats; }
    [[nodiscard]] bool ok() const noexcept                      { return m_mesh != nullptr; }

private:
    dtNavMesh*    m_mesh  = nullptr;
    MMapLoadStats m_stats{};
};

// Load every .mmtile for mapId rooted at mmapsDir (the directory that
// contains "<mapId:04>.mmap"). Returns a LoadedMMap whose ok() reflects
// whether the .mmap header was readable; per-tile failures are tracked
// in stats(). On disk-missing returns a LoadedMMap with ok() == false.
[[nodiscard]] LoadedMMap loadMap(std::filesystem::path const& mmapsDir, uint32_t mapId);

// Format helpers ---------------------------------------------------------

[[nodiscard]] std::string mmapFilename(uint32_t mapId);
[[nodiscard]] std::string mmtileFilename(uint32_t mapId, int tx, int ty);

} // namespace world_editor::io

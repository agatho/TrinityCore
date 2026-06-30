/*
 * MapDb2Lookup - resolve MapId -> Map.Directory.
 *
 * Loads Map.db2 from CASC and caches an id->directory table that the
 * minimap loader uses to build CASC virtual paths
 * ("world/minimaps/<Directory>/map<gx>_<gy>.blp").
 *
 * Falls back to a hardcoded continent/instance table when CASC parsing
 * fails or no storage is available, so the editor still gets minimaps
 * for well-known maps in degraded-mode setups.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace world_editor::io
{

class CascClient;

// Per-map metadata pulled from Map.db2 (or the hardcoded fallback). Drives the
// grouped/searchable map picker.
struct MapMetadata
{
    static constexpr uint8_t kUnknown = 0xFF;

    uint32_t    mapId        = 0;
    std::string directory;                  // CASC minimap dir (lowercase)
    std::string name;                       // localized MapName (may be empty in fallback)
    uint8_t     instanceType = kUnknown;    // 0=world 1=dungeon 2=raid 3=bg 4=arena 5=scenario
    uint8_t     expansionId  = kUnknown;    // 0=Vanilla ... 10=War Within

    // Human label. Prefer the localized MapName, but ONLY when it's ASCII —
    // on a non-enUS client (e.g. koKR) the localized name is Korean/CJK, which
    // is useless in this English tool. In that case fall back to the always-
    // English, non-localized Directory (capitalized for readability).
    [[nodiscard]] std::string displayName() const
    {
        bool ascii = !name.empty();
        for (unsigned char c : name)
            if (c >= 0x80) { ascii = false; break; }
        if (ascii)
            return name;

        if (directory.empty())
            return std::string();
        std::string d = directory;
        if (!d.empty() && d[0] >= 'a' && d[0] <= 'z')
            d[0] = char(d[0] - 'a' + 'A');
        return d;
    }
};

class MapDb2Lookup
{
public:
    // Load Map.db2 from `casc`.  Returns false on failure; the hardcoded
    // fallback table is still populated either way so directoryFor()
    // continues to resolve well-known continent IDs.
    bool load(CascClient& casc);

    // Population without CASC.  Always populates the hardcoded table.
    void loadFallbackOnly();

    [[nodiscard]] std::optional<std::string> directoryFor(uint32_t mapId) const;

    // Full metadata for one map, or nullopt if unknown.
    [[nodiscard]] std::optional<MapMetadata> metadataFor(uint32_t mapId) const;
    // Every known map's metadata (unordered). Used to populate the map picker.
    [[nodiscard]] std::vector<MapMetadata> allMaps() const;

    [[nodiscard]] bool empty() const noexcept { return m_byMapId.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return m_byMapId.size(); }
    [[nodiscard]] std::string const& lastError() const { return m_lastError; }

private:
    void seedFallback();

    std::unordered_map<uint32_t, MapMetadata> m_byMapId;
    std::string                               m_lastError;
};

} // namespace world_editor::io

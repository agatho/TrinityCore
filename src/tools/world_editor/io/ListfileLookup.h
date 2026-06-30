/*
 * ListfileLookup - Qt-friendly façade around Road::ListfileMap.
 *
 * Modern WoW client data (TWW build 67186+) stores many minimap BLPs as
 * FileDataID-only entries — there is no virtual path in the CASC root
 * for `world/minimaps/<map>/map<col>_<row>.blp`, so a path-based
 * CascOpenFile() call returns ERR_FILE_NOT_FOUND every time.  The only
 * reliable lookup is FDID, sourced from the community wow-listfile
 * (https://github.com/wowdev/wow-listfile).
 *
 * The base parser already lives in extractor_common/ListfileMap; this
 * wrapper adds:
 *   - a QString + QSettings-friendly interface for the editor UI,
 *   - a *reverse* map (lowercased forward-slash-normalized path → FDID)
 *     so the minimap loader can take its existing candidate-paths list
 *     and find the FDID without learning a new key,
 *   - structural diagnostics (entry count, last-load error).
 *
 * Memory footprint matches Road::ListfileMap (~150 MB for the full 2.17M
 * entries).  Operators wanting cheaper loads should pre-filter the CSV
 * to `world/minimaps/` before pointing the editor at it.
 */

#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace Road { class ListfileMap; }

namespace world_editor::io
{

class ListfileLookup
{
public:
    ListfileLookup();
    ~ListfileLookup();

    ListfileLookup(ListfileLookup const&) = delete;
    ListfileLookup& operator=(ListfileLookup const&) = delete;

    // Parse a CSV file on disk into both the forward (fdid → path) and
    // reverse (path → fdid) tables.  Returns false on open failure;
    // partial parse failures (malformed lines) succeed.  outError is
    // filled with a human-readable summary when the return is false.
    bool loadFromFile(QString const& csvPath, QString* outError = nullptr);

    // Same as loadFromFile but takes the raw CSV contents in-memory.
    // Used by the smoketest harness to avoid touching disk.
    void loadFromString(std::string_view content);

    // Drop both tables.
    void clear();

    // Lookup a forward-slash-normalized, case-insensitive path.  Returns
    // nullopt when the lookup misses (or when no listfile is loaded).
    [[nodiscard]] std::optional<uint32_t> resolveFdid(QString const& vpath) const;
    [[nodiscard]] std::optional<uint32_t> resolveFdid(std::string_view vpath) const;

    // Reverse: FDID → path.  Returns empty string on miss; QString to
    // match the UI surface (the diagnostics dock surfaces this).
    [[nodiscard]] QString pathFor(uint32_t fdid) const;

    [[nodiscard]] std::size_t entryCount() const noexcept { return m_pathToFdid.size(); }
    [[nodiscard]] bool        empty()      const noexcept { return m_pathToFdid.empty(); }
    [[nodiscard]] QString const& loadedPath() const noexcept { return m_loadedPath; }

private:
    // Normalize: lowercase, backslash→forward, trim.  Returned by value
    // (callers use it as the unordered_map key).
    [[nodiscard]] static std::string normalize(std::string_view in);

    // The forward map lives in Road::ListfileMap; we keep it heap-owned
    // so the header doesn't drag the implementation in.
    std::unique_ptr<Road::ListfileMap>     m_forward;
    std::unordered_map<std::string, uint32_t> m_pathToFdid;
    QString                                 m_loadedPath;
};

} // namespace world_editor::io

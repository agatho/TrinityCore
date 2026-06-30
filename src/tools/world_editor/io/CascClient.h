/*
 * CascClient - thin wrapper around TC's CASC::Storage for the editor.
 *
 * Hides the boost::filesystem::path / raw HANDLE plumbing behind a
 * std::string + std::vector<uint8_t> surface so the world_editor's
 * Qt-side code can read from the live WoW client install without
 * pulling boost or CascLib into every translation unit.
 *
 * Auto-resolves the product subdirectory (_retail_, _classic_, _ptr_)
 * when given a WoW install root; if the caller has already pointed
 * at a flavor subdir, it's used directly.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace CASC { class Storage; }

namespace world_editor::io
{

class ListfileLookup;

class CascClient
{
public:
    // 0x0001F3F6 - matches CASC_LOCALE_ALL_WOW so the default works for
    // any installed locale.  We pull the value as a literal here so the
    // header doesn't have to drag in CascPort.h.
    static constexpr uint32_t kLocaleAllWow = 0x0001F3F6u;

    CascClient();
    ~CascClient();

    CascClient(CascClient const&) = delete;
    CascClient& operator=(CascClient const&) = delete;

    // Opens the storage.  `path` may be either the install root
    // ("C:/World of Warcraft") or a flavor subdir
    // ("C:/World of Warcraft/_retail_").  Returns false on failure.
    bool open(std::string const& path, uint32_t localeMask = kLocaleAllWow);
    void close();
    [[nodiscard]] bool isOpen() const noexcept { return m_storage != nullptr; }

    // Underlying storage handle for code that needs to construct a
    // DB2CascFileSource against it.  Returns nullptr when not open.
    [[nodiscard]] std::shared_ptr<CASC::Storage> storage() const { return m_storage; }

    // Read a file by virtual path (e.g. "world/minimaps/azeroth/map32_48.blp").
    // When the path is not in CASC's internal root catalog (modern builds
    // store many files as FileDataID-only entries with no recorded path),
    // and a listfile has been attached via setListfile(), the call falls
    // back to listfile.resolveFdid(vpath) -> openByFileDataId(fdid).
    [[nodiscard]] bool readByPath(std::string const& vpath, std::vector<uint8_t>& out);
    [[nodiscard]] bool readByFileDataId(uint32_t fdid, std::vector<uint8_t>& out);

    // Attach a community listfile for the path -> FDID fallback inside
    // readByPath().  The pointer is non-owning and must outlive this
    // client; pass nullptr to detach.
    void setListfile(ListfileLookup const* lf) noexcept { m_listfile = lf; }

    // FDID open helper. Modern (TWW build 67186+) WoW data is FileDataID-
    // only — no virtual path string is stored in the CASC root for many
    // minimap BLPs — so the only reliable lookup is FDID via a community
    // listfile. Identical to readByFileDataId; kept as a named alias to
    // make caller intent obvious at call sites.
    [[nodiscard]] bool openByFileDataId(uint32_t fdid, std::vector<uint8_t>& out) const;

    // Convenience wrapper; identical semantics to isOpen(). Some callers
    // prefer the shorter spelling.
    [[nodiscard]] bool ready() const noexcept { return isOpen(); }

    // Walk the CASC root catalog and return file names that begin with
    // the lowercase `prefix` (case-insensitive comparison).  Capped at
    // `max` entries to keep memory + UI cost bounded.  Returns the
    // discovered paths in catalog order (no sort).  Empty vector on
    // miss or when storage is closed.
    [[nodiscard]] std::vector<std::string> listFiles(std::string const& prefix, std::size_t max = 50);

    // Last human-readable error from CascLib (empty when none).
    [[nodiscard]] std::string const& lastError() const { return m_lastError; }

private:
    bool readFromHandle(void* fileHandlePtr, std::vector<uint8_t>& out);

    std::shared_ptr<CASC::Storage> m_storage;
    uint32_t                       m_localeMask = kLocaleAllWow;
    std::string                    m_lastError;
    ListfileLookup const*          m_listfile = nullptr;
};

} // namespace world_editor::io

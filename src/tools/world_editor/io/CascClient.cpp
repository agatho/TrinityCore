/*
 * CascClient - implementation.
 */

#include "CascClient.h"

#include "CascHandles.h"
#include "ListfileLookup.h"

#include <QDebug>

#include <CascLib.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <memory>

namespace world_editor::io
{

namespace
{

// Try a list of product strings against the storage root.  Returns the
// first one that opens; storage out-param is filled on success.
CASC::Storage* TryOpen(boost::filesystem::path const& root, uint32_t localeMask)
{
    // Order matches the order most operators install; retail first.
    char const* products[] = { "wow", "wowt", "wow_beta", "wow_classic", "wow_classic_era" };
    for (char const* product : products)
    {
        if (CASC::Storage* s = CASC::Storage::Open(root, localeMask, product))
            return s;
    }
    return nullptr;
}

// If `path` is an install root containing flavor subdirs (_retail_,
// _classic_, _ptr_), return the first existing flavor subdir.  If the
// caller already pointed at a flavor subdir (i.e. it directly contains
// .build.info), the original path is returned.  Returns an empty path
// when nothing recognizable was found.
boost::filesystem::path ResolveStorageRoot(boost::filesystem::path const& input)
{
    namespace fs = boost::filesystem;
    if (input.empty())
        return {};

    // Direct hit: install flavor dir already.
    if (fs::exists(input / ".build.info"))
        return input;

    char const* flavors[] = { "_retail_", "_classic_", "_classic_era_", "_ptr_", "_beta_" };
    for (char const* flavor : flavors)
    {
        fs::path candidate = input / flavor;
        if (fs::exists(candidate / ".build.info"))
            return candidate;
    }

    // Fall back to letting CascLib try the raw path; user might point at
    // a fully custom layout.
    return input;
}

} // namespace

CascClient::CascClient() = default;

CascClient::~CascClient()
{
    close();
}

bool CascClient::open(std::string const& path, uint32_t localeMask)
{
    close();
    m_lastError.clear();
    m_localeMask = localeMask;

    boost::filesystem::path const root = ResolveStorageRoot(boost::filesystem::path(path));
    if (root.empty())
    {
        m_lastError = "Empty CASC path.";
        return false;
    }

    CASC::Storage* raw = TryOpen(root, localeMask);
    if (!raw)
    {
        m_lastError = std::string("CASC open failed: ")
                      + CASC::HumanReadableCASCError(GetCascError())
                      + " (path=" + root.string() + ")";
        return false;
    }
    m_storage = std::shared_ptr<CASC::Storage>(raw);
    return true;
}

void CascClient::close()
{
    m_storage.reset();
}

bool CascClient::readFromHandle(void* fileHandlePtr, std::vector<uint8_t>& out)
{
    auto* file = static_cast<CASC::File*>(fileHandlePtr);
    if (!file)
        return false;

    int64 const sizeSigned = file->GetSize();
    if (sizeSigned <= 0)
    {
        delete file;
        out.clear();
        return sizeSigned == 0; // 0-byte files are technically valid.
    }
    out.resize(std::size_t(sizeSigned));
    uint32 bytesRead = 0;
    bool const ok = file->ReadFile(out.data(), uint32(sizeSigned), &bytesRead)
                    && bytesRead == uint32(sizeSigned);
    delete file;
    if (!ok)
    {
        m_lastError = std::string("CASC read failed: ")
                      + CASC::HumanReadableCASCError(GetCascError());
        out.clear();
        return false;
    }
    return true;
}

bool CascClient::readByPath(std::string const& vpath, std::vector<uint8_t>& out)
{
    if (!m_storage)
    {
        m_lastError = "CASC storage not open.";
        return false;
    }
    CASC::File* file = m_storage->OpenFile(vpath.c_str(), m_localeMask, /*printErrors=*/false);
    if (file)
        return readFromHandle(file, out);

    // CASC's internal root catalog does not list this vpath.  Modern
    // builds (TWW 67186+) store many files (ADTs, BLPs) as FileDataID-
    // only entries with no recorded path string; the only way to reach
    // them is via FDID.  Resolve the path through the community listfile
    // (when one has been attached) and retry as an FDID open.
    if (m_listfile)
    {
        if (auto fdidOpt = m_listfile->resolveFdid(vpath))
        {
            bool const ok = readByFileDataId(*fdidOpt, out);
            // Only log the WDT path -- it's a one-shot per map switch and
            // pinpoints whether the fallback is actually being exercised.
            // ADT lookups are far too noisy for an INFO-level line.
            if (vpath.size() >= 4 && vpath.compare(vpath.size() - 4, 4, ".wdt") == 0)
                qInfo("[CascClient] WDT path '%s' resolved via listfile FDID=%u (open ok=%d)\n",
                    vpath.c_str(), unsigned(*fdidOpt), ok ? 1 : 0);
            return ok;
        }
        else if (vpath.size() >= 4 && vpath.compare(vpath.size() - 4, 4, ".wdt") == 0)
        {
            qInfo("[CascClient] WDT path '%s' NOT in listfile (path-catalog miss, no FDID fallback)\n",
                vpath.c_str());
        }
    }
    else if (vpath.size() >= 4 && vpath.compare(vpath.size() - 4, 4, ".wdt") == 0)
    {
        qInfo("[CascClient] WDT path '%s' attempted but m_listfile is NULL\n",
            vpath.c_str());
    }
    return false;
}

bool CascClient::readByFileDataId(uint32_t fdid, std::vector<uint8_t>& out)
{
    if (!m_storage)
    {
        m_lastError = "CASC storage not open.";
        return false;
    }
    CASC::File* file = m_storage->OpenFile(fdid, m_localeMask, /*printErrors=*/false);
    if (!file)
        return false;
    return readFromHandle(file, out);
}

bool CascClient::openByFileDataId(uint32_t fdid, std::vector<uint8_t>& out) const
{
    // Named alias for readByFileDataId; const-faced so callers iterating
    // a candidate-FDID list from a read-only ListfileLookup ref don't
    // have to drop the const.  Internally we still need to mutate the
    // last-error string when the storage handle is missing, so cast it
    // through a non-const this just for that single call.
    return const_cast<CascClient*>(this)->readByFileDataId(fdid, out);
}

namespace
{

// Lowercase + forward-slash-normalize for case-insensitive prefix match.
std::string NormalizePath(std::string s)
{
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';
        else if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    }
    return s;
}

} // namespace

std::vector<std::string> CascClient::listFiles(std::string const& prefix, std::size_t max)
{
    std::vector<std::string> results;
    if (!m_storage || max == 0)
        return results;

    HANDLE hStorage = m_storage->GetHandle();
    if (!hStorage)
        return results;

    // "*" mask matches every entry; we do the prefix filter ourselves
    // so callers get an exact-match feel without learning CascLib's
    // wildcard semantics.
    CASC_FIND_DATA fd = {};
    HANDLE hFind = CascFindFirstFile(hStorage, "*", &fd, nullptr);
    if (!hFind)
        return results;

    std::string const wanted = NormalizePath(prefix);
    results.reserve(max);
    do
    {
        std::string name = NormalizePath(fd.szFileName);
        if (name.size() >= wanted.size() && name.compare(0, wanted.size(), wanted) == 0)
        {
            results.push_back(std::move(name));
            if (results.size() >= max)
                break;
        }
    } while (CascFindNextFile(hFind, &fd));

    CascFindClose(hFind);
    return results;
}

} // namespace world_editor::io

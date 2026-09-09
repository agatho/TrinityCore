/*
 * ListfileLookup - implementation.
 *
 * Defers CSV parsing to Road::ListfileMap and tees every successful
 * insert into the reverse map (lowercased + forward-slash-normalized
 * path → FDID).  We can't piggyback Road::ListfileMap's parser hook
 * because it doesn't expose one, so the reverse table is rebuilt by
 * iterating the parsed forward map after the load finishes.  The cost
 * is one extra O(N) walk per file load; the editor only loads on
 * operator request so this is fine.
 */

#include "ListfileLookup.h"

#include "ListfileMap.h"

#include <QFile>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace world_editor::io
{

ListfileLookup::ListfileLookup()
    : m_forward(std::make_unique<Road::ListfileMap>())
{
}

ListfileLookup::~ListfileLookup() = default;

std::string ListfileLookup::normalize(std::string_view in)
{
    std::string out;
    out.reserve(in.size());
    // Skip leading whitespace; Road::ListfileMap doesn't trim aggressively.
    std::size_t b = 0;
    while (b < in.size() && (in[b] == ' ' || in[b] == '\t')) ++b;
    std::size_t e = in.size();
    while (e > b && (in[e - 1] == ' ' || in[e - 1] == '\t' ||
                     in[e - 1] == '\r' || in[e - 1] == '\n')) --e;
    for (std::size_t i = b; i < e; ++i)
    {
        char c = in[i];
        if (c == '\\')      c = '/';
        else if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        out.push_back(c);
    }
    return out;
}

bool ListfileLookup::loadFromFile(QString const& csvPath, QString* outError)
{
    clear();
    m_loadedPath = csvPath;
    QFileInfo info(csvPath);
    if (!info.exists() || !info.isReadable())
    {
        if (outError)
            *outError = QStringLiteral("Listfile '%1' is missing or unreadable.").arg(csvPath);
        return false;
    }
    std::ifstream f(csvPath.toStdString(), std::ios::binary);
    if (!f)
    {
        if (outError)
            *outError = QStringLiteral("Failed to open listfile '%1'.").arg(csvPath);
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    loadFromString(ss.str());
    if (m_pathToFdid.empty() && outError)
        *outError = QStringLiteral("Listfile '%1' parsed 0 entries (wrong format?).").arg(csvPath);
    return true;
}

void ListfileLookup::loadFromString(std::string_view content)
{
    // Hand off the raw text to Road::ListfileMap and then mirror the
    // populated forward map into our reverse table.  We can't call
    // ParseFromString a second time without re-reading the buffer, so
    // we walk it ourselves once just to harvest the (fdid, path) pairs
    // — cheaper than asking Road::ListfileMap to expose its internal
    // _entries map.
    std::size_t pos = 0;
    while (pos < content.size())
    {
        std::size_t end = content.find('\n', pos);
        std::string_view rawLine = (end == std::string_view::npos)
            ? content.substr(pos)
            : content.substr(pos, end - pos);
        pos = (end == std::string_view::npos) ? content.size() : end + 1;

        // Trim CR/LF/whitespace + skip comments + empty.
        std::size_t b = 0;
        while (b < rawLine.size() && (rawLine[b] == ' ' || rawLine[b] == '\t')) ++b;
        std::size_t e = rawLine.size();
        while (e > b && (rawLine[e - 1] == ' ' || rawLine[e - 1] == '\t' ||
                          rawLine[e - 1] == '\r' || rawLine[e - 1] == '\n')) --e;
        if (b == e || rawLine[b] == '#') continue;
        std::string_view line = rawLine.substr(b, e - b);

        // Find first ',' or ';'.
        std::size_t sep = std::string_view::npos;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == ',' || line[i] == ';') { sep = i; break; }
        }
        if (sep == std::string_view::npos) continue;

        std::string_view idStr = line.substr(0, sep);
        std::string_view path  = line.substr(sep + 1);
        // Strip surrounding whitespace on path/idStr.
        auto trim = [](std::string_view& s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
            while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t')) s.remove_suffix(1);
        };
        trim(idStr); trim(path);
        if (idStr.empty() || path.empty()) continue;

        uint32_t id = 0;
        bool ok = true;
        for (char c : idStr)
        {
            if (c < '0' || c > '9') { ok = false; break; }
            id = id * 10u + uint32_t(c - '0');
        }
        if (!ok) continue;

        std::string normalized = normalize(path);
        m_forward->Insert(id, std::string(path));
        m_pathToFdid[normalized] = id;
    }
}

void ListfileLookup::clear()
{
    if (m_forward) m_forward->Clear();
    m_pathToFdid.clear();
    m_loadedPath.clear();
}

std::optional<uint32_t> ListfileLookup::resolveFdid(QString const& vpath) const
{
    QByteArray const utf8 = vpath.toUtf8();
    return resolveFdid(std::string_view(utf8.constData(), std::size_t(utf8.size())));
}

std::optional<uint32_t> ListfileLookup::resolveFdid(std::string_view vpath) const
{
    if (m_pathToFdid.empty()) return std::nullopt;
    std::string const key = normalize(vpath);
    auto it = m_pathToFdid.find(key);
    if (it == m_pathToFdid.end()) return std::nullopt;
    return it->second;
}

QString ListfileLookup::pathFor(uint32_t fdid) const
{
    if (!m_forward) return {};
    auto v = m_forward->Lookup(fdid);
    if (!v) return {};
    return QString::fromUtf8(v->data(), int(v->size()));
}

} // namespace world_editor::io

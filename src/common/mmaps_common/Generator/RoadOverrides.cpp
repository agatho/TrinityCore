#include "RoadOverrides.h"

#include "Log.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace MMAP
{

RoadOverrides& RoadOverrides::Instance()
{
    static RoadOverrides inst;
    return inst;
}

namespace
{
    // CSV parser tolerant of:
    //   * blank lines
    //   * #-prefixed comment lines
    //   * trailing whitespace
    //   * quoted fields containing commas (rare — operator labels)
    // Returns false on a record whose required fields can't be parsed.
    // Records with kind != 1 are skipped silently (caller observes via
    // non-incrementing count).
    bool ParseLine(std::string const& line, RoadOverridePoint& out, uint32& kind_out)
    {
        // Skip blank / comment
        size_t i = 0;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i >= line.size() || line[i] == '#') return false;

        // Field order (from `.playerbot meta export`):
        // id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes
        std::vector<std::string> fields;
        std::string cur;
        bool in_quote = false;
        for (size_t p = i; p < line.size(); ++p)
        {
            char c = line[p];
            if (in_quote)
            {
                if (c == '"') in_quote = false;
                else cur.push_back(c);
            }
            else if (c == '"')
                in_quote = true;
            else if (c == ',')
            { fields.push_back(std::move(cur)); cur.clear(); }
            else
                cur.push_back(c);
        }
        fields.push_back(std::move(cur));

        // Need at least 9 fields (through radius). label/notes optional.
        if (fields.size() < 9) return false;

        // Use strtoull / strtod for resilience to whitespace.
        // id is fields[0] — not stored on out (we use the cache).
        out.map_id = static_cast<uint32>(std::strtoul(fields[1].c_str(), nullptr, 10));
        // zone_id (fields[2]) ignored.
        kind_out   = static_cast<uint32>(std::strtoul(fields[3].c_str(), nullptr, 10));
        // kind_name (fields[4]) ignored (redundant with kind).
        out.x      = static_cast<float>(std::strtod(fields[5].c_str(), nullptr));
        out.y      = static_cast<float>(std::strtod(fields[6].c_str(), nullptr));
        out.z      = static_cast<float>(std::strtod(fields[7].c_str(), nullptr));
        out.radius = static_cast<float>(std::strtod(fields[8].c_str(), nullptr));
        if (out.radius <= 0.0f) out.radius = 10.0f;
        return true;
    }
}

int RoadOverrides::LoadFromFile(std::string const& csv_path)
{
    std::ifstream in(csv_path);
    if (!in)
    {
        // Not an error — file is opt-in
        TC_LOG_INFO("maps.mmapgen",
            "[RoadOverrides] no override CSV at '{}'; skipping",
            csv_path);
        return 0;
    }
    road_points_.clear();
    road_points_.reserve(256);
    std::string line;
    int loaded   = 0;
    int skipped  = 0;
    int malformed = 0;
    while (std::getline(in, line))
    {
        RoadOverridePoint p{};
        uint32 kind = 0;
        if (!ParseLine(line, p, kind))
        {
            // ParseLine returns false for blank / comment / malformed.
            // We can't distinguish blanks from malformed without
            // pre-trimming — so we accept the noise; only complain
            // visibly if a row WITH a kind=1 prefix fails. Cheap.
            if (!line.empty() && line[0] != '#')
                ++malformed;
            continue;
        }
        if (kind != 1) { ++skipped; continue; }
        road_points_.push_back(p);
        ++loaded;
    }
    TC_LOG_INFO("maps.mmapgen",
        "[RoadOverrides] loaded {} road-kind row(s) from {} "
        "({} non-road kinds skipped, {} malformed)",
        loaded, csv_path, skipped, malformed);
    return loaded;
}

std::vector<RoadOverridePoint>
RoadOverrides::PointsOverlappingTile(uint32 map_id,
                                     float minX, float minZ,
                                     float maxX, float maxZ) const
{
    std::vector<RoadOverridePoint> out;
    if (road_points_.empty()) return out;
    // The tile bbox in TC navmesh coords: minX/maxX bound world X,
    // minZ/maxZ bound world Z (== world Y in TC's name conventions).
    // The override point's (x, y) are in TC world XY. We need to
    // compare TC-X against minX..maxX and TC-Y against minZ..maxZ.
    // Expand the rect by the override's radius so points just outside
    // whose circle reaches in still get returned.
    out.reserve(16);
    for (auto const& p : road_points_)
    {
        if (p.map_id != map_id) continue;
        const float r = p.radius;
        if (p.x < minX - r || p.x > maxX + r) continue;
        if (p.y < minZ - r || p.y > maxZ + r) continue;
        out.push_back(p);
    }
    return out;
}

} // namespace MMAP

/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// road_stats
//
// Scans a maps/ directory for parallel .road files (produced by the
// road-aware mapextractor), reports overall counts and per-map breakdowns
// so the owner can verify the extraction pipeline produced sensible data.
//
// Usage:
//   road_stats --maps <dir>           (default: ./maps)
//              [--map-id <N>]         (filter to one map)
//              [--show-empty]         (also list zero-mask files)
//
// Output:
//   total: 9512 .road files, 142306 road MCNKs (avg 14.96/file)
//   per-map:
//     0000  Azeroth          64 files  1234 MCNKs  (19.28/file)
//     0001  Kalimdor         64 files   987 MCNKs  (15.42/file)
//     ...
//   distribution (MCNKs per ADT):
//      0:  234 files
//      1-3:  502 files
//      4-10: 1820 files
//     11-30: 3001 files
//     31+:  3955 files

#include "RoadMapDefines.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct Options
    {
        std::string mapsDir = "maps";
        int  mapIdFilter = -1;
        bool showEmpty = false;
    };

    [[noreturn]] void Usage(int code)
    {
        std::fprintf(stderr,
            "road_stats — inspect .road files produced by mapextractor\n"
            "\n"
            "Usage:\n"
            "  road_stats [--maps <dir>] [--map-id N] [--show-empty]\n"
            "\n"
            "Defaults:\n"
            "  --maps      ./maps\n"
            "  --map-id    -1 (all maps)\n"
            "  --show-empty: list ADTs with zero road MCNKs (otherwise omitted)\n");
        std::exit(code);
    }

    Options ParseArgs(int argc, char** argv)
    {
        Options o;
        for (int i = 1; i < argc; ++i)
        {
            std::string_view a = argv[i];
            auto wantVal = [&]() -> std::string {
                if (i + 1 >= argc) Usage(1);
                return argv[++i];
            };
            if (a == "--help" || a == "-h") Usage(0);
            else if (a == "--maps")        o.mapsDir = wantVal();
            else if (a == "--map-id")      o.mapIdFilter = std::atoi(wantVal().c_str());
            else if (a == "--show-empty")  o.showEmpty = true;
            else { std::fprintf(stderr, "unknown arg: %s\n", argv[i]); Usage(1); }
        }
        return o;
    }

    struct RoadFileSummary
    {
        uint32 mapId      = 0;
        uint32 adtX       = 0;
        uint32 adtY       = 0;
        uint32 roadMcnks  = 0;
        bool   parseError = false;
    };

    bool ReadRoadFile(std::filesystem::path const& path, RoadFileSummary& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        TrinityCore::RoadMap::RoadFileHeader header;
        f.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!f) return false;
        if (header.magic != TrinityCore::RoadMap::kRoadFileMagic)
        {
            out.parseError = true;
            return false;
        }

        TrinityCore::RoadMap::RoadMaskGrid mask;
        f.read(reinterpret_cast<char*>(mask.data()), mask.size());
        if (!f) return false;

        out.mapId = header.mapId;
        out.adtX  = header.adtX;
        out.adtY  = header.adtY;
        uint32 n = 0;
        for (uint8 v : mask) if (v != 0) ++n;
        out.roadMcnks = n;
        return true;
    }

    // Parse a "NNNN_YY_XX.road" filename. Returns false if pattern doesn't
    // match. The filename convention mirrors .map's <mapId>_<y>_<x>.
    bool ParseRoadFilename(std::string const& name,
                            uint32& mapId, uint32& y, uint32& x)
    {
        // Expected format: NNNN_YY_XX.road (compact: NNNNYYXX.road)
        // MapMakeRoadFilePath uses the compact form: <mapId:04>_<y:02>_<x:02>.road
        // … actually it's <mapId:04>_<adtX:02>_<adtY:02>.road in my code. Let me
        // be permissive — try both.
        if (name.size() < strlen("00000000.road")) return false;
        if (name.size() < 5) return false;
        if (name.substr(name.size() - 5) != ".road") return false;

        // Strip extension.
        std::string base = name.substr(0, name.size() - 5);

        // Underscore-separated form: NNNN_YY_XX (matches .map convention)
        if (base.size() == 10 && base[4] == '_' && base[7] == '_')
        {
            std::string mapStr = base.substr(0, 4);
            std::string xStr   = base.substr(5, 2);
            std::string yStr   = base.substr(8, 2);
            if (std::all_of(mapStr.begin(), mapStr.end(),
                            [](char c) { return c >= '0' && c <= '9'; }) &&
                std::all_of(xStr.begin(), xStr.end(),
                            [](char c) { return c >= '0' && c <= '9'; }) &&
                std::all_of(yStr.begin(), yStr.end(),
                            [](char c) { return c >= '0' && c <= '9'; }))
            {
                mapId = static_cast<uint32>(std::atoi(mapStr.c_str()));
                x = static_cast<uint32>(std::atoi(xStr.c_str()));
                y = static_cast<uint32>(std::atoi(yStr.c_str()));
                return true;
            }
        }

        // Legacy compact 8-digit form: NNNNXXYY (for files generated by
        // earlier pre-fix builds — accepted on read so road_stats can
        // inspect old output).
        if (base.size() == 8 &&
            std::all_of(base.begin(), base.end(),
                        [](char c) { return c >= '0' && c <= '9'; }))
        {
            mapId = static_cast<uint32>(std::atoi(base.substr(0, 4).c_str()));
            x = static_cast<uint32>(std::atoi(base.substr(4, 2).c_str()));
            y = static_cast<uint32>(std::atoi(base.substr(6, 2).c_str()));
            return true;
        }

        return false;
    }

    char const* MapName(uint32 id)
    {
        switch (id)
        {
            case 0: return "Eastern Kingdoms";
            case 1: return "Kalimdor";
            case 530: return "Outland";
            case 571: return "Northrend";
            case 609: return "Ebon Hold";
            case 870: return "Pandaria";
            case 1116: return "Draenor";
            case 1220: return "Broken Isles";
            case 1643: return "Kul Tiras";
            case 1642: return "Zandalar";
            case 1669: return "Argus (Krokuun)";
            case 2222: return "The Maw";
            case 2444: return "Dragon Isles";
            case 2552: return "Khaz Algar";
            default: return "(unknown)";
        }
    }
}

int main(int argc, char** argv)
{
    Options opts = ParseArgs(argc, argv);

    std::filesystem::path dir(opts.mapsDir);
    if (!std::filesystem::is_directory(dir))
    {
        std::fprintf(stderr, "Not a directory: %s\n", opts.mapsDir.c_str());
        return 2;
    }

    std::vector<RoadFileSummary> all;
    std::size_t parseErrors = 0;

    for (auto const& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.size() < 5 || name.substr(name.size() - 5) != ".road") continue;

        uint32 mapId = 0, adtX = 0, adtY = 0;
        if (!ParseRoadFilename(name, mapId, adtY, adtX)) continue;
        if (opts.mapIdFilter >= 0 && static_cast<int>(mapId) != opts.mapIdFilter)
            continue;

        RoadFileSummary s;
        if (!ReadRoadFile(entry.path(), s))
        {
            if (s.parseError) ++parseErrors;
            continue;
        }
        all.push_back(s);
    }

    if (all.empty())
    {
        std::printf("No .road files found in %s%s\n",
                    opts.mapsDir.c_str(),
                    opts.mapIdFilter >= 0 ?
                      (" (filter: map " + std::to_string(opts.mapIdFilter) + ")").c_str()
                      : "");
        if (parseErrors > 0)
            std::printf("%zu files had bad magic.\n", parseErrors);
        return 0;
    }

    // Per-map aggregation.
    struct MapAgg { uint32 files = 0; uint32 totalMcnks = 0; };
    std::map<uint32, MapAgg> byMap;
    uint32 totalMcnks = 0;
    for (auto const& s : all)
    {
        auto& a = byMap[s.mapId];
        ++a.files;
        a.totalMcnks += s.roadMcnks;
        totalMcnks += s.roadMcnks;
    }

    std::printf("total: %zu .road files, %u road MCNKs (avg %.2f/file)\n",
                all.size(), totalMcnks,
                all.empty() ? 0.0 : double(totalMcnks) / all.size());

    if (parseErrors > 0)
        std::printf("(note: %zu files had bad magic — possible corruption)\n",
                    parseErrors);

    std::printf("\nper-map:\n");
    for (auto const& [mapId, agg] : byMap)
    {
        std::printf("  %04u  %-22s  %4u files  %6u MCNKs  (%.2f/file)\n",
                    mapId, MapName(mapId), agg.files, agg.totalMcnks,
                    double(agg.totalMcnks) / std::max<uint32>(agg.files, 1));
    }

    // Histogram: MCNKs per ADT.
    std::size_t b0 = 0, b1to3 = 0, b4to10 = 0, b11to30 = 0, b31p = 0;
    for (auto const& s : all)
    {
        if      (s.roadMcnks == 0)   ++b0;
        else if (s.roadMcnks <= 3)   ++b1to3;
        else if (s.roadMcnks <= 10)  ++b4to10;
        else if (s.roadMcnks <= 30)  ++b11to30;
        else                          ++b31p;
    }
    std::printf("\ndistribution (MCNKs per ADT):\n");
    std::printf("    0:     %5zu files\n", b0);
    std::printf("   1-3:    %5zu files\n", b1to3);
    std::printf("   4-10:   %5zu files\n", b4to10);
    std::printf("  11-30:   %5zu files\n", b11to30);
    std::printf("   31+:    %5zu files\n", b31p);

    if (opts.showEmpty && b0 > 0)
    {
        std::printf("\nempty .road files (zero MCNKs flagged):\n");
        for (auto const& s : all)
            if (s.roadMcnks == 0)
                std::printf("  %04u_%02u_%02u (%s)\n",
                            s.mapId, s.adtY, s.adtX, MapName(s.mapId));
    }

    return 0;
}

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

// wmo_road_stats
//
// Scans a Buildings/ directory for *.vmo.road sidecars produced by the
// road-aware vmap4_extractor (Phase 2). Reports per-WMO + global counts so
// the owner can verify the extraction landed correctly before committing
// to a multi-hour mmaps regen.
//
// Usage:
//   wmo_road_stats --buildings <dir>           (default: ./Buildings)
//                  [--show-top N]              (default: 20 — show N most-road WMOs)
//                  [--show-zero]               (also list WMOs with sidecar but 0 road triangles)
//
// Output:
//   total: 12345 .vmo.road sidecars, 87654 road triangles total
//   top-20 WMOs by road triangle count:
//     ...
//   distribution (road triangles per WMO):
//     ...

#include "WmoRoadFile.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct Options
    {
        std::string buildingsDir = "Buildings";
        int showTop = 20;
        bool showZero = false;
    };

    [[noreturn]] void Usage(int code)
    {
        std::fprintf(stderr,
            "wmo_road_stats — inspect .vmo.road sidecars from vmap4extractor\n\n"
            "Usage:\n"
            "  wmo_road_stats [--buildings <dir>] [--show-top N] [--show-zero]\n");
        std::exit(code);
    }

    Options ParseArgs(int argc, char** argv)
    {
        Options o;
        for (int i = 1; i < argc; ++i)
        {
            std::string_view a = argv[i];
            auto val = [&](char const* f) -> std::string {
                if (i + 1 >= argc) Usage(1);
                return argv[++i];
            };
            if (a == "--help" || a == "-h") Usage(0);
            else if (a == "--buildings") o.buildingsDir = val("--buildings");
            else if (a == "--show-top")  o.showTop = std::atoi(val("--show-top").c_str());
            else if (a == "--show-zero") o.showZero = true;
            else { std::fprintf(stderr, "unknown arg: %s\n", argv[i]); Usage(1); }
        }
        return o;
    }

    struct WmoSummary
    {
        std::string filename;
        uint32 nGroups = 0;
        uint32 totalRoadTriangles = 0;
        uint32 totalTriangles = 0;
        bool parseError = false;
    };

    bool ReadWmoRoadFile(std::filesystem::path const& path, WmoSummary& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        TrinityCore::WmoRoad::WmoRoadFileHeader hdr;
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!f) return false;
        if (hdr.magic != TrinityCore::WmoRoad::kWmoRoadMagic)
        {
            out.parseError = true;
            return false;
        }
        if (hdr.version != TrinityCore::WmoRoad::kWmoRoadVersion)
        {
            out.parseError = true;
            return false;
        }
        out.nGroups = hdr.nGroups;

        for (uint32 g = 0; g < hdr.nGroups; ++g)
        {
            TrinityCore::WmoRoad::WmoRoadGroupHeader gh;
            f.read(reinterpret_cast<char*>(&gh), sizeof(gh));
            if (!f) break;
            out.totalTriangles += gh.nColTriangles;

            std::vector<uint8> bits(gh.flagsBytes);
            if (gh.flagsBytes > 0)
            {
                f.read(reinterpret_cast<char*>(bits.data()), gh.flagsBytes);
                if (!f) break;
            }
            for (uint32 t = 0; t < gh.nColTriangles; ++t)
                if (TrinityCore::WmoRoad::GetTriangleRoadBit(bits.data(), t))
                    ++out.totalRoadTriangles;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    Options o = ParseArgs(argc, argv);

    std::filesystem::path dir(o.buildingsDir);
    if (!std::filesystem::is_directory(dir))
    {
        std::fprintf(stderr, "Not a directory: %s\n", o.buildingsDir.c_str());
        return 2;
    }

    std::vector<WmoSummary> all;
    std::size_t parseErrors = 0;

    for (auto const& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        // Sidecars are named <vmo>.vmo.road (.road suffix)
        if (name.size() < 5 || name.substr(name.size() - 5) != ".road") continue;

        WmoSummary s;
        s.filename = name;
        if (!ReadWmoRoadFile(entry.path(), s))
        {
            if (s.parseError) ++parseErrors;
            continue;
        }
        all.push_back(std::move(s));
    }

    if (all.empty())
    {
        std::printf("No .vmo.road sidecars found in %s\n", o.buildingsDir.c_str());
        if (parseErrors > 0)
            std::printf("(%zu files had bad magic/version)\n", parseErrors);
        return 0;
    }

    // Aggregate.
    uint64 totalRoad = 0;
    uint64 totalTri  = 0;
    for (auto const& s : all)
    {
        totalRoad += s.totalRoadTriangles;
        totalTri  += s.totalTriangles;
    }

    std::printf("total: %zu .vmo.road sidecars, %llu road triangles "
                "(of %llu collision triangles total = %.2f%%)\n",
                all.size(),
                static_cast<unsigned long long>(totalRoad),
                static_cast<unsigned long long>(totalTri),
                totalTri ? 100.0 * totalRoad / totalTri : 0.0);
    if (parseErrors > 0)
        std::printf("(%zu files had bad magic/version — possible corruption)\n",
                    parseErrors);

    // Top N by road triangle count.
    auto sorted = all;
    std::sort(sorted.begin(), sorted.end(),
              [](WmoSummary const& a, WmoSummary const& b) {
                  return a.totalRoadTriangles > b.totalRoadTriangles;
              });
    int topN = std::min<int>(o.showTop, static_cast<int>(sorted.size()));
    if (topN > 0)
    {
        std::printf("\ntop-%d WMOs by road triangle count:\n", topN);
        for (int i = 0; i < topN; ++i)
        {
            WmoSummary const& s = sorted[i];
            double pct = s.totalTriangles
                ? 100.0 * s.totalRoadTriangles / s.totalTriangles : 0.0;
            std::printf("  %6u road / %6u total (%.1f%%)  groups=%u  %s\n",
                        s.totalRoadTriangles, s.totalTriangles, pct,
                        s.nGroups, s.filename.c_str());
        }
    }

    // Distribution of road-triangle counts.
    std::size_t b0 = 0, b1to10 = 0, b11to100 = 0, b101to1000 = 0, b1000p = 0;
    for (auto const& s : all)
    {
        if      (s.totalRoadTriangles == 0)    ++b0;
        else if (s.totalRoadTriangles <= 10)   ++b1to10;
        else if (s.totalRoadTriangles <= 100)  ++b11to100;
        else if (s.totalRoadTriangles <= 1000) ++b101to1000;
        else                                    ++b1000p;
    }
    std::printf("\ndistribution (road triangles per WMO):\n");
    std::printf("       0:  %5zu WMOs\n", b0);
    std::printf("    1-10:  %5zu WMOs\n", b1to10);
    std::printf("  11-100:  %5zu WMOs\n", b11to100);
    std::printf("101-1000:  %5zu WMOs\n", b101to1000);
    std::printf("  1001+:   %5zu WMOs\n", b1000p);

    if (o.showZero && b0 > 0)
    {
        std::printf("\nWMOs with sidecar but ZERO road triangles "
                    "(possible classifier false-negatives):\n");
        for (auto const& s : all)
            if (s.totalRoadTriangles == 0)
                std::printf("  %s  (groups=%u, total tri=%u)\n",
                            s.filename.c_str(), s.nGroups, s.totalTriangles);
    }

    return 0;
}

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

// road_visualize
//
// Read .road files (16x16 per-MCNK road masks) and render them as ASCII
// for quick visual sanity-checking. Supports a single ADT or a rectangular
// region of adjacent ADTs (so you can eyeball road continuity across tile
// borders).
//
// Usage:
//   road_visualize --maps <dir> --map-id <N> --adt-x <X> --adt-y <Y>
//                  [--span <N>] [--show-coords]
//
// Region mode (--span S, default 1): renders an S×S block of ADTs centered
// on (adt-x, adt-y). E.g. --span 3 shows a 48×48 MCNK grid covering 1599
// yards on each side.
//
// Glyphs:
//   .   no road
//   #   road MCNK
//   ?   ADT missing (no .road file for that coord)
//   *   road mask byte > 1 (reserved future bits, just in case)

#include "RoadMapDefines.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct Options
    {
        std::string mapsDir = "maps";
        int mapId = 0;
        int adtX = -1;
        int adtY = -1;
        int span = 1;
        bool showCoords = false;
    };

    [[noreturn]] void Usage(int code)
    {
        std::fprintf(stderr,
            "road_visualize — ASCII render of .road files (16x16 MCNK masks)\n\n"
            "Usage:\n"
            "  road_visualize --maps <dir> --map-id N --adt-x X --adt-y Y\n"
            "                 [--span N] [--show-coords]\n\n"
            "Glyphs:\n"
            "  .   no road\n"
            "  #   road MCNK\n"
            "  ?   .road file not found\n"
            "  *   non-1 nonzero mask byte (future bits)\n");
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
            else if (a == "--maps")        o.mapsDir = val("--maps");
            else if (a == "--map-id")      o.mapId = std::atoi(val("--map-id").c_str());
            else if (a == "--adt-x")       o.adtX = std::atoi(val("--adt-x").c_str());
            else if (a == "--adt-y")       o.adtY = std::atoi(val("--adt-y").c_str());
            else if (a == "--span")        o.span = std::atoi(val("--span").c_str());
            else if (a == "--show-coords") o.showCoords = true;
            else { std::fprintf(stderr, "Unknown arg: %s\n", argv[i]); Usage(1); }
        }
        if (o.adtX < 0 || o.adtY < 0) Usage(1);
        if (o.span < 1) o.span = 1;
        return o;
    }

    // Returns true + populates mask if file is well-formed; false if absent or bad.
    bool LoadRoadFile(std::string const& path,
                      TrinityCore::RoadMap::RoadMaskGrid& mask)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        TrinityCore::RoadMap::RoadFileHeader h;
        f.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!f) return false;
        if (h.magic != TrinityCore::RoadMap::kRoadFileMagic) return false;
        if (h.version != TrinityCore::RoadMap::kRoadFileVersion) return false;
        f.read(reinterpret_cast<char*>(mask.data()), mask.size());
        return static_cast<bool>(f);
    }

    char Glyph(uint8 v)
    {
        if (v == 0) return '.';
        if (v == 1) return '#';
        return '*';
    }
}

int main(int argc, char** argv)
{
    Options o = ParseArgs(argc, argv);

    // Compute the (span × span) ADT block centered on (adtX, adtY).
    int half = (o.span - 1) / 2;
    int extra = (o.span - 1) - half;
    int minX = std::max(0, o.adtX - half);
    int maxX = std::min(63, o.adtX + extra);
    int minY = std::max(0, o.adtY - half);
    int maxY = std::min(63, o.adtY + extra);

    int blockCols = (maxX - minX + 1);
    int blockRows = (maxY - minY + 1);
    int totalRows = blockRows * TrinityCore::RoadMap::kMcnksPerAdtSide;
    int totalCols = blockCols * TrinityCore::RoadMap::kMcnksPerAdtSide;

    std::printf("Map %d  ADT block (%d..%d) × (%d..%d)  → %d×%d MCNKs (%.0f×%.0f yards)\n\n",
                o.mapId, minX, maxX, minY, maxY,
                totalCols, totalRows,
                blockCols * 533.33, blockRows * 533.33);

    // Load every ADT in the block into a 2D mask array of size
    // (blockRows * 16) × (blockCols * 16). Missing ADTs leave their cells
    // as 0xFF (rendered as '?').
    constexpr int kSide = TrinityCore::RoadMap::kMcnksPerAdtSide;
    std::vector<std::vector<uint8>> grid(totalRows, std::vector<uint8>(totalCols, 0xFF));

    std::size_t adtsFound = 0;
    std::size_t totalRoadMcnks = 0;

    // Filename convention: <mapId>_<adtX>_<adtY>.road (see MakeRoadFilePath).
    // ADT iteration: by convention adtX is the "outer / row" coord in TC's
    // .map naming, adtY is "column". Map (X, Y) → grid position.
    for (int ay = minY; ay <= maxY; ++ay)
    {
        for (int ax = minX; ax <= maxX; ++ax)
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s/%04d_%02d_%02d.road",
                          o.mapsDir.c_str(), o.mapId, ax, ay);
            TrinityCore::RoadMap::RoadMaskGrid mask{};
            if (!LoadRoadFile(buf, mask))
                continue;
            ++adtsFound;
            // Block placement.
            int blockRow = ay - minY;
            int blockCol = ax - minX;
            for (std::size_t r = 0; r < kSide; ++r)
            {
                for (std::size_t c = 0; c < kSide; ++c)
                {
                    uint8 v = mask[r * kSide + c];
                    grid[blockRow * kSide + r][blockCol * kSide + c] = v;
                    if (v != 0 && v != 0xFF) ++totalRoadMcnks;
                }
            }
        }
    }

    // Render top-down. Optional column-coordinate ruler.
    if (o.showCoords)
    {
        std::printf("       ");
        for (int x = 0; x < totalCols; ++x)
            std::printf("%c", (x % 16 == 0) ? '|' : ' ');
        std::printf("\n");
    }

    for (int r = 0; r < totalRows; ++r)
    {
        if (o.showCoords)
            std::printf("  %3d  ", r);
        for (int c = 0; c < totalCols; ++c)
        {
            uint8 v = grid[r][c];
            if (v == 0xFF) std::printf("?");
            else           std::printf("%c", Glyph(v));
            // Tile boundary separator.
            if ((c + 1) % kSide == 0 && c + 1 < totalCols)
                std::printf("|");
        }
        std::printf("\n");
        // ADT-row separator.
        if ((r + 1) % kSide == 0 && r + 1 < totalRows)
        {
            if (o.showCoords) std::printf("       ");
            for (int c = 0; c < totalCols; ++c)
            {
                std::printf("-");
                if ((c + 1) % kSide == 0 && c + 1 < totalCols)
                    std::printf("+");
            }
            std::printf("\n");
        }
    }

    std::printf("\nADTs found: %zu / %d × %d = %d\n",
                adtsFound, blockCols, blockRows, blockCols * blockRows);
    std::printf("Total road MCNKs in block: %zu\n", totalRoadMcnks);
    return 0;
}

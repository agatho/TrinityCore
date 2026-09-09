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

#include "tc_catch2.h"

#include "AdtTextureReader.h"
#include "RoadClassifier.h"
#include "RoadFileWriter.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace Road;
using namespace Road::AdtTexture;
using TrinityCore::RoadMap::RoadMaskGrid;
using TrinityCore::RoadMap::RoadFileHeader;
using TrinityCore::RoadMap::kRoadFileMagic;
using TrinityCore::RoadMap::kRoadFileVersion;
using TrinityCore::RoadMap::kMcnksPerAdt;
using TrinityCore::RoadMap::kRoadFileSize;

namespace
{
    // Helper to construct a synthetic ADT summary with N MCNKs flagged
    // by texture. ix/iy are set from idx so the mask matches the layout.
    AdtTextureSummary MakeSummary(std::initializer_list<std::pair<uint8, uint8>> roadCoords,
                                   char const* texturePath = "TILESET/ELWYNN/ElwynnCobbleStoneBase.blp")
    {
        AdtTextureSummary s;
        s.mapId = 0;
        s.adtX = 32;
        s.adtY = 32;

        // Mark only the listed (ix, iy) MCNKs as having layers + road texture.
        std::array<bool, 256> roadFlags{};
        for (auto [ix, iy] : roadCoords)
            if (iy < 16 && ix < 16)
                roadFlags[static_cast<std::size_t>(iy * 16 + ix)] = true;

        for (std::size_t idx = 0; idx < 256; ++idx)
        {
            McnkTextureSummary& m = s.mcnks[idx];
            uint8 iy = static_cast<uint8>(idx / 16);
            uint8 ix = static_cast<uint8>(idx % 16);
            m.mcnkIdx = static_cast<uint16>(idx);
            m.ix = ix;
            m.iy = iy;
            if (roadFlags[idx])
            {
                m.nLayers = 2;
                m.dominantTextureBlp = texturePath;
                m.dominantEffectId = 0;   // 0 → confidence 0.5 (no signal, passes >0.3 gate)
            }
            else
            {
                m.nLayers = 0;
                // No texture, will be skipped by BuildRoadMaskGrid.
            }
        }
        return s;
    }
}

// =============================================================================
// BuildRoadMaskGrid — pure transformation from texture-classified ADT summary
// to per-MCNK road mask grid. Mirrors classifier rule from design doc §3.3.
// =============================================================================

TEST_CASE("BuildRoadMaskGrid - empty summary produces zero mask",
          "[RoadFileWriter]")
{
    AdtTextureSummary s;   // all default; no layers
    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 0);
    for (uint8 v : grid) REQUIRE(v == 0);
}

TEST_CASE("BuildRoadMaskGrid - 4-cell contiguous road survives the filter",
          "[RoadFileWriter]")
{
    // Default contiguous-area threshold is 4. A 2x2 block should survive.
    auto s = MakeSummary({ {3,3}, {4,3}, {3,4}, {4,4} });
    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 4);
    REQUIRE(grid[3 * 16 + 3] == 1);
    REQUIRE(grid[4 * 16 + 4] == 1);
}

TEST_CASE("BuildRoadMaskGrid - isolated 3-cell road is filtered out",
          "[RoadFileWriter]")
{
    // 1x3 strip = 3 cells, below threshold 4.
    auto s = MakeSummary({ {5,5}, {6,5}, {7,5} });
    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 0);
}

TEST_CASE("BuildRoadMaskGrid - non-road texture is not flagged",
          "[RoadFileWriter]")
{
    // Grass texture — IsRoadTexturePath returns false.
    auto s = MakeSummary({ {3,3}, {4,3}, {3,4}, {4,4} },
                          "TILESET/ELWYNN/ElwynnGrass.blp");
    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 0);
}

TEST_CASE("BuildRoadMaskGrid - layer 0 (nLayers == 0) is skipped",
          "[RoadFileWriter]")
{
    AdtTextureSummary s;
    // Single MCNK with road texture but nLayers == 0 → should be skipped.
    s.mcnks[100].nLayers = 0;
    s.mcnks[100].dominantTextureBlp = "TILESET/ELWYNN/ElwynnCobbleStoneBase.blp";
    s.mcnks[100].ix = 4;
    s.mcnks[100].iy = 6;

    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 0);
}

TEST_CASE("BuildRoadMaskGrid - GroundEffect veto",
          "[RoadFileWriter]")
{
    // Texture says road; ground-effect lookup says vegetation (confidence 0.1).
    // Combined verdict should REJECT — the 2x2 block doesn't even reach
    // the contiguous-area filter.
    ClearGroundEffectTable();
    SetGroundEffectConfidenceForTesting(42, 0.1f);
    auto s = MakeSummary({ {3,3}, {4,3}, {3,4}, {4,4} });
    // Override effect IDs to point at the vegetation entry.
    for (auto& m : s.mcnks)
        m.dominantEffectId = 42;

    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 0);
    ClearGroundEffectTable();
}

TEST_CASE("BuildRoadMaskGrid - large connected component preserved",
          "[RoadFileWriter]")
{
    // 5x5 = 25 cells, way over threshold.
    std::vector<std::pair<uint8, uint8>> coords;
    for (uint8 y = 1; y <= 5; ++y)
        for (uint8 x = 1; x <= 5; ++x)
            coords.emplace_back(x, y);
    AdtTextureSummary s;
    s.mapId = 0;
    for (std::size_t idx = 0; idx < 256; ++idx)
    {
        s.mcnks[idx].mcnkIdx = static_cast<uint16>(idx);
        s.mcnks[idx].iy = static_cast<uint8>(idx / 16);
        s.mcnks[idx].ix = static_cast<uint8>(idx % 16);
    }
    for (auto [x, y] : coords)
    {
        auto& m = s.mcnks[y * 16 + x];
        m.nLayers = 3;
        m.dominantTextureBlp = "TILESET/ELWYNN/ElwynnCobbleStoneBase.blp";
    }
    std::size_t nonZero = 0;
    auto grid = BuildRoadMaskGrid(s, &nonZero);
    REQUIRE(nonZero == 25);
}

// =============================================================================
// WriteRoadFile / file I/O round-trip
// =============================================================================

namespace
{
    std::string TempDir()
    {
        std::error_code ec;
        std::filesystem::path p = std::filesystem::temp_directory_path(ec) /
            ("road_test_" + std::to_string(::rand()));
        std::filesystem::create_directories(p, ec);
        return p.generic_string();
    }

    bool ReadRoadFile(std::string const& path, RoadFileHeader& hOut,
                       RoadMaskGrid& mOut)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.read(reinterpret_cast<char*>(&hOut), sizeof(hOut));
        if (!f) return false;
        f.read(reinterpret_cast<char*>(mOut.data()), mOut.size());
        return static_cast<bool>(f);
    }
}

TEST_CASE("WriteRoadFile - emits exactly 280 bytes with correct header",
          "[RoadFileWriter]")
{
    std::string dir = TempDir();
    RoadMaskGrid mask{};
    mask[3 * 16 + 5] = 1;
    mask[10 * 16 + 11] = 1;

    REQUIRE(WriteRoadFile(dir, 1234, 22, 33, mask, 0));

    std::string path = TrinityCore::RoadMap::MakeRoadFilePath(dir, 1234, 22, 33);

    // File size check.
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    REQUIRE(!ec);
    REQUIRE(sz == kRoadFileSize);

    // Round-trip header + mask.
    RoadFileHeader hOut;
    RoadMaskGrid   mOut;
    REQUIRE(ReadRoadFile(path, hOut, mOut));
    REQUIRE(hOut.magic == kRoadFileMagic);
    REQUIRE(hOut.version == kRoadFileVersion);
    REQUIRE(hOut.mapId == 1234);
    REQUIRE(hOut.adtX == 22);
    REQUIRE(hOut.adtY == 33);
    REQUIRE(mOut[3 * 16 + 5] == 1);
    REQUIRE(mOut[10 * 16 + 11] == 1);
    REQUIRE(mOut[0] == 0);

    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("WriteRoadFile - filename matches MakeRoadFilePath convention",
          "[RoadFileWriter]")
{
    std::string dir = TempDir();
    RoadMaskGrid mask{};
    REQUIRE(WriteRoadFile(dir, 0, 32, 48, mask, 0));

    std::string expected = dir + "/0000_32_48.road";
    REQUIRE(std::filesystem::exists(expected));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("WriteRoadFile - bad output directory returns false",
          "[RoadFileWriter]")
{
    RoadMaskGrid mask{};
    REQUIRE_FALSE(WriteRoadFile(
        "/this/path/definitely/does/not/exist/anywhere/zzzzz",
        0, 0, 0, mask, 0));
}

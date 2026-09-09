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

#include "RoadMapDefines.h"
#include "MMapDefines.h"

#include <cstring>
#include <span>
#include <vector>

using namespace TrinityCore::RoadMap;

// =============================================================================
// Layout invariants
// =============================================================================

TEST_CASE("RoadMap file format - struct layout invariants", "[RoadMap]")
{
    REQUIRE(sizeof(RoadFileHeader) == 24);
    REQUIRE(sizeof(RoadMaskGrid)   == 256);
    REQUIRE(kRoadFileSize          == 280);

    REQUIRE(kMcnksPerAdtSide == 16);
    REQUIRE(kMcnksPerAdt     == 256);
}

TEST_CASE("RoadMap file format - magic + version constants",
          "[RoadMap]")
{
    // Magic bytes spell "ROAD" on disk.
    RoadFileHeader h;
    auto const* bytes = reinterpret_cast<unsigned char const*>(&h.magic);
    REQUIRE(bytes[0] == 'R');
    REQUIRE(bytes[1] == 'O');
    REQUIRE(bytes[2] == 'A');
    REQUIRE(bytes[3] == 'D');

    REQUIRE(h.version == 1u);
    REQUIRE(h.flags == 0u);
    REQUIRE(h.mapId == 0u);
}

// =============================================================================
// Filename composition
// =============================================================================

TEST_CASE("MakeRoadFilePath - canonical formatting matches .map convention",
          "[RoadMap]")
{
    // Mirrors TC's existing `<mapId:04>_<y:02>_<x:02>.map` filename
    // pattern so TerrainBuilder::loadRoadMask can find the sibling file
    // via the same Trinity::StringFormat call.
    REQUIRE(MakeRoadFilePath("maps", 0, 32, 48) == "maps/0000_32_48.road");
    REQUIRE(MakeRoadFilePath("maps", 1, 31, 31) == "maps/0001_31_31.road");
    REQUIRE(MakeRoadFilePath("maps", 530, 0, 0) == "maps/0530_00_00.road");
    REQUIRE(MakeRoadFilePath("maps", 2444, 63, 63) == "maps/2444_63_63.road");
}

TEST_CASE("MakeRoadFilePath - mapId zero-pad to 4 digits, adt to 2",
          "[RoadMap]")
{
    REQUIRE(MakeRoadFilePath("X", 0, 0, 0) == "X/0000_00_00.road");
    REQUIRE(MakeRoadFilePath("X", 9, 9, 9) == "X/0009_09_09.road");
}

TEST_CASE("MakeRoadFilePath - alternate maps directory", "[RoadMap]")
{
    REQUIRE(MakeRoadFilePath("/srv/wow/maps", 0, 32, 32)
            == "/srv/wow/maps/0000_32_32.road");
    REQUIRE(MakeRoadFilePath("./out", 0, 32, 32)
            == "./out/0000_32_32.road");
}

// =============================================================================
// Round-trip in-memory write/read
// =============================================================================

namespace
{
    // Pack header + mask into a contiguous byte buffer.
    std::vector<uint8> SerializeRoadFile(RoadFileHeader const& header,
                                          RoadMaskGrid const& mask)
    {
        std::vector<uint8> out(kRoadFileSize);
        std::memcpy(out.data(), &header, sizeof(header));
        std::memcpy(out.data() + sizeof(header), mask.data(), mask.size());
        return out;
    }

    // Parse the buffer back into header + mask. Returns false on size or
    // magic mismatch.
    bool DeserializeRoadFile(std::span<uint8 const> bytes,
                              RoadFileHeader& outHeader,
                              RoadMaskGrid& outMask)
    {
        if (bytes.size() != kRoadFileSize)
            return false;
        std::memcpy(&outHeader, bytes.data(), sizeof(outHeader));
        if (outHeader.magic != kRoadFileMagic)
            return false;
        if (outHeader.version != kRoadFileVersion)
            return false;
        std::memcpy(outMask.data(),
                    bytes.data() + sizeof(outHeader),
                    outMask.size());
        return true;
    }
}

TEST_CASE("Road file round-trip - empty mask", "[RoadMap]")
{
    RoadFileHeader h;
    h.mapId = 0;
    h.adtX = 32;
    h.adtY = 32;
    RoadMaskGrid mask{};

    auto bytes = SerializeRoadFile(h, mask);
    REQUIRE(bytes.size() == 280);

    RoadFileHeader rh;
    RoadMaskGrid   rmask;
    REQUIRE(DeserializeRoadFile(bytes, rh, rmask));
    REQUIRE(rh.magic == kRoadFileMagic);
    REQUIRE(rh.version == kRoadFileVersion);
    REQUIRE(rh.mapId == 0);
    REQUIRE(rh.adtX == 32);
    REQUIRE(rh.adtY == 32);
    for (uint8 v : rmask)
        REQUIRE(v == 0);
}

TEST_CASE("Road file round-trip - populated mask", "[RoadMap]")
{
    RoadFileHeader h;
    h.mapId = 530;
    h.adtX = 22;
    h.adtY = 33;
    h.flags = static_cast<uint32>(RoadFileFlags::kExperimental);
    RoadMaskGrid mask{};
    // Paint an L-shape.
    for (std::size_t i = 0; i < 16; ++i)
        mask[5 * 16 + i] = 1;     // horizontal row
    for (std::size_t r = 5; r < 12; ++r)
        mask[r * 16 + 8] = 1;     // vertical column

    auto bytes = SerializeRoadFile(h, mask);

    RoadFileHeader rh;
    RoadMaskGrid   rmask;
    REQUIRE(DeserializeRoadFile(bytes, rh, rmask));
    REQUIRE(rh.mapId == 530);
    REQUIRE(rh.adtX == 22);
    REQUIRE(rh.adtY == 33);
    REQUIRE(rh.flags == static_cast<uint32>(RoadFileFlags::kExperimental));
    REQUIRE(std::memcmp(rmask.data(), mask.data(), 256) == 0);
}

TEST_CASE("Road file round-trip - corrupted magic fails parse", "[RoadMap]")
{
    RoadFileHeader h;
    h.mapId = 0; h.adtX = 0; h.adtY = 0;
    RoadMaskGrid mask{};
    auto bytes = SerializeRoadFile(h, mask);

    // Corrupt the magic field.
    bytes[0] = 'X';

    RoadFileHeader rh;
    RoadMaskGrid   rmask;
    REQUIRE_FALSE(DeserializeRoadFile(bytes, rh, rmask));
}

TEST_CASE("Road file round-trip - corrupted version fails parse",
          "[RoadMap]")
{
    RoadFileHeader h;
    h.mapId = 0; h.adtX = 0; h.adtY = 0;
    h.version = 999;
    RoadMaskGrid mask{};
    auto bytes = SerializeRoadFile(h, mask);

    RoadFileHeader rh;
    RoadMaskGrid   rmask;
    REQUIRE_FALSE(DeserializeRoadFile(bytes, rh, rmask));
}

TEST_CASE("Road file round-trip - wrong file size fails parse", "[RoadMap]")
{
    RoadFileHeader h;
    h.mapId = 0; h.adtX = 0; h.adtY = 0;
    RoadMaskGrid mask{};
    auto bytes = SerializeRoadFile(h, mask);

    // Truncate by 1 byte.
    bytes.pop_back();
    RoadFileHeader rh;
    RoadMaskGrid   rmask;
    REQUIRE_FALSE(DeserializeRoadFile(bytes, rh, rmask));

    // Or add a stray byte.
    bytes.push_back(0);
    bytes.push_back(0);
    REQUIRE_FALSE(DeserializeRoadFile(bytes, rh, rmask));
}

// =============================================================================
// MMapDefines NAV_AREA_ROAD invariants
// =============================================================================

TEST_CASE("MMapDefines - NAV_AREA_ROAD constants", "[RoadMap]")
{
    // Numerical value.
    REQUIRE(NAV_AREA_ROAD == 7);

    // Bit flag must not collide with existing flags.
    REQUIRE(NAV_ROAD         == 0x10);
    REQUIRE(NAV_GROUND       == 0x01);
    REQUIRE(NAV_GROUND_STEEP == 0x02);
    REQUIRE(NAV_WATER        == 0x04);
    REQUIRE(NAV_MAGMA_SLIME  == 0x08);

    // No bit overlap.
    REQUIRE((NAV_ROAD & (NAV_GROUND | NAV_GROUND_STEEP |
                          NAV_WATER | NAV_MAGMA_SLIME)) == 0);

    // Within the area_id mask.
    REQUIRE((NAV_AREA_ROAD & NAV_AREA_ALL_MASK) == NAV_AREA_ROAD);

    // Range bookkeeping.
    REQUIRE(NAV_AREA_MIN_VALUE == NAV_AREA_ROAD);
    REQUIRE(NAV_AREA_MAX_VALUE == NAV_AREA_GROUND);
}

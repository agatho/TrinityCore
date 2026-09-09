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

#include "ListfileMap.h"
#include "WmoRoadClassifier.h"

#include <cstring>
#include <string>
#include <vector>

using namespace Road::WmoRoad;

namespace
{
    // Helper: build a WmoMaterialTexture vector from a list of paths.
    std::vector<WmoMaterialTexture> Mats(std::initializer_list<char const*> paths)
    {
        std::vector<WmoMaterialTexture> out;
        for (char const* p : paths)
            out.push_back({ std::string(p) });
        return out;
    }
}

// =============================================================================
// IsTriangleRoad
// =============================================================================

TEST_CASE("IsTriangleRoad - resolves road material correctly", "[WmoRoad]")
{
    auto mats = Mats({
        "tileset/elwynn/elwynncobblestonebase.blp",       // material 0: road
        "world/wmo/azeroth/buildings/walls/grey.blp",     // material 1: not road
        "tileset/durotar/durotardirt.blp",                // material 2: not road
    });

    REQUIRE(IsTriangleRoad(0, mats));      // cobble → road
    REQUIRE_FALSE(IsTriangleRoad(1, mats));
    REQUIRE_FALSE(IsTriangleRoad(2, mats));
}

TEST_CASE("IsTriangleRoad - materialId 0xFF = no-collision triangle", "[WmoRoad]")
{
    auto mats = Mats({ "tileset/elwynn/cobble.blp" });
    REQUIRE_FALSE(IsTriangleRoad(0xFF, mats));   // sentinel value
}

TEST_CASE("IsTriangleRoad - out-of-bounds materialId returns false", "[WmoRoad]")
{
    auto mats = Mats({ "tileset/elwynn/cobble.blp" });
    REQUIRE_FALSE(IsTriangleRoad(5, mats));   // only 1 material defined
}

TEST_CASE("IsTriangleRoad - empty materials table", "[WmoRoad]")
{
    std::vector<WmoMaterialTexture> mats;
    REQUIRE_FALSE(IsTriangleRoad(0, mats));
}

// =============================================================================
// ClassifyMaterials + bitmap helpers
// =============================================================================

TEST_CASE("ClassifyMaterials - bitmap encodes road materials", "[WmoRoad]")
{
    auto mats = Mats({
        "tileset/stormwindcity/sw_cobble_a.blp",          // road
        "world/wmo/structure/wall_grey.blp",              // not
        "tileset/durotar/durotarroad_s.blp",              // road
        "world/wmo/structure/window_glass.blp",           // not
        "tileset/expansion10/11udm_brickroad01_512.blp",  // road
    });

    auto bitmap = ClassifyMaterials(mats);
    REQUIRE(MaterialBitSet(bitmap, 0));   // cobble
    REQUIRE_FALSE(MaterialBitSet(bitmap, 1));
    REQUIRE(MaterialBitSet(bitmap, 2));   // durotarroad
    REQUIRE_FALSE(MaterialBitSet(bitmap, 3));
    REQUIRE(MaterialBitSet(bitmap, 4));   // brickroad
}

TEST_CASE("ClassifyMaterials - empty input gives empty bitmap", "[WmoRoad]")
{
    auto bitmap = ClassifyMaterials({});
    REQUIRE(bitmap.empty());
}

TEST_CASE("ClassifyMaterials - all-road bitmap", "[WmoRoad]")
{
    auto mats = Mats({
        "tileset/elwynn/cobble1.blp", "tileset/elwynn/cobble2.blp",
        "tileset/elwynn/cobble3.blp", "tileset/elwynn/cobble4.blp",
        "tileset/elwynn/cobble5.blp", "tileset/elwynn/cobble6.blp",
        "tileset/elwynn/cobble7.blp", "tileset/elwynn/cobble8.blp",
    });
    auto bitmap = ClassifyMaterials(mats);
    REQUIRE(bitmap.size() == 1);
    REQUIRE(bitmap[0] == 0xFF);
}

TEST_CASE("ClassifyMaterials - bit indexing across byte boundary", "[WmoRoad]")
{
    auto mats = Mats({
        "world/wmo/grey.blp",                  // 0: not
        "world/wmo/grey.blp",                  // 1: not
        "world/wmo/grey.blp",                  // 2: not
        "world/wmo/grey.blp",                  // 3: not
        "world/wmo/grey.blp",                  // 4: not
        "world/wmo/grey.blp",                  // 5: not
        "world/wmo/grey.blp",                  // 6: not
        "world/wmo/grey.blp",                  // 7: not
        "tileset/elwynn/cobble.blp",           // 8: road  (second byte, bit 0)
        "world/wmo/grey.blp",                  // 9: not
        "tileset/durotar/road.blp",            // 10: road (second byte, bit 2)
    });
    auto bitmap = ClassifyMaterials(mats);
    REQUIRE(bitmap.size() == 2);
    REQUIRE(bitmap[0] == 0x00);
    REQUIRE((bitmap[1] & 0x01) == 0x01);   // bit 8
    REQUIRE((bitmap[1] & 0x04) == 0x04);   // bit 10
    REQUIRE(MaterialBitSet(bitmap, 8));
    REQUIRE(MaterialBitSet(bitmap, 10));
    REQUIRE_FALSE(MaterialBitSet(bitmap, 9));
}

TEST_CASE("MaterialBitSet - out-of-range index returns false", "[WmoRoad]")
{
    std::vector<uint8> bitmap = { 0xFF };
    REQUIRE(MaterialBitSet(bitmap, 0));
    REQUIRE(MaterialBitSet(bitmap, 7));
    REQUIRE_FALSE(MaterialBitSet(bitmap, 8));
    REQUIRE_FALSE(MaterialBitSet(bitmap, 1024));
}

TEST_CASE("SetMaterialBit - grows bitmap as needed", "[WmoRoad]")
{
    std::vector<uint8> bitmap;
    SetMaterialBit(bitmap, 0);
    SetMaterialBit(bitmap, 7);
    SetMaterialBit(bitmap, 8);
    SetMaterialBit(bitmap, 100);
    REQUIRE(bitmap.size() == 13);       // 100/8 = 12, +1
    REQUIRE(bitmap[0] == 0x81);          // bits 0 + 7
    REQUIRE(bitmap[1] == 0x01);          // bit 8
    REQUIRE((bitmap[12] & 0x10) == 0x10);// bit 100 = bit 4 of byte 12
}

// =============================================================================
// ResolveFromMotx (legacy)
// =============================================================================

TEST_CASE("ResolveFromMotx - resolves offsets to strings",
          "[WmoRoad]")
{
    // MOTX bytes: 3 null-terminated strings, total 30 bytes
    std::string motx = std::string("tileset/elwynn/cobble.blp")
                       + '\0'
                       + "world/wmo/wall.blp"
                       + '\0'
                       + "tileset/durotar/road.blp"
                       + '\0';
    std::vector<uint8> motxBytes(motx.begin(), motx.end());

    // Offsets after null terminators: cobble (25 chars + 1 NUL = 26),
    // then wall (18 + 1 = 19), then road (24 + 1 = 25). Cumulative: 0, 26, 45.
    std::vector<uint32> offsets = { 0, 26, 45 };
    auto out = ResolveFromMotx(motxBytes, offsets);
    REQUIRE(out.size() == 3);
    REQUIRE(out[0].textureBlp == "tileset/elwynn/cobble.blp");
    REQUIRE(out[1].textureBlp == "world/wmo/wall.blp");
    REQUIRE(out[2].textureBlp == "tileset/durotar/road.blp");
}

TEST_CASE("ResolveFromMotx - out-of-bounds offset leaves empty",
          "[WmoRoad]")
{
    std::string motx = "tileset/elwynn/cobble.blp";
    motx.push_back('\0');
    std::vector<uint8> motxBytes(motx.begin(), motx.end());

    std::vector<uint32> offsets = { 0, 9999 };
    auto out = ResolveFromMotx(motxBytes, offsets);
    REQUIRE(out.size() == 2);
    REQUIRE(out[0].textureBlp == "tileset/elwynn/cobble.blp");
    REQUIRE(out[1].textureBlp.empty());
}

// =============================================================================
// ResolveFromModi (modern)
// =============================================================================

TEST_CASE("ResolveFromModi - resolves via listfile", "[WmoRoad]")
{
    Road::ListfileMap lf;
    lf.Insert(100, "tileset/elwynn/cobble.blp");
    lf.Insert(200, "world/wmo/wall.blp");
    lf.Insert(300, "tileset/durotar/road.blp");

    std::vector<uint32> modi = { 100, 200, 300 };
    auto out = ResolveFromModi(modi, &lf);
    REQUIRE(out.size() == 3);
    REQUIRE(out[0].textureBlp == "tileset/elwynn/cobble.blp");
    REQUIRE(out[1].textureBlp == "world/wmo/wall.blp");
    REQUIRE(out[2].textureBlp == "tileset/durotar/road.blp");
}

TEST_CASE("ResolveFromModi - missing listfile entries fall back to FDID",
          "[WmoRoad]")
{
    Road::ListfileMap lf;
    lf.Insert(100, "tileset/elwynn/cobble.blp");

    std::vector<uint32> modi = { 100, 9999 };
    auto out = ResolveFromModi(modi, &lf);
    REQUIRE(out[0].textureBlp == "tileset/elwynn/cobble.blp");
    REQUIRE(out[1].textureBlp == "[FDID:9999]");
}

TEST_CASE("ResolveFromModi - no listfile gives all-FDID placeholders",
          "[WmoRoad]")
{
    std::vector<uint32> modi = { 100, 200 };
    auto out = ResolveFromModi(modi, nullptr);
    REQUIRE(out[0].textureBlp == "[FDID:100]");
    REQUIRE(out[1].textureBlp == "[FDID:200]");
}

TEST_CASE("ResolveFromModi - FDID zero gives empty path",
          "[WmoRoad]")
{
    Road::ListfileMap lf;
    lf.Insert(100, "tileset/elwynn/cobble.blp");

    std::vector<uint32> modi = { 100, 0, 200 };
    auto out = ResolveFromModi(modi, &lf);
    REQUIRE(out[0].textureBlp == "tileset/elwynn/cobble.blp");
    REQUIRE(out[1].textureBlp.empty());   // FDID 0 = no texture
    REQUIRE(out[2].textureBlp == "[FDID:200]");
}

// =============================================================================
// End-to-end: full WMO road classification
// =============================================================================

TEST_CASE("E2E - WMO with mixed materials classifies triangles correctly",
          "[WmoRoad]")
{
    // Build a synthetic WMO scenario: 5 materials, of which 2 are road.
    auto mats = Mats({
        "world/wmo/azeroth/buildings/stormwind/sw_wall_a.blp",      // 0: wall
        "tileset/stormwindcity/sw_cobble_a.blp",                    // 1: cobble (ROAD)
        "world/wmo/azeroth/buildings/stormwind/sw_window.blp",      // 2: window
        "tileset/elwynn/elwynncobblestonebase.blp",                 // 3: cobble (ROAD)
        "world/wmo/azeroth/buildings/stormwind/sw_door.blp",        // 4: door
    });

    auto matBitmap = ClassifyMaterials(mats);

    // Bridge surface: triangle with materialId=1 → road
    REQUIRE(IsTriangleRoad(1, mats));
    REQUIRE(MaterialBitSet(matBitmap, 1));

    // Wall: materialId=0 → not road
    REQUIRE_FALSE(IsTriangleRoad(0, mats));
    REQUIRE_FALSE(MaterialBitSet(matBitmap, 0));

    // Road approach: materialId=3 → road
    REQUIRE(IsTriangleRoad(3, mats));
    REQUIRE(MaterialBitSet(matBitmap, 3));

    // Synthetic MPY2 stream: 6 triangles using materials [0, 1, 1, 4, 3, 2]
    // - Triangles 1, 2 should be road (material 1 = cobble)
    // - Triangle 4 should be road (material 3 = elwynn cobble)
    // - Triangles 0, 3, 5 are not road
    std::vector<uint8> matIds = { 0, 1, 1, 4, 3, 2 };
    std::vector<bool> expected = { false, true, true, false, true, false };
    for (std::size_t i = 0; i < matIds.size(); ++i)
        REQUIRE(IsTriangleRoad(matIds[i], mats) == expected[i]);
}

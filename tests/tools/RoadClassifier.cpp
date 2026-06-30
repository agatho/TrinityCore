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

#include "RoadClassifier.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace Road;

// =============================================================================
// IsRoadTexturePath / MatchedRoadToken
//
// Test corpus is the exemplar BLP paths surveyed in
// ROAD_RESEARCH_02_TEXTURE_CATALOG.md (8K shipping tileset BLPs sampled).
// Negative cases are deliberately chosen from the false-positive exclusion
// notes in §4 of the catalog (dirt, bare _Tile, model paths, etc.).
// =============================================================================

TEST_CASE("Road texture name classifier - vanilla positives", "[RoadClassifier]")
{
    // Eastern Kingdoms
    REQUIRE(IsRoadTexturePath("TILESET/ELWYNN/ElwynnCobbleStoneBase.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/ELWYNN/ElwynnCobbleStoneDock.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/REDRIDGE/RedridgeRockRoadBase.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/DUSKWOOD/DuskwoodCobblestone.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Loch Modan/LOCHMODANBRICKROADBASE.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Wetlands/Wetlandsroad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/SilverPine/SilverPineRoad.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/TirisFall/TirisFallStoneRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/StormwindCity/SW_Cobble_A.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/StormwindCity/sw_cobble_Brick_Red_01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/PlagueLands/PlaguedRoadStone01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Stranglethorn/StrangleThornCobbleStoneRoad.blp"));

    // Kalimdor
    REQUIRE(IsRoadTexturePath("TILESET/Durotar/DUROTARROAD.BLP"));
    REQUIRE(IsRoadTexturePath("TILESET/Durotar/DurotarFlagstonesA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Orgrimmar/OrgMetalCobbleA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Barrens/BarrensRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Mullgore/MullgoreRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/ASHENVALE/ASHENVALEROAD.BLP"));
    REQUIRE(IsRoadTexturePath("TILESET/HYJAL/HyjalRoadC.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/Kalidar/KalidarDarnRoad.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/ASHZARA/AshzaraRoad_03.blp"));
}

TEST_CASE("Road texture name classifier - all expansions positives", "[RoadClassifier]")
{
    // BfC / TBC
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION01/EVERSONG/EversongRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION01/BloodMyst/Bloodmyst_Road.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION01/BloodMyst/Bloodmyst_DirtRoad.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION01/NAGRAND/NagrandRoad02.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION01/Terokkar/TerokkarForest_Road.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION01/BladesEdge/BladesEdgeElfRoad.blp"));

    // WotLK — heavy flagstone
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/BOREANTUNDRA/BT_CobblestonesA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/BOREANTUNDRA/BT_PathA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/BOREANTUNDRA/BT_RoadA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/DRAGONBLIGHT/DB_TitanRoadA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/DRAGONBLIGHT/DB_FlagstonesA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/DRAGONBLIGHT/DB_ScarletRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/GRIZZLYHILLS/GH_LOGROADA.BLP"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/LAKEWINTERGRASP/WG_TitanRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/ZULDRAK/ZD_FlagstonesA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/ZULDRAK/ZD_PathA.blp"));

    // Cata
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION03/Gilneas/GN_CobbleStoneA.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION03/LOSTISLES/Kezan_Road_01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION03/LOSTISLES/Goblin_Pavement_01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION03/ULDUM/Uldum_Road_Sand.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION03/TWILIGHTHIGHLANDS/TH_StonePath_01.blp"));

    // MoP
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION04/ValleyofFourWinds/VFW_Road01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION04/TURTLEZONE/TU_Road01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION04/ZONEB/V4W_FarmRoad01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION04/VALEOFETERNALBLOSSOMS/VEB_ROAD01_512.BLP"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION04/VALEOFETERNALBLOSSOMS/VEB_rockRoad_1024.blp"));

    // WoD
    REQUIRE(IsRoadTexturePath("Tileset/Expansion05/Shadowmoon/6SM_Road01_256.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion05/Frostwind/6FW_Road02_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION05/FROSTWIND/6FW_PATH01_256.BLP"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION05/NAGRAND/6NG_Path01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION05/TanaanJungle/6TJ_Road_01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/EXPANSION05/Talador/6TD_RockRoad01_512.blp"));

    // Legion
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Azsuna/7AZ_Road01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Road01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/ValSharah/7vs_Road_01.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/ValSharah/7vs_NightmareRoad_01.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/7XP_LeyRoad01_512.blp"));

    // BfA
    REQUIRE(IsRoadTexturePath("tileset/expansion07/kultiraszone/8kul_cobble_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/riverzone/8riv_cobble_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/desertzone/8des_path01_256.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/desertzone/8des_glassroad01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/desertzone/8des_sethrakroad01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/8naj_road01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/general/8war_stoneroad01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/general/8war_cobblebrick01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/general/8xp_tirisfal_bricks01.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/warfronts/8drk_warfronts_road01_512.blp"));

    // Shadowlands
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9bas_road02_1024.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9ard_road01_1024.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9cas_road04_1024.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9cas_roadtile01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9mal_road01_1024.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9prg_roadtile01_1024.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion08/9maw_roadtile01_512.blp"));

    // Dragonflight
    REQUIRE(IsRoadTexturePath("tileset/expansion09/10ed_stoneroad01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion09/10cav_tilepath01_512.blp"));

    // TWW
    REQUIRE(IsRoadTexturePath("tileset/expansion10/11ea_stoneroad01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion10/11ea_road01_1024.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion10/11nr_road02_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion10/11ar_hexroad01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion10/11UDM_BrickRoad01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion10/11UDM_AsphaltRoad01_1024.blp"));

    // Midnight / Expansion11 pre-release
    REQUIRE(IsRoadTexturePath("Tileset/Expansion11/12ESW_Road01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion11/12ESW_SilvermoonRoad01_1024.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion11/12DUR_RoadCracked_512.blp"));
}

TEST_CASE("Road texture name classifier - per-file overrides", "[RoadClassifier]")
{
    // Suramar tile family — would NOT match via substring tokens (no "road",
    // no "cobble", no "path"). Caught by the per-file override allowlist.
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Tile01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Tile02_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Tile03_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Tile04_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Tile05_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_Tile06_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion06/Suramar/7SR_MoonGuardTile01_512.blp"));
    REQUIRE(IsRoadTexturePath("tileset/expansion07/kultiraszone/8kul_citytile01_512.blp"));
}

TEST_CASE("Road texture name classifier - case and slash insensitivity", "[RoadClassifier]")
{
    // Same logical texture, mixed case + mixed slash style.
    REQUIRE(IsRoadTexturePath("TILESET/ELWYNN/ElwynnCobbleStoneBase.blp"));
    REQUIRE(IsRoadTexturePath("tileset/elwynn/elwynncobblestonebase.blp"));
    REQUIRE(IsRoadTexturePath("Tileset\\Elwynn\\ElwynnCobbleStoneBase.blp"));
    REQUIRE(IsRoadTexturePath("TILESET\\ELWYNN\\ELWYNNCOBBLESTONEBASE.BLP"));
}

TEST_CASE("Road texture name classifier - negative cases (true negatives)", "[RoadClassifier]")
{
    // Plain dirt — explicitly EXCLUDED per catalog §4.3.
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/ELWYNN/ElwynnDirt.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/Westfall/WestfallDirt.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/Barrens/BarrensDirt.blp"));

    // Bare "_Tile" with NUMERIC suffix — superseded by 2026-05-20 audit:
    // these ARE roads (Uldum temple plazas, Zul'Drak Argent Stand). The
    // _tile + plaza tokens now catch them. Instance-map runtime gating
    // (PathGenerator::CreateFilter checking IsDungeon/IsRaid) handles
    // false-positive risk inside dungeons.
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION03/ULDUM/UL_Tiles01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/ZULDRAK/ZD_Plaza_TileA.blp"));

    // Grass, snow, rock — not roads.
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/ELWYNN/ElwynnGrass.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/EXPANSION02/DRAGONBLIGHT/DB_SnowA.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("tileset/expansion10/11ea_stones_dark01_256.blp"));

    // Liquid / lava / water.
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/Generic/Water01.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/Hellfire/HellfireLava01.blp"));

    // Non-terrain assets (icons, UI, doodads). Even if the path contains
    // "road"-like tokens, it must be inside tileset/ to qualify.
    REQUIRE_FALSE(IsRoadTexturePath("INTERFACE/ICONS/Spell_Holy_PathOfLight.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("Creature/Goldshire/Roadway_NPC.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("World/Generic/Doodads/RoadSign01.blp"));

    // Garrison plot tile — `_tile` token now catches this, which is fine:
    // garrisons are phased instance maps and the runtime PathGenerator
    // disables road bonus on instance maps. The classifier-level tag is
    // harmless data; the runtime decides whether to use it.
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION05/SHADOWMOON/6SM_GARRISON_TILE.BLP"));

    // Empty / nonsense.
    REQUIRE_FALSE(IsRoadTexturePath(""));
    REQUIRE_FALSE(IsRoadTexturePath("notatexturepath"));
}

TEST_CASE("Road texture name classifier - sibling map files are road-consistent",
          "[RoadClassifier]")
{
    // Albedo BLP and its specular/height siblings should all be classified
    // the same way even though we'll only ever see the albedo in MTEX.
    // (Documented invariant from catalog §1.4.)
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/LAKEWINTERGRASP/WG_TitanRoadSnow01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/LAKEWINTERGRASP/WG_TitanRoadSnow01_s.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/EXPANSION02/LAKEWINTERGRASP/WG_TitanRoadSnow01_h.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion10/11UDM_BrickRoad01_Early_512.blp"));
}

TEST_CASE("MatchedRoadToken - identifies which token matched", "[RoadClassifier]")
{
    // Substring matches return the token used.
    REQUIRE(MatchedRoadToken("TILESET/ELWYNN/ElwynnCobbleStoneBase.blp") == "cobble");
    REQUIRE(MatchedRoadToken("TILESET/Wetlands/Wetlandsroad01.blp") == "road01");
    REQUIRE(MatchedRoadToken("TILESET/EXPANSION02/DRAGONBLIGHT/DB_FlagstonesA.blp") == "flagstone");
    REQUIRE(MatchedRoadToken("TILESET/EXPANSION03/LOSTISLES/Goblin_Pavement_01.blp") == "pavement");
    REQUIRE(MatchedRoadToken("Tileset/Expansion06/7XP_LeyRoad01_512.blp") == "leyroad");
    REQUIRE(MatchedRoadToken("tileset/expansion07/general/8war_stoneroad01_512.blp") == "stoneroad");
    REQUIRE(MatchedRoadToken("tileset/expansion10/11ar_hexroad01_512.blp") == "hexroad");

    // Per-file override returns the full path (lowercased).
    REQUIRE(MatchedRoadToken("Tileset/Expansion06/Suramar/7SR_Tile01_512.blp")
            == "tileset/expansion06/suramar/7sr_tile01_512.blp");

    // No match returns empty.
    REQUIRE(MatchedRoadToken("TILESET/ELWYNN/ElwynnGrass.blp").empty());
    REQUIRE(MatchedRoadToken("").empty());
}

// =============================================================================
// 2026-05-20 listfile audit additions: 10 new tokens from the corpus pass.
// =============================================================================

TEST_CASE("Road texture classifier - audit additions (2026-05-20)",
          "[RoadClassifier]")
{
    // Pandaria / WoD plaza tiles — caught by bare _tile token.
    REQUIRE(IsRoadTexturePath("TILESET/expansion04/isleofthethunderking/itk_tile01_1024.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion04/valeofeternalblossoms/veb_tile01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion04/kunlaisummit/kls_tile01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion05/talador/6td_draeneitile01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion05/nagrand/6ng_ogretile01_512.blp"));

    // BfA Zandalar / Vol'dun / Warfronts.
    REQUIRE(IsRoadTexturePath("TILESET/expansion07/desertzone/8des_tile01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion07/general/8war_tile01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion07/zuldazarzone/8zul_tile01_512.blp"));

    // Dragonflight + Thaldraszus titan plaza.
    REQUIRE(IsRoadTexturePath("TILESET/expansion09/10dg_tile01_512.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion09/10ti_tile01_512.blp"));

    // Plaza family.
    REQUIRE(IsRoadTexturePath("TILESET/expansion02/lakewintergrasp/wg_titanplaza01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion02/zuldrak/zd_plaza_tilea.blp"));

    // Slab family.
    REQUIRE(IsRoadTexturePath("TILESET/expansion03/vashjir/vj_slabcracked.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion04/valleyoffourwinds/vfw_slabs01_512.blp"));

    // Tier B compounds.
    REQUIRE(IsRoadTexturePath("TILESET/araithihighlands/arathihighlandscityfloor.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion02/dragonblight/db_titanfloor01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion02/dragonblight/db_scourgefloor01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion03/uldum/ul_floor_tiles_01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion06/valsharah/7vs_blackrookfloor_01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion06/valsharah/7vs_nightmarefloor_01.blp"));
    REQUIRE(IsRoadTexturePath("TILESET/expansion03/deepholm/dh_trogtown01.blp"));
}

TEST_CASE("Road texture classifier - audit additions do not match organic floors",
          "[RoadClassifier]")
{
    // Defensive: the audit warned against adding a bare "floor" token
    // because it would FP on jungle/forest/mulch. Verify those still
    // return false.
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/STV/StvJungleFloor01.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/Hinterland/HinterlandForestFloor01.blp"));
    REQUIRE_FALSE(IsRoadTexturePath("TILESET/Generic/MulchFloor02.blp"));
}

// =============================================================================
// IsRoadTexturePathForWmo — WMO-scope variant (no tileset/ gate)
// =============================================================================

TEST_CASE("IsRoadTexturePathForWmo - accepts non-tileset/ road paths",
          "[RoadClassifier][Wmo]")
{
    // WMO materials live under world/, dungeons/textures/, world/azeroth/
    // etc. They were rejected by the ADT-scope IsRoadTexturePath because
    // the path lacks "tileset/". The WMO-scope variant must accept them.
    REQUIRE(IsRoadTexturePathForWmo("world/azeroth/elwynn/buildings/stormwindentrance/lionroad.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/floor/mm_street_03.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/brick/mm_redbrick_03x.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/dalaran/eb_dalaran_cobble001.blp"));

    // Still also accepts tileset/ paths (used in WMOs that reference
    // shared terrain textures).
    REQUIRE(IsRoadTexturePathForWmo("tileset/stormwindcity/sw_cobble_a.blp"));
}

TEST_CASE("Road texture classifier - Midnight-listfile audit additions",
          "[RoadClassifier]")
{
    // 2026-05-20 (evening) Midnight-listfile audit. ADT-scope cases:
    REQUIRE(IsRoadTexturePath("Tileset/Expansion11/12elw_riverstones01_1024.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion10/11ar_stones01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion10/11krs_stones02_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion11/12vdl_stones01_512.blp"));

    // _stones01/_stones02 should NOT match the unrelated single-stone
    // file or higher-numbered stones (defensive: only _stones01 and
    // _stones02 are walkable per the audit).
    REQUIRE_FALSE(IsRoadTexturePath("Tileset/Expansion10/11ea_singlestone.blp"));
}

TEST_CASE("Road texture classifier - WMO-scope Midnight audit additions",
          "[RoadClassifier][Wmo]")
{
    // marble — Karazhan, Sunken Temple, Maw, 11go Midnight Undermine
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/11go_goblin/11go_goblin_marble_01.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/karazhan/7du_karazhan_marble.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/sunkentemple/sunkentemple_marble01.blp"));

    // wood_floor — Garrison + Midnight Quel'Thalas + Amani + Westfall
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/12hu_human/12hu_human_wood_floor_01.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/12tr_amani/12tr_amani_wood_floor_01.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/floor/mm_westfall_wood_floor_01.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/6hu_garrison/kk_wood_floor_01.blp"));

    // stone_floor — Garrison + Pandaren + Midnight 12hu
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/12hu_human/12hu_human_stone_floor_01.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/pandaren/base_houses/pa_stone_floor.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/bridge/dk_stone_floor_tile01.blp"));

    // floortile — Midnight Undermine, Sargeras, Ulduar
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/11go_goblin/11go_goblin_stonefloortile.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/7du_sargeras/floortile_01.blp"));
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/ulduar/ulduar_floortile_01.blp"));

    // street — already shipped pre-Midnight, but re-verify Midnight content.
    REQUIRE(IsRoadTexturePathForWmo("dungeons/textures/floor/mm_street_03.blp"));

    // tile0 — should match draeneitile/ogretile (no-underscore separator).
    REQUIRE(IsRoadTexturePath("Tileset/Expansion05/Talador/6td_draeneitile01_512.blp"));
    REQUIRE(IsRoadTexturePath("Tileset/Expansion05/Nagrand/6ng_ogretile01_512.blp"));
}

TEST_CASE("IsRoadTexturePathForWmo - defensive prefix rejection",
          "[RoadClassifier][Wmo]")
{
    // Tier D risk from the listfile audit: WMOs CAN reference any
    // FileDataID (broadsword*.blp has "road" substring; UI roadalpha
    // exists too). The WMO classifier must reject these namespaces up
    // front so we don't accidentally tag a building's collision mesh as
    // road because a stale item BLP got referenced.
    REQUIRE_FALSE(IsRoadTexturePathForWmo("item/objectcomponents/weapon/sword_2h_broadsword_a_01.blp"));
    REQUIRE_FALSE(IsRoadTexturePathForWmo("interface/glues/models/ui_dwarf/roadalpha.blp"));
    REQUIRE_FALSE(IsRoadTexturePathForWmo("character/human/skin01.blp"));
    REQUIRE_FALSE(IsRoadTexturePathForWmo("creature/dragon/dragon_brick_skin.blp"));
    REQUIRE_FALSE(IsRoadTexturePathForWmo("spells/spell_holy_road.blp"));
    REQUIRE_FALSE(IsRoadTexturePathForWmo("icon/itemicon/cobblestone.blp"));
}

// =============================================================================
// RoadConfidenceFromEffectId / GroundEffectTexture state
// =============================================================================

TEST_CASE("RoadConfidenceFromEffectId - default behavior (no table loaded)",
          "[RoadClassifier]")
{
    ClearGroundEffectTable();
    REQUIRE(RoadConfidenceFromEffectId(0) == Approx(0.5f));
    REQUIRE(RoadConfidenceFromEffectId(42) == Approx(0.5f));
    REQUIRE(RoadConfidenceFromEffectId(999999) == Approx(0.5f));
}

TEST_CASE("RoadConfidenceFromEffectId - with table populated", "[RoadClassifier]")
{
    ClearGroundEffectTable();

    SetGroundEffectConfidenceForTesting(10, 0.9f);   // strong road signal
    SetGroundEffectConfidenceForTesting(20, 0.1f);   // strong vegetation signal
    SetGroundEffectConfidenceForTesting(30, 0.5f);   // neutral

    REQUIRE(RoadConfidenceFromEffectId(10) == Approx(0.9f));
    REQUIRE(RoadConfidenceFromEffectId(20) == Approx(0.1f));
    REQUIRE(RoadConfidenceFromEffectId(30) == Approx(0.5f));

    // Holes in the table default to 0.5 (no signal).
    REQUIRE(RoadConfidenceFromEffectId(0) == Approx(0.5f));
    REQUIRE(RoadConfidenceFromEffectId(15) == Approx(0.5f));

    // Indices past the table end ALSO default to 0.5.
    REQUIRE(RoadConfidenceFromEffectId(9999) == Approx(0.5f));

    ClearGroundEffectTable();
}

TEST_CASE("RoadConfidenceFromEffectId - clamps out-of-range values", "[RoadClassifier]")
{
    ClearGroundEffectTable();

    SetGroundEffectConfidenceForTesting(5, -0.5f);   // should clamp to 0
    SetGroundEffectConfidenceForTesting(6, 1.5f);    // should clamp to 1

    REQUIRE(RoadConfidenceFromEffectId(5) == Approx(0.0f));
    REQUIRE(RoadConfidenceFromEffectId(6) == Approx(1.0f));

    ClearGroundEffectTable();
}

TEST_CASE("SetGroundEffectTable - bulk replacement", "[RoadClassifier]")
{
    ClearGroundEffectTable();

    std::vector<GroundEffectInfo> table(50);
    table[10].roadConfidence = 0.9f;
    table[20].roadConfidence = 0.1f;
    // table[5] retains the default-constructed 0.5f.

    SetGroundEffectTable(std::move(table));

    REQUIRE(RoadConfidenceFromEffectId(10) == Approx(0.9f));
    REQUIRE(RoadConfidenceFromEffectId(20) == Approx(0.1f));
    REQUIRE(RoadConfidenceFromEffectId(5) == Approx(0.5f));
    REQUIRE(RoadConfidenceFromEffectId(100) == Approx(0.5f));   // past end

    ClearGroundEffectTable();
    REQUIRE(RoadConfidenceFromEffectId(10) == Approx(0.5f));
}

// =============================================================================
// ComputeRoadConfidence — pure heuristic over a GroundEffectTexture record
// =============================================================================

namespace
{
    Road::GroundEffectRecord MakeRecord(uint32 id, uint32 density, uint8 sound = 0)
    {
        Road::GroundEffectRecord r;
        r.id = id;
        r.density = density;
        r.sound = sound;
        return r;
    }
}

TEST_CASE("ComputeRoadConfidence - null effect IDs return 0.5", "[RoadClassifier]")
{
    Road::GroundEffectRecord r{};
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.5f));   // id == 0

    r.id = 0xFFFFFFFFu;
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.5f));   // -1 sentinel
}

TEST_CASE("ComputeRoadConfidence - no doodads = paved (~0.80)",
          "[RoadClassifier]")
{
    Road::GroundEffectRecord r = MakeRecord(100, 0);
    // All doodadId slots are zero.
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.80f));
}

TEST_CASE("ComputeRoadConfidence - light vegetation (weight 1-2) returns ~0.60",
          "[RoadClassifier]")
{
    Road::GroundEffectRecord r = MakeRecord(101, 0);
    r.doodadId[0] = 42;
    r.doodadWeight[0] = 2;   // totalWeight == 2 → 0.60

    REQUIRE(ComputeRoadConfidence(r) == Approx(0.60f));
}

TEST_CASE("ComputeRoadConfidence - moderate vegetation (weight 3-4) returns 0.50",
          "[RoadClassifier]")
{
    Road::GroundEffectRecord r = MakeRecord(102, 0);
    r.doodadId[0] = 1;
    r.doodadId[1] = 2;
    r.doodadWeight[0] = 2;
    r.doodadWeight[1] = 2;   // totalWeight == 4 → 0.50

    REQUIRE(ComputeRoadConfidence(r) == Approx(0.50f));
}

TEST_CASE("ComputeRoadConfidence - heavy vegetation (weight 5-12) returns 0.30",
          "[RoadClassifier]")
{
    Road::GroundEffectRecord r = MakeRecord(103, 0);
    r.doodadId[0] = 1;
    r.doodadId[1] = 2;
    r.doodadId[2] = 3;
    r.doodadWeight[0] = 4;
    r.doodadWeight[1] = 4;
    r.doodadWeight[2] = 4;   // totalWeight == 12 → 0.30

    REQUIRE(ComputeRoadConfidence(r) == Approx(0.30f));
}

TEST_CASE("ComputeRoadConfidence - very heavy vegetation (weight > 12) returns 0.20",
          "[RoadClassifier]")
{
    Road::GroundEffectRecord r = MakeRecord(104, 0);
    r.doodadId[0] = 1;
    r.doodadId[1] = 2;
    r.doodadId[2] = 3;
    r.doodadId[3] = 4;
    r.doodadWeight[0] = 10;
    r.doodadWeight[1] = 10;
    r.doodadWeight[2] = 10;
    r.doodadWeight[3] = 10;   // totalWeight == 40 → 0.20

    REQUIRE(ComputeRoadConfidence(r) == Approx(0.20f));
}

TEST_CASE("ComputeRoadConfidence - density penalty caps at -0.10",
          "[RoadClassifier]")
{
    // No doodads → base confidence 0.80.
    // Density 8 → penalty 0. Density 28 → penalty (28-8)*0.005 = 0.10.
    // Density 1000 → penalty clamps to 0.10.
    Road::GroundEffectRecord r = MakeRecord(110, 8);
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.80f));

    r.density = 28;
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.70f));

    r.density = 1000;
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.70f));   // penalty clamps
}

TEST_CASE("ComputeRoadConfidence - density penalty does not go below 0",
          "[RoadClassifier]")
{
    Road::GroundEffectRecord r = MakeRecord(111, 0);   // density 0 < 8
    // (0 - 8) * 0.005 = -0.04, but clamp says penalty floor is 0.
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.80f));
}

TEST_CASE("ComputeRoadConfidence - negative DoodadWeight values are ignored",
          "[RoadClassifier]")
{
    // Per agent's findings: negative weights are semantic-noise; we treat
    // them as zero so they don't accidentally lower totalWeight.
    Road::GroundEffectRecord r = MakeRecord(120, 0);
    r.doodadId[0] = 1;
    r.doodadId[1] = 2;
    r.doodadWeight[0] = -50;   // ignored
    r.doodadWeight[1] = 3;     // counts

    // filledSlots == 2 (both doodadIds nonzero), totalWeight == 3.
    // 3 is in the (2 < w <= 4) bucket → 0.50.
    REQUIRE(ComputeRoadConfidence(r) == Approx(0.50f));
}

TEST_CASE("ComputeRoadConfidence - zero DoodadID means slot is empty",
          "[RoadClassifier]")
{
    // A row could have DoodadID == 0 but DoodadWeight != 0 (data corruption
    // or unused slot). The empty doodadId zeroes out the slot's contribution.
    Road::GroundEffectRecord r = MakeRecord(130, 0);
    r.doodadId[0] = 0;
    r.doodadWeight[0] = 100;   // ignored — doodadId is 0
    // No other slots filled → filledSlots == 0 → 0.80.

    REQUIRE(ComputeRoadConfidence(r) == Approx(0.80f));
}

TEST_CASE("ComputeRoadConfidence - final value clamps to [0, 1]",
          "[RoadClassifier]")
{
    // Verify the output is always in [0, 1] regardless of input.
    Road::GroundEffectRecord r = MakeRecord(140, 1000000);   // extreme density
    r.doodadId[0] = 1;
    r.doodadWeight[0] = 100;

    float v = ComputeRoadConfidence(r);
    REQUIRE(v >= 0.0f);
    REQUIRE(v <= 1.0f);
}

// =============================================================================
// ApplyContiguousAreaFilter — 16x16 flood-fill connected-component filter
// =============================================================================

namespace
{
    constexpr std::size_t kSide = Road::kMcnksPerSide;

    // Helper: set a rectangular block of MCNKs to road (value 1).
    void PaintRect(Road::McnkGrid& grid, std::size_t r0, std::size_t c0,
                   std::size_t height, std::size_t width)
    {
        for (std::size_t r = r0; r < r0 + height; ++r)
            for (std::size_t c = c0; c < c0 + width; ++c)
                grid[r * kSide + c] = 1;
    }

    std::size_t CountRoadCells(Road::McnkGrid const& grid)
    {
        return std::count_if(grid.begin(), grid.end(),
                             [](uint8 v) { return v != 0; });
    }
}

TEST_CASE("Contiguous-area filter - removes isolated single cell", "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    grid[5 * kSide + 5] = 1;

    REQUIRE(CountRoadCells(grid) == 1);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 1);
    REQUIRE(CountRoadCells(grid) == 0);
}

TEST_CASE("Contiguous-area filter - removes small clusters below threshold",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // 1x3 strip = 3 cells, below default threshold of 4.
    PaintRect(grid, 2, 2, 1, 3);

    REQUIRE(CountRoadCells(grid) == 3);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 3);
    REQUIRE(CountRoadCells(grid) == 0);
}

TEST_CASE("Contiguous-area filter - keeps clusters at or above threshold",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // 2x2 block = 4 cells, exactly the default threshold.
    PaintRect(grid, 4, 4, 2, 2);

    REQUIRE(CountRoadCells(grid) == 4);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 0);
    REQUIRE(CountRoadCells(grid) == 4);
}

TEST_CASE("Contiguous-area filter - keeps long road keeps small spur dropped",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // Long horizontal road (1x10 = 10 cells, well above threshold).
    PaintRect(grid, 8, 1, 1, 10);
    // Isolated 2-cell spur far away.
    grid[1 * kSide + 1] = 1;
    grid[1 * kSide + 2] = 1;

    REQUIRE(CountRoadCells(grid) == 12);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 2);   // only the spur
    REQUIRE(CountRoadCells(grid) == 10);

    // The long road survives.
    for (std::size_t c = 1; c <= 10; ++c)
        REQUIRE(grid[8 * kSide + c] == 1);
    // The spur is gone.
    REQUIRE(grid[1 * kSide + 1] == 0);
    REQUIRE(grid[1 * kSide + 2] == 0);
}

TEST_CASE("Contiguous-area filter - L-shape is one component",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // L-shape: vertical 1x3 + horizontal 3x1 sharing a corner = 5 cells.
    grid[3 * kSide + 3] = 1;
    grid[4 * kSide + 3] = 1;
    grid[5 * kSide + 3] = 1;
    grid[5 * kSide + 4] = 1;
    grid[5 * kSide + 5] = 1;

    REQUIRE(CountRoadCells(grid) == 5);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 0);     // 5 >= 4 threshold, kept
    REQUIRE(CountRoadCells(grid) == 5);
}

TEST_CASE("Contiguous-area filter - diagonal-only neighbors are NOT connected",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // Two cells touching only at the corner — under 4-connectivity these
    // are SEPARATE components.
    grid[5 * kSide + 5] = 1;
    grid[6 * kSide + 6] = 1;

    REQUIRE(CountRoadCells(grid) == 2);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 2);
    REQUIRE(cleared == 2);     // both components are size 1 < 2
    REQUIRE(CountRoadCells(grid) == 0);
}

TEST_CASE("Contiguous-area filter - cells on grid edges", "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // 3-cell road in the corner — touches edges, must not crash.
    grid[0 * kSide + 0] = 1;
    grid[0 * kSide + 1] = 1;
    grid[1 * kSide + 0] = 1;

    REQUIRE(CountRoadCells(grid) == 3);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 3);     // below threshold
    REQUIRE(CountRoadCells(grid) == 0);

    // Now a 5-cell L-shape in the corner — above threshold, must survive.
    grid[15 * kSide + 15] = 1;
    grid[15 * kSide + 14] = 1;
    grid[15 * kSide + 13] = 1;
    grid[14 * kSide + 15] = 1;
    grid[13 * kSide + 15] = 1;

    REQUIRE(CountRoadCells(grid) == 5);
    cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 0);
    REQUIRE(CountRoadCells(grid) == 5);
}

TEST_CASE("Contiguous-area filter - full grid stays intact",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    grid.fill(1);   // all 256 cells flagged

    REQUIRE(CountRoadCells(grid) == 256);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 0);
    REQUIRE(CountRoadCells(grid) == 256);
}

TEST_CASE("Contiguous-area filter - empty grid is a no-op",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};

    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 0);
    REQUIRE(CountRoadCells(grid) == 0);
}

TEST_CASE("Contiguous-area filter - threshold of 1 keeps everything",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};
    // Scatter isolated cells across the grid.
    grid[0 * kSide + 0] = 1;
    grid[3 * kSide + 7] = 1;
    grid[10 * kSide + 10] = 1;
    grid[15 * kSide + 15] = 1;

    std::size_t cleared = ApplyContiguousAreaFilter(grid, 1);
    REQUIRE(cleared == 0);
    REQUIRE(CountRoadCells(grid) == 4);

    cleared = ApplyContiguousAreaFilter(grid, 0);   // threshold 0 = noop
    REQUIRE(cleared == 0);
    REQUIRE(CountRoadCells(grid) == 4);
}

TEST_CASE("Contiguous-area filter - multiple components mixed sizes",
          "[RoadClassifier]")
{
    Road::McnkGrid grid{};

    // Component A: 1 cell at (1,1) — kill.
    grid[1 * kSide + 1] = 1;

    // Component B: 2 cells at (3,3)-(3,4) — kill.
    grid[3 * kSide + 3] = 1;
    grid[3 * kSide + 4] = 1;

    // Component C: 4 cells (2x2) at (6,6)-(7,7) — keep.
    PaintRect(grid, 6, 6, 2, 2);

    // Component D: 9 cells (3x3) at (10,10)-(12,12) — keep.
    PaintRect(grid, 10, 10, 3, 3);

    REQUIRE(CountRoadCells(grid) == 1 + 2 + 4 + 9);
    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 1 + 2);
    REQUIRE(CountRoadCells(grid) == 4 + 9);

    // Verify specific cells.
    REQUIRE(grid[1 * kSide + 1] == 0);
    REQUIRE(grid[3 * kSide + 3] == 0);
    REQUIRE(grid[6 * kSide + 6] == 1);
    REQUIRE(grid[10 * kSide + 10] == 1);
    REQUIRE(grid[12 * kSide + 12] == 1);
}

TEST_CASE("Contiguous-area filter - preserves non-zero values (not just 1)",
          "[RoadClassifier]")
{
    // Future-proofing: the road mask is uint8 not bool so it can encode
    // additional info (bridge=0x02, special=0x04, etc.). The filter should
    // treat any nonzero value as "road" and PRESERVE the value when keeping.
    Road::McnkGrid grid{};
    PaintRect(grid, 4, 4, 2, 2);
    for (std::size_t r = 4; r < 6; ++r)
        for (std::size_t c = 4; c < 6; ++c)
            grid[r * kSide + c] = 0x07;   // non-1 sentinel

    std::size_t cleared = ApplyContiguousAreaFilter(grid, 4);
    REQUIRE(cleared == 0);
    REQUIRE(grid[4 * kSide + 4] == 0x07);
    REQUIRE(grid[5 * kSide + 5] == 0x07);
}

// =============================================================================
// End-to-end smoke test that ties the pieces together.
// =============================================================================

TEST_CASE("End-to-end - classify dominant texture + apply filter",
          "[RoadClassifier]")
{
    ClearGroundEffectTable();
    SetGroundEffectConfidenceForTesting(100, 0.9f);   // road-like ground effect
    SetGroundEffectConfidenceForTesting(200, 0.1f);   // vegetation ground effect

    auto classifyMcnk = [](std::string_view textureBlp, uint32 effectId) -> bool {
        return IsRoadTexturePath(textureBlp) &&
               RoadConfidenceFromEffectId(effectId) > 0.3f;
    };

    // Goldshire road dirt — texture YES + effect YES → road.
    REQUIRE(classifyMcnk("TILESET/ELWYNN/ElwynnCobbleStoneBase.blp", 100));

    // Same road texture but the ground-effect doodad list is a grass tuft
    // (effect confidence 0.1, below 0.3 threshold) → NOT road. This is the
    // "secondary signal vetoes" scenario per design doc §3.3.
    REQUIRE_FALSE(classifyMcnk("TILESET/ELWYNN/ElwynnCobbleStoneBase.blp", 200));

    // Grass texture under any effect ID → not a road.
    REQUIRE_FALSE(classifyMcnk("TILESET/ELWYNN/ElwynnGrass.blp", 100));
    REQUIRE_FALSE(classifyMcnk("TILESET/ELWYNN/ElwynnGrass.blp", 200));

    // No effect-table entry (effectId 0) → confidence defaults to 0.5,
    // above the 0.3 threshold → road verdict still applies for road textures.
    REQUIRE(classifyMcnk("TILESET/EXPANSION02/DRAGONBLIGHT/DB_TitanRoadA.blp", 0));

    ClearGroundEffectTable();
}

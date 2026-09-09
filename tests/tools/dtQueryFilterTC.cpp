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

#include "dtQueryFilterTC.h"

#include <cmath>

namespace
{
    // Make 3 floats in a row easier to write inline.
    struct V { float x, y, z; };

    float SlopeMul(V a, V b, float coef = 1.0f)
    {
        float pa[3] = { a.x, a.y, a.z };
        float pb[3] = { b.x, b.y, b.z };
        return dtQueryFilterTC::ComputeSlopeMultiplier(pa, pb, coef);
    }
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - horizontal segment is 1.0",
          "[dtQueryFilterTC]")
{
    // Pure horizontal: y unchanged.
    REQUIRE(SlopeMul({0, 0, 0}, {10, 0, 0}) == Approx(1.0f));
    REQUIRE(SlopeMul({0, 5, 0}, {10, 5, 7}) == Approx(1.0f));
    // Slope going DOWN (-y) is still treated the same.
    REQUIRE(SlopeMul({0, 100, 0}, {10, 100, 0}) == Approx(1.0f));
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - 45 deg slope is 1.45",
          "[dtQueryFilterTC]")
{
    // dy = dx; tan = 1; slope = 45 degrees
    float v = SlopeMul({0, 0, 0}, {10, 10, 0});
    REQUIRE(v == Approx(1.0f + 45.0f / 100.0f).margin(0.01f));
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - 30 deg slope is 1.30",
          "[dtQueryFilterTC]")
{
    // tan(30°) ≈ 0.5774; let dy = 5.774, horiz = 10 → slope ≈ 30°
    float v = SlopeMul({0, 0, 0}, {10, 5.774f, 0});
    REQUIRE(v == Approx(1.30f).margin(0.02f));
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - signed slope (going down) still penalized",
          "[dtQueryFilterTC]")
{
    // Slope down — we penalize EITHER direction because climbing down a
    // road that drops 30° is still a road to avoid.
    float up   = SlopeMul({0, 0, 0}, {10, 5.774f, 0});
    float down = SlopeMul({0, 5.774f, 0}, {10, 0, 0});
    REQUIRE(up == Approx(down).margin(0.001f));
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - clamps to 2.0",
          "[dtQueryFilterTC]")
{
    // Near-vertical (89 degrees): would naively give 1.89, fine.
    // Coefficient inflation: at coef=10, 30° → 1 + 30*10/100 = 4.0, clamps to 2.0.
    float v = SlopeMul({0, 0, 0}, {10, 5.774f, 0}, 10.0f);
    REQUIRE(v == Approx(2.0f));
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - coefficient 0 disables penalty",
          "[dtQueryFilterTC]")
{
    float v = SlopeMul({0, 0, 0}, {10, 100, 0}, 0.0f);
    REQUIRE(v == Approx(1.0f));
}

TEST_CASE("dtQueryFilterTC::ComputeSlopeMultiplier - pure-vertical segment returns 1.0",
          "[dtQueryFilterTC]")
{
    // Degenerate: horiz = 0 → would divide by zero. Should return 1.0
    // rather than infinity.
    float v = SlopeMul({5, 0, 5}, {5, 100, 5});
    REQUIRE(v == Approx(1.0f));
}

TEST_CASE("dtQueryFilterTC - default state: road bonus enabled, slope coef 1.0",
          "[dtQueryFilterTC]")
{
    dtQueryFilterTC f;
    REQUIRE_FALSE(f.GetDisableRoadBonus());
    REQUIRE(f.GetSlopeCoefficient() == Approx(1.0f));
}

TEST_CASE("dtQueryFilterTC - SetDisableRoadBonus toggles", "[dtQueryFilterTC]")
{
    dtQueryFilterTC f;
    f.SetDisableRoadBonus(true);
    REQUIRE(f.GetDisableRoadBonus());
    f.SetDisableRoadBonus(false);
    REQUIRE_FALSE(f.GetDisableRoadBonus());
}

TEST_CASE("dtQueryFilterTC - SetSlopeCoefficient is read back", "[dtQueryFilterTC]")
{
    dtQueryFilterTC f;
    f.SetSlopeCoefficient(0.5f);
    REQUIRE(f.GetSlopeCoefficient() == Approx(0.5f));
    f.SetSlopeCoefficient(0.0f);
    REQUIRE(f.GetSlopeCoefficient() == Approx(0.0f));
}

TEST_CASE("dtQueryFilterTC - area cost set/get works (inherited from dtQueryFilter)",
          "[dtQueryFilterTC]")
{
    dtQueryFilterTC f;
    f.setAreaCost(7, 0.5f);
    REQUIRE(f.getAreaCost(7) == Approx(0.5f));
    f.setAreaCost(11, 1.2f);
    REQUIRE(f.getAreaCost(11) == Approx(1.2f));
}

TEST_CASE("dtQueryFilterTC - BeginPathStats resets max slope tracker",
          "[dtQueryFilterTC]")
{
    // Default state: no segments evaluated yet → 1.0.
    dtQueryFilterTC f;
    REQUIRE(f.GetMaxSlopeFactorThisPath() == Approx(1.0f));
    f.BeginPathStats();
    REQUIRE(f.GetMaxSlopeFactorThisPath() == Approx(1.0f));
}

TEST_CASE("dtQueryFilterTC::ResetStats zeros global counters",
          "[dtQueryFilterTC]")
{
    // Call TallyPath with a few synthetic outcomes to bump counters,
    // then ResetStats and verify all zero.
    dtQueryFilterTC::TallyPath(/*mesh*/ nullptr, /*polys*/ nullptr,
                                /*count*/ 0,
                                /*roadBonusDisabled*/ true,
                                /*maxSlopeFactor*/ 1.5f,
                                /*inInstance*/ true,
                                /*mapId*/ 0);
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0,
                                false, 1.05f, false, 0);

    auto before = dtQueryFilterTC::SampleStats();
    REQUIRE(before.pathsRun >= 2);
    // Slope > 1.10 fires on first call only (1.5 vs 1.05).
    REQUIRE(before.pathsWithSlopePenalty >= 1);
    REQUIRE(before.pathsInInstance >= 1);
    REQUIRE(before.pathsRoadBonusDisabled >= 1);

    dtQueryFilterTC::ResetStats();
    auto after = dtQueryFilterTC::SampleStats();
    REQUIRE(after.pathsRun == 0);
    REQUIRE(after.pathsWithRoadPoly == 0);
    REQUIRE(after.pathsRoadBonusDisabled == 0);
    REQUIRE(after.pathsWithSlopePenalty == 0);
    REQUIRE(after.roadPolysVisited == 0);
    REQUIRE(after.totalPolysVisited == 0);
    REQUIRE(after.pathsInInstance == 0);
}

TEST_CASE("dtQueryFilterTC::TallyPath - slope threshold is 1.10",
          "[dtQueryFilterTC]")
{
    // Slope tracker fires when max factor > 1.10. Verify the boundary.
    dtQueryFilterTC::ResetStats();

    // Below threshold — should NOT count as slope-penalty path.
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.099f, false);
    REQUIRE(dtQueryFilterTC::SampleStats().pathsWithSlopePenalty == 0);

    // At threshold (1.10) — strict > so still no fire.
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.10f, false);
    REQUIRE(dtQueryFilterTC::SampleStats().pathsWithSlopePenalty == 0);

    // Above threshold — fires.
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.101f, false);
    REQUIRE(dtQueryFilterTC::SampleStats().pathsWithSlopePenalty == 1);

    dtQueryFilterTC::ResetStats();
}

TEST_CASE("dtQueryFilterTC::TallyPath - null mesh path still counts pathsRun",
          "[dtQueryFilterTC]")
{
    // Defensive: BuildShortcut paths or NOT_USING_PATH paths produce no
    // poly array. TallyPath should still increment pathsRun so the
    // operator sees ALL CalculatePath outcomes in /roadstats, not just
    // mesh-walking ones.
    dtQueryFilterTC::ResetStats();
    auto base = dtQueryFilterTC::SampleStats();
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.0f, false, 0);
    auto after = dtQueryFilterTC::SampleStats();
    REQUIRE(after.pathsRun == base.pathsRun + 1);
    REQUIRE(after.roadPolysVisited == 0);
    REQUIRE(after.totalPolysVisited == 0);
    dtQueryFilterTC::ResetStats();
}

TEST_CASE("dtQueryFilterTC - per-map stats track each map separately",
          "[dtQueryFilterTC]")
{
    dtQueryFilterTC::ResetStats();

    // 3 paths on map 0 (Eastern Kingdoms — IS tracked), 1 path on
    // map 1, 2 paths on map 530, 1 path with kNoMapId (skipped from
    // per-map but counted globally).
    for (int i = 0; i < 3; ++i)
        dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.0f, false, 0u);
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, true, 1.0f, false, 1u);
    for (int i = 0; i < 2; ++i)
        dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.2f, true, 530u);
    dtQueryFilterTC::TallyPath(nullptr, nullptr, 0, false, 1.0f, false,
                                dtQueryFilterTC::kNoMapId);

    // Map 0 (Eastern Kingdoms) IS now tracked per-map. The old gate
    // used `mapId != 0` and accidentally hid all EK pathfinding from
    // the operator's view.
    auto map0 = dtQueryFilterTC::SampleStatsForMap(0);
    REQUIRE(map0.pathsRun == 3);

    auto map1 = dtQueryFilterTC::SampleStatsForMap(1);
    REQUIRE(map1.pathsRun == 1);
    REQUIRE(map1.pathsRoadBonusDisabled == 1);

    auto map530 = dtQueryFilterTC::SampleStatsForMap(530);
    REQUIRE(map530.pathsRun == 2);
    REQUIRE(map530.pathsInInstance == 2);
    REQUIRE(map530.pathsWithSlopePenalty == 2);

    // kNoMapId path is NOT in any per-map bucket.
    auto noMap = dtQueryFilterTC::SampleStatsForMap(dtQueryFilterTC::kNoMapId);
    REQUIRE(noMap.pathsRun == 0);

    // Global counters see ALL paths regardless of map id.
    auto global = dtQueryFilterTC::SampleStats();
    REQUIRE(global.pathsRun == 7);
    REQUIRE(global.pathsInInstance == 2);

    // List should contain maps 0, 1, 530 — but not kNoMapId.
    auto maps = dtQueryFilterTC::ListMapsWithStats();
    REQUIRE(maps.size() == 3);
    bool has0 = false, has1 = false, has530 = false, hasSentinel = false;
    for (uint32 m : maps) {
        if (m == 0u) has0 = true;
        if (m == 1u) has1 = true;
        if (m == 530u) has530 = true;
        if (m == dtQueryFilterTC::kNoMapId) hasSentinel = true;
    }
    REQUIRE(has0);
    REQUIRE(has1);
    REQUIRE(has530);
    REQUIRE_FALSE(hasSentinel);

    dtQueryFilterTC::ResetStats();

    // After reset, per-map table is empty too.
    REQUIRE(dtQueryFilterTC::ListMapsWithStats().empty());
    REQUIRE(dtQueryFilterTC::SampleStatsForMap(0).pathsRun == 0);
    REQUIRE(dtQueryFilterTC::SampleStatsForMap(1).pathsRun == 0);
    REQUIRE(dtQueryFilterTC::SampleStatsForMap(530).pathsRun == 0);
}

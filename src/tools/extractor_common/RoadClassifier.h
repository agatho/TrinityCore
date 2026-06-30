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

#ifndef TRINITYCORE_ROAD_CLASSIFIER_H
#define TRINITYCORE_ROAD_CLASSIFIER_H

#include "Define.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Road
{
    // Per-MCNK road verdict. 16x16 grid covers one ADT tile.
    constexpr std::size_t kMcnksPerSide = 16;
    constexpr std::size_t kMcnkGridSize = kMcnksPerSide * kMcnksPerSide;

    using McnkGrid = std::array<uint8, kMcnkGridSize>;

    // Texture-name classifier. Pure function: case-insensitive, no I/O.
    // Returns true if the given BLP path identifies a road/path/pavement surface.
    //
    // Decision order:
    //   1. Path must contain "tileset/" (else: not a terrain texture).
    //   2. Match against per-file override allowlist (full-path equality).
    //   3. Match against any substring token in the allowlist.
    //   4. Otherwise: not a road.
    //
    // The substring/override tables are compiled-in constants; see RoadClassifier.cpp.
    bool IsRoadTexturePath(std::string_view blpPath);

    // The token, if any, that matched a given path under IsRoadTexturePath.
    // Empty string_view if no match. Useful for telemetry and FP/FN debugging
    // during the P1.2 validation pass.
    std::string_view MatchedRoadToken(std::string_view blpPath);

    // WMO-scope classifier. Same token + override matching as
    // IsRoadTexturePath, but skips the `tileset/` scope gate because WMO
    // materials live under a broader texture vocabulary (world/wmo/,
    // world/azeroth/, dungeons/textures/, ...). Materials referenced from
    // MOMT/MDID are by definition mesh-surface textures, so the scope
    // safety-belt isn't needed and would mis-reject every WMO road.
    //
    // Used by Phase 2 (WMO road sidecars). ADTs continue to use the
    // scoped variant so UI/icon/character textures can't false-positive
    // even if they share a substring with a road token.
    bool IsRoadTexturePathForWmo(std::string_view blpPath);

    // Same as MatchedRoadToken but for the WMO-scope classifier.
    std::string_view MatchedRoadTokenForWmo(std::string_view blpPath);

    // One record from GroundEffectTexture.db2.
    //
    // Schema sourced from ROAD_P10a_GROUND_EFFECT_TEXTURE.md. Modern layout
    // (12.0.x, LAYOUT 0xD93D5678) has 5 storage fields beyond the ID:
    //   Density (u32), Sound (u8), DoodadID[4] (u16),
    //   DoodadWeight[4] (i8), SplatDensity[4] (i8).
    //
    // SplatDensity (added 10.0.x) is not used by Phase A heuristic — kept
    // here so the loader can ingest the whole row without losing data.
    struct GroundEffectRecord
    {
        uint32 id           = 0;     // GroundEffectTexture.ID == MCLY.effectId
        uint32 density      = 0;
        uint8  sound        = 0;
        uint16 doodadId[4]      = { 0, 0, 0, 0 };
        int8   doodadWeight[4]  = { 0, 0, 0, 0 };
        int8   splatDensity[4]  = { 0, 0, 0, 0 };
    };

    // Pure heuristic: derive road confidence from a single GroundEffectTexture
    // record. Used by both the production loader (LoadGroundEffectTextureTable)
    // and unit tests.
    //
    // Phase A (current): doodad slot count + weight + density penalty.
    // Phase B (future): adds footstep-family signal via Sound -> Footstep
    // TerrainLookup cross-ref.
    //
    // Returns value in [0, 1] (clamped):
    //   id == 0 OR id == 0xFFFFFFFF        → 0.5  (no signal — null effect)
    //   no doodad slots filled              → ~0.80 (paved surface)
    //   light doodad coverage               → 0.30 .. 0.60
    //   heavy doodad coverage               → 0.20 (vegetated)
    //   penalty: high Density adds up to -0.10
    float ComputeRoadConfidence(GroundEffectRecord const& rec);

    // GroundEffectTexture-based confidence. Secondary classifier signal.
    // Returns value in [0.0, 1.0]:
    //   0.0  — strongly NOT a road (e.g. effectId clearly maps to a grass-tuft doodad set)
    //   0.5  — no signal (effectId == 0, or not present in the loaded table)
    //   1.0  — strongly road (e.g. no doodads at all, or paved-footstep sound family)
    //
    // The classifier rule (per RoadResearch00 brief, design doc §3.3):
    //   subcell_is_road = IsRoadTexturePath(blp) && RoadConfidenceFromEffectId(effectId) > 0.3
    //
    // If no GroundEffectTexture table has been loaded, this always returns 0.5
    // (degenerates to texture-only matching, which is acceptable for unit tests
    // and as a fallback when the DB2 isn't available).
    float RoadConfidenceFromEffectId(uint32 effectId);

    // Connected-component filter on the 16x16 MCNK road mask.
    //
    // For each connected component of road-flagged MCNKs (4-connectivity),
    // if the component contains fewer than minComponentSize cells, clear
    // those cells. This removes isolated false positives — a small plaza
    // floor tagged road by texture-only matching that doesn't form an
    // actual road network.
    //
    // Mutates grid in-place. Returns number of cells cleared.
    //
    // Default threshold (4) corresponds to ~133 yards of contiguous road
    // (MCNK = 33.33 yards). Tunable; see design doc §3.4 and §11.3.
    std::size_t ApplyContiguousAreaFilter(McnkGrid& grid, uint8 minComponentSize = 4);

    // ---- GroundEffectTexture table (populated by the loader) ----
    //
    // Public so the loader can populate it. Read by RoadConfidenceFromEffectId.
    // Indexed by GroundEffectTexture.ID == MCLY.effectId.
    //
    // The full schema is documented in ROAD_P10a_GROUND_EFFECT_TEXTURE.md.
    // For classification we only need a precomputed [0..1] confidence per ID.
    struct GroundEffectInfo
    {
        // Aggregate confidence in [0..1] that this effect set indicates a paved
        // (road-like) surface rather than a vegetated/dirt surface. Computed
        // once at load time from the DB2 fields (doodad counts, sound family).
        float roadConfidence = 0.5f;
    };

    // Replace the entire table. Called once after GroundEffectTexture.db2 loads.
    // Pass an empty vector to clear (e.g. for tests that want pure
    // texture-only behavior).
    //
    // The vector is indexed by effectId. Sparse IDs that don't appear in the
    // DB2 should map to a default-constructed GroundEffectInfo (confidence 0.5).
    void SetGroundEffectTable(std::vector<GroundEffectInfo> table);

    // For tests: install a specific confidence value for a specific effectId.
    // Resizes the table as needed. Confidence is clamped to [0, 1].
    void SetGroundEffectConfidenceForTesting(uint32 effectId, float confidence);

    // Clear all installed GroundEffectTexture data; subsequent
    // RoadConfidenceFromEffectId calls return 0.5 (no signal).
    void ClearGroundEffectTable();
}

#endif // TRINITYCORE_ROAD_CLASSIFIER_H

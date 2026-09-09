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

#include "RoadClassifier.h"

#include <algorithm>
#include <cstring>
#include <queue>

namespace Road
{
    // -----------------------------------------------------------------------
    // Compile-time data tables.
    //
    // Sourced from ROAD_RESEARCH_02_TEXTURE_CATALOG.md §5.1 and §5.2.
    // All tokens are lowercase; matching is case-insensitive via ToLower below.
    // -----------------------------------------------------------------------

    // Substring tokens. Order is significant only for MatchedRoadToken
    // diagnostics (first match wins).
    constexpr std::string_view kSubstringTokens[] = {
        // Primary "road" family (compound + numeric variants first to ensure
        // they win the diagnostic match over the bare "_road"/"road_" tokens).
        "stoneroad",
        "hexroad",
        "leyroad",
        "titanroad",
        "asphaltroad",
        "brickroad",
        "rockroad",
        "farmroad",
        "oldroad",
        "scarletroad",
        "elfroad",
        "logroada",
        "logroadb",
        "logroad",
        "silvermoonroad",
        "nightmareroad",
        "glassroad",
        "sethrakroad",
        "warfronts_road",
        "roadtile",
        "road01",
        "road02",
        "road03",
        "road04",
        "road05",
        "road06",
        "_road",
        "road_",
        "road.blp",

        // Bare "road" fallback. Catches the <Word>Road<Word> pattern
        // (no underscore separator) that the more specific tokens above
        // miss: PlaguedRoadStone01.blp, EastPlaguedRoadBase.blp,
        // NagrandRoadClean.blp, TerokkarForest_RoadBrown.blp's siblings,
        // IronForgeRock06road2.blp's single-digit-suffix variants, etc.
        //
        // The tileset/ scope check guards against FPs in non-terrain assets
        // (UI icons, doodads, models). The catalog §1.3 lists "road" as the
        // canonical token — the compound tokens above are kept first only so
        // that MatchedRoadToken returns the most specific name for telemetry.
        "road",

        // Cobblestone family.
        "cobble",

        // Path family. _path / path_ / stonepath / tilepath.
        "stonepath",
        "tilepath",
        "_path",
        "path_",

        // Stone-class surfaces.
        "flagstone",
        "pavement",
        "brick",

        // Plaza / tile family — added 2026-05-20 after listfile audit
        // turned up Pandaria/WoD/Legion/BfA/DF walkable plazas missed by
        // the road/cobble/brick tokens. Compound tokens FIRST so
        // MatchedRoadToken returns the most specific name for telemetry;
        // bare "_tile" is a catch-all last.
        "cityfloor",       // Stromgarde plaza
        "titanfloor",      // Dragonblight Wyrmrest plaza
        "scourgefloor",    // Dragonblight Naxxramas approach
        "blackrookfloor",  // Val'sharah Black Rook Hold approach
        "nightmarefloor",  // Val'sharah Emerald Nightmare paving
        "floor_tiles",     // Uldum temple plazas
        "trogtown",        // Deepholm troggen town
        "plaza",           // Wintergrasp titan plaza, Zul'Drak Argent Stand
        "slab",            // Vashjir + Valley of the Four Winds paths
        "_tile",           // generic plaza/tile catch-all — Pandaria, WoD,
                           // Legion, BfA, DF — base files + _s/_h variants.
                           // Note: this token makes the suramar/8kul
                           // overrides below redundant but they're kept
                           // for explicit traceability.
        "tile0",           // numbered tile variant: catches the "no
                           // underscore separator" cases that `_tile`
                           // misses — `draeneitile01`, `ogretile01`,
                           // `blossomtile01` — plus reinforces matching
                           // on `_tile01.blp` files. Restricted to
                           // numbered suffixes so it doesn't FP on
                           // `tile.blp` substring noise.
        "street",          // dungeon WMO street textures — `mm_street_03`,
                           // `7dal_street`, etc. ADT `tileset/` paths
                           // rarely use this token (terrain road tokens
                           // dominate), but WMO collision materials do.

        // 2026-05-20 (evening) Midnight-listfile audit additions —
        // Quel'Thalas/Goblin/Amani new dungeon tilesets + retroactive
        // catch-up for vanilla Karazhan/Sunken Temple/Garrison/Westfall
        // floors that the prior audits missed.
        "marble",          // Karazhan, Sunken Temple, Valhalla, DireMaul,
                           // 11go Midnight Undermine, 9maw Maw rock —
                           // 55 paths total. UI/icon marble hits are
                           // blocked by the WMO prefix guard.
        "wood_floor",      // Garrison, Nightmare raid, Argent Crusade,
                           // Westfall/Duskwood, 12hu Midnight Quel'Thalas,
                           // 12tr Amani remake — 31 paths.
        "stone_floor",     // Garrison, Xenodar, Pandaren bases, Draenei,
                           // Eredar, dk_bridge, 12hu Midnight — 38 paths.
        "floortile",       // 11go Undermine + Sargeras + Warden Prison +
                           // Mage + Ulduar + Abyssal Maw + Mantid + vanilla
                           // Goblin/Kezan — 32 paths.
        "riverstones",     // 12elw + 12esw Midnight river-ford ADT surfaces
                           // — 9 base BLPs (~27 with _s/_h variants).
        "_stones01",       // Numbered loose-stone walkable ADT surface —
                           // 12vdl, 11ar, 11krs, 11ea_dark, 9du, 9prg,
                           // 10can, 10cav, 10hgl, 7az.
        "_stones02",       // Sibling of _stones01 for ADTs that ship two
                           // walkable-stone tile variants.
    };

    // Per-file overrides for textures that are roads but don't match any
    // substring token. Lowercased, forward-slash-normalized.
    constexpr std::string_view kOverridePaths[] = {
        // Suramar Nightborne pavement (named *_Tile* not *_Road*).
        "tileset/expansion06/suramar/7sr_tile01_512.blp",
        "tileset/expansion06/suramar/7sr_tile02_512.blp",
        "tileset/expansion06/suramar/7sr_tile03_512.blp",
        "tileset/expansion06/suramar/7sr_tile04_512.blp",
        "tileset/expansion06/suramar/7sr_tile05_512.blp",
        "tileset/expansion06/suramar/7sr_tile06_512.blp",
        "tileset/expansion06/suramar/7sr_moonguardtile01_512.blp",

        // Boralus city plaza floor.
        "tileset/expansion07/kultiraszone/8kul_citytile01_512.blp",
    };

    // -----------------------------------------------------------------------
    // GroundEffectTexture lookup state. File-local; mutated by setters above.
    // -----------------------------------------------------------------------

    namespace
    {
        std::vector<GroundEffectInfo> g_groundEffectTable;

        // Convert one character to lowercase ASCII without locale dependency.
        // BLP paths are pure ASCII; std::tolower is locale-sensitive and was
        // observed to misbehave under MSVC + non-C locales.
        constexpr char AsciiToLower(char c) noexcept
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }

        // Lowercase + normalize backslashes to forward slashes in a single pass.
        // BLP paths in the listfile use forward slashes; ADT MTEX strings as
        // shipped by Blizzard use backslashes. We normalize once so the rest
        // of the classifier is path-style-agnostic.
        std::string Normalize(std::string_view in)
        {
            std::string out;
            out.reserve(in.size());
            for (char c : in)
                out.push_back(c == '\\' ? '/' : AsciiToLower(c));
            return out;
        }

        // True if normalized path is scoped to the tileset/ root.
        bool IsTilesetPath(std::string_view normalized) noexcept
        {
            return normalized.find("tileset/") != std::string_view::npos;
        }
    }

    // -----------------------------------------------------------------------
    // Texture name classifier.
    // -----------------------------------------------------------------------

    std::string_view MatchedRoadToken(std::string_view blpPath)
    {
        std::string const normalized = Normalize(blpPath);
        if (!IsTilesetPath(normalized))
            return {};

        // Per-file overrides first — they're rare but authoritative.
        for (std::string_view const& full : kOverridePaths)
            if (normalized == full)
                return full;

        // Substring tokens.
        for (std::string_view const& tok : kSubstringTokens)
            if (normalized.find(tok) != std::string_view::npos)
                return tok;

        return {};
    }

    bool IsRoadTexturePath(std::string_view blpPath)
    {
        return !MatchedRoadToken(blpPath).empty();
    }

    // WMO-scope variant: same matching, no `tileset/` gate.
    std::string_view MatchedRoadTokenForWmo(std::string_view blpPath)
    {
        std::string const normalized = Normalize(blpPath);

        // Defensive prefix rejection — WMO MOMT/MDID can reference any
        // FileDataID; in theory that includes UI textures, character
        // textures, or weapon decals that happen to contain "road" or
        // "brick" as substrings (e.g. broadsword*, interface/...roadalpha,
        // character/...brick*). Materials that surface in collision
        // geometry are always under world/, dungeons/, environments/, or
        // tileset/ paths. Reject other namespaces up front.
        auto startsWith = [&](std::string_view prefix) {
            return normalized.size() >= prefix.size() &&
                   normalized.compare(0, prefix.size(), prefix) == 0;
        };
        if (startsWith("interface/") || startsWith("item/") ||
            startsWith("character/")  || startsWith("creature/") ||
            startsWith("spells/")     || startsWith("icon/"))
            return {};

        // Per-file overrides first.
        for (std::string_view const& full : kOverridePaths)
            if (normalized == full)
                return full;

        // Substring tokens.
        for (std::string_view const& tok : kSubstringTokens)
            if (normalized.find(tok) != std::string_view::npos)
                return tok;

        return {};
    }

    bool IsRoadTexturePathForWmo(std::string_view blpPath)
    {
        return !MatchedRoadTokenForWmo(blpPath).empty();
    }

    // -----------------------------------------------------------------------
    // GroundEffectTexture pure heuristic.
    //
    // Inputs are a single DB2 row's fields; output is the [0..1] confidence
    // that the surface decorated by this effect set is paved (road-like)
    // rather than vegetated. Spec sourced from
    // ROAD_P10a_GROUND_EFFECT_TEXTURE.md.
    //
    // Threshold table (Phase A):
    //   filled doodad slots == 0   →  0.80  (no plants/rocks at all = paved)
    //   totalWeight     <=  2      →  0.60
    //   totalWeight     <=  4      →  0.50
    //   totalWeight     <= 12      →  0.30
    //   totalWeight     >  12      →  0.20  (heavy vegetation)
    // Then subtract clamp((Density - 8) * 0.005, 0, 0.10) as a density
    // penalty, and clamp the final value to [0, 1].
    //
    // The sentinel id values 0 and 0xFFFFFFFF mean "no GroundEffect set" and
    // return the neutral 0.5 (no signal).
    // -----------------------------------------------------------------------

    float ComputeRoadConfidence(GroundEffectRecord const& rec)
    {
        if (rec.id == 0 || rec.id == 0xFFFFFFFFu)
            return 0.5f;

        int filledSlots = 0;
        int totalWeight = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (rec.doodadId[i] != 0)
            {
                ++filledSlots;
                // Treat negative weights as zero — the i8 type is signed but
                // negative values are semantic-noise in shipping data.
                if (rec.doodadWeight[i] > 0)
                    totalWeight += rec.doodadWeight[i];
            }
        }

        float doodadConf;
        if (filledSlots == 0)
            doodadConf = 0.80f;
        else if (totalWeight <= 2)
            doodadConf = 0.60f;
        else if (totalWeight <= 4)
            doodadConf = 0.50f;
        else if (totalWeight <= 12)
            doodadConf = 0.30f;
        else
            doodadConf = 0.20f;

        float densityPenalty = std::clamp(
            (static_cast<float>(rec.density) - 8.0f) * 0.005f,
            0.0f, 0.10f);

        return std::clamp(doodadConf - densityPenalty, 0.0f, 1.0f);
    }

    // -----------------------------------------------------------------------
    // GroundEffectTexture-based confidence (lookup against loaded table).
    // -----------------------------------------------------------------------

    float RoadConfidenceFromEffectId(uint32 effectId)
    {
        if (g_groundEffectTable.empty())
            return 0.5f;
        if (effectId >= g_groundEffectTable.size())
            return 0.5f;
        return g_groundEffectTable[effectId].roadConfidence;
    }

    void SetGroundEffectTable(std::vector<GroundEffectInfo> table)
    {
        g_groundEffectTable = std::move(table);
    }

    void SetGroundEffectConfidenceForTesting(uint32 effectId, float confidence)
    {
        confidence = std::clamp(confidence, 0.0f, 1.0f);
        if (effectId >= g_groundEffectTable.size())
            g_groundEffectTable.resize(effectId + 1);
        g_groundEffectTable[effectId].roadConfidence = confidence;
    }

    void ClearGroundEffectTable()
    {
        g_groundEffectTable.clear();
    }

    // -----------------------------------------------------------------------
    // Contiguous-area filter (4-connectivity flood fill on a 16x16 grid).
    // -----------------------------------------------------------------------

    namespace
    {
        // (row, col) -> index in McnkGrid. Row-major.
        constexpr std::size_t kSide = kMcnksPerSide;

        constexpr std::size_t Idx(std::size_t row, std::size_t col) noexcept
        {
            return row * kSide + col;
        }

        // 4-neighbor offsets: (dr, dc).
        struct Neighbor { int dr, dc; };
        constexpr Neighbor kNeighbors[4] = {
            { -1,  0 }, { +1,  0 }, {  0, -1 }, {  0, +1 },
        };
    }

    std::size_t ApplyContiguousAreaFilter(McnkGrid& grid, uint8 minComponentSize)
    {
        if (minComponentSize <= 1)
            return 0;

        std::array<uint8, kMcnkGridSize> visited{};
        std::size_t cleared = 0;

        // Pre-allocate a queue large enough for any worst-case component.
        // 256 cells fits trivially on the stack via std::array, but std::queue
        // doesn't accept std::array, so use a vector with reserved capacity.
        std::vector<std::size_t> component;
        component.reserve(kMcnkGridSize);
        std::vector<std::size_t> frontier;
        frontier.reserve(kMcnkGridSize);

        for (std::size_t startRow = 0; startRow < kSide; ++startRow)
        {
            for (std::size_t startCol = 0; startCol < kSide; ++startCol)
            {
                std::size_t startIdx = Idx(startRow, startCol);
                if (visited[startIdx] || grid[startIdx] == 0)
                    continue;

                // BFS flood-fill collecting all 4-connected road cells.
                component.clear();
                frontier.clear();
                frontier.push_back(startIdx);
                visited[startIdx] = 1;

                while (!frontier.empty())
                {
                    std::size_t cur = frontier.back();
                    frontier.pop_back();
                    component.push_back(cur);

                    int curRow = static_cast<int>(cur / kSide);
                    int curCol = static_cast<int>(cur % kSide);

                    for (Neighbor n : kNeighbors)
                    {
                        int nr = curRow + n.dr;
                        int nc = curCol + n.dc;
                        if (nr < 0 || nr >= static_cast<int>(kSide) ||
                            nc < 0 || nc >= static_cast<int>(kSide))
                            continue;
                        std::size_t nIdx = Idx(static_cast<std::size_t>(nr),
                                               static_cast<std::size_t>(nc));
                        if (visited[nIdx] || grid[nIdx] == 0)
                            continue;
                        visited[nIdx] = 1;
                        frontier.push_back(nIdx);
                    }
                }

                if (component.size() < minComponentSize)
                {
                    for (std::size_t idx : component)
                        grid[idx] = 0;
                    cleared += component.size();
                }
            }
        }

        return cleared;
    }
}

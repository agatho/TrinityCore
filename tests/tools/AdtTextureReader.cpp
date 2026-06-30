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

#include <array>
#include <cstring>
#include <vector>

using namespace Road::AdtTexture;

namespace
{
    // Synthesize a 4096-byte alpha grid with a horizontal split: top half
    // is `top`, bottom half is `bot`.
    std::array<uint8, kAlphaPixels> MakeSplitAlphaGrid(uint8 top, uint8 bot)
    {
        std::array<uint8, kAlphaPixels> g;
        for (std::size_t y = 0; y < kAlphaSidePixels; ++y)
            for (std::size_t x = 0; x < kAlphaSidePixels; ++x)
                g[y * kAlphaSidePixels + x] = (y < 32) ? top : bot;
        return g;
    }

    // Synthesize a 4096-byte alpha grid where alpha = `value` everywhere.
    std::array<uint8, kAlphaPixels> MakeUniformAlphaGrid(uint8 value)
    {
        std::array<uint8, kAlphaPixels> g;
        g.fill(value);
        return g;
    }
}

// =============================================================================
// DecodeMcalAlpha — branch 1: RLE
// =============================================================================

TEST_CASE("DecodeMcalAlpha RLE - single fill of all 4096 bytes",
          "[AdtTextureReader]")
{
    // RLE: control byte 0x80 | count, then one value byte to fill `count`.
    // 4096 bytes = 32 chunks of 128 (max count per control). 0x7F = 127.
    // We'll do 4096 = 32 * 128, but max count is 0x7F = 127. So we need
    // 4096 / 127 ≈ 33 chunks; remainder 5 (4096 - 32*127 = 4096 - 4064 = 32).
    std::vector<uint8> src;
    std::size_t remaining = 4096;
    while (remaining > 0)
    {
        uint8 take = static_cast<uint8>(std::min<std::size_t>(remaining, 0x7F));
        src.push_back(0x80u | take);   // fill
        src.push_back(0xAA);           // value
        remaining -= take;
    }
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0x200u, /*useFullByteAlpha=*/false, out);
    REQUIRE(ok);
    for (uint8 v : out)
        REQUIRE(v == 0xAA);
}

TEST_CASE("DecodeMcalAlpha RLE - copy mode reproduces source bytes",
          "[AdtTextureReader]")
{
    // RLE: control byte with high bit CLEAR = copy mode, then `count` bytes.
    std::vector<uint8> src;
    std::size_t remaining = 4096;
    std::size_t idx = 0;
    while (remaining > 0)
    {
        uint8 take = static_cast<uint8>(std::min<std::size_t>(remaining, 0x7F));
        src.push_back(take);   // copy mode (no high bit)
        for (uint8 i = 0; i < take; ++i)
            src.push_back(static_cast<uint8>(idx++ & 0xFF));
        remaining -= take;
    }
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0x200u, /*useFullByteAlpha=*/false, out);
    REQUIRE(ok);
    for (std::size_t i = 0; i < kAlphaPixels; ++i)
        REQUIRE(out[i] == static_cast<uint8>(i & 0xFF));
}

TEST_CASE("DecodeMcalAlpha RLE - mixed fill + copy", "[AdtTextureReader]")
{
    // First 2048 bytes filled with 0x55, next 2048 copied from sequence.
    std::vector<uint8> src;
    // Fill 2048 with 0x55 (in 0x7F chunks).
    {
        std::size_t remaining = 2048;
        while (remaining > 0)
        {
            uint8 take = static_cast<uint8>(std::min<std::size_t>(remaining, 0x7F));
            src.push_back(0x80u | take);
            src.push_back(0x55);
            remaining -= take;
        }
    }
    // Copy 2048 bytes of i&0xFF.
    {
        std::size_t remaining = 2048;
        std::size_t idx = 0;
        while (remaining > 0)
        {
            uint8 take = static_cast<uint8>(std::min<std::size_t>(remaining, 0x7F));
            src.push_back(take);
            for (uint8 i = 0; i < take; ++i)
                src.push_back(static_cast<uint8>(idx++ & 0xFF));
            remaining -= take;
        }
    }
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0x200u, /*useFullByteAlpha=*/false, out);
    REQUIRE(ok);
    for (std::size_t i = 0; i < 2048; ++i)
        REQUIRE(out[i] == 0x55);
    for (std::size_t i = 0; i < 2048; ++i)
        REQUIRE(out[2048 + i] == static_cast<uint8>(i & 0xFF));
}

TEST_CASE("DecodeMcalAlpha RLE - truncated input returns false",
          "[AdtTextureReader]")
{
    std::vector<uint8> src = { 0x80u | 0x7F, 0xAA };   // only 127 bytes worth
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0x200u, /*useFullByteAlpha=*/false, out);
    REQUIRE_FALSE(ok);
}

// =============================================================================
// DecodeMcalAlpha — branch 2: 4096-byte 8-bit direct copy
// =============================================================================

TEST_CASE("DecodeMcalAlpha 4096-byte - exact passthrough",
          "[AdtTextureReader]")
{
    std::vector<uint8> src(4096);
    for (std::size_t i = 0; i < 4096; ++i)
        src[i] = static_cast<uint8>((i * 31) & 0xFFu);

    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0u, /*useFullByteAlpha=*/true, out);
    REQUIRE(ok);
    for (std::size_t i = 0; i < 4096; ++i)
        REQUIRE(out[i] == src[i]);
}

TEST_CASE("DecodeMcalAlpha 4096-byte - too-small input fails",
          "[AdtTextureReader]")
{
    std::vector<uint8> src(100, 0xFF);
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0u, /*useFullByteAlpha=*/true, out);
    REQUIRE_FALSE(ok);
}

// =============================================================================
// DecodeMcalAlpha — branch 3: 2048-byte 4-bit nibble-packed
// =============================================================================

TEST_CASE("DecodeMcalAlpha 2048-byte - nibble unpack with *17 scaling",
          "[AdtTextureReader]")
{
    // Each byte holds two nibbles; low nibble first, then high.
    // We'll set every byte to 0x0F → low=15, high=0 → low pixel=255, high=0.
    std::vector<uint8> src(2048, 0x0F);
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0u, /*useFullByteAlpha=*/false, out);
    REQUIRE(ok);
    for (std::size_t i = 0; i < kAlphaPixels; i += 2)
    {
        REQUIRE(out[i] == 255);
        REQUIRE(out[i + 1] == 0);
    }
}

TEST_CASE("DecodeMcalAlpha 2048-byte - high nibble = 0xF0",
          "[AdtTextureReader]")
{
    std::vector<uint8> src(2048, 0xF0);
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0u, /*useFullByteAlpha=*/false, out);
    REQUIRE(ok);
    for (std::size_t i = 0; i < kAlphaPixels; i += 2)
    {
        REQUIRE(out[i] == 0);
        REQUIRE(out[i + 1] == 255);
    }
}

TEST_CASE("DecodeMcalAlpha 2048-byte - mid-value nibble scaling",
          "[AdtTextureReader]")
{
    // 0x88: low nibble 8 → 8*17 = 136, high nibble 8 → 136.
    std::vector<uint8> src(2048, 0x88);
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0u, /*useFullByteAlpha=*/false, out);
    REQUIRE(ok);
    for (uint8 v : out)
        REQUIRE(v == 136);
}

TEST_CASE("DecodeMcalAlpha 2048-byte - truncated input fails",
          "[AdtTextureReader]")
{
    std::vector<uint8> src(100, 0xFF);
    std::array<uint8, kAlphaPixels> out;
    bool ok = DecodeMcalAlpha(src.data(), src.size(),
                              /*mclyFlags=*/0u, /*useFullByteAlpha=*/false, out);
    REQUIRE_FALSE(ok);
}

TEST_CASE("DecodeMcalAlpha - branch selection by flags + useFullByteAlpha",
          "[AdtTextureReader]")
{
    // Two test buffers — the same bytes interpreted differently.
    std::vector<uint8> src2048(2048, 0xFF);

    std::array<uint8, kAlphaPixels> outA, outB;
    bool a = DecodeMcalAlpha(src2048.data(), src2048.size(),
                             /*mclyFlags=*/0u, /*useFullByteAlpha=*/false, outA);
    bool b = DecodeMcalAlpha(src2048.data(), src2048.size(),
                             /*mclyFlags=*/0u, /*useFullByteAlpha=*/true, outB);
    REQUIRE(a);          // 2048-byte branch accepts 2048
    REQUIRE_FALSE(b);    // 4096-byte branch rejects 2048
}

// =============================================================================
// AggregateDominantLayer
// =============================================================================

TEST_CASE("AggregateDominantLayer - zero layers = no-op",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 0;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 0);
    REQUIRE(m.subcellsWonPerLayer[0] == 0);
}

TEST_CASE("AggregateDominantLayer - single-layer ADT picks layer 0",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 1;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 0);
    REQUIRE(m.subcellsWonPerLayer[0] == 64);
}

TEST_CASE("AggregateDominantLayer - layer 1 dominates all subcells",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 2;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    alphas[1] = MakeUniformAlphaGrid(255);   // > threshold everywhere
    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 1);
    REQUIRE(m.subcellsWonPerLayer[1] == 64);
    REQUIRE(m.subcellsWonPerLayer[0] == 0);
}

TEST_CASE("AggregateDominantLayer - threshold cutoff (alpha == 128 is BELOW)",
          "[AdtTextureReader]")
{
    // kAlphaDominantThreshold == 128. Rule is alpha > threshold, so 128
    // does NOT count.
    McnkTextureSummary m;
    m.nLayers = 2;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    alphas[1] = MakeUniformAlphaGrid(128);
    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 0);
    REQUIRE(m.subcellsWonPerLayer[0] == 64);

    // Bump to 129 — now layer 1 wins.
    alphas[1] = MakeUniformAlphaGrid(129);
    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 1);
}

TEST_CASE("AggregateDominantLayer - layer 2 wins over layer 1 (higher-idx priority)",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 3;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    alphas[1] = MakeUniformAlphaGrid(255);
    alphas[2] = MakeUniformAlphaGrid(255);   // higher index — should win
    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 2);
}

TEST_CASE("AggregateDominantLayer - per-subcell mixed dominance",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 3;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    // Top half: layer 1 wins.
    // Bottom half: layer 2 wins.
    alphas[1] = MakeSplitAlphaGrid(255, 0);
    alphas[2] = MakeSplitAlphaGrid(0, 255);
    AggregateDominantLayer(m, alphas);
    // 32 subcells per half × 8 columns = 4 rows × 8 = 32 each.
    // Actually subcells are 8 high, 8 wide. The top 4 subcell rows are in
    // the top alpha half; bottom 4 subcell rows are in the bottom half.
    // So 32 subcells win layer 1, 32 win layer 2. Tie → higher index.
    REQUIRE(m.dominantLayerIdx == 2);
    REQUIRE(m.subcellsWonPerLayer[1] == 32);
    REQUIRE(m.subcellsWonPerLayer[2] == 32);
}

TEST_CASE("AggregateDominantLayer - all four layers mixed",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 4;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    // Layer 1 active everywhere; layer 2 covers a quarter; layer 3 a small patch
    alphas[1] = MakeUniformAlphaGrid(200);
    // Layer 2: top-left quarter (alpha pixels rows 0..31 cols 0..31).
    for (std::size_t y = 0; y < 32; ++y)
        for (std::size_t x = 0; x < 32; ++x)
            alphas[2][y * kAlphaSidePixels + x] = 200;
    // Layer 3: tiny 16x16 patch in the very top-left.
    for (std::size_t y = 0; y < 16; ++y)
        for (std::size_t x = 0; x < 16; ++x)
            alphas[3][y * kAlphaSidePixels + x] = 200;

    AggregateDominantLayer(m, alphas);
    // Sanity: total wins should add to 64.
    uint32 total =
        m.subcellsWonPerLayer[0] + m.subcellsWonPerLayer[1] +
        m.subcellsWonPerLayer[2] + m.subcellsWonPerLayer[3];
    REQUIRE(total == 64);

    // Layer 3 patch: spans alpha pixels [0..15]. Subcells are 8 pixels wide,
    // so subcells (0,0)/(0,1)/(1,0)/(1,1) are entirely within the patch and
    // their center (at +4) is at alpha 200 → win layer 3.
    REQUIRE(m.subcellsWonPerLayer[3] == 4);

    // Layer 2 quarter: alpha pixels [0..31] in both axes minus the layer-3
    // patch above. Subcell centers at (0..3, 0..3) — 16 subcells. Minus the
    // 4 owned by layer 3 = 12 subcells.
    REQUIRE(m.subcellsWonPerLayer[2] == 12);

    // Layer 1: remaining subcells = 64 - 16 = 48.
    REQUIRE(m.subcellsWonPerLayer[1] == 48);

    // Tie-break: 48 > 12 > 4 → layer 1 has the most. dominant = 1.
    REQUIRE(m.dominantLayerIdx == 1);
}

TEST_CASE("AggregateDominantLayer - off-center subcell coverage wins (max-over-subcell)",
          "[AdtTextureReader]")
{
    // Regression for the thin-road false-negative bug. Layer 1 covers ONLY an
    // off-center corner of subcell (0,0): alpha pixels rows 0..3, cols 0..3 set
    // to 200, everything else 0. The subcell center pixel is (+4,+4) = (4,4),
    // which is OUTSIDE this corner patch and reads 0.
    //
    // Old center-pixel sampling: subcell (0,0) center == 0 → layer 0 wins → the
    // road texel is dropped. New max-over-subcell: the subcell's max alpha for
    // layer 1 is 200 (> threshold) → layer 1 wins exactly that one subcell.
    McnkTextureSummary m;
    m.nLayers = 2;
    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    for (std::size_t y = 0; y < 4; ++y)
        for (std::size_t x = 0; x < 4; ++x)
            alphas[1][y * kAlphaSidePixels + x] = 200;

    AggregateDominantLayer(m, alphas);

    // Exactly one subcell (0,0) is won by layer 1; the other 63 by layer 0.
    REQUIRE(m.subcellsWonPerLayer[1] == 1);
    REQUIRE(m.subcellsWonPerLayer[0] == 63);
    // Layer 0 still wins the most subcells overall → dominant = 0.
    REQUIRE(m.dominantLayerIdx == 0);
}

TEST_CASE("AggregateDominantLayer - dominantTextureBlp + dominantEffectId copied",
          "[AdtTextureReader]")
{
    McnkTextureSummary m;
    m.nLayers = 3;
    m.layers[0].textureBlp = "base.blp";
    m.layers[0].effectId   = 100;
    m.layers[1].textureBlp = "grass.blp";
    m.layers[1].effectId   = 200;
    m.layers[2].textureBlp = "TILESET/ELWYNN/cobble.blp";
    m.layers[2].effectId   = 300;

    std::array<std::array<uint8, kAlphaPixels>, kMaxLayersPerMcnk> alphas{};
    alphas[2] = MakeUniformAlphaGrid(255);   // layer 2 dominates

    AggregateDominantLayer(m, alphas);
    REQUIRE(m.dominantLayerIdx == 2);
    REQUIRE(m.dominantTextureBlp == "TILESET/ELWYNN/cobble.blp");
    REQUIRE(m.dominantEffectId == 300);
}

// =============================================================================
// Constants sanity
// =============================================================================

TEST_CASE("AdtTextureReader constants", "[AdtTextureReader]")
{
    REQUIRE(kMcnksPerAdtSide == 16);
    REQUIRE(kMcnksPerAdt == 256);
    REQUIRE(kSubcellsPerSide == 8);
    REQUIRE(kSubcellsPerMcnk == 64);
    REQUIRE(kAlphaSidePixels == 64);
    REQUIRE(kAlphaPixels == 4096);
    REQUIRE(kMaxLayersPerMcnk == 4);
    REQUIRE(kAlphaDominantThreshold == 128);
}

TEST_CASE("AdtTextureReader WdtFlags - UseFullByteAlpha", "[AdtTextureReader]")
{
    WdtFlags f{};
    REQUIRE_FALSE(f.UseFullByteAlpha());

    f.adtHasBigAlpha = true;
    REQUIRE(f.UseFullByteAlpha());

    f = {};
    f.adtHasHeightTexturing = true;
    REQUIRE(f.UseFullByteAlpha());

    f.adtHasBigAlpha = true;
    REQUIRE(f.UseFullByteAlpha());   // both set
}

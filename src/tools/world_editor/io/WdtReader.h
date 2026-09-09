/*
 * WdtReader - parse a map's WDT (root manifest) and surface its MAID
 * chunk so callers can resolve per-tile root/tex0/obj0/obj1/lod ADT
 * FileDataIDs directly, bypassing the community listfile.
 *
 * Why this exists: modern WoW (BfA+) stores ADTs as FileDataID-only
 * entries in CASC -- the path `world/maps/<map>/<map>_<row>_<col>.adt`
 * is not always in CASC's internal root catalog, and the community
 * listfile is missing roughly half of the (row, col) combinations,
 * so name-based lookup is unreliable.  Every Blizzard tool (and
 * wow.export's `WDTLoader.js`) resolves ADTs through the WDT's MAID
 * chunk instead, which is an authoritative 64x64 grid of FileDataIDs
 * authored by Blizzard.
 *
 * MAID indexing matches wow.export and TC's map_extractor:
 *
 *     entries[(y * 64) + x]
 *
 * where `y` is the row (north-south, +X axis in WoW) and `x` is the
 * column (east-west, +Y axis in WoW).  Editor (gx, gy) maps to
 * (row=gx, col=gy); see `entryFor(gx, gy)`.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace world_editor::io
{

class CascClient;

// One row in the WDT MAID chunk -- per-tile FileDataIDs for the
// split-file ADT family.  All fields are zero when the tile is empty
// or the WDT is pre-MAID (Classic).  The caller should fall back to
// listfile-name resolution only when `rootADT == 0`.
struct WdtMaidEntry
{
    uint32_t rootADT       = 0;  // <map>_<row>_<col>.adt
    uint32_t obj0ADT       = 0;  // <map>_<row>_<col>_obj0.adt   (placements + WMO refs)
    uint32_t obj1ADT       = 0;  // <map>_<row>_<col>_obj1.adt   (high-detail variant)
    uint32_t tex0ADT       = 0;  // <map>_<row>_<col>_tex0.adt   (texture layers + alphas)
    uint32_t lodADT        = 0;  // <map>_<row>_<col>_lod.adt    (Shadowlands+ LOD)
    uint32_t mapTexture    = 0;  // single-bake ground texture (DF+)
    uint32_t mapTextureN   = 0;  // normal map for mapTexture
    uint32_t minimapTexture= 0;  // minimap BLP for this tile
};

// Parsed WDT.  Only MPHD flags + MAIN existence bits + MAID FDIDs are
// surfaced; group/texture chunks are skipped (rendered by their own
// loaders).
struct Wdt
{
    static constexpr int kGridSize = 64;

    // MPHD.flags raw bits.  Bit 0x4 = MCAL alphas are full-byte (4096
    // bytes per channel).  Bit 0x80 = height-textured ADTs.  Bit 0x200
    // = WDT carries MAID (modern path).  Used by the alpha decoder + by
    // the editor when deciding which lookup path to take.
    uint32_t mphdFlags = 0;

    // 64*64 = 4096 entries.  `tileExists[y*64+x]` = true when MAIN.flag
    // bit 0x1 is set.
    std::array<bool, kGridSize * kGridSize> tileExists{};

    // 64*64 MAID entries; all-zero when the WDT predates MAID.
    std::array<WdtMaidEntry, kGridSize * kGridSize> maid{};

    [[nodiscard]] bool hasMaid() const noexcept { return (mphdFlags & 0x200u) != 0; }

    // Lookup by editor's (gx, gy) where gx = row (north-south), gy = col (east-west).
    [[nodiscard]] WdtMaidEntry const& entryFor(int gx, int gy) const
    {
        static constexpr WdtMaidEntry kEmpty{};
        if (gx < 0 || gx >= kGridSize || gy < 0 || gy >= kGridSize)
            return kEmpty;
        return maid[std::size_t(gx) * kGridSize + std::size_t(gy)];
    }

    [[nodiscard]] bool tileExistsAt(int gx, int gy) const
    {
        if (gx < 0 || gx >= kGridSize || gy < 0 || gy >= kGridSize)
            return false;
        return tileExists[std::size_t(gx) * kGridSize + std::size_t(gy)];
    }
};

// Load the WDT for `mapDir` (e.g. "kalimdor").  Tries virtual path
// "world/maps/<mapDir>/<mapDir>.wdt" first; if CASC's root catalog
// doesn't list the path the call falls through to CascClient's
// listfile-FDID fallback.  Returns false on miss; partial parse is
// allowed (e.g. WDT with only MPHD+MAIN populates `tileExists` but
// leaves `maid` zeroed).
[[nodiscard]] bool loadWdt(CascClient& casc, std::string const& mapDir, Wdt& out);

} // namespace world_editor::io

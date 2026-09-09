/*
 * AdtLiquidReader - parse ADT liquid surfaces (MH2O modern, MCLQ legacy)
 * out of CASC for the editor's translucent water pass.
 *
 * Returns one LiquidChunk per MCNK that carries renderable liquid -- the
 * highest-Z layer wins when MH2O stacks layers, which covers every
 * Azeroth scenario the editor needs for v1.  Vertex offsets/widths
 * inside the chunk's 8x8 quad grid are honoured: chunks whose surface
 * only covers a corner produce an existsBitmap with the off-cells
 * cleared so the renderer can skip them.
 *
 * Layout reference: src/tools/map_extractor/adt.h + loadlib.cpp.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace world_editor::io
{

class CascClient;

struct LiquidVertex { float x, y, z; };

// Per-MCNK liquid surface.  v1 ships one layer per chunk -- the highest-Z
// layer wins on MH2O stacks.
struct LiquidChunk
{
    int      mcnkX = 0, mcnkY = 0; // 0..15 inside the ADT
    float    minHeight = 0.0f, maxHeight = 0.0f;
    enum class Kind : uint8_t { Water = 0, Ocean = 1, Magma = 2, Slime = 3 };
    Kind     kind = Kind::Water;
    // 9x9 vertex heights, row-major (j * 9 + i).  Cells flagged inactive
    // in MH2O.exists_bitmap have an undefined height -- the existsBitmap
    // below tells the renderer which 8x8 quads to emit.
    LiquidVertex vertices[81]{};
    // bit (row * 8 + col) set => quad (row, col) is part of the liquid
    // surface.  Synthesized from MH2O.OffsetExistsBitmap + width/height
    // for modern surfaces; from MCLQ.flags for legacy (flag != 0x0F).
    uint64_t existsBitmap = 0;
};

struct AdtLiquid
{
    uint32_t mapId = 0;
    int      gx = 0, gy = 0;
    std::vector<LiquidChunk> chunks;
};

// Load `<map_dir>/<map_dir>_<gx>_<gy>.adt` (with _obj0 sibling if MH2O
// lives in the obj split) from CASC and fill `out` with one entry per
// MCNK that carries liquid.  Empty result => tile has no water.  Returns
// false only on a fatal parse error / missing ADT root.
[[nodiscard]] bool loadAdtLiquid(CascClient& casc,
                                 std::string const& mapDir,
                                 uint32_t mapId, int gx, int gy,
                                 AdtLiquid& out,
                                 uint32_t rootFdid = 0);

} // namespace world_editor::io

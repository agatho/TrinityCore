/*
 * AdtReader - load a single ADT tile (heights, normals, holes, texture
 * layers + alpha maps) from CASC for the editor's realistic-terrain
 * pass.
 *
 * Supports both pre-Cata monolithic ADTs (everything in a single
 * <map>_<x>_<y>.adt) and modern split-file ADTs (root.adt + tex0.adt).
 * Texture references resolve through either MTEX (legacy string paths)
 * or MDID (Legion+ FileDataIDs), whichever the tile carries.
 *
 * The chunk-walking helpers are intentionally a copy of the pattern in
 * extractor_common/AdtTextureReader.cpp rather than a shared link --
 * keeping AdtTextureReader's surface narrow for the road extractor and
 * avoiding a coupling between the editor's render path and the road
 * pipeline's listfile / dominant-layer aggregation.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace world_editor::io
{

class CascClient;

// A single MCLY layer record.  When `alpha` is empty (only valid for
// layer 0) the layer covers the whole chunk implicitly.
struct AdtLayer
{
    uint32_t textureFileDataId = 0;  // 0 when only an MTEX BLP path was available
    std::string textureBlpPath;      // non-empty for legacy / for diagnostics
    uint32_t flags = 0;              // raw MCLY.flags
    // 64*64 = 4096 alpha samples [0..255].  Empty for layer 0 (implicit
    // full coverage); decoded from MCAL for layers 1..7.
    std::vector<uint8_t> alpha;

    // --- STAGE B (8-layer terrain + height parallax) ---
    // Height texture for this layer (MHID, same index space as MDID).  0 =>
    // no dedicated height map; the renderer falls back to the diffuse texture
    // (effective_hid) so the parallax term degrades to a flat luminance bias.
    uint32_t heightTextureFileDataId = 0;
    // MTXP per-layer texture parameters.  texParamFlags carries the wow.export
    // "scale" in bits 4-7 (UV scale = 2^bits).  heightScale / heightOffset feed
    // the height-weighted blend term `h * heightScale + heightOffset`.
    // Defaults make a layer WITHOUT MTXP blend purely by alpha:
    //   heightScale = 0, heightOffset = 1  =>  term = h*0 + 1 = 1.
    uint32_t texParamFlags = 0;
    float    heightScale   = 0.0f;
    float    heightOffset  = 1.0f;
    // UV-tiling scale derived from texParamFlags bits 4-7 (2^bits); default 1.
    float    layerScale    = 1.0f;
};

struct AdtChunk
{
    // Tile-local index of this chunk inside the ADT (0..15 each).
    int ix = 0;
    int iy = 0;

    // TC world-frame AABB corners of the chunk's footprint.  minX/minY
    // are the chunk's most-northwest sample's world XY; the chunk
    // extends -CHUNKSIZE (33.333y) in both axes.
    float minX = 0.0f;
    float minY = 0.0f;

    // 9x9 = 81 outer-grid (V9) heights, row-major (row index = TC -X axis,
    // col index = TC -Y axis), from MCVT index y*17 + x.
    float heights[81] = {};
    // 8x8 = 64 inner-grid (V8) heights, row-major, from MCVT index
    // y*17 + 9 + x.  V8[r][c] is the CENTRE of the cell bounded by V9
    // corners (r,c),(r,c+1),(r+1,c),(r+1,c+1).  The real client mesh fans
    // 4 triangles from each V8 centre to those 4 corners (256 tris/chunk);
    // using only V9 (128 tris/chunk) yields a coarse surface with cracks.
    float heightsV8[64] = {};
    // 9x9 per-vertex normals (V9) in TC world frame, central-difference.
    float normals[81][3] = {};
    // 8x8 per-vertex normals (V8 centres); average of the 4 corner normals.
    float normalsV8[64][3] = {};
    // 9x9 per-vertex MCCV vertex colour (R, G, B, A in 0..255 byte form).
    // When `hasMccv` is false the array is filled with 0x7F so the shader's
    // `c *= mccv.rgb * 2.0` multiplier resolves to ~1.0 (neutral, matches
    // wow.export's behaviour when MCCV is absent).
    uint8_t mccv[81][4] = {};
    bool    hasMccv = false;
    // Holes bitmask in MCNK's 8x8 layout.  Bit (row*8 + col) is set
    // when the (row, col) sub-quad is a hole.
    uint64_t holesMask = 0;

    std::vector<AdtLayer> layers;  // up to 8 (STAGE B; was 4)
};

struct AdtTile
{
    uint32_t mapId = 0;
    int gx = 0;
    int gy = 0;
    // Intrinsic world XY of chunk(0,0)'s NW corner, taken straight from the
    // MCNK header (zpos/xpos).  This is where the ADT *data* says it lives,
    // independent of the (gx,gy) we requested -- comparing the two reveals
    // a transposed / wrong-tile MAID lookup.
    float intrinsicX0 = 0.0f;
    float intrinsicY0 = 0.0f;
    bool  hasIntrinsicPos = false;
    // 16x16 = 256 chunks, row-major (iy * 16 + ix).
    std::vector<AdtChunk> chunks;
};

// Load the ADT for navmesh tile (gx, gy) of the given mapDir.
//
// Resolution order matches wow.export's `ADTExporter._load_tile`:
//   1. If `rootFdid > 0` is given (caller supplied from WDT MAID), use
//      it via CASC FDID-open.  Same for `tex0Fdid` (split-file tex0.adt).
//   2. Otherwise fall back to virtual-path resolution
//      `world/maps/<dir>/<dir>_<gx>_<gy>.adt` -- which today routes
//      through CascClient::readByPath -> listfile-FDID fallback.  Used
//      for Classic-era WDTs that predate the MAID chunk.
//
// `mapDir` is the lowercase Map.Directory string (e.g. "kalimdor").
// Returns false on missing files or fatal parse errors.
[[nodiscard]] bool loadAdtTile(CascClient& casc,
                               std::string const& mapDir,
                               uint32_t mapId,
                               int gx, int gy,
                               AdtTile& out,
                               uint32_t rootFdid = 0,
                               uint32_t tex0Fdid = 0);

} // namespace world_editor::io

/*
 * WmoDoodadReader - parse a WMO root file's doodad set (MODS/MODN/MODI/MODD)
 * and emit world-space DoodadInstance records for the editor's realistic-3D
 * pass.
 *
 * A WMO carries its own interior doodads (mailboxes on counters, candles on
 * tables, banners on walls, ...) referenced from these chunks:
 *   - MOHD     : header counts (nDoodadNames / nDoodadDefs / nDoodadSets).
 *   - MODS[]   : doodad sets - { char Name[20], uint32 StartIndex,
 *                                uint32 Count, char _pad[4] }.  Set 0 is
 *                "Default" and always active (mirrors retail behaviour).
 *   - MODN     : packed cstring blob of M2 paths (legacy).
 *   - MODI[]   : one uint32 FileDataId per doodad NameIndex (modern; replaces
 *                MODN string-table indirection).
 *   - MODD[]   : { uint32 NameIndex:24, flags:8;  Vec3D Position;
 *                  Quaternion(X,Y,Z,W); float Scale; uint32 Color }.
 *                Position lives in the WMO's LOCAL frame; the caller must
 *                supply the placement transform that turns local -> world.
 *
 * The MODD positions / rotations are emitted in the same TC editor world
 * frame that AdtDoodadReader uses so the renderer can drop both into the
 * same m_doodadInstances draw-list with zero special-casing.
 */

#pragma once

#include "AdtDoodadReader.h"     // DoodadInstance struct shared with ADT MDDF path.

#include <cstdint>
#include <vector>

namespace world_editor::io
{

class CascClient;

// Where a WMO sits in the world, in CLIENT-frame coordinates exactly as
// they appear in the parent ADT's MODF chunk.  positionXYZ is in the
// raw (X, Y=up, Z) client triplet; rotationDegXYZ is the (rotX, rotY,
// rotZ) Euler triple from MODF.Rotation (degrees).  scale is 1.0 for
// pre-Legion MODFs and may carry a /1024 factor for modern ones - the
// caller decodes MODF.Scale before passing it in.
//
// Keeping the wire-frame here avoids a second axis-swap inside the
// reader: the same client-frame math vmap_extractor's Doodad::ExtractSet
// performs is then mirrored 1:1, with the editor-frame conversion applied
// only at the final emit step.
struct WmoPlacement
{
    float positionXYZ[3]  = { 0.0f, 0.0f, 0.0f };   // MODF.Position (client X, Y_up, Z).
    float rotationDegXYZ[3] = { 0.0f, 0.0f, 0.0f };  // MODF.Rotation (degrees, client X/Y/Z).
    float scale = 1.0f;
};

// Load `wmoRootFileDataId` from CASC, parse MOHD/MODS/MODN/MODI/MODD,
// pick `setIndex` (and always also union with set 0 - retail behaviour),
// and append every spawn in those sets, transformed into the editor TC
// world frame via `placement`, to `out`.
//
// Returns false only on hard I/O failure (FDID missing / wrong magic /
// truncated header).  An empty doodad set, missing MODI/MODN, or
// out-of-range setIndex returns true with no entries appended.
[[nodiscard]] bool loadWmoDoodads(CascClient& casc,
                                  uint32_t wmoRootFileDataId,
                                  uint32_t setIndex,
                                  WmoPlacement const& placement,
                                  std::vector<DoodadInstance>& out);

// Editor-frame placement tuple for a WMO ROOT mesh (walls / floors /
// ceilings / bridges), derived from a MODF placement with the SAME
// client->editor math the interior-doodad path uses (kClientMid origin
// shift + S=diag(-1,-1,1) rotation conjugation), so the root geometry and
// its MODD interior props land in the same world spot and share the same
// translate * Rz * Ry * Rx * scale model composition the doodad pass uses.
struct WmoRootPlacement
{
    float x = 0.0f, y = 0.0f, z = 0.0f;      // editor TC world.
    float rotZ = 0.0f, rotY = 0.0f, rotX = 0.0f; // radians, applied Z then Y then X.
    float scale = 1.0f;
};

// Compute the editor-frame root placement for a MODF instance.  Mirrors
// the WMO-root portion of loadWmoDoodads' transform with an identity
// interior-doodad offset/quat, so SceneView3D can place the textured WMO
// geometry with no duplicated Mat3 plumbing.
[[nodiscard]] WmoRootPlacement computeWmoRootPlacement(WmoPlacementInstance const& wp);

} // namespace world_editor::io

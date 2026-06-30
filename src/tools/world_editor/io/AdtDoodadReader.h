/*
 * AdtDoodadReader - parse ADT MDDF (doodad placement) entries for the
 * editor's realistic-3D pass.
 *
 * Doodads (M2 props -- trees, rocks, mailboxes, fences, lampposts ...) are
 * placed by the client into every ADT via the MDDF chunk.  Each entry
 * references an M2 file either:
 *   - through MMID -> MMDX  (legacy filename-string indexing), or
 *   - through MDDF.Flags & 0x40 (MDDF entry's Id field is itself an
 *     M2 FileDataId; no MMID/MMDX needed).
 *
 * In modern (BfA+) ADTs split into root/obj0/tex0, the doodad chunks live
 * in `_obj0.adt`; the world_editor must read that sibling on top of the
 * root.  Legacy maps carry MMDX/MMID/MDDF in the monolithic .adt.
 *
 * Output positions are pre-converted to the TC world frame (X north,
 * Y west, Z up) so the renderer can drop the data straight into its
 * model matrix without re-applying the client's (Z, X, Y) axis-swap.
 * Rotations are converted from the client's degree-Euler triple
 * (X pitch, Y yaw, Z roll) to TC-frame radians applied ZYX (matching
 * vmap_extractor's Doodad::ExtractSet convention).
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace world_editor::io
{

class CascClient;

struct DoodadInstance
{
    uint32_t modelFileDataId = 0;   // M2 FDID after MMID/MMDID/flag resolution.
    std::string modelPath;          // non-empty only for legacy MMDX-string lookup; used as a fallback.
    float    x = 0.0f;              // TC world X.
    float    y = 0.0f;              // TC world Y.
    float    z = 0.0f;              // TC world Z.
    float    rotZ = 0.0f;           // radians, applied first.
    float    rotY = 0.0f;           // radians, applied second.
    float    rotX = 0.0f;           // radians, applied last.
    float    scale = 1.0f;          // post-MDDF /1024 scale factor.
    uint32_t uniqueId = 0;          // MDDF.UniqueId (preserved for diag / picking).
};

// Load the doodad placements for a single ADT tile.
//
// Preferred path (when caller supplies non-zero FDIDs from the WDT MAID
// chunk): open obj0 by FDID, fall back to root.adt by FDID.  Without
// FDIDs the function falls back to virtual-path resolution
// `<map_dir>_<gx>_<gy>_obj0.adt` / `..._<gx>_<gy>.adt`.  Returns false
// only on I/O failure; an ADT with zero doodads is a success that
// produces an empty vector.
[[nodiscard]] bool loadAdtDoodads(CascClient& casc,
                                  std::string const& mapDir,
                                  uint32_t mapId,
                                  int gx, int gy,
                                  std::vector<DoodadInstance>& out,
                                  uint32_t obj0Fdid = 0,
                                  uint32_t rootFdid = 0);

// One placed WMO instance read from an ADT's MODF chunk.  This is the
// data we need to (a) identify the WMO root in CASC and (b) transform
// its interior doodads into world space.  Coordinates + rotations are
// preserved in CLIENT frame so WmoDoodadReader can mirror
// vmap4_extractor's math precisely.
struct WmoPlacementInstance
{
    uint32_t wmoRootFileDataId = 0;     // FDID after MWMO/MWID/flag-0x8 resolution.
    std::string wmoPath;                // legacy MWMO-string fallback (FDID == 0).
    uint32_t uniqueId = 0;
    float    posXYZ[3] = { 0.0f, 0.0f, 0.0f };       // MODF.Position (client X, Y_up, Z).
    float    rotDegXYZ[3] = { 0.0f, 0.0f, 0.0f };    // MODF.Rotation (degrees, client X/Y/Z).
    uint16_t doodadSet = 0;             // MODF.DoodadSet; 0 == Default.
    uint16_t flags     = 0;             // raw MODF.Flags (bit 0x8 = FDID).
    float    scale = 1.0f;              // MODF.Scale / 1024 when flags & 0x4, else 1.0.
};

// Load the WMO placements for a single ADT tile.  Same file-fallback
// strategy as loadAdtDoodads (modern obj0.adt first, then monolithic).
// MODF.Flags bit 0x8 makes MODF.Id a FileDataId (modern); legacy entries
// resolve to a MWMO/MWID-indexed path string.  An ADT with no MODF
// entries returns true with an empty list.
[[nodiscard]] bool loadAdtWmoPlacements(CascClient& casc,
                                        std::string const& mapDir,
                                        uint32_t mapId,
                                        int gx, int gy,
                                        std::vector<WmoPlacementInstance>& out,
                                        uint32_t obj0Fdid = 0,
                                        uint32_t rootFdid = 0);

} // namespace world_editor::io

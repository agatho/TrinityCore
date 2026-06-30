/*
 * WMOReader - textured render-geometry decoder for the editor's WMO pass.
 *
 * Parses the modern WMO root + group container:
 *   - Root file: MOHD (counts + bbox), MOMT (materials), MOTX (legacy
 *     texture path blob), MDID (modern per-material texture FileDataIds),
 *     GFID (per-group FileDataIds).
 *   - Group files (one MOGP each): MOVT (positions), MONR (normals),
 *     MOTV (UV set 0), MOVI/MOVX (indices), MOBA (render batches),
 *     MOCV (optional vertex colour).
 *
 * Texture FDID resolution is the three-way MDID > inline-texture1 > MOTX-
 * offset scheme used by vmap4_extractor / wow.export (see chunks research
 * §4 + memory project_v2_road_p2_momt_inline_fdid).
 *
 * Render geometry only: no portals, no collision split (the editor's
 * translucent collision overlay covers that), no lighting beyond the
 * recorded interior flag + optional MOCV channel.  Vertices come out in
 * TC-axis-converted local space (already through FixCoord, identical to
 * io::M2Mesh) so the renderer applies translate-rotate-scale directly with
 * no per-vertex axis swap.
 *
 * Standalone: Qt-free, GL-free, TC-core-free.  Only dependency is
 * CascClient + the C++ standard library.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace world_editor::io
{

class CascClient;

// One render batch (MOBA) within a group -> one draw range + material.
struct WmoSubMesh
{
    uint32_t materialId        = 0;     // MOMT index this batch draws with.
    uint32_t textureFileDataId = 0;     // resolved BLP FDID (modern inline / MDID).
    std::string texturePath;            // legacy MOTX string fallback (fdid == 0).
    uint32_t indexStart        = 0;     // into WmoGroupMesh::indices.
    uint32_t indexCount        = 0;
    uint8_t  blendMode         = 0;     // MOMT.blendMode (0 opaque / 1 alphatest / 2+ blend).
    bool     interior          = false; // group MOGP flag 0x2000 (interior) — for ambient pick (Step 2).
};

// ONE per MOGP group file.  Vertices are interleaved 8 floats:
//   { x, y, z, nx, ny, nz, u, v }
// in TC-axis-converted LOCAL space (already through FixCoord, same as
// io::M2Mesh::vertices) so the renderer applies translate-rotate-scale
// directly with no per-vertex axis swap.
struct WmoGroupMesh
{
    std::vector<float>      vertices;   // 8 floats/vertex.
    std::vector<uint32_t>   indices;
    std::vector<WmoSubMesh> subMeshes;  // one per MOBA batch.

    // OPTIONAL parallel vertex colour channel (MOCV), 4 bytes/vertex
    // (R,G,B,A), empty when the group has no MOCV.  NOT uploaded by the
    // v1 doodad-program render path; reserved for the Step-2 mpv_wmo
    // shader.  When present, colours.size() == (vertices.size()/8)*4.
    std::vector<uint8_t>    colours;

    bool interior = false;              // MOGP flag 0x2000.
    float bboxMin[3] = { 0, 0, 0 };
    float bboxMax[3] = { 0, 0, 0 };
};

// The whole WMO: every group's geometry + a union bbox.
struct WmoModel
{
    std::vector<WmoGroupMesh> groups;
    float bboxMin[3] = { 0, 0, 0 };
    float bboxMax[3] = { 0, 0, 0 };
};

// Load the root WMO referenced by `wmoRootFileDataId` from CASC, parse its
// materials + group-file list, then load every group file and decode its
// render geometry.  Returns false on hard I/O failure (FDID missing / wrong
// magic / truncated MOHD) or when zero usable groups were produced.
[[nodiscard]] bool loadWmo(CascClient& casc, uint32_t wmoRootFileDataId, WmoModel& out);

} // namespace world_editor::io

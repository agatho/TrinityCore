/*
 * M2Reader - bind-pose static-mesh decoder for the editor's doodad pass.
 *
 * Parses the modern (BfA+) chunked M2 container:
 *   - MD21 chunk wraps the legacy MD20-magic header + offset table.
 *   - Vertices (MOVT-equivalent: ofsVertices inside MD20 payload).
 *   - SFID chunk -> external skin FileDataIds (skin file holds submesh +
 *     index lists; we read LOD 0 only).
 *   - TXID chunk -> texture FileDataIds (one per M2 texture array entry).
 *   - When SFID/TXID are absent we fall back to the M2's legacy
 *     filename-based lookup (rare on modern maps but needed for
 *     handful of legacy props still in the live archive).
 *
 * Bind pose only: no bone transform, no animation, no particle / ribbon
 * data.  The editor renders doodads as static albedo + Lambert lighting.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace world_editor::io
{

class CascClient;

// STAGE A (T2.5): the small, stable combiner contract shared by the reader and
// the renderer's fragment-shader switch.  We deliberately collapse the full
// 37-case PS table that wow.export resolves through its ShaderMapper down to the
// 4 combiners that cover the overwhelming majority of map doodads (solid props,
// alpha-keyed foliage, detail/mod2x two-texture variants).  Forward-compatible:
// more cases may be appended without touching the flat-memcpy wire format.
enum class M2Combiner : uint8_t
{
    Opaque         = 0,   // d = tex0.rgb                     (single-texture / current behavior)
    Mod            = 1,   // d = tex0.rgb ; alpha-test on tex0.a (single-texture, alpha-keyed)
    Diffuse_T1_T2  = 2,   // d = tex0.rgb * tex1.rgb          (two textures, modulate; tex1 @ uv1)
    Diffuse_T1_Env = 3,   // d = tex0.rgb * tex1.rgb * 2.0    (two textures, mod2x)
};

struct M2SubMesh
{
    uint32_t materialIndex      = 0;
    uint32_t textureFileDataId  = 0;    // tex0; 0 when only a legacy texture path was available.
    std::string texturePath;            // tex0 legacy fallback, populated only when textureFileDataId == 0.
    uint32_t textureFileDataId2 = 0;    // tex1; 0 -> no second texture (combinerId stays Opaque/Mod).
    std::string texturePath2;           // tex1 legacy fallback.
    uint32_t indexStart         = 0;    // into M2Mesh::indices.
    uint32_t indexCount         = 0;
    uint8_t  blendMode          = 0;    // 0 = opaque, 1 = alpha-test, 2/3+ = blend.
    uint8_t  combinerId         = 0;    // M2Combiner; 0 = Opaque (current behavior).
};

struct M2Mesh
{
    // Interleaved 10 floats per vertex: { x, y, z, nx, ny, nz, u0, v0, u1, v1 }
    // in TC-axis-converted local space (i.e. already through fixCoords
    // so the renderer can apply translate-rotate-scale directly without
    // a per-vertex axis swap).  uv1 (the M2 T2 UV) feeds the second texture
    // of the Diffuse_T1_T2 combiner; single-texture doodads ignore it.
    std::vector<float>     vertices;
    std::vector<uint32_t>  indices;
    std::vector<M2SubMesh> subMeshes;

    // Local-space AABB of the bind-pose vertices.  Used by the renderer
    // to build per-instance bounding spheres for frustum culling.
    float bboxMinX = 0.0f, bboxMinY = 0.0f, bboxMinZ = 0.0f;
    float bboxMaxX = 0.0f, bboxMaxY = 0.0f, bboxMaxZ = 0.0f;
};

// Load the M2 referenced by `modelFileDataId`.  Returns false on missing
// file / parse failure / zero usable submeshes.
[[nodiscard]] bool loadM2(CascClient& casc, uint32_t modelFileDataId, M2Mesh& out);

} // namespace world_editor::io

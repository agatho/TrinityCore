#include "SceneView3D.h"

#include "../io/AdtDoodadReader.h"
#include "../io/AdtLiquidReader.h"
#include "../io/AdtReader.h"
#include "../io/WdtReader.h"
#include "../io/BlpReader.h"
#include "../io/CascClient.h"
#include "../io/M2Reader.h"
#include "../io/MapDb2Lookup.h"
#include "../io/MapReader.h"
#include "../io/MapTileCache.h"
#include "../io/WmoDoodadReader.h"
#include "../io/WMOReader.h"
#include "TerrainTextureCache.h"

#include <DetourNavMesh.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>
#include <QFocusEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QPointer>
#include <QQuaternion>
#include <QRunnable>
#include <QSettings>
#include <QThreadPool>
#include <QTransform>
#include <QVector3D>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace world_editor::render
{

namespace
{

// ---- STAGE B3: per-frame GPU upload budgets ----
//
// Heavier assets upload fewer payloads per frame so a burst of newly-visible
// models spreads across several frames instead of one stall (reference §2
// concurrency ladder: terrain cheap+high-count, WMO/M2/water heavy+low-count).
// Terrain + minimap stay at 8 (already shipped, cheap).
constexpr int kUploadBudgetWmo    = 1;   // wow.export UPLOAD_BUDGET_GROUPS analogue.
constexpr int kUploadBudgetDoodad = 2;   // wow.export M2 UPLOAD_BUDGET = 2.
constexpr int kUploadBudgetWater  = 1;   // PERF: 1/frame -- each water tile is a
                                         // synchronous MH2O decode + GL upload on
                                         // the render thread (~40-60ms); 2/frame
                                         // stalled to ~100ms while flying.

constexpr char const* NAV_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_world;   // (TC X, TC Y, TC Z)
layout(location = 1) in vec4 in_color;
uniform mat4 u_mvp;
out vec4 v_color;
void main()
{
    gl_Position = u_mvp * vec4(in_world, 1.0);
    v_color = in_color;
}
)";
constexpr char const* NAV_FSHADER = R"(
#version 330 core
in vec4 v_color;
out vec4 out_color;
void main() { out_color = v_color; }
)";

constexpr char const* SPAWN_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_world;
layout(location = 1) in vec2 in_offset;  // -1..1 unit-quad corners
layout(location = 2) in vec4 in_color;
uniform mat4  u_mvp;
uniform vec2  u_viewport;
uniform float u_pixelSize;
out vec4 v_color;
out vec2 v_uv;
void main()
{
    vec4 clip = u_mvp * vec4(in_world, 1.0);
    // Offset in clip space by pixelSize pixels in each direction.
    clip.xy += in_offset * (u_pixelSize / u_viewport) * clip.w * 2.0;
    gl_Position = clip;
    v_color = in_color;
    v_uv = in_offset;
}
)";
constexpr char const* SPAWN_FSHADER = R"(
#version 330 core
in vec4 v_color;
in vec2 v_uv;
out vec4 out_color;
void main()
{
    float d = length(v_uv);
    if (d > 1.0) discard;
    float aa = fwidth(d);
    float a = 1.0 - smoothstep(1.0 - aa, 1.0, d);
    out_color = vec4(v_color.rgb, a * v_color.a);
}
)";

// Shared GLSL fog + hemisphere-ambient helpers.  Every lit fragment shader
// is concatenated with this snippet so applyFog() / hemisphereAmbient()
// resolve identically across passes.  The runtime uploads:
//   u_fogStart / u_fogEnd       -- distance gate (start = no fog inside)
//   u_fogDensity                -- exponential coefficient (1/yd)
//   u_fogColor                  -- target tint
//   u_skyAmbient / u_horizonAmbient / u_groundAmbient -- 3-band hemisphere
//   u_cameraPos                 -- camera world position for view-dependent
//                                  fog distance
//
// Fog model is wow.export's legacy_exp_fog (`mpv_fog.inc.glsl:60-65`):
//   t = 1 - exp(-density * max(0, dist - start))
// clamped to 1 once dist exceeds u_fogEnd.  When fog is disabled the
// runtime sets density = 0 + end = 1e9, leaving t = 0 (no tint).
//
// Hemisphere ambient mirrors `mpv_light.inc.glsl:14-16`:
//   ambient = sky*max(n.z,0) + ground*max(-n.z,0) + horizon*(1-|n.z|)
// Gives soft horizon tint on cliff faces; the legacy single-band ambient
// scalar (m_ambient) is preserved as a brightness scale on top so the
// existing time-of-day LUT continues to brighten / dim the whole scene.
constexpr char const* FOG_HELPER = R"(
uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_fogDensity;
uniform vec3  u_fogColor;
uniform float u_fogHeight;        // world Z of the fog plane (TC up-axis is +Z)
uniform float u_fogHeightFalloff; // per-unit-Z attenuation; 0 => height term disabled
uniform vec3  u_skyAmbient;
uniform vec3  u_horizonAmbient;
uniform vec3  u_groundAmbient;
uniform vec3  u_cameraPos;
// Fog interpolation factor t in [0,1] for a shaded point: 0 = no fog (inside
// u_fogStart), 1 = fully fogged (at/beyond u_fogEnd).  Factored out so a
// TRANSLUCENT pass can ramp its own alpha by the same curve -- a translucent
// surface must converge to a fully-opaque fog wall at distance, exactly like
// the opaque terrain, or it composites over the bare sky and "floats" as a
// bright sliver where the far terrain has been culled.
float fogFactor(vec3 worldPos)
{
    float dist  = length(worldPos - u_cameraPos);
    float gated = max(0.0, dist - u_fogStart);
    // Height factor (wow.export mpv_fog.inc.glsl:48-50,61-63, ported to TC +Z up):
    // fog thins the higher above the fog plane the shaded point sits.  Folded
    // into the exponent (not into t) so the curve stays monotonic and the term
    // is identically a no-op when u_fogHeightFalloff == 0 (the default).
    float h      = max(0.0, worldPos.z - u_fogHeight);
    float hAtten = exp(-h * u_fogHeightFalloff);   // in (0..1], == 1.0 when falloff == 0
    float t = 1.0 - exp(-u_fogDensity * gated * hAtten);
    if (dist > u_fogEnd) t = 1.0;                  // start/end hard clamp preserved
    return clamp(t, 0.0, 1.0);
}
vec3 applyFog(vec3 albedo, vec3 worldPos)
{
    return mix(albedo, u_fogColor, fogFactor(worldPos));
}
vec3 hemisphereAmbient(vec3 n)
{
    float upT   = max(0.0,  n.z);
    float downT = max(0.0, -n.z);
    float bandT = 1.0 - min(1.0, abs(n.z));
    return u_skyAmbient * upT + u_groundAmbient * downT + u_horizonAmbient * bandT;
}
)";

// Lit + textured terrain.  When u_hasTexture != 0 the albedo comes from
// the bound minimap tile (sampled via in_uv); otherwise the per-vertex
// greyscale elevation colour is used.  Lambert sun + ambient bake the
// shape of cliffs into the result so terrain reads as 3D in screen
// space.  Also used for the realistic WMO pass (u_hasTexture=0).
constexpr char const* LIT_TERRAIN_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_world;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec4 in_color;
uniform mat4 u_mvp;
out vec2 v_uv;
out vec3 v_normal;
out vec4 v_color;
out vec3 v_worldPos;
void main()
{
    gl_Position = u_mvp * vec4(in_world, 1.0);
    v_uv       = in_uv;
    v_normal   = in_normal;
    v_color    = in_color;
    v_worldPos = in_world;
}
)";
constexpr char const* LIT_TERRAIN_FSHADER_HEAD = R"(
#version 330 core
in vec2 v_uv;
in vec3 v_normal;
in vec4 v_color;
in vec3 v_worldPos;
uniform sampler2D u_texture;
uniform vec3      u_sunDir;     // world-space, normalized, points TOWARD light
uniform vec3      u_sunColor;   // tints lit term, default (1,1,1)
uniform float     u_ambient;    // 0..1
uniform int       u_hasTexture; // 0 = fallback to v_color rgb, 1 = sample u_texture
out vec4 out_color;
)";
constexpr char const* LIT_TERRAIN_FSHADER_BODY = R"(
void main()
{
    vec3 n = normalize(v_normal);
    float lambert = max(0.0, dot(n, normalize(u_sunDir)));
    vec3 hemi = hemisphereAmbient(n) * u_ambient;   // no 2x: the doubling washed out walls + over-blued the scene
    vec3 lit = hemi + u_sunColor * lambert;
    vec3 albedo = (u_hasTexture != 0) ? texture(u_texture, v_uv).rgb : v_color.rgb;
    out_color = vec4(applyFog(albedo * lit, v_worldPos), v_color.a);
}
)";

// ADT alpha-blended composite -- this is the retail terrain path.
// STAGE B: up to 8 diffuse layers (deduped into <=8 GPU samplers) + an R8
// sampler2DArray alpha (7 slices = layers 1..7, layer 0 implicit base) +
// per-layer MTXP UV scale and an optional height-weighted "peaks lift their
// layer" blend (u_heightBlend).  Per-vertex: world xyz, chunk-local UV,
// world-space normal, MCCV colour.  Final colour *= MCCV * Lambert(normal,
// sun) + hemisphere ambient + fog.  When u_hasAlpha == 0 only the base layer
// draws (chunk with one layer / no MCAL).
constexpr char const* ADT_TERRAIN_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_world;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec4 in_color;     // MCCV per-vertex 4ub (0..1)
uniform mat4 u_mvp;
out vec2 v_uv;
out vec3 v_normal;
out vec3 v_worldPos;
out vec4 v_color;
void main()
{
    gl_Position = u_mvp * vec4(in_world, 1.0);
    v_uv       = in_uv;
    v_normal   = in_normal;
    v_worldPos = in_world;
    v_color    = in_color;
}
)";
// STAGE B: 8 diffuse layers + a R8 sampler2DArray alpha (7 slices, layers
// 1..7) + an optional height-weighted "peaks lift their layer" blend.
//   * Diffuse layers are deduped per chunk into <=8 GPU samplers; u_slotForLayer
//     maps each layer to its sampler slot so a texture reused across layers
//     occupies a single unit.
//   * Absent alpha slices read 0 => that layer contributes no coverage; a layer
//     whose diffuse failed to resolve gets its alpha plane zeroed at pack time
//     (so a MIDDLE hole, not just a trailing one, drops out cleanly).
//   * When u_heightBlend != 0 the §2.3 height-weighted blend runs; height is
//     read from each layer's diffuse luminance (the effective_hid fallback --
//     world_editor's TerrainTextureCache yields GL handles, not CPU pixels, so a
//     dedicated R8 height array is deferred; diffuse luminance is the documented
//     fallback and keeps us at 8 diffuse + 1 alpha array = 9 of 16 units).
constexpr char const* ADT_TERRAIN_FSHADER_HEAD = R"(
#version 330 core
in vec2 v_uv;
in vec3 v_normal;
in vec3 v_worldPos;
in vec4 v_color;
uniform sampler2D      u_layer0;
uniform sampler2D      u_layer1;
uniform sampler2D      u_layer2;
uniform sampler2D      u_layer3;
uniform sampler2D      u_layer4;
uniform sampler2D      u_layer5;
uniform sampler2D      u_layer6;
uniform sampler2D      u_layer7;
uniform sampler2DArray u_alpha;          // unit 8: R8 64x64, 7 slices (layers 1..7)
uniform int       u_layerCount;          // 1..8
uniform int       u_hasAlpha;
uniform int       u_slotForLayer[8];     // layer i -> diffuse sampler slot (dedup)
uniform float     u_layerScale[8];       // per-layer UV scale (default 1)
uniform int       u_heightBlend;         // 0 => plain mix, 1 => height-weighted
uniform float     u_heightScale[8];      // MTXP height scale (default 0)
uniform float     u_heightOffset[8];     // MTXP height offset (default 1)
uniform vec3      u_sunDir;
uniform vec3      u_sunColor;
uniform float     u_ambient;
out vec4 out_color;

// Sample diffuse slot `s` at `uv`.  GLSL 330 cannot index samplers by a
// variable, so unroll the (deduped) slot selection.
vec3 sampleSlot(int s, vec2 uv)
{
    if (s == 0) return texture(u_layer0, uv).rgb;
    if (s == 1) return texture(u_layer1, uv).rgb;
    if (s == 2) return texture(u_layer2, uv).rgb;
    if (s == 3) return texture(u_layer3, uv).rgb;
    if (s == 4) return texture(u_layer4, uv).rgb;
    if (s == 5) return texture(u_layer5, uv).rgb;
    if (s == 6) return texture(u_layer6, uv).rgb;
    return texture(u_layer7, uv).rgb;
}
)";
constexpr char const* ADT_TERRAIN_FSHADER_BODY = R"(
// Diffuse colour for layer i (handles its dedup slot + per-layer UV scale).
vec3 layerColor(int i)
{
    vec2 uv = v_uv * (8.0 / u_layerScale[i]);
    return sampleSlot(u_slotForLayer[i], uv);
}
// Alpha coverage for layer i (>=1); layer 0 is the implicit base.
float layerAlpha(int i)
{
    return (u_hasAlpha != 0 && i < u_layerCount)
        ? texture(u_alpha, vec3(v_uv, float(i - 1))).r
        : 0.0;
}

void main()
{
    vec3 c;
    if (u_heightBlend == 0)
    {
        // --- B1 plain-mix path (8 layers) ---
        c = layerColor(0);
        if (u_layerCount > 1) c = mix(c, layerColor(1), layerAlpha(1));
        if (u_layerCount > 2) c = mix(c, layerColor(2), layerAlpha(2));
        if (u_layerCount > 3) c = mix(c, layerColor(3), layerAlpha(3));
        if (u_layerCount > 4) c = mix(c, layerColor(4), layerAlpha(4));
        if (u_layerCount > 5) c = mix(c, layerColor(5), layerAlpha(5));
        if (u_layerCount > 6) c = mix(c, layerColor(6), layerAlpha(6));
        if (u_layerCount > 7) c = mix(c, layerColor(7), layerAlpha(7));
    }
    else
    {
        // --- B2 height-weighted blend (research §2.3 A-E, unrolled) ---
        // Base weight = 1 - sum(upper alphas); upper weights = their alpha.
        float a1 = layerAlpha(1), a2 = layerAlpha(2), a3 = layerAlpha(3);
        float a4 = layerAlpha(4), a5 = layerAlpha(5), a6 = layerAlpha(6), a7 = layerAlpha(7);
        float asum = a1 + a2 + a3 + a4 + a5 + a6 + a7;
        float w0 = 1.0 - clamp(asum, 0.0, 1.0);
        float w[8];
        w[0] = w0; w[1] = a1; w[2] = a2; w[3] = a3;
        w[4] = a4; w[5] = a5; w[6] = a6; w[7] = a7;
        // Per-layer height-modulated weight.  Height proxy = diffuse luminance
        // (effective_hid fallback); pct = w * (h * scale + offset).
        vec3 col[8];
        float pct[8];
        float maxp = 0.0;
        for (int i = 0; i < 8; ++i)
        {
            if (i < u_layerCount)
            {
                col[i] = layerColor(i);
                float h = dot(col[i], vec3(0.299, 0.587, 0.114));
                pct[i] = w[i] * (h * u_heightScale[i] + u_heightOffset[i]);
                pct[i] = max(pct[i], 0.0);
                maxp = max(maxp, pct[i]);
            }
            else { col[i] = vec3(0.0); pct[i] = 0.0; }
        }
        // Soften: layers far below the peak fade out.
        float psum = 0.0;
        for (int i = 0; i < 8; ++i)
        {
            pct[i] *= 1.0 - clamp(maxp - pct[i], 0.0, 1.0);
            psum += pct[i];
        }
        c = vec3(0.0);
        if (psum > 0.0)
        {
            for (int i = 0; i < 8; ++i)
                c += col[i] * (pct[i] / psum);
        }
        else
        {
            c = layerColor(0);
        }
    }
    // MCCV vertex colour (neutral 0.5 grey when absent); 2x to match the
    // retail terrain-shader convention.
    c *= v_color.rgb * 2.0;
    vec3 n = normalize(v_normal);
    float lambert = max(0.0, dot(n, normalize(u_sunDir)));
    vec3 hemi = hemisphereAmbient(n) * u_ambient;   // no 2x: the doubling washed out walls + over-blued the scene
    vec3 lit = hemi + u_sunColor * lambert;
    out_color = vec4(applyFog(c * lit, v_worldPos), 1.0);
}
)";

// Doodad (M2 prop) pass.  Per-vertex layout: float3 pos + float3 normal
// + float2 uv (32 bytes / vertex).  Per-draw uniform `u_model` carries
// the instance's translate * rotateZYX * scale; `u_mvp` is the
// camera's projection * view; the vertex shader composes them so we
// don't pay the cost of pre-baked world transforms in client memory.
// When u_hasTexture == 0 (M2 had no resolvable texture for this batch)
// the fragment shader paints a neutral grey -- better than discarding
// the entire submesh because the lit silhouette still reads.
constexpr char const* DOODAD_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec2 in_uv2;   // STAGE A: T2 UV for the second texture.
uniform mat4 u_mvp;
uniform mat4 u_model;
out vec3 v_normal;
out vec2 v_uv;
out vec2 v_uv2;
out vec3 v_worldPos;
void main()
{
    vec4 worldPos = u_model * vec4(in_pos, 1.0);
    gl_Position = u_mvp * worldPos;
    // u_model carries no non-uniform scale (we apply isotropic scale),
    // so rotating the normal by the model matrix is sufficient.
    v_normal   = mat3(u_model) * in_normal;
    v_uv       = in_uv;
    v_uv2      = in_uv2;
    v_worldPos = worldPos.xyz;
}
)";
constexpr char const* DOODAD_FSHADER_HEAD = R"(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec2 v_uv2;
in vec3 v_worldPos;
uniform sampler2D u_texture;
uniform sampler2D u_texture2;     // STAGE A: second texture (combiners 2/3).
uniform vec3      u_sunDir;
uniform vec3      u_sunColor;
uniform float     u_ambient;
uniform int       u_hasTexture;
uniform int       u_hasTexture2;  // STAGE A: 1 when tex1 bound, else 0.
uniform int       u_combinerId;   // STAGE A: M2Combiner selector.
uniform float     u_alphaCutoff;  // > 0 enables alpha-test discard.
out vec4 out_color;
)";
constexpr char const* DOODAD_FSHADER_BODY = R"(
void main()
{
    // STAGE A combiner switch.  tex0 keeps the existing grey fallback when the
    // M2 had no resolvable texture; tex1 defaults to white so an absent second
    // texture is a no-op modulate (defensive even if a combiner id leaked).
    // tex1 samples v_uv2 (the M2 T2 UV) per the Diffuse_T1_T2 reference math.
    vec4 t0 = (u_hasTexture  != 0) ? texture(u_texture,  v_uv)  : vec4(0.55, 0.52, 0.48, 1.0);
    vec4 t1 = (u_hasTexture2 != 0) ? texture(u_texture2, v_uv2) : vec4(1.0);
    vec3 albedo;
    if      (u_combinerId == 2) albedo = t0.rgb * t1.rgb;        // Diffuse_T1_T2 (modulate)
    else if (u_combinerId == 3) albedo = t0.rgb * t1.rgb * 2.0;  // Diffuse_T1_Env (mod2x)
    else                        albedo = t0.rgb;                 // Opaque / Mod
    vec4 sampled = vec4(albedo, t0.a);
    if (u_alphaCutoff > 0.0 && sampled.a < u_alphaCutoff) discard;
    vec3 n = normalize(v_normal);
    float lambert = max(0.0, dot(n, normalize(u_sunDir)));
    vec3 hemi = hemisphereAmbient(n) * u_ambient;   // no 2x: the doubling washed out walls + over-blued the scene
    vec3 lit = hemi + u_sunColor * lambert;
    out_color = vec4(applyFog(sampled.rgb * lit, v_worldPos), sampled.a);
}
)";

// Textured WMO pass (mpv_wmo-style).  Real wall / floor / ceiling / bridge
// geometry from the client WMO group files.  Vertex layout: float3 pos +
// float3 normal + float2 uv + 4ub colour (36 bytes / vertex).  Per-draw
// `u_model` carries the instance's translate * rotateZYX * scale; `u_mvp`
// is the camera projection * view.  The MOCV vertex colour (v_color) is the
// baked WMO light the retail client modulates the texture by; when a group
// has no MOCV the reader emits opaque white so the modulate is a no-op.
// `u_interior` picks a warmer, dimmer ambient floor for interior groups so
// rooms do not read as flat as exterior facades under the sky hemisphere.
constexpr char const* WMO_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;   // MOCV 4ub (0..1), white when absent.
uniform mat4 u_mvp;
uniform mat4 u_model;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;
out vec3 v_worldPos;
void main()
{
    vec4 worldPos = u_model * vec4(in_pos, 1.0);
    gl_Position = u_mvp * worldPos;
    // Isotropic instance scale only, so mat3(u_model) rotates the normal.
    v_normal   = mat3(u_model) * in_normal;
    v_uv       = in_uv;
    v_color    = in_color;
    v_worldPos = worldPos.xyz;
}
)";
constexpr char const* WMO_FSHADER_HEAD = R"(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
in vec3 v_worldPos;
uniform sampler2D u_texture;
uniform vec3      u_sunDir;
uniform vec3      u_sunColor;
uniform float     u_ambient;
uniform int       u_hasTexture;
uniform float     u_alphaCutoff;   // > 0 enables alpha-test discard.
uniform int       u_interior;      // 1 = interior group (warmer, dimmer ambient floor).
out vec4 out_color;
)";
constexpr char const* WMO_FSHADER_BODY = R"(
void main()
{
    vec4 sampled = (u_hasTexture != 0) ? texture(u_texture, v_uv)
                                       : vec4(0.55, 0.52, 0.48, 1.0);
    if (u_alphaCutoff > 0.0 && sampled.a < u_alphaCutoff) discard;
    vec3 n = normalize(v_normal);
    // Interior groups get no direct sun (they sit under a roof) and a
    // slightly warm ambient floor so walls do not read as black; exterior
    // facades take the full hemisphere ambient + sun lambert like terrain.
    vec3 hemi = hemisphereAmbient(n) * u_ambient;   // no 2x: the doubling washed out walls + over-blued the scene
    vec3 lit;
    if (u_interior != 0)
    {
        vec3 interiorAmbient = max(hemi, vec3(0.28, 0.26, 0.22));
        lit = interiorAmbient;
    }
    else
    {
        float lambert = max(0.0, dot(n, normalize(u_sunDir)));
        lit = hemi + u_sunColor * lambert;
    }
    // MOCV baked vertex light modulates the albedo (2x, matching the retail
    // WMO shader convention); white colour leaves it unchanged.
    vec3 albedo = sampled.rgb * v_color.rgb * 2.0;
    out_color = vec4(applyFog(albedo * lit, v_worldPos), sampled.a);
}
)";

// Liquid surface pass.  One draw call per MCNK liquid chunk.  Per-vertex:
// world XYZ + kind float (0=water, 1=ocean, 2=magma, 3=slime).  Vertex
// shader interpolates world XY for a screen-space-independent wave
// pattern; fragment shader picks the base albedo from a small per-kind
// LUT, perturbs the surface normal with two octaves of sinusoidal noise,
// and lambert-lights against the sun before mixing with sky/fog.  Alpha
// is per-kind: 0.7 for water/ocean (see-through), 0.95 for magma/slime
// (mostly opaque but still blended so the silhouette edge softens).
constexpr char const* LIT_WATER_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_world;
layout(location = 1) in float in_kind;
uniform mat4 u_mvp;
out vec3 v_worldPos;
out float v_kind;
void main()
{
    gl_Position = u_mvp * vec4(in_world, 1.0);
    v_worldPos  = in_world;
    v_kind      = in_kind;
}
)";
constexpr char const* LIT_WATER_FSHADER_HEAD = R"(
#version 330 core
in vec3 v_worldPos;
in float v_kind;
uniform vec3  u_sunDir;
uniform vec3  u_sunColor;
uniform float u_ambient;
uniform float u_time;       // seconds since view init -- drives the waves
uniform vec3  u_skyTint;    // sky horizon colour for fresnel blend
out vec4 out_color;
)";
constexpr char const* LIT_WATER_FSHADER_BODY = R"(
void main()
{
    // Two-octave sinusoidal noise on world XY drives a tiny normal
    // perturbation.  Frequency tuned (0.06 / yard) so a single MCNK
    // (33y) carries ~2 full wave cycles -- reads as ripples at typical
    // camera distance but not as tiling artefacts up close.
    float w1 = sin(v_worldPos.x * 0.06 + u_time)
             * sin(v_worldPos.y * 0.06 + u_time * 1.13);
    float w2 = sin(v_worldPos.x * 0.18 - u_time * 1.7)
             * sin(v_worldPos.y * 0.18 + u_time * 0.91);
    float dxNoise = (cos(v_worldPos.x * 0.06 + u_time)
                    * sin(v_worldPos.y * 0.06 + u_time * 1.13)) * 0.06
                  + (cos(v_worldPos.x * 0.18 - u_time * 1.7)
                    * sin(v_worldPos.y * 0.18 + u_time * 0.91)) * 0.05;
    float dyNoise = (sin(v_worldPos.x * 0.06 + u_time)
                    * cos(v_worldPos.y * 0.06 + u_time * 1.13)) * 0.06
                  + (sin(v_worldPos.x * 0.18 - u_time * 1.7)
                    * cos(v_worldPos.y * 0.18 + u_time * 0.91)) * 0.05;
    vec3 n = normalize(vec3(-dxNoise, -dyNoise, 1.0));
    float lambert = max(0.0, dot(n, normalize(u_sunDir)));

    int kind = int(v_kind + 0.5);
    vec3 base = vec3(0.10, 0.30, 0.60);  // Water default
    float alpha = 0.70;
    bool fullBright = false;
    if (kind == 1)      { base = vec3(0.05, 0.20, 0.45);  alpha = 0.75; }       // Ocean
    else if (kind == 2) { base = vec3(0.95, 0.30, 0.05);  alpha = 0.95; fullBright = true; } // Magma
    else if (kind == 3) { base = vec3(0.30, 0.65, 0.20);  alpha = 0.92; }       // Slime

    // Fresnel-ish: angle between camera-to-fragment view ray and the
    // (jittered) up normal.  Stronger reflection at glancing angles ->
    // mix toward the sky colour so distant water reads as "sky lake".
    vec3 viewDir = normalize(u_cameraPos - v_worldPos);
    float fres = pow(1.0 - max(0.0, dot(n, viewDir)), 3.0);
    vec3 wavedBase = base + vec3(0.04, 0.06, 0.08) * (w1 * 0.5 + w2 * 0.25);
    // Water lighting reuses the hemisphere ambient helper so distant lakes
    // pick up the same horizon tint as the surrounding terrain instead of
    // dimming to a flat single-band ambient.
    vec3 hemi = hemisphereAmbient(n) * u_ambient;   // no 2x: the doubling washed out walls + over-blued the scene
    vec3 lit = fullBright
        ? wavedBase
        : (wavedBase * hemi + wavedBase * u_sunColor * lambert);
    vec3 albedo = mix(lit, u_skyTint, fres * 0.35);
    // Glittering specular highlight on water/ocean only.
    if (!fullBright)
    {
        vec3 r = reflect(-normalize(u_sunDir), n);
        float spec = pow(max(0.0, dot(r, viewDir)), 64.0);
        albedo += u_sunColor * spec * 0.6;
    }
    // Translucent water must become OPAQUE at fog distance so it blends into
    // the same fog wall as the terrain behind it.  Without this the colour
    // fogs to u_fogColor but the 0.70 alpha keeps compositing over the sky
    // dome (where far terrain is culled), so distant water "floats" as bright
    // hard-edged slivers above the fogged ridge.  Ramp alpha -> 1 by the fog
    // factor: near water stays translucent, far water reads as solid haze.
    float fog = fogFactor(v_worldPos);
    out_color = vec4(applyFog(albedo, v_worldPos), mix(alpha, 1.0, fog));
}
)";

// Sky dome.  Renders a triangle-list ring dome (segments x rings) anchored
// to the camera position so the dome travels with the operator -- mirrors
// `SkyRenderer.js:40-95` in wow.export.  Each vertex carries:
//   in_pos    -- unit direction on the dome (x, y, z), z in [0, 1].
//   in_t      -- per-vertex band coordinate [0, 5] from zenith (0) to the
//                distinct below-horizon sky-ground (5), derived from the
//                vertex's REAL elevation angle so the six band stops land at
//                wow.export's stated elevation angles (70/40/20/8 deg).
// The fragment shader interpolates that 6-band gradient (zenith -> horizon ->
// below-horizon) plus a sun glow disc derived from the ray direction.  Drawn
// first each frame with depth-write OFF so every subsequent opaque draw wins
// the depth test.
constexpr char const* SKY_VSHADER = R"(
#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in float in_t;
uniform mat4 u_mvp;
uniform vec3 u_camPos;
out vec3 v_rayDir;
out float v_t;
void main()
{
    // Anchor dome to the camera.  Push z slightly down to give the dome a
    // small "skirt" so the horizon band reaches below the camera even when
    // it's flying very high over the terrain.
    vec3 worldPos = u_camPos + in_pos * 8000.0 + vec3(0.0, 0.0, -2000.0);
    gl_Position = u_mvp * vec4(worldPos, 1.0);
    // Force z=1 (far plane) so the depth test (LEQUAL) places the dome
    // strictly behind anything else.  Preserve clip x/y/w for the
    // perspective-correct interpolation of the gradient parameter.
    gl_Position.z = gl_Position.w;
    v_rayDir = normalize(in_pos);
    v_t = in_t;
}
)";
constexpr char const* SKY_FSHADER = R"(
#version 330 core
in vec3 v_rayDir;
in float v_t;
uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform vec3 u_skyGround;
uniform vec3 u_sunDir;
uniform vec3 u_sunColor;
out vec4 out_color;
void main()
{
    // Six-band vertical gradient (wow.export SkyRenderer.js:18-22 parity):
    //   band 0 = zenith, bands 1-3 step down through the sky, band 4 = the
    //   horizon/smog band, band 5 = the distinct below-horizon sky-ground.
    // v_t is the per-vertex band coordinate in [0, 5], computed CPU-side
    // from the vertex's REAL elevation angle (see the VBO build) so the
    // stops land at wow.export's stated elevation angles (70/40/20/8 deg)
    // rather than at evenly-spaced ring indices.  Six colour stops derived
    // in-shader from the zenith / horizon / sky-ground uniforms (no extra
    // upload):
    vec3 b0 = u_zenith;                            // zenith
    vec3 b1 = mix(u_zenith,  u_horizon, 0.30);     // upper sky
    vec3 b2 = mix(u_zenith,  u_horizon, 0.62);     // mid sky
    vec3 b3 = mix(u_horizon, u_zenith,  0.18);     // near-horizon, still skyward
    vec3 b4 = u_horizon;                           // horizon / smog band
    vec3 b5 = u_skyGround;                         // below-horizon (SkyFogColor)
    // Stops ordered to match v_t: 0 -> zenith ... 5 -> below-horizon.
    vec3 stops[6] = vec3[6](b0, b1, b2, b3, b4, b5);
    float ft  = clamp(v_t, 0.0, 5.0);
    int   seg = int(floor(ft));
    seg       = clamp(seg, 0, 5);                  // defensive: v_t never < 0
    float f   = fract(ft);
    vec3 sky  = mix(stops[seg], stops[min(seg + 1, 5)], smoothstep(0.0, 1.0, f));

    // Sun glow disc -- power-32 cos lobe + warm sun colour.  Slightly
    // softer than the fullscreen-quad version because the dome's
    // discretised samples need a bigger lobe to read as a disc.
    float sunDot = max(0.0, dot(v_rayDir, normalize(u_sunDir)));
    float glow = pow(sunDot, 32.0) * 0.7;
    sky += u_sunColor * glow;

    out_color = vec4(sky, 1.0);
}
)";

// Default sun direction + ambient at noon.  Replaced at runtime by the
// time-of-day LUT inside refreshAtmosphere().  Sun direction is in TC
// world space (X=north, Y=west, Z=up); tuned so cliffs facing south-east
// at noon shade brightest, north-west slopes go dark.
constexpr float SUN_DIR_X = 0.4f;
constexpr float SUN_DIR_Y = 0.5f;
constexpr float SUN_DIR_Z = 0.7f;
constexpr float SUN_AMBIENT = 0.35f;

// Mirrors NavMeshView's NAV_AREA_* palette.
struct Rgb { uint8_t r, g, b; };
constexpr Rgb COL_GROUND       = { 170, 170, 170 };
constexpr Rgb COL_GROUND_STEEP = { 110, 110, 110 };
constexpr Rgb COL_WATER        = {  48,  96, 168 };
constexpr Rgb COL_MAGMA_SLIME  = { 168,  48,  32 };
constexpr Rgb COL_ROAD         = { 255, 170,   0 };
constexpr Rgb COL_UNKNOWN      = {  90,  60, 110 };
constexpr uint8_t NAV_AREA_GROUND       = 11;
constexpr uint8_t NAV_AREA_GROUND_STEEP = 10;
constexpr uint8_t NAV_AREA_WATER        = 9;
constexpr uint8_t NAV_AREA_MAGMA_SLIME  = 8;
constexpr uint8_t NAV_AREA_ROAD         = 7;

inline Rgb colorForArea(uint8_t a)
{
    switch (a)
    {
        case NAV_AREA_GROUND:       return COL_GROUND;
        case NAV_AREA_GROUND_STEEP: return COL_GROUND_STEEP;
        case NAV_AREA_WATER:        return COL_WATER;
        case NAV_AREA_MAGMA_SLIME:  return COL_MAGMA_SLIME;
        case NAV_AREA_ROAD:         return COL_ROAD;
        default:                    return COL_UNKNOWN;
    }
}

constexpr struct { float x, y; } SPAWN_CORNERS[6] =
{
    { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f },
    {  1.0f, -1.0f }, { 1.0f,  1.0f }, { -1.0f, 1.0f },
};

constexpr float SPAWN_PIXEL_SIZE = 4.0f;
constexpr float MOVE_SPEED       = 80.0f;  // yards / second.
constexpr float ROTATE_SENS      = 0.005f; // radians / pixel.

} // namespace

SceneView3D::SceneView3D(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    for (bool& v : m_layer3dVisible)
        v = true;
    // The navmesh polygon overlay sits at the terrain surface and Z-fights
    // the textured ADT terrain, so it's opt-in (off by default) for the 3D
    // view -- the operator toggles it on via the NavMesh layer when they
    // actually want to inspect pathing.
    m_layer3dVisible[size_t(Layer::NavMesh)] = false;
    {
        QSettings s;
        m_verboseLogging = s.value(QStringLiteral("viewer3d/verbose_log"), false).toBool();
    }
    loadFlySpeed();
    // 60 Hz tick to apply WASD input.
    connect(&m_tickTimer, &QTimer::timeout, this, &SceneView3D::onTick);
    m_tickTimer.start(16);
}

void SceneView3D::loadFlySpeed()
{
    QSettings s;
    bool ok = false;
    float const v = s.value(QStringLiteral("viewer3d/fly_speed"), MOVE_SPEED).toFloat(&ok);
    if (ok && std::isfinite(v) && v > 0.0f)
        m_flySpeed = std::clamp(v, 1.0f, 20000.0f);
    else
        m_flySpeed = MOVE_SPEED;
}

void SceneView3D::saveFlySpeed() const
{
    QSettings s;
    s.setValue(QStringLiteral("viewer3d/fly_speed"), m_flySpeed);
}

SceneView3D::~SceneView3D()
{
    makeCurrent();
    if (m_navVbo.isCreated())    m_navVbo.destroy();
    if (m_navVao.isCreated())    m_navVao.destroy();
    if (m_spawnVbo.isCreated())  m_spawnVbo.destroy();
    if (m_spawnVao.isCreated())  m_spawnVao.destroy();
    if (m_atrVbo.isCreated())    m_atrVbo.destroy();
    if (m_atrVao.isCreated())    m_atrVao.destroy();
    if (m_gyVbo.isCreated())     m_gyVbo.destroy();
    if (m_gyVao.isCreated())     m_gyVao.destroy();
    if (m_wmoVbo.isCreated())    m_wmoVbo.destroy();
    if (m_wmoVao.isCreated())    m_wmoVao.destroy();
    if (m_litWmoVbo.isCreated()) m_litWmoVbo.destroy();
    if (m_litWmoVao.isCreated()) m_litWmoVao.destroy();
    if (m_skyVbo.isCreated())    m_skyVbo.destroy();
    if (m_skyVao.isCreated())    m_skyVao.destroy();
    destroyAdtTerrainTiles();
    destroyDoodadResources();
    destroyTexturedWmoResources();
    destroyWaterTiles();
    if (m_terrainTextureCache)
        m_terrainTextureCache->clear(*this);
    m_terrainTextureCache.reset();
    destroyLitTerrainTiles();
    destroyMinimapTextures();
    delete m_navProgram;        m_navProgram        = nullptr;
    delete m_spawnProgram;      m_spawnProgram      = nullptr;
    delete m_litTerrainProgram; m_litTerrainProgram = nullptr;
    delete m_adtTerrainProgram; m_adtTerrainProgram = nullptr;
    delete m_doodadProgram;     m_doodadProgram     = nullptr;
    delete m_skyProgram;        m_skyProgram        = nullptr;
    delete m_waterProgram;      m_waterProgram      = nullptr;
    doneCurrent();
}

void SceneView3D::setNavMesh(io::LoadedMMap mesh)
{
    m_mesh = std::move(mesh);
    m_meshBoundsValid = false;
    if (m_mesh.ok())
    {
        dtNavMesh const* nm = m_mesh.navmesh();
        float minX = std::numeric_limits<float>::infinity();
        float maxX = -std::numeric_limits<float>::infinity();
        float minY = std::numeric_limits<float>::infinity();
        float maxY = -std::numeric_limits<float>::infinity();
        float minZ = std::numeric_limits<float>::infinity();
        float maxZ = -std::numeric_limits<float>::infinity();
        for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
        {
            dtMeshTile const* tile = nm->getTile(ti);
            if (!tile || !tile->header || tile->header->polyCount <= 0) continue;
            // Detour bmin/bmax: [0]=Y, [1]=Z, [2]=X.
            minX = std::min(minX, tile->header->bmin[2]);
            maxX = std::max(maxX, tile->header->bmax[2]);
            minY = std::min(minY, tile->header->bmin[0]);
            maxY = std::max(maxY, tile->header->bmax[0]);
            minZ = std::min(minZ, tile->header->bmin[1]);
            maxZ = std::max(maxZ, tile->header->bmax[1]);
        }
        if (std::isfinite(minX))
        {
            m_meshMinX = minX; m_meshMaxX = maxX;
            m_meshMinY = minY; m_meshMaxY = maxY;
            m_meshMinZ = minZ; m_meshMaxZ = maxZ;
            m_meshBoundsValid = true;
        }
    }
    rebuildNavmeshBuffer();
    if (m_meshBoundsValid)
    {
        frameMesh();
        // Re-frame on the first paint after this load: setNavMesh may run
        // before the GL widget has its real width/height, which would skew
        // any projection-dependent placement.  See paintGL.
        m_pendingInitialFrame = true;
    }
    // Async ADT scan is keyed off (mapId, mesh).  Reset so the next paint
    // re-dispatches against the new tile set.
    m_adtScanDispatched.store(false);
    m_adtGateDiagLogged = false;
    // WDT MAID was for the previous mapId -- drop so ensureWdt() picks up
    // the new map's WDT on the next ADT dispatch.
    dropWdtCache();
    update();
}

void SceneView3D::setSpawns(std::vector<Spawn> spawns)
{
    m_spawns = std::move(spawns);
    rebuildSpawnBuffer();
    update();
}

void SceneView3D::setAnnotations(std::vector<Annotation> annotations)
{
    m_annotations = std::move(annotations);
    rebuildAnnotBuffer();
    update();
}

void SceneView3D::setPaths(std::vector<Path> paths)
{
    m_paths = std::move(paths);
    rebuildPathBuffer();
    update();
}

void SceneView3D::setAreatriggers(std::vector<Areatrigger> atrs)
{
    m_areatriggers = std::move(atrs);
    rebuildAreatriggerBuffer();
    update();
}

void SceneView3D::setGraveyards(std::vector<Graveyard> gys)
{
    m_graveyards = std::move(gys);
    rebuildGraveyardBuffer();
    update();
}

void SceneView3D::setVmapMesh(io::LoadedVmap mesh)
{
    m_wmoMesh = std::move(mesh);
    rebuildWmoBuffer();
    rebuildLitWmoBuffer();
    update();
}

void SceneView3D::setWmoVisible(bool on)
{
    m_wmoVisible = on;
    update();
}

void SceneView3D::setMapTileCache(io::MapTileCache* cache)
{
    m_mapCache = cache;
}

void SceneView3D::rebuildHeightmapTerrain(uint32_t mapId)
{
    m_heightmapMapId  = mapId;
    m_heightmapPending = true;
    // Per-tile textures + the lit terrain VAOs are pinned to the
    // previous mapId; drop them so the next paint rebuilds against
    // the new map.
    destroyLitTerrainTiles();
    destroyMinimapTextures();
    destroyAdtTerrainTiles();
    destroyDoodadResources();
    destroyTexturedWmoResources();
    destroyWaterTiles();
    if (m_terrainTextureCache)
    {
        makeCurrent();
        m_terrainTextureCache->clear(*this);
        doneCurrent();
    }
    if (m_buffersReady) update();
}

void SceneView3D::setCamera(float x, float y, float z, float yawRad, float pitchRad)
{
    m_camX = x; m_camY = y; m_camZ = z;
    m_yaw = yawRad; m_pitch = pitchRad;
    update();
}

void SceneView3D::frameMesh()
{
    if (!m_meshBoundsValid) return;
    // Centre the camera over the mesh bounds at a height that comfortably
    // shows the full extent.  We use max(z_avg+200, max_terrain_z+200) per
    // the dispatch: anchored to top-of-terrain so the camera never spawns
    // inside the geometry of a tall continent like Northrend.
    float const centerX = 0.5f * (m_meshMinX + m_meshMaxX);
    float const centerY = 0.5f * (m_meshMinY + m_meshMaxY);
    float const zAvg    = 0.5f * (m_meshMinZ + m_meshMaxZ);
    float const span    = std::max(m_meshMaxX - m_meshMinX, m_meshMaxY - m_meshMinY);

    // Anchor the start altitude to the TERRAIN under the focus point, not to
    // the continent's AABB.  The old span*0.6 framing put the camera at
    // orbital height over a continent (tens of thousands of yards) -- almost
    // everything that high is beyond the draw radius and renders as empty fog,
    // and the operator then has to fly down for a long time to reach the
    // ground.  Sample the heightmap at the centre so we sit a fixed, useful
    // distance above the actual surface.
    float focusZ = zAvg;
    if (m_mapCache)
    {
        float const h = m_mapCache->heightAt(m_heightmapMapId, centerX, centerY);
        if (h > io::ADT_INVALID_HEIGHT)
            focusZ = h;
    }

    // Small meshes (single instance / arena, < ~2.5 tiles) still frame the
    // whole AABB so the operator sees the entire dungeon on open.  Continents
    // get a low overview: a few hundred yards above the local terrain, looking
    // forward-and-down so near ground fills the view inside the draw radius.
    bool const smallMesh = span < 2500.0f;
    float altitude;
    float pitch;
    if (smallMesh)
    {
        altitude = std::max(focusZ + 300.0f, m_meshMaxZ + span * 0.6f);
        pitch    = -0.8f;
    }
    else
    {
        altitude = focusZ + 650.0f;     // ~650 yd above the surface at centre
        pitch    = -0.5f;               // ~-29 deg: see across the near terrain
    }

    m_camX  = centerX;
    m_camY  = centerY;
    m_camZ  = altitude;
    // Yaw 0 = facing +X (TC north).  Camera sits over the focus point and looks
    // forward-and-down so the near terrain (within the draw radius) fills the
    // frame instead of a fogged continental panorama.
    m_yaw   = 0.0f;
    m_pitch = pitch;
    qInfo("[scene3d] framed mesh: center=(%.1f, %.1f), focusZ=%.1f, camera Z=%.1f, pitch=%.2f rad, span=%.0f%s",
        centerX, centerY, focusZ, m_camZ, m_pitch, span, smallMesh ? " (small)" : "");
    update();
}

void SceneView3D::initializeGL()
{
    initializeOpenGLFunctions();
    // Cache GL identity for the About dialog while the context is current.
    if (auto const* ver = glGetString(GL_VERSION))
        m_glVersionString = QString::fromLatin1(reinterpret_cast<char const*>(ver));
    if (auto const* ren = glGetString(GL_RENDERER))
        m_glRendererString = QString::fromLatin1(reinterpret_cast<char const*>(ren));
    glClearColor(16 / 255.0f, 16 / 255.0f, 20 / 255.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_navProgram = new QOpenGLShaderProgram(this);
    m_navProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   NAV_VSHADER);
    m_navProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, NAV_FSHADER);
    m_navProgram->bindAttributeLocation("in_world", 0);
    m_navProgram->bindAttributeLocation("in_color", 1);
    m_navProgram->link();
    m_navUMvp = m_navProgram->uniformLocation("u_mvp");
    m_navVao.create();
    m_navVbo.create();
    m_navVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    m_spawnProgram = new QOpenGLShaderProgram(this);
    m_spawnProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   SPAWN_VSHADER);
    m_spawnProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, SPAWN_FSHADER);
    m_spawnProgram->bindAttributeLocation("in_world",  0);
    m_spawnProgram->bindAttributeLocation("in_offset", 1);
    m_spawnProgram->bindAttributeLocation("in_color",  2);
    m_spawnProgram->link();
    m_spawnUMvp       = m_spawnProgram->uniformLocation("u_mvp");
    m_spawnUViewport  = m_spawnProgram->uniformLocation("u_viewport");
    m_spawnUPixelSize = m_spawnProgram->uniformLocation("u_pixelSize");
    m_spawnVao.create();
    m_spawnVbo.create();
    m_spawnVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // Terrain pipeline: reuses nav shader (in_world + in_color) so no
    // new program needed.  We bind the same nav program in paintGL.
    m_terrainVao.create();
    m_terrainVbo.create();
    m_terrainVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    m_pathVao.create();
    m_pathVbo.create();
    m_pathVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_annotVao.create();
    m_annotVbo.create();
    m_annotVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_atrVao.create();
    m_atrVbo.create();
    m_atrVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_gyVao.create();
    m_gyVbo.create();
    m_gyVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_wmoVao.create();
    m_wmoVbo.create();
    m_wmoVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    // Lit terrain + lit WMO program.  Shared because both passes want
    // the same vertex inputs (position + uv + normal + color) and the
    // same Lambert + ambient lighting model.  The WMO pass binds the
    // program with u_hasTexture=0 so the texture sampler is unused.
    // FOG_HELPER is inlined ahead of main() so applyFog() resolves; the
    // runtime pushes u_fogStart/u_fogEnd/u_fogColor each frame.
    std::string const litTerrainFs = std::string(LIT_TERRAIN_FSHADER_HEAD)
        + FOG_HELPER + LIT_TERRAIN_FSHADER_BODY;
    m_litTerrainProgram = new QOpenGLShaderProgram(this);
    m_litTerrainProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   LIT_TERRAIN_VSHADER);
    m_litTerrainProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, litTerrainFs.c_str());
    m_litTerrainProgram->bindAttributeLocation("in_world",  0);
    m_litTerrainProgram->bindAttributeLocation("in_uv",     1);
    m_litTerrainProgram->bindAttributeLocation("in_normal", 2);
    m_litTerrainProgram->bindAttributeLocation("in_color",  3);
    m_litTerrainProgram->link();
    m_litTerrainUMvp     = m_litTerrainProgram->uniformLocation("u_mvp");
    m_litTerrainUSunDir  = m_litTerrainProgram->uniformLocation("u_sunDir");
    m_litTerrainUAmbient = m_litTerrainProgram->uniformLocation("u_ambient");
    m_litTerrainUHasTex  = m_litTerrainProgram->uniformLocation("u_hasTexture");
    m_litTerrainUTexture = m_litTerrainProgram->uniformLocation("u_texture");

    m_litWmoVao.create();
    m_litWmoVbo.create();
    m_litWmoVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    // ADT alpha-blended terrain program.  Distinct from m_litTerrainProgram
    // because the fragment shader needs 4 terrain samplers + 1 alpha
    // sampler + a layerCount uniform; we keep the simpler lit terrain
    // shader for the minimap-projection fallback.
    std::string const adtFs = std::string(ADT_TERRAIN_FSHADER_HEAD)
        + FOG_HELPER + ADT_TERRAIN_FSHADER_BODY;
    m_adtTerrainProgram = new QOpenGLShaderProgram(this);
    bool const adtVsOk = m_adtTerrainProgram->addShaderFromSourceCode(
        QOpenGLShader::Vertex,   ADT_TERRAIN_VSHADER);
    bool const adtFsOk = m_adtTerrainProgram->addShaderFromSourceCode(
        QOpenGLShader::Fragment, adtFs.c_str());
    m_adtTerrainProgram->bindAttributeLocation("in_world",  0);
    m_adtTerrainProgram->bindAttributeLocation("in_uv",     1);
    m_adtTerrainProgram->bindAttributeLocation("in_normal", 2);
    m_adtTerrainProgram->bindAttributeLocation("in_color",  3);
    bool const adtLinkOk = m_adtTerrainProgram->link();
    qInfo("[scene3d-adt] shader build: vsOk=%d fsOk=%d linkOk=%d log='%s'",
        adtVsOk ? 1 : 0, adtFsOk ? 1 : 0, adtLinkOk ? 1 : 0,
        m_adtTerrainProgram->log().toLocal8Bit().constData());
    m_adtUMvp        = m_adtTerrainProgram->uniformLocation("u_mvp");
    m_adtUSunDir     = m_adtTerrainProgram->uniformLocation("u_sunDir");
    m_adtUAmbient    = m_adtTerrainProgram->uniformLocation("u_ambient");
    m_adtULayerCount = m_adtTerrainProgram->uniformLocation("u_layerCount");
    m_adtUHasAlpha   = m_adtTerrainProgram->uniformLocation("u_hasAlpha");
    for (int i = 0; i < 8; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "u_layer%d", i);
        m_adtULayerTex[i] = m_adtTerrainProgram->uniformLocation(name);
    }
    m_adtUAlphaArray   = m_adtTerrainProgram->uniformLocation("u_alpha");
    // STAGE B array uniforms.  Some drivers report array uniforms under the
    // "[0]" name; fall back to that so setUniformValueArray targets element 0.
    auto fetchArray = [&](char const* base) -> int {
        int loc = m_adtTerrainProgram->uniformLocation(base);
        if (loc < 0)
        {
            std::string indexed = std::string(base) + "[0]";
            loc = m_adtTerrainProgram->uniformLocation(indexed.c_str());
        }
        return loc;
    };
    m_adtUSlotForLayer = fetchArray("u_slotForLayer");
    m_adtULayerScale   = fetchArray("u_layerScale");
    m_adtUHeightScale  = fetchArray("u_heightScale");
    m_adtUHeightOffset = fetchArray("u_heightOffset");
    m_adtUHeightArray  = m_adtTerrainProgram->uniformLocation("u_heightArray");
    m_adtUHeightBlend  = m_adtTerrainProgram->uniformLocation("u_heightBlend");
    // STAGE B budget guard: assert the fragment stage can bind our sampler set
    // (8 diffuse + 1 alpha array = 9; GL3.3 guarantees >=16).  GL_MAX_TEXTURE_
    // IMAGE_UNITS is the fragment-stage limit the driver actually enforces, NOT
    // the combined vertex+fragment count.
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_glMaxTexImageUnits);
    qInfo("[scene3d-adt] uniforms: slotForLayer=%d layerScale=%d heightBlend=%d "
          "heightScale=%d heightOffset=%d | maxTexImageUnits=%d (need 9)",
        m_adtUSlotForLayer, m_adtULayerScale, m_adtUHeightBlend,
        m_adtUHeightScale, m_adtUHeightOffset, int(m_glMaxTexImageUnits));

    // Doodad (M2 prop) pipeline.
    std::string const doodadFs = std::string(DOODAD_FSHADER_HEAD)
        + FOG_HELPER + DOODAD_FSHADER_BODY;
    m_doodadProgram = new QOpenGLShaderProgram(this);
    m_doodadProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   DOODAD_VSHADER);
    m_doodadProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, doodadFs.c_str());
    m_doodadProgram->bindAttributeLocation("in_pos",    0);
    m_doodadProgram->bindAttributeLocation("in_normal", 1);
    m_doodadProgram->bindAttributeLocation("in_uv",     2);
    m_doodadProgram->bindAttributeLocation("in_uv2",    3);
    m_doodadProgram->link();
    m_doodadUMvp         = m_doodadProgram->uniformLocation("u_mvp");
    m_doodadUModel       = m_doodadProgram->uniformLocation("u_model");
    m_doodadUSunDir      = m_doodadProgram->uniformLocation("u_sunDir");
    m_doodadUAmbient     = m_doodadProgram->uniformLocation("u_ambient");
    m_doodadUHasTex      = m_doodadProgram->uniformLocation("u_hasTexture");
    m_doodadUTexture     = m_doodadProgram->uniformLocation("u_texture");
    m_doodadUAlphaCutoff = m_doodadProgram->uniformLocation("u_alphaCutoff");
    m_doodadUTexture2    = m_doodadProgram->uniformLocation("u_texture2");
    m_doodadUHasTex2     = m_doodadProgram->uniformLocation("u_hasTexture2");
    m_doodadUCombinerId  = m_doodadProgram->uniformLocation("u_combinerId");

    // Textured WMO (mpv_wmo-style) pipeline.  Distinct from the doodad
    // program because it carries a 4ub MOCV colour attribute + an interior
    // ambient toggle; shares FOG_HELPER (fog + hemisphere ambient).
    std::string const wmoFs = std::string(WMO_FSHADER_HEAD)
        + FOG_HELPER + WMO_FSHADER_BODY;
    m_wmoProgram = new QOpenGLShaderProgram(this);
    bool const wmoVsOk = m_wmoProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   WMO_VSHADER);
    bool const wmoFsOk = m_wmoProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, wmoFs.c_str());
    m_wmoProgram->bindAttributeLocation("in_pos",    0);
    m_wmoProgram->bindAttributeLocation("in_normal", 1);
    m_wmoProgram->bindAttributeLocation("in_uv",     2);
    m_wmoProgram->bindAttributeLocation("in_color",  3);
    bool const wmoLinkOk = m_wmoProgram->link();
    qInfo("[scene3d-wmo] shader build: vsOk=%d fsOk=%d linkOk=%d log='%s'",
        wmoVsOk ? 1 : 0, wmoFsOk ? 1 : 0, wmoLinkOk ? 1 : 0,
        m_wmoProgram->log().toLocal8Bit().constData());
    m_wmoUMvp         = m_wmoProgram->uniformLocation("u_mvp");
    m_wmoUModel       = m_wmoProgram->uniformLocation("u_model");
    m_wmoUHasTex      = m_wmoProgram->uniformLocation("u_hasTexture");
    m_wmoUTexture     = m_wmoProgram->uniformLocation("u_texture");
    m_wmoUAlphaCutoff = m_wmoProgram->uniformLocation("u_alphaCutoff");
    m_wmoUInterior    = m_wmoProgram->uniformLocation("u_interior");

    // Sky-dome pipeline.  Ring dome (segments x rings) anchored to camera
    // position; fragment shader picks a band colour from a 3-stop gradient
    // (ground / horizon / zenith) + a sun-glow disc.  Drawn first each
    // frame with depth-write disabled.  Geometry built immediately below.
    m_skyProgram = new QOpenGLShaderProgram(this);
    m_skyProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   SKY_VSHADER);
    m_skyProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, SKY_FSHADER);
    m_skyProgram->bindAttributeLocation("in_pos", 0);
    m_skyProgram->bindAttributeLocation("in_t",   1);
    m_skyProgram->link();
    m_skyUMvp         = m_skyProgram->uniformLocation("u_mvp");
    m_skyUCamPos      = m_skyProgram->uniformLocation("u_camPos");
    m_skyUZenith      = m_skyProgram->uniformLocation("u_zenith");
    m_skyUHorizon     = m_skyProgram->uniformLocation("u_horizon");
    m_skyUGround      = m_skyProgram->uniformLocation("u_skyGround");
    m_skyUSunDir      = m_skyProgram->uniformLocation("u_sunDir");
    m_skyUSunColor    = m_skyProgram->uniformLocation("u_sunColor");

    // Liquid (water / ocean / magma / slime) surface pipeline.  Shares
    // FOG_HELPER so applyFog() in the fragment shader resolves; the
    // runtime pushes u_fogStart/u_fogEnd/u_fogColor + u_sunDir + u_sunColor
    // + u_ambient + u_cameraPos via applyFogAndSunUniforms().
    std::string const waterFs = std::string(LIT_WATER_FSHADER_HEAD)
        + FOG_HELPER + LIT_WATER_FSHADER_BODY;
    m_waterProgram = new QOpenGLShaderProgram(this);
    m_waterProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   LIT_WATER_VSHADER);
    m_waterProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, waterFs.c_str());
    m_waterProgram->bindAttributeLocation("in_world", 0);
    m_waterProgram->bindAttributeLocation("in_kind",  1);
    m_waterProgram->link();
    m_waterUMvp     = m_waterProgram->uniformLocation("u_mvp");
    m_waterUTime    = m_waterProgram->uniformLocation("u_time");
    m_waterUSkyTint = m_waterProgram->uniformLocation("u_skyTint");

    m_skyVao.create();
    m_skyVbo.create();
    m_skyVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    {
        // Triangle-list ring dome: 24 segments x 8 horizontal rings, with
        // a vertex band parameter v_t in [0, 1] from bottom skirt to
        // zenith (extra weight near the horizon for a soft transition).
        // Each quad emits 6 vertices (two triangles); the (segments + 1)
        // wrap is implicit because we re-emit the first column at the end.
        constexpr int kSegments = 24;
        constexpr int kRings    = 8;
        // Phi (latitude) samples.  -0.15 dips below horizon to ensure the
        // ground band is visible when the camera looks slightly downward.
        // Skewed toward the horizon so the horizon transition isn't
        // sliced through by a single ring band.
        std::array<float, kRings + 1> phi;
        for (int i = 0; i <= kRings; ++i)
        {
            float const u = float(i) / float(kRings);
            // Bias: cube the parameter so rings cluster near v=0.5
            // (horizon) and spread out at extremes.
            float const biased = 0.5f + 0.5f * std::sin((u - 0.5f) * 3.14159265f);
            phi[size_t(i)] = -0.15f + biased * (1.55f);
        }
        // Map an elevation angle (radians above the horizon plane) to the
        // wow.export 6-band coordinate in [0, 5]: band 0 = zenith, the
        // integer boundaries land at the stated elevation angles
        // 70 / 40 / 20 / 8 / 2 deg, and band 5 = below-horizon sky-ground.
        // The fragment shader interpolates between the six colour stops with
        // this coordinate, so the bands track real elevation rather than the
        // (sine-biased) ring index.  Piecewise-linear so each span maps onto
        // exactly one unit of band coordinate.
        auto elevationToBand = [](float phiRad) -> float
        {
            constexpr float kPi = 3.14159265f;
            float const deg = phiRad * (180.0f / kPi);
            // Boundary elevations for band edges 0..5 (zenith down to ground).
            // band 0: [90, 70], 1: [70, 40], 2: [40, 20], 3: [20, 8],
            // band 4: [8, 2], band 5: <= 2 (incl. below horizon).
            constexpr float kEdge[6] = { 90.0f, 70.0f, 40.0f, 20.0f, 8.0f, 2.0f };
            if (deg >= kEdge[0]) return 0.0f;
            for (int k = 0; k < 5; ++k)
            {
                if (deg >= kEdge[k + 1])
                {
                    float const span = kEdge[k] - kEdge[k + 1];
                    float const f = (kEdge[k] - deg) / span; // 0 at upper edge
                    return float(k) + f;
                }
            }
            return 5.0f; // at/under the lowest edge (horizon / below-horizon)
        };

        struct SkyVtx { float x, y, z, t; };
        std::vector<SkyVtx> verts;
        verts.reserve(size_t(kSegments) * size_t(kRings) * 6);
        for (int r = 0; r < kRings; ++r)
        {
            float const ph0 = phi[size_t(r)];
            float const ph1 = phi[size_t(r + 1)];
            // Per-vertex band coordinate in [0, 5] from the REAL elevation
            // angle of each ring (NOT the linear ring index), so the six
            // band stops align with wow.export's stated elevation angles.
            float const t0 = elevationToBand(ph0);
            float const t1 = elevationToBand(ph1);
            float const z0 = std::sin(ph0), c0 = std::cos(ph0);
            float const z1 = std::sin(ph1), c1 = std::cos(ph1);
            for (int s = 0; s < kSegments; ++s)
            {
                float const th0 = (float(s)     / float(kSegments)) * 2.0f * 3.14159265f;
                float const th1 = (float(s + 1) / float(kSegments)) * 2.0f * 3.14159265f;
                SkyVtx const a = { c0 * std::cos(th0), c0 * std::sin(th0), z0, t0 };
                SkyVtx const b = { c0 * std::cos(th1), c0 * std::sin(th1), z0, t0 };
                SkyVtx const c = { c1 * std::cos(th0), c1 * std::sin(th0), z1, t1 };
                SkyVtx const d = { c1 * std::cos(th1), c1 * std::sin(th1), z1, t1 };
                verts.push_back(a); verts.push_back(b); verts.push_back(c);
                verts.push_back(b); verts.push_back(d); verts.push_back(c);
            }
        }
        m_skyVertexCount = static_cast<GLsizei>(verts.size());
        m_skyVao.bind();
        m_skyVbo.bind();
        m_skyVbo.allocate(verts.data(), int(verts.size() * sizeof(SkyVtx)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkyVtx),
            reinterpret_cast<void*>(offsetof(SkyVtx, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(SkyVtx),
            reinterpret_cast<void*>(offsetof(SkyVtx, t)));
        m_skyVbo.release();
        m_skyVao.release();
        m_skyBufferReady = true;
    }
    refreshAtmosphere();

    m_buffersReady = true;
    if (m_navDirty)   uploadNavmeshGeometry();
    if (m_spawnDirty) uploadSpawnGeometry();
    if (m_pathDirty)  uploadPathGeometry();
    if (m_annotDirty) uploadAnnotGeometry();
    if (m_atrDirty)   uploadAreatriggerGeometry();
    if (m_gyDirty)    uploadGraveyardGeometry();
    if (m_wmoDirty)   uploadWmoGeometry();
    if (m_litWmoDirty) uploadLitWmoGeometry();
}

void SceneView3D::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

QMatrix4x4 SceneView3D::projectionMatrix() const
{
    QMatrix4x4 m;
    float const aspect = float(std::max(1, width())) / float(std::max(1, height()));
    // Near stays at 1.0. Depth precision is governed by the near/far ratio, so a
    // larger near would help, but the camera is free-flown: fly speed is user
    // configurable down to 1.0 with an Alt x0.2 slow modifier (eye can creep to
    // and STOP <4 units from a wall) and high-res/trackpad wheels dolly in sub-50
    // (even sub-4) unit steps. There is no minimum-approach clamp in wheelEvent or
    // the WASD fly tick, so any near > the realistic closest approach would clip
    // surfaces during the close-inspection workflow (fly within ~5 units of a
    // terrain/WMO lip and confirm nothing clips). near=1.0 keeps that contract.
    // Far stays 50000.0 (distance is fog-hidden; shrinking it would pop tiles).
    m.perspective(65.0f, aspect, 1.0f, 50000.0f);
    return m;
}

QMatrix4x4 SceneView3D::viewMatrix() const
{
    // Camera at (m_camX, m_camY, m_camZ), looking along (cosPitch*cosYaw,
    // cosPitch*sinYaw, sinPitch) in TC world frame.
    // We use a Y-up GL convention internally by rebuilding axes: the
    // shader gets a matrix that maps TC (X north, Y west, Z up) to GL
    // (X right, Y up, Z into screen).
    float const cp = std::cos(m_pitch);
    float const sp = std::sin(m_pitch);
    float const cy = std::cos(m_yaw);
    float const sy = std::sin(m_yaw);
    // Look vector in TC frame.
    QVector3D const eye(m_camX, m_camY, m_camZ);
    QVector3D const fwd(cp * cy, cp * sy, sp);
    QVector3D const up(0.0f, 0.0f, 1.0f); // world up = +Z
    QMatrix4x4 m;
    m.lookAt(eye, eye + fwd, up);
    return m;
}

void SceneView3D::paintGL()
{
    // Background colour swaps between dark grey (sky off / non-realistic)
    // and the current horizon colour (sky on) so a single fragment that
    // happens to leak outside the sky quad blends naturally.
    if (m_realistic && m_skyVisible)
        glClearColor(m_horizonColor[0], m_horizonColor[1], m_horizonColor[2], 1.0f);
    else
        glClearColor(16 / 255.0f, 16 / 255.0f, 20 / 255.0f, 1.0f);
    // CRITICAL -- full pipeline-state reset every frame BEFORE the clear.
    // The QPainter HUD overlay (paintOverlay) runs Qt's GL paint engine, which
    // mutates pipeline state and does NOT restore it.  The damaging leaks:
    //   * GL_SCISSOR_TEST left ENABLED with a small clip rect -> glClear and
    //     every subsequent draw are confined to that rect, so the rest of the
    //     framebuffer keeps LAST frame's pixels.  At a fixed camera this is
    //     invisible (stale == correct); as the camera MOVES the stale regions
    //     no longer match -> occluded geometry shows in front, water draws over
    //     hills, and it visibly ACCUMULATES.  (This is the artifact; a single
    //     glDepthMask reset did NOT fix it because scissor was the real gate.)
    //   * GL_DEPTH_TEST disabled / glDepthMask FALSE / glDepthFunc changed ->
    //     broken occlusion.
    //   * GL_BLEND / GL_STENCIL_TEST / colour mask left in a 2D-paint state.
    // A headless fixed-camera screenshot cannot reveal any of this -- only a
    // moving camera does (see WE_CAMSPIN).  Reset everything the geometry
    // passes rely on so no leaked state can corrupt the frame.
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_buffersReady) return;

    // WE_PERF: env-gated per-frame section timing to localise "slow as hell".
    // Marks are captured at pass boundaries; the breakdown logs each frame.
    static bool const kPerf = qEnvironmentVariableIsSet("WE_PERF");
    QElapsedTimer __pt; qint64 __tStart=0,__tStream=0,__tTerrain=0,__tProps=0,__tWater=0,__tDoodadRebuild=0;
    if (kPerf) { __pt.start(); }

    // Atmospheric tints are time-of-day dependent; recompute each paint
    // so a slider drag updates uniforms without an explicit invalidation.
    refreshAtmosphere();

    // Lazy heightmap terrain build.
    if (m_heightmapPending && m_mapCache && !m_mapCache->mapsDir().empty() && m_mesh.ok())
    {
        rebuildTerrainBuffer();
        m_heightmapPending = false;
        // Invalidate the realistic-pass tile set; it will lazy-rebuild
        // the next time the realistic toggle is on.
        destroyLitTerrainTiles();
    }

    QMatrix4x4 const proj = projectionMatrix();
    QMatrix4x4 const view = viewMatrix();
    QMatrix4x4 const mvp  = proj * view;
    extractFrustumPlanes(mvp);
    m_drawnTilesThisFrame  = 0;
    m_culledTilesThisFrame = 0;
    m_totalTilesThisFrame  = 0;

    // Drain any pending async tile uploads BEFORE the draw loops -- so a
    // tile that finished loading mid-frame is immediately drawable.
    qint64 __dA=0,__dM=0,__dW=0,__dD=0;
    // PERF: ONE ADT tile of GL uploads per frame.  Each tile is 256 chunk VBOs
    // + 256 R8 alpha texture-arrays; uploading 8/frame saturated GPU upload
    // bandwidth and stalled the render thread for ~0.5-1s while flying.  At 1/
    // frame the visible terrain fills in over a second but the framerate stays
    // interactive.  (Budget is tiles, not chunks -- see drainPendingAdtUploads.)
    drainPendingAdtUploads(1);                 if (kPerf) __dA = __pt.elapsed();
    drainPendingMinimapUploads(8);             if (kPerf) __dM = __pt.elapsed();
    // STAGE B1/B3: budgeted GL upload of worker-decoded WMO + M2 payloads.
    // Heavy assets stay low (1 WMO / 2 M2 per frame) so a city's worth of
    // newly-visible models pops in over a few frames rather than one stall.
    drainPendingWmoUploads(kUploadBudgetWmo);  if (kPerf) __dW = __pt.elapsed();
    drainPendingDoodadUploads(kUploadBudgetDoodad); if (kPerf) __dD = __pt.elapsed();
    if (kPerf)
        qInfo("[WE_PERF-drain] adt=%lld minimap=%lld wmo=%lld doodad=%lld (ms, cumulative marks)",
            (long long)__dA, (long long)(__dM-__dA), (long long)(__dW-__dM), (long long)(__dD-__dW));

    // Re-frame the camera on the first paint after a navmesh load so the
    // projection-dependent placement uses the widget's real aspect ratio.
    if (m_pendingInitialFrame && m_meshBoundsValid)
    {
        m_pendingInitialFrame = false;
        frameMesh();
    }

    // Sky dome first: depth-write off + depth-test off so it draws under
    // anything that subsequently writes depth.  Gated by realistic-mode
    // toggle so the non-realistic fallback view keeps its grey clear.
    if (m_realistic && m_skyVisible && m_skyProgram && m_skyBufferReady)
        drawSky(proj, view);

    // Lazy build of the realistic-pass per-tile geometry the first
    // paint after the toggle flips on (or the heightmap changes).
    if (m_realistic && m_litTerrainTiles.empty()
        && m_terrainVertexCount > 0 && m_mapCache && m_mesh.ok())
    {
        rebuildLitTerrainTiles();
    }

    // Lazy ADT terrain build.  Triggers once per (realistic, heightmap)
    // generation when CASC is available.  Loads are dispatched to a
    // QThreadPool; the GL drainer above promotes finished payloads into
    // VAOs.  m_adtScanDispatched gates the one-shot scan; per-tile fall-
    // through to the minimap-projection pipeline happens inside the draw
    // loop while tiles are still pending.
    if (m_realistic && m_mesh.ok()
        && m_cascClient && m_cascClient->isOpen()
        && m_mapDb2 && m_adtTerrainProgram)
    {
        dispatchAdtTileLoads();
        // Evict resident tiles the camera has left behind (GL context is
        // current here).  Keeps VRAM bounded as the camera roams the map.
        unloadFarAdtTiles();
    }
    else
    {
        // One-shot gate diagnostic: log exactly which precondition kept
        // the ADT pass from running.  Sticky flag so we don't spam every
        // frame.  Re-armed by setNavMesh / setRealistic to catch state
        // changes.
        if (!m_adtGateDiagLogged)
        {
            m_adtGateDiagLogged = true;
            qInfo("[scene3d-adt] dispatch gate FAILED: realistic=%d mesh.ok=%d "
                  "casc=%d cascOpen=%d mapDb2=%d adtProg=%d",
                  m_realistic ? 1 : 0,
                  m_mesh.ok() ? 1 : 0,
                  m_cascClient ? 1 : 0,
                  (m_cascClient && m_cascClient->isOpen()) ? 1 : 0,
                  m_mapDb2 ? 1 : 0,
                  m_adtTerrainProgram ? 1 : 0);
        }
    }

    // Lazy doodad-instance enumeration.  Reuses the same per-tile loop
    // as the ADT terrain pass but stores only the MDDF placements (no
    // per-mesh GPU upload yet -- meshes stream in asynchronously: the draw
    // loop queues conservatively-visible FDIDs via dispatchDoodadLoad and a
    // worker decode + budgeted GL drain uploads them, STAGE B1).
    //
    // rebuildDoodadInstances() enumerates BOTH the M2 doodad placements and
    // the textured-WMO root placements in one tile walk (the MODF read it
    // needs for interior doodads also yields the WMO instances), so a single
    // build covers both layers.  Fire it when either layer is visible but
    // not yet built.
    // Re-stream props when the camera crosses into a new tile so near doodads/
    // WMOs come in and far ones drop (rebuildDoodadInstances is camera-radius
    // gated).  Sentinel forces the first build.
    int const propCamGx = int(std::floor(32 - m_camX / 533.3333f));
    int const propCamGy = int(std::floor(32 - m_camY / 533.3333f));
    bool const propCamCrossed =
        (propCamGx != m_doodadStreamCamGx || propCamGy != m_doodadStreamCamGy);
    bool const needDoodadBuild =
        m_realistic && m_doodadsVisible && (!m_doodadsBuilt || propCamCrossed);
    bool const needWmoBuild =
        m_realistic && m_texturedWmosVisible && (!m_texturedWmosBuilt || propCamCrossed) && m_wmoProgram;
    if ((needDoodadBuild || needWmoBuild) && m_mesh.ok()
        && m_cascClient && m_cascClient->isOpen()
        && m_mapDb2 && m_doodadProgram)
    {
        qint64 const __rb = kPerf ? __pt.elapsed() : 0;
        rebuildDoodadInstances();
        if (kPerf) __tDoodadRebuild = __pt.elapsed() - __rb;
    }
    if (kPerf) __tStream = __pt.elapsed();

    // ADT alpha-blended terrain pass.  For each navmesh tile we either
    // draw the per-chunk ADT geometry (preferred) OR fall through to
    // the minimap-projection lit tile below.  Build a quick lookup so
    // the minimap-projection pass can skip tiles already covered.
    QSet<uint32_t> adtCoveredTiles;
    int adtChunksDrawnThisFrame = 0;
    int adtTilesDrawnThisFrame = 0;
    GLenum adtFirstGlErr = GL_NO_ERROR;
    if (m_realistic && !m_adtTerrainHidden && m_adtTerrainProgram && !m_adtTerrainTiles.empty())
    {
        m_adtTerrainProgram->bind();
        m_adtTerrainProgram->setUniformValue(m_adtUMvp, mvp);
        applyFogAndSunUniforms(*m_adtTerrainProgram);
        // STAGE B: bind sampler-unit indices once -- 8 diffuse on units 0..7,
        // the R8 alpha sampler2DArray on unit 8.  Texture handles bind per-chunk.
        for (int i = 0; i < 8; ++i)
            if (m_adtULayerTex[i] >= 0)
                m_adtTerrainProgram->setUniformValue(m_adtULayerTex[i], i);
        if (m_adtUAlphaArray >= 0) m_adtTerrainProgram->setUniformValue(m_adtUAlphaArray, 8);
        constexpr float kTileSize  = 533.3333f;
        constexpr int   kCenterGid = 32;
        for (AdtTileRender& tile : m_adtTerrainTiles)
        {
            if (!tile.loaded) continue;
            adtCoveredTiles.insert((uint32_t(tile.gy) << 16) | (uint32_t(tile.gx) & 0xFFFFu));
            // Frustum-cull entire tile against its bounding sphere.
            float const tileMaxX = (kCenterGid - tile.gx) * kTileSize;
            float const tileMinX = tileMaxX - kTileSize;
            float const tileMaxY = (kCenterGid - tile.gy) * kTileSize;
            float const tileMinY = tileMaxY - kTileSize;
            float const tcX = 0.5f * (tileMinX + tileMaxX);
            float const tcY = 0.5f * (tileMinY + tileMaxY);
            ++m_totalTilesThisFrame;
            // Frustum-cull against a sphere that bounds THIS tile's actual
            // geometry.  Using a single continent-wide mid-Z wrongly culled
            // high/low tiles (Teldrassil plateau at Z~1300, ocean at -515) as
            // the camera moved -- tiles blinked invisible, revealing the flat
            // tiles behind.  The XY footprint is fixed; the Z extent is the
            // tile's own min/max.  No hard distance cull is needed: streaming
            // keeps only nearby tiles resident, so distant terrain simply
            // isn't here to draw.
            float const halfXY = kTileSize * 0.7071f;
            float const tileMidZ = 0.5f * (tile.minZ + tile.maxZ);
            float const halfZ = 0.5f * (tile.maxZ - tile.minZ);
            float const tcR = std::sqrt(halfXY * halfXY + halfZ * halfZ) + 50.0f;
            if (!sphereInFrustum(tcX, tcY, tileMidZ, tcR))
            {
                ++m_culledTilesThisFrame;
                continue;
            }
            ++m_drawnTilesThisFrame;
            for (AdtChunkRender const& ch : tile.chunks)
            {
                if (!ch.vao || ch.vertexCount <= 0) continue;
                m_adtTerrainProgram->setUniformValue(m_adtULayerCount, ch.layerCount);
                m_adtTerrainProgram->setUniformValue(m_adtUHasAlpha,
                    ch.alphaArray != 0 ? 1 : 0);
                m_adtTerrainProgram->setUniformValue(m_adtUHeightBlend,
                    ch.heightBlend ? 1 : 0);
                // STAGE B per-chunk array uniforms.  setUniformValueArray pushes
                // all 8 elements (a plain setUniformValue would leave [1..7] at 0
                // -> 8.0/0 = inf UV).  slotForLayer is int[8]; the others float[8].
                if (m_adtUSlotForLayer >= 0)
                    m_adtTerrainProgram->setUniformValueArray(m_adtUSlotForLayer, ch.slotForLayer, 8);
                if (m_adtULayerScale >= 0)
                    m_adtTerrainProgram->setUniformValueArray(m_adtULayerScale, ch.layerScale, 8, 1);
                if (m_adtUHeightScale >= 0)
                    m_adtTerrainProgram->setUniformValueArray(m_adtUHeightScale, ch.heightScale, 8, 1);
                if (m_adtUHeightOffset >= 0)
                    m_adtTerrainProgram->setUniformValueArray(m_adtUHeightOffset, ch.heightOffset, 8, 1);
                // Diffuse handles on units 0..7 (deduped slots; empty slots = 0).
                for (int i = 0; i < 8; ++i)
                {
                    glActiveTexture(GL_TEXTURE0 + i);
                    glBindTexture(GL_TEXTURE_2D, ch.layerTex[i]);
                }
                // Alpha sampler2DArray on unit 8.
                glActiveTexture(GL_TEXTURE8);
                glBindTexture(GL_TEXTURE_2D_ARRAY, ch.alphaArray);
                ch.vao->bind();
                if (ch.indexCount > 0)
                    glDrawElements(GL_TRIANGLES, ch.indexCount, GL_UNSIGNED_SHORT, nullptr);
                else
                    glDrawArrays(GL_TRIANGLES, 0, ch.vertexCount);
                ch.vao->release();
                ++adtChunksDrawnThisFrame;
                if (adtFirstGlErr == GL_NO_ERROR)
                    adtFirstGlErr = glGetError();
            }
            ++adtTilesDrawnThisFrame;
        }
        // Unbind units 0..7 (2D) + unit 8 (2D_ARRAY).
        for (int i = 0; i < 8; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glActiveTexture(GL_TEXTURE0);
        m_adtTerrainProgram->release();
    }

    if (m_realistic && m_litTerrainProgram && !m_litTerrainTiles.empty())
    {
        // Textured + lit terrain.  Per-tile draw call so each tile can
        // bind its own minimap texture.  Missing-texture tiles fall
        // back to greyscale elevation shading via u_hasTexture=0.
        m_litTerrainProgram->bind();
        m_litTerrainProgram->setUniformValue(m_litTerrainUMvp, mvp);
        applyFogAndSunUniforms(*m_litTerrainProgram);
        m_litTerrainProgram->setUniformValue(m_litTerrainUTexture, 0);
        for (LitTerrainTile& tile : m_litTerrainTiles)
        {
            if (tile.vertexCount <= 0 || !tile.vao) continue;
            // Already covered by the ADT pass -- avoid overdraw + z-fighting.
            uint32_t const tileKey = (uint32_t(tile.gy) << 16) | (uint32_t(tile.gx) & 0xFFFFu);
            if (adtCoveredTiles.contains(tileKey)) continue;
            float const tcX = 0.5f * (tile.minX + tile.maxX);
            float const tcY = 0.5f * (tile.minY + tile.maxY);
            // The lit/heightmap pass is the FALLBACK for tiles the streamed
            // ADT terrain hasn't covered.  It is NOT streamed (all navmesh
            // tiles are resident as cheap heightmaps), so without a distance
            // limit it paints the WHOLE far continent as flat grey-green
            // heightmap slabs that persist as the camera moves.  Restrict it
            // to the ADT streaming radius: near gaps get filled (briefly,
            // until ADT streams in), far tiles draw nothing (fog/empty) like
            // the game.
            float const dxCam = tcX - m_camX;
            float const dyCam = tcY - m_camY;
            if (dxCam * dxCam + dyCam * dyCam >
                renderRadiusYards() * renderRadiusYards())
            {
                ++m_totalTilesThisFrame;
                ++m_culledTilesThisFrame;
                continue;
            }
            // Frustum cull.  Sphere centred at the tile's OWN centre (incl. its
            // real Z midpoint) with a radius covering the XY footprint + the
            // tile's height span.  Using the continent-global mesh mid-Z here
            // was the bug: one outlier navmesh tile inflated m_meshMin/MaxZ so
            // every sphere sat far off the terrain and was 100% false-culled
            // -> black terrain (caught via the headless screenshot oracle).
            float const tileSpanXY = std::max(tile.maxX - tile.minX, tile.maxY - tile.minY);
            float const tcZ = 0.5f * (tile.minZ + tile.maxZ);
            float const tcR = tileSpanXY * 0.7071f
                            + 0.5f * std::max(0.0f, tile.maxZ - tile.minZ) + 50.0f;
            ++m_totalTilesThisFrame;
            if (!sphereInFrustum(tcX, tcY, tcZ, tcR))
            {
                ++m_culledTilesThisFrame;
                continue;
            }
            ++m_drawnTilesThisFrame;
            // Lazy texture resolve on first draw of the tile.  We kick off
            // an async CASC + BLP load so the GL thread is never blocked
            // on a fresh tile; until it lands the tile renders with the
            // greyscale-elevation fallback (u_hasTexture=0).
            if (tile.texture == 0)
            {
                auto it = m_minimapTextures.find(tileKey);
                if (it != m_minimapTextures.end())
                    tile.texture = it->second;
                else
                    dispatchMinimapLoadIfNeeded(tile.gx, tile.gy);
            }
            m_litTerrainProgram->setUniformValue(m_litTerrainUHasTex,
                tile.texture != 0 ? 1 : 0);
            if (tile.texture != 0)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tile.texture);
            }
            tile.vao->bind();
            glDrawArrays(GL_TRIANGLES, 0, tile.vertexCount);
            tile.vao->release();
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        m_litTerrainProgram->release();
    }
    else if (m_navProgram && m_terrainVertexCount > 0)
    {
        // Terrain first (depth-write, opaque) - nav polys + everything
        // else paint over it with the depth test.
        m_navProgram->bind();
        m_navProgram->setUniformValue(m_navUMvp, mvp);
        m_terrainVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_terrainVertexCount);
        m_terrainVao.release();
        m_navProgram->release();
    }

    // The textured WMO pass replaces the lit/collision fallback whenever it
    // is enabled AND actually produced instances (real client WMO geometry
    // loaded).  When it is off or empty the old passes still draw so the
    // operator never loses the building silhouette.
    bool const useTexturedWmo =
        m_realistic && m_texturedWmosVisible && m_wmoProgram && !m_wmoInstances.empty();

    if (!useTexturedWmo && m_realistic && m_wmoVisible && m_litTerrainProgram && m_litWmoVertexCount > 0)
    {
        // Lit WMO pass: per-face flat normals + per-group hash tint +
        // Lambert lighting.  Opaque so buildings hide terrain that's
        // inside the structure.
        m_litTerrainProgram->bind();
        m_litTerrainProgram->setUniformValue(m_litTerrainUMvp, mvp);
        applyFogAndSunUniforms(*m_litTerrainProgram);
        m_litTerrainProgram->setUniformValue(m_litTerrainUHasTex, 0);
        m_litWmoVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_litWmoVertexCount);
        m_litWmoVao.release();
        m_litTerrainProgram->release();
    }
    else if (!useTexturedWmo && m_navProgram && m_wmoVisible && m_wmoVertexCount > 0)
    {
        // WMO collision triangles after terrain: gives operator a sense
        // of where buildings sit relative to the heightmap.  Same shader,
        // same xyz+rgba layout, semi-transparent per-vertex alpha so the
        // terrain underneath is still readable.
        m_navProgram->bind();
        m_navProgram->setUniformValue(m_navUMvp, mvp);
        m_wmoVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_wmoVertexCount);
        m_wmoVao.release();
        m_navProgram->release();
    }

    if (kPerf) __tTerrain = __pt.elapsed();

    // Textured WMO pass: real wall/floor/ceiling/bridge geometry from the
    // client WMO group files.  Opaque, depth-write ON (belongs with terrain
    // + doodads, BEFORE water).  Drawn BEFORE doodads so the interior props
    // the doodad pass emits from MODD sit visually on top of the floors.
    if (useTexturedWmo)
        drawTexturedWmos(mvp);

    // Doodad pass: M2 props placed by ADT MDDF.  Lit albedo + frustum
    // culled per-instance.  Drawn between WMO + nav overlay so trees
    // and rocks sit on the terrain but the nav-poly tint still reads.
    if (m_realistic && m_doodadsVisible && m_doodadProgram && !m_doodadInstances.empty())
        drawDoodads(mvp);

    if (kPerf) __tProps = __pt.elapsed();

    // Liquid (water / ocean / magma / slime) pass.  Translucent so it
    // sits on top of every opaque pass; depth-write OFF so subsequent
    // translucent overlays (nav polys, annotation discs) still test
    // depth against the underlying terrain.  Lazy per-tile build keyed
    // by the navmesh tile set, same lifecycle as ADT terrain.
    // STAGE B2/C2: stream water tiles camera-relative (build a budgeted few per
    // frame inside the camera's load ring) instead of one-shot building every
    // navmesh tile on the first paint (a large continent-scale hitch).  Evict
    // tiles the camera has left behind so water VRAM stays bounded.  The build
    // stays synchronous on the GL thread (loadAndUploadWaterTile uploads GL),
    // but the per-frame budget alone removes the hitch.
    if (m_realistic && m_waterVisible && m_waterProgram && m_mesh.ok()
        && m_cascClient && m_cascClient->isOpen() && m_mapDb2)
    {
        // Dispatch many decodes/frame (they run in parallel on the thread pool,
        // off the render thread) and upload a few finished ones/frame (cheap GL).
        streamWaterTiles(16);
        drainPendingWaterUploads(4);
        unloadFarWaterTiles();
    }
    qint64 const __tWaterStream = kPerf ? __pt.elapsed() : 0;
    if (m_realistic && m_waterVisible && m_waterProgram && !m_waterTiles.empty())
    {
        // Time-of-frame in seconds from the widget's first paint.  We
        // deliberately let it grow without bound -- sin() handles big
        // values cleanly and a 32-bit float keeps ~ms precision for
        // multiple hours of wall time, plenty for an editor session.
        static qint64 const kT0 = QDateTime::currentMSecsSinceEpoch();
        float const t = float(QDateTime::currentMSecsSinceEpoch() - kT0) / 1000.0f;

        GLboolean prevDepthMask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_waterProgram->bind();
        m_waterProgram->setUniformValue(m_waterUMvp, mvp);
        m_waterProgram->setUniformValue(m_waterUTime, t);
        m_waterProgram->setUniformValue(m_waterUSkyTint,
            QVector3D(m_horizonColor[0], m_horizonColor[1], m_horizonColor[2]));
        applyFogAndSunUniforms(*m_waterProgram);
        constexpr float kTileSize  = 533.3333f;
        constexpr int   kCenterGid = 32;
        // STAGE C1: hard XY render-distance ring (squared compare, no sqrt) --
        // the same gate the lit terrain pass uses, so water beyond the draw
        // radius is culled identically to terrain instead of drawn to infinity.
        float const waterRingR2 = renderRadiusYards() * renderRadiusYards();
        for (WaterTile& tile : m_waterTiles)
        {
            if (!tile.hasGeometry) continue;
            ++m_totalTilesThisFrame;
            float const tileMaxX = (kCenterGid - tile.gx) * kTileSize;
            float const tileMaxY = (kCenterGid - tile.gy) * kTileSize;
            float const tcX = tileMaxX - kTileSize * 0.5f;
            float const tcY = tileMaxY - kTileSize * 0.5f;
            // C1 distance ring first (cheapest reject): tiles beyond the draw
            // radius count as culled.
            {
                float const dxr = tcX - m_camX, dyr = tcY - m_camY;
                if (dxr * dxr + dyr * dyr > waterRingR2)
                {
                    ++m_culledTilesThisFrame;
                    continue;
                }
            }
            // Cull against a sphere bounding THIS tile's real water surface.
            // The XY footprint is the fixed tile span; the Z extent is the
            // per-tile min/max accumulated across all liquid chunks at build
            // time (mirror of the ADT terrain sphere).  Keep the +50 vertical
            // slack so a flat tile (minZ==maxZ, halfZ=0) degrades gracefully
            // to today's radius instead of culling its own surface.
            float const halfXY   = kTileSize * 0.7071f;
            float const tileMidZ = 0.5f * (tile.minZ + tile.maxZ);
            float const halfZ    = 0.5f * (tile.maxZ - tile.minZ);
            float const tcR      = std::sqrt(halfXY * halfXY + halfZ * halfZ) + 50.0f;
            if (!sphereInFrustum(tcX, tcY, tileMidZ, tcR))
            {
                ++m_culledTilesThisFrame;
                continue;
            }
            ++m_drawnTilesThisFrame;
            for (WaterChunkGpu const& ch : tile.chunks)
            {
                if (!ch.vao || ch.vertexCount <= 0) continue;
                ch.vao->bind();
                glDrawArrays(GL_TRIANGLES, 0, ch.vertexCount);
                ch.vao->release();
            }
        }
        m_waterProgram->release();
        glDepthMask(prevDepthMask);
        // Continuously repaint while water is visible so the wave
        // animation keeps progressing even without user input.
        update();
    }
    if (kPerf) __tWater = __pt.elapsed();

    // Navmesh polygon overlay.  It sits right at the terrain surface, so in
    // realistic mode it Z-fights the textured ADT terrain and reads as a
    // fuzzy sliver "shatter" over every walkable area.  Gate it on the
    // NavMesh layer toggle so it's an opt-in overlay, not an always-on
    // surface that fights the terrain.
    if (m_navProgram && m_navVertexCount > 0 && isLayerVisible(Layer::NavMesh))
    {
        m_navProgram->bind();
        m_navProgram->setUniformValue(m_navUMvp, mvp);
        m_navVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_navVertexCount);
        m_navVao.release();
        m_navProgram->release();
    }

    // Paths use the nav shader: same (xyz, rgba) per-vertex layout.
    if (m_navProgram && m_pathVertexCount > 0 && isLayerVisible(Layer::Paths))
    {
        m_navProgram->bind();
        m_navProgram->setUniformValue(m_navUMvp, mvp);
        glLineWidth(2.0f);
        m_pathVao.bind();
        glDrawArrays(GL_LINES, 0, m_pathVertexCount);
        m_pathVao.release();
        glLineWidth(1.0f);   // restore default; don't let width 2 leak to later GL_LINES passes
        m_navProgram->release();
    }

    if (m_navProgram && m_annotVertexCount > 0 && isLayerVisible(Layer::Annotations))
    {
        m_navProgram->bind();
        m_navProgram->setUniformValue(m_navUMvp, mvp);
        m_annotVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_annotVertexCount);
        m_annotVao.release();
        m_navProgram->release();
    }
    if (m_spawnProgram && m_spawnVertexCount > 0 && isLayerVisible(Layer::Spawns))
    {
        m_spawnProgram->bind();
        m_spawnProgram->setUniformValue(m_spawnUMvp, mvp);
        m_spawnProgram->setUniformValue(m_spawnUViewport,
            QVector2D(float(width()), float(height())));
        m_spawnProgram->setUniformValue(m_spawnUPixelSize, SPAWN_PIXEL_SIZE);
        m_spawnVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_spawnVertexCount);
        m_spawnVao.release();
        m_spawnProgram->release();
    }
    if (m_spawnProgram && m_atrVertexCount > 0 && isLayerVisible(Layer::Areatriggers))
    {
        m_spawnProgram->bind();
        m_spawnProgram->setUniformValue(m_spawnUMvp, mvp);
        m_spawnProgram->setUniformValue(m_spawnUViewport,
            QVector2D(float(width()), float(height())));
        m_spawnProgram->setUniformValue(m_spawnUPixelSize, SPAWN_PIXEL_SIZE);
        m_atrVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_atrVertexCount);
        m_atrVao.release();
        m_spawnProgram->release();
    }
    if (m_spawnProgram && m_gyVertexCount > 0 && isLayerVisible(Layer::Graveyards))
    {
        m_spawnProgram->bind();
        m_spawnProgram->setUniformValue(m_spawnUMvp, mvp);
        m_spawnProgram->setUniformValue(m_spawnUViewport,
            QVector2D(float(width()), float(height())));
        m_spawnProgram->setUniformValue(m_spawnUPixelSize, SPAWN_PIXEL_SIZE);
        m_gyVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_gyVertexCount);
        m_gyVao.release();
        m_spawnProgram->release();
    }

    // Per-second draw/cull report.  Keeps the world_editor log honest about
    // frustum-cull effectiveness + ADT vs minimap-fallback split without
    // spamming on every paint.  The ADT pass is the textured retail look;
    // the lit fallback is the greyscale heightmap projection -- the split
    // tells us at a glance whether ADT loads succeeded.
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastTileLogMs > 1000)
    {
        m_lastTileLogMs = nowMs;
        if (m_totalTilesThisFrame > 0)
            qInfo("[scene3d] tiles drawn=%d culled=%d total=%d",
                m_drawnTilesThisFrame, m_culledTilesThisFrame, m_totalTilesThisFrame);
        // ADT pipeline diagnostic: how many tiles are ADT-textured vs how
        // many still rely on the greyscale heightmap fallback.  Steady state
        // for a fully-loaded continent should be adt>0; if it stays at 0
        // we know the ADT pipeline never produced any textures.
        int adtLoaded = 0;
        for (AdtTileRender const& t : m_adtTerrainTiles)
            if (t.loaded) ++adtLoaded;
        int litFallback = int(m_litTerrainTiles.size()) - adtLoaded;
        if (litFallback < 0) litFallback = 0;
        qInfo("[scene3d-adt] paintGL: ADT tiles loaded=%d, lit fallback=%d "
              "(drawnTiles=%d drawnChunks=%d glErr=0x%x)",
            adtLoaded, litFallback,
            adtTilesDrawnThisFrame, adtChunksDrawnThisFrame,
            unsigned(adtFirstGlErr));
        // Per-pass inventory so we can tell which geometry pass owns the
        // on-screen pixels without needing interactive toggles.  realistic/
        // visibility flags included because a pass with verts>0 but flag=0
        // contributes nothing.  Gated on verbose so production stays quiet.
        if (m_verboseLogging)
        qInfo("[scene3d-pass] realistic=%d | ADT hidden=%d tiles=%zu | "
              "litWMO verts=%d vis=%d | collWMO verts=%d | doodads inst=%zu vis=%d built=%d | water tiles=%zu vis=%d",
            m_realistic ? 1 : 0,
            m_adtTerrainHidden ? 1 : 0, m_adtTerrainTiles.size(),
            int(m_litWmoVertexCount), m_wmoVisible ? 1 : 0,
            int(m_wmoVertexCount),
            m_doodadInstances.size(), m_doodadsVisible ? 1 : 0, m_doodadsBuilt ? 1 : 0,
            m_waterTiles.size(), m_waterVisible ? 1 : 0);
    }

    if (kPerf)
    {
        qint64 const __tEnd = __pt.elapsed();
        // Section deltas (ms).  stream incl. dispatch+evict+doodad rebuild;
        // terrain = ADT chunk draws + lit fallback; props = WMO+doodad draws;
        // water = liquid pass ONLY; overlay = nav/paths/annot/areatrigger/
        // graveyard/spawn passes after water.
        qInfo("[WE_PERF] frame=%lldms | stream=%lld terrain=%lld props=%lld water=%lld(strm=%lld draw=%lld) overlay=%lld | adtChunks=%d",
            (long long)__tEnd,
            (long long)__tStream,
            (long long)(__tTerrain - __tStream),
            (long long)(__tProps - __tTerrain),
            (long long)(__tWater - __tProps),
            (long long)(__tWaterStream - __tProps),
            (long long)(__tWater - __tWaterStream),
            (long long)(__tEnd - __tWater),
            adtChunksDrawnThisFrame);
    }

    // QPainter overlay AFTER all GL draws (Qt rebinds its own program).
    paintOverlay();
}

void SceneView3D::rebuildNavmeshBuffer()
{
    m_navDirty = true;
    if (m_buffersReady) { uploadNavmeshGeometry(); update(); }
}

void SceneView3D::uploadNavmeshGeometry()
{
    m_navDirty = false;
    if (!m_mesh.ok())
    {
        m_navVertexCount = 0;
        return;
    }
    std::vector<NavVertex> verts;
    verts.reserve(size_t(m_mesh.stats().polyCount) * 9);
    dtNavMesh const* nm = m_mesh.navmesh();
    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* tile = nm->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount <= 0) continue;
        for (int p = 0; p < tile->header->polyCount; ++p)
        {
            dtPoly const& poly = tile->polys[p];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
            int const nv = poly.vertCount;
            if (nv < 3) continue;
            Rgb const c = colorForArea(poly.getArea());
            for (int i = 1; i + 1 < nv; ++i)
            {
                int const idxs[3] = { 0, i, i + 1 };
                for (int k = 0; k < 3; ++k)
                {
                    float const* v = &tile->verts[poly.verts[idxs[k]] * 3];
                    NavVertex nv2;
                    // Detour (y, z, x) -> TC (x, y, z)
                    nv2.x = v[2];  nv2.y = v[0];  nv2.z = v[1];
                    nv2.r = c.r;   nv2.g = c.g;   nv2.b = c.b;   nv2.a = 220;
                    verts.push_back(nv2);
                }
            }
        }
    }
    m_navVertexCount = static_cast<GLsizei>(verts.size());
    m_navVao.bind();
    m_navVbo.bind();
    if (!verts.empty())
        m_navVbo.allocate(verts.data(), int(verts.size() * sizeof(NavVertex)));
    else
        m_navVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(NavVertex),
        reinterpret_cast<void*>(offsetof(NavVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(NavVertex),
        reinterpret_cast<void*>(offsetof(NavVertex, r)));
    m_navVbo.release();
    m_navVao.release();
}

void SceneView3D::rebuildSpawnBuffer()
{
    m_spawnDirty = true;
    if (m_buffersReady) { uploadSpawnGeometry(); update(); }
}

void SceneView3D::uploadSpawnGeometry()
{
    m_spawnDirty = false;
    std::vector<SpawnVertex> verts;
    verts.reserve(m_spawns.size() * 6);
    for (Spawn const& s : m_spawns)
    {
        uint8_t r, g, b;
        if (s.kind == SpawnKind::Creature) { r = 230; g = 60; b = 60; }
        else                                { r = 60;  g = 140; b = 230; }
        constexpr uint8_t a = 230;
        for (auto const& c : SPAWN_CORNERS)
        {
            SpawnVertex sv;
            sv.x = s.worldX; sv.y = s.worldY; sv.z = s.worldZ;
            sv.ox = c.x; sv.oy = c.y;
            sv.r = r; sv.g = g; sv.b = b; sv.a = a;
            verts.push_back(sv);
        }
    }
    m_spawnVertexCount = static_cast<GLsizei>(verts.size());
    m_spawnVao.bind();
    m_spawnVbo.bind();
    if (!verts.empty())
        m_spawnVbo.allocate(verts.data(), int(verts.size() * sizeof(SpawnVertex)));
    else
        m_spawnVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, r)));
    m_spawnVbo.release();
    m_spawnVao.release();
}

int SceneView3D::hitTestSpawn(QPoint const& screen, float pixelTolerance) const
{
    if (m_spawns.empty()) return -1;
    QMatrix4x4 const mvp = projectionMatrix() * viewMatrix();
    int best = -1;
    float bestD = pixelTolerance;
    for (size_t i = 0; i < m_spawns.size(); ++i)
    {
        Spawn const& s = m_spawns[i];
        float sx, sy;
        if (!projectToScreen(s.worldX, s.worldY, s.worldZ, mvp, sx, sy))
            continue;
        float const dx   = sx - float(screen.x());
        float const dy   = sy - float(screen.y());
        float const d    = std::sqrt(dx * dx + dy * dy);
        if (d < bestD)
        {
            bestD = d;
            best  = int(i);
        }
    }
    return best;
}

bool SceneView3D::projectToScreen(float wx, float wy, float wz,
                                  QMatrix4x4 const& mvp,
                                  float& outSx, float& outSy) const
{
    QVector4D const clip = mvp * QVector4D(wx, wy, wz, 1.0f);
    if (clip.w() <= 0.0f) return false;
    float const ndcX = clip.x() / clip.w();
    float const ndcY = clip.y() / clip.w();
    outSx = (ndcX * 0.5f + 0.5f) * float(width());
    outSy = (1.0f - (ndcY * 0.5f + 0.5f)) * float(height());
    return true;
}

bool SceneView3D::screenToWorldRay(QPoint const& screen,
                                   QVector3D& outOrigin, QVector3D& outDir) const
{
    // The projection uses logical width()/height() for its aspect, so the NDC
    // mapping here is in logical pixels too (no device-pixel-ratio).
    float const w = float(std::max(1, width()));
    float const h = float(std::max(1, height()));
    float const ndcX = 2.0f * float(screen.x()) / w - 1.0f;
    float const ndcY = 1.0f - 2.0f * float(screen.y()) / h;

    bool invertible = false;
    QMatrix4x4 const inv = (projectionMatrix() * viewMatrix()).inverted(&invertible);
    if (!invertible)
        return false;

    // Unproject the near and far plane points; the ray runs between them.
    QVector4D const nearH = inv * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D const farH  = inv * QVector4D(ndcX, ndcY,  1.0f, 1.0f);
    if (std::abs(nearH.w()) < 1e-9f || std::abs(farH.w()) < 1e-9f)
        return false;
    QVector3D const p0(nearH.x() / nearH.w(), nearH.y() / nearH.w(), nearH.z() / nearH.w());
    QVector3D const p1(farH.x()  / farH.w(),  farH.y()  / farH.w(),  farH.z()  / farH.w());

    outOrigin = p0;
    outDir    = p1 - p0;     // NOT normalised; callers normalise as needed
    return true;
}

bool SceneView3D::screenRayToPlaneZ(QPoint const& screen, float planeZ, QVector3D& out) const
{
    QVector3D p0, dir;
    if (!screenToWorldRay(screen, p0, dir))
        return false;
    if (std::abs(dir.z()) < 1e-6f)
        return false;                         // ray parallel to the plane
    float const t = (planeZ - p0.z()) / dir.z();
    if (t < 0.0f)
        return false;                         // plane is behind the camera
    out = p0 + t * dir;
    return true;
}

namespace
{
// Squared distance from point P to segment AB in 2D screen space.
float distPointSegSq2D(float px, float py,
                       float ax, float ay,
                       float bx, float by)
{
    float const dx = bx - ax, dy = by - ay;
    float const lenSq = dx*dx + dy*dy;
    if (lenSq < 1e-6f)
    {
        float const ex = px - ax, ey = py - ay;
        return ex*ex + ey*ey;
    }
    float t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
    t = std::clamp(t, 0.0f, 1.0f);
    float const cx = ax + t * dx, cy = ay + t * dy;
    float const ex = px - cx, ey = py - cy;
    return ex*ex + ey*ey;
}
} // namespace

int SceneView3D::hitTestPath(QPoint const& screen, float pixelTolerance) const
{
    if (m_paths.empty()) return -1;
    QMatrix4x4 const mvp = projectionMatrix() * viewMatrix();
    int best = -1;
    float bestDSq = pixelTolerance * pixelTolerance;
    float const px = float(screen.x()), py = float(screen.y());
    for (size_t i = 0; i < m_paths.size(); ++i)
    {
        auto const& nodes = m_paths[i].nodes;
        if (nodes.size() < 2) continue;
        for (size_t j = 1; j < nodes.size(); ++j)
        {
            float ax, ay, bx, by;
            // Skip segments with either endpoint behind camera.
            if (!projectToScreen(nodes[j-1].x, nodes[j-1].y, nodes[j-1].z, mvp, ax, ay))
                continue;
            if (!projectToScreen(nodes[j  ].x, nodes[j  ].y, nodes[j  ].z, mvp, bx, by))
                continue;
            float const dsq = distPointSegSq2D(px, py, ax, ay, bx, by);
            if (dsq < bestDSq)
            {
                bestDSq = dsq;
                best    = int(i);
            }
        }
    }
    return best;
}

int SceneView3D::hitTestAnnotation(QPoint const& screen) const
{
    if (m_annotations.empty()) return -1;
    QMatrix4x4 const mvp = projectionMatrix() * viewMatrix();
    int best = -1;
    // For each annotation, project the world-space center AND a point
    // offset by `radius` in +X (north).  The screen-space distance
    // between those two projections approximates the disc's pixel
    // radius at the camera's current zoom.  If `screen` is inside that
    // pixel radius, it's a hit.  When multiple discs overlap we pick
    // the one whose projected center is closest to the click (tightest
    // fit -- mirrors NavMeshView::hitTestAnnotation's intent).
    float bestCenterDSq = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < m_annotations.size(); ++i)
    {
        Annotation const& a = m_annotations[i];
        float cx, cy, ex, ey;
        if (!projectToScreen(a.x,            a.y, a.z, mvp, cx, cy))
            continue;
        if (!projectToScreen(a.x + a.radius, a.y, a.z, mvp, ex, ey))
            continue;
        float const rdx = ex - cx, rdy = ey - cy;
        float const screenRadiusSq = rdx*rdx + rdy*rdy;
        float const px = float(screen.x()), py = float(screen.y());
        float const dx = px - cx, dy = py - cy;
        float const dsq = dx*dx + dy*dy;
        if (dsq <= screenRadiusSq && dsq < bestCenterDSq)
        {
            bestCenterDSq = dsq;
            best          = int(i);
        }
    }
    return best;
}

void SceneView3D::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_placementMode)
    {
        // Placement mode hijacks the left-click: emit the world-space pick ray
        // and let MainWindow march it against the authoritative terrain/WMO
        // height source (snapToGround) to find the surface point.  Depth-buffer
        // readback proved unreliable across drivers here, so we don't use it.
        QVector3D origin, dir;
        if (screenToWorldRay(event->pos(), origin, dir))
            emit placementRayRequested(origin.x(), origin.y(), origin.z(),
                                       dir.x(), dir.y(), dir.z());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton)
    {
        // Priority: spawn > path > annotation.  Spawns are typically the
        // smallest hit target and the most clickable, so they win when
        // overlapping the cursor.
        int const sIdx = hitTestSpawn(event->pos());
        if (sIdx >= 0)
        {
            emit spawnClicked(sIdx);   // select immediately
            // Arm drag-to-move: the spawn follows the cursor in its current
            // horizontal plane until release.  A pure click (no movement)
            // stays a selection -- spawnMoved only fires once m_dragMoved.
            if (sIdx < int(m_spawns.size()))
            {
                m_dragSpawnIndex = sIdx;
                m_dragPlaneZ     = m_spawns[size_t(sIdx)].worldZ;
                m_dragMoved      = false;
            }
            event->accept();
            return;
        }
        int const pIdx = hitTestPath(event->pos());
        if (pIdx >= 0)
        {
            emit pathClicked(pIdx);
            event->accept();
            return;
        }
        int const aIdx = hitTestAnnotation(event->pos());
        if (aIdx >= 0)
        {
            emit annotationClicked(aIdx);
            event->accept();
            return;
        }
        int const atrIdx = hitTestAreatrigger(event->pos());
        if (atrIdx >= 0)
        {
            emit areatriggerClicked(atrIdx);
            event->accept();
            return;
        }
        int const gyIdx = hitTestGraveyard(event->pos());
        if (gyIdx >= 0)
        {
            emit graveyardClicked(gyIdx);
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::RightButton)
    {
        m_rotating = true;
        m_lastMouse = event->pos();
        setCursor(Qt::BlankCursor);
    }
    QOpenGLWidget::mousePressEvent(event);
}

void SceneView3D::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragSpawnIndex >= 0 && (event->buttons() & Qt::LeftButton))
    {
        if (m_dragSpawnIndex < int(m_spawns.size()))
        {
            QVector3D hit;
            if (screenRayToPlaneZ(event->pos(), m_dragPlaneZ, hit))
            {
                // Move in XY only; Z stays at the start altitude (right for
                // flying mobs; ground mobs re-snap on release if enabled).
                m_spawns[size_t(m_dragSpawnIndex)].worldX = hit.x();
                m_spawns[size_t(m_dragSpawnIndex)].worldY = hit.y();
                m_dragMoved = true;
                rebuildSpawnBuffer();   // live billboard preview
            }
        }
        event->accept();
        return;
    }
    if (m_rotating)
    {
        QPoint const d = event->pos() - m_lastMouse;
        m_lastMouse = event->pos();
        m_yaw   -= d.x() * ROTATE_SENS;
        m_pitch -= d.y() * ROTATE_SENS;
        // Clamp pitch to avoid gimbal flip.
        m_pitch = std::clamp(m_pitch, -1.55f, 1.55f);
        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void SceneView3D::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragSpawnIndex >= 0)
    {
        int const idx = m_dragSpawnIndex;
        bool const moved = m_dragMoved;
        m_dragSpawnIndex = -1;
        m_dragMoved      = false;
        // Only commit a move if the cursor actually dragged -- a plain click
        // is a selection, not an edit (avoids spurious snap-to-ground churn).
        if (moved && idx < int(m_spawns.size()))
        {
            render::Spawn const& s = m_spawns[size_t(idx)];
            emit spawnMoved(idx, s.worldX, s.worldY, m_dragPlaneZ);
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton && m_rotating)
    {
        m_rotating = false;
        unsetCursor();
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void SceneView3D::wheelEvent(QWheelEvent* event)
{
    // Wheel = dolly along look vector.
    float const steps = float(event->angleDelta().y()) / 120.0f;
    float const cp = std::cos(m_pitch);
    float const sp = std::sin(m_pitch);
    float const cy = std::cos(m_yaw);
    float const sy = std::sin(m_yaw);
    float const speed = 50.0f * steps;
    m_camX += cp * cy * speed;
    m_camY += cp * sy * speed;
    m_camZ += sp * speed;
    update();
    event->accept();
}

void SceneView3D::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_playingPath)
    {
        stopPathPlayback();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F)
    {
        frameMesh();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Tab)
    {
        m_overlayVisible = !m_overlayVisible;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_BracketLeft)
    {
        m_flySpeed = std::max(1.0f, m_flySpeed * 0.5f);
        saveFlySpeed();
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_BracketRight)
    {
        m_flySpeed = std::min(20000.0f, m_flySpeed * 2.0f);
        saveFlySpeed();
        update();
        event->accept();
        return;
    }
    // Diagnostic isolation toggles: 1 = WMO meshes, 2 = M2 doodads,
    // 3 = ADT terrain.  Lets us pin down which render pass is producing
    // the shattered-spike artifact without touching the Realistic toggle
    // (which currently crashes on the off-transition).
    if (event->key() == Qt::Key_1)
    {
        m_wmoVisible = !m_wmoVisible;
        qInfo("[scene3d-diag] WMO visible=%d (litWmoVerts=%d collWmoVerts=%d)",
            m_wmoVisible ? 1 : 0, int(m_litWmoVertexCount), int(m_wmoVertexCount));
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_2)
    {
        m_doodadsVisible = !m_doodadsVisible;
        qInfo("[scene3d-diag] doodads visible=%d (instances=%zu)",
            m_doodadsVisible ? 1 : 0, m_doodadInstances.size());
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_3)
    {
        m_adtTerrainHidden = !m_adtTerrainHidden;
        qInfo("[scene3d-diag] ADT terrain hidden=%d (tiles=%zu)",
            m_adtTerrainHidden ? 1 : 0, m_adtTerrainTiles.size());
        update();
        event->accept();
        return;
    }
    if (!event->isAutoRepeat())
        m_keys.insert(event->key());
    QOpenGLWidget::keyPressEvent(event);
}

void SceneView3D::keyReleaseEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
        m_keys.remove(event->key());
    QOpenGLWidget::keyReleaseEvent(event);
}

void SceneView3D::focusOutEvent(QFocusEvent* event)
{
    m_keys.clear();
    m_rotating = false;
    QOpenGLWidget::focusOutEvent(event);
}

void SceneView3D::rebuildPathBuffer()
{
    m_pathDirty = true;
    if (m_buffersReady) { uploadPathGeometry(); update(); }
}

void SceneView3D::uploadPathGeometry()
{
    m_pathDirty = false;
    std::vector<PathVertex> verts;
    for (Path const& p : m_paths)
    {
        if (p.nodes.size() < 2) continue;
        // Deterministic per-path colour (same hash as NavMeshView).
        uint32_t const h = p.pathId * 2654435761u;
        uint8_t const r = uint8_t((h >> 16) & 0xFF);
        uint8_t const g = uint8_t((h >>  8) & 0xFF);
        uint8_t const b = uint8_t( h        & 0xFF);
        for (size_t i = 0; i + 1 < p.nodes.size(); ++i)
        {
            verts.push_back({ p.nodes[i].x,   p.nodes[i].y,   p.nodes[i].z + 0.5f,   r, g, b, 230 });
            verts.push_back({ p.nodes[i+1].x, p.nodes[i+1].y, p.nodes[i+1].z + 0.5f, r, g, b, 230 });
        }
    }
    m_pathVertexCount = static_cast<GLsizei>(verts.size());
    m_pathVao.bind();
    m_pathVbo.bind();
    if (!verts.empty())
        m_pathVbo.allocate(verts.data(), int(verts.size() * sizeof(PathVertex)));
    else
        m_pathVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
        reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
        reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_pathVbo.release();
    m_pathVao.release();
}

void SceneView3D::rebuildAnnotBuffer()
{
    m_annotDirty = true;
    if (m_buffersReady) { uploadAnnotGeometry(); update(); }
}

void SceneView3D::uploadAnnotGeometry()
{
    m_annotDirty = false;
    constexpr int SLICES = 24;
    std::vector<AnnotVertex> verts;
    verts.reserve(m_annotations.size() * SLICES * 3);
    for (Annotation const& a : m_annotations)
    {
        // Per-kind colour (matches NavMeshView palette roughly).
        uint8_t r = 180, g = 180, b = 180;
        switch (a.kind)
        {
            case AnnotationKind::Road:      r=255; g=170; b=  0; break;
            case AnnotationKind::Crossroad: r=255; g=235; b= 25; break;
            case AnnotationKind::City:      r=240; g=140; b=215; break;
            case AnnotationKind::Village:   r=180; g=115; b=180; break;
            case AnnotationKind::Hub:       r=100; g=215; b=100; break;
            case AnnotationKind::Danger:    r=240; g= 40; b= 40; break;
            case AnnotationKind::Vendor:    r= 75; g=205; b=240; break;
            case AnnotationKind::Mailbox:   r=140; g=140; b=240; break;
            case AnnotationKind::Innkeeper: r=240; g=190; b=140; break;
            default: break;
        }
        float const z = a.z + 0.2f; // slight lift over terrain
        // Triangle fan around (a.x, a.y) at radius a.radius.
        for (int i = 0; i < SLICES; ++i)
        {
            float const t0 = 2.0f * float(M_PI) * float(i)     / float(SLICES);
            float const t1 = 2.0f * float(M_PI) * float(i + 1) / float(SLICES);
            verts.push_back({ a.x,                        a.y,                        z, r, g, b, 110 });
            verts.push_back({ a.x + a.radius * std::cos(t0), a.y + a.radius * std::sin(t0), z, r, g, b, 110 });
            verts.push_back({ a.x + a.radius * std::cos(t1), a.y + a.radius * std::sin(t1), z, r, g, b, 110 });
        }
    }
    m_annotVertexCount = static_cast<GLsizei>(verts.size());
    m_annotVao.bind();
    m_annotVbo.bind();
    if (!verts.empty())
        m_annotVbo.allocate(verts.data(), int(verts.size() * sizeof(AnnotVertex)));
    else
        m_annotVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AnnotVertex),
        reinterpret_cast<void*>(offsetof(AnnotVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(AnnotVertex),
        reinterpret_cast<void*>(offsetof(AnnotVertex, r)));
    m_annotVbo.release();
    m_annotVao.release();
}

void SceneView3D::rebuildTerrainBuffer()
{
    if (!m_mapCache || m_mapCache->mapsDir().empty() || !m_mesh.ok()) return;

    // Walk the navmesh tile AABBs to find which (gx, gy) tiles are
    // present, then sample each at a 33x33 grid (32x32 quads per tile).
    constexpr int GRID = 33;       // verts per side
    constexpr int CELL = GRID - 1; // quads per side
    constexpr int CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;

    std::vector<std::pair<int, int>> tiles;
    dtNavMesh const* nm = m_mesh.navmesh();
    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* mt = nm->getTile(ti);
        if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
        float const minX = mt->header->bmin[2];
        float const minY = mt->header->bmin[0];
        int const gx = int(std::floor(CENTER_GRID_ID - minX / TILE_SIZE));
        int const gy = int(std::floor(CENTER_GRID_ID - minY / TILE_SIZE));
        tiles.emplace_back(gx, gy);
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    std::vector<TerrainVertex> verts;
    verts.reserve(tiles.size() * CELL * CELL * 6);

    for (auto const& [gx, gy] : tiles)
    {
        float const worldMaxX = (CENTER_GRID_ID - gx) * TILE_SIZE;
        float const worldMinX = worldMaxX - TILE_SIZE;
        float const worldMaxY = (CENTER_GRID_ID - gy) * TILE_SIZE;
        float const worldMinY = worldMaxY - TILE_SIZE;

        std::vector<float> samples(GRID * GRID);
        float hmin = 1e9f, hmax = -1e9f;
        for (int row = 0; row < GRID; ++row)
        {
            for (int col = 0; col < GRID; ++col)
            {
                float const wx = worldMaxX - float(row) * (TILE_SIZE / float(CELL));
                float const wy = worldMaxY - float(col) * (TILE_SIZE / float(CELL));
                float const h  = m_mapCache->heightAt(m_heightmapMapId, wx, wy);
                samples[row * GRID + col] = h;
                if (h > io::ADT_INVALID_HEIGHT)
                {
                    if (h < hmin) hmin = h;
                    if (h > hmax) hmax = h;
                }
            }
        }
        if (hmax <= hmin) { hmin = 0.0f; hmax = 1.0f; }
        float const range = hmax - hmin;

        auto pushVert = [&](int row, int col)
        {
            float const wx = worldMaxX - float(row) * (TILE_SIZE / float(CELL));
            float const wy = worldMaxY - float(col) * (TILE_SIZE / float(CELL));
            float       h  = samples[row * GRID + col];
            if (h <= io::ADT_INVALID_HEIGHT) h = hmin;
            uint8_t r, g, b;
            if (h < 0.0f)
            {
                float const t = std::min(1.0f, -h / 60.0f);
                r = uint8_t(40 + (1.0f - t) * 20);
                g = uint8_t(60 + (1.0f - t) * 30);
                b = uint8_t(120 + t * 80);
            }
            else
            {
                float const t = std::clamp((h - hmin) / range, 0.0f, 1.0f);
                uint8_t const v = uint8_t(70 + t * 150);
                r = v;
                g = uint8_t(std::min(255, int(v) + int(t * 18)));
                b = uint8_t(v - int(t * 20));
            }
            verts.push_back({ wx, wy, h, r, g, b, 255 });
        };

        for (int row = 0; row < CELL; ++row)
        {
            for (int col = 0; col < CELL; ++col)
            {
                pushVert(row,     col);
                pushVert(row + 1, col);
                pushVert(row,     col + 1);
                pushVert(row + 1, col);
                pushVert(row + 1, col + 1);
                pushVert(row,     col + 1);
            }
        }
    }

    m_terrainVertexCount = static_cast<GLsizei>(verts.size());
    m_terrainVao.bind();
    m_terrainVbo.bind();
    if (!verts.empty())
        m_terrainVbo.allocate(verts.data(), int(verts.size() * sizeof(TerrainVertex)));
    else
        m_terrainVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
        reinterpret_cast<void*>(offsetof(TerrainVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TerrainVertex),
        reinterpret_cast<void*>(offsetof(TerrainVertex, r)));
    m_terrainVbo.release();
    m_terrainVao.release();
}

void SceneView3D::startPathPlayback(Path const& p, float velocity)
{
    if (p.nodes.size() < 2)
        return;
    m_playbackNodes = p.nodes;
    m_playbackSegment = 0;
    m_playbackT       = 0.0f;
    m_playbackLastMs  = QDateTime::currentMSecsSinceEpoch();
    // Use the requested velocity if positive, else the path's recorded
    // velocity, else a comfortable 5 yards/sec default.
    if (velocity > 0.0f)
        m_playbackVelocity = velocity;
    else if (p.velocity > 0.0f)
        m_playbackVelocity = p.velocity;
    else
        m_playbackVelocity = 5.0f;
    m_playingPath = true;
    // Seed the camera at the first node so the operator sees a clean
    // start.  We offset eye height by ~1.7 yards (player height) so we
    // look like a player walking the route.
    m_camX = m_playbackNodes.front().x;
    m_camY = m_playbackNodes.front().y;
    m_camZ = m_playbackNodes.front().z + 1.7f;
    update();
    emit pathPlaybackTick(1, int(m_playbackNodes.size()));
}

void SceneView3D::stopPathPlayback()
{
    if (!m_playingPath) return;
    m_playingPath = false;
    m_playbackNodes.clear();
    m_playbackSegment = 0;
    m_playbackT       = 0.0f;
    emit pathPlaybackFinished();
}

void SceneView3D::onTick()
{
    if (m_playingPath && m_playbackNodes.size() >= 2)
    {
        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        float  const dt  = std::min(0.1f, float(now - m_playbackLastMs) / 1000.0f);
        m_playbackLastMs = now;

        bool finished = false;
        // Advance T along segment(s).  A single tick may cross multiple
        // short segments at high velocity, so loop.
        float dtRemaining = dt;
        while (dtRemaining > 0.0f && !finished
               && m_playbackSegment + 1 < int(m_playbackNodes.size()))
        {
            PathNode const& a = m_playbackNodes[m_playbackSegment];
            PathNode const& b = m_playbackNodes[m_playbackSegment + 1];
            float const dx = b.x - a.x;
            float const dy = b.y - a.y;
            float const dz = b.z - a.z;
            float const segLen = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (segLen < 0.01f)
            {
                m_playbackSegment += 1;
                m_playbackT = 0.0f;
                emit pathPlaybackTick(m_playbackSegment + 1, int(m_playbackNodes.size()));
                continue;
            }
            float const remainingDist = (1.0f - m_playbackT) * segLen;
            float const distThisTick  = m_playbackVelocity * dtRemaining;
            if (distThisTick >= remainingDist)
            {
                // Cross segment boundary; bank the remainder.
                m_playbackSegment += 1;
                m_playbackT = 0.0f;
                dtRemaining -= remainingDist / m_playbackVelocity;
                emit pathPlaybackTick(m_playbackSegment + 1, int(m_playbackNodes.size()));
                if (m_playbackSegment + 1 >= int(m_playbackNodes.size()))
                    finished = true;
            }
            else
            {
                m_playbackT += distThisTick / segLen;
                dtRemaining = 0.0f;
            }
        }

        if (finished)
        {
            // Settle camera at the last node, then stop.
            PathNode const& last = m_playbackNodes.back();
            m_camX = last.x;
            m_camY = last.y;
            m_camZ = last.z + 1.7f;
            stopPathPlayback();
            update();
            return;
        }

        // Compute current camera position by interpolating into the
        // active segment, then orient yaw/pitch toward the segment's
        // far endpoint.  Look vector in TC: dx = b.x - a.x, etc.
        PathNode const& a = m_playbackNodes[m_playbackSegment];
        PathNode const& b = m_playbackNodes[m_playbackSegment + 1];
        float const t = m_playbackT;
        m_camX = a.x + (b.x - a.x) * t;
        m_camY = a.y + (b.y - a.y) * t;
        m_camZ = (a.z + (b.z - a.z) * t) + 1.7f;
        float const dx = b.x - a.x;
        float const dy = b.y - a.y;
        float const dz = b.z - a.z;
        float const flatLen = std::sqrt(dx*dx + dy*dy);
        if (flatLen > 0.001f)
            m_yaw = std::atan2(dy, dx);
        m_pitch = std::atan2(dz, std::max(0.001f, flatLen));
        update();
        // Fall through to also process WASD input (operator can nudge
        // camera mid-playback).  Actually no -- it fights the playback.
        return;
    }

    if (m_keys.isEmpty())
        return;
    constexpr float DT = 0.016f;
    float speed = m_flySpeed * DT;
    if (m_keys.contains(Qt::Key_Shift)) speed *= 5.0f;
    if (m_keys.contains(Qt::Key_Alt))   speed *= 0.2f;

    float const cp = std::cos(m_pitch);
    float const sp = std::sin(m_pitch);
    float const cy = std::cos(m_yaw);
    float const sy = std::sin(m_yaw);
    // Forward (TC frame): (cp*cy, cp*sy, sp).  Right = forward x up.
    QVector3D const fwd(cp * cy, cp * sy, sp);
    QVector3D const up (0.0f, 0.0f, 1.0f);
    QVector3D right = QVector3D::crossProduct(fwd, up).normalized();
    bool moved = false;
    auto step = [&](QVector3D const& v, float dir)
    {
        m_camX += v.x() * speed * dir;
        m_camY += v.y() * speed * dir;
        m_camZ += v.z() * speed * dir;
        moved = true;
    };
    if (m_keys.contains(Qt::Key_W))     step(fwd,   1.0f);
    if (m_keys.contains(Qt::Key_S))     step(fwd,  -1.0f);
    if (m_keys.contains(Qt::Key_A))     step(right, -1.0f);
    if (m_keys.contains(Qt::Key_D))     step(right,  1.0f);
    if (m_keys.contains(Qt::Key_E))     step(up,     1.0f);
    if (m_keys.contains(Qt::Key_Q))     step(up,    -1.0f);
    if (moved)
        update();
}

void SceneView3D::rebuildAreatriggerBuffer()
{
    m_atrDirty = true;
    if (m_buffersReady) { uploadAreatriggerGeometry(); update(); }
}

void SceneView3D::uploadAreatriggerGeometry()
{
    m_atrDirty = false;
    std::vector<SpawnVertex> verts;
    verts.reserve(m_areatriggers.size() * 6);
    for (Areatrigger const& a : m_areatriggers)
    {
        // Purple to match the 2D marker palette.
        constexpr uint8_t r = 180, g = 60, b = 200, alpha = 220;
        for (auto const& c : SPAWN_CORNERS)
        {
            SpawnVertex sv;
            sv.x = a.x; sv.y = a.y; sv.z = a.z;
            sv.ox = c.x; sv.oy = c.y;
            sv.r = r; sv.g = g; sv.b = b; sv.a = alpha;
            verts.push_back(sv);
        }
    }
    m_atrVertexCount = static_cast<GLsizei>(verts.size());
    m_atrVao.bind();
    m_atrVbo.bind();
    if (!verts.empty())
        m_atrVbo.allocate(verts.data(), int(verts.size() * sizeof(SpawnVertex)));
    else
        m_atrVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, r)));
    m_atrVbo.release();
    m_atrVao.release();
}

void SceneView3D::rebuildGraveyardBuffer()
{
    m_gyDirty = true;
    if (m_buffersReady) { uploadGraveyardGeometry(); update(); }
}

void SceneView3D::uploadGraveyardGeometry()
{
    m_gyDirty = false;
    std::vector<SpawnVertex> verts;
    verts.reserve(m_graveyards.size() * 6);
    for (Graveyard const& gy : m_graveyards)
    {
        // Cyan to match the 2D marker palette.
        constexpr uint8_t r = 60, g = 200, b = 200, alpha = 220;
        for (auto const& c : SPAWN_CORNERS)
        {
            SpawnVertex sv;
            sv.x = gy.x; sv.y = gy.y; sv.z = gy.z;
            sv.ox = c.x; sv.oy = c.y;
            sv.r = r; sv.g = g; sv.b = b; sv.a = alpha;
            verts.push_back(sv);
        }
    }
    m_gyVertexCount = static_cast<GLsizei>(verts.size());
    m_gyVao.bind();
    m_gyVbo.bind();
    if (!verts.empty())
        m_gyVbo.allocate(verts.data(), int(verts.size() * sizeof(SpawnVertex)));
    else
        m_gyVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpawnVertex),
        reinterpret_cast<void*>(offsetof(SpawnVertex, r)));
    m_gyVbo.release();
    m_gyVao.release();
}

int SceneView3D::hitTestAreatrigger(QPoint const& screen, float pixelTolerance) const
{
    if (m_areatriggers.empty()) return -1;
    QMatrix4x4 const mvp = projectionMatrix() * viewMatrix();
    int best = -1;
    float bestD = pixelTolerance;
    for (size_t i = 0; i < m_areatriggers.size(); ++i)
    {
        Areatrigger const& a = m_areatriggers[i];
        float sx, sy;
        if (!projectToScreen(a.x, a.y, a.z, mvp, sx, sy)) continue;
        float const dx = sx - float(screen.x()), dy = sy - float(screen.y());
        float const d  = std::sqrt(dx*dx + dy*dy);
        if (d < bestD) { bestD = d; best = int(i); }
    }
    return best;
}

int SceneView3D::hitTestGraveyard(QPoint const& screen, float pixelTolerance) const
{
    if (m_graveyards.empty()) return -1;
    QMatrix4x4 const mvp = projectionMatrix() * viewMatrix();
    int best = -1;
    float bestD = pixelTolerance;
    for (size_t i = 0; i < m_graveyards.size(); ++i)
    {
        Graveyard const& g = m_graveyards[i];
        float sx, sy;
        if (!projectToScreen(g.x, g.y, g.z, mvp, sx, sy)) continue;
        float const dx = sx - float(screen.x()), dy = sy - float(screen.y());
        float const d  = std::sqrt(dx*dx + dy*dy);
        if (d < bestD) { bestD = d; best = int(i); }
    }
    return best;
}

void SceneView3D::rebuildWmoBuffer()
{
    m_wmoDirty = true;
    if (m_buffersReady) { uploadWmoGeometry(); update(); }
}

void SceneView3D::setRealistic(bool on)
{
    if (m_realistic == on) return;
    m_realistic = on;
    // Drop the per-tile lit pipeline when toggling off so a future
    // toggle-on rebuilds with fresh data (and avoids holding GL state
    // we won't draw with).  Keep the texture cache; reloading from
    // CASC is the slow part.
    if (!m_realistic)
    {
        destroyLitTerrainTiles();
        destroyAdtTerrainTiles();
        destroyDoodadResources();
        destroyTexturedWmoResources();
    }
    // Re-arm the ADT-gate diagnostic so the next paint logs the new state.
    m_adtGateDiagLogged = false;
    qInfo("[scene3d] setRealistic(%d) -- ADT/lit pipelines re-armed", on ? 1 : 0);
    update();
}

void SceneView3D::setDoodadsVisible(bool on)
{
    if (m_doodadsVisible == on) return;
    m_doodadsVisible = on;
    update();
}

void SceneView3D::setTexturedWmosVisible(bool on)
{
    if (m_texturedWmosVisible == on) return;
    m_texturedWmosVisible = on;
    update();
}

void SceneView3D::setLayerVisible(Layer layer, bool visible)
{
    size_t const idx = size_t(layer);
    if (idx >= size_t(Layer::_Count)) return;
    if (m_layer3dVisible[idx] == visible) return;
    m_layer3dVisible[idx] = visible;
    update();
}

bool SceneView3D::isLayerVisible(Layer layer) const noexcept
{
    size_t const idx = size_t(layer);
    if (idx >= size_t(Layer::_Count)) return true;
    return m_layer3dVisible[idx];
}

void SceneView3D::setCascClient(io::CascClient* casc, io::MapDb2Lookup* mapDb2)
{
    m_cascClient = casc;
    m_mapDb2     = mapDb2;
    // Drop cached textures + tile VAOs; the next realistic-pass paint
    // will re-resolve every tile through CASC.
    destroyMinimapTextures();
    destroyLitTerrainTiles();
    destroyAdtTerrainTiles();
    destroyDoodadResources();
    destroyTexturedWmoResources();
    // Rebuild the terrain-texture cache on top of the new CASC client.
    if (m_terrainTextureCache)
    {
        makeCurrent();
        m_terrainTextureCache->clear(*this);
        doneCurrent();
    }
    m_terrainTextureCache.reset();
    if (casc)
        m_terrainTextureCache = std::make_unique<TerrainTextureCache>(casc);
    update();
}

void SceneView3D::setMinimapDir(QString const& dir)
{
    if (m_minimapDir == dir) return;
    m_minimapDir = dir;
    destroyMinimapTextures();
    destroyLitTerrainTiles();
    update();
}

void SceneView3D::destroyMinimapTextures()
{
    if (m_minimapTextures.empty()) return;
    bool const haveContext = (QOpenGLContext::currentContext() == context());
    bool const widgetContext = context() != nullptr;
    if (!haveContext && widgetContext) makeCurrent();
    for (auto& [k, tex] : m_minimapTextures)
        if (tex != 0) glDeleteTextures(1, &tex);
    if (!haveContext && widgetContext) doneCurrent();
    m_minimapTextures.clear();
    m_minimapInFlight.clear();
    {
        std::lock_guard<std::mutex> g(m_minimapPendingMutex);
        m_minimapPending.clear();
    }
}

void SceneView3D::destroyAdtTerrainTiles()
{
    if (m_adtTerrainTiles.empty()) return;
    bool const haveContext = (QOpenGLContext::currentContext() == context());
    bool const widgetContext = context() != nullptr;
    if (!haveContext && widgetContext) makeCurrent();
    for (AdtTileRender& tile : m_adtTerrainTiles)
    {
        for (AdtChunkRender& ch : tile.chunks)
        {
            if (ch.vbo && ch.vbo->isCreated()) ch.vbo->destroy();
            if (ch.ebo && ch.ebo->isCreated()) ch.ebo->destroy();
            if (ch.vao && ch.vao->isCreated()) ch.vao->destroy();
            // STAGE B: alphaArray + heightArray are OWNED; layerTex[] are
            // BORROWED cache handles -- zero them, never delete.
            if (ch.alphaArray  != 0) glDeleteTextures(1, &ch.alphaArray);
            if (ch.heightArray != 0) glDeleteTextures(1, &ch.heightArray);
            ch.alphaArray = 0;
            ch.heightArray = 0;
            for (int i = 0; i < 8; ++i) ch.layerTex[i] = 0;
        }
    }
    if (!haveContext && widgetContext) doneCurrent();
    m_adtTerrainTiles.clear();
    // Async scan + in-flight set are scoped to the current (mapId, mesh).
    // Wiping the tile set means a stale background payload would land on
    // an unrelated map; clear those bookkeeping fields too.  Reset the
    // streaming camera-tile sentinel so the next frame re-scans from scratch.
    m_adtScanDispatched.store(false);
    m_streamCamGx = -100000;
    m_streamCamGy = -100000;
    m_adtInFlight.clear();
    {
        std::lock_guard<std::mutex> g(m_adtPendingMutex);
        m_adtPending.clear();
    }
}

void SceneView3D::rebuildAdtTerrainTiles()
{
    destroyAdtTerrainTiles();
    if (!m_mesh.ok() || !m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
        return;
    if (!m_terrainTextureCache)
        m_terrainTextureCache = std::make_unique<TerrainTextureCache>(m_cascClient);

    // Same tile enumeration as the minimap-projection pipeline so we
    // can mark exactly the same set as ADT-covered.
    constexpr int   CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;
    std::vector<std::pair<int, int>> tiles;
    dtNavMesh const* nm = m_mesh.navmesh();
    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* mt = nm->getTile(ti);
        if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
        float const minX = mt->header->bmin[2];
        float const minY = mt->header->bmin[0];
        int const gx = int(std::floor(CENTER_GRID_ID - minX / TILE_SIZE));
        int const gy = int(std::floor(CENTER_GRID_ID - minY / TILE_SIZE));
        tiles.emplace_back(gx, gy);
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    m_adtTerrainTiles.reserve(tiles.size());
    for (auto const& [gx, gy] : tiles)
    {
        AdtTileRender tile;
        tile.gx = gx;
        tile.gy = gy;
        tile.loadAttempted = true;
        // Per-tile try; missing data is silent so the fallback pipeline
        // picks up the slack.
        tile.loaded = loadAndUploadAdtTile(gx, gy, tile);
        m_adtTerrainTiles.push_back(std::move(tile));
    }
}

bool SceneView3D::buildAdtChunkTextures(int layerCount,
                                        uint32_t const layerFdid[8],
                                        std::string const layerPath[8],
                                        std::vector<uint8_t>& alphaPlanes,
                                        bool hasAlpha,
                                        float const layerScale[8],
                                        float const heightScale[8],
                                        float const heightOffset[8],
                                        bool heightBlend,
                                        AdtChunkRender& ch)
{
    int const nLayers = (std::min<int>)(layerCount, 8);
    ch.layerCount = nLayers;
    if (nLayers == 0)
        return false;

    // Resolve each layer's diffuse texture, de-duplicating into <=8 GPU
    // sampler slots.  A FileDataID (or path) reused across layers shares one
    // slot -- with <=8 layers this always fits the 8 diffuse units.
    GLuint slotTex[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint32_t slotKey[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int slotCount = 0;
    for (int i = 0; i < nLayers; ++i)
    {
        uint32_t const fdid = layerFdid[i];
        std::string const& path = layerPath[i];
        GLuint tex = 0;
        if (fdid != 0)
            tex = m_terrainTextureCache->textureForFileDataId(fdid, *this);
        if (tex == 0 && !path.empty())
            tex = m_terrainTextureCache->textureForPath(path, *this);
        ch.layerTex[i] = tex;

        if (tex == 0)
        {
            // Unresolved diffuse: zero this layer's alpha plane so it drops out
            // entirely (covers a MIDDLE hole, not only a trailing one).  Layer
            // index i>=1 maps to alpha slice i-1.
            if (i >= 1 && hasAlpha && alphaPlanes.size() >= size_t(i) * 4096)
                std::memset(alphaPlanes.data() + size_t(i - 1) * 4096, 0, 4096);
            ch.slotForLayer[i] = 0;  // harmless; alpha is zeroed so never sampled
            continue;
        }

        // Find an existing slot with the same key (dedup) else allocate one.
        uint32_t const key = fdid != 0
            ? fdid : (0x80000000u | uint32_t(tex));
        int slot = -1;
        for (int s = 0; s < slotCount; ++s)
            if (slotKey[s] == key) { slot = s; break; }
        if (slot < 0 && slotCount < 8)
        {
            slot = slotCount++;
            slotKey[slot] = key;
            slotTex[slot]  = tex;
        }
        if (slot < 0) slot = 0;  // overflow guard (cannot happen with <=8 layers)
        ch.slotForLayer[i] = slot;
    }
    ch.slotCount = slotCount;
    for (int s = 0; s < 8; ++s)
        ch.layerTex[s] = (s < slotCount) ? slotTex[s] : 0;

    // Layer 0 must have a base albedo or the chunk is not renderable.
    if (slotCount == 0 || ch.layerTex[ch.slotForLayer[0]] == 0)
        return false;

    // Copy per-layer parallax metadata.
    for (int i = 0; i < 8; ++i)
    {
        ch.layerScale[i]   = (layerScale   && i < nLayers) ? layerScale[i]   : 1.0f;
        ch.heightScale[i]  = (heightScale  && i < nLayers) ? heightScale[i]  : 0.0f;
        ch.heightOffset[i] = (heightOffset && i < nLayers) ? heightOffset[i] : 1.0f;
    }
    ch.heightBlend = heightBlend && nLayers > 1;

    // Build the OWNED R8 GL_TEXTURE_2D_ARRAY alpha (7 slices = layers 1..7).
    if (hasAlpha && alphaPlanes.size() >= size_t(64 * 64 * 7))
    {
        glGenTextures(1, &ch.alphaArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, ch.alphaArray);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, 64, 64, 7, 0,
                     GL_RED, GL_UNSIGNED_BYTE, nullptr);
        for (int s = 0; s < 7; ++s)
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, s, 64, 64, 1,
                            GL_RED, GL_UNSIGNED_BYTE,
                            alphaPlanes.data() + size_t(s) * 4096);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
    return true;
}

bool SceneView3D::loadAndUploadAdtTile(int gx, int gy, AdtTileRender& out)
{
    auto dirOpt = m_mapDb2->directoryFor(m_heightmapMapId);
    if (!dirOpt)
    {
        qDebug("[scene3d-adt] tile (gx,gy)=(%d,%d): loadAdtTex0 result=no mapDir for mapId=%u",
            gx, gy, m_heightmapMapId);
        return false;
    }

    uint32_t syncRootFdid = 0;
    uint32_t syncTex0Fdid = 0;
    if (io::Wdt const* wdt = ensureWdt())
    {
        io::WdtMaidEntry const& e = wdt->entryFor(gx, gy);
        syncRootFdid = e.rootADT;
        syncTex0Fdid = e.tex0ADT;
    }
    io::AdtTile adt;
    if (!io::loadAdtTile(*m_cascClient, *dirOpt, m_heightmapMapId, gx, gy, adt,
                         syncRootFdid, syncTex0Fdid))
    {
        qDebug("[scene3d-adt] tile (gx,gy)=(%d,%d): loadAdtTile returned false (mapDir=%s)",
            gx, gy, dirOpt->c_str());
        return false;
    }
    if (adt.chunks.empty())
    {
        qDebug("[scene3d-adt] tile (gx,gy)=(%d,%d): loadAdtTile ok but chunks=0",
            gx, gy);
        return false;
    }

    out.chunks.reserve(adt.chunks.size());
    bool anyRenderable = false;

    // Sample first chunk for diagnostic (mirrors the worker's log).
    {
        io::AdtChunk const& sample = adt.chunks.front();
        uint32_t fdids[4] = { 0, 0, 0, 0 };
        int alphaSizes[4] = { 0, 0, 0, 0 };
        int const lc = (std::min<int>)(int(sample.layers.size()), 4);
        for (int i = 0; i < lc; ++i)
        {
            fdids[i] = sample.layers[size_t(i)].textureFileDataId;
            alphaSizes[i] = int(sample.layers[size_t(i)].alpha.size());
        }
        qDebug("[scene3d-adt] tile (%d,%d) sync chunk0: layers=%d fdids=[%u %u %u %u] alphaSizes=[%d %d %d %d] hasMccv=%d",
            gx, gy, lc, fdids[0], fdids[1], fdids[2], fdids[3],
            alphaSizes[0], alphaSizes[1], alphaSizes[2], alphaSizes[3],
            sample.hasMccv ? 1 : 0);
    }

    int syncChunksWithTex0 = 0;
    int syncChunksWithAlpha = 0;
    int syncGpuTextures = 0;

    for (io::AdtChunk const& ach : adt.chunks)
    {
        AdtChunkRender ch;
        ch.layerCount = std::min<int>(int(ach.layers.size()), 8);
        if (ch.layerCount == 0)
        {
            out.chunks.push_back(std::move(ch));
            continue;
        }

        // STAGE B: pack the 7-plane R8 alpha (layers 1..7) + per-layer MTXP
        // metadata, then resolve diffuse + build the alpha array via the shared
        // helper (mirrors the async drainer; single source of truth).
        bool hasAnyAlpha = false;
        std::vector<uint8_t> alphaPlanes(64 * 64 * 7, 0);
        float layerScale[8]   = { 1,1,1,1,1,1,1,1 };
        float heightScale[8]  = { 0,0,0,0,0,0,0,0 };
        float heightOffset[8] = { 1,1,1,1,1,1,1,1 };
        bool  heightBlend     = false;
        uint32_t    layerFdid[8] = { 0,0,0,0,0,0,0,0 };
        std::string layerPath[8];
        for (int i = 0; i < ch.layerCount; ++i)
        {
            io::AdtLayer const& L = ach.layers[size_t(i)];
            layerScale[i]   = L.layerScale;
            heightScale[i]  = L.heightScale;
            heightOffset[i] = L.heightOffset;
            layerFdid[i]    = L.textureFileDataId;
            layerPath[i]    = L.textureBlpPath;
            if (L.heightScale != 0.0f) heightBlend = true;
            if (i >= 1 && L.alpha.size() >= 4096)
            {
                std::memcpy(alphaPlanes.data() + size_t(i - 1) * 4096,
                            L.alpha.data(), 4096);
                hasAnyAlpha = true;
            }
        }

        bool const ok = buildAdtChunkTextures(ch.layerCount, layerFdid, layerPath,
            alphaPlanes, hasAnyAlpha,
            layerScale, heightScale, heightOffset, heightBlend, ch);
        if (!ok)
        {
            out.chunks.push_back(std::move(ch));
            continue;
        }
        ++syncChunksWithTex0;
        syncGpuTextures += ch.slotCount;
        if (ch.alphaArray != 0) ++syncChunksWithAlpha;

        // Build chunk geometry: 9x9 outer V9 grid + holes-aware quad
        // emission.  Skip the (row, col) quad when its hole bit is set.
        constexpr float kChunkSize = 33.33333f;
        constexpr float kUnitSize  = kChunkSize / 8.0f;
        float const chunkMaxX = ach.minX + kChunkSize;
        float const chunkMaxY = ach.minY + kChunkSize;
        auto sample = [&](int y, int x) -> AdtTerrainVertex
        {
            AdtTerrainVertex v;
            v.x = chunkMaxX - float(y) * kUnitSize;
            v.y = chunkMaxY - float(x) * kUnitSize;
            v.z = ach.heights[size_t(y * 9 + x)];
            v.u = float(x) / 8.0f;
            v.v = float(y) / 8.0f;
            v.nx = ach.normals[size_t(y * 9 + x)][0];
            v.ny = ach.normals[size_t(y * 9 + x)][1];
            v.nz = ach.normals[size_t(y * 9 + x)][2];
            v.r = ach.mccv[size_t(y * 9 + x)][0];
            v.g = ach.mccv[size_t(y * 9 + x)][1];
            v.b = ach.mccv[size_t(y * 9 + x)][2];
            v.a = ach.mccv[size_t(y * 9 + x)][3];
            return v;
        };

        std::vector<AdtTerrainVertex> verts;
        verts.reserve(8 * 8 * 6);
        // One-shot per upload: log first emitted vertex so we can confirm
        // the position is sane (not NaN / off-screen) when no terrain
        // pixels appear despite the draw count being non-zero.
        static bool s_firstVertexLogged = false;
        for (int row = 0; row < 8; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                if (ach.holesMask & (uint64_t(1) << (row * 8 + col)))
                    continue;
                AdtTerrainVertex const v0 = sample(row, col);
                if (!s_firstVertexLogged)
                {
                    s_firstVertexLogged = true;
                    qInfo("[scene3d-adt] first vertex sample: world=(%.1f, %.1f, %.1f) "
                          "uv=(%.3f, %.3f) n=(%.2f, %.2f, %.2f) rgba=(%u,%u,%u,%u)",
                        v0.x, v0.y, v0.z, v0.u, v0.v,
                        v0.nx, v0.ny, v0.nz,
                        unsigned(v0.r), unsigned(v0.g), unsigned(v0.b), unsigned(v0.a));
                }
                verts.push_back(v0);
                verts.push_back(sample(row + 1, col));
                verts.push_back(sample(row,     col + 1));
                verts.push_back(sample(row + 1, col));
                verts.push_back(sample(row + 1, col + 1));
                verts.push_back(sample(row,     col + 1));
            }
        }

        if (verts.empty())
        {
            // All-holes chunk; nothing to draw but record so the tile is
            // still considered ADT-covered (no fall-through artifacts).
            out.chunks.push_back(std::move(ch));
            anyRenderable = true;
            continue;
        }

        ch.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        ch.vao = std::make_unique<QOpenGLVertexArrayObject>();
        ch.vertexCount = static_cast<GLsizei>(verts.size());
        ch.vbo->create();
        ch.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        ch.vao->create();
        ch.vao->bind();
        ch.vbo->bind();
        ch.vbo->allocate(verts.data(), int(verts.size() * sizeof(AdtTerrainVertex)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AdtTerrainVertex),
            reinterpret_cast<void*>(offsetof(AdtTerrainVertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(AdtTerrainVertex),
            reinterpret_cast<void*>(offsetof(AdtTerrainVertex, u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(AdtTerrainVertex),
            reinterpret_cast<void*>(offsetof(AdtTerrainVertex, nx)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(AdtTerrainVertex),
            reinterpret_cast<void*>(offsetof(AdtTerrainVertex, r)));
        ch.vbo->release();
        ch.vao->release();

        anyRenderable = true;
        out.chunks.push_back(std::move(ch));
    }

    qDebug("[scene3d-adt] tile (%d,%d) sync GL upload: chunks=%zu texedChunks=%d alphaChunks=%d gpuTex=%d loaded=%d",
        gx, gy, adt.chunks.size(), syncChunksWithTex0, syncChunksWithAlpha,
        syncGpuTextures, anyRenderable ? 1 : 0);

    return anyRenderable;
}

void SceneView3D::destroyLitTerrainTiles()
{
    if (m_litTerrainTiles.empty()) return;
    bool const haveContext = (QOpenGLContext::currentContext() == context());
    bool const widgetContext = context() != nullptr;
    if (!haveContext && widgetContext) makeCurrent();
    for (LitTerrainTile& t : m_litTerrainTiles)
    {
        if (t.vbo && t.vbo->isCreated()) t.vbo->destroy();
        if (t.vao && t.vao->isCreated()) t.vao->destroy();
        // Texture handles live in m_minimapTextures; don't delete here.
        t.texture = 0;
    }
    if (!haveContext && widgetContext) doneCurrent();
    m_litTerrainTiles.clear();
}

GLuint SceneView3D::loadOrUploadMinimapTile(int gx, int gy)
{
    uint32_t const key = (uint32_t(gy) << 16) | (uint32_t(gx) & 0xFFFFu);
    if (auto it = m_minimapTextures.find(key); it != m_minimapTextures.end())
        return it->second; // already attempted (0 if previously missing)

    QImage img;
    bool found = false;

    // 1. PNG on disk -- operator's manual extracts win.  Same naming
    //    conventions as NavMeshView::loadOrUploadMinimapTile.  WoW client
    //    minimap files are map<gy>_<gx>.<ext> (gy = east-west col / client
    //    x-index, gx = north-south row / client y-index); legacy fallback
    //    keeps the swapped order in case some extracts kept TC's ordering.
    if (!m_minimapDir.isEmpty())
    {
        auto makePng = [&](int a, int b)
        {
            QStringList paths;
            paths << QStringLiteral("%1/%2/map%3_%4.png").arg(m_minimapDir).arg(m_heightmapMapId).arg(a).arg(b);
            paths << QStringLiteral("%1/map%2/map%3_%4.png").arg(m_minimapDir).arg(m_heightmapMapId).arg(a).arg(b);
            paths << QStringLiteral("%1/%2/%3_%4.png").arg(m_minimapDir).arg(m_heightmapMapId).arg(a).arg(b);
            return paths;
        };
        QStringList candidates = makePng(gy, gx);   // canonical client order
        candidates += makePng(gx, gy);              // swapped legacy fallback
        for (QString const& candidate : candidates)
            if (img.load(candidate)) { found = true; break; }
    }

    // 2. CASC fallback -- BLP directly from a live client install.  Same
    //    primary/legacy ordering as the PNG path above.
    if (!found && m_cascClient && m_mapDb2 && m_cascClient->isOpen())
    {
        if (auto dir = m_mapDb2->directoryFor(m_heightmapMapId))
        {
            auto makeBlp = [&](int a, int b)
            {
                std::string p = "world/minimaps/" + *dir;
                p += "/map";
                p += std::to_string(a);
                p += "_";
                p += std::to_string(b);
                p += ".blp";
                return p;
            };
            std::string const vpathPrimary = makeBlp(gy, gx);   // canonical client order
            std::string const vpathLegacy  = makeBlp(gx, gy);   // swapped fallback
            auto tryLoad = [&](std::string const& path) -> bool
            {
                std::vector<uint8_t> blob;
                if (!m_cascClient->readByPath(path, blob)) return false;
                io::BlpImage decoded;
                if (!io::decodeBlp(blob, decoded) || decoded.width <= 0 || decoded.height <= 0) return false;
                img = QImage(decoded.rgba.data(), decoded.width, decoded.height,
                             decoded.width * 4, QImage::Format_RGBA8888).copy();
                return true;
            };
            if (tryLoad(vpathPrimary)) found = true;
            else if (tryLoad(vpathLegacy)) found = true;
        }
    }

    if (!found || img.isNull())
    {
        m_minimapTextures[key] = 0;
        return 0;
    }
    // Capture the BLP decoder's original pixel format BEFORE conversion
    // so the first-tile diagnostic below can tell the operator which
    // format the decoder produced.  See NavMeshView::loadOrUploadMinimapTile
    // for the full rationale -- the manual reinterpret_cast<QRgb const*>
    // pixel loop used to live here, and it silently reads garbage for any
    // non-ARGB32/RGB32 QImage (e.g. Format_RGBA8888, Format_Indexed8,
    // Format_RGB888), producing the corrupted brown/blue stripe blobs.
    // Normalise to Format_ARGB32, then use Qt's affine QTransform path so
    // the transpose is correct regardless of what the decoder emits.
    QImage::Format const originalFormat = img.format();
    if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_RGB32)
        img = img.convertToFormat(QImage::Format_ARGB32);

    // Per-tile transform.  Used to be a hardcoded transpose; now the
    // MinimapDiagnosticsDock drives the choice on the 2D viewer and we
    // mirror it here via the static getter so both viewers stay in
    // visual lockstep.  The 2D viewer's setMinimapTransform also flushes
    // its own texture cache when the operator picks a new transform; the
    // 3D viewer rebuilds its cache lazily on the next map open (no
    // separate setter to keep cross-viewer plumbing small).
    img = NavMeshView::applyMinimapTransform(img, NavMeshView::currentMinimapTransform());

    // glTexImage2D below uploads GL_RGBA / GL_UNSIGNED_BYTE.  After the
    // ARGB32 normalisation + transpose, convert to RGBA8888 so the GL
    // byte order is correct on little-endian hosts (ARGB32 is BGRA in
    // memory).
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);

    // First-tile-per-map-switch diagnostic: emits exactly once per
    // process so the operator can verify the BLP decoder's format in
    // their build.  A function-local static is enough here because the
    // 3D scene only loads one continent at a time and the operator
    // restarts the editor between continents.
    static bool s_loggedOnce = false;
    if (!s_loggedOnce)
    {
        s_loggedOnce = true;
        qDebug() << "[minimap] tile gx=" << gx << "gy=" << gy
                 << "fdid=" << 0u
                 << "originalFormat=" << static_cast<int>(originalFormat)
                 << "convertedTo=Format_ARGB32 transform="
                 << NavMeshView::minimapTransformName(NavMeshView::currentMinimapTransform());
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // GL polish (matches NavMeshView::loadOrUploadMinimapTile): sized
    // internal format GL_RGBA8, mipmap generation + LINEAR_MIPMAP_LINEAR.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 img.width(), img.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_minimapTextures[key] = tex;
    return tex;
}

void SceneView3D::rebuildLitTerrainTiles()
{
    destroyLitTerrainTiles();
    if (!m_mapCache || m_mapCache->mapsDir().empty() || !m_mesh.ok()) return;

    // Mirrors rebuildTerrainBuffer's tile enumeration; produces one VBO
    // per tile so we can bind a per-tile minimap texture.  Each vertex
    // carries UV computed from (world XY - tile origin) / tile size,
    // and a smooth-ish per-vertex normal from finite-difference of
    // adjacent heights so cliffs shade darker.
    constexpr int   GRID = 33;
    constexpr int   CELL = GRID - 1;
    constexpr int   CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;
    constexpr float STEP = TILE_SIZE / float(CELL);

    std::vector<std::pair<int, int>> tiles;
    dtNavMesh const* nm = m_mesh.navmesh();
    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* mt = nm->getTile(ti);
        if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
        float const minX = mt->header->bmin[2];
        float const minY = mt->header->bmin[0];
        int const gx = int(std::floor(CENTER_GRID_ID - minX / TILE_SIZE));
        int const gy = int(std::floor(CENTER_GRID_ID - minY / TILE_SIZE));
        tiles.emplace_back(gx, gy);
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    m_litTerrainTiles.reserve(tiles.size());
    for (auto const& [gx, gy] : tiles)
    {
        float const worldMaxX = (CENTER_GRID_ID - gx) * TILE_SIZE;
        float const worldMinX = worldMaxX - TILE_SIZE;
        float const worldMaxY = (CENTER_GRID_ID - gy) * TILE_SIZE;
        float const worldMinY = worldMaxY - TILE_SIZE;

        std::vector<float> samples(GRID * GRID);
        float hmin = 1e9f, hmax = -1e9f;
        for (int row = 0; row < GRID; ++row)
        {
            for (int col = 0; col < GRID; ++col)
            {
                float const wx = worldMaxX - float(row) * STEP;
                float const wy = worldMaxY - float(col) * STEP;
                float const h  = m_mapCache->heightAt(m_heightmapMapId, wx, wy);
                samples[row * GRID + col] = h;
                if (h > io::ADT_INVALID_HEIGHT)
                {
                    if (h < hmin) hmin = h;
                    if (h > hmax) hmax = h;
                }
            }
        }
        if (hmax <= hmin) { hmin = 0.0f; hmax = 1.0f; }
        float const range = hmax - hmin;

        // Sanitised height for normal + position math (NaN/sentinel ->
        // clamp to tile minimum so the surface stays continuous).
        auto sampleSan = [&](int row, int col) -> float
        {
            float h = samples[row * GRID + col];
            if (h <= io::ADT_INVALID_HEIGHT) h = hmin;
            return h;
        };

        auto computeNormal = [&](int row, int col) -> std::array<float, 3>
        {
            // Central-difference using neighbour heights along TC -X
            // (row+) and -Y (col+).  Normal = cross(d/drow, d/dcol).
            int const rL = std::max(0, row - 1);
            int const rR = std::min(GRID - 1, row + 1);
            int const cL = std::max(0, col - 1);
            int const cR = std::min(GRID - 1, col + 1);
            float const hL = sampleSan(rL, col);
            float const hR = sampleSan(rR, col);
            float const hD = sampleSan(row, cL);
            float const hU = sampleSan(row, cR);
            float const dRow = float(rR - rL) * STEP;
            float const dCol = float(cR - cL) * STEP;
            // Row axis is TC -X; col axis is TC -Y.  Tangent_row =
            // (-dRow, 0, hR-hL); tangent_col = (0, -dCol, hU-hD).
            float const tRx = -dRow, tRy = 0.0f,  tRz = hR - hL;
            float const tCx = 0.0f,  tCy = -dCol, tCz = hU - hD;
            // cross(tR, tC).
            float nx = tRy * tCz - tRz * tCy;
            float ny = tRz * tCx - tRx * tCz;
            float nz = tRx * tCy - tRy * tCx;
            // Force +Z hemisphere; navmesh terrain is up-facing.
            if (nz < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
            float const len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
            else             { nx = 0.0f; ny = 0.0f; nz = 1.0f; }
            return { nx, ny, nz };
        };

        std::vector<LitTerrainVertex> verts;
        verts.reserve(CELL * CELL * 6);

        auto pushVert = [&](int row, int col)
        {
            float const wx = worldMaxX - float(row) * STEP;
            float const wy = worldMaxY - float(col) * STEP;
            float const h  = sampleSan(row, col);
            // UV: project the minimap so the +X (north) end of the tile
            // is at v=0 / +Y (west) end at u=0.  This matches the BLP
            // tile layout used by the 2D minimap pass.
            float const u = float(col) / float(CELL);
            float const v = float(row) / float(CELL);
            auto const n = computeNormal(row, col);
            // Fallback greyscale colour (used when no texture loads).
            uint8_t r, g, b;
            if (h < 0.0f)
            {
                float const t = std::min(1.0f, -h / 60.0f);
                r = uint8_t(40 + (1.0f - t) * 20);
                g = uint8_t(60 + (1.0f - t) * 30);
                b = uint8_t(120 + t * 80);
            }
            else
            {
                float const t = std::clamp((h - hmin) / range, 0.0f, 1.0f);
                uint8_t const vv = uint8_t(70 + t * 150);
                r = vv;
                g = uint8_t(std::min(255, int(vv) + int(t * 18)));
                b = uint8_t(int(vv) - int(t * 20));
            }
            verts.push_back({ wx, wy, h, u, v, n[0], n[1], n[2], r, g, b, 255 });
        };

        for (int row = 0; row < CELL; ++row)
        {
            for (int col = 0; col < CELL; ++col)
            {
                pushVert(row,     col);
                pushVert(row + 1, col);
                pushVert(row,     col + 1);
                pushVert(row + 1, col);
                pushVert(row + 1, col + 1);
                pushVert(row,     col + 1);
            }
        }

        LitTerrainTile tile;
        tile.gx   = gx;
        tile.gy   = gy;
        tile.minX = worldMinX; tile.maxX = worldMaxX;
        tile.minY = worldMinY; tile.maxY = worldMaxY;
        tile.minZ = hmin;      tile.maxZ = hmax;     // real per-tile Z for cull
        tile.vbo  = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        tile.vao  = std::make_unique<QOpenGLVertexArrayObject>();
        tile.vertexCount = static_cast<GLsizei>(verts.size());
        tile.vbo->create();
        tile.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        tile.vao->create();
        tile.vao->bind();
        tile.vbo->bind();
        tile.vbo->allocate(verts.data(), int(verts.size() * sizeof(LitTerrainVertex)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LitTerrainVertex),
            reinterpret_cast<void*>(offsetof(LitTerrainVertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(LitTerrainVertex),
            reinterpret_cast<void*>(offsetof(LitTerrainVertex, u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(LitTerrainVertex),
            reinterpret_cast<void*>(offsetof(LitTerrainVertex, nx)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LitTerrainVertex),
            reinterpret_cast<void*>(offsetof(LitTerrainVertex, r)));
        tile.vbo->release();
        tile.vao->release();
        m_litTerrainTiles.push_back(std::move(tile));
    }
}

void SceneView3D::rebuildLitWmoBuffer()
{
    m_litWmoDirty = true;
    if (m_buffersReady) { uploadLitWmoGeometry(); update(); }
}

void SceneView3D::uploadLitWmoGeometry()
{
    m_litWmoDirty = false;
    auto const& tris = m_wmoMesh.triangles();
    std::vector<LitWmoVertex> verts;
    verts.reserve(tris.size() * 3);
    // Deterministic per-group tint: quantise the centroid to 12-yard
    // buckets and hash the (kind, bucketX, bucketY) triple.  Adjacent
    // walls of the same building cluster into the same bucket and
    // share a tint; distinct buildings get distinct colours.  This is
    // the closest we can get to "WMO groups read distinctly" without
    // ADT/WMO group-id metadata (deliberately out of scope).
    auto bucketHash = [](int kind, int bx, int by) -> uint32_t
    {
        uint32_t h = uint32_t(kind) * 0x9E3779B1u
                   ^ uint32_t(bx)   * 0x85EBCA77u
                   ^ uint32_t(by)   * 0xC2B2AE3Du;
        h ^= h >> 16; h *= 0x7FEB352Du;
        h ^= h >> 15; h *= 0x846CA68Bu;
        h ^= h >> 16;
        return h;
    };
    for (auto const& t : tris)
    {
        // Per-face flat normal: cross of two edges.  Force +Z hemisphere
        // for ceiling/floor faces so the sun term doesn't cancel out;
        // walls keep their natural orientation either way.
        float const ex1 = t.v[1][0] - t.v[0][0];
        float const ey1 = t.v[1][1] - t.v[0][1];
        float const ez1 = t.v[1][2] - t.v[0][2];
        float const ex2 = t.v[2][0] - t.v[0][0];
        float const ey2 = t.v[2][1] - t.v[0][1];
        float const ez2 = t.v[2][2] - t.v[0][2];
        float nx = ey1 * ez2 - ez1 * ey2;
        float ny = ez1 * ex2 - ex1 * ez2;
        float nz = ex1 * ey2 - ey1 * ex2;
        float const len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
        else             { nx = 0.0f; ny = 0.0f; nz = 1.0f; }

        // Centroid for the per-group hash bucket.
        float const cx = (t.v[0][0] + t.v[1][0] + t.v[2][0]) * (1.0f / 3.0f);
        float const cy = (t.v[0][1] + t.v[1][1] + t.v[2][1]) * (1.0f / 3.0f);
        int const bx = int(std::floor(cx / 12.0f));
        int const by = int(std::floor(cy / 12.0f));
        int const kindInt = (t.kind == io::VmapSpawnKind::M2) ? 1 : 0;
        uint32_t const h = bucketHash(kindInt, bx, by);
        // Walls / buildings: cool palette anchored on 175-200 grey-blue
        // so the lit pass doesn't blow out highlights.  M2 (props) get
        // a slightly warmer base so they read distinct from buildings.
        uint8_t r, g, b;
        if (t.kind == io::VmapSpawnKind::M2)
        {
            r = uint8_t(150 + ((h >> 0)  & 0x3F)); // 150..213
            g = uint8_t(125 + ((h >> 8)  & 0x3F)); // 125..188
            b = uint8_t(100 + ((h >> 16) & 0x3F)); // 100..163
        }
        else
        {
            r = uint8_t(140 + ((h >> 0)  & 0x4F));
            g = uint8_t(140 + ((h >> 8)  & 0x4F));
            b = uint8_t(160 + ((h >> 16) & 0x4F));
        }
        constexpr uint8_t alpha = 255;
        for (int v = 0; v < 3; ++v)
        {
            LitWmoVertex lv;
            lv.x = t.v[v][0]; lv.y = t.v[v][1]; lv.z = t.v[v][2];
            lv.nx = nx; lv.ny = ny; lv.nz = nz;
            lv.r = r; lv.g = g; lv.b = b; lv.a = alpha;
            verts.push_back(lv);
        }
    }
    m_litWmoVertexCount = static_cast<GLsizei>(verts.size());
    m_litWmoVao.bind();
    m_litWmoVbo.bind();
    if (!verts.empty())
        m_litWmoVbo.allocate(verts.data(), int(verts.size() * sizeof(LitWmoVertex)));
    else
        m_litWmoVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LitWmoVertex),
        reinterpret_cast<void*>(offsetof(LitWmoVertex, x)));
    // UV slot unused for WMO -- still bind something so attribute 1 is
    // valid in the shared shader (sampling is gated by u_hasTexture).
    glDisableVertexAttribArray(1);
    glVertexAttrib2f(1, 0.0f, 0.0f);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(LitWmoVertex),
        reinterpret_cast<void*>(offsetof(LitWmoVertex, nx)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LitWmoVertex),
        reinterpret_cast<void*>(offsetof(LitWmoVertex, r)));
    m_litWmoVbo.release();
    m_litWmoVao.release();
}

void SceneView3D::uploadWmoGeometry()
{
    m_wmoDirty = false;
    auto const& tris = m_wmoMesh.triangles();
    // Pack 3 NavVertex per triangle.  Two-tone by spawn kind so the
    // operator can tell buildings (WMO -- cool gray) from doodads
    // (M2 -- warmer earthy tone) at a glance.  Both are translucent so
    // the terrain underneath still reads through.
    constexpr uint8_t kWmoR = 170, kWmoG = 170, kWmoB = 185, kWmoA = 110;
    constexpr uint8_t kM2R  = 180, kM2G  = 145, kM2B  = 110, kM2A  = 130;
    std::vector<NavVertex> verts;
    verts.reserve(tris.size() * 3);
    for (auto const& t : tris)
    {
        bool const isM2 = (t.kind == io::VmapSpawnKind::M2);
        uint8_t const r = isM2 ? kM2R : kWmoR;
        uint8_t const g = isM2 ? kM2G : kWmoG;
        uint8_t const b = isM2 ? kM2B : kWmoB;
        uint8_t const a = isM2 ? kM2A : kWmoA;
        for (int v = 0; v < 3; ++v)
        {
            NavVertex nv;
            nv.x = t.v[v][0];
            nv.y = t.v[v][1];
            nv.z = t.v[v][2];
            nv.r = r; nv.g = g; nv.b = b; nv.a = a;
            verts.push_back(nv);
        }
    }
    m_wmoVertexCount = static_cast<GLsizei>(verts.size());
    m_wmoVao.bind();
    m_wmoVbo.bind();
    if (!verts.empty())
        m_wmoVbo.allocate(verts.data(), int(verts.size() * sizeof(NavVertex)));
    else
        m_wmoVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(NavVertex),
        reinterpret_cast<void*>(offsetof(NavVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(NavVertex),
        reinterpret_cast<void*>(offsetof(NavVertex, r)));
    m_wmoVbo.release();
    m_wmoVao.release();
}

// ============================================================================
// Doodad (M2 prop) pass.
// ============================================================================

void SceneView3D::destroyDoodadResources()
{
    // ADDITIVE residency reset (always runs alongside destroyTexturedWmoResources,
    // which clears m_wmoInstances): drop the loaded-tile + emitted-uid sets and
    // the stream-cam sentinel so a fresh map re-streams from scratch.  Must run
    // BEFORE the early-return below, or a doodad-less map would keep stale tiles
    // marked loaded while its WMO instances were cleared -> buildings vanish.
    m_loadedPropTiles.clear();
    m_emittedWmoUids.clear();
    m_doodadStreamCamGx = -100000;
    m_doodadStreamCamGy = -100000;

    if (m_doodadMeshes.empty() && m_doodadInstances.empty())
    {
        m_doodadsBuilt = false;
        return;
    }
    bool const haveContext = (QOpenGLContext::currentContext() == context());
    bool const widgetContext = context() != nullptr;
    if (!haveContext && widgetContext) makeCurrent();
    for (auto& [fdid, mesh] : m_doodadMeshes)
    {
        if (mesh.vbo && mesh.vbo->isCreated()) mesh.vbo->destroy();
        if (mesh.ibo && mesh.ibo->isCreated()) mesh.ibo->destroy();
        if (mesh.vao && mesh.vao->isCreated()) mesh.vao->destroy();
        // Textures live in m_terrainTextureCache (shared with ADT layer
        // textures); don't double-free here.
    }
    if (!haveContext && widgetContext) doneCurrent();
    m_doodadMeshes.clear();
    m_doodadInstances.clear();
    m_doodadsBuilt = false;
    // STAGE B1: drop any in-flight / pending async loads so a worker that
    // finishes after this teardown does not resurrect a mesh into the cleared
    // cache (uploadDoodadPayload would re-create the entry on a stale FDID).
    m_doodadInFlight.clear();
    {
        std::lock_guard<std::mutex> g(m_doodadPendingMutex);
        m_doodadPending.clear();
    }
}

void SceneView3D::rebuildDoodadInstances()
{
    // ADDITIVE (Phase 1): do NOT clear -- appending only un-loaded near tiles
    // keeps already-materialised props resident, so flying past a tile doesn't
    // re-load + flicker it (the clear-and-rebuild-every-cross churn).  Cleared
    // only on map switch (destroyDoodadResources).
    // Mark both layers built so we don't retry per paint when ADTs have no
    // doodads / WMOs.  The single tile walk below populates both lists (the
    // MODF read the doodad path already performs also yields the textured
    // WMO root instances), so one rebuild covers both passes.
    m_doodadsBuilt      = true;
    m_texturedWmosBuilt = true;

    if (!m_mesh.ok() || !m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
        return;
    auto dirOpt = m_mapDb2->directoryFor(m_heightmapMapId);
    if (!dirOpt) return;

    // Tile enumeration mirrors rebuildAdtTerrainTiles -- same tile set,
    // same coordinate convention.
    constexpr int   CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;
    // Camera-radius gate: only materialise props near the camera, not the
    // whole continent (488K instances + 3.1M WMO verts tanked the framerate).
    // Record the camera tile so paintGL re-streams on the next tile-cross.
    m_doodadStreamCamGx = int(std::floor(CENTER_GRID_ID - m_camX / TILE_SIZE));
    m_doodadStreamCamGy = int(std::floor(CENTER_GRID_ID - m_camY / TILE_SIZE));
    float const propRadius   = std::min(renderRadiusYards(), 2400.0f);  // fog-capped
    float const propRadiusSq = propRadius * propRadius;
    std::vector<std::pair<int, int>> tiles;
    dtNavMesh const* nm = m_mesh.navmesh();
    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* mt = nm->getTile(ti);
        if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
        float const minX = mt->header->bmin[2];
        float const minY = mt->header->bmin[0];
        int const gx = int(std::floor(CENTER_GRID_ID - minX / TILE_SIZE));
        int const gy = int(std::floor(CENTER_GRID_ID - minY / TILE_SIZE));
        tiles.emplace_back(gx, gy);
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    // Dedup table: a placed WMO may straddle several ADT tiles (so its
    // MODF entry recurs in each neighbouring obj0).  The MODF.uniqueId is
    // stable across tiles -- skip a placement we've already materialised
    // to avoid drawing the same set of interior doodads N times.
    // Persistent straddler dedup (a WMO spanning tiles recurs in each neighbour's
    // obj0): a member set so additive loads across tile-crosses don't re-emit it.
    auto& wmoEmitted = m_emittedWmoUids;     // uniqueId set.

    // Cache the WDT once so every tile's MAID lookup is O(1).
    io::Wdt const* wdt = ensureWdt();

    for (auto const& [gx, gy] : tiles)
    {
        // Distance gate: skip tiles whose centre is beyond the prop radius of
        // the camera.  This is what bounds the prop count (was: whole map).
        float const tileCx = (CENTER_GRID_ID - gx) * TILE_SIZE - 0.5f * TILE_SIZE;
        float const tileCy = (CENTER_GRID_ID - gy) * TILE_SIZE - 0.5f * TILE_SIZE;
        float const ddx = tileCx - m_camX, ddy = tileCy - m_camY;
        if (ddx * ddx + ddy * ddy > propRadiusSq)
            continue;

        // ADDITIVE: skip tiles already materialised; only NEW near tiles are
        // enumerated + appended.  This is what stops the per-tile-cross churn.
        uint64_t const tileKey = (uint64_t(uint32_t(gx)) << 32) | uint32_t(gy);
        if (!m_loadedPropTiles.insert(tileKey).second)
            continue;

        uint32_t obj0Fdid = 0;
        uint32_t rootFdid = 0;
        if (wdt)
        {
            io::WdtMaidEntry const& e = wdt->entryFor(gx, gy);
            obj0Fdid = e.obj0ADT;
            rootFdid = e.rootADT;
        }
        std::vector<io::DoodadInstance> tileDoodads;
        if (!io::loadAdtDoodads(*m_cascClient, *dirOpt, m_heightmapMapId,
                                gx, gy, tileDoodads, obj0Fdid, rootFdid))
            continue;
        m_doodadInstances.reserve(m_doodadInstances.size() + tileDoodads.size());
        for (io::DoodadInstance const& d : tileDoodads)
        {
            if (d.modelFileDataId == 0)
                continue;     // legacy filename-only entries: skipped in v1 (no path -> FDID listfile here).
            DoodadInstanceGpu gpu;
            gpu.modelFdid = d.modelFileDataId;
            gpu.x = d.x;  gpu.y = d.y;  gpu.z = d.z;
            gpu.rotZ = d.rotZ;  gpu.rotY = d.rotY;  gpu.rotX = d.rotX;
            gpu.scale = d.scale;
            m_doodadInstances.push_back(gpu);
        }

        // WMO interior doodads (MODD/MODS/MODN/MODI inside each placed
        // WMO referenced via this ADT's MODF).  Each placement contributes
        // its set 0 + per-placement DoodadSet, transformed to world space
        // by the WMO root reader.  Legacy entries with no FDID are
        // skipped (no path->FDID listfile in the editor; matches the
        // legacy-MDDF skip above).
        std::vector<io::WmoPlacementInstance> wmoPlacements;
        if (!io::loadAdtWmoPlacements(*m_cascClient, *dirOpt, m_heightmapMapId,
                                      gx, gy, wmoPlacements, obj0Fdid, rootFdid))
            continue;
        for (io::WmoPlacementInstance const& wp : wmoPlacements)
        {
            if (wp.wmoRootFileDataId == 0)
                continue;
            if (wp.uniqueId != 0 && wmoEmitted.find(wp.uniqueId) != wmoEmitted.end())
                continue;
            wmoEmitted.insert(wp.uniqueId);

            // Textured WMO ROOT geometry instance (walls / floors / etc).
            // Same uniqueId dedup as the interior doodads below, and the
            // identical client->editor placement math (kClientMid origin +
            // S-conjugated rotation) so root + props align.  The actual
            // group geometry streams in asynchronously (drawTexturedWmos
            // queues conservatively-visible roots via dispatchWmoLoad, STAGE B1).
            {
                io::WmoRootPlacement const rp = io::computeWmoRootPlacement(wp);
                WmoInstanceGpu gi;
                gi.wmoRootFdid = wp.wmoRootFileDataId;
                gi.x = rp.x;  gi.y = rp.y;  gi.z = rp.z;
                gi.rotZ = rp.rotZ;  gi.rotY = rp.rotY;  gi.rotX = rp.rotX;
                gi.scale = rp.scale;
                m_wmoInstances.push_back(gi);
            }

            io::WmoPlacement placement;
            placement.positionXYZ[0] = wp.posXYZ[0];
            placement.positionXYZ[1] = wp.posXYZ[1];
            placement.positionXYZ[2] = wp.posXYZ[2];
            placement.rotationDegXYZ[0] = wp.rotDegXYZ[0];
            placement.rotationDegXYZ[1] = wp.rotDegXYZ[1];
            placement.rotationDegXYZ[2] = wp.rotDegXYZ[2];
            placement.scale = wp.scale;

            std::vector<io::DoodadInstance> wmoDoodads;
            if (!io::loadWmoDoodads(*m_cascClient, wp.wmoRootFileDataId, wp.doodadSet, placement, wmoDoodads))
                continue;
            m_doodadInstances.reserve(m_doodadInstances.size() + wmoDoodads.size());
            for (io::DoodadInstance const& d : wmoDoodads)
            {
                if (d.modelFileDataId == 0)
                    continue;
                DoodadInstanceGpu gpu;
                gpu.modelFdid = d.modelFileDataId;
                gpu.x = d.x;  gpu.y = d.y;  gpu.z = d.z;
                gpu.rotZ = d.rotZ;  gpu.rotY = d.rotY;  gpu.rotX = d.rotX;
                gpu.scale = d.scale;
                m_doodadInstances.push_back(gpu);
            }
        }
    }
}

// STAGE B1: the draw path no longer calls this -- M2 meshes stream in via
// dispatchDoodadLoad (worker) + uploadDoodadPayload (GL drain).  Retained as the
// synchronous reference / fallback (CASC read + GL upload in one call) so the
// load logic has a single readable definition.
bool SceneView3D::ensureDoodadMeshLoaded(uint32_t fdid)
{
    auto it = m_doodadMeshes.find(fdid);
    if (it != m_doodadMeshes.end())
        return it->second.loaded;

    DoodadGpuMesh& gpu = m_doodadMeshes[fdid];
    gpu.loadAttempted = true;

    io::M2Mesh mesh;
    if (!io::loadM2(*m_cascClient, fdid, mesh))
        return false;
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.subMeshes.empty())
        return false;

    // Bounding sphere from the local-space AABB.  Frustum culling uses
    // this radius * instance.scale.
    float const cx = 0.5f * (mesh.bboxMinX + mesh.bboxMaxX);
    float const cy = 0.5f * (mesh.bboxMinY + mesh.bboxMaxY);
    float const cz = 0.5f * (mesh.bboxMinZ + mesh.bboxMaxZ);
    float const dx = 0.5f * (mesh.bboxMaxX - mesh.bboxMinX);
    float const dy = 0.5f * (mesh.bboxMaxY - mesh.bboxMinY);
    float const dz = 0.5f * (mesh.bboxMaxZ - mesh.bboxMinZ);
    gpu.centerX = cx; gpu.centerY = cy; gpu.centerZ = cz;
    gpu.radius = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (gpu.radius < 0.5f) gpu.radius = 0.5f;

    gpu.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    gpu.ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    gpu.vao = std::make_unique<QOpenGLVertexArrayObject>();
    gpu.vbo->create();
    gpu.ibo->create();
    gpu.vao->create();

    gpu.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    gpu.ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);

    gpu.vao->bind();
    gpu.vbo->bind();
    gpu.vbo->allocate(mesh.vertices.data(), int(mesh.vertices.size() * sizeof(float)));
    gpu.ibo->bind();
    gpu.ibo->allocate(mesh.indices.data(), int(mesh.indices.size() * sizeof(uint32_t)));
    constexpr int kStride = int(sizeof(float)) * 10;  // STAGE A: +uv1.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(sizeof(float) * 6));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(sizeof(float) * 8));
    gpu.vao->release();
    gpu.vbo->release();
    gpu.ibo->release();

    gpu.vertexCount = GLsizei(mesh.vertices.size() / 10);
    gpu.indexCount  = GLsizei(mesh.indices.size());

    gpu.subMeshes.reserve(mesh.subMeshes.size());
    for (io::M2SubMesh const& sm : mesh.subMeshes)
    {
        DoodadGpuSubMesh d;
        d.indexStart = sm.indexStart;
        d.indexCount = sm.indexCount;
        d.blendMode  = sm.blendMode;
        d.combinerId = sm.combinerId;
        if (m_terrainTextureCache)
        {
            if (sm.textureFileDataId != 0)
                d.texture = m_terrainTextureCache->textureForFileDataId(sm.textureFileDataId, *this);
            if (d.texture == 0 && !sm.texturePath.empty())
                d.texture = m_terrainTextureCache->textureForPath(sm.texturePath, *this);
            if (sm.textureFileDataId2 != 0)
                d.texture2 = m_terrainTextureCache->textureForFileDataId(sm.textureFileDataId2, *this);
            if (d.texture2 == 0 && !sm.texturePath2.empty())
                d.texture2 = m_terrainTextureCache->textureForPath(sm.texturePath2, *this);
        }
        gpu.subMeshes.push_back(d);
    }
    gpu.loaded = true;
    return true;
}

void SceneView3D::drawDoodads(QMatrix4x4 const& mvp)
{
    if (!m_doodadProgram || m_doodadInstances.empty())
        return;

    // Extract 6 frustum planes from MVP (row * sign convention).  Used
    // to bounding-sphere reject instances cheaply before any GL state
    // change.  Planes stored as (a, b, c, d) with the convention that
    // a point (x,y,z) is inside when a*x + b*y + c*z + d >= 0.
    struct Plane { float a, b, c, d; };
    Plane planes[6];
    auto m = mvp;
    float const* p = m.constData(); // column-major, m[col*4 + row].
    auto get = [&](int row, int col) { return p[col * 4 + row]; };
    auto setPlane = [&](Plane& pl, int row, float sign)
    {
        pl.a = get(3, 0) + sign * get(row, 0);
        pl.b = get(3, 1) + sign * get(row, 1);
        pl.c = get(3, 2) + sign * get(row, 2);
        pl.d = get(3, 3) + sign * get(row, 3);
        float len = std::sqrt(pl.a * pl.a + pl.b * pl.b + pl.c * pl.c);
        if (len > 1e-6f) { pl.a /= len; pl.b /= len; pl.c /= len; pl.d /= len; }
    };
    setPlane(planes[0], 0,  1.0f); // left
    setPlane(planes[1], 0, -1.0f); // right
    setPlane(planes[2], 1,  1.0f); // bottom
    setPlane(planes[3], 1, -1.0f); // top
    setPlane(planes[4], 2,  1.0f); // near
    setPlane(planes[5], 2, -1.0f); // far

    auto sphereInFrustum = [&](float sx, float sy, float sz, float sr) -> bool
    {
        for (int i = 0; i < 6; ++i)
        {
            float const dist = planes[i].a * sx + planes[i].b * sy + planes[i].c * sz + planes[i].d;
            if (dist < -sr)
                return false;
        }
        return true;
    };

    m_doodadProgram->bind();
    m_doodadProgram->setUniformValue(m_doodadUMvp, mvp);
    applyFogAndSunUniforms(*m_doodadProgram);
    m_doodadProgram->setUniformValue(m_doodadUTexture, 0);
    m_doodadProgram->setUniformValue(m_doodadUTexture2, 1);  // STAGE A: second sampler -> unit 1.

    // Sort instances by mesh FDID so the GL bind churn drops to once
    // per unique model rather than once per instance.  std::stable_sort
    // keeps the per-tile order intact within each group -- helpful for
    // debugging overlapping placements.  Sorting also groups each FDID's
    // instances into a contiguous run for the conservative-visibility scan.
    std::stable_sort(m_doodadInstances.begin(), m_doodadInstances.end(),
        [](DoodadInstanceGpu const& a, DoodadInstanceGpu const& b)
        {
            return a.modelFdid < b.modelFdid;
        });

    // STAGE B1/C1: M2 meshes load asynchronously now.  Conservative per-instance
    // gate (world position + generous radius) decides whether to QUEUE the FDID;
    // M2 props read close (trees/rocks), so a ~500u draw ring (reference §1).
    constexpr float kM2ConservRadius = 60.0f;
    constexpr float kM2DrawRadius    = 1500.0f;   // generous vs reference 500u so editor fly-throughs keep context.
    float const m2RingR2 = kM2DrawRadius * kM2DrawRadius;
    auto m2InstanceInRange = [&](DoodadInstanceGpu const& in, float consR) -> bool
    {
        float const dx = in.x - m_camX, dy = in.y - m_camY, dz = in.z - m_camZ;
        if (dx * dx + dy * dy + dz * dz > m2RingR2) return false;
        return sphereInFrustum(in.x, in.y, in.z, consR);
    };

    int dbgUniqueModels = 0, dbgLoadOk = 0, dbgLoadFail = 0, dbgQueued = 0;
    int dbgInstCulled = 0, dbgInstDrawn = 0, dbgSubMeshDraws = 0;
    DoodadGpuMesh* boundMesh = nullptr;
    std::size_t const nInst = m_doodadInstances.size();
    for (std::size_t i = 0; i < nInst; )
    {
        uint32_t const fdid = m_doodadInstances[i].modelFdid;
        std::size_t runEnd = i + 1;
        while (runEnd < nInst && m_doodadInstances[runEnd].modelFdid == fdid)
            ++runEnd;
        ++dbgUniqueModels;

        auto it = m_doodadMeshes.find(fdid);
        bool const loaded    = (it != m_doodadMeshes.end() && it->second.loaded);
        bool const attempted = (it != m_doodadMeshes.end());
        if (!loaded)
        {
            if (!attempted)
            {
                for (std::size_t k = i; k < runEnd; ++k)
                {
                    if (m2InstanceInRange(m_doodadInstances[k], kM2ConservRadius))
                    {
                        dispatchDoodadLoad(fdid);
                        ++dbgQueued;
                        break;
                    }
                }
            }
            else
            {
                ++dbgLoadFail;
            }
            i = runEnd;
            continue;
        }
        ++dbgLoadOk;
        boundMesh = &it->second;
        boundMesh->vao->bind();

        for (std::size_t k = i; k < runEnd; ++k)
        {
            DoodadInstanceGpu const& inst = m_doodadInstances[k];
            // C1: per-instance distance ring (squared compare, no sqrt).
            float const ddx = inst.x - m_camX, ddy = inst.y - m_camY, ddz = inst.z - m_camZ;
            if (ddx * ddx + ddy * ddy + ddz * ddz > m2RingR2)
            {
                ++dbgInstCulled;
                continue;
            }
            // Per-instance world-space center for the frustum test.  The mesh's
            // local center rotates with the instance, so we move it through the
            // instance's translate-scale before the sphere-vs-plane check.
            float const sx = inst.x + inst.scale * boundMesh->centerX;
            float const sy = inst.y + inst.scale * boundMesh->centerY;
            float const sz = inst.z + inst.scale * boundMesh->centerZ;
            float const sr = boundMesh->radius * inst.scale;
            if (!sphereInFrustum(sx, sy, sz, sr))
            {
                ++dbgInstCulled;
                continue;
            }
            ++dbgInstDrawn;

            // World matrix: translate * Rz * Ry * Rx * scale.
            QMatrix4x4 model;
            model.translate(inst.x, inst.y, inst.z);
            model.rotate(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, inst.rotZ * 180.0f / 3.14159265f));
            model.rotate(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, inst.rotY * 180.0f / 3.14159265f));
            model.rotate(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, inst.rotX * 180.0f / 3.14159265f));
            model.scale(inst.scale);
            m_doodadProgram->setUniformValue(m_doodadUModel, model);

            for (DoodadGpuSubMesh const& sm : boundMesh->subMeshes)
            {
                if (sm.indexCount == 0) continue;
                m_doodadProgram->setUniformValue(m_doodadUHasTex,  sm.texture  != 0 ? 1 : 0);
                m_doodadProgram->setUniformValue(m_doodadUHasTex2, sm.texture2 != 0 ? 1 : 0);
                m_doodadProgram->setUniformValue(m_doodadUCombinerId, int(sm.combinerId));
                float const alphaCutoff = (sm.blendMode == 1) ? 0.5f : 0.0f;
                m_doodadProgram->setUniformValue(m_doodadUAlphaCutoff, alphaCutoff);
                // Bind a real handle to unit 1 only when this submesh has a
                // second texture; otherwise u_hasTexture2 == 0 neutralizes any
                // stale unit-1 state (the shader's white default), so the
                // sampler is never read.  Leave unit 0 active to match the
                // existing per-instance teardown.
                if (sm.texture2 != 0)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, sm.texture2);
                }
                if (sm.texture != 0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, sm.texture);
                }
                glActiveTexture(GL_TEXTURE0);
                glDrawElements(GL_TRIANGLES,
                               GLsizei(sm.indexCount),
                               GL_UNSIGNED_INT,
                               reinterpret_cast<void*>(uintptr_t(sm.indexStart) * sizeof(uint32_t)));
                ++dbgSubMeshDraws;
            }
        }
        if (boundMesh->vao && boundMesh->vao->isCreated())
            boundMesh->vao->release();
        boundMesh = nullptr;
        i = runEnd;
    }
    if (boundMesh && boundMesh->vao && boundMesh->vao->isCreated())
        boundMesh->vao->release();
    // Clean up both texture units; leave unit 0 active to match prior teardown.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_doodadProgram->release();

    // Throttled one-line breakdown of the doodad pass so we can pin "no
    // models visible" failures (load? cull? all subMeshes empty?).
    static qint64 s_lastDoodadLogMs = 0;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_verboseLogging && nowMs - s_lastDoodadLogMs > 2000)
    {
        s_lastDoodadLogMs = nowMs;
        qInfo("[scene3d-doodad] uniqueModels=%d loadOk=%d loadFail=%d queued=%d "
              "instCulled=%d instDrawn=%d subMeshDraws=%d (instances=%zu, inFlight=%d)",
            dbgUniqueModels, dbgLoadOk, dbgLoadFail, dbgQueued,
            dbgInstCulled, dbgInstDrawn, dbgSubMeshDraws,
            m_doodadInstances.size(), int(m_doodadInFlight.size()));
    }
}

// ============================================================================
// Textured WMO pass: real client wall/floor/ceiling/bridge geometry.
// Structurally mirrors the doodad pass (per-FDID VBO/IBO/VAO cache, lazy
// load, per-instance u_model + frustum cull) but with a 36-byte vertex
// (pos/normal/uv + 4ub MOCV colour) and the dedicated m_wmoProgram.
// ============================================================================

void SceneView3D::destroyTexturedWmoResources()
{
    if (m_wmoModels.empty() && m_wmoInstances.empty())
    {
        m_texturedWmosBuilt = false;
        return;
    }
    bool const haveContext   = (QOpenGLContext::currentContext() == context());
    bool const widgetContext = context() != nullptr;
    if (!haveContext && widgetContext) makeCurrent();
    for (auto& [fdid, model] : m_wmoModels)
    {
        if (model.vbo && model.vbo->isCreated()) model.vbo->destroy();
        if (model.ibo && model.ibo->isCreated()) model.ibo->destroy();
        if (model.vao && model.vao->isCreated()) model.vao->destroy();
        // Textures live in m_terrainTextureCache (shared with ADT + doodad
        // layers); don't double-free here.
    }
    if (!haveContext && widgetContext) doneCurrent();
    m_wmoModels.clear();
    m_wmoInstances.clear();
    m_texturedWmosBuilt = false;
    // STAGE B1: drop in-flight / pending async loads (see destroyDoodadResources).
    m_wmoInFlight.clear();
    {
        std::lock_guard<std::mutex> g(m_wmoPendingMutex);
        m_wmoPending.clear();
    }
}

// STAGE B1: the draw path no longer calls this -- WMO models stream in via
// dispatchWmoLoad (worker) + uploadWmoPayload (GL drain).  Retained as the
// synchronous reference / fallback so the flatten+upload logic has one
// readable definition.
bool SceneView3D::ensureWmoModelLoaded(uint32_t fdid)
{
    auto it = m_wmoModels.find(fdid);
    if (it != m_wmoModels.end())
        return it->second.loaded;

    WmoGpuModel& gpu = m_wmoModels[fdid];
    gpu.loadAttempted = true;

    io::WmoModel model;
    if (!io::loadWmo(*m_cascClient, fdid, model) || model.groups.empty())
        return false;

    // Flatten every group into one interleaved VBO + one IBO.  The reader's
    // per-submesh indexStart/indexCount index into the GROUP's own index
    // buffer, so each group's range is shifted by the running iBase, and the
    // index VALUES are bumped by the running vertex base (vBase) so they
    // point at the right verts in the merged buffer.  Per-vertex layout is
    // 9 floats (pos3 + normal3 + uv2) packed as 8 floats from the reader,
    // plus a 4ub MOCV colour appended (white when the group has no MOCV).
    std::vector<WmoVertex>     verts;
    std::vector<uint32_t>      idx;
    std::vector<WmoGpuSubMesh> subs;
    uint32_t vBase = 0;
    for (io::WmoGroupMesh const& g : model.groups)
    {
        uint32_t const gVerts = uint32_t(g.vertices.size() / 8);
        bool const haveCol = (g.colours.size() / 4) >= gVerts;
        for (uint32_t v = 0; v < gVerts; ++v)
        {
            WmoVertex wv{};
            wv.x  = g.vertices[v * 8 + 0];
            wv.y  = g.vertices[v * 8 + 1];
            wv.z  = g.vertices[v * 8 + 2];
            wv.nx = g.vertices[v * 8 + 3];
            wv.ny = g.vertices[v * 8 + 4];
            wv.nz = g.vertices[v * 8 + 5];
            wv.u  = g.vertices[v * 8 + 6];
            wv.v  = g.vertices[v * 8 + 7];
            if (haveCol)
            {
                wv.r = g.colours[v * 4 + 0];
                wv.g = g.colours[v * 4 + 1];
                wv.b = g.colours[v * 4 + 2];
                wv.a = g.colours[v * 4 + 3];
            }
            else
            {
                wv.r = wv.g = wv.b = wv.a = 255;   // white -> no MOCV modulate.
            }
            verts.push_back(wv);
        }

        uint32_t const iBase = uint32_t(idx.size());
        for (uint32_t ix : g.indices)
            idx.push_back(ix + vBase);

        for (io::WmoSubMesh const& sm : g.subMeshes)
        {
            WmoGpuSubMesh d;
            d.indexStart = iBase + sm.indexStart;   // group-local -> model-global.
            d.indexCount = sm.indexCount;
            d.blendMode  = sm.blendMode;
            d.interior   = sm.interior;
            if (m_terrainTextureCache)
            {
                if (sm.textureFileDataId != 0)
                    d.texture = m_terrainTextureCache->textureForFileDataId(sm.textureFileDataId, *this);
                if (d.texture == 0 && !sm.texturePath.empty())
                    d.texture = m_terrainTextureCache->textureForPath(sm.texturePath, *this);
            }
            subs.push_back(d);
        }
        vBase += gVerts;
    }

    if (verts.empty() || idx.empty() || subs.empty())
        return false;

    // Bounding sphere from the model union bbox (already FixCoord'd / local).
    float const cx = 0.5f * (model.bboxMin[0] + model.bboxMax[0]);
    float const cy = 0.5f * (model.bboxMin[1] + model.bboxMax[1]);
    float const cz = 0.5f * (model.bboxMin[2] + model.bboxMax[2]);
    float const dx = 0.5f * (model.bboxMax[0] - model.bboxMin[0]);
    float const dy = 0.5f * (model.bboxMax[1] - model.bboxMin[1]);
    float const dz = 0.5f * (model.bboxMax[2] - model.bboxMin[2]);
    gpu.centerX = cx; gpu.centerY = cy; gpu.centerZ = cz;
    gpu.radius  = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (gpu.radius < 0.5f) gpu.radius = 0.5f;

    gpu.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    gpu.ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    gpu.vao = std::make_unique<QOpenGLVertexArrayObject>();
    gpu.vbo->create();
    gpu.ibo->create();
    gpu.vao->create();
    gpu.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    gpu.ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);

    gpu.vao->bind();
    gpu.vbo->bind();
    gpu.vbo->allocate(verts.data(), int(verts.size() * sizeof(WmoVertex)));
    gpu.ibo->bind();
    gpu.ibo->allocate(idx.data(), int(idx.size() * sizeof(uint32_t)));
    constexpr int kStride = int(sizeof(WmoVertex));   // 36 bytes.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(offsetof(WmoVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(offsetof(WmoVertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(offsetof(WmoVertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, kStride,
        reinterpret_cast<void*>(offsetof(WmoVertex, r)));
    gpu.vao->release();
    gpu.vbo->release();
    gpu.ibo->release();

    gpu.vertexCount = GLsizei(verts.size());
    gpu.indexCount  = GLsizei(idx.size());
    gpu.subMeshes   = std::move(subs);
    gpu.loaded = true;
    return true;
}

void SceneView3D::drawTexturedWmos(QMatrix4x4 const& mvp)
{
    if (!m_wmoProgram || m_wmoInstances.empty())
        return;

    // Frustum planes are already extracted once per paint (extractFrustumPlanes
    // at the top of paintGL); reuse the shared member helper.
    m_wmoProgram->bind();
    m_wmoProgram->setUniformValue(m_wmoUMvp, mvp);
    applyFogAndSunUniforms(*m_wmoProgram);
    m_wmoProgram->setUniformValue(m_wmoUTexture, 0);

    // Sort by root FDID so each model's VAO binds once AND a contiguous run of
    // instances shares one FDID (the conservative-visibility scan below walks
    // that run before deciding whether to load/draw the model).
    std::stable_sort(m_wmoInstances.begin(), m_wmoInstances.end(),
        [](WmoInstanceGpu const& a, WmoInstanceGpu const& b)
        {
            return a.wmoRootFdid < b.wmoRootFdid;
        });

    // STAGE B1/C1: the model is no longer loaded synchronously in this loop.
    // Before touching the model cache we run a CONSERVATIVE per-instance test
    // using only the instance WORLD POSITION (inst.x/y/z -- known without a
    // loaded model) against the frustum + the camera draw ring, with a generous
    // fixed radius.  An FDID is only queued/drawn once at least one of its
    // instances passes that test, so a city's worth of FDIDs does NOT burst in
    // on the first frame (which would defeat the budgeted streaming).
    constexpr float kWmoConservRadius = 300.0f;          // generous: catches large WMOs whose bbox we can't see yet.
    constexpr float kWmoDrawRadius    = 5000.0f;          // WMOs read far (city skylines); reference §1.
    float const wmoRingR2 = kWmoDrawRadius * kWmoDrawRadius;
    auto instanceInRange = [&](WmoInstanceGpu const& in, float consR) -> bool
    {
        float const dx = in.x - m_camX, dy = in.y - m_camY, dz = in.z - m_camZ;
        if (dx * dx + dy * dy + dz * dz > wmoRingR2) return false;   // C1 distance ring.
        return sphereInFrustum(in.x, in.y, in.z, consR);
    };

    int dbgUniqueModels = 0, dbgLoadOk = 0, dbgLoadFail = 0;
    int dbgInstCulled = 0, dbgInstDrawn = 0, dbgSubMeshDraws = 0;
    int dbgQueued = 0;
    // Index walk over contiguous FDID runs (instances are sorted by FDID).
    // Processing per run keeps the conservative-visibility probe scan bounded
    // to its own run, so the whole pass stays O(instances) even when most FDIDs
    // are out of view and never load.
    WmoGpuModel* boundModel = nullptr;
    std::size_t const nInst = m_wmoInstances.size();
    for (std::size_t i = 0; i < nInst; )
    {
        uint32_t const fdid = m_wmoInstances[i].wmoRootFdid;
        std::size_t runEnd = i + 1;
        while (runEnd < nInst && m_wmoInstances[runEnd].wmoRootFdid == fdid)
            ++runEnd;
        ++dbgUniqueModels;

        auto it = m_wmoModels.find(fdid);
        bool const loaded    = (it != m_wmoModels.end() && it->second.loaded);
        bool const attempted = (it != m_wmoModels.end());   // loaded or hard-failed.
        if (!loaded)
        {
            if (!attempted)
            {
                // Queue ONLY if at least one instance of this run is
                // conservatively in view (review fix #2 -- never burst the
                // whole map).  Probe stays within this run.
                for (std::size_t k = i; k < runEnd; ++k)
                {
                    if (instanceInRange(m_wmoInstances[k], kWmoConservRadius))
                    {
                        dispatchWmoLoad(fdid);
                        ++dbgQueued;
                        break;
                    }
                }
            }
            else
            {
                ++dbgLoadFail;
            }
            i = runEnd;
            continue;
        }
        ++dbgLoadOk;
        boundModel = &it->second;
        boundModel->vao->bind();

        for (std::size_t k = i; k < runEnd; ++k)
        {
            WmoInstanceGpu const& inst = m_wmoInstances[k];
            // C1: per-instance distance ring (squared compare, no sqrt).
            float const ddx = inst.x - m_camX, ddy = inst.y - m_camY, ddz = inst.z - m_camZ;
            if (ddx * ddx + ddy * ddy + ddz * ddz > wmoRingR2)
            {
                ++dbgInstCulled;
                continue;
            }
            // Per-instance world-space center for the frustum test (rotation
            // does not move the bounding sphere; only scale grows the radius).
            float const sx = inst.x + inst.scale * boundModel->centerX;
            float const sy = inst.y + inst.scale * boundModel->centerY;
            float const sz = inst.z + inst.scale * boundModel->centerZ;
            float const sr = boundModel->radius * inst.scale;
            if (!sphereInFrustum(sx, sy, sz, sr))
            {
                ++dbgInstCulled;
                continue;
            }
            ++dbgInstDrawn;

            // World matrix: translate * Rz * Ry * Rx * scale (same composition
            // order as the doodad pass + computeWmoRootPlacement's decode).
            QMatrix4x4 model;
            model.translate(inst.x, inst.y, inst.z);
            // Z-up FixCoord stands the buildings, but the city comes out yawed
            // 90 deg from the terrain layout; correct it with a uniform 90 deg
            // clockwise turn (negative about world up Z, viewed top-down).
            model.rotate(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, -90.0f));
            model.rotate(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, inst.rotZ * 180.0f / 3.14159265f));
            model.rotate(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, inst.rotY * 180.0f / 3.14159265f));
            model.rotate(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, inst.rotX * 180.0f / 3.14159265f));
            model.scale(inst.scale);
            m_wmoProgram->setUniformValue(m_wmoUModel, model);

            for (WmoGpuSubMesh const& sm : boundModel->subMeshes)
            {
                if (sm.indexCount == 0) continue;
                m_wmoProgram->setUniformValue(m_wmoUHasTex, sm.texture != 0 ? 1 : 0);
                m_wmoProgram->setUniformValue(m_wmoUInterior, sm.interior ? 1 : 0);
                float const alphaCutoff = (sm.blendMode == 1) ? 0.5f : 0.0f;
                m_wmoProgram->setUniformValue(m_wmoUAlphaCutoff, alphaCutoff);
                if (sm.texture != 0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, sm.texture);
                }
                glDrawElements(GL_TRIANGLES,
                               GLsizei(sm.indexCount),
                               GL_UNSIGNED_INT,
                               reinterpret_cast<void*>(uintptr_t(sm.indexStart) * sizeof(uint32_t)));
                ++dbgSubMeshDraws;
            }
        }
        if (boundModel->vao && boundModel->vao->isCreated())
            boundModel->vao->release();
        boundModel = nullptr;
        i = runEnd;
    }
    if (boundModel && boundModel->vao && boundModel->vao->isCreated())
        boundModel->vao->release();
    glBindTexture(GL_TEXTURE_2D, 0);
    m_wmoProgram->release();

    static qint64 s_lastWmoLogMs = 0;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_verboseLogging && nowMs - s_lastWmoLogMs > 2000)
    {
        s_lastWmoLogMs = nowMs;
        qInfo("[scene3d-wmo] uniqueModels=%d loadOk=%d loadFail=%d queued=%d "
              "instCulled=%d instDrawn=%d subMeshDraws=%d (instances=%zu, inFlight=%d)",
            dbgUniqueModels, dbgLoadOk, dbgLoadFail, dbgQueued,
            dbgInstCulled, dbgInstDrawn, dbgSubMeshDraws,
            m_wmoInstances.size(), int(m_wmoInFlight.size()));
    }
}

// ============================================================================
// Atmospheric layer: time-of-day LUT + fog + sky dome.
// ============================================================================

namespace
{
struct AtmoSample
{
    float sunDir[3];
    float sunColor[3];
    float ambient;
    float zenith[3];
    float horizon[3];
    float fogColor[3];
    float skyGround[3];   // distinct below-horizon sky band (wow.export SkyFogColor)
    float fogStart;
    float fogEnd;
    float fogHeight;        // world Z of the fog plane (TC +Z up)
    float fogHeightFalloff; // per-unit-Z attenuation; 0 => height fog OFF this hour
};

// Per-hour atmospheric LUT.  Index = hour (0..23); intermediate hours
// interpolate linearly.  Designed to read "WoW-ish": warm dawn at 5-7,
// neutral midday, red dusk at 18-20, deep blue night.  Fog distance
// shortens at night to hide the lack of moon lighting.
// Column order: sunDir{3}, sunColor{3}, ambient, zenith{3}, horizon{3},
// fogColor{3}, skyGround{3}, fogStart, fogEnd, fogHeight, fogHeightFalloff.
// skyGround is a darker, slightly-blue variant of fogColor at midday and warms
// toward dawn/dusk; it is the distinct below-horizon sky band (band 5),
// decoupled from the lighting ground ambient.
// fogHeight is the world-Z of the fog plane and fogHeightFalloff the per-unit-Z
// attenuation (wow.export height-fog, mpv_fog.inc.glsl:48-50): distant terrain
// above the plane de-fogs relative to valley floors.  The plane sits low so the
// ground-hugging fog pools in basins; falloff is gentlest at midday and a touch
// stronger at dawn/dusk/night when ground fog reads thicker.  falloff == 0 on a
// row makes the height term an exact no-op for that hour.
constexpr AtmoSample kTodLut[24] = {
    /* 00 */ { { 0.30f, -0.50f, -0.40f }, { 0.40f, 0.45f, 0.65f }, 0.10f, { 0.04f, 0.06f, 0.12f }, { 0.06f, 0.08f, 0.16f }, { 0.06f, 0.08f, 0.16f }, { 0.03f, 0.04f, 0.10f },  400.0f, 1100.0f,  50.0f, 0.0014f },
    /* 01 */ { { 0.30f, -0.50f, -0.40f }, { 0.38f, 0.42f, 0.62f }, 0.09f, { 0.03f, 0.05f, 0.11f }, { 0.05f, 0.07f, 0.15f }, { 0.05f, 0.07f, 0.15f }, { 0.03f, 0.04f, 0.10f },  380.0f, 1050.0f,  50.0f, 0.0014f },
    /* 02 */ { { 0.25f, -0.45f, -0.45f }, { 0.36f, 0.40f, 0.60f }, 0.08f, { 0.03f, 0.04f, 0.10f }, { 0.04f, 0.06f, 0.14f }, { 0.04f, 0.06f, 0.14f }, { 0.02f, 0.03f, 0.09f },  360.0f, 1000.0f,  50.0f, 0.0015f },
    /* 03 */ { { 0.20f, -0.40f, -0.45f }, { 0.34f, 0.38f, 0.58f }, 0.08f, { 0.03f, 0.04f, 0.10f }, { 0.04f, 0.06f, 0.14f }, { 0.04f, 0.06f, 0.14f }, { 0.02f, 0.03f, 0.09f },  360.0f, 1000.0f,  50.0f, 0.0015f },
    /* 04 */ { { 0.15f, -0.30f, -0.40f }, { 0.40f, 0.40f, 0.58f }, 0.10f, { 0.05f, 0.06f, 0.13f }, { 0.10f, 0.10f, 0.18f }, { 0.10f, 0.10f, 0.18f }, { 0.05f, 0.06f, 0.13f },  400.0f, 1100.0f,  50.0f, 0.0016f },
    /* 05 */ { { 0.55f, -0.20f,  0.10f }, { 0.95f, 0.65f, 0.45f }, 0.18f, { 0.20f, 0.30f, 0.55f }, { 0.85f, 0.55f, 0.35f }, { 0.78f, 0.55f, 0.40f }, { 0.40f, 0.30f, 0.26f },  500.0f, 1300.0f,  60.0f, 0.0016f },
    /* 06 */ { { 0.60f, -0.10f,  0.25f }, { 1.00f, 0.78f, 0.55f }, 0.24f, { 0.32f, 0.42f, 0.68f }, { 0.95f, 0.70f, 0.45f }, { 0.88f, 0.70f, 0.55f }, { 0.46f, 0.38f, 0.34f },  550.0f, 1400.0f,  60.0f, 0.0014f },
    /* 07 */ { { 0.55f,  0.10f,  0.40f }, { 1.00f, 0.88f, 0.72f }, 0.30f, { 0.36f, 0.52f, 0.82f }, { 0.92f, 0.82f, 0.78f }, { 0.86f, 0.82f, 0.86f }, { 0.42f, 0.44f, 0.52f },  600.0f, 1500.0f,  70.0f, 0.0012f },
    /* 08 */ { { 0.50f,  0.30f,  0.55f }, { 1.00f, 0.96f, 0.88f }, 0.32f, { 0.34f, 0.55f, 0.90f }, { 0.82f, 0.86f, 0.94f }, { 0.78f, 0.86f, 0.96f }, { 0.34f, 0.40f, 0.52f },  600.0f, 1500.0f,  70.0f, 0.0011f },
    /* 09 */ { { 0.45f,  0.45f,  0.65f }, { 1.00f, 1.00f, 0.96f }, 0.34f, { 0.32f, 0.56f, 0.92f }, { 0.80f, 0.86f, 0.96f }, { 0.78f, 0.86f, 0.96f }, { 0.32f, 0.40f, 0.54f },  600.0f, 1500.0f,  80.0f, 0.0010f },
    /* 10 */ { { 0.42f,  0.48f,  0.70f }, { 1.00f, 1.00f, 1.00f }, 0.35f, { 0.31f, 0.56f, 0.93f }, { 0.78f, 0.86f, 0.96f }, { 0.78f, 0.86f, 0.96f }, { 0.30f, 0.40f, 0.56f },  600.0f, 1500.0f,  80.0f, 0.0010f },
    /* 11 */ { { 0.41f,  0.50f,  0.72f }, { 1.00f, 1.00f, 1.00f }, 0.35f, { 0.30f, 0.56f, 0.93f }, { 0.78f, 0.86f, 0.96f }, { 0.78f, 0.86f, 0.96f }, { 0.30f, 0.40f, 0.56f },  600.0f, 1500.0f,  80.0f, 0.0010f },
    /* 12 */ { { 0.40f,  0.50f,  0.75f }, { 1.00f, 1.00f, 1.00f }, 0.36f, { 0.30f, 0.55f, 0.92f }, { 0.78f, 0.86f, 0.96f }, { 0.78f, 0.86f, 0.96f }, { 0.30f, 0.40f, 0.56f },  600.0f, 1500.0f,  80.0f, 0.0010f },
    /* 13 */ { { 0.38f,  0.50f,  0.72f }, { 1.00f, 1.00f, 1.00f }, 0.36f, { 0.31f, 0.55f, 0.92f }, { 0.78f, 0.86f, 0.96f }, { 0.78f, 0.86f, 0.96f }, { 0.30f, 0.40f, 0.56f },  600.0f, 1500.0f,  80.0f, 0.0010f },
    /* 14 */ { { 0.36f,  0.48f,  0.68f }, { 1.00f, 0.99f, 0.96f }, 0.35f, { 0.32f, 0.55f, 0.92f }, { 0.80f, 0.86f, 0.96f }, { 0.78f, 0.86f, 0.96f }, { 0.32f, 0.40f, 0.55f },  600.0f, 1500.0f,  80.0f, 0.0010f },
    /* 15 */ { { 0.32f,  0.46f,  0.62f }, { 1.00f, 0.97f, 0.92f }, 0.34f, { 0.34f, 0.55f, 0.92f }, { 0.82f, 0.86f, 0.94f }, { 0.80f, 0.86f, 0.94f }, { 0.36f, 0.42f, 0.54f },  600.0f, 1500.0f,  80.0f, 0.0011f },
    /* 16 */ { { 0.28f,  0.42f,  0.52f }, { 1.00f, 0.93f, 0.84f }, 0.32f, { 0.36f, 0.54f, 0.90f }, { 0.86f, 0.86f, 0.90f }, { 0.84f, 0.86f, 0.90f }, { 0.40f, 0.44f, 0.52f },  600.0f, 1500.0f,  70.0f, 0.0012f },
    /* 17 */ { { 0.22f,  0.36f,  0.40f }, { 1.00f, 0.88f, 0.74f }, 0.30f, { 0.36f, 0.50f, 0.86f }, { 0.92f, 0.80f, 0.66f }, { 0.88f, 0.80f, 0.72f }, { 0.46f, 0.42f, 0.44f },  560.0f, 1400.0f,  70.0f, 0.0014f },
    /* 18 */ { { 0.10f,  0.30f,  0.22f }, { 1.00f, 0.66f, 0.42f }, 0.24f, { 0.30f, 0.40f, 0.74f }, { 0.95f, 0.62f, 0.36f }, { 0.90f, 0.66f, 0.50f }, { 0.46f, 0.34f, 0.30f },  500.0f, 1300.0f,  60.0f, 0.0016f },
    /* 19 */ { { 0.00f,  0.18f,  0.10f }, { 0.95f, 0.50f, 0.32f }, 0.18f, { 0.22f, 0.28f, 0.58f }, { 0.85f, 0.48f, 0.30f }, { 0.80f, 0.52f, 0.42f }, { 0.40f, 0.28f, 0.26f },  460.0f, 1250.0f,  60.0f, 0.0016f },
    /* 20 */ { {-0.10f,  0.10f, -0.10f }, { 0.78f, 0.40f, 0.28f }, 0.14f, { 0.14f, 0.18f, 0.40f }, { 0.55f, 0.30f, 0.22f }, { 0.50f, 0.32f, 0.28f }, { 0.24f, 0.18f, 0.18f },  440.0f, 1200.0f,  55.0f, 0.0015f },
    /* 21 */ { {-0.20f,  0.05f, -0.25f }, { 0.55f, 0.42f, 0.45f }, 0.12f, { 0.08f, 0.12f, 0.26f }, { 0.20f, 0.18f, 0.26f }, { 0.18f, 0.18f, 0.26f }, { 0.10f, 0.10f, 0.16f },  420.0f, 1150.0f,  55.0f, 0.0015f },
    /* 22 */ { {-0.25f, -0.10f, -0.35f }, { 0.46f, 0.42f, 0.55f }, 0.11f, { 0.05f, 0.08f, 0.18f }, { 0.10f, 0.12f, 0.22f }, { 0.09f, 0.10f, 0.20f }, { 0.05f, 0.06f, 0.13f },  410.0f, 1120.0f,  50.0f, 0.0014f },
    /* 23 */ { {-0.28f, -0.30f, -0.40f }, { 0.42f, 0.44f, 0.60f }, 0.10f, { 0.04f, 0.07f, 0.14f }, { 0.07f, 0.10f, 0.18f }, { 0.07f, 0.10f, 0.18f }, { 0.04f, 0.05f, 0.11f },  400.0f, 1100.0f,  50.0f, 0.0014f },
};

constexpr void copyVec3(float (&dst)[3], float const (&src)[3])
{
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
}
constexpr float lerpf(float a, float b, float t) { return a + (b - a) * t; }
constexpr void lerpVec3(float (&dst)[3], float const (&a)[3], float const (&b)[3], float t)
{
    dst[0] = lerpf(a[0], b[0], t);
    dst[1] = lerpf(a[1], b[1], t);
    dst[2] = lerpf(a[2], b[2], t);
}

void sampleAtmosphere(float hour, AtmoSample& out)
{
    if (hour < 0.0f)  hour = 0.0f;
    if (hour >= 24.0f) hour = std::fmod(hour, 24.0f);
    int const  i0 = int(std::floor(hour)) % 24;
    int const  i1 = (i0 + 1) % 24;
    float const t = hour - std::floor(hour);
    AtmoSample const& a = kTodLut[i0];
    AtmoSample const& b = kTodLut[i1];
    lerpVec3(out.sunDir,    a.sunDir,    b.sunDir,    t);
    lerpVec3(out.sunColor,  a.sunColor,  b.sunColor,  t);
    out.ambient = lerpf(a.ambient, b.ambient, t);
    lerpVec3(out.zenith,    a.zenith,    b.zenith,    t);
    lerpVec3(out.horizon,   a.horizon,   b.horizon,   t);
    lerpVec3(out.fogColor,  a.fogColor,  b.fogColor,  t);
    lerpVec3(out.skyGround, a.skyGround, b.skyGround, t);
    out.fogStart = lerpf(a.fogStart, b.fogStart, t);
    out.fogEnd   = lerpf(a.fogEnd,   b.fogEnd,   t);
    out.fogHeight        = lerpf(a.fogHeight,        b.fogHeight,        t);
    out.fogHeightFalloff = lerpf(a.fogHeightFalloff, b.fogHeightFalloff, t);
}
} // namespace

void SceneView3D::setSkyVisible(bool on)
{
    if (m_skyVisible == on) return;
    m_skyVisible = on;
    update();
}

void SceneView3D::setWaterVisible(bool on)
{
    if (m_waterVisible == on) return;
    m_waterVisible = on;
    update();
}

void SceneView3D::destroyWaterTiles()
{
    // STAGE B2: reset the streaming cam sentinel + pending queue unconditionally
    // so the next paint re-scans the load ring for the fresh map (the sentinel
    // is what gates the per-tile-cross re-scan in streamWaterTiles).
    m_waterStreamCamGx = -100000;
    m_waterStreamCamGy = -100000;
    m_waterPendingBuild.clear();
    // Drop async-water inboxes too (worker payloads for the old map are stale).
    {
        std::lock_guard<std::mutex> g(m_waterPendingMutex);
        m_waterPending.clear();
    }
    m_waterInFlight.clear();
    if (m_waterTiles.empty()) return;
    bool const haveContext = (QOpenGLContext::currentContext() == context());
    if (!haveContext) makeCurrent();
    for (WaterTile& tile : m_waterTiles)
    {
        for (WaterChunkGpu& ch : tile.chunks)
        {
            if (ch.vao && ch.vao->isCreated()) ch.vao->destroy();
            if (ch.vbo && ch.vbo->isCreated()) ch.vbo->destroy();
        }
    }
    if (!haveContext) doneCurrent();
    m_waterTiles.clear();
}

void SceneView3D::rebuildWaterTiles()
{
    destroyWaterTiles();
    if (!m_mesh.ok() || !m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
        return;

    // Same tile enumeration as rebuildAdtTerrainTiles -- match ADT
    // coverage so the water sits on the same set of tiles the operator
    // currently sees in the realistic terrain pass.
    constexpr int   CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;
    std::vector<std::pair<int, int>> tiles;
    dtNavMesh const* nm = m_mesh.navmesh();
    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* mt = nm->getTile(ti);
        if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
        float const minX = mt->header->bmin[2];
        float const minY = mt->header->bmin[0];
        int const gx = int(std::floor(CENTER_GRID_ID - minX / TILE_SIZE));
        int const gy = int(std::floor(CENTER_GRID_ID - minY / TILE_SIZE));
        tiles.emplace_back(gx, gy);
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    m_waterTiles.reserve(tiles.size());
    for (auto const& [gx, gy] : tiles)
    {
        WaterTile tile;
        tile.gx = gx;
        tile.gy = gy;
        tile.loadAttempted = true;
        tile.hasGeometry   = loadAndUploadWaterTile(gx, gy, tile);
        m_waterTiles.push_back(std::move(tile));
    }
}

// STAGE B2: camera-relative water streaming.  On a camera tile-cross, enumerate
// the in-world navmesh tiles inside the load ring, drop the ones already
// resident / already queued, and stash the rest sorted nearest-first.  Every
// frame, build at most `maxBuildsThisFrame` of those queued tiles -- the build
// itself stays on the GL thread (loadAndUploadWaterTile uploads GL buffers), but
// the per-frame cap removes the one-shot continent hitch the old all-at-once
// rebuildWaterTiles caused.
void SceneView3D::streamWaterTiles(int maxBuildsThisFrame)
{
    if (!m_mesh.ok() || !m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
        return;

    constexpr int   CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;

    int const camGx = int(std::floor(CENTER_GRID_ID - m_camX / TILE_SIZE));
    int const camGy = int(std::floor(CENTER_GRID_ID - m_camY / TILE_SIZE));

    // Re-enumerate the pending set only when the camera crosses into a new
    // tile (the navmesh walk is O(maxTiles) -- too much to do every frame).
    if (camGx != m_waterStreamCamGx || camGy != m_waterStreamCamGy)
    {
        m_waterStreamCamGx = camGx;
        m_waterStreamCamGy = camGy;

        // Load ring = draw radius + 1 tile so water is ready just before it
        // enters the draw distance.
        int const loadRingTiles = int(std::ceil(renderRadiusYards() / TILE_SIZE)) + 1;

        // Collect the in-world tiles (navmesh-covered) inside the ring that are
        // neither resident nor already pending.
        std::vector<std::pair<int, int>> ring;
        dtNavMesh const* nm = m_mesh.navmesh();
        for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
        {
            dtMeshTile const* mt = nm->getTile(ti);
            if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
            float const minX = mt->header->bmin[2];
            float const minY = mt->header->bmin[0];
            int const gx = int(std::floor(CENTER_GRID_ID - minX / TILE_SIZE));
            int const gy = int(std::floor(CENTER_GRID_ID - minY / TILE_SIZE));
            if (std::abs(gx - camGx) > loadRingTiles || std::abs(gy - camGy) > loadRingTiles)
                continue;
            bool resident = false;
            for (WaterTile const& wt : m_waterTiles)
                if (wt.gx == gx && wt.gy == gy) { resident = true; break; }
            if (resident) continue;
            ring.emplace_back(gx, gy);
        }
        std::sort(ring.begin(), ring.end());
        ring.erase(std::unique(ring.begin(), ring.end()), ring.end());
        // Nearest-first (squared tile-grid distance to the camera tile) so the
        // tiles the operator is most likely looking at build first.
        std::sort(ring.begin(), ring.end(),
            [camGx, camGy](std::pair<int, int> const& a, std::pair<int, int> const& b)
            {
                int const da = (a.first - camGx) * (a.first - camGx) + (a.second - camGy) * (a.second - camGy);
                int const db = (b.first - camGx) * (b.first - camGx) + (b.second - camGy) * (b.second - camGy);
                return da < db;
            });
        m_waterPendingBuild = std::move(ring);
    }

    // PERF: DISPATCH up to the budget of async decodes from the front (nearest)
    // of the queue.  The decode (loadAdtLiquid, ~70ms) now runs on a worker
    // thread; the GL VBO upload happens later in drainPendingWaterUploads.  So
    // this no longer stalls the render thread -- the budget just paces how many
    // parallel decodes we kick off per camera-cross frame.
    int dispatched = 0;
    while (dispatched < maxBuildsThisFrame && !m_waterPendingBuild.empty())
    {
        auto [gx, gy] = m_waterPendingBuild.front();
        m_waterPendingBuild.erase(m_waterPendingBuild.begin());
        bool resident = false;
        for (WaterTile const& wt : m_waterTiles)
            if (wt.gx == gx && wt.gy == gy) { resident = true; break; }
        if (resident) continue;
        dispatchWaterLoad(gx, gy);
        ++dispatched;
    }
}

// STAGE C2: free GL resources of water tiles whose center is beyond the unload
// radius (draw radius + hysteresis) so water VRAM stays bounded as the camera
// roams.  Mirrors unloadFarAdtTiles; GL context is current (called from paintGL).
void SceneView3D::unloadFarWaterTiles()
{
    if (m_waterTiles.empty()) return;
    constexpr float TILE_SIZE  = 533.3333f;
    constexpr int   kCenterGid = 32;
    float const unloadRadius = renderRadiusYards() + 2.0f * TILE_SIZE;
    float const unloadR2 = unloadRadius * unloadRadius;

    std::size_t freed = 0;
    auto it = m_waterTiles.begin();
    while (it != m_waterTiles.end())
    {
        WaterTile& tile = *it;
        float const tcX = (kCenterGid - tile.gx) * TILE_SIZE - 0.5f * TILE_SIZE;
        float const tcY = (kCenterGid - tile.gy) * TILE_SIZE - 0.5f * TILE_SIZE;
        float const dx = tcX - m_camX, dy = tcY - m_camY;
        if (dx * dx + dy * dy <= unloadR2)
        {
            ++it;
            continue;
        }
        for (WaterChunkGpu& ch : tile.chunks)
        {
            if (ch.vao && ch.vao->isCreated()) ch.vao->destroy();
            if (ch.vbo && ch.vbo->isCreated()) ch.vbo->destroy();
        }
        it = m_waterTiles.erase(it);
        ++freed;
    }
    // A re-scan must re-queue an evicted tile if the camera turns back, so
    // force the next streamWaterTiles call to re-enumerate.
    if (freed > 0)
    {
        m_waterStreamCamGx = -100000;
        m_waterStreamCamGy = -100000;
    }
}

bool SceneView3D::loadAndUploadWaterTile(int gx, int gy, WaterTile& out)
{
    auto dirOpt = m_mapDb2->directoryFor(m_heightmapMapId);
    if (!dirOpt) return false;

    uint32_t liquidRootFdid = 0;
    if (io::Wdt const* wdt = ensureWdt())
        liquidRootFdid = wdt->entryFor(gx, gy).rootADT;

    io::AdtLiquid liquid;
    if (!io::loadAdtLiquid(*m_cascClient, *dirOpt, m_heightmapMapId, gx, gy, liquid,
                           liquidRootFdid))
        return false;
    if (liquid.chunks.empty())
        return false;

    out.chunks.reserve(liquid.chunks.size());
    bool anyRenderable = false;
    // Accumulate the rendered water surface's Z extent across EVERY chunk of
    // this tile (NOT per-chunk -- a per-chunk reset would leave the cull
    // sphere holding only the last chunk's Z and could hide visible water).
    // Seeded to +/-inf and updated on each emitted (post-transform) vertex;
    // if no vertex is ever emitted these stay at inf and we restore the
    // 0/0 default below (the tile is skipped via hasGeometry=false anyway).
    float accMinZ = std::numeric_limits<float>::infinity();
    float accMaxZ = -std::numeric_limits<float>::infinity();
    for (io::LiquidChunk const& lc : liquid.chunks)
    {
        // Emit two tris per 1x1 quad whose existsBitmap bit is set.  Use
        // the vertex grid's actual stored Z so puddles and lake edges
        // pick up MH2O's per-vertex variation.
        std::vector<WaterVertex> verts;
        verts.reserve(64 * 6);
        float const kindF = float(uint8_t(lc.kind));
        auto sample = [&](int row, int col) -> WaterVertex
        {
            io::LiquidVertex const& src = lc.vertices[size_t(row * 9 + col)];
            // Track the real rendered Z (src.z is exactly what gets pushed)
            // across all chunks so the cull sphere bounds this tile's water.
            accMinZ = std::min(accMinZ, src.z);
            accMaxZ = std::max(accMaxZ, src.z);
            return { src.x, src.y, src.z, kindF };
        };
        for (int row = 0; row < 8; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                if (!(lc.existsBitmap & (uint64_t(1) << (row * 8 + col))))
                    continue;
                WaterVertex const v00 = sample(row,     col);
                WaterVertex const v10 = sample(row + 1, col);
                WaterVertex const v01 = sample(row,     col + 1);
                WaterVertex const v11 = sample(row + 1, col + 1);
                verts.push_back(v00);
                verts.push_back(v10);
                verts.push_back(v01);
                verts.push_back(v10);
                verts.push_back(v11);
                verts.push_back(v01);
            }
        }
        if (verts.empty()) continue;

        WaterChunkGpu ch;
        ch.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        ch.vao = std::make_unique<QOpenGLVertexArrayObject>();
        ch.vertexCount = static_cast<GLsizei>(verts.size());
        ch.vbo->create();
        ch.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        ch.vao->create();
        ch.vao->bind();
        ch.vbo->bind();
        ch.vbo->allocate(verts.data(), int(verts.size() * sizeof(WaterVertex)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
            reinterpret_cast<void*>(offsetof(WaterVertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
            reinterpret_cast<void*>(offsetof(WaterVertex, kind)));
        ch.vbo->release();
        ch.vao->release();
        out.chunks.push_back(std::move(ch));
        anyRenderable = true;
    }
    // Commit the accumulated Z extent only if at least one vertex was emitted;
    // otherwise leave out.minZ/out.maxZ at their 0/0 default (the caller sees
    // hasGeometry=false and skips the tile, so the values are never read).
    if (anyRenderable)
    {
        out.minZ = accMinZ;
        out.maxZ = accMaxZ;
    }
    return anyRenderable;
}

// ---- Async water (mirror of the ADT async path) -------------------------
// PERF: off-thread water decode (CASC read + io::loadAdtLiquid + WaterVertex
// build).  Posts a CPU-only payload; the GL drainer turns it into VBO/VAO.  No
// GL handles here.  Removes the ~70ms/tile synchronous loadAdtLiquid that
// stalled the render thread inside streamWaterTiles.
struct WaterLoadTask : public QRunnable
{
    WaterLoadTask() { setAutoDelete(true); }
    QPointer<world_editor::render::SceneView3D> view;
    world_editor::io::CascClient*   casc   = nullptr;
    world_editor::io::MapDb2Lookup* mapDb2 = nullptr;
    uint32_t                        mapId  = 0;
    int                             gx     = 0;
    int                             gy     = 0;
    uint32_t                        rootFdid = 0;
    void run() override;
};

void SceneView3D::enqueueWaterPending(WaterTilePending pending)
{
    std::lock_guard<std::mutex> g(m_waterPendingMutex);
    m_waterPending.push_back(std::move(pending));
}

void SceneView3D::dispatchWaterLoad(int gx, int gy)
{
    if (!m_cascClient || !m_cascClient->isOpen() || !m_mapDb2) return;
    uint32_t const key = (uint32_t(gy) << 16) | (uint32_t(gx) & 0xFFFFu);
    if (m_waterInFlight.contains(key)) return;
    for (WaterTile const& wt : m_waterTiles)
        if (wt.gx == gx && wt.gy == gy) return;   // already resident
    m_waterInFlight.insert(key);
    uint32_t rootFdid = 0;
    if (io::Wdt const* wdt = ensureWdt())
        rootFdid = wdt->entryFor(gx, gy).rootADT;
    WaterLoadTask* task = new WaterLoadTask;
    task->view     = this;
    task->casc     = m_cascClient;
    task->mapDb2   = m_mapDb2;
    task->mapId    = m_heightmapMapId;
    task->gx       = gx;
    task->gy       = gy;
    task->rootFdid = rootFdid;
    QThreadPool::globalInstance()->start(task);
}

void SceneView3D::drainPendingWaterUploads(int maxThisFrame)
{
    std::vector<WaterTilePending> batch;
    {
        std::lock_guard<std::mutex> g(m_waterPendingMutex);
        if (m_waterPending.empty()) return;
        int const take = std::min<int>(maxThisFrame, int(m_waterPending.size()));
        batch.reserve(take);
        for (int i = 0; i < take; ++i)
            batch.push_back(std::move(m_waterPending[size_t(i)]));
        m_waterPending.erase(m_waterPending.begin(), m_waterPending.begin() + take);
    }
    for (WaterTilePending& p : batch)
    {
        uint32_t const key = (uint32_t(p.gy) << 16) | (uint32_t(p.gx) & 0xFFFFu);
        m_waterInFlight.remove(key);
        bool resident = false;
        for (WaterTile const& wt : m_waterTiles)
            if (wt.gx == p.gx && wt.gy == p.gy) { resident = true; break; }
        if (resident) continue;
        WaterTile tile;
        tile.gx = p.gx;
        tile.gy = p.gy;
        tile.loadAttempted = true;
        if (p.ok)
        {
            for (WaterChunkCpu& cpu : p.chunks)
            {
                if (cpu.verts.empty()) continue;
                WaterChunkGpu ch;
                ch.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
                ch.vao = std::make_unique<QOpenGLVertexArrayObject>();
                ch.vertexCount = static_cast<GLsizei>(cpu.verts.size());
                ch.vbo->create();
                ch.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
                ch.vao->create();
                ch.vao->bind();
                ch.vbo->bind();
                ch.vbo->allocate(cpu.verts.data(), int(cpu.verts.size() * sizeof(WaterVertex)));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
                    reinterpret_cast<void*>(offsetof(WaterVertex, x)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
                    reinterpret_cast<void*>(offsetof(WaterVertex, kind)));
                ch.vbo->release();
                ch.vao->release();
                tile.chunks.push_back(std::move(ch));
            }
            tile.hasGeometry = !tile.chunks.empty();
            tile.minZ = p.minZ;
            tile.maxZ = p.maxZ;
        }
        m_waterTiles.push_back(std::move(tile));
    }
}

void WaterLoadTask::run()
{
    namespace io = world_editor::io;
    using world_editor::render::SceneView3D;

    SceneView3D::WaterTilePending pending;
    pending.gx = gx;
    pending.gy = gy;
    pending.ok = false;

    bool const haveCasc = casc && casc->isOpen() && mapDb2;
    auto dirOpt = haveCasc ? mapDb2->directoryFor(mapId) : std::optional<std::string>{};
    if (!view.isNull() && haveCasc && dirOpt)
    {
        io::AdtLiquid liquid;
        if (io::loadAdtLiquid(*casc, *dirOpt, mapId, gx, gy, liquid, rootFdid)
            && !liquid.chunks.empty())
        {
            float accMinZ =  std::numeric_limits<float>::infinity();
            float accMaxZ = -std::numeric_limits<float>::infinity();
            // PERF: merge ALL of the tile's liquid chunks into ONE vertex buffer.
            // Water needs no per-chunk GL state (one shader, kind is per-vertex),
            // so a single VBO/VAO per tile replaces up to 256 -- the GL upload
            // was the remaining render-thread cost after the decode went async.
            SceneView3D::WaterChunkCpu merged;
            for (io::LiquidChunk const& lc : liquid.chunks)
            {
                float const kindF = float(uint8_t(lc.kind));
                auto sample = [&](int row, int col) -> SceneView3D::WaterVertex
                {
                    io::LiquidVertex const& src = lc.vertices[size_t(row * 9 + col)];
                    accMinZ = std::min(accMinZ, src.z);
                    accMaxZ = std::max(accMaxZ, src.z);
                    return { src.x, src.y, src.z, kindF };
                };
                for (int row = 0; row < 8; ++row)
                    for (int col = 0; col < 8; ++col)
                    {
                        if (!(lc.existsBitmap & (uint64_t(1) << (row * 8 + col))))
                            continue;
                        SceneView3D::WaterVertex const v00 = sample(row,     col);
                        SceneView3D::WaterVertex const v10 = sample(row + 1, col);
                        SceneView3D::WaterVertex const v01 = sample(row,     col + 1);
                        SceneView3D::WaterVertex const v11 = sample(row + 1, col + 1);
                        merged.verts.push_back(v00); merged.verts.push_back(v10); merged.verts.push_back(v01);
                        merged.verts.push_back(v10); merged.verts.push_back(v11); merged.verts.push_back(v01);
                    }
            }
            if (!merged.verts.empty())
            {
                pending.chunks.push_back(std::move(merged));
                pending.minZ = accMinZ;
                pending.maxZ = accMaxZ;
                pending.ok   = true;
            }
        }
    }

    QPointer<SceneView3D> guardedView = view;
    QMetaObject::invokeMethod(QCoreApplication::instance(),
        [guardedView, pending = std::move(pending)]() mutable
        {
            if (!guardedView) return;
            guardedView->enqueueWaterPending(std::move(pending));
            guardedView->update();
        },
        Qt::QueuedConnection);
}

void SceneView3D::setFogEnabled(bool on)
{
    if (m_fogEnabled == on) return;
    m_fogEnabled = on;
    update();
}

void SceneView3D::setTimeOfDay(float hours)
{
    // Wrap into [0, 24) so a slider that goes slightly out-of-range still
    // resolves to a meaningful entry in the LUT.
    if (!std::isfinite(hours)) hours = 12.0f;
    hours = std::fmod(hours, 24.0f);
    if (hours < 0.0f) hours += 24.0f;
    if (std::abs(hours - m_timeOfDay) < 1e-4f) return;
    m_timeOfDay = hours;
    refreshAtmosphere();
    update();
}

void SceneView3D::refreshAtmosphere()
{
    AtmoSample s;
    sampleAtmosphere(m_timeOfDay, s);
    // Normalize sun direction so the shader's Lambert dot is well-behaved
    // even when the LUT entry isn't a unit vector.
    float sl = std::sqrt(s.sunDir[0]*s.sunDir[0] + s.sunDir[1]*s.sunDir[1] + s.sunDir[2]*s.sunDir[2]);
    if (sl < 1e-4f) sl = 1.0f;
    m_sunDir[0] = s.sunDir[0] / sl;
    m_sunDir[1] = s.sunDir[1] / sl;
    m_sunDir[2] = s.sunDir[2] / sl;
    copyVec3(m_sunColor,     s.sunColor);
    copyVec3(m_zenithColor,  s.zenith);
    copyVec3(m_horizonColor, s.horizon);
    copyVec3(m_fogColor,     s.fogColor);
    copyVec3(m_skyGroundColor, s.skyGround);
    m_ambient  = s.ambient;
    m_fogStart = s.fogStart;
    m_fogEnd   = s.fogEnd;
    // Three-band hemisphere ambient derived from the same LUT entry:
    //   sky    = zenith    -- bright above
    //   horizon = horizon  -- equator band (already in m_horizonColor)
    //   ground = horizon * 0.45 -- warm dirt tone below
    // We multiply each band by the ambient scalar at the call site (shader)
    // so a single LUT slider drag still brightens / dims the whole scene.
    m_skyAmbient[0]     = s.zenith[0];
    m_skyAmbient[1]     = s.zenith[1];
    m_skyAmbient[2]     = s.zenith[2];
    m_horizonAmbient[0] = s.horizon[0];
    m_horizonAmbient[1] = s.horizon[1];
    m_horizonAmbient[2] = s.horizon[2];
    m_groundAmbient[0]  = s.horizon[0] * 0.45f;
    m_groundAmbient[1]  = s.horizon[1] * 0.45f;
    m_groundAmbient[2]  = s.horizon[2] * 0.45f;
    // Legacy-exp fog density: pick the rate so the fog mix factor reaches
    // ~63% of u_fogColor exactly at the LUT's fogEnd distance.  That makes
    // the LUT readable as "where the fog peaks" while the analytic curve
    // ramps softly from u_fogStart.  3.0 / span is conservative; retail
    // uses a tighter 4.6 / span (~99%) but the shorter ramps swallow
    // backgrounds even at midday.
    float const span = std::max(1.0f, s.fogEnd - s.fogStart);
    m_fogDensity = 3.0f / span;
    // Height-fog plane + per-unit-Z attenuation (wow.export height fog, ported
    // to TC +Z up).  Drives applyFog's altitude term so distant high terrain
    // de-fogs relative to valley floors; a falloff of 0 makes it a no-op.
    m_fogHeight        = s.fogHeight;
    m_fogHeightFalloff = s.fogHeightFalloff;
}

void SceneView3D::applyFogAndSunUniforms(QOpenGLShaderProgram& prog) const
{
    prog.setUniformValue("u_sunDir",   QVector3D(m_sunDir[0],   m_sunDir[1],   m_sunDir[2]));
    prog.setUniformValue("u_sunColor", QVector3D(m_sunColor[0], m_sunColor[1], m_sunColor[2]));
    prog.setUniformValue("u_ambient",  m_ambient);
    prog.setUniformValue("u_cameraPos", QVector3D(m_camX, m_camY, m_camZ));
    prog.setUniformValue("u_skyAmbient",     QVector3D(m_skyAmbient[0],     m_skyAmbient[1],     m_skyAmbient[2]));
    prog.setUniformValue("u_horizonAmbient", QVector3D(m_horizonAmbient[0], m_horizonAmbient[1], m_horizonAmbient[2]));
    prog.setUniformValue("u_groundAmbient",  QVector3D(m_groundAmbient[0],  m_groundAmbient[1],  m_groundAmbient[2]));
    if (m_fogEnabled)
    {
        prog.setUniformValue("u_fogStart",   m_fogStart);
        prog.setUniformValue("u_fogEnd",     m_fogEnd);
        prog.setUniformValue("u_fogDensity", m_fogDensity);
        prog.setUniformValue("u_fogColor", QVector3D(m_fogColor[0], m_fogColor[1], m_fogColor[2]));
        // Height-fog plane + falloff (no-op when falloff == 0).  Pushed in both
        // branches for symmetry with the density sentinel below.
        prog.setUniformValue("u_fogHeight",        m_fogHeight);
        prog.setUniformValue("u_fogHeightFalloff", m_fogHeightFalloff);
    }
    else
    {
        // Sentinel range + zero density; fog mix factor stays 0 inside the
        // shader so the helper is a no-op without per-pass branching.
        prog.setUniformValue("u_fogStart",   1.0e9f);
        prog.setUniformValue("u_fogEnd",     1.0e9f);
        prog.setUniformValue("u_fogDensity", 0.0f);
        prog.setUniformValue("u_fogColor", QVector3D(0.0f, 0.0f, 0.0f));
        // Zero the falloff so the altitude term is a guaranteed no-op too; push
        // u_fogHeight explicitly (rather than relying on a GL-default 0) to
        // match the density-sentinel house style.
        prog.setUniformValue("u_fogHeight",        m_fogHeight);
        prog.setUniformValue("u_fogHeightFalloff", 0.0f);
    }
}

void SceneView3D::drawSky(QMatrix4x4 const& proj, QMatrix4x4 const& view)
{
    // Disable depth write + face culling so the dome doesn't fight any
    // other pass for the framebuffer.  Restore the prior state at the
    // end.  We do keep depth TEST on (GL_LESS) so other geometry that
    // already wrote depth correctly hides the sky.
    GLboolean prevDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
    GLboolean const prevCull = glIsEnabled(GL_CULL_FACE);

    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    QMatrix4x4 const vp = proj * view;

    m_skyProgram->bind();
    m_skyProgram->setUniformValue(m_skyUMvp,     vp);
    m_skyProgram->setUniformValue(m_skyUCamPos,  QVector3D(m_camX, m_camY, m_camZ));
    m_skyProgram->setUniformValue(m_skyUZenith,  QVector3D(m_zenithColor[0],  m_zenithColor[1],  m_zenithColor[2]));
    m_skyProgram->setUniformValue(m_skyUHorizon, QVector3D(m_horizonColor[0], m_horizonColor[1], m_horizonColor[2]));
    if (m_skyUGround >= 0)
    {
        // Distinct below-horizon sky-ground band (not the lighting ground
        // ambient) -- decouples the lower sky from the dirt tone (Gap G1).
        m_skyProgram->setUniformValue(m_skyUGround,
            QVector3D(m_skyGroundColor[0], m_skyGroundColor[1], m_skyGroundColor[2]));
    }
    m_skyProgram->setUniformValue(m_skyUSunDir,  QVector3D(m_sunDir[0],   m_sunDir[1],   m_sunDir[2]));
    m_skyProgram->setUniformValue(m_skyUSunColor,QVector3D(m_sunColor[0], m_sunColor[1], m_sunColor[2]));

    m_skyVao.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_skyVertexCount);
    m_skyVao.release();
    m_skyProgram->release();

    if (prevCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glDepthMask(prevDepthMask);
}

// ============================================================================
// Frustum-cull helpers.  Identical math to the inline doodad-pass culler
// (see drawDoodads); pulled out here so the terrain / ADT / liquid / WMO
// passes can share the planes computed once per paint.
// ============================================================================

void SceneView3D::extractFrustumPlanes(QMatrix4x4 const& mvp)
{
    float const* p = mvp.constData(); // column-major; p[col * 4 + row].
    auto get = [&](int row, int col) { return p[col * 4 + row]; };
    auto setPlane = [&](FrustumPlane& pl, int row, float sign)
    {
        pl.a = get(3, 0) + sign * get(row, 0);
        pl.b = get(3, 1) + sign * get(row, 1);
        pl.c = get(3, 2) + sign * get(row, 2);
        pl.d = get(3, 3) + sign * get(row, 3);
        float len = std::sqrt(pl.a * pl.a + pl.b * pl.b + pl.c * pl.c);
        if (len > 1e-6f) { pl.a /= len; pl.b /= len; pl.c /= len; pl.d /= len; }
    };
    setPlane(m_frustumPlanes[0], 0,  1.0f); // left
    setPlane(m_frustumPlanes[1], 0, -1.0f); // right
    setPlane(m_frustumPlanes[2], 1,  1.0f); // bottom
    setPlane(m_frustumPlanes[3], 1, -1.0f); // top
    setPlane(m_frustumPlanes[4], 2,  1.0f); // near
    setPlane(m_frustumPlanes[5], 2, -1.0f); // far
}

bool SceneView3D::sphereInFrustum(float cx, float cy, float cz, float r) const
{
    for (int i = 0; i < 6; ++i)
    {
        FrustumPlane const& pl = m_frustumPlanes[i];
        float const dist = pl.a * cx + pl.b * cy + pl.c * cz + pl.d;
        if (dist < -r) return false;
    }
    return true;
}

// ============================================================================
// Async first-tile loading.  CASC + BLP decode run on QThreadPool workers
// and hand back CPU-only payloads; the GL thread drains the pending queue
// inside paintGL and turns the payloads into VAOs / textures.
// ============================================================================

namespace
{
// Small RAII bundle so the worker has both a back-pointer (raw, for
// queued invokeMethod) and a payload it builds before posting.
struct AdtLoadTask : public QRunnable
{
    AdtLoadTask() { setAutoDelete(true); }

    // Captured by value.  We deliberately copy the small structs the
    // worker needs; the SceneView3D pointer is the only shared resource.
    QPointer<world_editor::render::SceneView3D> view;
    world_editor::io::CascClient*      casc      = nullptr;
    world_editor::io::MapDb2Lookup*    mapDb2    = nullptr;
    uint32_t                           mapId     = 0;
    int                                gx        = 0;
    int                                gy        = 0;
    // Per-tile FileDataIDs sourced from the map's WDT MAID chunk (zero
    // when the WDT predates MAID -- the AdtReader then falls back to
    // path-based resolution).  Authoritative for modern WoW builds.
    uint32_t                           rootFdid = 0;
    uint32_t                           tex0Fdid = 0;
    bool                               verbose   = false; // settings-gated diag

    void run() override;
};

struct MinimapLoadTask : public QRunnable
{
    MinimapLoadTask() { setAutoDelete(true); }
    QPointer<world_editor::render::SceneView3D> view;
    world_editor::io::CascClient*      casc      = nullptr;
    world_editor::io::MapDb2Lookup*    mapDb2    = nullptr;
    uint32_t                           mapId     = 0;
    QString                            minimapDir;
    int                                gx        = 0;
    int                                gy        = 0;

    void run() override;
};

// STAGE B1: off-thread WMO root decode (CASC read + io::loadWmo).  Builds the
// render-ready CPU payload (interleaved verts / merged indices / per-submesh
// metadata incl. texture FDID+path + model bounding sphere); the GL drainer
// turns it into VBO/IBO/VAO and resolves textures.  No GL handles here.
struct WmoLoadTask : public QRunnable
{
    WmoLoadTask() { setAutoDelete(true); }
    QPointer<world_editor::render::SceneView3D> view;
    world_editor::io::CascClient* casc = nullptr;
    uint32_t                      fdid = 0;
    void run() override;
};

// STAGE B1: off-thread M2 doodad decode (CASC read + io::loadM2).
struct DoodadLoadTask : public QRunnable
{
    DoodadLoadTask() { setAutoDelete(true); }
    QPointer<world_editor::render::SceneView3D> view;
    world_editor::io::CascClient* casc = nullptr;
    uint32_t                      fdid = 0;
    void run() override;
};

} // namespace

io::Wdt const* SceneView3D::ensureWdt()
{
    if (!m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
        return nullptr;
    if (m_wdt && m_wdtMapId == m_heightmapMapId)
        return m_wdt.get();
    // Different map (or first load) -- drop stale cache.
    m_wdt.reset();
    m_wdtMapId = m_heightmapMapId;

    auto dirOpt = m_mapDb2->directoryFor(m_heightmapMapId);
    if (!dirOpt)
        return nullptr;

    auto fresh = std::make_unique<io::Wdt>();
    if (!io::loadWdt(*m_cascClient, *dirOpt, *fresh))
    {
        qInfo("[scene3d-adt] WDT load FAILED for mapId=%u dir='%s' -- "
              "will fall back to path resolution",
              m_heightmapMapId, dirOpt->c_str());
        return nullptr;
    }
    // Count populated MAID entries to give a one-liner sanity log.
    int maidCount = 0;
    for (auto const& e : fresh->maid) if (e.rootADT != 0) ++maidCount;
    qInfo("[scene3d-adt] WDT loaded mapId=%u dir='%s' mphd=0x%x hasMaid=%d "
          "tiles=%d maid_root_nonzero=%d",
          m_heightmapMapId, dirOpt->c_str(), fresh->mphdFlags,
          fresh->hasMaid() ? 1 : 0,
          (int)std::count(fresh->tileExists.begin(), fresh->tileExists.end(), true),
          maidCount);
    m_wdt = std::move(fresh);
    return m_wdt.get();
}

void SceneView3D::dropWdtCache()
{
    m_wdt.reset();
    m_wdtMapId = 0;
}

void SceneView3D::dispatchAdtTileLoads()
{
    if (!m_mesh.ok() || !m_cascClient || !m_cascClient->isOpen() || !m_mapDb2)
        return;

    constexpr int   CENTER_GRID_ID = 32;
    constexpr float TILE_SIZE = 533.3333f;

    // STREAMING: load only the tiles within a radius of the camera; the
    // whole continent is NOT held resident.  The camera's current tile:
    int const camGx = int(std::floor(CENTER_GRID_ID - m_camX / TILE_SIZE));
    int const camGy = int(std::floor(CENTER_GRID_ID - m_camY / TILE_SIZE));

    // Re-scan only when the camera crosses into a new tile -- enumerating a
    // ~19x19 neighbourhood every frame would be wasteful.  unloadFarAdtTiles()
    // (GL thread) handles eviction; this only handles loading.
    if (camGx == m_streamCamGx && camGy == m_streamCamGy)
        return;
    m_streamCamGx = camGx;
    m_streamCamGy = camGy;

    io::Wdt const* wdt = ensureWdt();

    // Load radius in tiles = draw radius + 1 ring so tiles are ready before
    // they enter the draw distance.
    int const loadRingTiles = int(std::ceil(renderRadiusYards() / TILE_SIZE)) + 1;

    QSet<uint32_t> seen;
    int dispatched = 0;
    for (int gx = camGx - loadRingTiles; gx <= camGx + loadRingTiles; ++gx)
    {
        if (gx < 0 || gx >= io::Wdt::kGridSize) continue;
        for (int gy = camGy - loadRingTiles; gy <= camGy + loadRingTiles; ++gy)
        {
            if (gy < 0 || gy >= io::Wdt::kGridSize) continue;
            // Only tiles that actually exist in the world.  For MAID-less
            // legacy maps the WDT pointer may be null -> attempt the load
            // and let AdtReader's path-fallback decide.
            if (wdt && wdt->hasMaid() &&
                wdt->entryFor(gx, gy).rootADT == 0 && !wdt->tileExistsAt(gx, gy))
                continue;

            uint32_t const key = (uint32_t(gy) << 16) | (uint32_t(gx) & 0xFFFFu);
            if (seen.contains(key)) continue;
            seen.insert(key);
            if (m_adtInFlight.contains(key)) continue;
            bool alreadyLoaded = false;
            for (AdtTileRender const& t : m_adtTerrainTiles)
                if (t.gx == gx && t.gy == gy) { alreadyLoaded = true; break; }
            if (alreadyLoaded) continue;
            m_adtInFlight.insert(key);
            AdtLoadTask* task = new AdtLoadTask;
            task->view   = this;
            task->casc   = m_cascClient;
            task->mapDb2 = m_mapDb2;
            task->mapId  = m_heightmapMapId;
            task->gx     = gx;
            task->gy     = gy;
            task->verbose = m_verboseLogging;
            if (wdt)
            {
                io::WdtMaidEntry const& e = wdt->entryFor(gx, gy);
                task->rootFdid = e.rootADT;
                task->tex0Fdid = e.tex0ADT;
            }
            QThreadPool::globalInstance()->start(task);
            ++dispatched;
        }
    }
    if (dispatched > 0)
        qInfo("[scene3d] stream: cam tile (%d,%d) dispatched %d ADT loads "
              "(ring=%d, resident=%zu)", camGx, camGy, dispatched,
              loadRingTiles, m_adtTerrainTiles.size());
}

// GL-thread tile eviction.  Frees the GL resources of any resident ADT tile
// whose center is beyond the unload radius (draw radius + hysteresis ring) of
// the camera, and drops it from the resident set + in-flight bookkeeping so it
// can be re-streamed if the camera returns.  Called from paintGL where the GL
// context is current.
void SceneView3D::unloadFarAdtTiles()
{
    if (m_adtTerrainTiles.empty()) return;
    constexpr float TILE_SIZE  = 533.3333f;
    constexpr int   kCenterGid = 32;
    // Unload further out than we load (hysteresis) so a tile sitting right at
    // the boundary doesn't thrash load/unload as the camera jitters.
    float const unloadRadius = renderRadiusYards() + 2.0f * TILE_SIZE;
    float const unloadR2 = unloadRadius * unloadRadius;

    std::size_t freed = 0;
    auto it = m_adtTerrainTiles.begin();
    while (it != m_adtTerrainTiles.end())
    {
        AdtTileRender& tile = *it;
        float const tcX = (kCenterGid - tile.gx) * TILE_SIZE - 0.5f * TILE_SIZE;
        float const tcY = (kCenterGid - tile.gy) * TILE_SIZE - 0.5f * TILE_SIZE;
        float const dx = tcX - m_camX, dy = tcY - m_camY;
        if (dx * dx + dy * dy <= unloadR2)
        {
            ++it;
            continue;
        }
        // Beyond the keep radius -- free GPU resources (layer textures are
        // owned by the shared cache, so only zero the handles; alpha + the
        // VBO/VAO are per-chunk and owned here).
        for (AdtChunkRender& ch : tile.chunks)
        {
            if (ch.vbo && ch.vbo->isCreated()) ch.vbo->destroy();
            if (ch.ebo && ch.ebo->isCreated()) ch.ebo->destroy();
            if (ch.vao && ch.vao->isCreated()) ch.vao->destroy();
            if (ch.alphaArray  != 0) glDeleteTextures(1, &ch.alphaArray);
            if (ch.heightArray != 0) glDeleteTextures(1, &ch.heightArray);
            ch.alphaArray = 0;
            ch.heightArray = 0;
            for (int i = 0; i < 8; ++i) ch.layerTex[i] = 0;
        }
        uint32_t const key = (uint32_t(tile.gy) << 16) | (uint32_t(tile.gx) & 0xFFFFu);
        m_adtInFlight.remove(key);
        it = m_adtTerrainTiles.erase(it);
        ++freed;
    }
    if (freed > 0 && m_verboseLogging)
        qInfo("[scene3d] stream: evicted %zu far ADT tiles (resident=%zu)",
              freed, m_adtTerrainTiles.size());
}

void AdtLoadTask::run()
{
    namespace io = world_editor::io;
    using world_editor::render::SceneView3D;

    SceneView3D::AdtTilePending pending;
    pending.gx = gx;
    pending.gy = gy;
    pending.ok = false;

    bool const haveCasc = casc && casc->isOpen() && mapDb2;
    auto dirOpt = haveCasc ? mapDb2->directoryFor(mapId) : std::optional<std::string>{};
    if (view.isNull() || !haveCasc || !dirOpt)
    {
        // Post failure so the GL thread can drop the in-flight key.
        qDebug("[scene3d-adt] tile (gx,gy)=(%d,%d): loadAdtTile precheck failed (view=%d casc=%d mapDb2=%d dir=%d)",
            gx, gy, view.isNull() ? 0 : 1, (casc && casc->isOpen()) ? 1 : 0,
            mapDb2 ? 1 : 0, dirOpt.has_value() ? 1 : 0);
    }
    else
    {
        io::AdtTile adt;
        bool const loadOk = io::loadAdtTile(*casc, *dirOpt, mapId, gx, gy, adt,
                                            rootFdid, tex0Fdid);
        qDebug("[scene3d-adt] tile (gx,gy)=(%d,%d): loadAdtTile result=%d chunks=%zu "
               "(rootFdid=%u tex0Fdid=%u)",
            gx, gy, loadOk ? 1 : 0, adt.chunks.size(), rootFdid, tex0Fdid);
        if (loadOk && !adt.chunks.empty())
        {
            // Sample the first chunk's layer/alpha state so the operator can
            // see whether the ADT actually carries texture info or whether
            // every chunk is degenerate.
            io::AdtChunk const& sample = adt.chunks.front();
            uint32_t fdids[4] = { 0, 0, 0, 0 };
            int alphaSizes[4] = { 0, 0, 0, 0 };
            int const lc = (std::min<int>)(int(sample.layers.size()), 4);
            for (int i = 0; i < lc; ++i)
            {
                fdids[i] = sample.layers[size_t(i)].textureFileDataId;
                alphaSizes[i] = int(sample.layers[size_t(i)].alpha.size());
            }
            qDebug("[scene3d-adt] tile (%d,%d) chunk0: layers=%d fdids=[%u %u %u %u] alphaSizes=[%d %d %d %d] hasMccv=%d",
                gx, gy, lc, fdids[0], fdids[1], fdids[2], fdids[3],
                alphaSizes[0], alphaSizes[1], alphaSizes[2], alphaSizes[3],
                sample.hasMccv ? 1 : 0);
        }
        // Verbose terrain-decode diagnostic.  Distinguishes the three
        // failure modes that all look like "scattered tiles": (a) a chunk
        // whose 81 V9 heights are all equal == MCVT was NOT found, so it
        // collapsed flat to baseZ; (b) wild min/max == garbage floats read
        // from a mis-located MCVT; (c) healthy varied terrain.  Per-tile
        // counts of flat vs varied chunks + the global Z range tell us at a
        // glance whether the MCVT sub-chunk locator is working for ALL 256
        // chunks or only some.
        if (verbose && loadOk && !adt.chunks.empty())
        {
            int flatChunks = 0, variedChunks = 0;
            int holedQuads = 0;        // total culled quads across the tile.
            int fullyHoledChunks = 0;  // chunks with all 64 quads holed.
            float tileMinZ =  std::numeric_limits<float>::infinity();
            float tileMaxZ = -std::numeric_limits<float>::infinity();
            float worstSpan = 0.0f;   // largest within-chunk height spread.
            int   worstChunkIdx = -1;
            // Roughness = worst single-vertex deviation from its 4-neighbour
            // average.  A smooth steep slope has LOW roughness (each vertex
            // follows its neighbours); a needle spike has HIGH roughness (one
            // vertex far off its neighbours).  This is what actually isolates
            // the spikes, which total-span does not.
            float worstRough = 0.0f;
            int   worstRoughChunkIdx = -1;
            for (size_t ci = 0; ci < adt.chunks.size(); ++ci)
            {
                io::AdtChunk const& ach = adt.chunks[ci];
                int chHoled = 0;
                for (int b = 0; b < 64; ++b)
                    if (ach.holesMask & (uint64_t(1) << b)) ++chHoled;
                holedQuads += chHoled;
                if (chHoled == 64) ++fullyHoledChunks;
                float cMin =  std::numeric_limits<float>::infinity();
                float cMax = -std::numeric_limits<float>::infinity();
                for (int i = 0; i < 81; ++i)
                {
                    float const h = ach.heights[size_t(i)];
                    if (h < cMin) cMin = h;
                    if (h > cMax) cMax = h;
                }
                // Per-vertex roughness over interior points (1..7).
                for (int y = 1; y <= 7; ++y)
                    for (int x = 1; x <= 7; ++x)
                    {
                        float const h  = ach.heights[size_t(y*9+x)];
                        float const avg = 0.25f * (ach.heights[size_t((y-1)*9+x)] +
                                                   ach.heights[size_t((y+1)*9+x)] +
                                                   ach.heights[size_t(y*9+x-1)] +
                                                   ach.heights[size_t(y*9+x+1)]);
                        float const rough = std::abs(h - avg);
                        if (rough > worstRough) { worstRough = rough; worstRoughChunkIdx = int(ci); }
                    }
                if (cMin <= cMax)
                {
                    if (cMin < tileMinZ) tileMinZ = cMin;
                    if (cMax > tileMaxZ) tileMaxZ = cMax;
                    float const span = cMax - cMin;
                    if (span > worstSpan) { worstSpan = span; worstChunkIdx = int(ci); }
                    if (span < 0.01f) ++flatChunks; else ++variedChunks;
                }
            }
            (void)worstChunkIdx;
            // Dump the worst chunk's full 9x9 V9 grid (only the very first
            // pathological tile, to avoid log spam) so we can see whether the
            // spikes are a single outlier vertex, an edge-overflow row, or
            // fully interleaved garbage.  Static one-shot: benign cross-thread
            // race -- worst case a couple of tiles dump.
            static bool s_worstChunkDumped = false;
            if (!s_worstChunkDumped && worstRough > 150.0f && worstRoughChunkIdx >= 0)
            {
                s_worstChunkDumped = true;
                io::AdtChunk const& wc = adt.chunks[size_t(worstRoughChunkIdx)];
                qInfo("[scene3d-adt-dump] tile (%d,%d) roughChunk idx=%d ix=%d iy=%d rough=%.1f span=%.1f -- 9x9 V9 grid:",
                    gx, gy, worstRoughChunkIdx, wc.ix, wc.iy, worstRough, worstSpan);
                for (int y = 0; y <= 8; ++y)
                {
                    qInfo("[scene3d-adt-dump]  row%d: %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
                        y,
                        wc.heights[y*9+0], wc.heights[y*9+1], wc.heights[y*9+2],
                        wc.heights[y*9+3], wc.heights[y*9+4], wc.heights[y*9+5],
                        wc.heights[y*9+6], wc.heights[y*9+7], wc.heights[y*9+8]);
                }
            }
            // Derived NW-corner world XY from the (gx,gy) we asked for vs
            // the ADT's own intrinsic position.  A mismatch == we loaded the
            // wrong tile's data (transposed / bad MAID index) and is the
            // smoking gun for tile-level shatter.
            constexpr float kTileSize = 533.3333f;
            float const derivedX = (32 - gx) * kTileSize;
            float const derivedY = (32 - gy) * kTileSize;
            float const dX = adt.hasIntrinsicPos ? (adt.intrinsicX0 - derivedX) : 0.0f;
            float const dY = adt.hasIntrinsicPos ? (adt.intrinsicY0 - derivedY) : 0.0f;
            qInfo("[scene3d-adt-v] tile (%d,%d): chunks=%zu flat=%d varied=%d "
                  "holedQuads=%d/%zu fullyHoled=%d "
                  "tileZ=[%.1f..%.1f] worstChunkSpan=%.1f worstRough=%.1f | deltaXY=(%.1f,%.1f)%s",
                gx, gy, adt.chunks.size(), flatChunks, variedChunks,
                holedQuads, adt.chunks.size() * 64, fullyHoledChunks,
                tileMinZ, tileMaxZ, worstSpan, worstRough, dX, dY,
                (adt.hasIntrinsicPos && (std::abs(dX) > 1.0f || std::abs(dY) > 1.0f))
                    ? " <-- WRONG-TILE/TRANSPOSE" : "");
        }
        if (loadOk && !adt.chunks.empty())
        {
            pending.chunks.reserve(adt.chunks.size());
            float wMinZ =  std::numeric_limits<float>::infinity();
            float wMaxZ = -std::numeric_limits<float>::infinity();
            for (io::AdtChunk const& ach : adt.chunks)
            {
                SceneView3D::AdtChunkCpuPayload cpu;
                cpu.layerCount = (std::min<int>)(int(ach.layers.size()), 8);
                if (cpu.layerCount == 0)
                {
                    pending.chunks.push_back(std::move(cpu));
                    continue;
                }
                // Texture resolution must happen on the GL thread (touches
                // TerrainTextureCache + GL state).  Record the per-layer
                // FDID / path indirectly by leaving layerTex zero; the GL
                // drainer re-resolves via the cache.  We pre-pack alpha
                // here because alpha bytes are pure CPU work.
                //
                // STAGE B: 7 contiguous R8 64x64 planes (one per layer 1..7).
                // Absent layers stay zero (=> 0 coverage), matching research
                // §3.3.  Layers whose diffuse fails to resolve are zeroed on the
                // GL thread (handles a MIDDLE hole, not just a trailing one).
                bool hasAnyAlpha = false;
                std::vector<uint8_t> planes(64 * 64 * 7, 0);
                for (int layerIdx = 1; layerIdx < cpu.layerCount; ++layerIdx)
                {
                    io::AdtLayer const& L = ach.layers[size_t(layerIdx)];
                    if (L.alpha.size() < 4096) continue;
                    uint8_t* dst = planes.data() + size_t(layerIdx - 1) * 4096;
                    std::memcpy(dst, L.alpha.data(), 4096);
                    hasAnyAlpha = true;
                }
                if (hasAnyAlpha)
                {
                    cpu.alphaPlanes = std::move(planes);
                    cpu.hasAlpha    = true;
                }
                // STAGE B: carry per-layer MTXP parallax metadata (pure CPU)
                // PLUS the diffuse texture refs, so the GL drainer resolves
                // textures without re-decoding the ADT off CASC.
                for (int i = 0; i < cpu.layerCount; ++i)
                {
                    io::AdtLayer const& L = ach.layers[size_t(i)];
                    cpu.layerScale[i]   = L.layerScale;
                    cpu.heightScale[i]  = L.heightScale;
                    cpu.heightOffset[i] = L.heightOffset;
                    cpu.layerFdid[i]    = L.textureFileDataId;
                    cpu.layerPath[i]    = L.textureBlpPath;
                    // Any layer carrying a non-zero MTXP heightScale enables the
                    // height-weighted blend for this chunk.
                    if (L.heightScale != 0.0f)
                        cpu.heightBlend = true;
                }
                // Build the FULL 145-vertex MCNK mesh (matches the client /
                // wow.export): 81 V9 outer-grid corners + 64 V8 inner-grid
                // cell centres, with 4 triangles fanned from each V8 centre to
                // its 4 V9 corners (256 tris/chunk).  Using only the V9 grid
                // (2 tris/cell) gives a coarse surface with cracks; the V8
                // centres carry real height detail.  Indexed so the 145 verts
                // aren't duplicated per-triangle.
                constexpr float kChunkSize = 33.33333f;
                constexpr float kUnitSize  = kChunkSize / 8.0f;
                float const chunkMaxX = ach.minX + kChunkSize;
                float const chunkMaxY = ach.minY + kChunkSize;
                cpu.verts.resize(145);
                // V9 outer grid: vertex index = y*9 + x, world from (y,x).
                for (int y = 0; y <= 8; ++y)
                    for (int x = 0; x <= 8; ++x)
                    {
                        SceneView3D::AdtTerrainVertexPub& v = cpu.verts[size_t(y * 9 + x)];
                        v.x = chunkMaxX - float(y) * kUnitSize;
                        v.y = chunkMaxY - float(x) * kUnitSize;
                        v.z = ach.heights[size_t(y * 9 + x)];
                        v.u = float(x) / 8.0f;
                        v.v = float(y) / 8.0f;
                        v.nx = ach.normals[size_t(y * 9 + x)][0];
                        v.ny = ach.normals[size_t(y * 9 + x)][1];
                        v.nz = ach.normals[size_t(y * 9 + x)][2];
                        v.r = ach.mccv[size_t(y * 9 + x)][0];
                        v.g = ach.mccv[size_t(y * 9 + x)][1];
                        v.b = ach.mccv[size_t(y * 9 + x)][2];
                        v.a = ach.mccv[size_t(y * 9 + x)][3];
                    }
                // V8 inner grid: vertex index = 81 + r*8 + c, at the cell
                // centre (offset +0.5 unit in both axes from corner (r,c)).
                for (int r = 0; r < 8; ++r)
                    for (int c = 0; c < 8; ++c)
                    {
                        SceneView3D::AdtTerrainVertexPub& v = cpu.verts[size_t(81 + r * 8 + c)];
                        v.x = chunkMaxX - (float(r) + 0.5f) * kUnitSize;
                        v.y = chunkMaxY - (float(c) + 0.5f) * kUnitSize;
                        v.z = ach.heightsV8[size_t(r * 8 + c)];
                        v.u = (float(c) + 0.5f) / 8.0f;
                        v.v = (float(r) + 0.5f) / 8.0f;
                        v.nx = ach.normalsV8[size_t(r * 8 + c)][0];
                        v.ny = ach.normalsV8[size_t(r * 8 + c)][1];
                        v.nz = ach.normalsV8[size_t(r * 8 + c)][2];
                        // MCCV: average the 4 corners (V8 has its own MCCV in
                        // the file, but averaging the corners is visually fine
                        // and avoids carrying a second colour array).
                        int const tl = r * 9 + c, tr = r * 9 + c + 1;
                        int const bl = (r + 1) * 9 + c, br = (r + 1) * 9 + c + 1;
                        auto avg4 = [&](int ch4) -> uint8_t {
                            return uint8_t((int(ach.mccv[size_t(tl)][ch4]) + ach.mccv[size_t(tr)][ch4]
                                          + ach.mccv[size_t(bl)][ch4] + ach.mccv[size_t(br)][ch4]) / 4);
                        };
                        v.r = avg4(0); v.g = avg4(1); v.b = avg4(2); v.a = avg4(3);
                    }
                // Indices: per cell (r,c), fan 4 tris centre->corners (skip holes).
                cpu.indices.reserve(256 * 3);
                for (int r = 0; r < 8; ++r)
                    for (int c = 0; c < 8; ++c)
                    {
                        if (ach.holesMask & (uint64_t(1) << (r * 8 + c)))
                            continue;
                        uint16_t const center = uint16_t(81 + r * 8 + c);
                        uint16_t const tl = uint16_t(r * 9 + c);
                        uint16_t const tr = uint16_t(r * 9 + c + 1);
                        uint16_t const bl = uint16_t((r + 1) * 9 + c);
                        uint16_t const br = uint16_t((r + 1) * 9 + c + 1);
                        cpu.indices.push_back(center); cpu.indices.push_back(tl); cpu.indices.push_back(tr);
                        cpu.indices.push_back(center); cpu.indices.push_back(tr); cpu.indices.push_back(br);
                        cpu.indices.push_back(center); cpu.indices.push_back(br); cpu.indices.push_back(bl);
                        cpu.indices.push_back(center); cpu.indices.push_back(bl); cpu.indices.push_back(tl);
                    }
                for (auto const& v : cpu.verts)
                {
                    if (v.z < wMinZ) wMinZ = v.z;
                    if (v.z > wMaxZ) wMaxZ = v.z;
                }
                pending.chunks.push_back(std::move(cpu));
            }
            if (wMinZ <= wMaxZ) { pending.tileMinZ = wMinZ; pending.tileMaxZ = wMaxZ; }
            pending.ok = !pending.chunks.empty();
        }
    }

    // Post back to the GL thread via a queued lambda.  Capturing a
    // QPointer<> means a SceneView3D destroyed mid-flight resolves to null
    // when the GUI thread dispatches the slot, and we silently drop the
    // payload instead of dereferencing freed memory.
    QPointer<SceneView3D> guardedView = view;
    QMetaObject::invokeMethod(QCoreApplication::instance(),
        [guardedView, pending = std::move(pending)]() mutable
        {
            if (!guardedView) return;
            guardedView->enqueueAdtPending(std::move(pending));
            guardedView->update();
        },
        Qt::QueuedConnection);
}

void SceneView3D::enqueueAdtPending(AdtTilePending pending)
{
    std::lock_guard<std::mutex> g(m_adtPendingMutex);
    m_adtPending.push_back(std::move(pending));
}

void SceneView3D::enqueueMinimapPending(MinimapPending pending)
{
    std::lock_guard<std::mutex> g(m_minimapPendingMutex);
    m_minimapPending.push_back(std::move(pending));
}

void SceneView3D::drainPendingAdtUploads(int maxThisFrame)
{
    std::vector<AdtTilePending> batch;
    {
        std::lock_guard<std::mutex> g(m_adtPendingMutex);
        if (m_adtPending.empty()) return;
        int const take = std::min<int>(maxThisFrame, int(m_adtPending.size()));
        batch.reserve(take);
        for (int i = 0; i < take; ++i)
            batch.push_back(std::move(m_adtPending[size_t(i)]));
        m_adtPending.erase(m_adtPending.begin(), m_adtPending.begin() + take);
    }
    if (!m_terrainTextureCache && m_cascClient)
        m_terrainTextureCache = std::make_unique<TerrainTextureCache>(m_cascClient);

    // PERF: the worker carried each layer's diffuse FDID / BLP path in the
    // payload, so there is NO GL-thread ADT re-decode here any more -- texture
    // resolution (FDID -> GL handle via the shared cache) is the only GL work.
    for (AdtTilePending& pending : batch)
    {
        uint32_t const key = (uint32_t(pending.gy) << 16)
                           | (uint32_t(pending.gx) & 0xFFFFu);
        m_adtInFlight.remove(key);
        if (!pending.ok)
        {
            qDebug("[scene3d-adt] tile (%d,%d): worker reported !ok; dropping",
                pending.gx, pending.gy);
            continue;
        }

        AdtTileRender tile;
        tile.gx = pending.gx;
        tile.gy = pending.gy;
        tile.loadAttempted = true;
        tile.loaded = false;
        tile.chunks.reserve(pending.chunks.size());
        // Tile Z extent came from the worker (no re-decode to recompute it).
        float tileMinZ = pending.tileMinZ;
        float tileMaxZ = pending.tileMaxZ;

        int firstTexedChunk = -1;
        int chunksWithTex0 = 0;
        int chunksWithAlpha = 0;
        int totalGpuTextures = 0;

        size_t const chunkN = pending.chunks.size();
        for (size_t ci = 0; ci < chunkN; ++ci)
        {
            AdtChunkCpuPayload& cpu = pending.chunks[ci];
            AdtChunkRender ch;
            ch.layerCount = cpu.layerCount;
            // STAGE B: resolve diffuse (deduped) + build the R8 alpha array via
            // the shared helper (same path the sync loader uses).  Texture refs
            // come straight from the worker payload -- no ADT re-decode.
            bool const ok = buildAdtChunkTextures(cpu.layerCount, cpu.layerFdid, cpu.layerPath,
                cpu.alphaPlanes, cpu.hasAlpha,
                cpu.layerScale, cpu.heightScale, cpu.heightOffset, cpu.heightBlend, ch);
            if (!ok)
            {
                tile.chunks.push_back(std::move(ch));
                continue;
            }
            ++chunksWithTex0;
            if (firstTexedChunk < 0) firstTexedChunk = int(ci);
            totalGpuTextures += ch.slotCount;
            if (ch.alphaArray != 0) ++chunksWithAlpha;

            if (!cpu.verts.empty())
            {
                ch.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
                ch.vao = std::make_unique<QOpenGLVertexArrayObject>();
                ch.vertexCount = static_cast<GLsizei>(cpu.verts.size());
                ch.vbo->create();
                ch.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
                ch.vao->create();
                ch.vao->bind();
                ch.vbo->bind();
                ch.vbo->allocate(cpu.verts.data(),
                    int(cpu.verts.size() * sizeof(SceneView3D::AdtTerrainVertexPub)));
                static_assert(sizeof(SceneView3D::AdtTerrainVertexPub) == sizeof(AdtTerrainVertex),
                    "AdtTerrainVertexPub layout must match private AdtTerrainVertex");
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AdtTerrainVertex),
                    reinterpret_cast<void*>(offsetof(AdtTerrainVertex, x)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(AdtTerrainVertex),
                    reinterpret_cast<void*>(offsetof(AdtTerrainVertex, u)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(AdtTerrainVertex),
                    reinterpret_cast<void*>(offsetof(AdtTerrainVertex, nx)));
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(AdtTerrainVertex),
                    reinterpret_cast<void*>(offsetof(AdtTerrainVertex, r)));
                // Index buffer (bound while the VAO is bound so the element-
                // array binding is captured in VAO state).
                if (!cpu.indices.empty())
                {
                    ch.ebo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
                    ch.indexCount = static_cast<GLsizei>(cpu.indices.size());
                    ch.ebo->create();
                    ch.ebo->setUsagePattern(QOpenGLBuffer::StaticDraw);
                    ch.ebo->bind();
                    ch.ebo->allocate(cpu.indices.data(),
                        int(cpu.indices.size() * sizeof(uint16_t)));
                }
                ch.vbo->release();
                ch.vao->release();
                if (ch.ebo) ch.ebo->release();
                tile.loaded = true;
            }
            tile.chunks.push_back(std::move(ch));
        }
        qDebug("[scene3d-adt] tile (%d,%d) GL upload: chunks=%zu texedChunks=%d alphaChunks=%d gpuTex=%d loaded=%d firstTexedChunk=%d",
            pending.gx, pending.gy, chunkN, chunksWithTex0, chunksWithAlpha,
            totalGpuTextures, tile.loaded ? 1 : 0, firstTexedChunk);
        if (tileMinZ <= tileMaxZ) { tile.minZ = tileMinZ; tile.maxZ = tileMaxZ; }
        // Z-SOURCE DIAGNOSTIC (WE_ZDIAG): compare the CASC-ADT geometry Z range
        // for this tile against the .map heightmap (heightAt) sampled at the
        // tile centre + corners.  If they diverge, the lit-fallback (built from
        // .map) and the streamed ADT (CASC) sit at different heights -> the
        // "hill pops/expands" pop + seam.  Same-source agreement => the pop is
        // load-order/coarseness, not stale data.
        if (m_mapCache && tileMinZ <= tileMaxZ
            && qEnvironmentVariableIsSet("WE_ZDIAG"))
        {
            constexpr float TS = 533.3333f;
            float const wMaxX = (32 - pending.gx) * TS;
            float const wMaxY = (32 - pending.gy) * TS;
            float const cx = wMaxX - TS * 0.5f, cy = wMaxY - TS * 0.5f;
            float const mapCenter = m_mapCache->heightAt(m_heightmapMapId, cx, cy);
            float const mapNW = m_mapCache->heightAt(m_heightmapMapId, wMaxX - 1.0f, wMaxY - 1.0f);
            qWarning("[WE_ZDIAG] tile(%d,%d) ADT Z=[%.1f..%.1f] | .map center=%.1f NW=%.1f | dCenter=%.1f",
                pending.gx, pending.gy, tileMinZ, tileMaxZ, mapCenter, mapNW,
                mapCenter - 0.5f * (tileMinZ + tileMaxZ));
        }
        if (tile.loaded)
            m_adtTerrainTiles.push_back(std::move(tile));
    }
}

void SceneView3D::dispatchMinimapLoadIfNeeded(int gx, int gy)
{
    uint32_t const key = (uint32_t(gy) << 16) | (uint32_t(gx) & 0xFFFFu);
    if (m_minimapTextures.find(key) != m_minimapTextures.end()) return;
    if (m_minimapInFlight.contains(key)) return;
    if ((!m_cascClient || !m_cascClient->isOpen() || !m_mapDb2) && m_minimapDir.isEmpty())
    {
        m_minimapTextures[key] = 0;
        return;
    }
    m_minimapInFlight.insert(key);
    MinimapLoadTask* task = new MinimapLoadTask;
    task->view       = this;
    task->casc       = m_cascClient;
    task->mapDb2     = m_mapDb2;
    task->mapId      = m_heightmapMapId;
    task->minimapDir = m_minimapDir;
    task->gx         = gx;
    task->gy         = gy;
    QThreadPool::globalInstance()->start(task);
}

void MinimapLoadTask::run()
{
    namespace io = world_editor::io;
    using world_editor::render::SceneView3D;

    SceneView3D::MinimapPending pending;
    pending.gx = gx;
    pending.gy = gy;
    pending.ok = false;

    if (view.isNull()) return;

    QImage img;
    bool found = false;

    if (!minimapDir.isEmpty())
    {
        auto makePng = [&](int a, int b)
        {
            QStringList paths;
            paths << QStringLiteral("%1/%2/map%3_%4.png").arg(minimapDir).arg(mapId).arg(a).arg(b);
            paths << QStringLiteral("%1/map%2/map%3_%4.png").arg(minimapDir).arg(mapId).arg(a).arg(b);
            paths << QStringLiteral("%1/%2/%3_%4.png").arg(minimapDir).arg(mapId).arg(a).arg(b);
            return paths;
        };
        QStringList candidates = makePng(gy, gx);
        candidates += makePng(gx, gy);
        for (QString const& candidate : candidates)
            if (img.load(candidate)) { found = true; break; }
    }
    if (!found && casc && mapDb2 && casc->isOpen())
    {
        if (auto dir = mapDb2->directoryFor(mapId))
        {
            auto makeBlp = [&](int a, int b)
            {
                std::string p = "world/minimaps/" + *dir + "/map"
                              + std::to_string(a) + "_" + std::to_string(b) + ".blp";
                return p;
            };
            auto tryLoad = [&](std::string const& path) -> bool
            {
                std::vector<uint8_t> blob;
                if (!casc->readByPath(path, blob)) return false;
                io::BlpImage decoded;
                if (!io::decodeBlp(blob, decoded) || decoded.width <= 0 || decoded.height <= 0)
                    return false;
                img = QImage(decoded.rgba.data(), decoded.width, decoded.height,
                             decoded.width * 4, QImage::Format_RGBA8888).copy();
                return true;
            };
            if (tryLoad(makeBlp(gy, gx)))      found = true;
            else if (tryLoad(makeBlp(gx, gy))) found = true;
        }
    }

    if (found && !img.isNull())
    {
        pending.img = img.convertToFormat(QImage::Format_ARGB32);
        pending.ok  = true;
    }

    QPointer<SceneView3D> guardedView = view;
    QMetaObject::invokeMethod(QCoreApplication::instance(),
        [guardedView, pending = std::move(pending)]() mutable
        {
            if (!guardedView) return;
            guardedView->enqueueMinimapPending(std::move(pending));
            guardedView->update();
        },
        Qt::QueuedConnection);
}

// ============================================================================
// STAGE B1: async WMO / M2 mesh streaming.  Worker decodes geometry off the GL
// thread; the GL drainer uploads a budgeted few payloads per frame + resolves
// textures.  Structurally identical to the ADT pipeline above.
// ============================================================================

void WmoLoadTask::run()
{
    namespace io = world_editor::io;
    using world_editor::render::SceneView3D;

    SceneView3D::WmoPending pending;
    pending.fdid = fdid;
    pending.ok   = false;

    if (view.isNull() || !casc || !casc->isOpen())
    {
        // Still post back so the GL thread drops the in-flight key.
    }
    else
    {
        io::WmoModel model;
        if (io::loadWmo(*casc, fdid, model) && !model.groups.empty())
        {
            // Flatten every group into one interleaved VBO + one IBO -- the
            // SAME merge the old synchronous ensureWmoModelLoaded did, but on
            // the worker.  Per-submesh indexStart/indexCount are group-local,
            // so each group's range is shifted by the running iBase and index
            // VALUES bumped by the running vertex base (vBase).
            uint32_t vBase = 0;
            for (io::WmoGroupMesh const& g : model.groups)
            {
                uint32_t const gVerts = uint32_t(g.vertices.size() / 8);
                bool const haveCol = (g.colours.size() / 4) >= gVerts;
                for (uint32_t v = 0; v < gVerts; ++v)
                {
                    SceneView3D::WmoCpuVertex wv{};
                    wv.x  = g.vertices[v * 8 + 0];
                    wv.y  = g.vertices[v * 8 + 1];
                    wv.z  = g.vertices[v * 8 + 2];
                    wv.nx = g.vertices[v * 8 + 3];
                    wv.ny = g.vertices[v * 8 + 4];
                    wv.nz = g.vertices[v * 8 + 5];
                    wv.u  = g.vertices[v * 8 + 6];
                    wv.v  = g.vertices[v * 8 + 7];
                    if (haveCol)
                    {
                        wv.r = g.colours[v * 4 + 0];
                        wv.g = g.colours[v * 4 + 1];
                        wv.b = g.colours[v * 4 + 2];
                        wv.a = g.colours[v * 4 + 3];
                    }
                    else
                    {
                        wv.r = wv.g = wv.b = wv.a = 255;
                    }
                    pending.verts.push_back(wv);
                }
                uint32_t const iBase = uint32_t(pending.indices.size());
                for (uint32_t ix : g.indices)
                    pending.indices.push_back(ix + vBase);
                for (io::WmoSubMesh const& sm : g.subMeshes)
                {
                    SceneView3D::WmoCpuSubMesh d;
                    d.indexStart        = iBase + sm.indexStart;
                    d.indexCount        = sm.indexCount;
                    d.blendMode         = sm.blendMode;
                    d.interior          = sm.interior;
                    d.textureFileDataId = sm.textureFileDataId;
                    d.texturePath       = sm.texturePath;
                    pending.subMeshes.push_back(std::move(d));
                }
                vBase += gVerts;
            }
            if (!pending.verts.empty() && !pending.indices.empty() && !pending.subMeshes.empty())
            {
                float const cx = 0.5f * (model.bboxMin[0] + model.bboxMax[0]);
                float const cy = 0.5f * (model.bboxMin[1] + model.bboxMax[1]);
                float const cz = 0.5f * (model.bboxMin[2] + model.bboxMax[2]);
                float const dx = 0.5f * (model.bboxMax[0] - model.bboxMin[0]);
                float const dy = 0.5f * (model.bboxMax[1] - model.bboxMin[1]);
                float const dz = 0.5f * (model.bboxMax[2] - model.bboxMin[2]);
                pending.centerX = cx; pending.centerY = cy; pending.centerZ = cz;
                pending.radius  = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (pending.radius < 0.5f) pending.radius = 0.5f;
                pending.ok = true;
            }
        }
    }

    QPointer<SceneView3D> guardedView = view;
    QMetaObject::invokeMethod(QCoreApplication::instance(),
        [guardedView, pending = std::move(pending)]() mutable
        {
            if (!guardedView) return;
            guardedView->enqueueWmoPending(std::move(pending));
            guardedView->update();
        },
        Qt::QueuedConnection);
}

void DoodadLoadTask::run()
{
    namespace io = world_editor::io;
    using world_editor::render::SceneView3D;

    SceneView3D::DoodadPending pending;
    pending.fdid = fdid;
    pending.ok   = false;

    if (view.isNull() || !casc || !casc->isOpen())
    {
        // Post back so the in-flight key is dropped.
    }
    else
    {
        io::M2Mesh mesh;
        if (io::loadM2(*casc, fdid, mesh)
            && !mesh.vertices.empty() && !mesh.indices.empty() && !mesh.subMeshes.empty())
        {
            // M2Mesh::vertices is already interleaved 10 floats
            // (pos3/nrm3/uv0_2/uv1_2) -- the DoodadCpuVertex layout matches it
            // 1:1, so a flat copy.
            uint32_t const vCount = uint32_t(mesh.vertices.size() / 10);
            pending.verts.resize(vCount);
            std::memcpy(pending.verts.data(), mesh.vertices.data(),
                        size_t(vCount) * 10 * sizeof(float));
            pending.indices = std::move(mesh.indices);
            for (io::M2SubMesh const& sm : mesh.subMeshes)
            {
                SceneView3D::DoodadCpuSubMesh d;
                d.indexStart         = sm.indexStart;
                d.indexCount         = sm.indexCount;
                d.blendMode          = sm.blendMode;
                d.textureFileDataId  = sm.textureFileDataId;
                d.texturePath        = sm.texturePath;
                d.textureFileDataId2 = sm.textureFileDataId2;
                d.texturePath2       = sm.texturePath2;
                d.combinerId         = sm.combinerId;
                pending.subMeshes.push_back(std::move(d));
            }
            float const cx = 0.5f * (mesh.bboxMinX + mesh.bboxMaxX);
            float const cy = 0.5f * (mesh.bboxMinY + mesh.bboxMaxY);
            float const cz = 0.5f * (mesh.bboxMinZ + mesh.bboxMaxZ);
            float const dx = 0.5f * (mesh.bboxMaxX - mesh.bboxMinX);
            float const dy = 0.5f * (mesh.bboxMaxY - mesh.bboxMinY);
            float const dz = 0.5f * (mesh.bboxMaxZ - mesh.bboxMinZ);
            pending.centerX = cx; pending.centerY = cy; pending.centerZ = cz;
            pending.radius  = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (pending.radius < 0.5f) pending.radius = 0.5f;
            pending.ok = true;
        }
    }

    QPointer<SceneView3D> guardedView = view;
    QMetaObject::invokeMethod(QCoreApplication::instance(),
        [guardedView, pending = std::move(pending)]() mutable
        {
            if (!guardedView) return;
            guardedView->enqueueDoodadPending(std::move(pending));
            guardedView->update();
        },
        Qt::QueuedConnection);
}

void SceneView3D::enqueueWmoPending(WmoPending pending)
{
    std::lock_guard<std::mutex> g(m_wmoPendingMutex);
    m_wmoPending.push_back(std::move(pending));
}

void SceneView3D::enqueueDoodadPending(DoodadPending pending)
{
    std::lock_guard<std::mutex> g(m_doodadPendingMutex);
    m_doodadPending.push_back(std::move(pending));
}

// Concurrency caps mirror the reference renderer (heavier asset = fewer in
// flight): WMO 1, M2 2.  Guard BEFORE enqueuing so a city's worth of fresh
// FDIDs spreads across frames instead of saturating the pool.
void SceneView3D::dispatchWmoLoad(uint32_t fdid)
{
    if (fdid == 0 || !m_cascClient || !m_cascClient->isOpen())
        return;
    if (m_wmoModels.find(fdid) != m_wmoModels.end()) return; // loaded or failed.
    if (m_wmoInFlight.contains(fdid)) return;
    constexpr int kMaxWmoConcurrent = 1;
    if (m_wmoInFlight.size() >= kMaxWmoConcurrent) return;
    m_wmoInFlight.insert(fdid);
    WmoLoadTask* task = new WmoLoadTask;
    task->view = this;
    task->casc = m_cascClient;
    task->fdid = fdid;
    QThreadPool::globalInstance()->start(task);
}

void SceneView3D::dispatchDoodadLoad(uint32_t fdid)
{
    if (fdid == 0 || !m_cascClient || !m_cascClient->isOpen())
        return;
    if (m_doodadMeshes.find(fdid) != m_doodadMeshes.end()) return;
    if (m_doodadInFlight.contains(fdid)) return;
    constexpr int kMaxM2Concurrent = 2;
    if (m_doodadInFlight.size() >= kMaxM2Concurrent) return;
    m_doodadInFlight.insert(fdid);
    DoodadLoadTask* task = new DoodadLoadTask;
    task->view = this;
    task->casc = m_cascClient;
    task->fdid = fdid;
    QThreadPool::globalInstance()->start(task);
}

bool SceneView3D::uploadWmoPayload(WmoPending& p)
{
    // Re-check membership: the FDID may have been evicted (map switch -> the
    // model map cleared) while the worker was awaiting CASC.  Drop the payload
    // rather than resurrect a stale model.  (We always create the entry below
    // so a failed payload still records loadAttempted and stops re-dispatch.)
    WmoGpuModel& gpu = m_wmoModels[p.fdid];
    gpu.loadAttempted = true;
    if (!p.ok || p.verts.empty() || p.indices.empty() || p.subMeshes.empty())
        return false;
    if (!m_terrainTextureCache && m_cascClient)
        m_terrainTextureCache = std::make_unique<TerrainTextureCache>(m_cascClient);

    gpu.centerX = p.centerX; gpu.centerY = p.centerY; gpu.centerZ = p.centerZ;
    gpu.radius  = p.radius;

    gpu.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    gpu.ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    gpu.vao = std::make_unique<QOpenGLVertexArrayObject>();
    gpu.vbo->create();
    gpu.ibo->create();
    gpu.vao->create();
    gpu.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    gpu.ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);

    gpu.vao->bind();
    gpu.vbo->bind();
    gpu.vbo->allocate(p.verts.data(), int(p.verts.size() * sizeof(WmoCpuVertex)));
    gpu.ibo->bind();
    gpu.ibo->allocate(p.indices.data(), int(p.indices.size() * sizeof(uint32_t)));
    constexpr int kStride = int(sizeof(WmoCpuVertex));   // 36 bytes (== WmoVertex).
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(offsetof(WmoCpuVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(offsetof(WmoCpuVertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kStride,
        reinterpret_cast<void*>(offsetof(WmoCpuVertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, kStride,
        reinterpret_cast<void*>(offsetof(WmoCpuVertex, r)));
    gpu.vao->release();
    gpu.vbo->release();
    gpu.ibo->release();

    gpu.vertexCount = GLsizei(p.verts.size());
    gpu.indexCount  = GLsizei(p.indices.size());
    gpu.subMeshes.reserve(p.subMeshes.size());
    for (WmoCpuSubMesh const& cs : p.subMeshes)
    {
        WmoGpuSubMesh d;
        d.indexStart = cs.indexStart;
        d.indexCount = cs.indexCount;
        d.blendMode  = cs.blendMode;
        d.interior   = cs.interior;
        if (m_terrainTextureCache)
        {
            if (cs.textureFileDataId != 0)
                d.texture = m_terrainTextureCache->textureForFileDataId(cs.textureFileDataId, *this);
            if (d.texture == 0 && !cs.texturePath.empty())
                d.texture = m_terrainTextureCache->textureForPath(cs.texturePath, *this);
        }
        gpu.subMeshes.push_back(d);
    }
    gpu.loaded = true;
    return true;
}

bool SceneView3D::uploadDoodadPayload(DoodadPending& p)
{
    DoodadGpuMesh& gpu = m_doodadMeshes[p.fdid];
    gpu.loadAttempted = true;
    if (!p.ok || p.verts.empty() || p.indices.empty() || p.subMeshes.empty())
        return false;
    if (!m_terrainTextureCache && m_cascClient)
        m_terrainTextureCache = std::make_unique<TerrainTextureCache>(m_cascClient);

    gpu.centerX = p.centerX; gpu.centerY = p.centerY; gpu.centerZ = p.centerZ;
    gpu.radius  = p.radius;

    gpu.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    gpu.ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    gpu.vao = std::make_unique<QOpenGLVertexArrayObject>();
    gpu.vbo->create();
    gpu.ibo->create();
    gpu.vao->create();
    gpu.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    gpu.ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);

    gpu.vao->bind();
    gpu.vbo->bind();
    gpu.vbo->allocate(p.verts.data(), int(p.verts.size() * sizeof(DoodadCpuVertex)));
    gpu.ibo->bind();
    gpu.ibo->allocate(p.indices.data(), int(p.indices.size() * sizeof(uint32_t)));
    constexpr int kStride = int(sizeof(DoodadCpuVertex));  // 40 bytes (== 10 floats).
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(sizeof(float) * 6));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<void*>(sizeof(float) * 8));
    gpu.vao->release();
    gpu.vbo->release();
    gpu.ibo->release();

    gpu.vertexCount = GLsizei(p.verts.size());
    gpu.indexCount  = GLsizei(p.indices.size());
    gpu.subMeshes.reserve(p.subMeshes.size());
    for (DoodadCpuSubMesh const& cs : p.subMeshes)
    {
        DoodadGpuSubMesh d;
        d.indexStart = cs.indexStart;
        d.indexCount = cs.indexCount;
        d.blendMode  = cs.blendMode;
        d.combinerId = cs.combinerId;
        if (m_terrainTextureCache)
        {
            if (cs.textureFileDataId != 0)
                d.texture = m_terrainTextureCache->textureForFileDataId(cs.textureFileDataId, *this);
            if (d.texture == 0 && !cs.texturePath.empty())
                d.texture = m_terrainTextureCache->textureForPath(cs.texturePath, *this);
            if (cs.textureFileDataId2 != 0)
                d.texture2 = m_terrainTextureCache->textureForFileDataId(cs.textureFileDataId2, *this);
            if (d.texture2 == 0 && !cs.texturePath2.empty())
                d.texture2 = m_terrainTextureCache->textureForPath(cs.texturePath2, *this);
        }
        gpu.subMeshes.push_back(d);
    }
    gpu.loaded = true;
    return true;
}

void SceneView3D::drainPendingWmoUploads(int maxThisFrame)
{
    std::vector<WmoPending> batch;
    {
        std::lock_guard<std::mutex> g(m_wmoPendingMutex);
        if (m_wmoPending.empty()) return;
        int const take = std::min<int>(maxThisFrame, int(m_wmoPending.size()));
        batch.reserve(take);
        for (int i = 0; i < take; ++i)
            batch.push_back(std::move(m_wmoPending[size_t(i)]));
        m_wmoPending.erase(m_wmoPending.begin(), m_wmoPending.begin() + take);
    }
    for (WmoPending& p : batch)
    {
        m_wmoInFlight.remove(p.fdid);
        uploadWmoPayload(p);
    }
}

void SceneView3D::drainPendingDoodadUploads(int maxThisFrame)
{
    std::vector<DoodadPending> batch;
    {
        std::lock_guard<std::mutex> g(m_doodadPendingMutex);
        if (m_doodadPending.empty()) return;
        int const take = std::min<int>(maxThisFrame, int(m_doodadPending.size()));
        batch.reserve(take);
        for (int i = 0; i < take; ++i)
            batch.push_back(std::move(m_doodadPending[size_t(i)]));
        m_doodadPending.erase(m_doodadPending.begin(), m_doodadPending.begin() + take);
    }
    for (DoodadPending& p : batch)
    {
        m_doodadInFlight.remove(p.fdid);
        uploadDoodadPayload(p);
    }
}

void SceneView3D::drainPendingMinimapUploads(int maxThisFrame)
{
    std::vector<MinimapPending> batch;
    {
        std::lock_guard<std::mutex> g(m_minimapPendingMutex);
        if (m_minimapPending.empty()) return;
        int const take = std::min<int>(maxThisFrame, int(m_minimapPending.size()));
        batch.reserve(take);
        for (int i = 0; i < take; ++i)
            batch.push_back(std::move(m_minimapPending[size_t(i)]));
        m_minimapPending.erase(m_minimapPending.begin(), m_minimapPending.begin() + take);
    }
    for (MinimapPending& mp : batch)
    {
        uint32_t const key = (uint32_t(mp.gy) << 16) | (uint32_t(mp.gx) & 0xFFFFu);
        m_minimapInFlight.remove(key);
        if (!mp.ok || mp.img.isNull())
        {
            m_minimapTextures[key] = 0;
            continue;
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        QImage const& src = mp.img;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
            src.width(), src.height(), 0,
            GL_BGRA, GL_UNSIGNED_BYTE, src.constBits());
        glBindTexture(GL_TEXTURE_2D, 0);
        m_minimapTextures[key] = tex;

        // Find the LitTerrainTile that wanted this and assign so next paint
        // picks it up without re-querying.
        for (LitTerrainTile& t : m_litTerrainTiles)
            if (t.gx == mp.gx && t.gy == mp.gy) { t.texture = tex; break; }
    }
}

// ============================================================================
// HUD overlay (Tab to toggle).  QPainter pass over the GL framebuffer.
// ============================================================================

void SceneView3D::paintOverlay()
{
    if (!m_overlayVisible) return;
    // Sample terrain Z under the camera once per second so we don't
    // hammer the MapTileCache.
    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    if (m_mapCache && (now - m_overlayUpdateMs) > 1000)
    {
        m_overlayUpdateMs = now;
        float const z = m_mapCache->heightAt(m_heightmapMapId, m_camX, m_camY);
        if (z > io::ADT_INVALID_HEIGHT)
        {
            m_terrainZUnderCam   = z;
            m_heightAboveTerrain = m_camZ - z;
            m_haveTerrainSample  = true;
        }
        else
        {
            m_haveTerrainSample = false;
        }
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont font = painter.font();
    font.setPointSize(9);
    font.setFamily(QStringLiteral("Consolas"));
    painter.setFont(font);

    // Soft dark background panel for legibility.
    int const x0 = 8;
    int const y0 = 8;
    int const w  = 360;
    int const h  = 76;
    painter.fillRect(QRect(x0, y0, w, h), QColor(0, 0, 0, 140));
    painter.setPen(QColor(225, 225, 235, 255));

    QString const camLine = QStringLiteral("Camera: (%1, %2, %3)")
        .arg(m_camX, 0, 'f', 1).arg(m_camY, 0, 'f', 1).arg(m_camZ, 0, 'f', 1);
    QString const heightLine = m_haveTerrainSample
        ? QStringLiteral("Height above terrain: %1 yd (terrain Z=%2)")
            .arg(m_heightAboveTerrain, 0, 'f', 1).arg(m_terrainZUnderCam, 0, 'f', 1)
        : QStringLiteral("Height above terrain: <no sample>");
    QString const tilesLine = QStringLiteral("Tiles drawn: %1 / %2 (culled: %3)")
        .arg(m_drawnTilesThisFrame).arg(m_totalTilesThisFrame).arg(m_culledTilesThisFrame);
    QString const speedLine = QStringLiteral("Fly: %1 yd/s  (Alt=0.2x, Shift=5x, [ ] adjust, Tab hide, F frame)")
        .arg(m_flySpeed, 0, 'f', 1);

    int const lh = 16;
    painter.drawText(x0 + 8, y0 + 14, tilesLine);
    painter.drawText(x0 + 8, y0 + 14 + lh, camLine);
    painter.drawText(x0 + 8, y0 + 14 + 2 * lh, heightLine);
    painter.drawText(x0 + 8, y0 + 14 + 3 * lh, speedLine);
}

} // namespace world_editor::render

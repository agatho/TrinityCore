#include "NavMeshView.h"

#include "RoadOverlayBuilder.h"

#include "../../extractor_common/RoadCorridor.h"

#include "../io/BlpReader.h"
#include "../io/CascClient.h"
#include "../io/ListfileLookup.h"
#include "../io/MapDb2Lookup.h"
#include "../io/MapReader.h"
#include "../io/MapTileCache.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <QContextMenuEvent>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLTexture>
#include <QPainter>
#include <QStringList>
#include <QTransform>
#include <QWheelEvent>

#include <cstring>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <system_error>

#include <QMainWindow>
#include <QSettings>
#include <QStatusBar>

namespace
{

// Palette matches mmap_world_dump (so the editor + dumper agree on
// what each polygon means at a glance).  Indexed by NAV_AREA_*.
struct Rgb { uint8_t r, g, b; };
constexpr Rgb COLOR_GROUND       = { 170, 170, 170 };
constexpr Rgb COLOR_GROUND_STEEP = { 110, 110, 110 };
constexpr Rgb COLOR_WATER        = {  48,  96, 168 };
constexpr Rgb COLOR_MAGMA_SLIME  = { 168,  48,  32 };
constexpr Rgb COLOR_ROAD         = { 255, 170,   0 };
constexpr Rgb COLOR_UNKNOWN      = {  90,  60, 110 };

// NAV_AREA_* values mirror MMapDefines.h.
constexpr uint8_t NAV_AREA_GROUND       = 11;
constexpr uint8_t NAV_AREA_GROUND_STEEP = 10;
constexpr uint8_t NAV_AREA_WATER        = 9;
constexpr uint8_t NAV_AREA_MAGMA_SLIME  = 8;
constexpr uint8_t NAV_AREA_ROAD         = 7;

Rgb colorForArea(uint8_t area)
{
    switch (area)
    {
        case NAV_AREA_GROUND:       return COLOR_GROUND;
        case NAV_AREA_GROUND_STEEP: return COLOR_GROUND_STEEP;
        case NAV_AREA_WATER:        return COLOR_WATER;
        case NAV_AREA_MAGMA_SLIME:  return COLOR_MAGMA_SLIME;
        case NAV_AREA_ROAD:         return COLOR_ROAD;
        default:                    return COLOR_UNKNOWN;
    }
}

// Vertex shader: world (TC X, TC Y) -> NDC, using the same anchor /
// yardsPerPixel transform as Coords::worldToScreen.  The trailing
// u_viewRotationRad uniform rotates the resulting screen-space pixel
// around the viewport center BEFORE the NDC conversion, so the entire
// 2D viewer (heightmap, minimap, spawns, paths, annotations, ATs,
// graveyards, ...) shares one continent-level rotation knob.  The
// matching CPU helpers worldToScreen2D / screenToWorld2D apply the
// same rotation to QPainter / mouse-pick coords.
constexpr char const* VSHADER = R"(
#version 330 core
layout(location = 0) in vec2 in_world;   // (TC X, TC Y)
layout(location = 1) in vec4 in_color;   // 0..1
uniform float u_yardsPerPixel;
uniform vec2  u_anchorWorld;             // (anchorX, anchorY) in TC frame
uniform vec2  u_anchorPixel;             // pixel that anchors the world point
uniform vec2  u_viewportSize;            // (w, h) in pixels
uniform float u_viewRotationRad;         // screen-space rotation around viewport center
out vec4 v_color;
void main()
{
    // Match world_editor::coords::worldToScreen:
    //   sx = anchorPixel.x + (anchorWorld.y - world.y) / ypp
    //   sy = anchorPixel.y + (anchorWorld.x - world.x) / ypp
    float sx = u_anchorPixel.x + (u_anchorWorld.y - in_world.y) / u_yardsPerPixel;
    float sy = u_anchorPixel.y + (u_anchorWorld.x - in_world.x) / u_yardsPerPixel;
    // Apply continent-level screen-space rotation around viewport center.
    vec2 c = u_viewportSize * 0.5;
    float cr = cos(u_viewRotationRad);
    float sr = sin(u_viewRotationRad);
    vec2 d = vec2(sx, sy) - c;
    sx = c.x + d.x * cr - d.y * sr;
    sy = c.y + d.x * sr + d.y * cr;
    // Convert (sx, sy) pixel coords with origin top-left to NDC.
    float ndcX =  (sx / u_viewportSize.x) * 2.0 - 1.0;
    float ndcY = -((sy / u_viewportSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_color = in_color;
}
)";

constexpr char const* FSHADER = R"(
#version 330 core
in vec4 v_color;
out vec4 out_color;
void main()
{
    out_color = v_color;
}
)";

// Spawn-icon vertex shader.  We emit 6 vertices per spawn (two
// triangles forming a screen-space quad).  in_offset is one of
// (-1,-1) (1,-1) (-1,1) (1,1) (1,-1) (-1,1) (1,1) etc. - pure unit
// quad corners.  The vertex shader places the spawn world point on
// screen then offsets by in_offset * pixelSize in screen space.
constexpr char const* SPAWN_VSHADER = R"(
#version 330 core
layout(location = 0) in vec2 in_world;    // TC (x, y) of spawn
layout(location = 1) in vec2 in_offset;   // unit quad corner: -1..1
layout(location = 2) in vec4 in_color;    // 0..1 RGBA

uniform float u_yardsPerPixel;
uniform vec2  u_anchorWorld;
uniform vec2  u_anchorPixel;
uniform vec2  u_viewportSize;
uniform float u_pixelSize;        // half-size of the icon, in pixels.
uniform float u_viewRotationRad;  // continent-level screen rotation

out vec4 v_color;
out vec2 v_uv;

void main()
{
    float sx = u_anchorPixel.x + (u_anchorWorld.y - in_world.y) / u_yardsPerPixel;
    float sy = u_anchorPixel.y + (u_anchorWorld.x - in_world.x) / u_yardsPerPixel;
    // Rotate the world anchor of the icon FIRST, then apply the pixel-
    // space offset.  This keeps the icon square-aligned to the screen so
    // rotation only repositions the icon, never reshapes it.
    vec2 c = u_viewportSize * 0.5;
    float cr = cos(u_viewRotationRad);
    float sr = sin(u_viewRotationRad);
    vec2 d = vec2(sx, sy) - c;
    sx = c.x + d.x * cr - d.y * sr;
    sy = c.y + d.x * sr + d.y * cr;
    sx += in_offset.x * u_pixelSize;
    sy += in_offset.y * u_pixelSize;

    float ndcX =  (sx / u_viewportSize.x) * 2.0 - 1.0;
    float ndcY = -((sy / u_viewportSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_color = in_color;
    v_uv = in_offset;
}
)";

// Renders a filled circle (anti-aliased) inside the quad.
constexpr char const* SPAWN_FSHADER = R"(
#version 330 core
in vec4 v_color;
in vec2 v_uv;
out vec4 out_color;
void main()
{
    float d = length(v_uv);
    if (d > 1.0)
        discard;
    float aa = fwidth(d);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, d);
    // Ring around the circle for legibility against bright backgrounds.
    float ring = smoothstep(0.55, 0.7, d) * (1.0 - smoothstep(0.85, 1.0, d));
    vec3 rgb = mix(v_color.rgb, vec3(0.05, 0.05, 0.05), ring * 0.7);
    out_color = vec4(rgb, alpha * v_color.a);
}
)";

constexpr struct { float x, y; } SPAWN_QUAD_CORNERS[6] =
{
    { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f },
    {  1.0f, -1.0f }, { 1.0f,  1.0f }, { -1.0f, 1.0f },
};

constexpr float SPAWN_ICON_PIXEL_RADIUS = 6.0f;

// Heightmap shader: textured world quad.  Same world->screen transform
// as the other layers; an extra uniform carries the tile's world AABB
// so the vertex shader knows where each quad corner lands.
constexpr char const* HEIGHT_VSHADER = R"(
#version 330 core
layout(location = 0) in vec2 in_corner;   // (0,0) (1,0) (0,1) (1,1) etc - unit-square corners
uniform float u_yardsPerPixel;
uniform vec2  u_anchorWorld;
uniform vec2  u_anchorPixel;
uniform vec2  u_viewportSize;
uniform vec4  u_tileBounds;  // (minX, maxX, minY, maxY) in world frame
uniform float u_viewRotationRad;

out vec2 v_uv;
void main()
{
    float worldX = mix(u_tileBounds.x, u_tileBounds.y, in_corner.x);
    float worldY = mix(u_tileBounds.z, u_tileBounds.w, in_corner.y);

    float sx = u_anchorPixel.x + (u_anchorWorld.y - worldY) / u_yardsPerPixel;
    float sy = u_anchorPixel.y + (u_anchorWorld.x - worldX) / u_yardsPerPixel;
    vec2 c = u_viewportSize * 0.5;
    float cr = cos(u_viewRotationRad);
    float sr = sin(u_viewRotationRad);
    vec2 d = vec2(sx, sy) - c;
    sx = c.x + d.x * cr - d.y * sr;
    sy = c.y + d.x * sr + d.y * cr;
    float ndcX =  (sx / u_viewportSize.x) * 2.0 - 1.0;
    float ndcY = -((sy / u_viewportSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);

    // UV maps to the texture: x along world X (so increasing in_corner.x
    // moves toward the texture's X axis).
    v_uv = in_corner;
}
)";

constexpr char const* HEIGHT_FSHADER = R"(
#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 out_color;
void main()
{
    vec3 c = texture(u_tex, v_uv).rgb;
    out_color = vec4(c, 1.0);
}
)";

constexpr struct { float x, y; } HEIGHT_QUAD_CORNERS[6] =
{
    { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
};

// Path shader: world-space line segments, per-vertex colour.  Same
// world->screen transform as the other pipelines.
constexpr char const* PATH_VSHADER = R"(
#version 330 core
layout(location = 0) in vec2 in_world;
layout(location = 1) in vec4 in_color;
uniform float u_yardsPerPixel;
uniform vec2  u_anchorWorld;
uniform vec2  u_anchorPixel;
uniform vec2  u_viewportSize;
uniform float u_viewRotationRad;
out vec4 v_color;
void main()
{
    float sx = u_anchorPixel.x + (u_anchorWorld.y - in_world.y) / u_yardsPerPixel;
    float sy = u_anchorPixel.y + (u_anchorWorld.x - in_world.x) / u_yardsPerPixel;
    vec2 c = u_viewportSize * 0.5;
    float cr = cos(u_viewRotationRad);
    float sr = sin(u_viewRotationRad);
    vec2 d = vec2(sx, sy) - c;
    sx = c.x + d.x * cr - d.y * sr;
    sy = c.y + d.x * sr + d.y * cr;
    float ndcX =  (sx / u_viewportSize.x) * 2.0 - 1.0;
    float ndcY = -((sy / u_viewportSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_color = in_color;
}
)";
constexpr char const* PATH_FSHADER = R"(
#version 330 core
in vec4 v_color;
out vec4 out_color;
void main()
{
    out_color = v_color;
}
)";

// Road-overlay shader.  Identical screen-projection plumbing to the path
// shader (so the polylines stay glued to the same world coords through
// pan / zoom / view-rotation), but no per-vertex color — the fragment
// stage simply outputs `u_color`.  Auto-extracted roads bind a gold,
// handcrafted roads bind a coral red.
constexpr char const* ROAD_VSHADER = R"(
#version 330 core
layout(location = 0) in vec2 in_world;
uniform float u_yardsPerPixel;
uniform vec2  u_anchorWorld;
uniform vec2  u_anchorPixel;
uniform vec2  u_viewportSize;
uniform float u_viewRotationRad;
void main()
{
    float sx = u_anchorPixel.x + (u_anchorWorld.y - in_world.y) / u_yardsPerPixel;
    float sy = u_anchorPixel.y + (u_anchorWorld.x - in_world.x) / u_yardsPerPixel;
    vec2 c = u_viewportSize * 0.5;
    float cr = cos(u_viewRotationRad);
    float sr = sin(u_viewRotationRad);
    vec2 d = vec2(sx, sy) - c;
    sx = c.x + d.x * cr - d.y * sr;
    sy = c.y + d.x * sr + d.y * cr;
    float ndcX =  (sx / u_viewportSize.x) * 2.0 - 1.0;
    float ndcY = -((sy / u_viewportSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
}
)";
constexpr char const* ROAD_FSHADER = R"(
#version 330 core
uniform vec4 u_color;
out vec4 out_color;
void main()
{
    out_color = u_color;
}
)";

// Deterministic per-path colour so consecutive paths visually separate.
struct PathColor { uint8_t r, g, b; };
inline PathColor colorForPathId(uint32_t pathId)
{
    // Cheap hash-of-id -> hue, sat=0.7 val=0.95 to stay readable on
    // both light heightmap + dark BG.
    uint32_t h = pathId * 2654435761u;
    float const hue = float(h & 0xFFFFFF) / float(0xFFFFFF);
    float const s = 0.7f, v = 0.95f;
    float const i = std::floor(hue * 6.0f);
    float const f = hue * 6.0f - i;
    float const p = v * (1.0f - s);
    float const q = v * (1.0f - f * s);
    float const t = v * (1.0f - (1.0f - f) * s);
    float r = 0, g = 0, b = 0;
    switch (int(i) % 6)
    {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return { uint8_t(r * 255), uint8_t(g * 255), uint8_t(b * 255) };
}

// Annotation pipeline: each annotation expands to a 2-triangle quad
// scaled by `radius` in world units. The vertex shader offsets the
// world center by (offset.x, offset.y) * radius BEFORE projecting to
// screen, so zooming scales the disc visibly the same way the
// underlying terrain scales.
constexpr char const* ANNOT_VSHADER = R"(
#version 330 core
layout(location = 0) in vec2 in_center;  // (TC X, TC Y) of annotation center
layout(location = 1) in vec2 in_offset;  // -1..1 unit-quad corner
layout(location = 2) in float in_radius; // world-space radius (yards)
layout(location = 3) in vec4  in_color;  // 0..1 RGBA

uniform float u_yardsPerPixel;
uniform vec2  u_anchorWorld;
uniform vec2  u_anchorPixel;
uniform vec2  u_viewportSize;
uniform float u_viewRotationRad;

out vec2 v_uv;
out vec4 v_color;

void main()
{
    // World position of this vertex: offset along (X-north, Y-west)
    // away from the center.  Order: offset.x scales the world X axis,
    // offset.y scales world Y.  Because the 2D view is image-x = -Y,
    // image-y = -X, we apply directly (the projection below mirrors as
    // needed).
    vec2 worldXY = vec2(in_center.x + in_offset.y * in_radius,
                        in_center.y + in_offset.x * in_radius);

    float sx = u_anchorPixel.x + (u_anchorWorld.y - worldXY.y) / u_yardsPerPixel;
    float sy = u_anchorPixel.y + (u_anchorWorld.x - worldXY.x) / u_yardsPerPixel;

    vec2 c = u_viewportSize * 0.5;
    float cr = cos(u_viewRotationRad);
    float sr = sin(u_viewRotationRad);
    vec2 d = vec2(sx, sy) - c;
    sx = c.x + d.x * cr - d.y * sr;
    sy = c.y + d.x * sr + d.y * cr;

    float ndcX =  (sx / u_viewportSize.x) * 2.0 - 1.0;
    float ndcY = -((sy / u_viewportSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);

    v_uv = in_offset;
    v_color = in_color;
}
)";

// Fragment shader: filled disc + ring outline. v_uv lies in [-1, 1]^2,
// length(v_uv) is the normalized radial distance.  The disc fill is
// semi-transparent so the navmesh underneath remains visible; the
// outline is opaque to pop against bright tiles.
constexpr char const* ANNOT_FSHADER = R"(
#version 330 core
in vec2 v_uv;
in vec4 v_color;
out vec4 out_color;
void main()
{
    float d = length(v_uv);
    if (d > 1.0)
        discard;
    float aa = fwidth(d);
    float discFill = 1.0 - smoothstep(1.0 - aa, 1.0, d);
    // Outer ring: 0.96..1.00 strong opaque, fades out by 1.0.
    float ring     = smoothstep(0.94, 0.97, d) * (1.0 - smoothstep(0.99, 1.00, d));
    // Inner fill is ~35% opacity so terrain stays readable through it.
    float alpha    = discFill * (0.35 + ring * 0.65) * v_color.a;
    vec3  rgb      = mix(v_color.rgb, vec3(0.06, 0.06, 0.08), ring * 0.8);
    out_color = vec4(rgb, alpha);
}
)";

struct Rgb01 { float r, g, b; };
constexpr Rgb01 colorForKind(world_editor::render::AnnotationKind k)
{
    switch (k)
    {
        case world_editor::render::AnnotationKind::Road:      return { 1.00f, 0.67f, 0.00f };
        case world_editor::render::AnnotationKind::Crossroad: return { 1.00f, 0.93f, 0.10f };
        case world_editor::render::AnnotationKind::City:      return { 0.95f, 0.55f, 0.85f };
        case world_editor::render::AnnotationKind::Village:   return { 0.70f, 0.45f, 0.70f };
        case world_editor::render::AnnotationKind::Hub:       return { 0.40f, 0.85f, 0.40f };
        case world_editor::render::AnnotationKind::Danger:    return { 0.95f, 0.15f, 0.15f };
        case world_editor::render::AnnotationKind::Vendor:    return { 0.30f, 0.80f, 0.95f };
        case world_editor::render::AnnotationKind::Mailbox:   return { 0.55f, 0.55f, 0.95f };
        case world_editor::render::AnnotationKind::Innkeeper: return { 0.95f, 0.75f, 0.55f };
        case world_editor::render::AnnotationKind::Other:     return { 0.70f, 0.70f, 0.70f };
        // Elevator = bright cyan (vertical transit, stands out against
        // both terrain and the warm road palette).
        case world_editor::render::AnnotationKind::Elevator:  return { 0.20f, 0.85f, 0.95f };
        // Dock = deep marine blue (boat boarding point — pairs visually
        // with water).
        case world_editor::render::AnnotationKind::Dock:      return { 0.10f, 0.45f, 0.85f };
        case world_editor::render::AnnotationKind::Unknown:
        case world_editor::render::AnnotationKind::Count_:
        default:                                              return { 0.50f, 0.50f, 0.50f };
    }
}

} // anonymous namespace

namespace world_editor::render
{

NavMeshView::NavMeshView(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    // Hydrate the continent-level view rotation from QSettings so the
    // operator's last toggle survives an editor restart.  Default 0.0
    // (no view-level rotation): the 2026-05-26 A/B pass 2 confirmed that
    // rotating the whole view broke the layers that were already correct
    // (heightmap / spawns / paths / annotations); the rotation bug lives
    // ONLY in the minimap BLP axis mapping and is fixed via the per-tile
    // m_minimapTransform default (Rotate90CW).  The View menu retains a
    // failsafe checkbox to flip back to -90.0 for comparison.
    QSettings persist;
    bool ok = false;
    double const deg = persist.value(QStringLiteral("viewer2d/view_rotation_degrees"),
                                     0.0).toDouble(&ok);
    m_viewRotationDegrees = ok ? float(deg) : 0.0f;
    s_currentViewRotationDegrees = m_viewRotationDegrees;
}

char const* annotationKindName(AnnotationKind kind)
{
    switch (kind)
    {
        case AnnotationKind::Unknown:   return "unknown";
        case AnnotationKind::Road:      return "road";
        case AnnotationKind::Crossroad: return "crossroad";
        case AnnotationKind::City:      return "city";
        case AnnotationKind::Village:   return "village";
        case AnnotationKind::Hub:       return "hub";
        case AnnotationKind::Danger:    return "danger";
        case AnnotationKind::Vendor:    return "vendor";
        case AnnotationKind::Mailbox:   return "mailbox";
        case AnnotationKind::Innkeeper: return "innkeeper";
        case AnnotationKind::Other:     return "other";
        case AnnotationKind::Elevator:  return "elevator";
        case AnnotationKind::Dock:      return "dock";
        default:                        return "unknown";
    }
}

NavMeshView::~NavMeshView()
{
    // Make the GL context current so QOpenGLBuffer/VAO destructors can
    // release their resources cleanly.
    makeCurrent();
    destroyHeightmapTextures();
    if (m_pathVbo.isCreated())    m_pathVbo.destroy();
    if (m_pathVao.isCreated())    m_pathVao.destroy();
    if (m_roadGraphVbo.isCreated()) m_roadGraphVbo.destroy();
    if (m_roadGraphVao.isCreated()) m_roadGraphVao.destroy();
    if (m_flightEdgeVbo.isCreated()) m_flightEdgeVbo.destroy();
    if (m_flightEdgeVao.isCreated()) m_flightEdgeVao.destroy();
    if (m_transportRouteVbo.isCreated()) m_transportRouteVbo.destroy();
    if (m_transportRouteVao.isCreated()) m_transportRouteVao.destroy();
    if (m_pathDebugVbo.isCreated()) m_pathDebugVbo.destroy();
    if (m_pathDebugVao.isCreated()) m_pathDebugVao.destroy();
    if (m_roadVbo.isCreated())             m_roadVbo.destroy();
    if (m_roadVao.isCreated())             m_roadVao.destroy();
    if (m_handcraftedRoadVbo.isCreated())  m_handcraftedRoadVbo.destroy();
    if (m_handcraftedRoadVao.isCreated())  m_handcraftedRoadVao.destroy();
    if (m_impactVbo.isCreated())           m_impactVbo.destroy();
    if (m_impactVao.isCreated())           m_impactVao.destroy();
    delete m_roadProgram; m_roadProgram = nullptr;
    delete m_pathProgram; m_pathProgram = nullptr;
    if (m_vbo.isCreated())        m_vbo.destroy();
    if (m_vao.isCreated())        m_vao.destroy();
    if (m_spawnVbo.isCreated())   m_spawnVbo.destroy();
    if (m_spawnVao.isCreated())   m_spawnVao.destroy();
    if (m_annotVbo.isCreated())   m_annotVbo.destroy();
    if (m_annotVao.isCreated())   m_annotVao.destroy();
    if (m_heightVbo.isCreated())  m_heightVbo.destroy();
    if (m_heightVao.isCreated())  m_heightVao.destroy();
    delete m_program;
    m_program = nullptr;
    delete m_spawnProgram;
    m_spawnProgram = nullptr;
    delete m_annotProgram;
    m_annotProgram = nullptr;
    delete m_heightProgram;
    m_heightProgram = nullptr;
    doneCurrent();
}

void NavMeshView::destroyHeightmapTextures()
{
    if (!m_heightmapTiles.empty())
    {
        for (HeightmapTile& t : m_heightmapTiles)
        {
            if (t.texture)
            {
                glDeleteTextures(1, &t.texture);
                t.texture = 0;
            }
        }
        m_heightmapTiles.clear();
    }
}

void NavMeshView::setMapTileCache(io::MapTileCache* cache)
{
    m_mapCache = cache;
}

void NavMeshView::rebuildHeightmapTiles(uint32_t mapId)
{
    // Flush every cached tile + per-tile attempt cache BEFORE updating
    // m_heightmapMapId.  Without this, switching from map 0 to map 1
    // would leave the previous map's BLP textures + heightmap quads on
    // screen (keyed by gx/gy, not mapId), and only the NEW map's tiles
    // that didn't already exist at the same (gx, gy) would appear --
    // producing a Frankenstein overlay of two continents.
    bool const mapChanged = (mapId != m_heightmapMapId);
    if (mapChanged)
    {
        destroyMinimapTextures();
        destroyHeightmapTextures();
        m_heightmapTiles.clear();
        m_heightmapBuildQueue.clear();
        m_heightmapBuildIndex = 0;
        m_heightmapBuilding = false;
        m_mapFileCoverage.clear();
        m_minimapFdidByTile.clear();
        m_minimapOrientationLogged = false;
        m_streamProgressLoggedAt = 0;
    }
    m_heightmapMapId = mapId;
    m_heightmapBuilt = false;
    m_heightmapPending = true;
    if (m_buffersReady)
        update();
}

void NavMeshView::setNavMesh(io::LoadedMMap mesh)
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
        for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
        {
            dtMeshTile const* tile = nm->getTile(ti);
            if (!tile || !tile->header || tile->header->polyCount <= 0)
                continue;
            // Detour AABBs are in Detour frame: [0]=TC Y, [2]=TC X.
            minX = std::min(minX, tile->header->bmin[2]);
            maxX = std::max(maxX, tile->header->bmax[2]);
            minY = std::min(minY, tile->header->bmin[0]);
            maxY = std::max(maxY, tile->header->bmax[0]);
        }
        if (std::isfinite(minX) && std::isfinite(maxX))
        {
            m_meshMinX = minX;  m_meshMaxX = maxX;
            m_meshMinY = minY;  m_meshMaxY = maxY;
            m_meshBoundsValid = true;
        }
    }

    rebuildBuffers();
    // The road skeleton is keyed to the navmesh contents, so a new mesh
    // invalidates it.  The CPU-side rebuild runs unconditionally; the
    // GPU upload defers to the next paint when the GL context is ready.
    rebuildAutoRoadOverlay();
    if (m_meshBoundsValid)
        frameMesh();
    update();
}

void NavMeshView::setSpawns(std::vector<Spawn> spawns)
{
    m_spawns = std::move(spawns);
    m_hoveredSpawn = -1;
    // Mark the SpawnDensity grid for lazy rebuild on next paint.
    m_spawnDensityDirty = true;
    rebuildSpawnBuffers();
    update();
}

void NavMeshView::rebuildSpawnDensityGrid() const
{
    // Count spawns per 50-yard cell.  Filtered spawns are excluded so
    // the heatmap reflects the operator's current view.
    m_spawnDensityCells.clear();
    m_spawnDensityCells.reserve(m_spawns.size() / 4 + 16);
    for (Spawn const& s : m_spawns)
    {
        if (!spawnPassesPhaseFilter(s))
            continue;
        CellKey k{};
        k.cx = int(std::floor(s.worldX / kSpawnDensityCellYards));
        k.cy = int(std::floor(s.worldY / kSpawnDensityCellYards));
        ++m_spawnDensityCells[k];
    }
    m_spawnDensityDirty = false;
}

void NavMeshView::setAnnotations(std::vector<Annotation> annotations)
{
    m_annotations = std::move(annotations);
    m_hoveredAnnotation = -1;
    // Clamp the selected index defensively: a model swap might have
    // dropped the previously-selected row.
    if (m_selectedAnnotation >= int(m_annotations.size()))
        m_selectedAnnotation = -1;
    bool const glReady = m_annotVbo.isCreated();
    qDebug() << "[annotation] viewer received" << m_annotations.size()
             << "total annotations (GL ready=" << (glReady ? 'Y' : 'N') << ")";
    rebuildAnnotationBuffers();
    rebuildRoadGraphBuffer();
    update();
}

void NavMeshView::setSelectedAnnotation(int index)
{
    if (index < -1 || index >= int(m_annotations.size()))
        index = -1;
    if (m_selectedAnnotation == index)
        return;
    m_selectedAnnotation = index;
    update();
}

void NavMeshView::setPaths(std::vector<Path> paths)
{
    m_paths = std::move(paths);
    rebuildPathBuffers();
    update();
}

void NavMeshView::setAreatriggers(std::vector<Areatrigger> atrs)
{
    m_areatriggers = std::move(atrs);
    rebuildAreatriggerBuffer();
    rebuildAreatriggerShapes();
    update();
}

void NavMeshView::rebuildAreatriggerShapes()
{
    // GL_LINES with PathVertex layout (world x, world y, RGBA byte).
    struct V { float x, y; uint8_t r, g, b, a; };
    static_assert(sizeof(V) == 12, "atr-shape vertex size drift");
    std::vector<V> verts;

    constexpr uint8_t R = 220, G = 100, B = 240, A = 220;
    auto pushSegment = [&](float ax, float ay, float bx, float by)
    {
        verts.push_back({ ax, ay, R, G, B, A });
        verts.push_back({ bx, by, R, G, B, A });
    };
    auto pushCircle = [&](float cx, float cy, float radius, int slices = 32)
    {
        if (radius < 0.5f) radius = 0.5f;
        for (int i = 0; i < slices; ++i)
        {
            float const t0 = 2.0f * float(M_PI) * float(i)     / float(slices);
            float const t1 = 2.0f * float(M_PI) * float(i + 1) / float(slices);
            pushSegment(cx + radius * std::cos(t0), cy + radius * std::sin(t0),
                        cx + radius * std::cos(t1), cy + radius * std::sin(t1));
        }
    };
    auto pushBox = [&](float cx, float cy, float hx, float hy, float orientation)
    {
        // Rotated rectangle: 4 corners in local coords, transformed by yaw.
        if (hx < 0.5f) hx = 0.5f;
        if (hy < 0.5f) hy = 0.5f;
        float const co = std::cos(orientation), so = std::sin(orientation);
        auto rotate = [&](float lx, float ly, float& wx, float& wy)
        {
            // World X north, Y west; +X is "forward" of orientation=0.
            wx = cx + lx * co - ly * so;
            wy = cy + ly * co + lx * so;
        };
        float p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
        rotate(+hx, +hy, p0x, p0y);
        rotate(-hx, +hy, p1x, p1y);
        rotate(-hx, -hy, p2x, p2y);
        rotate(+hx, -hy, p3x, p3y);
        pushSegment(p0x, p0y, p1x, p1y);
        pushSegment(p1x, p1y, p2x, p2y);
        pushSegment(p2x, p2y, p3x, p3y);
        pushSegment(p3x, p3y, p0x, p0y);
    };

    for (Areatrigger const& a : m_areatriggers)
    {
        switch (a.shape)
        {
            case 0: // Sphere - data0 = radius
            case 3: // Cylinder - data0 = radius (top-down indistinguishable from sphere)
            case 4: // Disk - data0 = radius
                pushCircle(a.x, a.y, a.shapeData[0]);
                break;
            case 1: // Box - data0/data1 = half-extents X/Y (yards)
                pushBox(a.x, a.y, a.shapeData[0], a.shapeData[1], a.orientation);
                break;
            case 2: // Polygon - data0..7 contain polygon vertices via a separate table;
                    // Phase 7b fallback: small marker circle.
                pushCircle(a.x, a.y, 2.0f, 12);
                break;
            default:
                pushCircle(a.x, a.y, 2.0f, 12);
                break;
        }
    }

    m_atrShapeVertexCount = static_cast<GLsizei>(verts.size());

    // Skip the GL upload when this widget's context isn't up yet -- e.g. the
    // editor opened straight into the 3D view, so this 2D NavMeshView was never
    // shown and initializeGL() hasn't run.  Calling glEnableVertexAttribArray
    // through an uninitialised QOpenGLFunctions table is an access violation
    // (crash-2026-05-31).  The shapes get built from m_areatriggers when the
    // operator switches to 2D and initializeGL() calls us with a live context.
    // When invoked outside paintGL (a MainWindow setter) bracket with
    // makeCurrent, mirroring destroyMinimapTextures().
    if (!context())
        return;
    bool const needCurrent = (QOpenGLContext::currentContext() != context());
    if (needCurrent) makeCurrent();
    m_atrShapeVao.bind();
    m_atrShapeVbo.bind();
    if (!verts.empty())
        m_atrShapeVbo.allocate(verts.data(), int(verts.size() * sizeof(V)));
    else
        m_atrShapeVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, r)));
    m_atrShapeVbo.release();
    m_atrShapeVao.release();
    if (needCurrent) doneCurrent();
}

void NavMeshView::setGraveyards(std::vector<Graveyard> gys)
{
    m_graveyards = std::move(gys);
    rebuildGraveyardBuffer();
    update();
}

void NavMeshView::setInstanceEntrances(std::vector<InstanceEntrance> entries)
{
    // Painter-only overlay; no GL buffers to rebuild, just stash + repaint.
    m_instanceEntrances = std::move(entries);
    update();
}

void NavMeshView::setSpawnLinks(std::vector<std::pair<int64_t, int64_t>> links)
{
    // Painter-only; resolved against m_spawns at draw time so this is a
    // pure cache swap.  Empty input clears the overlay.
    m_spawnLinks = std::move(links);
    update();
}

void NavMeshView::setWmoFootprints(std::vector<std::tuple<float, float, float, float>> aabbs)
{
    // QPainter overlay; no GL state to touch.  Layer::WmoOutline visibility
    // gates the paint pass independently so callers can churn the footprint
    // list freely without flashing the layer off-on.
    m_wmoFootprints = std::move(aabbs);
    update();
}

void NavMeshView::rebuildAreatriggerBuffer()
{
    m_atrDirty = true;
    if (m_buffersReady) { uploadAreatriggerGeometry(); update(); }
}

void NavMeshView::uploadAreatriggerGeometry()
{
    // Same vertex layout as spawn icons (x, y, ox, oy, r, g, b, a).
    struct V { float x, y; float ox, oy; uint8_t r, g, b, a; };
    static_assert(sizeof(V) == 20, "atr vertex size drift");
    std::vector<V> verts;
    verts.reserve(m_areatriggers.size() * 6);
    for (Areatrigger const& a : m_areatriggers)
    {
        constexpr uint8_t r = 200, g = 80, b = 230, al = 230;
        for (auto const& c : SPAWN_QUAD_CORNERS)
            verts.push_back({ a.x, a.y, c.x, c.y, r, g, b, al });
    }
    m_atrDirty = false;
    m_atrVertexCount = static_cast<GLsizei>(verts.size());
    m_atrVao.bind();
    m_atrVbo.bind();
    if (!verts.empty())
        m_atrVbo.allocate(verts.data(), int(verts.size() * sizeof(V)));
    else
        m_atrVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, r)));
    m_atrVbo.release();
    m_atrVao.release();
}

void NavMeshView::rebuildGraveyardBuffer()
{
    m_gyDirty = true;
    if (m_buffersReady) { uploadGraveyardGeometry(); update(); }
}

void NavMeshView::uploadGraveyardGeometry()
{
    struct V { float x, y; float ox, oy; uint8_t r, g, b, a; };
    std::vector<V> verts;
    verts.reserve(m_graveyards.size() * 6);
    for (Graveyard const& g : m_graveyards)
    {
        constexpr uint8_t r = 80, gC = 220, b = 230, al = 230;
        for (auto const& c : SPAWN_QUAD_CORNERS)
            verts.push_back({ g.x, g.y, c.x, c.y, r, gC, b, al });
    }
    m_gyDirty = false;
    m_gyVertexCount = static_cast<GLsizei>(verts.size());
    m_gyVao.bind();
    m_gyVbo.bind();
    if (!verts.empty())
        m_gyVbo.allocate(verts.data(), int(verts.size() * sizeof(V)));
    else
        m_gyVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(V),
                          reinterpret_cast<void*>(offsetof(V, r)));
    m_gyVbo.release();
    m_gyVao.release();
}

int NavMeshView::hitTestAreatrigger(QPoint const& screen, float pixelTolerance) const
{
    int best = -1; float bestD2 = pixelTolerance * pixelTolerance;
    for (size_t i = 0; i < m_areatriggers.size(); ++i)
    {
        auto const& a = m_areatriggers[i];
        auto const [sx, sy] = worldToScreen2D({ a.x, a.y, a.z });
        float const dx = sx - float(screen.x()), dy = sy - float(screen.y());
        float const d2 = dx * dx + dy * dy;
        if (d2 <= bestD2) { bestD2 = d2; best = int(i); }
    }
    return best;
}

int NavMeshView::hitTestGraveyard(QPoint const& screen, float pixelTolerance) const
{
    int best = -1; float bestD2 = pixelTolerance * pixelTolerance;
    for (size_t i = 0; i < m_graveyards.size(); ++i)
    {
        auto const& g = m_graveyards[i];
        auto const [sx, sy] = worldToScreen2D({ g.x, g.y, g.z });
        float const dx = sx - float(screen.x()), dy = sy - float(screen.y());
        float const d2 = dx * dx + dy * dy;
        if (d2 <= bestD2) { bestD2 = d2; best = int(i); }
    }
    return best;
}

void NavMeshView::rebuildPathBuffers()
{
    m_pathDirty = true;
    if (m_buffersReady)
    {
        uploadPathGeometry();
        update();
    }
}

void NavMeshView::rebuildRoadGraphBuffer()
{
    // GL_LINES segments connecting Road annotations that are within
    // kRoadGraphLinkRadius of each other.  Brute O(N^2) -- annotations
    // typically count in the hundreds per map; a spatial hash is overkill.
    // The radius is wider than the typical Road default-radius (12y) so
    // the operator sees a connected network even when they're spacing
    // waypoints a bit farther apart.
    constexpr float kRoadGraphLinkRadius   = 40.0f;
    constexpr float kRoadGraphLinkRadiusSq = kRoadGraphLinkRadius * kRoadGraphLinkRadius;

    struct PathVertex { float x, y; uint8_t r, g, b, a; };
    std::vector<PathVertex> verts;
    // Pre-filter to just Road annotations + collect their (x, y, z).
    struct RoadPt { float x, y; };
    std::vector<RoadPt> roads;
    roads.reserve(m_annotations.size());
    for (Annotation const& a : m_annotations)
        if (a.kind == AnnotationKind::Road)
            roads.push_back({ a.x, a.y });

    // Slightly desaturated orange — Road annotation discs are already
    // orange (1.00, 0.67, 0.00); pick a softer tone for the linking
    // lines so the discs stay the focal point.
    constexpr uint8_t kR = 200, kG = 140, kB = 40, kA = 200;

    verts.reserve(roads.size() * 2);  // upper-bound estimate
    for (size_t i = 0; i < roads.size(); ++i)
    {
        for (size_t j = i + 1; j < roads.size(); ++j)
        {
            float const dx = roads[i].x - roads[j].x;
            float const dy = roads[i].y - roads[j].y;
            float const dsq = dx * dx + dy * dy;
            if (dsq > kRoadGraphLinkRadiusSq)
                continue;
            verts.push_back({ roads[i].x, roads[i].y, kR, kG, kB, kA });
            verts.push_back({ roads[j].x, roads[j].y, kR, kG, kB, kA });
        }
    }

    m_roadGraphVertexCount = static_cast<GLsizei>(verts.size());
    if (!m_buffersReady || !m_roadGraphVbo.isCreated())
        return;
    m_roadGraphVao.bind();
    m_roadGraphVbo.bind();
    if (!verts.empty())
        m_roadGraphVbo.allocate(verts.data(), int(verts.size() * sizeof(PathVertex)));
    else
        m_roadGraphVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_roadGraphVbo.release();
    m_roadGraphVao.release();
}

void NavMeshView::rebuildAutoRoadOverlay()
{
    // Walk the dtNavMesh and extract a polyline skeleton from every
    // NAV_AREA_ROAD polygon.  Builder is light enough to rerun every
    // setNavMesh; the result lives in m_pendingRoadVertices until the
    // GL context flushes it to the VBO.
    m_pendingRoadVertices.clear();
    if (m_mesh.ok())
    {
        RoadOverlayBuilder builder;
        builder.buildFromNavmesh(m_mesh.navmesh());
        m_pendingRoadVertices = builder.polyline_vertices();
    }
    m_pendingRoadDirty = true;
    if (m_buffersReady)
    {
        uploadAutoRoadOverlay();
        update();
    }
}

void NavMeshView::uploadAutoRoadOverlay()
{
    m_pendingRoadDirty = false;
    m_roadVertexCount = static_cast<GLsizei>(m_pendingRoadVertices.size());
    if (!m_roadVbo.isCreated() || !m_roadVao.isCreated())
        return;
    m_roadVao.bind();
    m_roadVbo.bind();
    if (!m_pendingRoadVertices.empty())
        m_roadVbo.allocate(m_pendingRoadVertices.data(),
                           int(m_pendingRoadVertices.size() * sizeof(QVector2D)));
    else
        m_roadVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QVector2D),
                          reinterpret_cast<void*>(0));
    m_roadVbo.release();
    m_roadVao.release();
}

void NavMeshView::setHandcraftedRoadPolylines(std::vector<QVector2D> const& vertices)
{
    // Even-count requirement: GL_LINES consumes pairs.  Truncate any odd
    // trailing vertex defensively so a partially-built feed doesn't make
    // the GPU read past the end of the buffer.
    m_pendingHandcraftedRoadVertices = vertices;
    if (m_pendingHandcraftedRoadVertices.size() % 2 != 0)
        m_pendingHandcraftedRoadVertices.pop_back();
    m_pendingHandcraftedRoadDirty = true;
    // Refresh the connectivity diagnostic (junctions / dead ends / gaps) so the
    // overlay matches the new segment set. Done here (data change), not per frame.
    rebuildRoadConnectivityDiagnostic();
    // VBO/VAO are created in initializeGL, which may not have fired yet
    // when the dock first calls us.  Gate the immediate upload on the VBO
    // being created (not on m_buffersReady -- that flag only flips on the
    // first setNavMesh, but handcrafted polylines can arrive before any
    // navmesh is loaded).  The dirty flag is the safety net: paintGL
    // checks it on every frame and flushes if needed, so even if the
    // immediate upload is skipped here the next frame picks it up.
    bool const glReady = m_handcraftedRoadVbo.isCreated();
    qDebug() << "[handcrafted-road] viewer received" << m_pendingHandcraftedRoadVertices.size()
             << "vertices (GL ready=" << (glReady ? 'Y' : 'N') << "), pending list size="
             << m_pendingHandcraftedRoadVertices.size();
    if (glReady)
    {
        uploadHandcraftedRoadOverlay();
        update();
    }
}

void NavMeshView::uploadHandcraftedRoadOverlay()
{
    m_pendingHandcraftedRoadDirty = false;
    m_handcraftedRoadVertexCount = static_cast<GLsizei>(m_pendingHandcraftedRoadVertices.size());
    if (!m_handcraftedRoadVbo.isCreated() || !m_handcraftedRoadVao.isCreated())
        return;
    m_handcraftedRoadVao.bind();
    m_handcraftedRoadVbo.bind();
    if (!m_pendingHandcraftedRoadVertices.empty())
        m_handcraftedRoadVbo.allocate(m_pendingHandcraftedRoadVertices.data(),
                                      int(m_pendingHandcraftedRoadVertices.size() * sizeof(QVector2D)));
    else
        m_handcraftedRoadVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QVector2D),
                          reinterpret_cast<void*>(0));
    m_handcraftedRoadVbo.release();
    m_handcraftedRoadVao.release();
    qDebug() << "[handcrafted-road] viewer GL upload:" << m_handcraftedRoadVertexCount
             << "vertices uploaded to VBO";
}

void NavMeshView::rebuildRoadConnectivityDiagnostic()
{
    m_roadJunctionNodes.clear();
    m_roadDanglingNodes.clear();
    m_roadGapPairs.clear();

    auto const& verts = m_pendingHandcraftedRoadVertices;
    if (verts.size() < 2)
        return;

    // Cluster endpoints within the merge epsilon into nodes, exactly like the
    // worldserver's HandcraftedRoadGraph::BuildFromStorage, so the overlay is a
    // faithful preview of the routable graph the bots will build.
    float const eps  = ROAD_NODE_MERGE_EPSILON_YARDS;
    float const eps2 = eps * eps;
    struct Node { float x, y; int degree; };
    std::vector<Node> nodes;
    nodes.reserve(verts.size());

    auto findOrAdd = [&](float x, float y) -> size_t
    {
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            float dx = nodes[i].x - x;
            float dy = nodes[i].y - y;
            if (dx * dx + dy * dy <= eps2)
                return i;
        }
        nodes.push_back({ x, y, 0 });
        return nodes.size() - 1;
    };

    // Each consecutive vertex PAIR is a segment (GL_LINES layout). Both of its
    // endpoints add one to their node's degree (= incident edge count).
    for (size_t i = 0; i + 1 < verts.size(); i += 2)
    {
        size_t a = findOrAdd(verts[i].x(),     verts[i].y());
        size_t b = findOrAdd(verts[i + 1].x(), verts[i + 1].y());
        if (a == b)
            continue; // degenerate / sub-epsilon segment
        nodes[a].degree += 1;
        nodes[b].degree += 1;
    }

    for (Node const& n : nodes)
    {
        if (n.degree >= 3)
            m_roadJunctionNodes.emplace_back(n.x, n.y);
        else if (n.degree == 1)
            m_roadDanglingNodes.emplace_back(n.x, n.y);
    }

    // Near-miss gaps: two DISTINCT nodes that are close enough the operator
    // likely meant to connect (> merge epsilon, <= gap-warn) but won't merge in
    // the bot graph. Restricted to pairs where at least one side is a dead end
    // (degree 1) so we flag genuine loose-end gaps, not parallel through-roads.
    float const gap     = ROAD_GAP_WARN_YARDS;
    float const gapHiSq = gap * gap;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        for (size_t j = i + 1; j < nodes.size(); ++j)
        {
            if (nodes[i].degree != 1 && nodes[j].degree != 1)
                continue;
            float dx = nodes[i].x - nodes[j].x;
            float dy = nodes[i].y - nodes[j].y;
            float d2 = dx * dx + dy * dy;
            if (d2 > eps2 && d2 <= gapHiSq)
                m_roadGapPairs.emplace_back(QVector2D(nodes[i].x, nodes[i].y),
                                            QVector2D(nodes[j].x, nodes[j].y));
        }
    }

    if (!m_roadDanglingNodes.empty() || !m_roadGapPairs.empty())
        qDebug() << "[handcrafted-road] connectivity:" << nodes.size() << "nodes,"
                 << m_roadJunctionNodes.size() << "junctions,"
                 << m_roadDanglingNodes.size() << "dead ends,"
                 << m_roadGapPairs.size() << "near-miss gaps";
}

bool NavMeshView::hitTestRoadDiagnostic(QPoint const& screen, float& outWorldX,
                                        float& outWorldY, float hitRadiusPixels) const
{
    float bestD2 = hitRadiusPixels * hitRadiusPixels;
    bool found = false;

    auto consider = [&](float wx, float wy)
    {
        auto const [sx, sy] = worldToScreen2D({ wx, wy, 0.0f });
        float const dx = sx - float(screen.x());
        float const dy = sy - float(screen.y());
        float const d2 = dx * dx + dy * dy;
        if (d2 <= bestD2)
        {
            bestD2    = d2;
            outWorldX = wx;
            outWorldY = wy;
            found     = true;
        }
    };

    // Gap rings (red) first, then dead-end rings (amber). Junctions are
    // informational and not selectable.
    for (auto const& pr : m_roadGapPairs)
    {
        consider(pr.first.x(),  pr.first.y());
        consider(pr.second.x(), pr.second.y());
    }
    for (QVector2D const& n : m_roadDanglingNodes)
        consider(n.x(), n.y());

    return found;
}

// ---- Handcrafted-road segment placement + impact preview ----

void NavMeshView::enterSegmentPlacementMode()
{
    m_segmentPlacementState = SegmentPlacementState::WaitingForStart;
    m_chainSegmentCount     = 0;
    setCursor(Qt::CrossCursor);
    emit handcraftedSegmentPlacementStateChanged(int(m_segmentPlacementState));
    emit handcraftedChainSegmentCountChanged(m_chainSegmentCount);
    update();
}

void NavMeshView::cancelSegmentPlacement()
{
    if (m_segmentPlacementState == SegmentPlacementState::None)
        return;
    m_segmentPlacementState = SegmentPlacementState::None;
    m_segmentStartWorld     = coords::WorldPos{};
    m_segmentHoverWorld     = coords::WorldPos{};
    m_chainSegmentCount     = 0;
    setCursor(m_placementMode ? Qt::CrossCursor : Qt::ArrowCursor);
    emit handcraftedSegmentPlacementStateChanged(int(m_segmentPlacementState));
    emit handcraftedChainSegmentCountChanged(m_chainSegmentCount);
    update();
}

int NavMeshView::findSnapTarget(float qx, float qy,
                                std::vector<QVector2D> const& candidates,
                                float radiusYards)
{
    float const r2 = radiusYards * radiusYards;
    int bestIdx = -1;
    float bestD2 = r2;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        float const dx = candidates[i].x() - qx;
        float const dy = candidates[i].y() - qy;
        float const d2 = dx * dx + dy * dy;
        if (d2 <= bestD2)
        {
            bestD2 = d2;
            bestIdx = int(i);
        }
    }
    return bestIdx;
}

void NavMeshView::uploadImpactGeometry()
{
    m_impactDirty = false;
    m_impactVertexCount = static_cast<GLsizei>(m_pendingImpactVerts.size());
    if (!m_impactVbo.isCreated() || !m_impactVao.isCreated())
        return;
    m_impactVao.bind();
    m_impactVbo.bind();
    if (!m_pendingImpactVerts.empty())
        m_impactVbo.allocate(m_pendingImpactVerts.data(),
                             int(m_pendingImpactVerts.size() * sizeof(Vertex)));
    else
        m_impactVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));
    m_impactVbo.release();
    m_impactVao.release();
}

NavMeshView::ImpactPreviewResult NavMeshView::previewSegmentImpact(float fromX, float fromY,
                                                                    float toX,   float toY,
                                                                    float width)
{
    ImpactPreviewResult out;
    if (!m_mesh.ok() || !m_mesh.navmesh())
        return out;

    Road::Segment seg{ fromX, fromY, toX, toY, width };
    Road::CorridorResult const r = Road::ScanCorridor(*m_mesh.navmesh(), seg);
    out.polyRefs      = r.polyRefs;
    out.tilesScanned  = r.tilesScanned;
    out.polysExamined = r.polysExamined;

    // Build a triangle-list highlight from each returned poly's fan.
    // Detour stores verts in (Y, Z, X) = (TC Y, height, TC X); we emit
    // GL_TRIANGLES vertices in TC (X, Y) so the existing nav-mesh shader
    // can render them.  Color is translucent yellow (RGBA 240,220,60,140).
    m_pendingImpactVerts.clear();
    m_pendingImpactVerts.reserve(out.polyRefs.size() * 12);

    dtNavMesh const* nm = m_mesh.navmesh();
    for (uint64_t refU64 : out.polyRefs)
    {
        dtPolyRef const ref = static_cast<dtPolyRef>(refU64);
        dtMeshTile const* tile = nullptr;
        dtPoly const*     poly = nullptr;
        if (dtStatusFailed(nm->getTileAndPolyByRef(ref, &tile, &poly)) || !tile || !poly)
            continue;
        if (poly->vertCount < 3)
            continue;
        // Fan-triangulate the polygon around vertex 0.
        float const* v0 = &tile->verts[poly->verts[0] * 3];
        float const v0tcX = v0[2];
        float const v0tcY = v0[0];
        for (int i = 1; i + 1 < poly->vertCount; ++i)
        {
            float const* v1 = &tile->verts[poly->verts[i] * 3];
            float const* v2 = &tile->verts[poly->verts[i + 1] * 3];
            Vertex va{ v0tcX, v0tcY, 240, 220, 60, 140 };
            Vertex vb{ v1[2], v1[0], 240, 220, 60, 140 };
            Vertex vc{ v2[2], v2[0], 240, 220, 60, 140 };
            m_pendingImpactVerts.push_back(va);
            m_pendingImpactVerts.push_back(vb);
            m_pendingImpactVerts.push_back(vc);
        }
    }
    m_impactDirty = true;
    m_impactDeadlineMs = QDateTime::currentMSecsSinceEpoch() + kImpactPreviewLifetimeMs;
    if (m_buffersReady)
        uploadImpactGeometry();
    update();
    return out;
}

void NavMeshView::clearSegmentImpactPreview()
{
    m_pendingImpactVerts.clear();
    m_impactDirty = true;
    m_impactDeadlineMs = 0;
    if (m_buffersReady)
        uploadImpactGeometry();
    update();
}

size_t NavMeshView::applyHandcraftedSegmentToLocalNavmesh(float fromX, float fromY,
                                                          float toX,   float toY,
                                                          float width)
{
    if (!m_mesh.ok() || !m_mesh.navmesh())
        return 0;
    Road::Segment seg{ fromX, fromY, toX, toY, width };
    size_t const tagged = Road::ApplyCorridorToNavmesh(*m_mesh.navmesh(), seg);
    // The freshly retagged polygons now read as NAV_AREA_ROAD, so the
    // auto-extracted gold overlay needs a rebuild to surface them.
    if (tagged > 0)
        rebuildAutoRoadOverlay();
    // Force the next paint to re-upload before drawing.
    if (m_buffersReady)
        uploadAutoRoadOverlay();
    update();
    return tagged;
}

void NavMeshView::rebuildRoadOverlayFromCurrentNavmesh()
{
    rebuildAutoRoadOverlay();
    if (m_buffersReady)
        uploadAutoRoadOverlay();
    update();
}

void NavMeshView::setGatheringNodes(std::unordered_map<int64_t, uint8_t> nodes)
{
    // Hot-set: same pattern as setSpawnGroupColors / setFactionTintMap.
    // Visibility is gated by Layer::GatheringNodes so the operator can
    // load the map once and toggle the overlay freely.
    m_gatheringNodes = std::move(nodes);
    update();
}

void NavMeshView::setFlightGraph(std::vector<FlightNode> nodes, std::vector<FlightEdge> edges)
{
    m_flightNodes = std::move(nodes);
    m_flightEdges = std::move(edges);
    rebuildFlightEdgeBuffer();
    update();
}

void NavMeshView::rebuildFlightEdgeBuffer()
{
    // GL_LINES segments connecting taxi_path FromTaxiNode -> ToTaxiNode.
    // Only edges whose BOTH endpoints are in the on-map FlightNode list
    // are emitted; cross-map flight paths are intentionally skipped (the
    // viewer is single-map).
    struct PathVertex { float x, y; uint8_t r, g, b, a; };
    static_assert(sizeof(PathVertex) == 12, "flight edge vertex size drift");

    // O(1) lookup id -> node position.  Linear in node count; flight-node
    // sets per map are bounded in the hundreds, so the unordered_map is
    // overkill but defensive (taxi_nodes can balloon on heavily-modded
    // shards).
    std::unordered_map<uint32_t, std::pair<float, float>> idToXY;
    idToXY.reserve(m_flightNodes.size() * 2 + 1);
    for (FlightNode const& n : m_flightNodes)
        idToXY.emplace(n.id, std::pair<float, float>(n.x, n.y));

    constexpr uint8_t kR = 60, kG = 180, kB = 255, kA = 200;
    std::vector<PathVertex> verts;
    verts.reserve(m_flightEdges.size() * 2);
    for (FlightEdge const& e : m_flightEdges)
    {
        auto const itA = idToXY.find(e.fromId);
        auto const itB = idToXY.find(e.toId);
        if (itA == idToXY.end() || itB == idToXY.end())
            continue;
        verts.push_back({ itA->second.first, itA->second.second, kR, kG, kB, kA });
        verts.push_back({ itB->second.first, itB->second.second, kR, kG, kB, kA });
    }

    m_flightEdgeVertexCount = static_cast<GLsizei>(verts.size());
    if (!m_buffersReady || !m_flightEdgeVbo.isCreated())
        return;
    m_flightEdgeVao.bind();
    m_flightEdgeVbo.bind();
    if (!verts.empty())
        m_flightEdgeVbo.allocate(verts.data(), int(verts.size() * sizeof(PathVertex)));
    else
        m_flightEdgeVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_flightEdgeVbo.release();
    m_flightEdgeVao.release();
}

void NavMeshView::setTransportRoutes(std::vector<std::vector<coords::WorldPos>> routes)
{
    m_transportRoutes = std::move(routes);
    rebuildTransportRouteBuffer();
    update();
}

void NavMeshView::rebuildTransportRouteBuffer()
{
    // GL_LINES polyline per route (orange, thick).  We unroll each Nx-point
    // route into (Nx-1)*2 line endpoints so a single glDrawArrays renders
    // every transport's path in one call.
    struct PathVertex { float x, y; uint8_t r, g, b, a; };
    static_assert(sizeof(PathVertex) == 12, "transport route vertex size drift");

    // Orange (matches NAV_AREA_ROAD palette) with strong alpha so the
    // route reads cleanly on top of terrain shading.
    constexpr uint8_t kR = 255, kG = 140, kB = 0, kA = 230;
    std::vector<PathVertex> verts;
    size_t segCount = 0;
    for (auto const& route : m_transportRoutes)
        if (route.size() >= 2) segCount += route.size() - 1;
    verts.reserve(segCount * 2);
    for (auto const& route : m_transportRoutes)
    {
        if (route.size() < 2) continue;
        for (size_t i = 0; i + 1 < route.size(); ++i)
        {
            verts.push_back({ route[i].x,     route[i].y,     kR, kG, kB, kA });
            verts.push_back({ route[i + 1].x, route[i + 1].y, kR, kG, kB, kA });
        }
    }

    m_transportRouteVertexCount = static_cast<GLsizei>(verts.size());
    if (!m_buffersReady || !m_transportRouteVbo.isCreated())
        return;
    m_transportRouteVao.bind();
    m_transportRouteVbo.bind();
    if (!verts.empty())
        m_transportRouteVbo.allocate(verts.data(), int(verts.size() * sizeof(PathVertex)));
    else
        m_transportRouteVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_transportRouteVbo.release();
    m_transportRouteVao.release();
}

void NavMeshView::uploadPathGeometry()
{
    // Pack interleaved (x, y, r, g, b, a) as floats+bytes; one pair of
    // verts per segment using GL_LINES.
    struct PathVertex
    {
        float x, y;
        uint8_t r, g, b, a;
    };
    static_assert(sizeof(PathVertex) == 12, "PathVertex packed size drift");

    std::vector<PathVertex> verts;
    for (Path const& p : m_paths)
    {
        if (p.nodes.size() < 2) continue;
        PathColor const c = colorForPathId(p.pathId);
        constexpr uint8_t a = 220;
        for (size_t i = 0; i + 1 < p.nodes.size(); ++i)
        {
            PathNode const& a0 = p.nodes[i];
            PathNode const& a1 = p.nodes[i + 1];
            verts.push_back({ a0.x, a0.y, c.r, c.g, c.b, a });
            verts.push_back({ a1.x, a1.y, c.r, c.g, c.b, a });
        }
    }

    m_pathDirty = false;
    m_pathVertexCount = static_cast<GLsizei>(verts.size());

    m_pathVao.bind();
    m_pathVbo.bind();
    if (!verts.empty())
        m_pathVbo.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(PathVertex)));
    else
        m_pathVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_pathVbo.release();
    m_pathVao.release();

    // Re-upload the per-node markers alongside the path geometry so
    // they stay in sync with the rendered polylines.
    uploadPathNodeGeometry();
}

void NavMeshView::uploadPathNodeGeometry()
{
    // Same SoA layout as the spawn shader: per-vertex
    // (worldX, worldY, offsetX, offsetY, r, g, b, a).
    struct SpawnVertex
    {
        float x, y;
        float ox, oy;
        uint8_t r, g, b, a;
    };
    static_assert(sizeof(SpawnVertex) == 20, "SpawnVertex packed size drift");

    // Pre-count to reserve.
    size_t totalNodes = 0;
    for (Path const& p : m_paths)
        totalNodes += p.nodes.size();

    std::vector<SpawnVertex> verts;
    verts.reserve(totalNodes * 6);
    for (size_t pi = 0; pi < m_paths.size(); ++pi)
    {
        Path const& p = m_paths[pi];
        PathColor const c = colorForPathId(p.pathId);
        for (size_t ni = 0; ni < p.nodes.size(); ++ni)
        {
            PathNode const& n = p.nodes[ni];
            float drawX = n.x;
            float drawY = n.y;
            uint8_t r = c.r, g = c.g, b = c.b, a = 240;
            if (int(pi) == m_draggingPathIdx && int(ni) == m_draggingNodeIdx
                && m_pathDragDidMove)
            {
                drawX = m_pathDragCurrentWorld.x;
                drawY = m_pathDragCurrentWorld.y;
                r = 255; g = 220; b = 80; a = 220;
            }
            for (auto const& corner : SPAWN_QUAD_CORNERS)
                verts.push_back({ drawX, drawY, corner.x, corner.y, r, g, b, a });
        }
    }

    m_pathNodeVertexCount = static_cast<GLsizei>(verts.size());
    if (!m_buffersReady || !m_pathNodeVbo.isCreated())
        return;
    m_pathNodeVao.bind();
    m_pathNodeVbo.bind();
    if (!verts.empty())
        m_pathNodeVbo.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(SpawnVertex)));
    else
        m_pathNodeVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
                          reinterpret_cast<void*>(offsetof(SpawnVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
                          reinterpret_cast<void*>(offsetof(SpawnVertex, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpawnVertex),
                          reinterpret_cast<void*>(offsetof(SpawnVertex, r)));
    m_pathNodeVbo.release();
    m_pathNodeVao.release();
}

void NavMeshView::setLayerVisible(Layer layer, bool visible)
{
    auto const idx = static_cast<size_t>(layer);
    if (idx >= size_t(Layer::_Count))
        return;
    m_layerVisible[idx] = visible;
    update();
}

bool NavMeshView::isLayerVisible(Layer layer) const
{
    auto const idx = static_cast<size_t>(layer);
    if (idx >= size_t(Layer::_Count))
        return false;
    return m_layerVisible[idx];
}

void NavMeshView::setBattlemasterEntries(std::vector<uint32_t> entries)
{
    // Stored as a hash map (value unused) for O(1) per-spawn lookup
    // during the QPainter overlay pass.  std::unordered_set would be
    // semantically cleaner but the header already includes
    // <unordered_map> for m_spawnGroupColors, so we reuse that.
    m_battlemasterEntries.clear();
    m_battlemasterEntries.reserve(entries.size());
    for (uint32_t e : entries)
        m_battlemasterEntries.emplace(e, uint8_t(1));
    update();
}

void NavMeshView::setHighlightedSiblings(std::vector<int64_t> guids)
{
    // Cap matches MainWindow's collection limit (200 siblings); we still
    // accept anything the caller hands us but keep the membership set
    // sized to the actual payload so big DB-pumped vectors don't bloat
    // the per-paint lookup.
    m_siblingGuids = std::move(guids);
    m_siblingGuidSet.clear();
    m_siblingGuidSet.reserve(m_siblingGuids.size());
    for (int64_t g : m_siblingGuids)
        m_siblingGuidSet.insert(g);
    update();
}

void NavMeshView::setBattlemasterRange(float radiusYards)
{
    // Clamp matches the View menu QInputDialog range so QSettings/CLI
    // can't push the overlay into degenerate territory.
    m_battlemasterRadiusYards = std::clamp(radiusYards, 1.0f, 50.0f);
    update();
}

void NavMeshView::panTo(float worldX, float worldY, float yardsPerPixel)
{
    int const viewW = std::max(1, width());
    int const viewH = std::max(1, height());
    if (yardsPerPixel > 0.0f)
        m_view.yardsPerPixel = yardsPerPixel;
    m_view.anchorWorld  = coords::WorldPos{ worldX, worldY, 0.0f };
    m_view.anchorPixelX = viewW / 2;
    m_view.anchorPixelY = viewH / 2;
    update();
}

std::vector<coords::WorldPos> NavMeshView::findRoute(
    float startX, float startY, float startZ,
    float endX,   float endY,   float endZ,
    int   maxStraightPathPoints) const
{
    std::vector<coords::WorldPos> out;
    dtNavMesh const* nm = m_mesh.navmesh();
    if (!nm) return out;

    // Detour's coord convention is (x, y, z) where +Y is up.  TC world is
    // (X=north, Y=west, Z=up).  Detour buffers store TC tuples remapped:
    //   Detour[0] = TC Y    (existing code already reads bmin[2]=TC X,
    //   Detour[1] = TC Z     bmin[0]=TC Y in the heightmap tile walk,
    //   Detour[2] = TC X    so this mapping is consistent.)
    auto tcToDt = [](float tcX, float tcY, float tcZ, float* out_) {
        out_[0] = tcY;
        out_[1] = tcZ;
        out_[2] = tcX;
    };
    auto dtToTc = [](float const* dt) {
        return coords::WorldPos{ dt[2], dt[0], dt[1] };
    };

    // dtNavMeshQuery is heap-allocated because dtFreeNavMeshQuery /
    // dtAllocNavMeshQuery is how Detour wants it; using a local
    // unique_ptr with a custom deleter is the cleanest RAII shim.
    struct QueryDeleter { void operator()(dtNavMeshQuery* q) const { if (q) dtFreeNavMeshQuery(q); } };
    std::unique_ptr<dtNavMeshQuery, QueryDeleter> query(dtAllocNavMeshQuery());
    if (!query) return out;
    if (dtStatusFailed(query->init(nm, /*maxNodes*/ 4096)))
        return out;

    float startDt[3], endDt[3];
    tcToDt(startX, startY, startZ, startDt);
    tcToDt(endX,   endY,   endZ,   endDt);

    // Search box for findNearestPoly.  Generous on Y (up axis) because
    // bots may stand on a mesh well above the geometry sample we got.
    float const extents[3] = { 5.0f, 50.0f, 5.0f };

    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    dtPolyRef startRef = 0, endRef = 0;
    float startNearest[3], endNearest[3];
    if (dtStatusFailed(query->findNearestPoly(startDt, extents, &filter,
                                              &startRef, startNearest)))
        return out;
    if (startRef == 0)
        return out;
    if (dtStatusFailed(query->findNearestPoly(endDt, extents, &filter,
                                              &endRef, endNearest)))
        return out;
    if (endRef == 0)
        return out;

    // Poly-string path between start and end.
    std::vector<dtPolyRef> polyPath(maxStraightPathPoints);
    int polyPathCount = 0;
    if (dtStatusFailed(query->findPath(startRef, endRef,
                                       startNearest, endNearest,
                                       &filter,
                                       polyPath.data(),
                                       &polyPathCount,
                                       int(polyPath.size()))))
        return out;
    if (polyPathCount <= 0)
        return out;

    // Now resolve to straight (world-space) waypoints.  Detour's
    // findStraightPath includes the start point as the first entry;
    // we strip it so the caller can prepend their actual starting
    // node (which may differ slightly from `startNearest` after the
    // poly-snap).
    std::vector<float>      straight(maxStraightPathPoints * 3);
    std::vector<unsigned char> flags(maxStraightPathPoints);
    std::vector<dtPolyRef>  refs(maxStraightPathPoints);
    int straightCount = 0;
    if (dtStatusFailed(query->findStraightPath(
            startNearest, endNearest,
            polyPath.data(), polyPathCount,
            straight.data(), flags.data(), refs.data(),
            &straightCount, maxStraightPathPoints)))
        return out;

    out.reserve(straightCount > 0 ? size_t(straightCount - 1) : 0u);
    // Skip index 0 (start duplicate).
    for (int i = 1; i < straightCount; ++i)
    {
        float const* p = &straight[i * 3];
        out.push_back(dtToTc(p));
    }
    return out;
}

void NavMeshView::frameMesh()
{
    if (!m_meshBoundsValid)
        return;
    float const meshW = m_meshMaxY - m_meshMinY;
    float const meshH = m_meshMaxX - m_meshMinX;
    int const viewW = std::max(1, width());
    int const viewH = std::max(1, height());
    float const margin = 1.05f;
    float const yppX = (meshW * margin) / float(viewW);
    float const yppY = (meshH * margin) / float(viewH);
    m_view.yardsPerPixel = std::max({ yppX, yppY, 0.25f });
    m_view.anchorWorld = coords::WorldPos{
        (m_meshMinX + m_meshMaxX) * 0.5f,
        (m_meshMinY + m_meshMaxY) * 0.5f,
        0.0f };
    m_view.anchorPixelX = viewW / 2;
    m_view.anchorPixelY = viewH / 2;
    update();
}

void NavMeshView::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(16 / 255.0f, 16 / 255.0f, 20 / 255.0f, 1.0f); // mmap_world_dump BG.

    m_program = new QOpenGLShaderProgram(this);
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, VSHADER);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, FSHADER);
    m_program->bindAttributeLocation("in_world", 0);
    m_program->bindAttributeLocation("in_color", 1);
    m_program->link();

    m_uYardsPerPixel = m_program->uniformLocation("u_yardsPerPixel");
    m_uAnchorWorld   = m_program->uniformLocation("u_anchorWorld");
    m_uAnchorPixel   = m_program->uniformLocation("u_anchorPixel");
    m_uViewportSize  = m_program->uniformLocation("u_viewportSize");
    m_uViewRotation  = m_program->uniformLocation("u_viewRotationRad");

    m_vao.create();
    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    if (m_pendingDirty)
        uploadGeometry();

    // Spawn-icon pipeline.
    m_spawnProgram = new QOpenGLShaderProgram(this);
    m_spawnProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   SPAWN_VSHADER);
    m_spawnProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, SPAWN_FSHADER);
    m_spawnProgram->bindAttributeLocation("in_world",  0);
    m_spawnProgram->bindAttributeLocation("in_offset", 1);
    m_spawnProgram->bindAttributeLocation("in_color",  2);
    m_spawnProgram->link();
    m_spawnUYpp      = m_spawnProgram->uniformLocation("u_yardsPerPixel");
    m_spawnUAnchorW  = m_spawnProgram->uniformLocation("u_anchorWorld");
    m_spawnUAnchorP  = m_spawnProgram->uniformLocation("u_anchorPixel");
    m_spawnUViewport = m_spawnProgram->uniformLocation("u_viewportSize");
    m_spawnUPixelSize = m_spawnProgram->uniformLocation("u_pixelSize");
    m_spawnUViewRotation = m_spawnProgram->uniformLocation("u_viewRotationRad");

    m_spawnVao.create();
    m_spawnVbo.create();
    m_spawnVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_pendingSpawnDirty)
        uploadSpawnGeometry();

    // Path pipeline.
    m_pathProgram = new QOpenGLShaderProgram(this);
    m_pathProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   PATH_VSHADER);
    m_pathProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, PATH_FSHADER);
    m_pathProgram->bindAttributeLocation("in_world", 0);
    m_pathProgram->bindAttributeLocation("in_color", 1);
    m_pathProgram->link();
    m_pathUYpp      = m_pathProgram->uniformLocation("u_yardsPerPixel");
    m_pathUAnchorW  = m_pathProgram->uniformLocation("u_anchorWorld");
    m_pathUAnchorP  = m_pathProgram->uniformLocation("u_anchorPixel");
    m_pathUViewport = m_pathProgram->uniformLocation("u_viewportSize");
    m_pathUViewRotation = m_pathProgram->uniformLocation("u_viewRotationRad");
    m_pathVao.create();
    m_pathVbo.create();
    m_pathVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    if (m_pathDirty)
        uploadPathGeometry();

    // Road-graph polylines.  Same shader (path) -- just a second VBO.
    m_roadGraphVao.create();
    m_roadGraphVbo.create();
    m_roadGraphVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    rebuildRoadGraphBuffer();

    // Flight-path edge polylines (taxi_path).  Path shader; populated
    // whenever setFlightGraph() is called from MainWindow at map load.
    m_flightEdgeVao.create();
    m_flightEdgeVbo.create();
    m_flightEdgeVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    rebuildFlightEdgeBuffer();

    // Transport routes (transports + transport_animation).  Path shader,
    // populated by setTransportRoutes() from MainWindow at map load.
    m_transportRouteVao.create();
    m_transportRouteVbo.create();
    m_transportRouteVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    rebuildTransportRouteBuffer();

    // Path-debug polyline (live findRoute() preview).  Path shader.
    m_pathDebugVao.create();
    m_pathDebugVbo.create();
    m_pathDebugVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    rebuildPathDebugBuffer();

    // Road-overlay pipeline.  Dedicated shader (uniform color, no per-
    // vertex attributes beyond world XY) so the two passes (auto-extracted
    // gold + handcrafted coral) just rebind the program and flip u_color.
    m_roadProgram = new QOpenGLShaderProgram(this);
    m_roadProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   ROAD_VSHADER);
    m_roadProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, ROAD_FSHADER);
    m_roadProgram->bindAttributeLocation("in_world", 0);
    m_roadProgram->link();
    m_roadUYpp          = m_roadProgram->uniformLocation("u_yardsPerPixel");
    m_roadUAnchorW      = m_roadProgram->uniformLocation("u_anchorWorld");
    m_roadUAnchorP      = m_roadProgram->uniformLocation("u_anchorPixel");
    m_roadUViewport     = m_roadProgram->uniformLocation("u_viewportSize");
    m_roadUViewRotation = m_roadProgram->uniformLocation("u_viewRotationRad");
    m_roadUColor        = m_roadProgram->uniformLocation("u_color");
    m_roadVao.create();
    m_roadVbo.create();
    m_roadVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_handcraftedRoadVao.create();
    m_handcraftedRoadVbo.create();
    m_handcraftedRoadVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    if (m_pendingRoadDirty)
        uploadAutoRoadOverlay();
    if (m_pendingHandcraftedRoadDirty)
        uploadHandcraftedRoadOverlay();

    // Impact-preview triangle list (translucent yellow polygon highlight).
    // Re-uses the nav-mesh shader vertex layout (world XY + RGBA) so we
    // don't have to spin up a 5th shader for what is conceptually the
    // same surface fill.
    m_impactVao.create();
    m_impactVbo.create();
    m_impactVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    if (m_impactDirty)
        uploadImpactGeometry();

    // Per-node path markers.  Reuses the spawn shader (pixel-space
    // quads).
    m_pathNodeVao.create();
    m_pathNodeVbo.create();
    m_pathNodeVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    uploadPathNodeGeometry();

    // Quest starter <-> ender lines (path shader, GL_LINES).
    m_questLineVao.create();
    m_questLineVbo.create();
    m_questLineVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    rebuildQuestLineBuffer();

    // Areatrigger / Graveyard buffers share the spawn shader.
    m_atrVao.create();
    m_atrVbo.create();
    m_atrVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    if (m_atrDirty) uploadAreatriggerGeometry();
    m_atrShapeVao.create();
    m_atrShapeVbo.create();
    m_atrShapeVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    rebuildAreatriggerShapes();
    m_gyVao.create();
    m_gyVbo.create();
    m_gyVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    if (m_gyDirty) uploadGraveyardGeometry();

    // Annotation pipeline.
    m_annotProgram = new QOpenGLShaderProgram(this);
    m_annotProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   ANNOT_VSHADER);
    m_annotProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, ANNOT_FSHADER);
    m_annotProgram->bindAttributeLocation("in_center", 0);
    m_annotProgram->bindAttributeLocation("in_offset", 1);
    m_annotProgram->bindAttributeLocation("in_radius", 2);
    m_annotProgram->bindAttributeLocation("in_color",  3);
    m_annotProgram->link();
    m_annotUYpp      = m_annotProgram->uniformLocation("u_yardsPerPixel");
    m_annotUAnchorW  = m_annotProgram->uniformLocation("u_anchorWorld");
    m_annotUAnchorP  = m_annotProgram->uniformLocation("u_anchorPixel");
    m_annotUViewport = m_annotProgram->uniformLocation("u_viewportSize");
    m_annotUViewRotation = m_annotProgram->uniformLocation("u_viewRotationRad");

    m_annotVao.create();
    m_annotVbo.create();
    m_annotVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    if (m_pendingAnnotDirty)
        uploadAnnotationGeometry();

    // Heightmap pipeline.
    m_heightProgram = new QOpenGLShaderProgram(this);
    m_heightProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,   HEIGHT_VSHADER);
    m_heightProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, HEIGHT_FSHADER);
    m_heightProgram->bindAttributeLocation("in_corner", 0);
    m_heightProgram->link();
    m_heightUYpp        = m_heightProgram->uniformLocation("u_yardsPerPixel");
    m_heightUAnchorW    = m_heightProgram->uniformLocation("u_anchorWorld");
    m_heightUAnchorP    = m_heightProgram->uniformLocation("u_anchorPixel");
    m_heightUViewport   = m_heightProgram->uniformLocation("u_viewportSize");
    m_heightUTileBounds = m_heightProgram->uniformLocation("u_tileBounds");
    m_heightUViewRotation = m_heightProgram->uniformLocation("u_viewRotationRad");

    m_heightVao.create();
    m_heightVbo.create();
    m_heightVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_heightVao.bind();
    m_heightVbo.bind();
    m_heightVbo.allocate(HEIGHT_QUAD_CORNERS, int(sizeof(HEIGHT_QUAD_CORNERS)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    m_heightVbo.release();
    m_heightVao.release();
}

void NavMeshView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    // Anchor stays at the centre when the widget resizes so a frameMesh
    // call before resize stays visually centered.
    m_view.anchorPixelX = w / 2;
    m_view.anchorPixelY = h / 2;
}

void NavMeshView::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Safety-net flush for the handcrafted-road overlay: setHandcrafted
    // RoadPolylines may be called before the GL VBO is created OR before
    // the first setNavMesh (which is what flips m_buffersReady).  In
    // either case the dirty flag stays set; once we're inside paintGL the
    // context is live, so flush here so the next frame picks up the
    // pending vertices.  uploadHandcraftedRoadOverlay() itself early-outs
    // if the VBO/VAO aren't created.
    if (m_pendingHandcraftedRoadDirty && m_handcraftedRoadVbo.isCreated())
        uploadHandcraftedRoadOverlay();

    // Safety-net flush for the annotation overlay.  setAnnotations may be
    // called before initializeGL has run (creating m_annotVbo) when the
    // dock pre-populates from QSettings — same bug shape that bit the
    // handcrafted-road overlay.  Once we're in paintGL the GL context is
    // live, so flush here and the next frame picks up the discs.
    if (m_pendingAnnotDirty && m_annotVbo.isCreated() && m_annotVao.isCreated())
        uploadAnnotationGeometry();

    // Continent-level screen-space rotation, applied uniformly to every
    // 2D-viewer GL pipeline via the u_viewRotationRad uniform.  Computed
    // once per frame; bound by each shader-bind block below.  Mirror in
    // the QPainter overlay + mouse-pick path via worldToScreen2D /
    // screenToWorld2D so layer alignment is preserved.
    float const viewRotRad = m_viewRotationDegrees * float(M_PI) / 180.0f;

    // Heightmap is drawn FIRST so the nav polys + spawns + annotations
    // sit visibly on top of it.  Lazy build on the first paint after a
    // setNavMesh call.
    //
    // Big continents (Kalimdor / EK / Northrend / Pandaria / Broken Isles
    // / Zandalar / Kul Tiras / Shadowlands / Dragon Isles / Khaz Algar)
    // span 600-900 occupied tiles.  Building all of them on one paint
    // froze the UI for 5-15s.  We now seed a queue on the first paint
    // and process kHeightmapTilesPerFrame tiles per frame, re-scheduling
    // via update() until the queue drains.  The window stays interactive
    // (pan/zoom/menus respond) and the heightmap streams in tile-by-tile.
    constexpr int kHeightmapTilesPerFrame = 8;

    if (m_heightmapPending && m_mapCache && !m_mapCache->mapsDir().empty()
        && m_mesh.ok())
    {
        m_heightmapPending  = false;
        m_heightmapBuilding = true;
        m_heightmapBuilt    = false;
        m_heightmapBuildIndex = 0;
        m_heightmapBuildQueue.clear();
        m_streamProgressLoggedAt = 0;
        destroyHeightmapTextures();

        // Collect the unique set of (gx, gy) tiles to render.
        //
        // Coverage source -- UNION of:
        //   (a) dtNavMesh tiles loaded from <mmapsDir>/0000_<gx>_<gy>.mmtile
        //       (gated by which mmtile files the operator has regenerated
        //       locally; partial mmtile sets used to silently gate the
        //       entire minimap layer, per TILE_COVERAGE_DIAGNOSIS.md).
        //   (b) `.map` files on disk under <mapsDir>/<paddedMapId>_<gx>_<gy>.map,
        //       which carry full continent coverage independent of any
        //       navmesh regen state.
        // The negative-cache in loadOrUploadMinimapTile (m_minimapTextures[k]=0)
        // absorbs misses for ocean/missing tiles cheaply.  This restores
        // upper/lower-EK minimap coverage when the local mmtile set is
        // only the middle band.
        dtNavMesh const* nm = m_mesh.navmesh();
        int navTiles = 0;
        for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
        {
            dtMeshTile const* mt = nm->getTile(ti);
            if (!mt || !mt->header || mt->header->polyCount <= 0) continue;
            // bmin/bmax: index 0 = TC Y, 2 = TC X.
            float const minX = mt->header->bmin[2];
            float const minY = mt->header->bmin[0];
            int const gx = int(std::floor(coords::TILE_CENTER_INDEX - minX / coords::TILE_SIZE));
            int const gy = int(std::floor(coords::TILE_CENTER_INDEX - minY / coords::TILE_SIZE));
            m_heightmapBuildQueue.emplace_back(gx, gy);
            ++navTiles;
        }
        // Second seeding source: tiles discovered as `.map` files on disk.
        // Cached per-mapId so we don't rescan every frame.
        std::vector<std::pair<int, int>> const& diskTiles =
            scanMapFileCoverageForMapId(m_heightmapMapId);
        int const diskCount = int(diskTiles.size());
        for (auto const& [gx, gy] : diskTiles)
            m_heightmapBuildQueue.emplace_back(gx, gy);

        // Dedupe so re-scanning duplicate tile records doesn't cause us
        // to upload the same texture twice.
        std::sort(m_heightmapBuildQueue.begin(), m_heightmapBuildQueue.end());
        m_heightmapBuildQueue.erase(
            std::unique(m_heightmapBuildQueue.begin(), m_heightmapBuildQueue.end()),
            m_heightmapBuildQueue.end());
        m_heightmapTiles.reserve(m_heightmapBuildQueue.size());

        qDebug().nospace()
            << "[minimap] coverage: dtNavMesh=" << navTiles << " tiles, mapFiles="
            << diskCount << " tiles, union=" << int(m_heightmapBuildQueue.size()) << " tiles"
            << " (mapId=" << m_heightmapMapId << ")";
        // Companion line for the setMinimapTransform path: paintGL is the
        // first place we know the seeded queue size, so emit the operator-
        // visible "(N tiles to stream)" tail here.  See
        // restartMinimapStreaming for the earlier half of the message.
        qDebug().nospace()
            << "[minimap] streaming: ("
            << qulonglong(m_heightmapBuildQueue.size())
            << " tiles to stream)";
    }

    if (m_heightmapBuilding && m_mapCache && !m_mapCache->mapsDir().empty())
    {
        size_t const queueSize = m_heightmapBuildQueue.size();
        size_t const endIdx    = std::min(queueSize, m_heightmapBuildIndex + kHeightmapTilesPerFrame);
        for (size_t qi = m_heightmapBuildIndex; qi < endIdx; ++qi)
        {
            int const gx = m_heightmapBuildQueue[qi].first;
            int const gy = m_heightmapBuildQueue[qi].second;
        {
            float const worldMaxX = (coords::TILE_CENTER_INDEX - gx) * coords::TILE_SIZE;
            float const worldMinX = worldMaxX - coords::TILE_SIZE;
            float const worldMaxY = (coords::TILE_CENTER_INDEX - gy) * coords::TILE_SIZE;
            float const worldMinY = worldMaxY - coords::TILE_SIZE;

            float const ypp = (worldMaxX - worldMinX) / float(io::ADT_HEIGHT_GRID_V8);
            // Pull the tile via MapTileCache. heightAt does the lookup
            // we'd otherwise do manually; we want raw V8/V9 instead.
            float const sampleX = worldMinX + 0.5f * coords::TILE_SIZE;
            float const sampleY = worldMinY + 0.5f * coords::TILE_SIZE;
            (void)m_mapCache->heightAt(m_heightmapMapId, sampleX, sampleY);
            // After heightAt(), the tile is in the cache.  Re-fetch raw
            // via a public sample loop: walk the 128x128 V8 grid and
            // build an RGB texture greyscale-shaded by elevation.
            constexpr int W = io::ADT_HEIGHT_GRID_V8;
            std::vector<uint8_t> rgb(W * W * 3, 0);
            // Find min/max over the tile for normalization.
            //
            // Texel layout must match the heightmap fragment shader's
            // `v_uv = in_corner` plumbing: in_corner.x -> worldX
            // (mix(minX, maxX, in_corner.x)) and in_corner.y -> worldY
            // (mix(minY, maxY, in_corner.y)).  glTexImage2D treats
            // pixels[row*W+col] as texel (s=col, t=row), so:
            //   col index 0..W-1  ->  worldX  minX..maxX   (U / s axis)
            //   row index 0..W-1  ->  worldY  minY..maxY   (V / t axis)
            // Earlier revisions used row -> worldX and col -> worldY,
            // which transposed each tile's interior 90 degrees -- the
            // per-tile AABB still rendered in the right place so the
            // continent outline looked correct, but adjacent tile
            // textures never blended at their seams.
            float hmin = 1e9f, hmax = -1e9f;
            std::vector<float> samples(W * W);
            for (int row = 0; row < W; ++row)
            {
                for (int col = 0; col < W; ++col)
                {
                    float const wx = worldMinX + (float(col) + 0.5f) * (coords::TILE_SIZE / float(W));
                    float const wy = worldMinY + (float(row) + 0.5f) * (coords::TILE_SIZE / float(W));
                    float const h  = m_mapCache->heightAt(m_heightmapMapId, wx, wy);
                    samples[row * W + col] = h;
                    if (h > io::ADT_INVALID_HEIGHT) {
                        if (h < hmin) hmin = h;
                        if (h > hmax) hmax = h;
                    }
                }
            }
            if (hmax <= hmin) { hmin = 0.0f; hmax = 1.0f; }
            float const range = hmax - hmin;
            // Spacing between adjacent V8 samples in yards.  Used by the
            // slope-shading pass to convert dH/dpixel into a true slope.
            float const sampleSpacingYd = coords::TILE_SIZE / float(W);
            for (int row = 0; row < W; ++row)
            {
                for (int col = 0; col < W; ++col)
                {
                    float const h = samples[row * W + col];
                    uint8_t r, g, b;
                    if (h <= io::ADT_INVALID_HEIGHT)
                    {
                        r = 30; g = 30; b = 30; // hole / no data
                    }
                    else if (h < 0.0f)
                    {
                        // Below-water tint blue.
                        float const t = std::min(1.0f, -h / 60.0f);
                        r = uint8_t(40 + (1.0f - t) * 20);
                        g = uint8_t(60 + (1.0f - t) * 30);
                        b = uint8_t(120 + t * 80);
                    }
                    else
                    {
                        float const t = std::clamp((h - hmin) / range, 0.0f, 1.0f);
                        // Greyscale ramp 70..220 plus subtle warm tint at altitude.
                        uint8_t const v = uint8_t(70 + t * 150);
                        r = v;
                        g = uint8_t(std::min(255, int(v) + int(t * 18)));
                        b = uint8_t(v - int(t * 20));
                    }
                    // Slope shading: darken cliffs, lighten plateaus so
                    // the 2D map reads like a relief map instead of a
                    // flat elevation gradient.  Skip holes/water — only
                    // shade dry land.
                    if (h > io::ADT_INVALID_HEIGHT && h >= 0.0f)
                    {
                        int const colW = std::max(0, col - 1);
                        int const colE = std::min(W - 1, col + 1);
                        int const rowN = std::max(0, row - 1);
                        int const rowS = std::min(W - 1, row + 1);
                        float const hW = samples[row * W + colW];
                        float const hE = samples[row * W + colE];
                        float const hN = samples[rowN * W + col];
                        float const hS = samples[rowS * W + col];
                        // Treat invalid neighbours as the centre's own
                        // height so slope reads 0 at the hole boundary
                        // rather than spiking to a fake cliff.
                        float const sW = (hW > io::ADT_INVALID_HEIGHT) ? hW : h;
                        float const sE = (hE > io::ADT_INVALID_HEIGHT) ? hE : h;
                        float const sN = (hN > io::ADT_INVALID_HEIGHT) ? hN : h;
                        float const sS = (hS > io::ADT_INVALID_HEIGHT) ? hS : h;
                        float const dHx = (sE - sW) * 0.5f;
                        float const dHy = (sS - sN) * 0.5f;
                        float const slope = std::sqrt(dHx * dHx + dHy * dHy) / sampleSpacingYd;
                        // slope is dH per horizontal yard.  Cliffs run
                        // 0.5-2.0+; gentle terrain ~0.05.  Map into a
                        // darken factor 1.0 (no change) to ~0.55 for a
                        // sheer drop.  tan(45deg) = 1.0 corresponds to
                        // factor ~0.7, which reads convincingly as a
                        // mountain face on the 2D view.
                        float const shade = std::clamp(1.0f - slope * 0.45f, 0.55f, 1.0f);
                        r = uint8_t(float(r) * shade);
                        g = uint8_t(float(g) * shade);
                        b = uint8_t(float(b) * shade);
                    }
                    size_t const idx = (size_t(row) * W + col) * 3;
                    rgb[idx + 0] = r;
                    rgb[idx + 1] = g;
                    rgb[idx + 2] = b;
                }
            }
            // Upload.
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, W, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
            glBindTexture(GL_TEXTURE_2D, 0);

            HeightmapTile ht;
            ht.gx       = gx;
            ht.gy       = gy;
            ht.texture  = tex;
            ht.worldMinX = worldMinX;
            ht.worldMaxX = worldMaxX;
            ht.worldMinY = worldMinY;
            ht.worldMaxY = worldMaxY;
            m_heightmapTiles.push_back(ht);
            (void)ypp;

            // Probe + upload minimap tile in the same batch.  The
            // cache makes subsequent paints a hashmap lookup.
            (void)loadOrUploadMinimapTile(gx, gy);
        }
        } // end for (qi)
        m_heightmapBuildIndex = endIdx;
        // Streaming-progress log: every 50 tiles uploaded, emit a one-line
        // log entry so the operator can correlate transform changes with
        // the rate at which the chunked-build queue drains.  We compare
        // build-index to the last-logged threshold so we never emit twice
        // for the same boundary even when batches straddle it.
        size_t const milestone = (m_heightmapBuildIndex / 50) * 50;
        if (milestone > m_streamProgressLoggedAt)
        {
            m_streamProgressLoggedAt = milestone;
            if (milestone > 0)
            {
                qDebug().nospace()
                    << "[minimap] streaming progress: "
                    << qulonglong(milestone) << "/" << qulonglong(queueSize) << " tiles uploaded";
            }
        }
        if (m_heightmapBuildIndex >= queueSize)
        {
            m_heightmapBuilding = false;
            m_heightmapBuilt    = true;
            m_heightmapBuildQueue.clear();
            m_heightmapBuildIndex = 0;
        }
        else
        {
            // Re-schedule a paint so the next batch streams in without
            // waiting for user input.  Window stays responsive between
            // batches because Qt processes input events between paints.
            update();
        }
    }

    // Draw whatever heightmap tiles have streamed in so far -- partial
    // results render correctly while the chunked build is in progress.
    if (m_heightProgram && !m_heightmapTiles.empty() && isLayerVisible(Layer::Heightmap))
    {
        m_heightProgram->bind();
        m_heightProgram->setUniformValue(m_heightUYpp, m_view.yardsPerPixel);
        m_heightProgram->setUniformValue(m_heightUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_heightProgram->setUniformValue(m_heightUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_heightProgram->setUniformValue(m_heightUViewport,
            QVector2D(float(width()), float(height())));
        m_heightProgram->setUniformValue(m_heightUViewRotation, viewRotRad);

        m_heightVao.bind();
        for (HeightmapTile const& ht : m_heightmapTiles)
        {
            m_heightProgram->setUniformValue(m_heightUTileBounds,
                QVector4D(ht.worldMinX, ht.worldMaxX, ht.worldMinY, ht.worldMaxY));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ht.texture);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        m_heightVao.release();
        m_heightProgram->release();
    }

    // Minimap layer: draws extracted real-map PNG tiles using the same
    // height shader (which is just a textured screen-space quad shader
    // -- the heightmap shading happens during the per-tile texture
    // build, not in GLSL).  Tiles without a loaded PNG silently skip,
    // leaving the heightmap underneath visible.  This means the
    // operator who has extracted only Eastern Kingdoms minimaps still
    // sees Kalimdor / Outland as elevation shading without needing
    // every continent up-front.
    if (m_heightProgram && !m_heightmapTiles.empty()
        && !m_minimapTextures.empty() && isLayerVisible(Layer::Minimap))
    {
        m_heightProgram->bind();
        m_heightProgram->setUniformValue(m_heightUYpp, m_view.yardsPerPixel);
        m_heightProgram->setUniformValue(m_heightUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_heightProgram->setUniformValue(m_heightUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_heightProgram->setUniformValue(m_heightUViewport,
            QVector2D(float(width()), float(height())));
        m_heightProgram->setUniformValue(m_heightUViewRotation, viewRotRad);
        m_heightVao.bind();
        int drawn = 0, skipped = 0;
        for (HeightmapTile const& ht : m_heightmapTiles)
        {
            uint32_t const key = (uint32_t(ht.gy) << 16) | (uint32_t(ht.gx) & 0xFFFFu);
            auto it = m_minimapTextures.find(key);
            if (it == m_minimapTextures.end() || it->second == 0)
                { ++skipped; continue; }
            m_heightProgram->setUniformValue(m_heightUTileBounds,
                QVector4D(ht.worldMinX, ht.worldMaxX, ht.worldMinY, ht.worldMaxY));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, it->second);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ++drawn;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        m_heightVao.release();
        m_heightProgram->release();
        // Throttled diagnostic: once per second tell the operator how
        // many real-map tiles are landing vs being skipped.  This is the
        // single most useful signal when troubleshooting "I see the
        // heightmap but no textures" -- if drawn==0 the issue is in the
        // load path, not the render path.
        m_minimapTilesDrawnCount   = drawn;
        m_minimapTilesSkippedCount = skipped;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_minimapLogLastMs == 0 || nowMs - m_minimapLogLastMs >= 1000)
        {
            m_minimapLogLastMs = nowMs;
            qDebug().nospace()
                << "[minimap] mapId=" << m_heightmapMapId
                << " render: drawn=" << drawn << " skipped=" << skipped
                << " (cache=" << qulonglong(m_minimapTextures.size()) << ")";
        }
    }

    if (m_program && m_vertexCount > 0 && isLayerVisible(Layer::NavMesh))
    {
        m_program->bind();
        m_program->setUniformValue(m_uYardsPerPixel, m_view.yardsPerPixel);
        m_program->setUniformValue(m_uAnchorWorld,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_program->setUniformValue(m_uAnchorPixel,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_program->setUniformValue(m_uViewportSize,
            QVector2D(float(width()), float(height())));
        m_program->setUniformValue(m_uViewRotation, viewRotRad);

        m_vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
        m_vao.release();
        m_program->release();
    }

    // Road overlay: walks the dtNavMesh's NAV_AREA_ROAD polygons to
    // surface the auto-extracted skeleton (gold), then overlays an
    // optional handcrafted polyline set (coral red) pushed by an external
    // agent.  Drawn AFTER the heightmap / minimap / navmesh passes so the
    // polylines sit on top of the basemap but BEFORE spawn / annotation /
    // path overlays so clickable entities still occlude the lines.
    // Diagnostic: one qDebug per (roads-layer-on, handcrafted-vertex-count)
    // state transition so the trace shows whether the draw pass would
    // actually emit handcrafted lines.  Static cache because paintGL is a
    // hot loop and we don't want a log per frame.
    {
        static bool   s_lastVisible = false;
        static GLsizei s_lastCount  = -1;
        bool const visible = isLayerVisible(Layer::Roads);
        if (visible != s_lastVisible || m_handcraftedRoadVertexCount != s_lastCount)
        {
            qDebug() << "[handcrafted-road] paintGL: drawing"
                     << m_handcraftedRoadVertexCount
                     << "handcrafted vertices, layer visible="
                     << (visible ? 'Y' : 'N');
            s_lastVisible = visible;
            s_lastCount   = m_handcraftedRoadVertexCount;
        }
    }
    if (m_roadProgram && isLayerVisible(Layer::Roads)
        && (m_roadVertexCount > 0 || m_handcraftedRoadVertexCount > 0))
    {
        m_roadProgram->bind();
        m_roadProgram->setUniformValue(m_roadUYpp, m_view.yardsPerPixel);
        m_roadProgram->setUniformValue(m_roadUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_roadProgram->setUniformValue(m_roadUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_roadProgram->setUniformValue(m_roadUViewport,
            QVector2D(float(width()), float(height())));
        m_roadProgram->setUniformValue(m_roadUViewRotation, viewRotRad);
        glLineWidth(2.0f);
        if (m_roadVertexCount > 0)
        {
            // Auto-extracted = warm gold/yellow; readable against both
            // dark navmesh shading and bright minimap tiles.
            m_roadProgram->setUniformValue(m_roadUColor,
                QVector4D(0.9f, 0.75f, 0.2f, 0.9f));
            m_roadVao.bind();
            glDrawArrays(GL_LINES, 0, m_roadVertexCount);
            m_roadVao.release();
        }
        if (m_handcraftedRoadVertexCount > 0)
        {
            // Handcrafted = coral red so it visibly differs from the
            // auto-extracted gold even where the two networks overlap.
            m_roadProgram->setUniformValue(m_roadUColor,
                QVector4D(0.95f, 0.4f, 0.4f, 0.9f));
            m_handcraftedRoadVao.bind();
            glDrawArrays(GL_LINES, 0, m_handcraftedRoadVertexCount);
            m_handcraftedRoadVao.release();
        }
        m_roadProgram->release();
    }

    // In-progress handcrafted-road placement preview: when the operator
    // is in WaitingForEnd state, paint a single coral-red line from the
    // captured start point to the current mouse-hover world position so
    // the segment-being-placed is visible before the second click.
    if (m_roadProgram && m_segmentPlacementState == SegmentPlacementState::WaitingForEnd)
    {
        // Stage a 2-vertex GL_LINES draw without touching the persistent
        // handcrafted VBO so the live preview can't pollute it.
        QVector2D verts[2] = {
            QVector2D(m_segmentStartWorld.x, m_segmentStartWorld.y),
            QVector2D(m_segmentHoverWorld.x, m_segmentHoverWorld.y),
        };
        QOpenGLBuffer scratch(QOpenGLBuffer::VertexBuffer);
        scratch.create();
        scratch.setUsagePattern(QOpenGLBuffer::StreamDraw);
        QOpenGLVertexArrayObject scratchVao;
        scratchVao.create();
        scratchVao.bind();
        scratch.bind();
        scratch.allocate(verts, int(sizeof(verts)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QVector2D),
                              reinterpret_cast<void*>(0));
        m_roadProgram->bind();
        m_roadProgram->setUniformValue(m_roadUYpp, m_view.yardsPerPixel);
        m_roadProgram->setUniformValue(m_roadUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_roadProgram->setUniformValue(m_roadUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_roadProgram->setUniformValue(m_roadUViewport,
            QVector2D(float(width()), float(height())));
        m_roadProgram->setUniformValue(m_roadUViewRotation, viewRotRad);
        m_roadProgram->setUniformValue(m_roadUColor,
            QVector4D(0.95f, 0.55f, 0.45f, 0.95f));
        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, 2);
        m_roadProgram->release();
        scratch.release();
        scratchVao.release();
        scratch.destroy();
        scratchVao.destroy();
    }

    // Impact preview overlay: translucent yellow triangle fan of every
    // poly the candidate segment would retag.  Auto-clears after
    // kImpactPreviewLifetimeMs ms via the deadline check.  Drawn AFTER
    // road overlay so the highlight reads on top of both gold + coral.
    if (m_program && m_impactVertexCount > 0)
    {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs >= m_impactDeadlineMs)
        {
            // Deadline elapsed: drop the highlight on the next paint.
            m_impactVertexCount = 0;
            m_pendingImpactVerts.clear();
            uploadImpactGeometry();
        }
        else
        {
            m_program->bind();
            m_program->setUniformValue(m_uYardsPerPixel, m_view.yardsPerPixel);
            m_program->setUniformValue(m_uAnchorWorld,
                QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
            m_program->setUniformValue(m_uAnchorPixel,
                QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
            m_program->setUniformValue(m_uViewportSize,
                QVector2D(float(width()), float(height())));
            m_program->setUniformValue(m_uViewRotation, viewRotRad);
            m_impactVao.bind();
            glDrawArrays(GL_TRIANGLES, 0, m_impactVertexCount);
            m_impactVao.release();
            m_program->release();
            // Schedule a repaint so the highlight expires on time without
            // requiring user interaction.
            update();
        }
    }

    if (m_annotProgram && m_annotVertexCount > 0 && isLayerVisible(Layer::Annotations))
    {
        m_annotProgram->bind();
        m_annotProgram->setUniformValue(m_annotUYpp, m_view.yardsPerPixel);
        m_annotProgram->setUniformValue(m_annotUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_annotProgram->setUniformValue(m_annotUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_annotProgram->setUniformValue(m_annotUViewport,
            QVector2D(float(width()), float(height())));
        m_annotProgram->setUniformValue(m_annotUViewRotation, viewRotRad);

        m_annotVao.bind();
        glDrawArrays(GL_TRIANGLES, 0, m_annotVertexCount);
        m_annotVao.release();
        m_annotProgram->release();
    }

    // Paths drawn after navmesh + annotations but before spawns so the
    // dots stay on top of the line crossings they may overlap.
    if (m_pathProgram && m_pathVertexCount > 0 && isLayerVisible(Layer::Paths))
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(2.0f);
        m_pathVao.bind();
        glDrawArrays(GL_LINES, 0, m_pathVertexCount);
        m_pathVao.release();
        m_pathProgram->release();
    }

    // Road-graph polylines: thin lines connecting AnnotationKind::Road
    // points that are within ~3 * default-radius of each other.  Drawn
    // after waypoints so the operator can distinguish the two.
    if (m_pathProgram && m_roadGraphVertexCount > 0 && isLayerVisible(Layer::Annotations))
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(1.5f);
        m_roadGraphVao.bind();
        glDrawArrays(GL_LINES, 0, m_roadGraphVertexCount);
        m_roadGraphVao.release();
        m_pathProgram->release();
    }

    // Flight-path edges: light blue GL_LINES between taxi nodes that
    // share a taxi_path row.  Drawn after the road graph so its color
    // (orange) and the flight color (blue) read distinctly when both
    // layers happen to overlap.  Nodes themselves are painted in the
    // QPainter pass below alongside their text labels.
    if (m_pathProgram && m_flightEdgeVertexCount > 0 && isLayerVisible(Layer::FlightPaths))
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(1.5f);
        m_flightEdgeVao.bind();
        glDrawArrays(GL_LINES, 0, m_flightEdgeVertexCount);
        m_flightEdgeVao.release();
        m_pathProgram->release();
    }

    // Transport routes: thick orange polylines connecting consecutive
    // transport_animation keyframes.  Drawn on top of flight + road so
    // the operator can read transport paths over busy capital terrain.
    if (m_pathProgram && m_transportRouteVertexCount > 0 && isLayerVisible(Layer::TransportRoutes))
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(4.0f);
        m_transportRouteVao.bind();
        glDrawArrays(GL_LINES, 0, m_transportRouteVertexCount);
        m_transportRouteVao.release();
        m_pathProgram->release();
    }

    // Path-debug polyline: drawn thick + on top of nav/road so the
    // operator can read the route at any zoom.  Color is per-segment
    // (set in rebuildPathDebugBuffer): green<10y / yellow<30y / red>=30y.
    if (m_pathProgram && m_pathDebugVertexCount > 0 && m_pathDebugMode)
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(3.0f);
        m_pathDebugVao.bind();
        glDrawArrays(GL_LINES, 0, m_pathDebugVertexCount);
        m_pathDebugVao.release();
        m_pathProgram->release();
    }

    auto drawWithSpawnShader = [&](QOpenGLVertexArrayObject& vao, GLsizei count, float pixelRadius)
    {
        if (!m_spawnProgram || count <= 0) return;
        m_spawnProgram->bind();
        m_spawnProgram->setUniformValue(m_spawnUYpp,      m_view.yardsPerPixel);
        m_spawnProgram->setUniformValue(m_spawnUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_spawnProgram->setUniformValue(m_spawnUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_spawnProgram->setUniformValue(m_spawnUViewport,
            QVector2D(float(width()), float(height())));
        m_spawnProgram->setUniformValue(m_spawnUViewRotation, viewRotRad);
        m_spawnProgram->setUniformValue(m_spawnUPixelSize, pixelRadius);
        vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, count);
        vao.release();
        m_spawnProgram->release();
    };

    if (m_atrVertexCount > 0 && isLayerVisible(Layer::Areatriggers))
        drawWithSpawnShader(m_atrVao, m_atrVertexCount, 5.0f);
    // Areatrigger shape outlines: reuse the path shader (world XY + RGBA).
    if (m_pathProgram && m_atrShapeVertexCount > 0 && isLayerVisible(Layer::Areatriggers))
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(1.5f);
        m_atrShapeVao.bind();
        glDrawArrays(GL_LINES, 0, m_atrShapeVertexCount);
        m_atrShapeVao.release();
        m_pathProgram->release();
    }
    if (m_gyVertexCount > 0 && isLayerVisible(Layer::Graveyards))
        drawWithSpawnShader(m_gyVao, m_gyVertexCount, 7.0f);
    // Path-node markers drawn just before spawn icons so spawn icons
    // (which are usually atop the path) remain visible.  Smaller radius
    // than spawn icons so nodes don't visually dominate the polylines.
    if (m_pathNodeVertexCount > 0 && isLayerVisible(Layer::Paths))
        drawWithSpawnShader(m_pathNodeVao, m_pathNodeVertexCount, 4.0f);
    if (m_spawnVertexCount > 0 && isLayerVisible(Layer::Spawns))
        drawWithSpawnShader(m_spawnVao, m_spawnVertexCount, SPAWN_ICON_PIXEL_RADIUS);

    // Quest starter/ender connector lines (path shader, GL_LINES).
    // Drawn AFTER spawn icons so the lines appear above the icons; the
    // glyphs that come next then sit on top of both.
    if (m_pathProgram && m_questLineVertexCount > 0 && isLayerVisible(Layer::Quests))
    {
        m_pathProgram->bind();
        m_pathProgram->setUniformValue(m_pathUYpp, m_view.yardsPerPixel);
        m_pathProgram->setUniformValue(m_pathUAnchorW,
            QVector2D(m_view.anchorWorld.x, m_view.anchorWorld.y));
        m_pathProgram->setUniformValue(m_pathUAnchorP,
            QVector2D(float(m_view.anchorPixelX), float(m_view.anchorPixelY)));
        m_pathProgram->setUniformValue(m_pathUViewport,
            QVector2D(float(width()), float(height())));
        m_pathProgram->setUniformValue(m_pathUViewRotation, viewRotRad);
        glLineWidth(1.5f);
        m_questLineVao.bind();
        glDrawArrays(GL_LINES, 0, m_questLineVertexCount);
        m_questLineVao.release();
        m_pathProgram->release();
    }

    // QPainter overlay for quest-marker glyphs.  We pre-collect every
    // glyph + screen pos so a single QPainter setup covers the whole
    // batch (QPainter on a QOpenGLWidget creates a software-rasterized
    // pass; setting it up is expensive but cheap to reuse).
    bool const needsObjectiveOverlay = isLayerVisible(Layer::Quests)
        && m_questObjectivesVisible
        && !m_questObjectiveMarkers.empty();
    bool const needsPathDebugMarkers = m_pathDebugMode && m_pathDebugState >= 1;
    bool const needsBattlemasterRange = isLayerVisible(Layer::BattlemasterRange)
        && !m_battlemasterEntries.empty()
        && !m_spawns.empty()
        && m_battlemasterRadiusYards > 0.0f
        && m_view.yardsPerPixel > 0.0f;
    // SpawnDensity heatmap: only renders when toggled on and there is
    // at least one spawn to bucket.  The grid is rebuilt lazily below
    // if dirty.
    bool const needsSpawnDensity = isLayerVisible(Layer::SpawnDensity)
        && !m_spawns.empty()
        && m_view.yardsPerPixel > 0.0f;
    bool const needsFlightNodes = isLayerVisible(Layer::FlightPaths)
        && !m_flightNodes.empty();
    bool const needsGatheringNodes = isLayerVisible(Layer::GatheringNodes)
        && !m_gatheringNodes.empty()
        && !m_spawns.empty();
    // Sibling-highlight: 2px golden ring at icon_radius+4 around every
    // spawn whose guid sits in the m_siblingGuidSet.  Layer-gated so the
    // menu toggle can suppress the visual without dropping the set.
    bool const needsSiblingHighlight = isLayerVisible(Layer::SiblingHighlight)
        && !m_siblingGuidSet.empty()
        && !m_spawns.empty();
    // Instance-entrance painter pass: purple ring + italic target-map label.
    bool const needsInstanceEntrances = isLayerVisible(Layer::InstanceEntrance)
        && !m_instanceEntrances.empty();
    // 2D WMO footprint rectangles.  Layer::WmoOutline is shared with the
    // 3D WMO mesh toggle, but in this 2D viewer only the footprint geometry
    // listens to it -- the 3D mesh-visibility lives on SceneView3D.
    bool const needsWmoFootprints = isLayerVisible(Layer::WmoOutline)
        && !m_wmoFootprints.empty();
    // linked_respawn dependency segments.  Resolved against m_spawns at
    // paint time so spawn-list churn doesn't invalidate the link cache.
    bool const needsSpawnLinks = isLayerVisible(Layer::SpawnLinks)
        && !m_spawnLinks.empty()
        && !m_spawns.empty();
    // Snap candidates + crossroad nodes: painted from the persisted
    // handcrafted-road endpoint pairs.  The snap pass is gated on the
    // placement FSM being in WaitingForEnd; the crossroad pass surfaces
    // any world position where 3+ segments share an endpoint and runs
    // whenever the Roads layer is on so the operator can see junctions
    // even when not actively placing.
    bool const needsSnapHints = (m_segmentPlacementState == SegmentPlacementState::WaitingForEnd
                                 || m_segmentPlacementState == SegmentPlacementState::WaitingForStart)
        && !m_pendingHandcraftedRoadVertices.empty();
    bool const needsCrossroadNodes = isLayerVisible(Layer::Roads)
        && !m_pendingHandcraftedRoadVertices.empty();
    // Annotation hover/select highlight: a bright ring around the disc +
    // a small label tooltip near the cursor.  Layer-gated by
    // Layer::Annotations so toggling the layer off hides every annotation-
    // related visual (disc, hover ring, selection ring) in lockstep.
    bool const needsAnnotationHighlight = isLayerVisible(Layer::Annotations)
        && !m_annotations.empty()
        && (m_hoveredAnnotation >= 0 || m_selectedAnnotation >= 0);
    bool const needsPainter = m_boxSelecting
        || (isLayerVisible(Layer::Quests) && !m_questMarkers.empty())
        || needsObjectiveOverlay
        || needsPathDebugMarkers
        || needsBattlemasterRange
        || needsSpawnDensity
        || needsFlightNodes
        || needsGatheringNodes
        || needsSiblingHighlight
        || needsInstanceEntrances
        || needsWmoFootprints
        || needsSpawnLinks
        || needsSnapHints
        || needsCrossroadNodes
        || needsAnnotationHighlight;
    if (needsPainter)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // SpawnDensity layer: translucent 50-yard cells tinted by
        // bucketed spawn count.  Drawn FIRST so quest glyphs, path
        // markers, etc. paint on top.  Alpha ~80 keeps the GL terrain
        // pass visible underneath.
        if (needsSpawnDensity)
        {
            if (m_spawnDensityDirty)
                rebuildSpawnDensityGrid();

            // Visible-world AABB in TC (X, Y) so we can cull cells off-screen.
            // With m_viewRotationDegrees != 0 the visible region is no longer
            // axis-aligned in world space, so we min/max over all 4 screen
            // corners (covers the rotated bounding box conservatively).
            coords::WorldPos const wTL = screenToWorld2D(0.0f, 0.0f);
            coords::WorldPos const wTR = screenToWorld2D(float(width()), 0.0f);
            coords::WorldPos const wBR = screenToWorld2D(float(width()), float(height()));
            coords::WorldPos const wBL = screenToWorld2D(0.0f, float(height()));
            float const visMinX = std::min({ wTL.x, wTR.x, wBR.x, wBL.x });
            float const visMaxX = std::max({ wTL.x, wTR.x, wBR.x, wBL.x });
            float const visMinY = std::min({ wTL.y, wTR.y, wBR.y, wBL.y });
            float const visMaxY = std::max({ wTL.y, wTR.y, wBR.y, wBL.y });

            // Color ramp matching the spec: 0 invisible, 1-5 light
            // green, 6-15 yellow, 16-30 orange, 31-50 red, 51+ bright red.
            auto rampColor = [](uint32_t count) -> QColor {
                if (count == 0)         return QColor(  0,   0,   0,   0);
                if (count <= 5)         return QColor(140, 220, 120,  80);
                if (count <= 15)        return QColor(240, 220,  80,  80);
                if (count <= 30)        return QColor(240, 160,  60,  80);
                if (count <= 50)        return QColor(230,  80,  60,  80);
                return                         QColor(255,  30,  30,  80);
            };

            painter.save();
            painter.setPen(Qt::NoPen);
            for (auto const& [k, count] : m_spawnDensityCells)
            {
                float const cellMinX = float(k.cx) * kSpawnDensityCellYards;
                float const cellMaxX = cellMinX + kSpawnDensityCellYards;
                float const cellMinY = float(k.cy) * kSpawnDensityCellYards;
                float const cellMaxY = cellMinY + kSpawnDensityCellYards;
                // Cull cells fully outside the viewport.
                if (cellMaxX < visMinX || cellMinX > visMaxX
                    || cellMaxY < visMinY || cellMinY > visMaxY)
                    continue;
                // Project all 4 corners so cardinal (0/90/180/270) view
                // rotations produce an accurate screen-aligned rect.
                auto const [sx0, sy0] = worldToScreen2D({ cellMinX, cellMinY, 0.0f });
                auto const [sx1, sy1] = worldToScreen2D({ cellMaxX, cellMinY, 0.0f });
                auto const [sx2, sy2] = worldToScreen2D({ cellMaxX, cellMaxY, 0.0f });
                auto const [sx3, sy3] = worldToScreen2D({ cellMinX, cellMaxY, 0.0f });
                float const rx = std::min({ sx0, sx1, sx2, sx3 });
                float const ry = std::min({ sy0, sy1, sy2, sy3 });
                float const rw = std::max({ sx0, sx1, sx2, sx3 }) - rx;
                float const rh = std::max({ sy0, sy1, sy2, sy3 }) - ry;
                if (rw < 1.0f || rh < 1.0f)
                    continue;
                painter.setBrush(rampColor(count));
                painter.drawRect(QRectF(rx, ry, rw, rh));
            }
            painter.restore();
        }

        // Path-debug start/end markers.  Lime green disc for start
        // (always rendered when state>=1), red disc for end (only
        // rendered after state>=2).  Black outline so they read on
        // bright nav-area colors.
        if (needsPathDebugMarkers)
        {
            painter.save();
            auto paintMarker = [&](coords::WorldPos const& p, QColor const& fill, QChar glyph)
            {
                auto const [sx, sy] = worldToScreen2D(p);
                if (sx < -32 || sx > width() + 32 || sy < -32 || sy > height() + 32)
                    return;
                int const cx = int(sx), cy = int(sy);
                painter.setPen(QPen(QColor(0, 0, 0, 230), 2));
                painter.setBrush(fill);
                painter.drawEllipse(QPoint(cx, cy), 6, 6);
                QFont f = painter.font();
                f.setBold(true);
                f.setPointSizeF(9.0);
                painter.setFont(f);
                painter.setPen(QColor(0, 0, 0, 240));
                painter.drawText(cx - 3, cy + 4, QString(glyph));
            };
            paintMarker(m_pathDebugStart, QColor( 80, 240,  80, 230), QChar('S'));
            if (m_pathDebugState >= 2)
                paintMarker(m_pathDebugEnd, QColor(240,  70,  70, 230), QChar('E'));
            painter.restore();
        }

        // Box-select overlay: dashed yellow rect painted on top of GL.
        if (m_boxSelecting)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, false);
            QPen pen(QColor(255, 210, 80, 220));
            pen.setStyle(Qt::DashLine);
            pen.setWidth(1);
            painter.setPen(pen);
            painter.setBrush(QColor(255, 210, 80, 30));
            QRect const rect = QRect(m_boxStartScreen, m_boxCurrentScreen).normalized();
            painter.drawRect(rect);
            painter.restore();
        }

        if (isLayerVisible(Layer::Quests) && !m_questMarkers.empty())
        {
            // Big legible glyphs over the spawn icons.  ? for starters,
            // ! for enders.  Color by faction (alliance blue / horde
            // red / neutral gold) so the operator can read quest
            // affiliations at a glance.
            QFont f = painter.font();
            f.setBold(true);
            f.setPointSizeF(10.5);
            painter.setFont(f);
            constexpr int kHalo = 1; // pixel shadow halo for legibility
            auto factionColor = [](uint8_t faction, bool isStarter) -> QColor {
                // Alliance blue, Horde red, neutral gold for starters;
                // a slightly desaturated variant for enders so the two
                // glyphs don't visually collide at the same NPC.
                switch (faction)
                {
                    case 1: return isStarter ? QColor( 90, 160, 255, 255)
                                             : QColor( 60, 120, 220, 255);
                    case 2: return isStarter ? QColor(255,  90,  90, 255)
                                             : QColor(220,  60,  60, 255);
                    case 3: return isStarter ? QColor(220, 150, 255, 255)
                                             : QColor(180, 110, 220, 255);
                    default: return isStarter ? QColor(255, 230,  60, 255)
                                              : QColor(255, 150,  30, 255);
                }
            };
            for (QuestMarker const& m : m_questMarkers)
            {
                auto const [sx, sy] = worldToScreen2D({ m.x, m.y, m.z });
                if (sx < -32 || sx > width() + 32 || sy < -32 || sy > height() + 32)
                    continue;
                int const xs = int(sx) - 6;
                int const xe = int(sx) + 4;
                int const yt = int(sy) - 9;
                if (!m.startsQuests.empty())
                {
                    painter.setPen(QColor(0, 0, 0, 220));
                    for (int dx = -kHalo; dx <= kHalo; ++dx)
                    for (int dy = -kHalo; dy <= kHalo; ++dy)
                    if (dx || dy)
                        painter.drawText(xs + dx, yt + dy, QStringLiteral("?"));
                    painter.setPen(factionColor(m.faction, /*isStarter*/ true));
                    painter.drawText(xs, yt, QStringLiteral("?"));
                }
                if (!m.endsQuests.empty())
                {
                    int const dx2 = m.startsQuests.empty() ? 0 : 8;
                    painter.setPen(QColor(0, 0, 0, 220));
                    for (int dx = -kHalo; dx <= kHalo; ++dx)
                    for (int dy = -kHalo; dy <= kHalo; ++dy)
                    if (dx || dy)
                        painter.drawText(xe + dx2 + dx, yt + dy, QStringLiteral("!"));
                    painter.setPen(factionColor(m.faction, /*isStarter*/ false));
                    painter.drawText(xe + dx2, yt, QStringLiteral("!"));
                }
            }
        }

        // Quest-objective overlay: kind-coded icons offset 4px south of
        // the spawn center.  If the spawn satisfies multiple objective
        // kinds (kill + gather, etc.) the icons stack left-to-right at
        // the same vertical offset so each kind is independently
        // legible.  Order: kill, gather, interact, talk, explore.
        if (needsObjectiveOverlay)
        {
            constexpr int kIconHalfSize = 3;     // 6px-wide icons.
            constexpr int kIconSpacing  = 8;     // Horizontal stride when stacking.
            constexpr int kVerticalOffset = 4;   // Down from spawn center.

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);

            QColor const haloColor(0, 0, 0, 200);
            QPen   haloPen(haloColor);
            haloPen.setWidth(1);

            QColor const killColor    (230,  60,  60, 230);  // Red triangle (sword tip down).
            QColor const gatherColor  (240, 150,  40, 230);  // Orange diamond.
            QColor const interactColor( 80, 140, 240, 230);  // Blue square.
            QColor const talkColor    ( 80, 220, 110, 230);  // Green circle.
            QColor const exploreColor (210, 130, 240, 230);  // Purple star.

            for (QuestObjectiveMarker const& mo : m_questObjectiveMarkers)
            {
                if (mo.kinds == 0) continue;
                auto const [sx, sy] = worldToScreen2D({ mo.x, mo.y, mo.z });
                if (sx < -32 || sx > width() + 32 || sy < -32 || sy > height() + 32)
                    continue;

                // Count active bits to compute the row start so the
                // icon strip is centered on the spawn icon (cosmetic).
                int activeKinds = 0;
                for (int b = 0; b < 5; ++b) if (mo.kinds & (1u << b)) ++activeKinds;
                int slotIdx = 0;
                int const xStart = int(sx) - ((activeKinds - 1) * kIconSpacing) / 2;
                int const yCenter = int(sy) + kVerticalOffset;

                auto drawHaloed = [&](int cx, QColor const& fill, auto&& shape)
                {
                    painter.setPen(haloPen);
                    painter.setBrush(fill);
                    shape(cx, yCenter);
                };

                if (mo.kinds & 0x01) // kill -> red triangle pointing down.
                {
                    int const cx = xStart + slotIdx * kIconSpacing;
                    drawHaloed(cx, killColor, [&](int x, int y)
                    {
                        QPolygon p;
                        p << QPoint(x - kIconHalfSize, y - kIconHalfSize)
                          << QPoint(x + kIconHalfSize, y - kIconHalfSize)
                          << QPoint(x,                 y + kIconHalfSize + 1);
                        painter.drawPolygon(p);
                    });
                    ++slotIdx;
                }
                if (mo.kinds & 0x02) // gather -> orange diamond.
                {
                    int const cx = xStart + slotIdx * kIconSpacing;
                    drawHaloed(cx, gatherColor, [&](int x, int y)
                    {
                        QPolygon p;
                        p << QPoint(x,                     y - kIconHalfSize - 1)
                          << QPoint(x + kIconHalfSize + 1, y)
                          << QPoint(x,                     y + kIconHalfSize + 1)
                          << QPoint(x - kIconHalfSize - 1, y);
                        painter.drawPolygon(p);
                    });
                    ++slotIdx;
                }
                if (mo.kinds & 0x04) // interact -> blue square.
                {
                    int const cx = xStart + slotIdx * kIconSpacing;
                    drawHaloed(cx, interactColor, [&](int x, int y)
                    {
                        painter.drawRect(QRect(x - kIconHalfSize, y - kIconHalfSize,
                                               kIconHalfSize * 2, kIconHalfSize * 2));
                    });
                    ++slotIdx;
                }
                if (mo.kinds & 0x08) // talk -> green small circle.
                {
                    int const cx = xStart + slotIdx * kIconSpacing;
                    drawHaloed(cx, talkColor, [&](int x, int y)
                    {
                        painter.drawEllipse(QPoint(x, y), kIconHalfSize - 1, kIconHalfSize - 1);
                    });
                    ++slotIdx;
                }
                if (mo.kinds & 0x10) // explore -> purple 5-point star (compact).
                {
                    int const cx = xStart + slotIdx * kIconSpacing;
                    drawHaloed(cx, exploreColor, [&](int x, int y)
                    {
                        QPolygon p;
                        // 10 vertices alternating outer/inner radius.
                        constexpr double kOuter = 3.5;
                        constexpr double kInner = 1.5;
                        for (int i = 0; i < 10; ++i)
                        {
                            double const ang = -M_PI / 2.0 + double(i) * (M_PI / 5.0);
                            double const r   = (i & 1) ? kInner : kOuter;
                            p << QPoint(x + int(std::cos(ang) * r),
                                        y + int(std::sin(ang) * r));
                        }
                        painter.drawPolygon(p);
                    });
                    ++slotIdx;
                }
            }
            painter.restore();
        }

        // Battlemaster recruitment-radius overlay: 1px dashed yellow
        // ring around every creature spawn whose entry is flagged
        // UNIT_NPC_FLAG_BATTLEMASTER.  Phase-filter-aware (spawns hidden
        // by the phase mask don't get a circle either).
        if (needsBattlemasterRange)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(QColor(255, 220, 60, 220));
            pen.setStyle(Qt::DashLine);
            pen.setWidthF(1.0);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            float const radiusPixels = m_battlemasterRadiusYards / m_view.yardsPerPixel;
            int const margin = int(radiusPixels) + 8;
            for (Spawn const& s : m_spawns)
            {
                if (s.kind != SpawnKind::Creature)
                    continue;
                if (!spawnPassesPhaseFilter(s))
                    continue;
                if (m_battlemasterEntries.find(s.entry) == m_battlemasterEntries.end())
                    continue;
                auto const [sx, sy] = worldToScreen2D({ s.worldX, s.worldY, s.worldZ });
                if (sx < -margin || sx > width() + margin || sy < -margin || sy > height() + margin)
                    continue;
                painter.drawEllipse(QPointF(sx, sy), qreal(radiusPixels), qreal(radiusPixels));
            }
            painter.restore();
        }

        // Sibling-highlight overlay: 2px golden ring at SPAWN_ICON_PIXEL_RADIUS+4
        // around every spawn whose guid is in m_siblingGuidSet.  Skips
        // spawns hidden by the phase mask so the operator can't see a
        // ring sitting on top of nothing.  Off-screen culling matches the
        // battlemaster overlay's margin pattern.
        if (needsSiblingHighlight)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(QColor(255, 220, 80, 220));
            pen.setWidthF(2.0);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            qreal const ringRadius = qreal(SPAWN_ICON_PIXEL_RADIUS) + 4.0;
            int const margin = int(ringRadius) + 8;
            for (Spawn const& s : m_spawns)
            {
                if (m_siblingGuidSet.find(s.guid) == m_siblingGuidSet.end())
                    continue;
                if (!spawnPassesPhaseFilter(s))
                    continue;
                auto const [sx, sy] = worldToScreen2D({ s.worldX, s.worldY, s.worldZ });
                if (sx < -margin || sx > width() + margin || sy < -margin || sy > height() + margin)
                    continue;
                painter.drawEllipse(QPointF(sx, sy), ringRadius, ringRadius);
            }
            painter.restore();
        }

        // Flight-path graph nodes: 4-pixel filled white disc with a
        // light-blue outer ring + a tiny name label offset above-right
        // of the node so labels don't overlap the disc.  Edge polylines
        // are drawn via GL above; this pass just renders the endpoints.
        if (needsFlightNodes)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QFont f = painter.font();
            f.setPointSizeF(7.5);
            painter.setFont(f);
            QPen ring(QColor(60, 180, 255, 240));
            ring.setWidthF(1.0);
            ring.setCosmetic(true);
            for (FlightNode const& n : m_flightNodes)
            {
                auto const [sx, sy] = worldToScreen2D({ n.x, n.y, n.z });
                if (sx < -32 || sx > width() + 32 || sy < -32 || sy > height() + 32)
                    continue;
                int const cx = int(sx), cy = int(sy);
                // Filled white disc + blue ring.
                painter.setBrush(QColor(255, 255, 255, 240));
                painter.setPen(ring);
                painter.drawEllipse(QPoint(cx, cy), 4, 4);
                // Name label, only when zoomed in enough to read it.
                // Skip when the label is empty (DB schema with no name
                // column / hotfix-only deployment).
                if (!n.name.isEmpty() && m_view.yardsPerPixel < 6.0f)
                {
                    int const lx = cx + 6;
                    int const ly = cy - 4;
                    // 1px black halo for legibility on bright nav-area fills.
                    painter.setPen(QColor(0, 0, 0, 200));
                    for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                        if (dx || dy)
                            painter.drawText(lx + dx, ly + dy, n.name);
                    painter.setPen(QColor(220, 235, 255, 255));
                    painter.drawText(lx, ly, n.name);
                }
            }
            painter.restore();
        }

        // Gathering-node heatmap icons.  Mining=brown ringed disc,
        // herb=green plus, fishing=light-blue tilde, treasure=gold star.
        // Drawn ~9px above the spawn icon so they don't collide with
        // quest glyphs / objective strip.  Phase-filter-aware (a hidden
        // spawn doesn't get an icon either).
        if (needsGatheringNodes)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            constexpr int kVerticalOffset = -9;  // Above spawn center.

            QColor const miningFill (130,  90,  40, 235);
            QColor const miningRing ( 70,  45,  20, 240);
            QColor const herbColor  ( 70, 170,  60, 235);
            QColor const fishColor  (100, 160, 255, 235);
            QColor const treasureFill(255, 210,  50, 235);
            QColor const treasureRing(120,  85,  10, 240);

            for (Spawn const& s : m_spawns)
            {
                if (s.kind != SpawnKind::GameObject)
                    continue;
                auto const it = m_gatheringNodes.find(s.guid);
                if (it == m_gatheringNodes.end())
                    continue;
                if (!spawnPassesPhaseFilter(s))
                    continue;
                auto const [sx, sy] = worldToScreen2D({ s.worldX, s.worldY, s.worldZ });
                if (sx < -32 || sx > width() + 32 || sy < -32 || sy > height() + 32)
                    continue;
                int const cx = int(sx);
                int const cy = int(sy) + kVerticalOffset;
                switch (it->second)
                {
                    case 0: // Mining: brown filled disc + darker ring.
                    {
                        QPen ring(miningRing);
                        ring.setWidthF(1.2);
                        painter.setPen(ring);
                        painter.setBrush(miningFill);
                        painter.drawEllipse(QPoint(cx, cy), 3, 3);
                        break;
                    }
                    case 1: // Herb: green plus sign (two crossed strokes).
                    {
                        QPen pen(herbColor);
                        pen.setWidthF(1.6);
                        pen.setCapStyle(Qt::FlatCap);
                        painter.setPen(pen);
                        painter.drawLine(cx - 3, cy, cx + 3, cy);
                        painter.drawLine(cx, cy - 3, cx, cy + 3);
                        break;
                    }
                    case 2: // Fishing: light-blue tilde (~) polyline.
                    {
                        QPen pen(fishColor);
                        pen.setWidthF(1.4);
                        pen.setCapStyle(Qt::RoundCap);
                        pen.setJoinStyle(Qt::RoundJoin);
                        painter.setPen(pen);
                        painter.setBrush(Qt::NoBrush);
                        QPolygon p;
                        p << QPoint(cx - 3, cy + 1)
                          << QPoint(cx - 1, cy - 2)
                          << QPoint(cx + 1, cy + 2)
                          << QPoint(cx + 3, cy - 1);
                        painter.drawPolyline(p);
                        break;
                    }
                    case 3: // Treasure: gold 5-point star.
                    {
                        QPen ring(treasureRing);
                        ring.setWidthF(0.8);
                        painter.setPen(ring);
                        painter.setBrush(treasureFill);
                        QPolygon p;
                        constexpr double kOuter = 3.5;
                        constexpr double kInner = 1.5;
                        for (int i = 0; i < 10; ++i)
                        {
                            double const ang = -M_PI / 2.0 + double(i) * (M_PI / 5.0);
                            double const r   = (i & 1) ? kInner : kOuter;
                            p << QPoint(cx + int(std::cos(ang) * r),
                                        cy + int(std::sin(ang) * r));
                        }
                        painter.drawPolygon(p);
                        break;
                    }
                    default: break;
                }
            }
            painter.restore();
        }

        // WMO 2D footprint pass: 1.5px dashed dark-purple rectangle with a
        // translucent fill per cached AABB.  Painted BEFORE the instance-
        // entrance ring so portal markers stay on top of building outlines
        // (entrances frequently sit inside their parent building's footprint).
        if (needsWmoFootprints)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen outline(QColor(90, 60, 110, 220));
            outline.setStyle(Qt::DashLine);
            outline.setWidthF(1.5);
            outline.setCosmetic(true);
            painter.setPen(outline);
            painter.setBrush(QColor(90, 60, 110, 25));
            // Visible-world AABB so we can cull off-screen footprints
            // cheaply on a continent's worth of WMOs.  Take min/max over
            // all 4 screen corners so the cull stays correct under view
            // rotation (the rotated visible region is not axis-aligned
            // in world space).
            coords::WorldPos const wTL = screenToWorld2D(0.0f, 0.0f);
            coords::WorldPos const wTR = screenToWorld2D(float(width()), 0.0f);
            coords::WorldPos const wBR = screenToWorld2D(float(width()), float(height()));
            coords::WorldPos const wBL = screenToWorld2D(0.0f, float(height()));
            float const visMinX = std::min({ wTL.x, wTR.x, wBR.x, wBL.x });
            float const visMaxX = std::max({ wTL.x, wTR.x, wBR.x, wBL.x });
            float const visMinY = std::min({ wTL.y, wTR.y, wBR.y, wBL.y });
            float const visMaxY = std::max({ wTL.y, wTR.y, wBR.y, wBL.y });
            for (auto const& fp : m_wmoFootprints)
            {
                float const minX = std::get<0>(fp);
                float const maxX = std::get<1>(fp);
                float const minY = std::get<2>(fp);
                float const maxY = std::get<3>(fp);
                if (maxX < visMinX || minX > visMaxX
                    || maxY < visMinY || minY > visMaxY)
                    continue;
                auto const [sx0, sy0] = worldToScreen2D({ minX, minY, 0.0f });
                auto const [sx1, sy1] = worldToScreen2D({ maxX, minY, 0.0f });
                auto const [sx2, sy2] = worldToScreen2D({ maxX, maxY, 0.0f });
                auto const [sx3, sy3] = worldToScreen2D({ minX, maxY, 0.0f });
                float const rx = std::min({ sx0, sx1, sx2, sx3 });
                float const ry = std::min({ sy0, sy1, sy2, sy3 });
                float const rw = std::max({ sx0, sx1, sx2, sx3 }) - rx;
                float const rh = std::max({ sy0, sy1, sy2, sy3 }) - ry;
                if (rw < 1.0f && rh < 1.0f)
                    continue;
                painter.drawRect(QRectF(rx, ry, rw, rh));
            }
            painter.restore();
        }

        // Instance-entrance overlay: 18px purple ring around each
        // areatrigger_teleport portal + italic target-map name below it.
        // Phase-filter / spawn data is irrelevant here -- the entries
        // come from MainWindow's DB join and are always world-visible.
        if (needsInstanceEntrances)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen ring(QColor(180, 80, 220, 235));
            ring.setWidthF(2.5);
            ring.setCosmetic(true);
            painter.setPen(ring);
            painter.setBrush(QColor(180, 80, 220, 40));
            QFont labelFont = painter.font();
            labelFont.setItalic(true);
            labelFont.setPointSizeF(8.0);
            constexpr int kRingRadius = 18;
            constexpr int kMargin = kRingRadius + 24;
            for (InstanceEntrance const& e : m_instanceEntrances)
            {
                auto const [sx, sy] = worldToScreen2D({ e.x, e.y, 0.0f });
                if (sx < -kMargin || sx > width() + kMargin || sy < -kMargin || sy > height() + kMargin)
                    continue;
                int const cx = int(sx), cy = int(sy);
                painter.drawEllipse(QPoint(cx, cy), kRingRadius, kRingRadius);
                if (!e.name.isEmpty())
                {
                    painter.setFont(labelFont);
                    // 1px black halo for legibility on bright nav-area fills.
                    int const lx = cx - 40;
                    int const ly = cy + kRingRadius + 12;
                    painter.setPen(QColor(0, 0, 0, 210));
                    for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                        if (dx || dy)
                            painter.drawText(QRect(lx + dx, ly + dy, 80, 16),
                                Qt::AlignHCenter | Qt::AlignTop, e.name);
                    painter.setPen(QColor(225, 195, 245, 245));
                    painter.drawText(QRect(lx, ly, 80, 16),
                        Qt::AlignHCenter | Qt::AlignTop, e.name);
                    // Restore the ring pen for the next iteration's ellipse.
                    painter.setPen(ring);
                }
            }
            painter.restore();
        }

        // linked_respawn dependency overlay: thin dotted dark-green segment
        // between every (from, to) spawn-icon pair where both spawns exist
        // in m_spawns and pass the phase filter.  Off-screen culls are done
        // per-endpoint with a small margin so segments crossing the viewport
        // edge still draw.  Lookup builds a transient guid->index map; link
        // count is small (a few hundred per map at most) so the overhead is
        // negligible vs the per-paint cost we already pay.
        if (needsSpawnLinks)
        {
            std::unordered_map<int64_t, int> guidToIndex;
            guidToIndex.reserve(m_spawns.size() * 2);
            for (int i = 0; i < int(m_spawns.size()); ++i)
                guidToIndex.emplace(m_spawns[i].guid, i);

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen link(QColor(60, 130, 60, 200));
            link.setWidthF(1.0);
            link.setCosmetic(true);
            link.setStyle(Qt::DashLine);
            painter.setPen(link);
            painter.setBrush(Qt::NoBrush);
            constexpr int kLinkMargin = 32;
            for (auto const& [fromGuid, toGuid] : m_spawnLinks)
            {
                auto itFrom = guidToIndex.find(fromGuid);
                auto itTo   = guidToIndex.find(toGuid);
                if (itFrom == guidToIndex.end() || itTo == guidToIndex.end())
                    continue;
                Spawn const& a = m_spawns[itFrom->second];
                Spawn const& b = m_spawns[itTo->second];
                if (!spawnPassesPhaseFilter(a) || !spawnPassesPhaseFilter(b))
                    continue;
                auto const [ax, ay] = worldToScreen2D({ a.worldX, a.worldY, 0.0f });
                auto const [bx, by] = worldToScreen2D({ b.worldX, b.worldY, 0.0f });
                bool const aOff = ax < -kLinkMargin || ax > width() + kLinkMargin
                               || ay < -kLinkMargin || ay > height() + kLinkMargin;
                bool const bOff = bx < -kLinkMargin || bx > width() + kLinkMargin
                               || by < -kLinkMargin || by > height() + kLinkMargin;
                if (aOff && bOff)
                    continue;
                painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
            }
            painter.restore();
        }

        // Road-graph connectivity diagnostic: a faithful preview of the routable
        // graph the worldserver builds from these segments (endpoints clustered
        // within ROAD_NODE_MERGE_EPSILON_YARDS == the server's RoadGraph.NodeMerge
        // Epsilon). Computed on data change in rebuildRoadConnectivityDiagnostic;
        // here we only draw the cached results.
        //   - white diamond  = junction (3+ segments meet)
        //   - red dashed link = near-miss GAP: loose ends that look connected but
        //                       won't merge -> bots can't route across (actionable)
        //   - amber ring      = dead end (degree-1 endpoint)
        if (needsCrossroadNodes)
        {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);

            // Junctions (degree >= 3): white diamond.
            painter.setPen(QPen(QColor(20, 20, 20, 230), 1.0));
            painter.setBrush(QColor(245, 245, 245, 230));
            for (QVector2D const& n : m_roadJunctionNodes)
            {
                auto const [sx, sy] = worldToScreen2D({ n.x(), n.y(), 0.0f });
                QPointF const pts[4] = {
                    QPointF(sx, sy - 5.0),
                    QPointF(sx + 5.0, sy),
                    QPointF(sx, sy + 5.0),
                    QPointF(sx - 5.0, sy),
                };
                painter.drawPolygon(pts, 4);
            }

            // Near-miss gaps: red dashed connector + ring on each loose end.
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(235, 40, 40, 235), 2.0, Qt::DashLine));
            for (auto const& pr : m_roadGapPairs)
            {
                auto const [ax, ay] = worldToScreen2D({ pr.first.x(),  pr.first.y(),  0.0f });
                auto const [bx, by] = worldToScreen2D({ pr.second.x(), pr.second.y(), 0.0f });
                painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
                painter.drawEllipse(QPointF(ax, ay), 6.0, 6.0);
                painter.drawEllipse(QPointF(bx, by), 6.0, 6.0);
            }

            // Dead ends (degree 1): amber ring. Fine if intentional; a cue to
            // check whether it was meant to tie into the rest of the network.
            painter.setPen(QPen(QColor(245, 170, 40, 230), 2.0));
            for (QVector2D const& n : m_roadDanglingNodes)
            {
                auto const [sx, sy] = worldToScreen2D({ n.x(), n.y(), 0.0f });
                painter.drawEllipse(QPointF(sx, sy), 5.0, 5.0);
            }

            painter.restore();
        }

        // Snap hints: while the placement FSM is in WaitingForStart /
        // WaitingForEnd, surface any persisted endpoint that's near the
        // cursor.  Outer ring (cyan) lights up everything within 2*radius;
        // a yellow highlight ring marks the actual snap target (the
        // candidate within SNAP_RADIUS_YARDS that the next click will
        // collapse to).
        if (needsSnapHints)
        {
            float const cursorX = m_segmentHoverWorld.x;
            float const cursorY = m_segmentHoverWorld.y;
            float const visRadius = 2.0f * SNAP_RADIUS_YARDS;
            float const visR2     = visRadius * visRadius;

            // Build candidate list the way mousePress does so the visual
            // and the snap logic stay in lockstep.
            std::vector<QVector2D> cands = m_pendingHandcraftedRoadVertices;
            if (m_segmentPlacementState == SegmentPlacementState::WaitingForEnd)
                cands.emplace_back(m_segmentStartWorld.x, m_segmentStartWorld.y);
            int const snapIdx = findSnapTarget(cursorX, cursorY, cands);

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            for (size_t i = 0; i < cands.size(); ++i)
            {
                float const dx = cands[i].x() - cursorX;
                float const dy = cands[i].y() - cursorY;
                if ((dx * dx + dy * dy) > visR2)
                    continue;
                auto const [sx, sy] = worldToScreen2D({ cands[i].x(), cands[i].y(), 0.0f });
                // Small filled cyan square: snap candidate in range.
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(60, 200, 230, 220));
                painter.drawRect(QRectF(sx - 3.0, sy - 3.0, 6.0, 6.0));
                if (int(i) == snapIdx)
                {
                    // Yellow halo: the click would land here.
                    painter.setPen(QPen(QColor(255, 220, 60, 240), 2.0));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(QPointF(sx, sy), 9.0, 9.0);
                }
            }
            painter.restore();
        }

        // Annotation hover/select highlight.  Selected gets a bright gold
        // ring + a small label tooltip; hovered (but not selected) gets a
        // dimmer cyan ring.  Both rings are drawn at the disc's world-
        // space radius so they hug the actual GL geometry irrespective of
        // current zoom.  When the same row is both hovered AND selected,
        // the selection visual wins (gold > cyan).
        if (needsAnnotationHighlight && m_view.yardsPerPixel > 0.0f)
        {
            auto drawRingAt = [&](int idx, QColor const& color, qreal penWidth, bool drawLabel)
            {
                if (idx < 0 || idx >= int(m_annotations.size()))
                    return;
                Annotation const& a = m_annotations[idx];
                auto const [sx, sy] = worldToScreen2D({ a.x, a.y, 0.0f });
                float const r       = std::max(a.radius, 1.0f);
                float const rPx     = r / m_view.yardsPerPixel;
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(QPen(color, penWidth));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QPointF(sx, sy), rPx, rPx);
                // Center dot so the operator can spot the exact placement
                // origin even when the radius circle stretches off-screen.
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawEllipse(QPointF(sx, sy), 3.0, 3.0);
                if (drawLabel)
                {
                    QString tip = QStringLiteral("id=%1 %2 r=%3y")
                        .arg(a.id)
                        .arg(QString::fromLatin1(annotationKindName(a.kind)))
                        .arg(double(a.radius), 0, 'f', 1);
                    if (!a.label.isEmpty())
                        tip = a.label + QStringLiteral("  -  ") + tip;
                    QFont f = painter.font();
                    f.setPointSize(std::max(f.pointSize(), 9));
                    f.setBold(true);
                    painter.setFont(f);
                    QFontMetricsF fm(f);
                    QRectF const textBounds = fm.boundingRect(tip).adjusted(-4, -2, 4, 2);
                    QPointF const anchor(sx + rPx + 6.0, sy - rPx - 4.0);
                    QRectF box = textBounds.translated(anchor);
                    // Clamp the tooltip into the widget rect so it stays
                    // visible when the disc is near the screen edge.
                    if (box.right() > width() - 4.0)
                        box.translate(width() - 4.0 - box.right(), 0.0);
                    if (box.left() < 4.0)
                        box.translate(4.0 - box.left(), 0.0);
                    if (box.top() < 4.0)
                        box.translate(0.0, 4.0 - box.top());
                    painter.setBrush(QColor(0, 0, 0, 200));
                    painter.setPen(QPen(color, 1.2));
                    painter.drawRoundedRect(box, 4.0, 4.0);
                    painter.setPen(Qt::white);
                    painter.drawText(box, Qt::AlignCenter, tip);
                }
                painter.restore();
            };

            // Hovered first (under the selection visual when they coincide).
            if (m_hoveredAnnotation >= 0 && m_hoveredAnnotation != m_selectedAnnotation)
                drawRingAt(m_hoveredAnnotation, QColor(60, 220, 230, 230), 1.8, false);
            if (m_selectedAnnotation >= 0)
                drawRingAt(m_selectedAnnotation, QColor(255, 200, 60, 240), 2.4, true);
        }

        painter.end();
    }
}

void NavMeshView::rebuildBuffers()
{
    m_pendingVertices.clear();
    if (!m_mesh.ok())
    {
        m_pendingDirty = true;
        if (m_buffersReady)
        {
            uploadGeometry();
            update();
        }
        return;
    }

    dtNavMesh const* nm = m_mesh.navmesh();
    // Reserve a guess: 3 verts per tri, ~6 tris per poly on average.
    m_pendingVertices.reserve(size_t(m_mesh.stats().polyCount) * 6 * 3);

    for (int ti = 0; ti < nm->getMaxTiles(); ++ti)
    {
        dtMeshTile const* tile = nm->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount <= 0)
            continue;

        for (int p = 0; p < tile->header->polyCount; ++p)
        {
            dtPoly const& poly = tile->polys[p];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;
            int const nv = poly.vertCount;
            if (nv < 3)
                continue;
            Rgb const color = colorForArea(poly.getArea());
            uint8_t const r = color.r, g = color.g, b = color.b, a = 255;

            // Project Detour verts to (TC X, TC Y).
            std::array<std::array<float, 2>, DT_VERTS_PER_POLYGON> tcVerts{};
            for (int i = 0; i < nv; ++i)
            {
                float const* v = &tile->verts[poly.verts[i] * 3];
                // dt[0] = TC Y, dt[2] = TC X.
                tcVerts[i] = { v[2], v[0] };
            }

            // Triangle fan.
            for (int i = 1; i + 1 < nv; ++i)
            {
                m_pendingVertices.push_back({ tcVerts[0][0],     tcVerts[0][1],     r, g, b, a });
                m_pendingVertices.push_back({ tcVerts[i][0],     tcVerts[i][1],     r, g, b, a });
                m_pendingVertices.push_back({ tcVerts[i + 1][0], tcVerts[i + 1][1], r, g, b, a });
            }
        }
    }

    m_pendingDirty = true;
    if (m_buffersReady)
    {
        uploadGeometry();
        update();
    }
}

void NavMeshView::uploadGeometry()
{
    m_buffersReady = true;
    m_pendingDirty = false;
    m_vertexCount = static_cast<GLsizei>(m_pendingVertices.size());

    m_vao.bind();
    m_vbo.bind();
    if (m_vertexCount > 0)
        m_vbo.allocate(m_pendingVertices.data(),
                       static_cast<int>(m_pendingVertices.size() * sizeof(Vertex)));
    else
        m_vbo.allocate(nullptr, 0);

    // attr0 = (world_x, world_y) as float; attr1 = (r, g, b, a) as unsigned byte normalized.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));

    m_vbo.release();
    m_vao.release();

    // Free the staging copy - the GPU has it now.
    m_pendingVertices.clear();
    m_pendingVertices.shrink_to_fit();
}

coords::WorldPos NavMeshView::screenToWorld(QPoint const& p) const
{
    // Public API: applies the continent-level view rotation by routing
    // through the rotation-aware screenToWorld2D helper, so callers that
    // hand us a raw screen pixel (mouse picks, click-to-place, etc.) get
    // a world point consistent with what the rotated GL view shows.
    return screenToWorld2D(p);
}

float NavMeshView::s_currentViewRotationDegrees = 0.0f;

void NavMeshView::setViewRotationDegrees(float deg)
{
    // Clamp to a sensible band - the only documented presets are 0 (legacy
    // layout, image-east = screen-right) and -90 (default, 90 CW rotated
    // so EK reads tall+narrow per the wow.export reference).  Other values
    // are accepted (they just won't have a matching toolbar slot).
    if (std::abs(deg - m_viewRotationDegrees) < 0.001f)
        return;
    m_viewRotationDegrees = deg;
    s_currentViewRotationDegrees = deg;
    QSettings persist;
    persist.setValue(QStringLiteral("viewer2d/view_rotation_degrees"), double(deg));
    qDebug().noquote()
        << QStringLiteral("[viewer2d] view_rotation_degrees = %1").arg(double(deg));
    update();
}

std::pair<float, float> NavMeshView::worldToScreen2D(coords::WorldPos const& w) const noexcept
{
    // Step 1: existing world->view-pixel transform (image-east = world -Y,
    // image-south = world -X).  See Coords.h::worldToScreen for the
    // ground truth.  Step 2: rotate the resulting pixel around the
    // viewport center by m_viewRotationDegrees, so the 2D viewer's
    // overall orientation matches the GL projection (which applies the
    // same rotation via u_viewRotationRad).
    auto const [sx0, sy0] = coords::worldToScreen(m_view, w);
    if (m_viewRotationDegrees == 0.0f)
        return { sx0, sy0 };
    float const rad = m_viewRotationDegrees * float(M_PI) / 180.0f;
    float const cr = std::cos(rad);
    float const sr = std::sin(rad);
    float const cx = float(width())  * 0.5f;
    float const cy = float(height()) * 0.5f;
    float const dx = sx0 - cx;
    float const dy = sy0 - cy;
    return { cx + dx * cr - dy * sr,
             cy + dx * sr + dy * cr };
}

coords::WorldPos NavMeshView::screenToWorld2D(float sx, float sy, float z) const noexcept
{
    if (m_viewRotationDegrees == 0.0f)
        return coords::screenToWorld(m_view, sx, sy, z);
    // Inverse-rotate the screen pixel around the viewport center, then
    // funnel through the legacy screen->world transform so the answer
    // matches what worldToScreen2D would round-trip.
    float const rad = -m_viewRotationDegrees * float(M_PI) / 180.0f;
    float const cr = std::cos(rad);
    float const sr = std::sin(rad);
    float const cx = float(width())  * 0.5f;
    float const cy = float(height()) * 0.5f;
    float const dx = sx - cx;
    float const dy = sy - cy;
    float const usx = cx + dx * cr - dy * sr;
    float const usy = cy + dx * sr + dy * cr;
    return coords::screenToWorld(m_view, usx, usy, z);
}

coords::WorldPos NavMeshView::screenToWorld2D(QPointF const& p) const noexcept
{
    return screenToWorld2D(float(p.x()), float(p.y()), 0.0f);
}

coords::WorldPos NavMeshView::screenToWorld2D(QPoint const& p) const noexcept
{
    return screenToWorld2D(float(p.x()), float(p.y()), 0.0f);
}

QPointF NavMeshView::unrotateScreenDelta(QPointF const& d) const noexcept
{
    if (m_viewRotationDegrees == 0.0f)
        return d;
    // Inverse rotation of a pure screen-space delta vector.  Used by
    // mouseMoveEvent's pan branch so dragging right on screen translates
    // the world along the rotated screen X axis (rather than the unrotated
    // world Y axis).
    float const rad = -m_viewRotationDegrees * float(M_PI) / 180.0f;
    float const cr = std::cos(rad);
    float const sr = std::sin(rad);
    return QPointF(d.x() * cr - d.y() * sr,
                   d.x() * sr + d.y() * cr);
}

int NavMeshView::hitTestSpawn(QPoint const& screen, float hitRadiusPixels) const
{
    if (m_spawns.empty())
        return -1;
    int   best = -1;
    float bestDist2 = hitRadiusPixels * hitRadiusPixels;
    for (size_t i = 0; i < m_spawns.size(); ++i)
    {
        Spawn const& s = m_spawns[i];
        // Skip filtered-out (invisible) spawns so the operator can't
        // click an icon that isn't drawn.
        if (!spawnPassesPhaseFilter(s))
            continue;
        coords::WorldPos const w{ s.worldX, s.worldY, s.worldZ };
        auto const [sx, sy] = worldToScreen2D(w);
        float const dx = sx - float(screen.x());
        float const dy = sy - float(screen.y());
        float const d2 = dx * dx + dy * dy;
        if (d2 <= bestDist2)
        {
            bestDist2 = d2;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void NavMeshView::rebuildSpawnBuffers()
{
    m_pendingSpawnVerts.clear();
    if (m_spawns.empty())
    {
        m_pendingSpawnDirty = true;
        if (m_buffersReady)
        {
            uploadSpawnGeometry();
            update();
        }
        return;
    }

    m_pendingSpawnVerts.reserve(m_spawns.size() * 6);
    for (Spawn const& s : m_spawns)
    {
        // Red for creatures, blue for game objects (Phase 1 palette;
        // we can recolour by template, ScriptName or phase later).
        uint8_t r, g, b;
        if (s.kind == SpawnKind::Creature) { r = 230; g =  60; b =  60; }
        else                                { r =  60; g = 140; b = 230; }
        constexpr uint8_t a = 230;

        for (auto const& corner : SPAWN_QUAD_CORNERS)
        {
            Vertex v;
            v.x = s.worldX;
            v.y = s.worldY;
            // Stuff the unit-quad corner into the .r/.g bytes pre-conversion?
            // No - we add a second VBO attr layout below.  Quick path: use
            // an interleaved vector but rely on offsetof for the unit-quad
            // corner stored as two more floats AFTER (r,g,b,a).  We don't
            // have that field in Vertex, so push raw bytes directly.
            v.r = r; v.g = g; v.b = b; v.a = a;
            // Tag the corner offset in the unused .a-padding via two
            // extra entries.  Cleanest: pack corner into separate
            // vertices array below.  We do it via a second push.
            m_pendingSpawnVerts.push_back(v);
            // Append corner offset as 2 floats stored at the end of the
            // Vertex struct: we cheat by reusing the (x, y) of a follow
            // entry.  Actually let's avoid that hack - dedicated struct.
            (void)corner;
        }
    }

    m_pendingSpawnDirty = true;
    if (m_buffersReady)
    {
        uploadSpawnGeometry();
        update();
    }
}

void NavMeshView::emitSelectionChanged()
{
    emit spawnSelectionChanged(m_selection);
}

void NavMeshView::setCascClient(io::CascClient* casc, io::MapDb2Lookup* mapDb2)
{
    m_cascClient = casc;
    m_mapDb2     = mapDb2;
    // Drop the per-tile attempt cache so existing tiles get re-resolved
    // through CASC on the next paint pass.
    destroyMinimapTextures();
    if (m_heightmapBuilt || m_heightmapBuilding)
    {
        m_heightmapPending = true;
        m_heightmapBuilt   = false;
        m_heightmapBuilding = false;
        m_heightmapBuildQueue.clear();
        m_heightmapBuildIndex = 0;
        destroyHeightmapTextures();
    }
    update();
}

void NavMeshView::setListfileLookup(io::ListfileLookup* listfile)
{
    if (m_listfile == listfile)
        return;
    m_listfile = listfile;
    // FDID cache is keyed by tile; previously-cached entries are still
    // valid (same map, same listfile semantics) but a new listfile may
    // resolve previously-missing tiles, so drop the negative-result entries
    // by clearing the whole minimap cache.  Same pattern as setCascClient.
    destroyMinimapTextures();
    if (m_heightmapBuilt || m_heightmapBuilding)
    {
        m_heightmapPending = true;
        m_heightmapBuilt   = false;
        m_heightmapBuilding = false;
        m_heightmapBuildQueue.clear();
        m_heightmapBuildIndex = 0;
        destroyHeightmapTextures();
    }
    update();
}

int NavMeshView::exportMinimapTilesFromCasc(ExportTileCallback const& sink)
{
    if (!m_cascClient || !m_mapDb2 || !m_cascClient->isOpen())
        return 0;
    auto dir = m_mapDb2->directoryFor(m_heightmapMapId);
    if (!dir)
        return 0;

    // Build the (gx, gy) set to walk.  Prefer the already-resolved
    // heightmap tile list (it's the authoritative set of "real" tiles);
    // fall back to the build queue if the heightmap hasn't finished
    // streaming yet.
    std::vector<std::pair<int, int>> tiles;
    if (!m_heightmapTiles.empty())
    {
        tiles.reserve(m_heightmapTiles.size());
        for (HeightmapTile const& ht : m_heightmapTiles)
            tiles.emplace_back(ht.gx, ht.gy);
    }
    else
    {
        tiles = m_heightmapBuildQueue;
    }

    int exported = 0;
    for (auto const& [gx, gy] : tiles)
    {
        std::string vpath = "world/minimaps/" + *dir;
        vpath += "/map";
        vpath += std::to_string(gx);
        vpath += "_";
        vpath += std::to_string(gy);
        vpath += ".blp";
        std::vector<uint8_t> blob;
        if (!m_cascClient->readByPath(vpath, blob))
            continue;
        io::BlpImage decoded;
        if (!io::decodeBlp(blob, decoded) || decoded.width <= 0 || decoded.height <= 0)
            continue;
        ExportedTile tile;
        tile.gx     = gx;
        tile.gy     = gy;
        tile.width  = decoded.width;
        tile.height = decoded.height;
        tile.rgba   = std::move(decoded.rgba);
        if (sink)
            sink(tile);
        ++exported;
    }
    return exported;
}

void NavMeshView::setMinimapDir(QString const& dir)
{
    if (m_minimapDir == dir)
        return;
    m_minimapDir = dir;
    // Drop the per-tile cache; the heightmap chunked-build loop will
    // re-attempt every tile on the next paint (we trigger that by
    // marking the heightmap pending so it rebuilds + re-probes).
    destroyMinimapTextures();
    if (m_heightmapBuilt || m_heightmapBuilding)
    {
        m_heightmapPending = true;
        m_heightmapBuilt   = false;
        m_heightmapBuilding = false;
        m_heightmapBuildQueue.clear();
        m_heightmapBuildIndex = 0;
        destroyHeightmapTextures();
    }
    update();
}

void NavMeshView::destroyMinimapTextures()
{
    if (!m_minimapTextures.empty() && context())
    {
        // GL textures must be destroyed with a current context.  When
        // called outside paintGL (e.g. setMinimapDir from MainWindow)
        // makeCurrent / doneCurrent bracket the deletes.
        bool const needCurrent = (QOpenGLContext::currentContext() != context());
        if (needCurrent) makeCurrent();
        for (auto& [k, tex] : m_minimapTextures)
        {
            if (tex != 0)
                glDeleteTextures(1, &tex);
        }
        if (needCurrent) doneCurrent();
    }
    m_minimapTextures.clear();
    // Reset diag log so a new map / new dir gets a fresh outcome line.
    m_minimapLoggedSuccess.clear();
    m_minimapLoggedFailure.clear();
    m_minimapLogLastMs         = 0;
    m_minimapTilesDrawnCount   = 0;
    m_minimapTilesSkippedCount = 0;
    // Allow the orientation-confirmation log line to fire once again on
    // the next map / minimap-dir reload.
    m_minimapOrientationLogged = false;
    // Diagnostic counters track per-map activity; clear them alongside the
    // logged-once sets so MinimapDiagnosticsDock shows a fresh slate on
    // map switch / minimap-dir reload.
    m_minimapSuccessfulLoads   = 0;
    m_minimapFailedLoads       = 0;
    m_minimapLastTried.clear();
    // Drop the per-map enumeration cache so the next map switch (or
    // operator-triggered reload) re-probes CASC for the new directory.
    m_minimapEnumeratedMapIds.clear();
    m_minimapDiscoveredNames.clear();
    // Listfile-resolved FDIDs are per-tile; on map switch every entry is
    // stale (different mapDir means different listfile paths).
    m_minimapFdidByTile.clear();
}

QStringList NavMeshView::probeMinimapCascNames(int max)
{
    // Operator-triggered probe of the live CASC catalog for whatever
    // minimap directory the current map resolves to.  Surfaces the
    // results both as a QStringList (for the dock popup) and via the
    // internal discovered-name cache so subsequent tile loads pick up
    // any new naming convention.
    QStringList out;
    if (!m_cascClient || !m_mapDb2 || !m_cascClient->isOpen())
        return out;
    auto dir = m_mapDb2->directoryFor(m_heightmapMapId);
    if (!dir)
        return out;

    std::string const base = "world/minimaps/" + *dir + "/";
    std::vector<std::string> listed = m_cascClient->listFiles(base, std::size_t(max <= 0 ? 50 : max));
    // Force the per-mapId one-shot to "already probed" so loadOrUpload
    // doesn't re-list on the next tile miss.  Also overwrite any prior
    // discovered set with the fresh one.
    m_minimapEnumeratedMapIds.insert(m_heightmapMapId);
    m_minimapDiscoveredNames[m_heightmapMapId] = listed;
    out.reserve(int(listed.size()));
    for (std::string const& name : listed)
        out << QString::fromStdString(name);
    return out;
}

GLuint NavMeshView::loadOrUploadMinimapTile(int gx, int gy)
{
    uint32_t const key = (uint32_t(gy) << 16) | (uint32_t(gx) & 0xFFFFu);
    if (auto it = m_minimapTextures.find(key); it != m_minimapTextures.end())
        return it->second; // already attempted (0 if previously missing)

    QImage img;
    bool found = false;
    // Diagnostic: per-mapId one-shot log lines explaining the outcome of
    // the first probed tile and the first failed tile.  Reason strings
    // are kept short so the operator can glance at qDebug output and
    // figure out whether the issue is missing PNG dir, missing CASC,
    // missing Map.db2 entry, or a decoder problem.
    QString failReason;

    // Canonical wow.export / WoW client minimap naming convention:
    //     world/minimaps/<dir>/map<paddedY>_<paddedX>.blp
    // where (X, Y) are wow.export's TileExporter info.x/info.y.  TC's
    // tile.gx is the north-south ROW index (== wow.export info.y) and
    // TC's tile.gy is the east-west COLUMN index (== wow.export info.x),
    // so the canonical TC-side path is map<pad2(gx)>_<pad2(gy)>.blp.
    //
    // Earlier revisions of this loader probed BOTH (gx, gy) and (gy, gx)
    // orderings to "be safe", but with a real client listfile both
    // orderings frequently resolve to DIFFERENT FDIDs that correspond to
    // two different real tiles in the world.  Whichever ordering wins
    // first becomes wrong for half the continent, producing a swapped /
    // garbled landmass shape (Eastern Kingdoms rendered short+wide
    // instead of tall+narrow).  Collapse to exactly one canonical path.
    auto const pad2 = [](int v) {
        std::string s = std::to_string(v);
        if (s.size() == 1)
            s.insert(s.begin(), '0');
        return s;
    };
    // 2026-05-26: operator A/B confirmed minimap landmass appears 90 CCW
    // from heightmap.  Both layers share the same world AABB, so the only
    // place that rotation can come from is the filename->cell mapping.
    // Swap gx/gy so each TC tile cell loads the BLP whose content actually
    // belongs at TC's (gx=N-S row, gy=E-W col) world position.
    std::string const canonicalPad = "map" + pad2(gy) + "_" + pad2(gx);

    // 1. PNG on disk -- operator's manual extracts win.  Single canonical
    //    path: <minimap_dir>/<mapId>/map<paddedGx>_<paddedGy>.png.
    QString canonicalPngPath;
    if (!m_minimapDir.isEmpty())
    {
        canonicalPngPath = QStringLiteral("%1/%2/%3.png")
            .arg(m_minimapDir)
            .arg(m_heightmapMapId)
            .arg(QString::fromStdString(canonicalPad));
        if (img.load(canonicalPngPath))
            found = true;
        else
            failReason = QStringLiteral("PNG miss (%1)").arg(canonicalPngPath);
    }
    else
    {
        failReason = QStringLiteral("no minimap_dir set");
    }

    // 2. CASC fallback -- read BLP from the live client install.
    if (!found)
    {
        if (!m_cascClient || !m_mapDb2)
            failReason = QStringLiteral("no CASC client wired");
        else if (!m_cascClient->isOpen())
            failReason = QStringLiteral("CASC not open");
        else if (auto dir = m_mapDb2->directoryFor(m_heightmapMapId); !dir)
            failReason = QStringLiteral("Map.db2 has no dir for mapId=%1").arg(m_heightmapMapId);
        else
        {
            // Canonical client virtual path.  See wow.export ADTExporter.js
            // and TerrainRenderer.js: minimaps are stored under
            //     world/minimaps/<dir>/map<paddedY>_<paddedX>.blp
            // where (X, Y) match wow.export's info.x/info.y.  In TC notation
            // that is map<pad2(gx)>_<pad2(gy)>.blp because TC's gx == client
            // info.y and TC's gy == client info.x.
            //
            // Probing alternative orderings here was the root cause of the
            // swapped-tile bug: the listfile may carry BOTH "mapA_B.blp"
            // and "mapB_A.blp" as DIFFERENT FDIDs corresponding to two real
            // tiles, and whichever the loader hit first won.  Stick to the
            // single canonical formula.
            std::string const base  = "world/minimaps/" + *dir + "/";
            std::string const vpath = base + canonicalPad + ".blp";

            std::vector<uint8_t> blob;
            std::string winning;
            if (m_cascClient->readByPath(vpath, blob))
                winning = vpath;

            // FDID fallback: modern (TWW build 67186+) WoW client data
            // stores many minimap BLPs as FileDataID-only entries — no
            // virtual path lives in the CASC root, so the readByPath call
            // above misses.  Resolve the canonical vpath through the
            // listfile and try openByFileDataId on the result.  Cache the
            // winning FDID so the same tile skips the listfile lookup on
            // subsequent paints; negative-cache misses so we don't keep
            // re-querying the listfile for a tile that doesn't exist.
            if (winning.empty() && m_listfile && !m_listfile->empty())
            {
                uint32_t fdid = 0;
                bool fromCache = false;
                if (auto it = m_minimapFdidByTile.find(key); it != m_minimapFdidByTile.end())
                {
                    fdid = it->second;
                    fromCache = true;
                }
                else
                {
                    if (auto resolved = m_listfile->resolveFdid(vpath))
                    {
                        fdid = *resolved;
                        m_minimapFdidByTile[key] = fdid;
                    }
                    else
                    {
                        m_minimapFdidByTile[key] = 0; // negative-cache
                    }
                }
                if (fdid != 0)
                {
                    if (m_cascClient->openByFileDataId(fdid, blob))
                    {
                        (void)fromCache;
                        winning = "[FDID:" + std::to_string(fdid) + "]";
                    }
                    else
                    {
                        // Negative-cache: don't keep retrying a FDID the
                        // archive doesn't actually contain.  The listfile
                        // is community-maintained and can lag the client.
                        m_minimapFdidByTile[key] = 0;
                    }
                }
            }

            if (winning.empty())
            {
                // First-miss-per-mapId: walk the CASC catalog ONCE so the
                // operator (and the next tile) can see the real names.
                if (m_minimapEnumeratedMapIds.insert(m_heightmapMapId).second)
                {
                    std::vector<std::string> listed = m_cascClient->listFiles(base, 20);
                    m_minimapDiscoveredNames[m_heightmapMapId] = listed;
                    qDebug().nospace()
                        << "[minimap] mapId=" << m_heightmapMapId
                        << " enumerating CASC prefix " << QString::fromStdString(base)
                        << " -> " << int(listed.size()) << " entries";
                    for (std::string const& name : listed)
                        qDebug().nospace() << "[minimap]   " << QString::fromStdString(name);
                }
                failReason = QStringLiteral("CASC miss (%1)")
                    .arg(QString::fromStdString(vpath));
            }
            else
            {
                io::BlpImage decoded;
                if (!io::decodeBlp(blob, decoded) || decoded.width <= 0 || decoded.height <= 0)
                {
                    failReason = QStringLiteral("BLP decode failed (%1 bytes)").arg(blob.size());
                }
                else
                {
                    img = QImage(decoded.rgba.data(), decoded.width, decoded.height,
                                 decoded.width * 4, QImage::Format_RGBA8888).copy();
                    found = true;
                    if (m_minimapLoggedSuccess.insert(m_heightmapMapId).second)
                    {
                        qDebug().nospace()
                            << "[minimap] mapId=" << m_heightmapMapId
                            << " tile " << gx << "," << gy
                            << ": loaded " << qulonglong(blob.size())
                            << " bytes / decoded " << decoded.width << "x" << decoded.height
                            << " (CASC " << QString::fromStdString(winning) << ")";
                    }
                }
            }
        }
    }

    if (!found || img.isNull())
    {
        if (m_minimapLoggedFailure.insert(m_heightmapMapId).second)
        {
            qDebug().nospace()
                << "[minimap] mapId=" << m_heightmapMapId
                << " tile " << gx << "," << gy << ": " << failReason;
        }
        m_minimapTextures[key] = 0;
        ++m_minimapFailedLoads;
        // Surface the single canonical vpath that the loader probed so the
        // operator can paste it into a CASC listfile lookup or a file
        // browser to verify the absence is real (rather than a path bug).
        m_minimapLastTried = QStringLiteral("%1,%2 -> fail vpath=world/minimaps/%3/%4.blp (%5)")
            .arg(gx).arg(gy)
            .arg(m_mapDb2 ? QString::fromStdString(m_mapDb2->directoryFor(m_heightmapMapId).value_or("?"))
                          : QStringLiteral("?"))
            .arg(QString::fromStdString(canonicalPad))
            .arg(failReason);
        return 0;
    }
    // PNG-on-disk success path: the CASC branch above logs inline on its
    // own first-success.  This catches the case where the operator has
    // run map_extractor outputs and the CASC branch never fires.
    if (!m_minimapLoggedSuccess.contains(m_heightmapMapId))
    {
        m_minimapLoggedSuccess.insert(m_heightmapMapId);
        qDebug().nospace()
            << "[minimap] mapId=" << m_heightmapMapId
            << " tile " << gx << "," << gy
            << ": loaded PNG / decoded " << img.width() << "x" << img.height();
    }
    // Capture the BLP decoder's original pixel format BEFORE any
    // conversion so the first-tile diagnostic below can tell the operator
    // exactly which format the decoder produced in their build.  An
    // earlier transpose pass used a manual `reinterpret_cast<QRgb const*>`
    // pixel loop, which silently reads garbage for any non-ARGB32/RGB32
    // QImage (e.g. Format_RGBA8888, Format_Indexed8, Format_RGB888) --
    // that produced the corrupted brown/blue stripe blobs reported on
    // the operator's build where the decoder emits RGBA8888.  Normalise
    // to Format_ARGB32 and use Qt's format-agnostic QTransform path so
    // the transpose is correct regardless of what the decoder emits.
    QImage::Format const originalFormat = img.format();
    if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_RGB32)
        img = img.convertToFormat(QImage::Format_ARGB32);

    // Transpose / rotation: BLP rows are stored north->south, BLP columns
    // are stored west->east.  The heightmap shader (reused unchanged for
    // the minimap layer) samples with U == world X, V == world Y.  The
    // operator's continent-level screenshot showed an additional 90 CW
    // rotation on top of whatever the per-tile transform was; the right
    // hypothesis is therefore Rotate90CCW (the inverse) on the per-tile
    // BLP, which is selectable from the MinimapDiagnosticsDock radio
    // panel.  Default at process start is still Transpose so an operator
    // who never opens the panel sees the previous behaviour.
    img = applyMinimapTransform(img, m_minimapTransform);

    // glTexImage2D below uploads GL_RGBA / GL_UNSIGNED_BYTE.  After the
    // ARGB32 normalisation + transpose, convert once more to RGBA8888
    // (matches the byte layout GL expects) so the upload sees the bytes
    // in R,G,B,A order rather than ARGB32's native B,G,R,A on little-
    // endian hosts.
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);

    // Compute the on-screen tile origin matching the heightmap formula in
    // rebuildHeightmapTiles() (line 1491).  Logged once so the operator
    // can sanity-check that minimap tiles land at the same world AABB the
    // heightmap layer uses.
    float const placedWorldMaxX = (coords::TILE_CENTER_INDEX - gx) * coords::TILE_SIZE;
    float const placedWorldMaxY = (coords::TILE_CENTER_INDEX - gy) * coords::TILE_SIZE;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // GL polish: explicit sized internal format (GL_RGBA8) for
    // driver-determinism, plus mipmap generation + LINEAR_MIPMAP_LINEAR
    // for the min filter so panned-out continent views don't shimmer.
    // Mirrors wow.export's GLTexture.set_rgba (GLTexture.js:43,181).
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
    ++m_minimapSuccessfulLoads;
    m_minimapLastTried = QStringLiteral("%1,%2 -> ok (%3x%4) vpath=world/minimaps/%5/%6.blp")
        .arg(gx).arg(gy).arg(img.width()).arg(img.height())
        .arg(m_mapDb2 ? QString::fromStdString(m_mapDb2->directoryFor(m_heightmapMapId).value_or("?"))
                      : QStringLiteral("?"))
        .arg(QString::fromStdString(canonicalPad));

    if (!m_minimapOrientationLogged)
    {
        m_minimapOrientationLogged = true;
        // Coverage summary -- repeats the navmesh/disk union sizes so the
        // operator can correlate a missing tile with whether the coverage
        // scan picked it up at all.
        auto const& diskTiles = m_mapFileCoverage[m_heightmapMapId];
        char const* const tName = minimapTransformName(m_minimapTransform);
        qDebug().nospace()
            << "[minimap] mapId=" << m_heightmapMapId
            << " dir=" << (m_mapDb2 ? QString::fromStdString(m_mapDb2->directoryFor(m_heightmapMapId).value_or("?")) : QStringLiteral("?"))
            << " coverage: dtNavMesh="
            << (m_mesh.ok() ? m_mesh.navmesh()->getMaxTiles() : 0)
            << " mapFiles=" << int(diskTiles.size())
            << " union=" << int(m_heightmapBuildQueue.size());
        // Spec step 4: one-shot per-map-switch sanity log confirming the
        // minimap and heightmap layers share the EXACT same gx/gy -> world
        // AABB.  The minimap quad world AABB is recomputed here from the
        // same formula rebuildHeightmapTiles() uses (lines 1602-1605) so a
        // future drift between the two layers' placement is immediately
        // visible in the operator's log.  If the two AABBs differ, that's
        // a separate alignment bug -- fix by making both layers use this
        // exact formula.
        float const heightWorldMaxX = (coords::TILE_CENTER_INDEX - gx) * coords::TILE_SIZE;
        float const heightWorldMinX = heightWorldMaxX - coords::TILE_SIZE;
        float const heightWorldMaxY = (coords::TILE_CENTER_INDEX - gy) * coords::TILE_SIZE;
        float const heightWorldMinY = heightWorldMaxY - coords::TILE_SIZE;
        qDebug().nospace()
            << "[minimap] view rotation = " << m_viewRotationDegrees
            << " (" << (m_viewRotationDegrees == 0.0f ? "off" : "on") << ")";
        qDebug().nospace()
            << "[minimap] per-tile transform = " << tName;
        qDebug().nospace()
            << "[minimap] tile (gx=" << gx << ", gy=" << gy
            << ") quad world AABB = ["
            << (placedWorldMaxX - coords::TILE_SIZE) << ".."
            << placedWorldMaxX << "] x ["
            << (placedWorldMaxY - coords::TILE_SIZE) << ".."
            << placedWorldMaxY << "]";
        qDebug().nospace()
            << "[minimap] heightmap tile (gx=" << gx << ", gy=" << gy
            << ") quad world AABB = ["
            << heightWorldMinX << ".."
            << heightWorldMaxX << "] x ["
            << heightWorldMinY << ".."
            << heightWorldMaxY << "] (should match minimap above)";
        qDebug().nospace()
            << "[minimap] tile gx=" << gx << " gy=" << gy
            << " fdid=" << (m_minimapFdidByTile.count(key) ? m_minimapFdidByTile[key] : 0u)
            << " originalFmt=" << static_cast<int>(originalFormat)
            << " transform=" << tName;
        qDebug().nospace()
            << "minimap_tile gx=" << gx << " gy=" << gy
            << " fdid=" << (m_minimapFdidByTile.count(key) ? m_minimapFdidByTile[key] : 0u)
            << " transform=" << tName
            << " imgWxH=" << img.width() << "x" << img.height()
            << " placedAt=("
            << (placedWorldMaxX - coords::TILE_SIZE) << ".."
            << placedWorldMaxX << ","
            << (placedWorldMaxY - coords::TILE_SIZE) << ".."
            << placedWorldMaxY << ")";
        // Spec step 4 diagnostic: sample the texture's center pixel + the
        // four corners so the operator can correlate which corner of the
        // BLP ends up at which world corner.  The image is RGBA8888 at
        // this point (post-transform, pre-upload), so pixel(x,y) returns a
        // QRgb whose channels are R,G,B,A in that order.
        auto colorAt = [&](int x, int y) {
            x = std::clamp(x, 0, img.width() - 1);
            y = std::clamp(y, 0, img.height() - 1);
            QRgb const c = img.pixel(x, y);
            return QStringLiteral("#%1%2%3")
                .arg(qRed(c),   2, 16, QLatin1Char('0'))
                .arg(qGreen(c), 2, 16, QLatin1Char('0'))
                .arg(qBlue(c),  2, 16, QLatin1Char('0'));
        };
        int const w = img.width(), h = img.height();
        qDebug().nospace()
            << "[minimap] sample colors (post-transform): center=" << colorAt(w/2, h/2)
            << " TL=" << colorAt(0, 0)
            << " TR=" << colorAt(w-1, 0)
            << " BL=" << colorAt(0, h-1)
            << " BR=" << colorAt(w-1, h-1);
    }
    return tex;
}

void NavMeshView::setQuestMarkers(std::vector<QuestMarker> markers)
{
    m_questMarkers = std::move(markers);
    rebuildQuestLineBuffer();
    update();
}

void NavMeshView::setQuestObjectiveMarkers(std::vector<QuestObjectiveMarker> markers)
{
    m_questObjectiveMarkers = std::move(markers);
    update();
}

void NavMeshView::setQuestObjectivesVisible(bool on)
{
    if (m_questObjectivesVisible == on)
        return;
    m_questObjectivesVisible = on;
    // Clear any hover-tooltip stuck from a previous overlay state.
    if (!on)
        setToolTip(QString());
    update();
}

int NavMeshView::hitTestQuestObjective(QPoint const& screen, float hitRadiusPixels) const
{
    if (!isLayerVisible(Layer::Quests) || !m_questObjectivesVisible || m_questObjectiveMarkers.empty())
        return -1;
    float bestDistSq = hitRadiusPixels * hitRadiusPixels;
    int   bestIdx   = -1;
    for (size_t i = 0; i < m_questObjectiveMarkers.size(); ++i)
    {
        QuestObjectiveMarker const& m = m_questObjectiveMarkers[i];
        auto const [sx, sy] = worldToScreen2D({ m.x, m.y, m.z });
        // Icons render at a 4px southward offset from the spawn center.
        float const dx = sx - float(screen.x());
        float const dy = (sy + 4.0f) - float(screen.y());
        float const d2 = dx * dx + dy * dy;
        if (d2 < bestDistSq)
        {
            bestDistSq = d2;
            bestIdx    = int(i);
        }
    }
    return bestIdx;
}

void NavMeshView::rebuildQuestLineBuffer()
{
    // Build GL_LINES connecting every starter to every ender of the
    // same quest.  N*N over the marker list is fine -- typical zone
    // map carries < 500 quest-involved spawns, and the join lives in a
    // hashmap to keep it sub-quadratic.
    struct PathVertex { float x, y; uint8_t r, g, b, a; };
    std::vector<PathVertex> verts;

    // Group spawn positions by quest id.
    struct QuestEnds { std::vector<std::pair<float,float>> starts, ends; };
    std::unordered_map<uint32_t, QuestEnds> byQuest;
    for (QuestMarker const& m : m_questMarkers)
    {
        for (uint32_t q : m.startsQuests)
            byQuest[q].starts.push_back({ m.x, m.y });
        for (uint32_t q : m.endsQuests)
            byQuest[q].ends.push_back({ m.x, m.y });
    }

    // Color: teal so the lines don't fight the orange road graph.
    constexpr uint8_t kR = 100, kG = 200, kB = 220, kA = 190;
    for (auto const& [qid, qe] : byQuest)
    {
        for (auto const& s : qe.starts)
        {
            for (auto const& e : qe.ends)
            {
                if (std::abs(s.first - e.first) < 0.001f
                    && std::abs(s.second - e.second) < 0.001f)
                    continue; // same NPC starts + ends; no line needed
                verts.push_back({ s.first, s.second, kR, kG, kB, kA });
                verts.push_back({ e.first, e.second, kR, kG, kB, kA });
            }
        }
        (void)qid;
    }

    m_questLineVertexCount = static_cast<GLsizei>(verts.size());
    if (!m_buffersReady || !m_questLineVbo.isCreated())
        return;
    m_questLineVao.bind();
    m_questLineVbo.bind();
    if (!verts.empty())
        m_questLineVbo.allocate(verts.data(), int(verts.size() * sizeof(PathVertex)));
    else
        m_questLineVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_questLineVbo.release();
    m_questLineVao.release();
}

void NavMeshView::setSpawnGroupColors(std::unordered_map<int64_t, uint32_t> colors)
{
    m_spawnGroupColors = std::move(colors);
    rebuildSpawnBuffers();
    update();
}

void NavMeshView::setFactionTintMap(std::unordered_map<uint32_t, uint8_t> map)
{
    m_factionTintMap = std::move(map);
    rebuildSpawnBuffers();
    update();
}

void NavMeshView::setLevelMap(std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>> map)
{
    m_levelMap = std::move(map);
    rebuildSpawnBuffers();
    update();
}

bool NavMeshView::spawnPassesPhaseFilter(Spawn const& s) const
{
    if (!m_spawnPhaseFilter.enabled)
        return true;
    // Always-visible convention: rows with both phaseId and phaseGroup
    // == 0 are not phase-restricted; they show regardless of filter.
    if (s.phaseId == 0 && s.phaseGroup == 0)
        return true;
    // Match-on-EITHER axis: phaseId OR phaseGroup.
    if (m_spawnPhaseFilter.phaseId != 0 && s.phaseId == m_spawnPhaseFilter.phaseId)
        return true;
    if (m_spawnPhaseFilter.phaseGroup != 0 && s.phaseGroup == m_spawnPhaseFilter.phaseGroup)
        return true;
    return false;
}

size_t NavMeshView::visibleSpawnCount() const
{
    if (!m_spawnPhaseFilter.enabled)
        return m_spawns.size();
    size_t n = 0;
    for (Spawn const& s : m_spawns)
        if (spawnPassesPhaseFilter(s))
            ++n;
    return n;
}

void NavMeshView::setSpawnPhaseFilter(SpawnPhaseFilter f)
{
    m_spawnPhaseFilter = f;
    // Filter gates SpawnDensity contribution; force a rebuild.
    m_spawnDensityDirty = true;
    rebuildSpawnBuffers();
    update();
}

void NavMeshView::uploadSpawnGeometry()
{
    // Build a SoA: per-vertex = (worldX, worldY, offsetX, offsetY,
    // r, g, b, a).  20 bytes per vertex.
    struct SpawnVertex
    {
        float x, y;
        float ox, oy;
        uint8_t r, g, b, a;
    };
    static_assert(sizeof(SpawnVertex) == 20, "SpawnVertex packed size drift");

    bool const groupTint = isLayerVisible(Layer::SpawnGroups)
                        && !m_spawnGroupColors.empty();
    bool const factionTint = isLayerVisible(Layer::FactionTint)
                          && !m_factionTintMap.empty();
    bool const levelHeat = isLayerVisible(Layer::LevelHeatmap)
                        && !m_levelMap.empty();

    // Faction-group palette (matches setFactionTintMap doc above).
    // Order: 0=Alliance 1=Horde 2=Sanctuary 3=Contested 4=Other.
    static constexpr uint8_t kFactionRGB[5][3] = {
        {  70, 130, 255 }, // Alliance  - blue
        { 220,  60,  60 }, // Horde     - red
        { 230, 210,  70 }, // Sanctuary - yellow
        { 180,  90, 220 }, // Contested - purple
        { 160, 160, 160 }, // Other     - gray
    };

    // Level-bracket palette - 12-stop ramp keyed by midpoint level.  L1..80
    // is 8 even bands (yellow -> green -> light-blue -> blue -> purple ->
    // pink -> orange -> red); L81+ collapses to dark-red; boss/level-0
    // (TC's -1 sentinel) hits bright magenta.  The lambda below picks a
    // stop and bilinearly interpolates against the next stop so adjacent
    // levels read as a gradient, not a hard banding.
    struct LvlStop { int level; uint8_t r, g, b; };
    static constexpr LvlStop kLevelStops[] = {
        {  1, 255, 250, 170 }, // pale yellow
        { 10, 255, 240, 130 },
        { 11, 120, 220, 120 }, // green
        { 20,  80, 200,  90 },
        { 21, 140, 220, 240 }, // light blue
        { 30,  90, 180, 230 },
        { 31,  70, 110, 230 }, // blue
        { 40,  60,  90, 210 },
        { 41, 160,  90, 220 }, // purple
        { 50, 140,  60, 200 },
        { 51, 240, 130, 200 }, // pink
        { 60, 230, 110, 180 },
        { 61, 250, 160,  60 }, // orange
        { 70, 235, 130,  40 },
        { 71, 230,  60,  60 }, // red
        { 80, 210,  40,  40 },
        { 81, 140,  20,  20 }, // dark red (L81+)
    };
    auto pickLevelColor = [&](int midLevel) -> std::array<uint8_t, 3> {
        // Boss / world-boss sentinel - bright magenta.
        if (midLevel <= 0)
            return { 255, 80, 230 };
        if (midLevel >= 81)
            return { kLevelStops[16].r, kLevelStops[16].g, kLevelStops[16].b };
        // Find bracketing stops [lo, hi] and interpolate.
        LvlStop const* lo = &kLevelStops[0];
        LvlStop const* hi = &kLevelStops[0];
        for (size_t k = 0; k < sizeof(kLevelStops) / sizeof(kLevelStops[0]); ++k)
        {
            if (kLevelStops[k].level <= midLevel)
                lo = &kLevelStops[k];
            if (kLevelStops[k].level >= midLevel) { hi = &kLevelStops[k]; break; }
        }
        if (lo == hi)
            return { lo->r, lo->g, lo->b };
        float const t = float(midLevel - lo->level) / float(hi->level - lo->level);
        return {
            uint8_t(float(lo->r) * (1.0f - t) + float(hi->r) * t),
            uint8_t(float(lo->g) * (1.0f - t) + float(hi->g) * t),
            uint8_t(float(lo->b) * (1.0f - t) + float(hi->b) * t),
        };
    };

    std::vector<SpawnVertex> verts;
    verts.reserve(m_spawns.size() * 6);
    for (size_t i = 0; i < m_spawns.size(); ++i)
    {
        Spawn const& s = m_spawns[i];
        // Phase-mask filter: drop spawns whose phase doesn't match BEFORE
        // any color/tint override (group-tint still applies to visible).
        if (!spawnPassesPhaseFilter(s))
            continue;
        uint8_t r, g, b;
        if (s.kind == SpawnKind::Creature) { r = 230; g =  60; b =  60; }
        else                                { r =  60; g = 140; b = 230; }
        uint8_t a = 230;
        // FactionTint layer: blend kind-default toward the faction-group
        // color by 60%.  Runs BEFORE the SpawnGroups override so groups
        // still win when both layers are on.  Creature-only -- GOs have
        // no faction in creature_template (game-object faction lives on
        // gameobject_template, not handled here).
        if (factionTint && s.kind == SpawnKind::Creature)
        {
            auto const it = m_factionTintMap.find(s.entry);
            if (it != m_factionTintMap.end())
            {
                uint8_t const idx = it->second < 5 ? it->second : 4;
                constexpr float kBlend = 0.60f;
                r = uint8_t(float(r) * (1.0f - kBlend) + float(kFactionRGB[idx][0]) * kBlend);
                g = uint8_t(float(g) * (1.0f - kBlend) + float(kFactionRGB[idx][1]) * kBlend);
                b = uint8_t(float(b) * (1.0f - kBlend) + float(kFactionRGB[idx][2]) * kBlend);
            }
        }
        // LevelHeatmap layer: blend kind-default toward the level-band
        // color by 70%.  Creature-only - GO templates have no level.
        // TC stores world-boss "level -1" as int8 in creature_template,
        // surfaced here as min==0 || max==0 from the loader; that path
        // pins the icon to bright magenta so world bosses pop visually.
        if (levelHeat && s.kind == SpawnKind::Creature)
        {
            auto const it = m_levelMap.find(s.entry);
            if (it != m_levelMap.end())
            {
                uint16_t const minL = it->second.first;
                uint16_t const maxL = it->second.second;
                int mid;
                if (minL == 0 || maxL == 0)
                    mid = 0; // boss / level-0 sentinel -> magenta
                else
                    mid = int((uint32_t(minL) + uint32_t(maxL)) / 2);
                auto const lc = pickLevelColor(mid);
                constexpr float kBlend = 0.70f;
                r = uint8_t(float(r) * (1.0f - kBlend) + float(lc[0]) * kBlend);
                g = uint8_t(float(g) * (1.0f - kBlend) + float(lc[1]) * kBlend);
                b = uint8_t(float(b) * (1.0f - kBlend) + float(lc[2]) * kBlend);
            }
        }
        // SpawnGroups layer override: if this guid is in a group, paint
        // the icon in the group's deterministic color so groups read at
        // a glance.  We keep alpha at 230 so it doesn't blow out.
        if (groupTint)
        {
            auto const it = m_spawnGroupColors.find(s.guid);
            if (it != m_spawnGroupColors.end())
            {
                uint32_t const c = it->second;
                r = uint8_t(c & 0xFF);
                g = uint8_t((c >> 8)  & 0xFF);
                b = uint8_t((c >> 16) & 0xFF);
            }
        }

        // Ghost during drag: yellow tint + reduced alpha at the drop
        // position so the operator sees where the icon will land.
        float drawX = s.worldX;
        float drawY = s.worldY;
        if (int(i) == m_draggingSpawn && m_dragDidMove)
        {
            drawX = m_dragCurrentWorld.x;
            drawY = m_dragCurrentWorld.y;
            r = 255; g = 220; b = 80; a = 180;
        }

        // Selected: brighten to cyan-tinted full alpha so the operator
        // sees which icons are part of a bulk-edit set.
        if (m_selection.contains(int(i)))
        {
            r = uint8_t(std::min(255, int(r) + 40));
            g = uint8_t(std::min(255, int(g) + 80));
            b = uint8_t(std::min(255, int(b) + 80));
            a = 255;
        }

        for (auto const& corner : SPAWN_QUAD_CORNERS)
            verts.push_back({ drawX, drawY, corner.x, corner.y, r, g, b, a });
    }

    m_pendingSpawnDirty = false;
    m_spawnVertexCount  = static_cast<GLsizei>(verts.size());

    m_spawnVao.bind();
    m_spawnVbo.bind();
    if (!verts.empty())
        m_spawnVbo.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(SpawnVertex)));
    else
        m_spawnVbo.allocate(nullptr, 0);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
                          reinterpret_cast<void*>(offsetof(SpawnVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpawnVertex),
                          reinterpret_cast<void*>(offsetof(SpawnVertex, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpawnVertex),
                          reinterpret_cast<void*>(offsetof(SpawnVertex, r)));

    m_spawnVbo.release();
    m_spawnVao.release();

    // Drop the temporary Vertex-staging from rebuildSpawnBuffers (it
    // wasn't authoritative; the SoA build above is).
    m_pendingSpawnVerts.clear();
    m_pendingSpawnVerts.shrink_to_fit();
}

int NavMeshView::hitTestPath(QPoint const& screen, float hitRadiusPixels) const
{
    int after = -1;
    float px = 0.0f, py = 0.0f;
    return hitTestPathSegment(screen, after, px, py, hitRadiusPixels);
}

int NavMeshView::hitTestPathSegment(QPoint const& screen, int& outAfterNode,
                                    float& outProjX, float& outProjY,
                                    float hitRadiusPixels) const
{
    outAfterNode = -1;
    outProjX = outProjY = 0.0f;
    if (m_paths.empty()) return -1;
    int   best     = -1;
    int   bestSeg  = -1;
    float bestDist = hitRadiusPixels;
    float bestProjX = 0.0f, bestProjY = 0.0f;
    QPointF const cursor(screen.x(), screen.y());
    for (size_t pi = 0; pi < m_paths.size(); ++pi)
    {
        Path const& p = m_paths[pi];
        if (p.nodes.size() < 2) continue;
        for (size_t i = 0; i + 1 < p.nodes.size(); ++i)
        {
            auto const [ax, ay] = worldToScreen2D(
                { p.nodes[i].x, p.nodes[i].y, p.nodes[i].z });
            auto const [bx, by] = worldToScreen2D(
                { p.nodes[i + 1].x, p.nodes[i + 1].y, p.nodes[i + 1].z });
            QPointF const a(ax, ay), b(bx, by);
            QPointF const ab = b - a;
            float const len2 = float(ab.x() * ab.x() + ab.y() * ab.y());
            if (len2 < 0.001f) continue;
            float t = float((cursor.x() - a.x()) * ab.x() + (cursor.y() - a.y()) * ab.y()) / len2;
            t = std::clamp(t, 0.0f, 1.0f);
            QPointF const proj = a + ab * qreal(t);
            float const dx = float(proj.x() - cursor.x());
            float const dy = float(proj.y() - cursor.y());
            float const d  = std::sqrt(dx * dx + dy * dy);
            if (d < bestDist)
            {
                bestDist = d;
                best     = static_cast<int>(pi);
                bestSeg  = static_cast<int>(i);
                // Convert the projected screen point back to world XY
                // so callers can offer "Insert node here" at the exact
                // cursor projection.
                coords::WorldPos const wp = screenToWorld2D(
                    float(proj.x()), float(proj.y()));
                bestProjX = wp.x;
                bestProjY = wp.y;
            }
        }
    }
    if (best >= 0)
    {
        outAfterNode = bestSeg;
        outProjX     = bestProjX;
        outProjY     = bestProjY;
    }
    return best;
}

NavMeshView::PathNodeHit NavMeshView::hitTestPathNode(QPoint const& screen,
                                                     float hitRadiusPixels) const
{
    PathNodeHit hit;
    if (m_paths.empty()) return hit;
    QPointF const cursor(screen.x(), screen.y());
    float bestDist = hitRadiusPixels;
    for (size_t pi = 0; pi < m_paths.size(); ++pi)
    {
        Path const& p = m_paths[pi];
        for (size_t ni = 0; ni < p.nodes.size(); ++ni)
        {
            auto const [sx, sy] = worldToScreen2D(
                { p.nodes[ni].x, p.nodes[ni].y, p.nodes[ni].z });
            float const dx = float(sx) - float(cursor.x());
            float const dy = float(sy) - float(cursor.y());
            float const d  = std::sqrt(dx * dx + dy * dy);
            if (d < bestDist)
            {
                bestDist       = d;
                hit.pathIndex  = static_cast<int>(pi);
                hit.nodeIndex  = static_cast<int>(ni);
            }
        }
    }
    return hit;
}

int NavMeshView::hitTestAnnotation(QPoint const& screen) const
{
    if (m_annotations.empty())
        return -1;
    coords::WorldPos const w = screenToWorld(screen);
    // Pick the smallest-radius annotation containing the cursor so
    // nested annotations (e.g. POI inside city footprint) are still
    // hoverable.
    int   best = -1;
    float bestR = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < m_annotations.size(); ++i)
    {
        Annotation const& a = m_annotations[i];
        float const dx = a.x - w.x;
        float const dy = a.y - w.y;
        float const d2 = dx * dx + dy * dy;
        float const r  = std::max(a.radius, 1.0f);
        if (d2 <= r * r && r < bestR)
        {
            bestR = r;
            best  = static_cast<int>(i);
        }
    }
    return best;
}

void NavMeshView::rebuildAnnotationBuffers()
{
    m_pendingAnnotDirty = true;
    // Mirror the handcrafted-road fix: gate the immediate upload on the
    // annotation VBO/VAO existing (created in initializeGL), NOT on
    // m_buffersReady (which only flips after the first successful
    // setNavMesh).  Annotations can legitimately arrive before any
    // navmesh is loaded — e.g. when MainWindow re-pushes after an INSERT
    // before the operator opens a map.  paintGL has a second safety-net
    // flush so even if this branch is skipped the next frame catches up.
    if (m_annotVbo.isCreated() && m_annotVao.isCreated())
    {
        uploadAnnotationGeometry();
        update();
    }
}

void NavMeshView::uploadAnnotationGeometry()
{
    // Vertex: (centerX, centerY, offsetX, offsetY, radius, r, g, b, a)
    // -> 9 floats = 36 bytes, but we keep colour as float to avoid
    // attribute interleave complications.
    struct AnnotVertex
    {
        float cx, cy;
        float ox, oy;
        float radius;
        float r, g, b, a;
    };
    static_assert(sizeof(AnnotVertex) == 36, "AnnotVertex packed size drift");

    std::vector<AnnotVertex> verts;
    verts.reserve(m_annotations.size() * 6);
    for (Annotation const& ann : m_annotations)
    {
        Rgb01 const c = colorForKind(ann.kind);
        constexpr float A = 1.0f;
        for (auto const& corner : SPAWN_QUAD_CORNERS)
        {
            verts.push_back({
                ann.x, ann.y,
                corner.x, corner.y,
                std::max(ann.radius, 1.0f),
                c.r, c.g, c.b, A });
        }
    }

    m_pendingAnnotDirty = false;
    m_annotVertexCount  = static_cast<GLsizei>(verts.size());
    qDebug() << "[annotation] viewer GL upload:" << m_annotations.size()
             << "discs (" << m_annotVertexCount << "verts) selected="
             << m_selectedAnnotation << "hovered=" << m_hoveredAnnotation;

    m_annotVao.bind();
    m_annotVbo.bind();
    if (!verts.empty())
        m_annotVbo.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(AnnotVertex)));
    else
        m_annotVbo.allocate(nullptr, 0);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(AnnotVertex),
                          reinterpret_cast<void*>(offsetof(AnnotVertex, cx)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(AnnotVertex),
                          reinterpret_cast<void*>(offsetof(AnnotVertex, ox)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(AnnotVertex),
                          reinterpret_cast<void*>(offsetof(AnnotVertex, radius)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(AnnotVertex),
                          reinterpret_cast<void*>(offsetof(AnnotVertex, r)));

    m_annotVbo.release();
    m_annotVao.release();
}

void NavMeshView::setPlacementMode(bool on)
{
    m_placementMode = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void NavMeshView::setPathDebugMode(bool on)
{
    if (m_pathDebugMode == on)
        return;
    m_pathDebugMode = on;
    // Always reset FSM when toggling so re-entering is predictable.
    clearPathDebug();
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void NavMeshView::clearPathDebug()
{
    m_pathDebugState   = 0;
    m_pathDebugStart   = coords::WorldPos{};
    m_pathDebugEnd     = coords::WorldPos{};
    m_pathDebugReached = false;
    m_pathDebugRoute.clear();
    rebuildPathDebugBuffer();
    update();
}

float NavMeshView::probeGroundZ(float worldX, float worldY) const
{
    // Use the heightmap layer's MapTileCache when the operator has
    // already wired it.  Falls back to 0 -- findRoute's vertical extent
    // (50y on the up axis) absorbs most reasonable terrain heights.
    if (m_mapCache && m_heightmapMapId != 0 && !m_mapCache->mapsDir().empty())
    {
        float const h = m_mapCache->heightAt(m_heightmapMapId, worldX, worldY);
        if (h > io::ADT_INVALID_HEIGHT)
            return h;
    }
    return 0.0f;
}

void NavMeshView::rebuildPathDebugBuffer()
{
    // Build a GL_LINES segment list from m_pathDebugRoute.  Color per
    // segment by length: green<10y, yellow<30y, red>=30y.  The long-red
    // case typically indicates an off-mesh link (teleport / jump-down)
    // that the operator needs to know about.
    struct PathVertex { float x, y; uint8_t r, g, b, a; };
    std::vector<PathVertex> verts;

    if (!m_pathDebugRoute.empty())
    {
        // Compose the full polyline starting at m_pathDebugStart so the
        // first segment (start -> route[0]) is also rendered.
        std::vector<coords::WorldPos> full;
        full.reserve(m_pathDebugRoute.size() + 1);
        full.push_back(m_pathDebugStart);
        for (auto const& p : m_pathDebugRoute)
            full.push_back(p);

        verts.reserve((full.size() - 1) * 2);
        for (size_t i = 0; i + 1 < full.size(); ++i)
        {
            float const dx = full[i + 1].x - full[i].x;
            float const dy = full[i + 1].y - full[i].y;
            float const seg = std::sqrt(dx * dx + dy * dy);
            uint8_t r, g, b;
            if      (seg < 10.0f) { r =  80; g = 220; b =  80; } // short -> green
            else if (seg < 30.0f) { r = 240; g = 220; b =  60; } // medium -> yellow
            else                  { r = 240; g =  70; b =  70; } // long -> red (off-mesh)
            constexpr uint8_t a = 230;
            verts.push_back({ full[i].x,     full[i].y,     r, g, b, a });
            verts.push_back({ full[i + 1].x, full[i + 1].y, r, g, b, a });
        }
    }

    m_pathDebugVertexCount = static_cast<GLsizei>(verts.size());
    if (!m_buffersReady || !m_pathDebugVbo.isCreated())
        return;
    m_pathDebugVao.bind();
    m_pathDebugVbo.bind();
    if (!verts.empty())
        m_pathDebugVbo.allocate(verts.data(), int(verts.size() * sizeof(PathVertex)));
    else
        m_pathDebugVbo.allocate(nullptr, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PathVertex),
                          reinterpret_cast<void*>(offsetof(PathVertex, r)));
    m_pathDebugVbo.release();
    m_pathDebugVao.release();
}

void NavMeshView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Handcrafted-road CHAIN placement mode: 2-click FSM, but the
        // second click does NOT exit the mode -- it transitions back to
        // WaitingForEnd with the previous endpoint as the new start so
        // the operator can keep dropping segments without re-arming
        // "Add chain..." between each pair.  Esc / right-click exits.
        // Each click is snap-checked against the existing handcrafted
        // road endpoints PLUS the in-flight chain start so the operator
        // can rope into an existing road or close a loop cleanly.
        if (m_segmentPlacementState != SegmentPlacementState::None)
        {
            coords::WorldPos w = screenToWorld(event->pos());

            // Snap check: existing handcrafted road endpoints + the
            // current chain start (when in WaitingForEnd).  The chain
            // start is appended so the operator can land exactly back on
            // the previous endpoint, closing a tight loop.
            std::vector<QVector2D> snapCands = m_pendingHandcraftedRoadVertices;
            if (m_segmentPlacementState == SegmentPlacementState::WaitingForEnd)
                snapCands.emplace_back(m_segmentStartWorld.x, m_segmentStartWorld.y);
            int const snapIdx = findSnapTarget(w.x, w.y, snapCands);
            if (snapIdx >= 0)
            {
                QVector2D const& s = snapCands[size_t(snapIdx)];
                qDebug() << "[handcrafted-road] snapped"
                         << QString("(%1, %2)").arg(w.x, 0, 'f', 2).arg(w.y, 0, 'f', 2)
                         << "->"
                         << QString("(%1, %2)").arg(s.x(), 0, 'f', 2).arg(s.y(), 0, 'f', 2);
                w.x = s.x();
                w.y = s.y();
            }

            if (m_segmentPlacementState == SegmentPlacementState::WaitingForStart)
            {
                m_segmentStartWorld = w;
                m_segmentHoverWorld = w;
                m_segmentPlacementState = SegmentPlacementState::WaitingForEnd;
                emit handcraftedSegmentPlacementStateChanged(int(m_segmentPlacementState));
                update();
            }
            else if (m_segmentPlacementState == SegmentPlacementState::WaitingForEnd)
            {
                float const fromX = m_segmentStartWorld.x;
                float const fromY = m_segmentStartWorld.y;
                float const toX   = w.x;
                float const toY   = w.y;
                // Stay in chain mode: roll the previous endpoint into the
                // new start, bump the counter, and keep the FSM in
                // WaitingForEnd.  The signal listeners (dock + status bar)
                // refresh from the persisted DB after the insert lands.
                m_segmentStartWorld = w;
                m_segmentHoverWorld = w;
                m_chainSegmentCount += 1;
                emit handcraftedSegmentPlaced(fromX, fromY, toX, toY);
                emit handcraftedChainSegmentCountChanged(m_chainSegmentCount);
                update();
            }
            QOpenGLWidget::mousePressEvent(event);
            return;
        }

        // Path-debug mode: 2-click FSM (start, end+compute, reset).
        // Takes priority over all other left-click semantics; MMB pan
        // still works.
        if (m_pathDebugMode)
        {
            coords::WorldPos const w = screenToWorld(event->pos());
            float const z = probeGroundZ(w.x, w.y);
            if (m_pathDebugState == 0)
            {
                m_pathDebugStart   = coords::WorldPos{ w.x, w.y, z };
                m_pathDebugState   = 1;
                m_pathDebugRoute.clear();
                m_pathDebugReached = false;
                rebuildPathDebugBuffer();
                update();
            }
            else if (m_pathDebugState == 1)
            {
                m_pathDebugEnd = coords::WorldPos{ w.x, w.y, z };
                m_pathDebugRoute = findRoute(m_pathDebugStart.x, m_pathDebugStart.y, m_pathDebugStart.z,
                                             m_pathDebugEnd.x,   m_pathDebugEnd.y,   m_pathDebugEnd.z,
                                             /*maxStraightPathPoints*/ 1024);
                // Total distance + reached-end metric reported via signal.
                float total = 0.0f;
                coords::WorldPos prev = m_pathDebugStart;
                for (auto const& p : m_pathDebugRoute)
                {
                    float const dx = p.x - prev.x, dy = p.y - prev.y;
                    total += std::sqrt(dx * dx + dy * dy);
                    prev = p;
                }
                if (!m_pathDebugRoute.empty())
                {
                    coords::WorldPos const& last = m_pathDebugRoute.back();
                    float const dx = last.x - m_pathDebugEnd.x;
                    float const dy = last.y - m_pathDebugEnd.y;
                    m_pathDebugReached = (dx * dx + dy * dy) <= (5.0f * 5.0f);
                }
                else
                {
                    m_pathDebugReached = false;
                }
                m_pathDebugState = 2;
                rebuildPathDebugBuffer();
                update();
                emit pathDebugComputed(int(m_pathDebugRoute.size()), total, m_pathDebugReached);
            }
            else
            {
                // Third click: reset and start a new pair.
                clearPathDebug();
            }
            QOpenGLWidget::mousePressEvent(event);
            return;
        }

        // Placement mode: every left-click drops an annotation, with
        // pan/spawn/annotation hit-tests disabled.  Middle-mouse still
        // pans even in placement mode so the operator can reposition.
        if (m_placementMode)
        {
            coords::WorldPos const w = screenToWorld(event->pos());
            emit placementRequested(w.x, w.y);
            QOpenGLWidget::mousePressEvent(event);
            return;
        }

        // Road connectivity-diagnostic pick (highest priority while the Roads
        // layer is on): clicking a red near-miss gap ring or an amber dead-end
        // ring selects the segment(s) at that node in the Handcrafted Roads dock
        // so the operator can Edit / Delete / reconnect them.  Skipped while
        // shift-selecting so box-select still works over the road overlay.
        if (!(event->modifiers() & Qt::ShiftModifier) && isLayerVisible(Layer::Roads))
        {
            float ndx = 0.0f, ndy = 0.0f;
            if (hitTestRoadDiagnostic(event->pos(), ndx, ndy))
            {
                emit roadDiagnosticClicked(ndx, ndy);
                QOpenGLWidget::mousePressEvent(event);
                return;
            }
        }

        bool const shiftHeld = (event->modifiers() & Qt::ShiftModifier);

        // Pick order: spawn icon (small) > path node (small) > path
        // segment (thin) > annotation disc (large) > pan/click.
        int const spawnIdx = hitTestSpawn(event->pos());
        PathNodeHit const nodeHit = (spawnIdx < 0)
                                    ? hitTestPathNode(event->pos())
                                    : PathNodeHit{};
        int const pathIdx  = (spawnIdx < 0 && nodeHit.pathIndex < 0)
                             ? hitTestPath(event->pos()) : -1;
        int const annIdx   = (spawnIdx < 0 && nodeHit.pathIndex < 0 && pathIdx < 0)
                             ? hitTestAnnotation(event->pos()) : -1;
        if (nodeHit.pathIndex >= 0 && !shiftHeld)
        {
            // Begin a drag-or-click on the node.  mouseRelease decides
            // between click + drag based on cumulative movement.
            m_draggingPathIdx     = nodeHit.pathIndex;
            m_draggingNodeIdx     = nodeHit.nodeIndex;
            m_pathDragStartScreen = event->pos();
            PathNode const& n = m_paths[nodeHit.pathIndex].nodes[nodeHit.nodeIndex];
            m_pathDragCurrentWorld = coords::WorldPos{ n.x, n.y, n.z };
            m_pathDragDidMove = false;
            setCursor(Qt::ClosedHandCursor);
            QOpenGLWidget::mousePressEvent(event);
            return;
        }
        if (pathIdx >= 0 && !shiftHeld)
        {
            emit pathClicked(pathIdx);
            QOpenGLWidget::mousePressEvent(event);
            return;
        }
        if (!shiftHeld)
        {
            int const atrIdx = hitTestAreatrigger(event->pos());
            if (atrIdx >= 0)
            {
                emit areatriggerClicked(atrIdx);
                QOpenGLWidget::mousePressEvent(event);
                return;
            }
            int const gyIdx = hitTestGraveyard(event->pos());
            if (gyIdx >= 0)
            {
                emit graveyardClicked(gyIdx);
                QOpenGLWidget::mousePressEvent(event);
                return;
            }
        }
        if (spawnIdx >= 0)
        {
            if (shiftHeld)
            {
                // Toggle membership; no drag-start.
                int const at = m_selection.indexOf(spawnIdx);
                if (at < 0) m_selection.append(spawnIdx);
                else        m_selection.removeAt(at);
                emitSelectionChanged();
                rebuildSpawnBuffers();
                update();
            }
            else
            {
                // Start a drag-or-click. We don't decide between click
                // and drag yet; mouseRelease does that based on
                // cumulative movement.
                m_draggingSpawn   = spawnIdx;
                m_dragStartScreen = event->pos();
                m_dragCurrentWorld = coords::WorldPos{
                    m_spawns[spawnIdx].worldX,
                    m_spawns[spawnIdx].worldY,
                    m_spawns[spawnIdx].worldZ };
                m_dragDidMove = false;
                setCursor(Qt::ClosedHandCursor);
            }
        }
        else if (annIdx >= 0)
        {
            emit annotationClicked(annIdx);
        }
        else if (shiftHeld)
        {
            // Shift + drag on empty space starts a box-select.
            m_boxSelecting     = true;
            m_boxStartScreen   = event->pos();
            m_boxCurrentScreen = event->pos();
            setCursor(Qt::CrossCursor);
        }
        else
        {
            // Clicking empty space with no modifier clears selection
            // AND starts a pan.
            if (!m_selection.isEmpty())
            {
                m_selection.clear();
                emitSelectionChanged();
                rebuildSpawnBuffers();
            }
            m_panning = true;
            m_panAnchor = event->pos();
            m_panAnchorWorld = m_view.anchorWorld;
            setCursor(Qt::ClosedHandCursor);

            coords::WorldPos const w = screenToWorld(event->pos());
            emit clicked(w.x, w.y);
        }
    }
    else if (event->button() == Qt::MiddleButton)
    {
        // MMB-pan works regardless of placement mode.
        m_panning = true;
        m_panAnchor = event->pos();
        m_panAnchorWorld = m_view.anchorWorld;
        setCursor(Qt::ClosedHandCursor);
    }
    QOpenGLWidget::mousePressEvent(event);
}

void NavMeshView::mouseMoveEvent(QMouseEvent* event)
{
    coords::WorldPos const w = screenToWorld(event->pos());
    emit hoverChanged(w.x, w.y);

    // Track the hover world position so the in-progress segment
    // preview line (paintGL) renders from m_segmentStartWorld to the
    // current cursor while the operator is choosing the second click.
    // Also refreshed during WaitingForStart so the snap-candidate hints
    // can follow the cursor before the operator has clicked anything.
    if (m_segmentPlacementState == SegmentPlacementState::WaitingForEnd
        || m_segmentPlacementState == SegmentPlacementState::WaitingForStart)
    {
        m_segmentHoverWorld = w;
        update();
    }

    // Box-select in progress: live-update the rect.
    if (m_boxSelecting)
    {
        m_boxCurrentScreen = event->pos();
        update();
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    // Drag in progress: ghost the spawn at the cursor.
    if (m_draggingSpawn >= 0)
    {
        QPoint const delta = event->pos() - m_dragStartScreen;
        if (std::abs(delta.x()) > DRAG_PIXEL_THRESHOLD ||
            std::abs(delta.y()) > DRAG_PIXEL_THRESHOLD)
        {
            m_dragDidMove = true;
        }
        if (m_dragDidMove)
        {
            m_dragCurrentWorld = w;
            // Z is left as-is until MainWindow optionally snaps-to-ground
            // on release.
            m_dragCurrentWorld.z = m_spawns[m_draggingSpawn].worldZ;
            // Refresh the spawn buffers so the ghost renders.
            rebuildSpawnBuffers();
        }
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    // Drag in progress: ghost the path node at the cursor.
    if (m_draggingPathIdx >= 0)
    {
        QPoint const delta = event->pos() - m_pathDragStartScreen;
        if (std::abs(delta.x()) > DRAG_PIXEL_THRESHOLD ||
            std::abs(delta.y()) > DRAG_PIXEL_THRESHOLD)
        {
            m_pathDragDidMove = true;
        }
        if (m_pathDragDidMove)
        {
            m_pathDragCurrentWorld = w;
            // Z stays the responsibility of MainWindow's snap-to-ground.
            if (m_draggingPathIdx < int(m_paths.size())
                && m_draggingNodeIdx < int(m_paths[m_draggingPathIdx].nodes.size()))
            {
                m_pathDragCurrentWorld.z =
                    m_paths[m_draggingPathIdx].nodes[m_draggingNodeIdx].z;
            }
            uploadPathNodeGeometry();
            update();
        }
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    int const spawnIdx = m_panning ? -1 : hitTestSpawn(event->pos());
    if (spawnIdx != m_hoveredSpawn)
    {
        m_hoveredSpawn = spawnIdx;
        emit spawnHovered(spawnIdx);
    }

    int const annIdx = (m_panning || spawnIdx >= 0) ? -1 : hitTestAnnotation(event->pos());
    if (annIdx != m_hoveredAnnotation)
    {
        m_hoveredAnnotation = annIdx;
        emit annotationHovered(annIdx);
    }

    // Quest-objective tooltip: shown when the cursor is on an objective
    // icon AND nothing higher-priority (spawn/annotation) is being
    // hovered.  We use QWidget::setToolTip so Qt drives the timing.
    if (!m_panning && spawnIdx < 0 && annIdx < 0
        && isLayerVisible(Layer::Quests) && m_questObjectivesVisible)
    {
        int const oi = hitTestQuestObjective(event->pos());
        if (oi >= 0 && oi < int(m_questObjectiveMarkers.size()))
        {
            QuestObjectiveMarker const& mo = m_questObjectiveMarkers[oi];
            QStringList parts;
            if (mo.kinds & 0x01) parts << QStringLiteral("kill");
            if (mo.kinds & 0x02) parts << QStringLiteral("gather");
            if (mo.kinds & 0x04) parts << QStringLiteral("interact");
            if (mo.kinds & 0x08) parts << QStringLiteral("talk");
            if (mo.kinds & 0x10) parts << QStringLiteral("explore");
            QString const kinds = parts.isEmpty()
                ? QStringLiteral("(none)") : parts.join('/');
            setToolTip(QStringLiteral("obj: %1 for quests %2")
                .arg(kinds, mo.quests.isEmpty() ? QStringLiteral("(?)") : mo.quests));
        }
        else
        {
            setToolTip(QString());
        }
    }
    else if (!m_panning)
    {
        // Outside objective hit-test scope: drop any stale objective tooltip.
        setToolTip(QString());
    }

    // Cursor reflects whichever pick state wins (placement mode wins all).
    if (!m_panning)
    {
        if (m_placementMode)
            setCursor(Qt::CrossCursor);
        else
            setCursor((spawnIdx >= 0 || annIdx >= 0) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    if (m_panning)
    {
        QPoint const delta = event->pos() - m_panAnchor;
        // Drag right -> camera moves right -> world moves left -> anchor world Y decreases.
        // Inverse-rotate the screen-space delta so the world translates
        // along the rotated screen axes rather than the unrotated world
        // axes (otherwise a 90 CW rotated view would pan vertically when
        // the operator drags horizontally).
        QPointF const ud = unrotateScreenDelta(QPointF(delta));
        m_view.anchorWorld.y = m_panAnchorWorld.y + float(ud.x()) * m_view.yardsPerPixel;
        m_view.anchorWorld.x = m_panAnchorWorld.x + float(ud.y()) * m_view.yardsPerPixel;
        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void NavMeshView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_boxSelecting)
    {
        QRect const rect = QRect(m_boxStartScreen, m_boxCurrentScreen).normalized();
        m_boxSelecting = false;

        // Add every spawn whose icon center lies inside the rect.
        bool const ctrlHeld = (event->modifiers() & Qt::ControlModifier);
        if (!ctrlHeld)
            m_selection.clear();
        for (size_t i = 0; i < m_spawns.size(); ++i)
        {
            Spawn const& s = m_spawns[i];
            auto const [sx, sy] = worldToScreen2D({ s.worldX, s.worldY, s.worldZ });
            if (rect.contains(int(sx), int(sy)))
            {
                if (!m_selection.contains(int(i)))
                    m_selection.append(int(i));
            }
        }
        emitSelectionChanged();
        rebuildSpawnBuffers();
        update();
        setCursor(Qt::ArrowCursor);
        QOpenGLWidget::mouseReleaseEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton && m_draggingSpawn >= 0)
    {
        int const idx = m_draggingSpawn;
        bool const moved = m_dragDidMove;
        coords::WorldPos const dropWorld = m_dragCurrentWorld;
        m_draggingSpawn = -1;
        m_dragDidMove   = false;
        setCursor(m_placementMode ? Qt::CrossCursor : Qt::ArrowCursor);
        if (moved)
        {
            // Reset the local ghost back to the model's stored position;
            // the caller will write the new (X, Y) via setSpawns once
            // SpawnModel propagates the change.
            rebuildSpawnBuffers();
            emit spawnMoved(idx, dropWorld.x, dropWorld.y);
        }
        else
        {
            // Treat as a click: spawn was tapped but not dragged.
            // Single-click collapses selection to just this spawn.
            m_selection.clear();
            m_selection.append(idx);
            emitSelectionChanged();
            rebuildSpawnBuffers();
            update();
            emit spawnClicked(idx);
        }
    }
    if (event->button() == Qt::LeftButton && m_draggingPathIdx >= 0)
    {
        int const pathIdx = m_draggingPathIdx;
        int const nodeIdx = m_draggingNodeIdx;
        bool const moved  = m_pathDragDidMove;
        coords::WorldPos const dropWorld = m_pathDragCurrentWorld;
        m_draggingPathIdx = -1;
        m_draggingNodeIdx = -1;
        m_pathDragDidMove = false;
        setCursor(m_placementMode ? Qt::CrossCursor : Qt::ArrowCursor);
        if (moved)
        {
            uploadPathNodeGeometry();
            emit pathNodeMoved(pathIdx, nodeIdx, dropWorld.x, dropWorld.y);
        }
        else
        {
            emit pathNodeClicked(pathIdx, nodeIdx);
        }
        update();
    }
    if ((event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) && m_panning)
    {
        m_panning = false;
        setCursor(m_placementMode ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void NavMeshView::contextMenuEvent(QContextMenuEvent* event)
{
    // Right-click while a handcrafted-road chain is in flight ends the
    // chain cleanly (mirrors Esc).  Done BEFORE the placementMode early-
    // return so the chain still exits even when generic placement mode
    // is also active.
    if (m_segmentPlacementState != SegmentPlacementState::None)
    {
        cancelSegmentPlacement();
        event->accept();
        return;
    }
    if (m_placementMode)
    {
        QOpenGLWidget::contextMenuEvent(event);
        return;
    }
    QPoint const local = event->pos();
    QPoint const global = event->globalPos();

    // Spawn icons render on top of paths, so they take right-click priority.
    int const spawnIdx = hitTestSpawn(local);
    if (spawnIdx >= 0)
    {
        emit spawnContextMenuRequested(spawnIdx, global);
        event->accept();
        return;
    }

    PathNodeHit const nodeHit = hitTestPathNode(local);
    if (nodeHit.pathIndex >= 0)
    {
        emit pathNodeContextMenuRequested(nodeHit.pathIndex, nodeHit.nodeIndex, global);
        event->accept();
        return;
    }

    int   afterNode = -1;
    float projX = 0.0f, projY = 0.0f;
    int const pathIdx = hitTestPathSegment(local, afterNode, projX, projY);
    if (pathIdx >= 0)
    {
        // Stash the projected world point in the drag-current vector so
        // the slot can pull it on accept.  We pass it through the signal
        // directly to avoid extra plumbing.
        emit pathSegmentContextMenuRequested(pathIdx, afterNode, projX, projY, global);
        event->accept();
        return;
    }

    QOpenGLWidget::contextMenuEvent(event);
}

void NavMeshView::wheelEvent(QWheelEvent* event)
{
    // Zoom anchored at the cursor: the world point under the cursor
    // must stay under the cursor after zoom.  Route through the
    // rotation-aware screenToWorld2D so the picked-world point matches
    // what the rotated GL view shows under the cursor.
    QPointF const cursorPos = event->position();
    coords::WorldPos const cursorWorldBefore = screenToWorld2D(cursorPos);

    float const steps = float(event->angleDelta().y()) / 120.0f;
    float const factor = std::pow(0.9f, steps);
    m_view.yardsPerPixel = std::clamp(m_view.yardsPerPixel * factor, 0.1f, 256.0f);

    // Re-anchor so cursorWorldBefore lands back under the cursor.  The
    // view rotation is applied around the viewport center AFTER the
    // legacy coords::worldToScreen pipeline; so we inverse-rotate the
    // cursor pixel to get the pre-rotation screen point, then solve the
    // legacy anchor equation against that.
    float ucx = float(cursorPos.x());
    float ucy = float(cursorPos.y());
    if (m_viewRotationDegrees != 0.0f)
    {
        float const rad = -m_viewRotationDegrees * float(M_PI) / 180.0f;
        float const cr = std::cos(rad);
        float const sr = std::sin(rad);
        float const cx = float(width())  * 0.5f;
        float const cy = float(height()) * 0.5f;
        float const dx = ucx - cx;
        float const dy = ucy - cy;
        ucx = cx + dx * cr - dy * sr;
        ucy = cy + dx * sr + dy * cr;
    }
    m_view.anchorWorld.y = cursorWorldBefore.y +
        (ucx - m_view.anchorPixelX) * m_view.yardsPerPixel;
    m_view.anchorWorld.x = cursorWorldBefore.x +
        (ucy - m_view.anchorPixelY) * m_view.yardsPerPixel;

    update();
    event->accept();
}

void NavMeshView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F)
    {
        frameMesh();
        event->accept();
        return;
    }

    // Esc cancels handcrafted-road segment placement.  Only swallowed when
    // a placement is actually in flight so other Esc handlers (e.g. close
    // dialogs) keep working in their normal contexts.
    if (event->key() == Qt::Key_Escape
        && m_segmentPlacementState != SegmentPlacementState::None)
    {
        cancelSegmentPlacement();
        event->accept();
        return;
    }

    // F1..F7 minimap A/B shortcuts were removed: they conflicted with
    // existing shortcuts (F1 = global Find, F3 = 3D view) and they only
    // flushed the texture cache without forcing the chunked-build queue
    // to re-seed, so tiles never came back.  The MinimapDiagnosticsDock
    // now ships a visible "Minimap transform (A/B test)" panel that
    // drives setMinimapTransform(), which both flushes the cache AND
    // forces re-streaming.
    QOpenGLWidget::keyPressEvent(event);
}

// Default Rotate90CW.  Operator A/B pass 2 on 2026-05-26 confirmed
// Rotate180 oriented each tile correctly but the continent ended up
// rotated 90 CW relative to the wow.export reference; pass 1's
// view-level -90 fix broke the heightmap / spawn / path / annotation
// layers because those were already correct.  Folding the missing
// 90 CCW into the per-tile transform (Rotate180 + Rotate90CCW ==
// Rotate90CW) restores the minimap continent orientation without
// touching any other layer.  This static mirror is what the 3D scene
// viewer reads to keep its minimap layer aligned to the 2D viewer's
// choice without an explicit setter wiring.
// 2026-05-26: operator A/B final answer.  After fixing the filename gy/gx
// swap in loadOrUploadMinimapTile (which corrected the 90 CCW landmass
// rotation), per-tile interior alignment requires Transpose + 180 so each
// BLP's coastlines/roads continue across cell seams cleanly.
NavMeshView::MinimapTransform NavMeshView::s_currentMinimapTransform =
    NavMeshView::MinimapTransform::Transpose_Rot180;

void NavMeshView::restartMinimapStreaming(char const* reason)
{
    // Flush GL textures + per-tile attempt cache.  This is what the old
    // F-key path did, but on its own it was insufficient: heightmap
    // state (m_heightmapBuilt / m_heightmapBuilding) stayed set so the
    // chunked-build seed branch in paintGL never reactivated and no new
    // streaming pass ran.  Force the streaming state machine back into
    // its "pending, no tiles, will re-seed on next paint" pose so the
    // panel re-seeds visibly.
    destroyMinimapTextures();
    if (m_heightmapBuilt || m_heightmapBuilding || !m_heightmapTiles.empty())
    {
        m_heightmapPending  = true;
        m_heightmapBuilt    = false;
        m_heightmapBuilding = false;
        m_heightmapBuildQueue.clear();
        m_heightmapBuildIndex = 0;
        destroyHeightmapTextures();
    }
    else if (m_heightmapMapId != 0)
    {
        // Map was loaded but the previous build never completed: still
        // request a fresh seed so the dock action is never a silent no-op.
        m_heightmapPending = true;
    }
    m_minimapOrientationLogged = false;
    // Streaming-progress counter is implicit (m_heightmapTiles.size() vs
    // queue size); the log line is emitted by paintGL when each batch
    // crosses a 50-tile multiple.  Reset the throttle here.
    m_streamProgressLoggedAt = 0;
    qDebug().noquote()
        << QStringLiteral("[minimap] %1 texture cache flushed, re-seeding").arg(QString::fromUtf8(reason));
    if (QWidget* w = window())
    {
        if (auto* mw = qobject_cast<QMainWindow*>(w))
        {
            if (QStatusBar* sb = mw->statusBar())
                sb->showMessage(QStringLiteral("[minimap] %1 — re-seeding").arg(QString::fromUtf8(reason)), 5000);
        }
    }
    update();
}

void NavMeshView::setMinimapTransform(MinimapTransform t)
{
    if (t == m_minimapTransform)
    {
        // Same value re-selected via the radio panel: still flush + re-seed
        // because the operator's intent is "show me this transform again
        // from scratch".  Cheap and matches the dock's UX.
    }
    m_minimapTransform = t;
    s_currentMinimapTransform = t;
    char const* const name = minimapTransformName(t);
    QString const reason = QStringLiteral("transform = %1").arg(QString::fromUtf8(name));
    restartMinimapStreaming(reason.toUtf8().constData());
}

char const* NavMeshView::minimapTransformName(MinimapTransform t) noexcept
{
    switch (t)
    {
        case MinimapTransform::Identity:          return "Identity (none)";
        case MinimapTransform::Transpose:         return "Transpose (swap rows/cols)";
        case MinimapTransform::Rotate90CW:        return "Rotate 90 CW";
        case MinimapTransform::Rotate90CCW:       return "Rotate 90 CCW";
        case MinimapTransform::Rotate180:         return "Rotate 180";
        case MinimapTransform::MirrorH:           return "Mirror horizontal";
        case MinimapTransform::MirrorV:           return "Mirror vertical";
        case MinimapTransform::Transpose_MirrorH: return "Transpose + mirror H";
        case MinimapTransform::Transpose_MirrorV: return "Transpose + mirror V";
        case MinimapTransform::Transpose_Rot180:  return "Transpose + 180";
        case MinimapTransform::Count_:            break;
    }
    return "unknown";
}

QImage NavMeshView::applyMinimapTransform(QImage const& img, MinimapTransform t)
{
    // The bare transpose matrix (0,1,1,0,0,0) maps (x,y) -> (y,x), i.e.
    // dst(col=y,row=x) = src(col=x,row=y).
    QTransform const kTranspose(0, 1, 1, 0, 0, 0);
    switch (t)
    {
        case MinimapTransform::Identity:
            return img;
        case MinimapTransform::Transpose:
            return img.transformed(kTranspose);
        case MinimapTransform::Rotate90CW:
        {
            QTransform r;
            r.rotate(90.0);
            return img.transformed(r);
        }
        case MinimapTransform::Rotate90CCW:
        {
            QTransform r;
            r.rotate(-90.0);
            return img.transformed(r);
        }
        case MinimapTransform::Rotate180:
            return img.mirrored(true, true);
        case MinimapTransform::MirrorH:
            return img.mirrored(true, false);
        case MinimapTransform::MirrorV:
            return img.mirrored(false, true);
        case MinimapTransform::Transpose_MirrorH:
            return img.transformed(kTranspose).mirrored(true, false);
        case MinimapTransform::Transpose_MirrorV:
            return img.transformed(kTranspose).mirrored(false, true);
        case MinimapTransform::Transpose_Rot180:
            return img.transformed(kTranspose).mirrored(true, true);
        case MinimapTransform::Count_:
            break;
    }
    return img;
}

std::vector<NavMeshView::CanonicalTileReport> NavMeshView::inspectCanonicalTiles()
{
    // Probe three known-good Eastern Kingdoms tiles for side-by-side
    // diagnostic comparison.  Coords are (gx, gy) pairs in TC notation;
    // the operator picked these because each one falls within a distinct
    // EK landmass (north Tirisfal, mid Stormwind, south Searing Gorge),
    // so a swap or mirror is immediately obvious in the dock readout.
    constexpr std::pair<int, int> kCanonical[] = { {34, 61}, {32, 48}, {49, 36} };

    auto const pad2 = [](int v) {
        std::string s = std::to_string(v);
        if (s.size() == 1)
            s.insert(s.begin(), '0');
        return s;
    };

    std::vector<CanonicalTileReport> out;
    out.reserve(std::size(kCanonical));

    std::optional<std::string> dirOpt;
    if (m_mapDb2 && m_heightmapMapId != 0)
        dirOpt = m_mapDb2->directoryFor(m_heightmapMapId);

    for (auto const& [gx, gy] : kCanonical)
    {
        CanonicalTileReport r;
        r.gx = gx;
        r.gy = gy;
        r.resolvedFdid  = QStringLiteral("n/a");
        r.blpEncoding   = QStringLiteral("n/a");
        r.qimageFormat  = QStringLiteral("n/a");

        if (!dirOpt)
        {
            r.notes = QStringLiteral("Map.db2 has no directory for current mapId (no map loaded?)");
            out.push_back(std::move(r));
            continue;
        }

        // 2026-05-26: operator A/B confirmed minimap landmass appears 90 CCW
    // from heightmap.  Both layers share the same world AABB, so the only
    // place that rotation can come from is the filename->cell mapping.
    // Swap gx/gy so each TC tile cell loads the BLP whose content actually
    // belongs at TC's (gx=N-S row, gy=E-W col) world position.
    std::string const canonicalPad = "map" + pad2(gy) + "_" + pad2(gx);
        std::string const vpath = "world/minimaps/" + *dirOpt + "/" + canonicalPad + ".blp";

        // FDID resolution (listfile).  Always probed first so the dock
        // can show what FDID a tile maps to even when CASC is closed.
        uint32_t fdid = 0;
        if (m_listfile && !m_listfile->empty())
        {
            if (auto resolved = m_listfile->resolveFdid(vpath))
            {
                fdid = *resolved;
                r.resolvedFdid = QString::number(fdid);
            }
            else
            {
                r.resolvedFdid = QStringLiteral("no listfile match");
            }
        }
        else
        {
            r.resolvedFdid = QStringLiteral("no listfile wired");
        }

        if (!m_cascClient || !m_cascClient->isOpen())
        {
            r.notes = QStringLiteral("CASC not open; vpath=%1").arg(QString::fromStdString(vpath));
            out.push_back(std::move(r));
            continue;
        }

        std::vector<uint8_t> blob;
        bool gotBlob = false;
        if (m_cascClient->readByPath(vpath, blob))
        {
            gotBlob = true;
            r.notes = QStringLiteral("CASC hit via vpath");
        }
        else if (fdid != 0 && m_cascClient->openByFileDataId(fdid, blob))
        {
            gotBlob = true;
            r.notes = QStringLiteral("CASC hit via FDID");
        }
        else
        {
            r.notes = QStringLiteral("CASC miss for vpath=%1 and fdid=%2")
                .arg(QString::fromStdString(vpath))
                .arg(fdid);
            out.push_back(std::move(r));
            continue;
        }

        // Peek the BLP header (bytes 0x08-0x0B carry colorEncoding /
        // alphaDepth / alphaEncoding / hasMips per BlpReader.cpp).
        if (gotBlob && blob.size() >= 0x0C
            && blob[0] == 'B' && blob[1] == 'L' && blob[2] == 'P' && blob[3] == '2')
        {
            uint8_t const colorEncoding = blob[0x08];
            uint8_t const alphaEncoding = blob[0x0A];
            if (colorEncoding == 1)
                r.blpEncoding = QStringLiteral("Pal8");
            else if (colorEncoding == 2)
            {
                switch (alphaEncoding)
                {
                    case 0: r.blpEncoding = QStringLiteral("DXT1"); break;
                    case 1: r.blpEncoding = QStringLiteral("DXT3"); break;
                    case 7: r.blpEncoding = QStringLiteral("DXT5"); break;
                    default: r.blpEncoding = QStringLiteral("DXT?"); break;
                }
            }
            else if (colorEncoding == 3)
                r.blpEncoding = QStringLiteral("ARGB");
            else
                r.blpEncoding = QStringLiteral("enc=%1").arg(colorEncoding);
        }

        io::BlpImage decoded;
        if (io::decodeBlp(blob, decoded) && decoded.width > 0 && decoded.height > 0)
        {
            QImage const img(decoded.rgba.data(), decoded.width, decoded.height,
                             decoded.width * 4, QImage::Format_RGBA8888);
            r.qimageFormat = QStringLiteral("Format_RGBA8888 (%1) %2x%3")
                .arg(int(img.format()))
                .arg(decoded.width)
                .arg(decoded.height);
        }
        else
        {
            r.qimageFormat = QStringLiteral("decode failed (%1 bytes)").arg(blob.size());
        }
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<std::pair<int, int>> const&
NavMeshView::scanMapFileCoverageForMapId(uint32_t mapId)
{
    auto const it = m_mapFileCoverage.find(mapId);
    if (it != m_mapFileCoverage.end())
        return it->second;

    std::vector<std::pair<int, int>> tiles;

    // Determine the source maps directory.  The MapTileCache exposes its
    // own mapsDir; that is the authoritative source for `.map` files.
    if (!m_mapCache)
    {
        auto [emplaced, ok] = m_mapFileCoverage.emplace(mapId, std::move(tiles));
        (void)ok;
        return emplaced->second;
    }

    std::filesystem::path const mapsDir = m_mapCache->mapsDir();
    if (mapsDir.empty())
    {
        auto [emplaced, ok] = m_mapFileCoverage.emplace(mapId, std::move(tiles));
        (void)ok;
        return emplaced->second;
    }

    // Filename pattern: "%04u_%02d_%02d.map" -- see io::mapTileFilename().
    // The mapId field is zero-padded to 4 chars but widens past mapId>=10000,
    // and the gx/gy fields are zero-padded to 2 chars (widen past 100,
    // though no TC map needs that).  Match by prefix matching the padded
    // mapId followed by '_' and a ".map" suffix; parse gx/gy from the middle.
    char prefixBuf[16];
    std::snprintf(prefixBuf, sizeof(prefixBuf), "%04u_", mapId);
    std::string const prefix = prefixBuf;

    std::error_code ec;
    std::filesystem::directory_iterator dirIt(mapsDir, ec);
    if (ec)
    {
        auto [emplaced, ok] = m_mapFileCoverage.emplace(mapId, std::move(tiles));
        (void)ok;
        return emplaced->second;
    }
    constexpr char kSuffix[] = ".map";
    constexpr size_t kSuffixLen = sizeof(kSuffix) - 1;
    for (auto const& entry : dirIt)
    {
        std::error_code feErr;
        if (!entry.is_regular_file(feErr)) continue;
        std::string const name = entry.path().filename().string();
        if (name.size() < prefix.size() + 3 + kSuffixLen) continue; // "0_0.map" min
        if (name.compare(0, prefix.size(), prefix) != 0) continue;
        if (name.compare(name.size() - kSuffixLen, kSuffixLen, kSuffix) != 0) continue;
        // Middle slice: "<gx>_<gy>".
        std::string const middle = name.substr(prefix.size(), name.size() - prefix.size() - kSuffixLen);
        size_t const sep = middle.find('_');
        if (sep == std::string::npos || sep == 0 || sep + 1 >= middle.size()) continue;
        std::string const gxStr = middle.substr(0, sep);
        std::string const gyStr = middle.substr(sep + 1);
        auto allDigits = [](std::string const& s) {
            if (s.empty()) return false;
            for (char c : s)
                if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            return true;
        };
        if (!allDigits(gxStr) || !allDigits(gyStr)) continue;
        try
        {
            int const gx = std::stoi(gxStr);
            int const gy = std::stoi(gyStr);
            tiles.emplace_back(gx, gy);
        }
        catch (...) { continue; }
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    auto [emplaced, ok] = m_mapFileCoverage.emplace(mapId, std::move(tiles));
    (void)ok;
    return emplaced->second;
}

} // namespace world_editor::render

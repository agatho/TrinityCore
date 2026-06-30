/*
 * Coords.h - single-source-of-truth for coordinate-system conventions
 *            in the TrinityCore world editor.
 *
 * Reference: HANDOFF_NATIVE_EDITOR.md  6 ("Coordinate system primer").
 *
 * Every transform in the editor MUST funnel through helpers declared
 * here, so the conventions live in one place and drift is detectable
 * via grep.
 *
 * WARNING: the yaw direction / zero-heading comment below is from the
 * handover doc. Before Phase 1 ships, the value MUST be verified
 * against Position::SetOrientation in
 *   src/server/game/Entities/Object/Position.cpp
 * If it disagrees with TC, fix THIS file, not TC.
 */

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace world_editor::coords
{

// ----- TrinityCore world frame (right-handed) ---------------------------

// World X axis: +X = north,  -X = south.
// World Y axis: +Y = west,   -Y = east.
// World Z axis: +Z = up.
//
// Yaw (orientation): radians, kept in [0, 2*pi). Verified against
// Position::RelocateOffset and Position::IsWithinBox in
// src/server/game/Entities/Object/Position.cpp on 2026-05-22:
//   - yaw =  0       -> facing +X (NORTH)
//   - yaw =  pi/2    -> facing +Y (WEST)   (CCW viewed from above)
//   - yaw =  pi      -> facing -X (SOUTH)
//   - yaw =  3*pi/2  -> facing -Y (EAST)
// The "counter-clockwise" remark in Position.cpp:73 corroborates this.

struct WorldPos
{
    float x = 0.0f;   // north(+) / south(-)
    float y = 0.0f;   // west(+)  / east(-)
    float z = 0.0f;   // up
};

// ----- Detour frame -----------------------------------------------------

// Detour stores positions in a Y-up frame; TC swaps to/from on every
// nav-mesh query. The canonical swap (see PathGenerator.cpp and
// mmap_probe.cpp) is:
//
//     dt[0] = tc.y;   dt[1] = tc.z;   dt[2] = tc.x;
//
// Inverse:  tc.x = dt[2];  tc.y = dt[0];  tc.z = dt[1];

inline constexpr std::array<float, 3> toDetour(WorldPos const& tc) noexcept
{
    return { tc.y, tc.z, tc.x };
}

inline constexpr WorldPos fromDetour(std::array<float, 3> const& dt) noexcept
{
    return WorldPos{ dt[2], dt[0], dt[1] };
}

// ----- ADT tile grid ----------------------------------------------------

// 64 x 64 tiles per map.  Tile size = 533.33333 yards (1600/3).
// Map center (0,0) sits at tx=32, ty=32.
//
// File naming on disk: "<mapId>_<tx>_<ty>.adt"  (some tools swap the
// pair; the editor MUST resolve via TileFromWorld() below, never via
// raw string parsing).

inline constexpr float TILE_SIZE = 533.33333f;
inline constexpr int   TILE_COUNT_PER_AXIS = 64;
inline constexpr int   TILE_CENTER_INDEX   = 32;
inline constexpr float MAP_HALF_EXTENT     = TILE_SIZE * TILE_CENTER_INDEX; // 17066.666

struct TileXY
{
    int tx = 0;   // along world Y (west-east)
    int ty = 0;   // along world X (north-south)
};

// World -> tile indices.  Anchors the axis convention TC's
// Map::GetGridXY uses (verify once at Phase 1 against
// src/server/game/Maps/Map.cpp - lock the result here, never re-derive).
inline constexpr TileXY tileFromWorld(WorldPos const& w) noexcept
{
    TileXY out;
    out.tx = TILE_CENTER_INDEX - static_cast<int>(w.y / TILE_SIZE);
    out.ty = TILE_CENTER_INDEX - static_cast<int>(w.x / TILE_SIZE);
    return out;
}

// ----- 2D screen / image frame ------------------------------------------

// The editor's 2D top-down view follows mmap_world_dump's convention:
//   image +X = world -Y  (i.e. east goes RIGHT)
//   image +Y = world -X  (i.e. south goes DOWN, so north is UP)
// Origin (0,0) sits at the top-left of the image, matching every other
// Qt/GL framebuffer.
//
// View transform parameters:
//   yardsPerPixel  - world-space size of one pixel.
//   anchorWorld    - the world coord that maps to anchorPixel.
//   anchorPixel    - the screen pixel that anchors the view (usually
//                    the widget center, so pan/zoom rotates around the
//                    cursor without re-anchoring).

struct ViewTransform
{
    float    yardsPerPixel = 4.0f;
    WorldPos anchorWorld   = { 0.0f, 0.0f, 0.0f };
    int      anchorPixelX  = 0;
    int      anchorPixelY  = 0;
};

inline std::pair<float, float> worldToScreen(ViewTransform const& v, WorldPos const& w) noexcept
{
    float const sx = v.anchorPixelX + (v.anchorWorld.y - w.y) / v.yardsPerPixel;
    float const sy = v.anchorPixelY + (v.anchorWorld.x - w.x) / v.yardsPerPixel;
    return { sx, sy };
}

inline WorldPos screenToWorld(ViewTransform const& v, float sx, float sy, float z = 0.0f) noexcept
{
    WorldPos out;
    out.y = v.anchorWorld.y - (sx - v.anchorPixelX) * v.yardsPerPixel;
    out.x = v.anchorWorld.x - (sy - v.anchorPixelY) * v.yardsPerPixel;
    out.z = z;
    return out;
}

// ----- Gameobject rotation ----------------------------------------------

// gameobject.rotation{0..3} is a quaternion stored as (x, y, z, w).
// gameobject.orientation is the yaw (z-axis) component, kept for legacy
// query paths.  When the editor rotates a GO, BOTH must be written in
// the same row update; Phase 3 will own that path via a single helper.
//
// Verify quaternion component order against
//   GameObject::SetWorldRotationFromTransformMatrix in
//   src/server/game/Entities/GameObject/GameObject.cpp
// before Phase 3 writes back to the DB.

struct Quat
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

inline Quat quatFromYaw(float yaw) noexcept
{
    float const half = yaw * 0.5f;
    return Quat{ 0.0f, 0.0f, std::sin(half), std::cos(half) };
}

} // namespace world_editor::coords

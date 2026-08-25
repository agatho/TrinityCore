/*
 * HandcraftedRoadStorage - worldserver-side read-only cache of the
 * `handcrafted_road` world-DB table.
 *
 * Loaded once at SetInitialWorldSettings. The Map / MapManager hook (wired
 * by a separate change) calls GetForMap(mapId) and applies each segment as a
 * NAV_AREA_ROAD=7 tag on the intersected detour polys.
 *
 * The editor (world_editor::io::HandcraftedRoadRepo) is the only writer;
 * worldserver never INSERTs into this table at runtime. A reload from
 * console (.reload handcrafted_road - future) would re-call LoadFromDB().
 */

#ifndef TRINITYCORE_HANDCRAFTED_ROAD_STORAGE_H
#define TRINITYCORE_HANDCRAFTED_ROAD_STORAGE_H

#include "Define.h"

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

struct HandcraftedRoadSegment
{
    uint32 id       = 0;
    uint32 mapId    = 0;
    float  fromX    = 0.0f;
    float  fromY    = 0.0f;
    float  toX      = 0.0f;
    float  toY      = 0.0f;
    float  width    = 8.0f;   // yards
    std::string comment;
    bool   verified = false;
};

class TC_GAME_API HandcraftedRoadStorage
{
public:
    // Called from World::SetInitialWorldSettings (or equivalent). Clears any
    // prior in-memory state, then re-fetches every row from `handcrafted_road`.
    static void LoadFromDB();

    // Empty vector if mapId has no handcrafted segments.
    static std::vector<HandcraftedRoadSegment> const& GetForMap(uint32 mapId);

    // For diagnostics / .reload command future-proofing.
    static size_t SegmentCount();
    static size_t MapCount();

    // Enumerate every mapId that has at least one loaded segment.
    // Surfaced by the `.handcrafted_road status` chat command. The
    // returned vector is sorted ascending so output is deterministic.
    static std::vector<uint32> MapIds();

    // Monotonic counter bumped on every LoadFromDB(). Consumers that cache a
    // derived structure (e.g. the Playerbot HandcraftedRoadGraph centerline
    // graph) compare this against the generation they built against to detect
    // a `.reload handcrafted_road` and rebuild — no cross-module call needed.
    static uint32 Generation();

private:
    static std::unordered_map<uint32, std::vector<HandcraftedRoadSegment>> _byMap;
    static std::vector<HandcraftedRoadSegment> _empty;
    static std::atomic<uint32> _generation;
};

#endif // TRINITYCORE_HANDCRAFTED_ROAD_STORAGE_H

// WorldMetadata - GM-curated world knowledge points (roads, cities, villages,
// crossroads, danger zones, hubs, vendors, mailboxes, innkeepers).
//
// Two consumers:
//   1. mmaps_generator: at regen time, reads `kind=Road` rows for the target
//      map and tags polygons within `radius` as NAV_AREA_ROAD. Solves the
//      Teldrassil road-tagging gap (the kalidar tileset's road textures
//      reference correctly but the classifier's effectId-confidence gate
//      rejects them, leaving zero road MCNKs despite road textures being
//      referenced; see project_teldrassil_road_classifier).
//   2. BotSnapshotBuilder: loads same rows once at startup; exposes a
//      WorldMetadataView::nearest(kind) helper so bot rules query
//      "am I in a city?", "is there a road within 50y?", etc. without
//      depending on extracted ADT data.
//
// Population: in-game `.playerbot meta add <kind>` GM command captures the
// GM's current position and zone, inserts a row. No client-side editor —
// the in-game capture flow IS the editor. List/delete subcommands round
// out CRUD.
//
// Storage: `characters` database, `playerbot_v2_world_metadata` table.
// See sql/playerbot_v2/0009_world_metadata.sql.

#pragma once

#include "Define.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::World
{
    // Wire-stable enum — values match the `kind` column in DB. Adding a new
    // kind requires (a) appending here, (b) updating the kind table at
    // BotCommandParser's `meta add` handler, (c) updating ParseKind() below.
    enum class WorldMetadataKind : uint8
    {
        Unknown    = 0,
        Road       = 1,
        Crossroad  = 2,
        City       = 3,
        Village    = 4,
        Hub        = 5,
        Danger     = 6,
        Vendor     = 7,
        Mailbox    = 8,
        Innkeeper  = 9,
        Other      = 10,
        // Vertical / multi-modal transit hints (route planner uses
        // these to cross floors via elevators and use transports
        // instead of forcing terrain-only pathfinding):
        //   Elevator -- between-floor transition.
        //   Dock     -- transport boarding point.  Includes both
        //               boat docks AND zeppelin towers; both are
        //               GO transports whose route can't be
        //               expressed as a contiguous navmesh walk.
        // Mirror of the world_editor's `render::AnnotationKind` --
        // wire-stable; append only, never renumber.
        Elevator   = 11,
        Dock       = 12,

        Count_
    };

    // Pretty-print + parse — used by GM command + diag output. Keep in
    // sync with the enum above.
    char const* KindToString(WorldMetadataKind k);
    WorldMetadataKind ParseKind(std::string const& s);

    // POD record matching the DB row. Loaded once at server start; updated
    // incrementally when `meta add/delete` mutates the table.
    struct WorldMetadataRecord
    {
        uint64               id        = 0;
        uint32               map_id    = 0;
        uint32               zone_id   = 0;
        WorldMetadataKind    kind      = WorldMetadataKind::Unknown;
        float                x         = 0.f;
        float                y         = 0.f;
        float                z         = 0.f;
        float                radius    = 10.f;
        std::string          label;
        std::string          notes;
        std::string          created_by;
    };

    // Thread-safe cache of all rows. Loaded synchronously at server boot;
    // refreshed on `meta add/delete/edit` GM commands. Readers take a
    // shared_lock; the rare writer takes a unique_lock.
    //
    // Snapshot-builder reads under shared_lock once per Build (returns a
    // const ref-counted copy via Snapshot()). Bot rules query via
    // BotSnapshotView::world_metadata() which mirrors the snapshot copy.
    class WorldMetadataStore
    {
    public:
        // Load all rows from characters.playerbot_v2_world_metadata.
        // Idempotent — clears and re-populates on each call. Returns the
        // number of rows loaded (0 on empty table, -1 on DB error).
        int64 ReloadFromDb();

        // Insert one record (and persist to DB). `id` field on `r` is
        // ignored on input; populated on output with the new auto_increment
        // value. Returns true on success.
        bool Insert(WorldMetadataRecord& r);

        // Remove by id (and persist). Returns true if a row was deleted.
        bool Delete(uint64 id);

        // Modify the radius of an existing row. Returns true if a row was
        // updated. Persists to DB. Other fields (kind, position) are
        // intentionally not mutable — for those, delete + readd.
        bool UpdateRadius(uint64 id, float new_radius);

        // Update label/notes for an existing row. Same write-through model.
        bool UpdateLabel(uint64 id, std::string const& new_label);
        bool UpdateNotes(uint64 id, std::string const& new_notes);

        // Count of rows currently in cache.
        size_t Size() const;

        // Copy of all records (thread-safe). Used by snapshot builder.
        std::vector<WorldMetadataRecord> Snapshot() const;

        // Records on the given map, optionally filtered to a kind. Returns
        // a copy under shared_lock. Used by mmaps_generator override.
        std::vector<WorldMetadataRecord> RecordsForMap(uint32 map_id) const;
        std::vector<WorldMetadataRecord> RecordsForMapAndKind(uint32 map_id,
                                                              WorldMetadataKind kind) const;

        // Singleton accessor — there's exactly one store per worldserver.
        static WorldMetadataStore& Instance();

    private:
        WorldMetadataStore() = default;

        mutable std::shared_mutex            mtx_;
        std::unordered_map<uint64, WorldMetadataRecord> by_id_;
        // Last load wall-clock time, for `.playerbot meta status`.
        std::chrono::system_clock::time_point loaded_at_{};
    };

} // namespace Playerbot::V2::World

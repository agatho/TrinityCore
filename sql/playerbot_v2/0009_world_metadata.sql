-- Migration: 0009_world_metadata
-- Date:    2026-05-22
-- Purpose: Persist GM-curated world-knowledge points (road centerlines,
--          city/village footprints, crossroads, danger zones, hubs)
--          so bots have authoritative situational data independent of
--          texture-classifier heuristics. Two consumers:
--             1. mmaps_generator reads `kind='road'` rows at regen time
--                and tags polygons within `radius` as NAV_AREA_ROAD.
--                Solves the Teldrassil road-tagging gap (kalidar
--                tileset's road textures don't pass the classifier's
--                effectId confidence gate, leaving zero road MCNKs in
--                .road files despite road textures being referenced).
--             2. BotSnapshotBuilder loads same rows once at startup
--                and bots query `nearest_metadata(kind)` for awareness
--                — "am I in a city?", "is the nearest crossroad N
--                yards away?", etc.
-- Reverts: yes (DROP TABLE).
--
-- Population mechanism: in-game GM types `.playerbot meta add road [r]`
-- standing on the spot to capture; the command writes a row keyed by
-- the GM's current position and zone. List/edit/delete subcommands
-- provided. No client-side editor — the in-game capture flow IS the
-- editor.
--
-- Schema versioning: kinds enumerated as a small uint8 set rather than
-- VARCHAR so the on-disk column is fixed-width and the C++ side can
-- compare against an enum without string match. Names below match
-- WorldMetadataKind in code:
--      0=unknown   (defensive default)
--      1=road      (road centerline waypoint; mmaps_generator consumes)
--      2=crossroad (intersection — used by route planner hints)
--      3=city      (capital / major settlement footprint)
--      4=village   (minor settlement footprint)
--      5=hub       (quest hub / outpost)
--      6=danger    (avoid-this-area, e.g. elite spawn zone)
--      7=vendor    (general vendor anchor — bot navigation hint)
--      8=mailbox   (mailbox anchor)
--      9=innkeeper (innkeeper anchor, e.g. hearth-rebind target)
--     10=other     (operator-defined; check label/notes for context)
--
-- `radius` (yards) is mandatory for area-kinds (city/village/hub/
-- danger). For point-kinds (road waypoint, crossroad, vendor, mailbox,
-- innkeeper) radius is the "effective range" — e.g. road waypoint with
-- radius=15 means polygons within 15y of the point get NAV_AREA_ROAD
-- tagged. Default radii are picked by the GM command, not the schema.
--
-- `zone_id` (Area.db2 ID) is denormalized for fast filter; we DO NOT
-- enforce FK because the area table lives in client data we don't
-- mirror.
--
-- Indexes: (map_id, kind) for the most common query (load all roads
-- for map X), plus (map_id, x, y) for nearest-point lookups via the
-- bot snapshot.

CREATE TABLE IF NOT EXISTS playerbot_v2_world_metadata (
    id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    map_id          INT  UNSIGNED   NOT NULL,
    zone_id         INT  UNSIGNED   NOT NULL DEFAULT 0,
    kind            TINYINT UNSIGNED NOT NULL,
    pos_x           FLOAT NOT NULL,
    pos_y           FLOAT NOT NULL,
    pos_z           FLOAT NOT NULL,
    radius          FLOAT NOT NULL DEFAULT 10.0,
    label           VARCHAR(96)  NOT NULL DEFAULT '',
    notes           VARCHAR(255) NOT NULL DEFAULT '',
    created_by      VARCHAR(64)  NOT NULL DEFAULT '',
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
                              ON UPDATE CURRENT_TIMESTAMP,
    KEY idx_map_kind (map_id, kind),
    KEY idx_map_xy   (map_id, pos_x, pos_y),
    KEY idx_zone     (zone_id),
    KEY idx_kind     (kind)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (9, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;

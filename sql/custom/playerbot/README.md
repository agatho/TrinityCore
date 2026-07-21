# Playerbot shared database

The Playerbot V2 module reads several tables from a **separate database**, not from world or
characters. Its name comes from the worldserver config key `Playerbot.SharedDatabase` (default
`playerbot`), and every query is cross-database qualified, e.g.

```sql
SELECT ... FROM playerbot.handcrafted_road ORDER BY mapId, id
```

That database is **not created by the TrinityCore SQL updater**, which only manages auth, characters,
world and hotfixes. It has to be created and populated separately - `playerbot_shared.sql` here is the
schema.

## Degradation when it is absent

Every loader treats a missing or empty table as "no data" and logs it rather than failing, so a server
starts fine without this database. What you lose is silent:

| table | consumer | effect when absent |
|---|---|---|
| `handcrafted_road` | `HandcraftedRoadStorage` | no road corridors are applied to the navmesh, so bots never prefer roads |
| `playerbot_dungeon_routes` | `DungeonScript` | no dungeon route waypoints - bots cannot run a dungeon path |
| `playerbot_nav_links` | `DungeonScript`, `State_Idle` | no authored traversal links (ledge drops, gap hops) |
| `playerbots_names` | bot creation | no name pool for generated bots |

## Road data specifically

`handcrafted_road` is the *working* road mechanism. The texture-based auto-detection in the mmaps
generator does not work and is disabled, so an empty `handcrafted_road` means no road preference at
all, not "fall back to automatic".

Corridors are applied by `Road::ApplyCorridorsToNavmesh` at **map-load time**, retagging navmesh
polygons with `NAV_AREA_ROAD`. That means adding road data needs **no mmap regeneration** - the
navmesh files on disk are untouched and only the in-memory tags change.

Segments are authored with the `world_editor` tool (`HandcraftedRoadDock`), which previews exactly
which polygons a segment will retag using the same `RoadCorridor` code the server runs, so the preview
is byte-exact with the runtime result.

## Schema provenance

`playerbot_shared.sql` is a **real `mysqldump --no-data` of the playerbot test environment**, not a
reconstruction. 49 tables; the one `*_bak_*` scratch table was excluded, and no data or credentials
are included.

Worth knowing: an earlier reconstruction from the loader `SELECT`s got the structure right but the
details wrong - `handcrafted_road.width` defaults to 8 rather than 10, `playerbot_nav_links.radius` to
12 rather than 2, `playerbots_names.gender` has three values (0/1/2, neutral) rather than two, and
columns that no query touches (`created_by`, `created_at`, `race_mask`, `is_taken`, `used_by_guid`,
`kind`) were invisible from the code alone. Inferring a schema from its readers is not good enough.

Note also that the module reads only four of these 49 tables through the queries visible in the core;
the rest belong to bot account pooling, templates, scheduling, statistics and dragonriding, and are
driven from inside the module.

## Database name

The test environment names this database **`wowc_playerbot`**, not the module default `playerbot`.
Always take the name from `Playerbot.SharedDatabase` rather than assuming.

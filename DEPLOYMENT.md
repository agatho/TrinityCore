# Deployment manifest — integration/all-systems

What this build needs beyond the source: which databases exist, what creates them, which config keys
matter, and which build options change the result. Written because integrating ~50 feature branches
means gathering their schemas and settings, not only their code — and several of the gaps here fail
**silently** rather than loudly.

Reference environment: `M:\PlayerbotServer\worldserver.conf`.

## Branches

| branch | contents |
|---|---|
| `integration/all-systems` | main line: all feature work, no bots |
| `integration/no-bots` | same tree, kept as the explicitly bot-free line |
| `integration/with-bots` | the above **plus** the Playerbot V2 module and its core hooks |

`with-bots` is a superset. Because every bot hook is compiled out under `#if defined(TRINITY_PLAYERBOT_V2)`
and `PlayerbotHooks.h` supplies inline no-op bodies otherwise, building `with-bots` with
`BUILD_PLAYERBOT_V2=0` produces an ordinary bot-free server. Two divergent branches are therefore not
strictly required.

## Databases

Five, not four. The TrinityCore SQL updater manages only the first four.

| database | config key | managed by updater | notes |
|---|---|---|---|
| auth | `LoginDatabaseInfo` | yes | `sql/base/auth_database.sql` + `sql/updates/auth` |
| characters | `CharacterDatabaseInfo` | yes | `sql/base/characters_database.sql` + `sql/updates/characters` |
| world | `WorldDatabaseInfo` | yes | `sql/updates/world` |
| hotfixes | `HotfixDatabaseInfo` | yes | `sql/updates/hotfixes` |
| **playerbot shared** | `PlayerbotsDatabaseInfo` + **`Playerbot.SharedDatabase`** | **no** | `sql/custom/playerbot/playerbot_shared.sql` — import by hand |

The shared database is referenced cross-database in queries (`FROM <shared>.handcrafted_road`), so
**`Playerbot.SharedDatabase` must name it correctly**. The reference environment uses
**`wowc_playerbot`**, not the module default `playerbot`. Point it at a database that does not exist
and every loader logs "empty or missing" and continues with zero rows.

## Feature tables

Roughly 108 custom tables are created across auth/characters/custom by the integrated features, plus
world-side tables (`world_quest_template`, `warband_reputation_faction`, `club_finder_posting`,
`club_finder_application`, `contribution*`, `delve*`). All are covered by the SQL in this repository —
verified by cross-checking every `CREATE TABLE` name against code references.

The playerbot shared database adds **49 more** that this repository does *not* create automatically.
Only four of those are visible in queries from the core (`handcrafted_road`,
`playerbot_dungeon_routes`, `playerbot_nav_links`, `playerbots_names`); the remainder cover bot account
pooling, templates, scheduling, statistics and dragonriding and are driven from inside the module.

## Build options

| option | default | effect |
|---|---|---|
| `BUILD_PLAYERBOT_V2` | `0` | builds `src/modules/PlayerbotV2` and defines `TRINITY_PLAYERBOT_V2`. With it off, bot sources are excluded from the game target entirely. |
| `TOOLS` | `0` | **required for handcrafted roads.** `RoadCorridor` lives in `src/tools/extractor_common`; with `TOOLS=0` the road code is compiled out via `TRINITY_HANDCRAFTED_ROADS`. |

`dep/recastnavigation/Detour` is built with `DT_VIRTUAL_QUERYFILTER=1`. Without it `dtQueryFilterTC`
overrides nothing — the base vtable is empty, so the road-cost formula compiles and silently never
runs.

## Configuration

59 `Playerbot*` keys are read from config, but only `Playerbot.SharedDatabase` is documented in
`worldserver.conf.dist` — in this repository *and* upstream in playerbot-v2. Everything else is read
with a built-in default and set ad hoc. The reference environment sets 37 of them; treat that file as
the working documentation until the section is written properly.

Keys with real deployment impact:

- `Playerbot.SharedDatabase` — see above.
- `PlayerbotsDatabaseInfo`, `PlayerbotsDatabase.WorkerThreads`, `PlayerbotsDatabase.SynchThreads` — the
  connection pool for that database.
- `Playerbot.Bg.Coordinator.Enable`, `Playerbot.Pve.Coordinator.Enable` — the battleground and PvE
  coordinators.

## Things that fail silently

Collected because none of these produce an error a reader would notice:

1. **Missing playerbot shared database or wrong name** → every loader logs "empty or missing" and
   continues. Result: bots with no road preference, no dungeon routes, no traversal links and no name
   pool. Looks like bad AI, not a missing database.
2. **`handcrafted_road` empty** → no road preference at all. The texture-based auto-detection does not
   work and is disabled, so there is no fallback. Roads are corridors applied at map-load time by
   `Road::ApplyCorridorsToNavmesh`, which means adding road data needs **no mmap regeneration**.
3. **`Detour` built without `DT_VIRTUAL_QUERYFILTER`** → road costs silently ignored.
4. **`TOOLS=0`** → handcrafted roads compiled out entirely, with no warning.
5. **SMSG registered `STATUS_UNHANDLED`** → the send is dropped at the `SendPacket` gate with an error
   log, not a crash. Conversely, flipping an opcode to `STATUS_NEVER` that has no send-site makes the
   gap tooling report it as implemented.

## Current state

- `no-bots` / `all-systems`: build clean, `--target worldserver`.
- `with-bots` with `BUILD_PLAYERBOT_V2=0`: builds clean.
- `with-bots` with `BUILD_PLAYERBOT_V2=1`: **does not yet link.** The core side is done; the remaining
  errors are module-facing core APIs still unported (`ClearAppliedHandcraftedRoads`, `TerrainMgrDetail`,
  `GameObject::GetControlZoneValue`, a fmt format-string issue).
- Nothing here has been run against a live server. Compilation and wire correctness are verified;
  startup, schema application and runtime behaviour are not.

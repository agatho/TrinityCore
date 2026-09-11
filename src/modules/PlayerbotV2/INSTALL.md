# PlayerbotV2 — installation

Setting up a server with bots, from a clean machine. Steps 1–5 are required;
step 6 is for an existing server whose world database has drifted and is **not**
part of a fresh install.

`playerbot.conf.dist` referenced this file before it existed. This is it.

---

## 0. What the module needs

Two databases beyond TrinityCore's usual three:

| database | holds | scope |
|---|---|---|
| `characters` (TC's own) | everything keyed to a character guid or account — spawned bots, personalities, preferences, squad presets, guild membership, craft orders, fleet telemetry | **per realm** |
| `playerbot` (new) | realm-independent content — the bot name pool, dungeon route waypoints, nav links, handcrafted roads, generation templates, class/race distributions, talent builds | **shared by every realm** |

The split is not cosmetic. Anything keyed on a character guid cannot be pooled
across realms: character guid 12442 exists on every realm, so those rows would
collide. Conversely, map-derived nav data and generation templates are identical
everywhere, so one copy serves all realms.

The name pool illustrates both halves. The list of names is static and shared;
whether a name is *taken* is per-realm, because character names are unique per
realm. The pool lives in the shared database and each realm tracks its own
consumption.

---

## 1. Build with the module enabled

```
cmake -DBUILD_PLAYERBOT_V2=1 ...
```

Without it `TRINITY_PLAYERBOT_V2` is undefined and the build fails in
`BattleGroundHandler.cpp` / `LFGHandler.cpp`, where `WorldSession::IsBot()` is
called unconditionally but declared only under that macro. If you see
`error C3861: 'IsBot': identifier not found`, this flag is why.

## 2. Install TrinityCore normally

Create and populate `auth`, `characters` and `world` exactly as upstream
documents, and import the TDB world database. Nothing in this module replaces
that.

## 3. Create and seed the shared database

```
mysql -u<user> -p < sql/playerbot_shared/playerbot_shared_schema.sql
mysql -u<user> -p playerbot < sql/playerbot_shared/playerbot_shared_data.sql
```

The schema file creates the database itself (`CREATE DATABASE IF NOT EXISTS
playerbot`), 49 tables, 7 views, 10 routines and 2 triggers. The data file adds
~117,600 rows of static content, including a pristine 107,339-name pool.

Both files are non-destructive — no `DROP` statements — and carry no `DEFINER`
clauses, so any account can restore them.

**Using a different schema name?** Edit the `CREATE DATABASE` and `USE` lines at
the top of the schema file and set `Playerbot.SharedDatabase` (step 4) to match.
The compiled-in default is `playerbot`, so leaving it alone requires no config.

## 4. Activate the module config

The build copies the template to `worldserver.conf.d/playerbot.conf.dist`. Drop
the suffix:

```
mv etc/worldserver.conf.d/playerbot.conf.dist etc/worldserver.conf.d/playerbot.conf
```

`ConfigMgr::LoadAdditionalDir` walks that directory and loads every `*.conf`, so
the rename is all that is needed. A `.dist` file is ignored by design.

Skipping this step is not fatal — every `Playerbot.*` key falls back to its
compiled-in default, and `Playerbot.SharedDatabase` already defaults to
`playerbot` — but nothing is tunable until you do it.

## 5. First start

On boot `PlayerbotMigrationMgr` scans `sql/playerbot_v2/` and applies each
migration to whichever database it targets, tracking each in that database's own
`playerbot_v2_schema_version` ledger. Nothing to run by hand.

It finds the directory through the `SourceDirectory` config key, which defaults
to the `CMAKE_SOURCE_DIR` baked in at build time. If you run the server on a
machine that has no source tree at that path — a copied binary, say — set
`SourceDirectory` in `worldserver.conf` to point at the checkout, otherwise the
migrations cannot be found and the module refuses to initialise.

Expected in the log:

```
[PlayerbotV2] Schema versions applied so far: N in characters, M in shared (`playerbot`)
[PlayerbotV2] Migration 17: 0 statement(s) to characters, 8 to shared (playerbot)
```

If the shared schema is missing or misnamed, initialisation stops with a message
naming the schema — create it per step 3 or correct `Playerbot.SharedDatabase`.

> Playerbot logging is silent unless you raise it. Add `Logger.playerbot.v2=3`
> to see the lines above.

---

## 6. NOT part of a fresh install: world-database repairs

`src/modules/PlayerbotV2/sql/world/` contains six scripts that are **repairs for
a world database that has drifted from canonical TDB**, not general fixes.
Nothing applies them automatically, and that is deliberate: on a correct TDB
import they would rewrite data that is already right.

Measured against canonical TDB, every one of them restores content TDB already
has:

| script | canonical TDB | the drifted DB they were written for |
|---|---|---|
| `0002` EotS flag (GO 208977, map 566) | 1 | 0 |
| `0003` Twin Peaks flags (227740/227741) | 4 | 2 |
| `0004` Strand of the Ancients (map 607) | 244 gameobjects, 57 creatures | 0 / 0 |
| `0006` immune event-creature factions | — | ~450 rows drifted |

Apply one only if you have verified the same gap in *your* world database.
`0005` is already marked deprecated — its table moved to the shared database.

---

## Notes

- `src/modules/PlayerbotV2/sql/shared/` (0001, 0003, 0004, 0005) predates
  `sql/playerbot_shared/` and is superseded by it: those four tables and their
  rows are already in the schema and data files above. Applying them is
  harmless but unnecessary.
- Migrations `0001`–`0016` target the character database and are left untouched
  by design — they are recorded as applied on every existing realm, so editing
  them would rewrite history that never replays. `0017` is additive and moves
  the four realm-independent tables into the shared schema, copying existing
  rows across.
- Known limitation: the runtime still *reads* those four tables unqualified,
  i.e. from the character database. `0017` establishes and keeps the shared
  copies in step; moving the read paths across is a separate change. Nothing
  regresses meanwhile.

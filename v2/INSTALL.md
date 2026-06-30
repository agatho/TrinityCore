# Playerbot V2 — Installation

End-to-end setup for the Playerbot V2 module on top of a working TrinityCore
server. For deeper detail see `BUILD.md`, `CONFIG.md`, and `SCHEMA.md` in this
directory.

## 1. Prerequisites

A buildable TrinityCore checkout for the target client, plus the standard
TrinityCore dependencies (CMake, a C++20 compiler, MySQL/MariaDB, Boost). If
you can already build and run a stock `worldserver`, you have everything needed.

## 2. Build with the module enabled

Configure CMake with the module flag turned on:

```
cmake <path-to-source> -DBUILD_PLAYERBOT_V2=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --config RelWithDebInfo --target worldserver
```

There is no separate V1 flag — `BUILD_PLAYERBOT_V2` is the only Playerbot option.

When `BUILD_PLAYERBOT_V2` is set, the build copies `playerbot.conf.dist` next to
`worldserver.conf.dist` automatically (see step 4).

## 3. Database setup

The module keeps all of its own data in a **dedicated schema**, separate from
TrinityCore's `auth` / `characters` / `world` databases. Bot characters
themselves live in the normal `characters` database; only bot *metadata* (name
pool, personalities, population targets, talent builds, guild metadata, …) lives
in the playerbot schema.

### 3a. Create the schema

```sql
CREATE DATABASE playerbot DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
GRANT ALL PRIVILEGES ON playerbot.* TO 'trinity'@'localhost';
```

Use whatever name and DB user your server already uses; just make the name match
`Playerbot.SharedDatabase` in step 4.

### 3b. Tables + seed data (automatic)

The numbered migrations in `sql/playerbot_v2/` are applied **automatically at
first boot** by `PlayerbotMigrationMgr`. They are append-only and idempotent,
tracked in the `playerbot_v2_schema_version` table, and include the ~107k-row
random **name pool** (`0015_name_pool.sql`) the fleet draws bot names from.

The runner reads these files from the source tree the server was built from. If
you deploy compiled binaries **without** the source tree, apply them manually in
numeric order instead, e.g.:

```
for f in sql/playerbot_v2/0*.sql; do mysql -u trinity -p playerbot < "$f"; done
```

### 3c. World database fixes (manual)

`src/modules/PlayerbotV2/sql/world/` contains a few targeted fixes to the
**world** database that some Battleground / Strand of the Ancients behavior
relies on. Apply them to your world DB:

```
for f in src/modules/PlayerbotV2/sql/world/0*.sql; do mysql -u trinity -p world < "$f"; done
```

(Substitute your actual world DB name.)

## 4. Configuration

Copy the distributed config next to your `worldserver.conf` and drop the
`.dist` suffix:

```
cp playerbot.conf.dist playerbot.conf
```

The module loads `playerbot.conf` from the same directory as the `worldserver.conf`
the server actually loaded. At minimum review:

- `Playerbot.SharedDatabase` — must equal the schema name from step 3a.
- `Playerbot.Population.TotalTarget` — how many bots to run.
- `Playerbot.AiWorkerThreads`, `Playerbot.TickBudgetMs` — performance tuning.

All keys are documented inline in `playerbot.conf.dist`; see `CONFIG.md` for the
narrative version.

## 5. First run

Start `worldserver` normally. On first boot the migrations apply and the name
pool loads (look for `[PlayerbotV2] Applying migration …` and
`[BotNamePool] Loaded in-memory pool: …` in the log). Bots spawn according to
the population settings; in-game GM commands are under `.playerbot`.

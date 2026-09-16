# CLAUDE.md

Guidance for Claude Code working in this repository.

TrinityCore WoW server emulator (C++20), forked at `github.com/agatho/TrinityCore`, tracking
retail **12.1 (Midnight)**. Beyond upstream it carries several feature lines — PlayerbotV2 (an
AI-driven bot fleet, `src/modules/PlayerbotV2/`), warband, housing, delves, pet battles and others.

**Where guidance lives:**

| File | Scope |
|---|---|
| this file | repo-wide: rules, branch model, build, databases, core architecture |
| `src/modules/PlayerbotV2/CLAUDE.md` | the bot module, on branches that carry it — loaded when working in that subtree |
| `CLAUDE.local.md` | machine-specific paths, this deployment's DB names (untracked) |

Feature-specific working notes (implementation status, phase plans) belong on the **feature branch**
that owns them — in that feature's `doc/` or a scoped `CLAUDE.md` — not in this file, which every
branch shares and every session loads.

---

## Hard constraints

Non-negotiable. Violating one of these wastes work and will be rejected.

- **Never bump map/vmap/mmap versions.** The server must stay a drop-in for stock TrinityCore so
  operators' existing extracted maps keep working. Map-system changes must be *additive* and
  load-compatible at the same version (sidecar files, ignored enum values). Never propose
  re-extraction.
- **Never roll back a binary.** This is a test server; an old build produces stale signal. On a
  crash loop: fix → clean build → deploy the new binary. Use the PDB archive for *symbols*, not
  for running. Crashes are data.
- **No workarounds, no band-aids.** If pathfinding fails, fix pathfinding. If a fix description
  starts with "skip" or "filter out", it is almost certainly a workaround — stop. Disabling a
  feature to dodge a crash, or filtering inputs so a downstream function stops throwing, is the
  wrong direction.
- **No teleport rescues.** A bot in a bad place is a symptom. Fix the movement gate, the routing,
  the population placement, or the data — in that order of suspicion. A bounded, explicitly
  justified teleport (e.g. a charter-founder grace) is fine; a generic "lost bot" teleport is not.
- **No global navmesh tuning to fix a local problem.** Raising `walkableClimb` or lowering
  `minRegionArea` to free one stuck bot changes pathing for every creature on every tile. The fix
  is a targeted off-mesh connection (`offmesh.txt`) or correct rasterization.
- **Quality over speed, always.** The project is unreleased, so refactoring anything is fair game.
  Do not offer "ship now, tune later" paths, and do not ask "fast or careful?" — the answer is
  always careful. This does not license bikeshedding; it means never cut the corner.
- **Never rewrite a source file with a truncating write** (`open(path,"w")`, naive `>`). Use the
  Edit tool. A truncating write that dies mid-stream destroys the file — this cost ~40 days of work
  once, recovered only from a dangling git blob.

---

## Branch model

- **`feature/*` branches are the golden source.** All real work lands there first.
- **`integration/*` branches are disposable merge products** — never a source of truth, never
  cherry-pick *from* one. `integration/12_1_with-bots` (core + bots) and `integration/12_1_all`.
- **`playerbot-v2`** is the public release branch for the bot module and golden source for it.
- **Never force-push `integration/12_1_with-bots`** — it is developed from several machines at once.

To merge forward, use the `merge-forward` skill (`.claude/skills/merge-forward/`): it runs the merge
through `merge-tree`/`commit-tree` so no worktree is mutated and a half-merged tree can never reach
a build.

---

## Build

Configure with `SERVERS=1`, `SCRIPTS=static`, `BUILD_PLAYERBOT_V2=1` (when building the bot module),
config `RelWithDebInfo`. Exact toolchain paths and the wrapper script are in `CLAUDE.local.md`.

```bash
cmake -S . -B build -G <generator> -DSERVERS=1 -DSCRIPTS=static -DBUILD_PLAYERBOT_V2=1
cmake --build build --config RelWithDebInfo --target worldserver
ctest --test-dir build          # Catch2, needs -DBUILD_TESTING=1
```

Key options: `SCRIPTS` (none/static/dynamic), `TOOLS`, `WITH_WARNINGS`, `WITH_COREDEBUG`.

**The configured build tree on the dev machine uses the Visual Studio generator**, and that brings
constraints a generic `cmake --build` does not:

- **Use the cmake bundled with Visual Studio**, not whatever is on `PATH`. A stock cmake 4.1.0-rc1
  cannot instantiate the VS18 generator (`could not create CMAKE_GENERATOR`); the copy inside the
  VS install can. This only bites when something triggers a reconfigure.
- **Adding a new `.cpp` requires a reconfigure** — the source glob is not `CONFIGURE_DEPENDS`.
  Symptom: `LNK2019` on the new file's registration function.
- **`C3859`/`C1076` PCH heap errors** mean parallel-compiler RAM exhaustion (usually because the
  live server is running), not a code error. Lower `CL_MPCount`. A genuine *hang* with no progress
  is the known MSVC stall — that needs a reboot, not flag-fiddling.
- Touching a widely-included core header (e.g. `Maps/Map.h`) recompiles hundreds of TUs.
- **Don't block on builds.** Run them in the background and do the next piece of work meanwhile.

Stage the binary next to the server as `worldserver.exe.new`; **the user deploys it.**

---

## Databases

Four logical databases: **world, characters, auth, hotfixes**. Created via
`sql/create/create_mysql.sql`, base schemas in `sql/base/`, updates in
`sql/updates/{auth,characters,world,hotfixes}/master/` named `YYYY_MM_DD_i_database.sql`. When
changing the `auth` or `characters` schema, update `sql/base/` too.

- **Never assume the actual schema names match those four words.** Read them from the server's
  `WorldDatabaseInfo` / `CharacterDatabaseInfo` in `worldserver.conf` before querying. A machine
  that has hosted several imports accumulates stale, empty schemas with the obvious names, and
  querying the wrong one returns empty results — which reads as "no handler exists" and produces
  confident, wrong conclusions. This has cost a full session. Current names: `CLAUDE.local.md`.
- DB user/password default to `playerbot`/`playerbot`, in the same spirit as stock TrinityCore's
  `trinity`/`trinity`.
- **Prepared statements** for all database access: add the enum to `<Name>Database.h`, register the
  SQL in `<Name>Database.cpp`, then `GetPreparedStatement(ENUM)` + `SetData()`.

---

## Core architecture

**Binaries:** `worldserver` (game world, ports 8085-8086, `worldserver.conf`) and `bnetserver`
(Battle.net auth, port 1119, REST 8081, `bnetserver.conf`). The worldserver also loads
`worldserver.conf.d/*.conf`.

**Source layout (`src/`):**

- `server/game/` — core game logic, ~60 subsystems, the largest component
  - `Handlers/` — packet handlers, one per system (`CharacterHandler`, `CollectionsHandler`, …)
  - `Entities/` — `Player`, `Creature`, `GameObject`, `Item`, `Unit`, `Object`, `SceneObject`
  - `DataStores/` — DB2 loading: structs in `DB2Structure.h`, stores in `DB2Stores.h/.cpp`,
    load metadata in `DB2LoadInfo.h`
  - `Server/Packets/` — packet serialization structs
  - `Server/Protocol/Opcodes.cpp` — CMSG/SMSG opcode registry
  - `Spells/`, `Combat/`, `Movement/`, `AI/`, `Maps/`, `Quests/` — major subsystems
  - `Playerbot/` — the core-side API the bot module drives (`PlayerbotAPI`), hooks, movement
- `server/database/Database/Implementation/` — database layer, prepared statements
- `server/scripts/` — zone/instance/spell scripts by continent
- `server/shared/`, `common/` — networking, crypto, threading, utilities
- `modules/` — out-of-core feature modules (see PlayerbotV2's own `CLAUDE.md`)
- `tools/` — map/vmap/mmap extractors, world editor

**Packet flow:** `Client → CMSG → WorldSession handler → manager/entity → database → SMSG → client`.
Handlers validate input, call into managers, persist via `CharacterDatabaseTransaction`, and send
the response packet.

**Adding DB2 data:** define the struct in `DB2Structure.h` → declare the store `extern` in
`DB2Stores.h` and instantiate in `DB2Stores.cpp` → add load info in `DB2LoadInfo.h` → add hotfix SQL
under `sql/updates/hotfixes/master/`.

**Code style:** `.editorconfig` — 4-space indent, 160-column limit, UTF-8 (Latin-1 for
`.c/.cpp/.h/.hpp`). Follow the
[TrinityCore C++ Development Standards](https://trinitycore.atlassian.net/wiki/spaces/tc/pages/2130103/C+Development+Standards).

---

## Working practices

- **Verify before claiming.** Run the command, read the output, then state the result. Report test
  failures with their output; say plainly what was skipped.
- **Data beats memory.** When a rule of thumb about game mechanics is load-bearing, check it against
  the live database or the client data — expansion-era knowledge goes stale. Armor proficiency, for
  one, moved to level 1 in Mists; acting on the Cataclysm rule put low-level plate wearers in mail.
- When the user says "it works for players", trust it and instrument the real code path rather than
  reasoning from static data that may come from the wrong database.
- `git stash` is shared across worktrees and other sessions may be using it — prefer a WIP commit.
- When implementing a new account-wide or customization system, look for the closest existing one
  first — `Garrison/` and the warband groups are the usual architectural references.

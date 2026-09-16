# CLAUDE.md — TrinityCore + PlayerbotV2

TrinityCore 12.1 fork carrying **PlayerbotV2** (`src/modules/PlayerbotV2/`), a bot fleet that
plays the game as ordinary players: levelling, questing, dungeons, battlegrounds, guilds,
professions, economy.

This file holds what the code and git history do **not** tell you. Everything here has cost a
session to learn at least once.

Machine-specific paths, database names and the local server layout live in `CLAUDE.local.md`
(untracked, loaded automatically when present).

---

## Hard constraints

Non-negotiable. Violating one of these wastes work and will be rejected.

- **Never bump map/vmap/mmap versions.** Playerbot must be a drop-in for stock TrinityCore so
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
- **`playerbot-v2`** is the public release branch for the module and is also golden source for it.
- **Never force-push `integration/12_1_with-bots`** — it is developed from several machines at once.

Merge forward without touching a worktree (also avoids building a half-merged tree):

```bash
git fetch origin <src> <dst>
T=$(git merge-tree --write-tree origin/<dst> <src-sha>)   # rc=0 and a bare SHA = clean merge
git diff --stat origin/<dst> $T                           # confirm only intended files move
M=$(git commit-tree $T -p $(git rev-parse origin/<dst>) -p <src-sha> -F- <<'MSG'
...message...
MSG
)
git push origin $M:refs/heads/<dst>
```

---

## Build

Configured with CMake generator **`Visual Studio 18 2026`**, config `RelWithDebInfo`,
`BUILD_PLAYERBOT_V2=1`. Exact paths and the wrapper script are in `CLAUDE.local.md`.

- **Use the VS-bundled cmake**, not whatever is on `PATH`. A stock cmake 4.1.0-rc1 cannot
  instantiate the VS18 generator (`could not create CMAKE_GENERATOR`); the copy shipped inside the
  Visual Studio install can. This only bites when something triggers a reconfigure.
- **Adding a new `.cpp` requires a cmake reconfigure** — the source glob is not `CONFIGURE_DEPENDS`.
  Symptom: `LNK2019` on the new file's registration function.
- **`C3859`/`C1076` PCH heap errors** mean parallel-compiler RAM exhaustion (usually because the
  live server is running), not a code error. Lower `CL_MPCount`. A genuine *hang* with no progress
  is the known MSVC stall — that needs a reboot, not flag-fiddling.
- Touching a widely-included core header (e.g. `Maps/Map.h`) recompiles hundreds of TUs.
  Module-only edits are fast.
- **Don't block on builds.** Run them in the background and do the next piece of work meanwhile.

Stage the binary next to the server as `worldserver.exe.new`; **the user deploys it.**

---

## Databases

- **Never assume the world database is called `world`.** Read the actual names from the server's
  `WorldDatabaseInfo` / `CharacterDatabaseInfo` in `worldserver.conf` before querying. A machine
  that has hosted several imports accumulates stale, empty world schemas, and querying the wrong
  one returns empty results — which reads as "no handler exists" and produces confident, wrong
  conclusions. This has cost a full session.
- The shared playerbot schema name is **configurable** via `Playerbot.SharedDatabase` (code default
  `playerbot`) because one machine may run several deployments. **Never hardcode it** — qualify
  shared tables as `{shared}.table` through the config key.
- DB user/password default to `playerbot`/`playerbot`, in the same spirit as stock TrinityCore's
  `trinity`/`trinity`.
- Module migrations live in `sql/playerbot_v2/` and are applied by `PlayerbotMigrationMgr`. Write
  plain, self-contained statements: the connection pool hands each statement a different connection,
  so session-scoped SQL (`SET @var`, `PREPARE`) is unsafe across statements.
- **V2 logging is silent unless `Logger.playerbot.v2=3` is set.** The umbrella `Logger.playerbot=1`
  is FATAL-only and hides everything the module emits.

---

## Code layout

| Path | What |
|---|---|
| `src/modules/PlayerbotV2/Bot/` | Per-bot AI: snapshot, states, idle rules, gear, talents |
| `src/modules/PlayerbotV2/Combat/` | Spec rotations (APLs) |
| `src/modules/PlayerbotV2/Fleet/` | Population, setup pipeline, guilds, queue filling |
| `src/modules/PlayerbotV2/Travel/`, `World/` | Routing, travel graph, world metadata |
| `src/modules/PlayerbotV2/Threading/` | Intent queue (lock-free MPSC ring) |
| `src/server/game/Playerbot/` | Core-side API the module drives (`PlayerbotAPI`), hooks, movement |

Architecture in one line: the builder publishes an immutable **snapshot** per bot → **rules** read
the snapshot and emit **intents** → the executor drains intents on the world thread and calls
`PlayerbotAPI`. Rules never touch `Player` directly.

- **Snapshot fields are often declared but never populated.** Before relying on one, grep the
  builder for its assignment — a field reading zero forever is a common and expensive false lead.
- **Do not add new alternatives to `IntentBody` directly.** ~100 variant alternatives already tip
  MSVC into `C1060` heap exhaustion across the spec rotations. Wrap a subsystem's intents in a
  sub-variant plus one wrapper struct and add only the wrapper — as `GuildIntent`, `ChatIntent` and
  `HousingIntent` already do.
- World-thread only: anything mutating `Player`. The snapshot builder may run elsewhere — read-only
  global stores (e.g. `TransmogMgr` maps, populated at load) are safe to read from it.

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

## Known-hard areas

- **Navigation robustness is the release blocker**: bots port and survive everywhere but reconverge
  and complete poorly. It is a *cluster* of routing/advance/cohesion fragilities, not one root cause,
  and it has regressed twice from well-meant local fixes. Changes here need multi-dungeon verification.
- Cross-map travel, elevators, and far-goal `move_to` wedges have a long fix history. Check the
  internal design notes before touching them.

# CLAUDE.md — PlayerbotV2

Scoped guidance for the bot module. The repo-wide rules, branch model and build live in the root
`CLAUDE.md`; machine paths and this deployment's database names in `CLAUDE.local.md`.

PlayerbotV2 is a fleet of AI-controlled players that inhabit the world as ordinary characters:
levelling, questing, dungeons, battlegrounds, arenas, guilds, professions and the economy. It is
built as an out-of-core module (`BUILD_PLAYERBOT_V2=1`) driving a core-side API.

---

## Architecture

The builder publishes an immutable **snapshot** per bot → **rules** read the snapshot and emit
**intents** → the executor drains intents on the world thread and calls `PlayerbotAPI`.

**Rules never touch `Player` directly.** That separation is what keeps bot logic off the world
thread, and it is the first thing to check when something misbehaves in a way that smells like a
race.

| Path | What |
|---|---|
| `Bot/` | Per-bot AI: snapshot builder, states, idle rules, gear, talents, diagnostics |
| `Bot/States/Rules/` | Registered idle rules (priority-ordered; `/whyidle` explains a decision) |
| `Combat/` | Spec rotations (APLs), one per (class, spec) |
| `Fleet/` | Population shaping, setup pipeline, guilds, queue filling, gear backfill |
| `Travel/`, `World/` | Routing, unified travel graph, world metadata |
| `Group/` | Group and party coordination |
| `Threading/` | Intent queue — lock-free Vyukov MPSC ring, one per bot |
| `Persistence/` | Schema migrations (`PlayerbotMigrationMgr`) |
| `Session/` | Headless bot login sessions |
| `../../server/game/Playerbot/` | Core side: `PlayerbotAPI`, hooks, movement helpers |

Internal design notes, audits and handovers live in `docs/` (untracked — local only). **Check there
before starting on any subsystem**; most have a written history.

---

## Traps

Each of these has cost at least one session.

- **Snapshot fields are often declared but never populated.** Before relying on one, grep the
  builder for its assignment. A field that reads zero forever is a common and expensive false lead.
- **Do not add new alternatives to `IntentBody` directly.** ~100 variant alternatives already tip
  MSVC into `C1060` heap exhaustion across the spec rotations, and the cost compounds with every
  rotation TU that includes the emitter. Wrap a subsystem's intents in a sub-variant plus one
  wrapper struct and add only the wrapper — as `GuildIntent`, `ChatIntent` and `HousingIntent` do.
- **World thread only:** anything mutating `Player`. The snapshot builder may run elsewhere;
  read-only global stores populated at load (e.g. `TransmogMgr`'s maps) are safe to read from it.
- **Migrations in `sql/playerbot_v2/` must be plain, self-contained statements.** The connection
  pool hands each statement a different connection, so session-scoped SQL (`SET @var`, `PREPARE`)
  is unsafe across statements.
- **The shared playerbot schema name is configurable** (`Playerbot.SharedDatabase`, code default
  `playerbot`) because one machine may run several deployments. Never hardcode it — qualify shared
  tables as `{shared}.table` through the config key.
- **The module logs nothing unless `Logger.playerbot.v2=3` is set.** The umbrella
  `Logger.playerbot=1` is FATAL-only. Channel matching is hierarchical, so `playerbot.v2.foo` falls
  back to `playerbot.v2`.
- **Adding a new rule file needs a cmake reconfigure** before its `Register*Rules` symbol resolves.

---

## Known-hard areas

- **Navigation robustness is the release blocker.** Bots port and survive everywhere but reconverge
  and complete poorly. It is a *cluster* of routing, advance and cohesion fragilities rather than
  one root cause, and it has regressed twice from well-meant local fixes. Changes here need
  multi-dungeon verification, not a single passing run.
- **Cross-map travel, elevators and far-goal `move_to` wedges** have a long fix history. A single
  long `move_to` toward a distant or elevated point climbs ledges and stalls without ever reporting
  a path block; chunked movement with bearing deflection is the established idiom.
- **Gearing** is level-scaled: rank candidates by effective item level for the wearer, not base item
  level, and never let a generated piece displace real loot across a quality boundary.

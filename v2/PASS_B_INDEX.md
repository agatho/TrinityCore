# Playerbot V2 — Pass B Index

**Purpose**: Pass B is the *implementable* design. Each document below is the source of truth for one concern; the implementation can be derived mechanically from them.

| # | Document | Concern | Status |
|---|---|---|---|
| 1 | [`MODULE_LAYOUT.md`](./MODULE_LAYOUT.md) | Full directory tree, every file's purpose, CMake shape | Pass B |
| 2 | [`CONTRACTS.md`](./CONTRACTS.md) | Concrete class signatures for all major types | Pass B |
| 3 | [`API.md`](./API.md) | `PlayerbotAPI.h` — every accessor, command, hook | Pass B |
| 4 | [`SCHEMA.md`](./SCHEMA.md) | Database schema and migration plan | Pass B |
| 5 | [`BUILD.md`](./BUILD.md) | Build flag, CMake wiring, compile-time checks | Pass B |
| 6 | [`CONFIG.md`](./CONFIG.md) | `playerbot.conf` schema, every key documented | Pass B |
| 7 | [`FEATURE_MATRIX.md`](./FEATURE_MATRIX.md) | Every feature → architectural component mapping | Pass B |
| — | [`ARCHITECTURE.md`](./ARCHITECTURE.md) | Pass A patterns — load-bearing for Pass B | Pass A |
| — | [`REQUIREMENTS.md`](./REQUIREMENTS.md) | CORE constraints + invariants | Phase 0 |
| — | [`FEATURES.md`](./FEATURES.md) | Player-mirror feature surface | Phase 0 |
| — | [`SYSTEM_FEATURES.md`](./SYSTEM_FEATURES.md) | Fleet feature surface | Phase 0 |

## Reading order for an implementer

1. `REQUIREMENTS.md` §1 — what we're building, the seven goals.
2. `ARCHITECTURE.md` §§1–9 — the patterns and *why*.
3. `MODULE_LAYOUT.md` — where every file lives.
4. `CONTRACTS.md` — what every class looks like.
5. `API.md` — what TrinityCore exposes.
6. `SCHEMA.md` + `BUILD.md` + `CONFIG.md` — the rest of the plumbing.
7. `FEATURE_MATRIX.md` — to verify nothing is missing.

## Pass B exit criteria

Pass B is done when:
- Every Pass A §14 open question is answered (in the relevant Pass B doc).
- Every feature in `FEATURES.md` and `SYSTEM_FEATURES.md` has a row in `FEATURE_MATRIX.md` pointing to a component in `MODULE_LAYOUT.md` and `CONTRACTS.md`.
- An implementer reading these docs can write code without making architectural decisions.

## Iteration 2 — threading + types + first end-to-end loop (landed 2026-05-01)

The bootstrap commit now extends with the foundational threading layer, the
core data types, and a complete world-tick → snapshot publish → AI worker tick
loop. Bot logic is still empty (Idle is a no-op), but the wiring is end-to-end:
on player login, a bot character registers; every world tick its snapshot is
published; the worker pool ticks BotAI; `Idle` runs and emits no intents.

Files added in this iteration:
- `Bot/BotTypes.h` — IDs, ActivityTier, BotState, Role, DispelType
- `Bot/BotPersonality.{h,cpp}` — personality enums + struct
- `Bot/BotRng.h` — splitmix64 per-bot RNG
- `Bot/BotActivityTier.{h,cpp}` — tier classification + tick periods
- `Bot/BotSnapshot.{h,cpp}` + `BotSnapshotView.{cpp,h}` — immutable snapshot + facade
- `Bot/BotSnapshotBuilder.{h,cpp}` — Player* → BotSnapshot
- `Bot/BotIntent.{h,cpp}` — typed intent variant (35 variants in V1.0 set)
- `Bot/BotIntentEmitter.{h,cpp}` — push handle with convenience methods
- `Bot/BotEventInbox.{h,cpp}` — single-producer/single-consumer event ring
- `Bot/BotAI.{h,cpp}` — top-level state machine + dispatch
- `Bot/BotRegistry.{h,cpp}` — owns BotAI/IntentQueue/EventInbox per bot
- `Bot/States/StateBase.h` — dispatch interface for all primary + cross-cutting states
- `Bot/States/State_Idle.cpp` — first real state (mostly no-op for now)
- `Bot/States/State_Stubs.cpp` — empty bodies for the other 13 states
- `Group/GroupSnapshot.{h,cpp}` — group view + role resolution
- `Threading/IntentQueue.{h,cpp}` — Vyukov-style lock-free MPSC
- `Threading/SnapshotPublisher.{h,cpp}` — atomic shared_ptr per-bot slots
- `Threading/AiWorkerPool.{h,cpp}` — N worker threads consuming bot tick tasks
- `Threading/FleetThread.{h,cpp}` — single thread for fleet-layer work
- `Threading/TickScheduler.{h,cpp}` — tier-aware bot tick scheduler
- `Services.{h,cpp}` — singleton accessors + lifecycle

Hook insertion sites added to core (3 of the 30 budgeted in MODULE_LAYOUT.md):
- `src/server/game/Handlers/CharacterHandler.cpp` — `OnPlayerLogin`
- `src/server/game/Server/WorldSession.cpp` — `OnPlayerLogout`
- `src/server/game/World/World.cpp` — wired `Module::OnWorldUpdate` into the world tick loop

Lifecycle wiring added to worldserver:
- `src/server/worldserver/Main.cpp` — `Module::Init()` after `SetInitialWorldSettings`, `Module::Shutdown()` before halt

CMake refinement:
- `src/server/game/CMakeLists.txt` — `Playerbot/` include path is now always public (header self-selects real vs inline-empty bodies)

## Iteration 3 — migrations, intent execution, first real API bodies (landed 2026-05-01)

Intents emitted by AI workers are now drained on the world thread and dispatched
to `PlayerbotAPI`. Three commands have real bodies: `cast_spell`, `move_to`,
`hearth`. The remaining ~32 intent variants no-op via the visitor's catch-all
template. Schema migrations now run at init via `PlayerbotMigrationMgr`.

Files added:
- `Persistence/PlayerbotMigrationMgr.{h,cpp}` — runs `sql/playerbot_v2/0001_init.sql`
- `Bot/BotIntentExecutor.cpp` — variant visitor for intent → API dispatch

Files modified:
- `src/server/game/Playerbot/PlayerbotAPI.cpp` — real bodies for cast_spell, move_to, hearth
- `PlayerbotV2.cpp` — `Module::Init` now runs migrations and refuses to init on failure; `OnWorldUpdate` drains intents per tick

## Iteration 4 — config plumbing (landed 2026-05-01)

Files added:
- `Util/ConfigReader.{h,cpp}` — typed accessors over TrinityCore's `sConfigMgr`,
  loading `playerbot.conf` at module init. AI worker count now driven by config.

Files modified:
- `Services.{h,cpp}` — owns and exposes `ConfigReader`; `AiWorkerPool` reads
  thread count from config at startup.

Remaining work toward V1.0 acceptance criteria:
- ~32 more intent variants need real `PlayerbotAPI` bodies (vendor, loot, quest accept/turn-in, group ops, AH, mail, talents, pets, housing)
- `Group/GroupSnapshotBuilder` (currently AI sees an empty group view)
- The remaining ≤27 hook insertion sites (combat events, group changes, quest progress, instance enter/exit, BG progress, encounter phase, mail, AH, LFG, housing purchase)
- `Fleet/PopulationManager` + `BotLifecycleManager` so bots actually exist (right now any logged-in player gets a registry entry)
- One real `Combat/Apl/Apl_Hunter_BeastMastery.cpp` (validate the rotation pipeline end-to-end with a single class)
- `Diagnostics/PerfCounters` + `BotInspector` (.playerbot inspect command)

## Bootstrap commit (landed 2026-05-01)

The minimum scaffolding to validate Pass B's wiring is now on disk. Behavior unchanged when `BUILD_PLAYERBOT_V2=OFF` (default).

Files created:
- `cmake/options.cmake` — added `BUILD_PLAYERBOT_V2` option (default OFF)
- `src/modules/CMakeLists.txt` — added conditional `add_subdirectory(PlayerbotV2)`
- `src/modules/PlayerbotV2/CMakeLists.txt` — module build with strict warnings, C++20, PCH
- `src/modules/PlayerbotV2/PCH.h` — precompiled header
- `src/modules/PlayerbotV2/PlayerbotV2.{h,cpp}` — module entry + 13 hook handler stubs
- `src/modules/PlayerbotV2/conf/playerbot.conf.dist` — minimal config sample
- `src/server/game/Playerbot/PlayerbotAPI.{h,cpp}` — facade skeleton with 9 representative methods (full surface lands incrementally)
- `src/server/game/Playerbot/PlayerbotHooks.{h,cpp}` — conditional hook surface (real when V2 on, inline-empty when off)
- `src/server/game/CMakeLists.txt` — V2 compile definition and include paths when ON
- `src/server/worldserver/CMakeLists.txt` — link `playerbot-v2` when ON
- `sql/playerbot_v2/0001_init.sql` — initial schema migration (schema_version + 5 V1.0 tables)
- `sql/playerbot_v2/README.md` — migration discipline

What's NOT yet present (deliberately):
- Hook *insertion sites* in core game code (Player.cpp, Unit.cpp, etc.) — added per-feature as subsystems land
- Friendship grants in `Player.h`, `Unit.h`, `Map.h`, `Group.h`, `Spell.h`, `Item.h`, `Quest.h` — added when the API surface that needs them lands
- Any of the 196 module source files beyond the entry point — added per `MODULE_LAYOUT.md` order
- Threading infrastructure (`SnapshotPublisher`, `IntentQueue`, `AiWorkerPool`) — first real subsystem to land
- Any `.playerbot` GM commands

Build verification (recommended next step before further code):
```
cmake -DBUILD_PLAYERBOT_V2=ON -DBUILD_PLAYERBOT=OFF -B build_v2
cmake --build build_v2 --target worldserver --config RelWithDebInfo
```
Should produce a worldserver binary identical in behavior to one built with V2 off (since all V2 hook handlers are stubs). Confirms wiring is correct before any subsystem implementation.

After bootstrap is verified, the implementation order is:
1. **Threading** (`SnapshotPublisher`, `IntentQueue`, `AiWorkerPool`) — foundational, everything depends on it
2. **PlayerbotAPI** completion — read accessors first, action commands second
3. **Persistence** (`PlayerbotMigrationMgr`, basic load/save)
4. **Bot/States/State_Idle** — simplest state, validates the dispatch shape
5. **Bot/States/State_LoggingIn** + **Fleet/BotLifecycleManager** — first end-to-end scenario: spawn an empty bot
6. **Movement** + **Bot/States/State_Travelling** — first observable behavior
7. ... continuing per `MODULE_LAYOUT.md` toward V1.0 acceptance criteria

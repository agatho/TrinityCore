# Playerbot V2 — Architecture (Pass A — medium fidelity)

**Status**: Draft for review. Pass A — names the patterns, justifies each against the feature surface, identifies open questions. Pass B (high-fidelity contracts, full directory tree, schema, CMake) follows after this is reviewed and amended.
**Last updated**: 2026-05-01
**Inputs**: `REQUIREMENTS.md` (CORE constraints + invariants), `FEATURES.md` (player-mirror surface), `SYSTEM_FEATURES.md` (fleet surface)
**Architectural style**: Boring. Every pattern selected here has decades of evidence. No novelty without earned justification.

---

## 0. Architectural principles (load-bearing — every section below honors these)

1. **Bots use systems as players do** (`REQUIREMENTS.md` §1.1 #7). Every in-world action goes through the same code paths a player would trigger. No backdoors.
2. **Single-writer for game state**. The world thread owns all mutations to `Player`/`Unit`/`Item`/`Map`. AI runs on read-only snapshots.
3. **Deadlock-free by construction, not convention**. The threading topology must make deadlock structurally impossible — not "we'll be careful."
4. **Architecture follows requirements**. Each pattern below is justified by *what the catalogs demand*, not by elegance.
5. **Crash isolation**. One bot's bug cannot cascade. Per-bot try/catch is the only acceptable pattern; no shared exception state.
6. **Boring scales, novel breaks**. State machines, action priority lists, scripted encounters, role functions, utility scoring. If a pattern needs explaining beyond a paragraph, it's wrong.

---

## 1. Threading model & data flow

### 1.1 The model
Three thread classes, one direction of data flow:

```
                          ┌───────────────────────────────────────┐
                          │            WORLD THREAD                │
                          │  (TrinityCore's existing main loop)    │
                          │                                        │
                          │  At start of each world tick:          │
                          │    1. publish per-bot snapshots        │
                          │    2. drain intent queues, execute     │
                          │       intents through PlayerbotAPI     │
                          │    3. fire hooks → BotEventInbox       │
                          └───────────────────────────────────────┘
                                          │       ▲
                  immutable snapshot      │       │   typed intents
                  pointer (atomic swap)   ▼       │   (lock-free MPSC)
                          ┌───────────────────────────────────────┐
                          │          AI WORKER POOL                │
                          │  (N threads, default min(8, hwc))      │
                          │                                        │
                          │  For each bot scheduled this tick:     │
                          │    1. read latest snapshot             │
                          │    2. read latest event inbox          │
                          │    3. run BotAI::tick(snapshot, rng)   │
                          │       → produce intents                │
                          │    4. push intents to bot's queue      │
                          └───────────────────────────────────────┘
                                          │
                  read-only snapshot      │
                          ▼
                          ┌───────────────────────────────────────┐
                          │          FLEET THREAD (1)              │
                          │                                        │
                          │  Periodic (1Hz default):               │
                          │    - Population manager decisions      │
                          │    - LFG mediator                      │
                          │    - BG filler                         │
                          │    - Neighborhood populator scheduler  │
                          │                                        │
                          │  Emits *control intents* into bot      │
                          │  queues (e.g., "queue for BG",         │
                          │  "buy plot in neighborhood X")         │
                          └───────────────────────────────────────┘
```

**Why this works (and why deadlocks are structurally impossible)**:
- AI workers and fleet thread *only read* shared state, via immutable snapshots replaced atomically.
- World thread *only writes* through PlayerbotAPI, never holds a lock that AI workers wait on.
- Intent queues are MPSC lock-free (one consumer = world thread; many producers = AI workers + fleet thread).
- There is no path where thread A holds resource X waiting for resource Y while thread B holds Y waiting for X. Wait-for graph has no cycle.

### 1.2 The snapshot
Per-bot, immutable, ref-counted, replaced atomically (single pointer `std::atomic<std::shared_ptr<BotSnapshot const>>` swap on world tick).

A snapshot contains exactly what the AI needs — derived from `FEATURES.md`'s state surface:
- Identity, stats, position, combat state, auras, cooldowns, inventory summary, equipment summary, known spells, group summary, quest log summary, movement state.
- Plus pointers (raw, valid until next snapshot) into TrinityCore data the AI may dereference *read-only* (e.g., `Map const*`, nearest enemies' minimal projections).

**Snapshot size budget**: ≤ 8 KB per bot. At 5K bots, ≈ 40 MB total snapshot footprint per tick — acceptable.

### 1.3 The intent
A typed message representing one game action the AI wants performed. Examples:

```cpp
struct CastSpellIntent { uint32 spell_id; ObjectGuid target; };
struct MoveToIntent    { float x, y, z; };
struct LootIntent      { ObjectGuid corpse; uint8 item_index; };
struct BuyPlotIntent   { uint32 neighborhood_id; uint32 plot_id; };
struct QueueLfgIntent  { LfgQueueId queue; LfgRole role; };
```

Intents are POD structs in a `std::variant` (or tagged union). Each maps 1:1 to a `PlayerbotAPI` action command.

**Rule**: an intent is the only way for AI code to influence game state. AI code that calls `Player::*` directly is a build error (enforced by `friend`-only access pattern).

### 1.4 Tick scheduling
- AI workers process bots at a rate determined by bot's *activity tier*:
  - **Combat tier**: ≥10 Hz (every world tick if needed)
  - **Active tier**: 5 Hz (questing, travelling, etc.)
  - **Idle tier**: 1 Hz (parked at inn, waiting on group)
  - **Hibernate tier**: 0.05 Hz (long-idle, just maintaining presence)
- Tier transitions driven by snapshot state. Promoted to combat tier on `OnDamageTaken`/`OnDamageDealt`/`in_combat=true`.
- Tick budget enforcement: if AI worker pool falls behind, lowest-tier bots skip ticks (graceful degradation).

### 1.5 Why not pure world-thread?
At 5K bots, even cheap per-bot decision-making at 200µs per tick = 1 second of CPU, blowing the world tick budget. AI workers are necessary.

### 1.6 Why not actor model / fibers / coroutines?
- Actors require a message-bus framework — adds 1000+ LOC of infrastructure for no expressivity gain over snapshot+intent.
- Fibers require careful stack management; harder to reason about under crash isolation requirement.
- Coroutines (C++20) are fine *inside* an AI worker (e.g., for multi-step decisions) but not as the inter-thread mechanism.

The model above is the simplest topology that satisfies CORE constraints. Locked.

---

## 2. Top-level decomposition

Three layers. Each has clear responsibility, clear inputs/outputs, no upward dependencies.

### 2.1 Per-bot layer (`src/modules/PlayerbotV2/Bot/`)
Owns: one bot's decision-making.

Components:
- `BotAI` — the top-level state machine and tick entry point.
- `BotSnapshotView` — typed, ergonomic facade over `BotSnapshot const&`.
- `BotIntentEmitter` — typed methods that push intents into the per-bot queue.
- `BotRng` — per-bot deterministic RNG (seeded from bot ID).
- `BotEventInbox` — per-bot event ring buffer (256 entries) consumed each tick.

Per-bot layer **does not know** about other bots or fleet. It receives a snapshot (its own), a slice of group state (read-only), and emits intents. Period.

### 2.2 Group/coordination layer (`src/modules/PlayerbotV2/Group/`)
Owns: shared state across the bots in a single group.

Components:
- `GroupSnapshot` — read-only summary of all members, threat, marks, ready-check state, loot rules.
- `GroupRoleResolver` — pure function `(member, group) → Role` (Tank/Healer/DPS/Off-tank).
- `GroupTacticsPolicy` — per-content tactics (e.g., dungeon vs raid vs delve vs BG vs world-quest), referenced by per-bot AI.

Each AI worker reads its bot's group snapshot from the same atomically-published snapshot bundle — no cross-bot synchronization.

### 2.3 Fleet layer (`src/modules/PlayerbotV2/Fleet/`)
Owns: ecosystem-level decisions (`SYSTEM_FEATURES.md` surface).

Components:
- `PopulationManager` — sets/maintains target bot count by faction/level/class/role/zone.
- `LfgMediator` — when player queues, selects bots to fill remaining roles.
- `BgFiller` — fills both factions for BG queues per `SYSTEM_FEATURES.md` §5.3.
- `NeighborhoodPopulator` — schedules bot housing acquisition + decoration to meet density quotas, **routing all actions through the per-bot AI** (no shortcut writes).
- `BotLifecycleManager` — spawn, despawn, mid-content protection, persistence.
- `AdminCommandHandler` — `.playerbot` GM commands.

Fleet layer **emits control intents** into bot intent queues (`QueueLfgIntent`, `BuyPlotIntent`, `JoinNeighborhoodIntent`). It does not bypass the per-bot AI; it *requests* and the AI responds.

This is the architectural expression of `REQUIREMENTS.md` §1.1 #7: even fleet-level decisions go through the player-equivalent path.

### 2.4 Why three layers (and not two or four)?
- Two layers (bot + system) collapses group coordination into either, producing the V1 entanglement.
- Four layers (bot + role + group + fleet) over-decomposes; role lives inside group naturally.
- Three is the minimum that keeps cross-bot logic from leaking into per-bot AI. Validated against V1's failure mode (cross-bot logic in `BehaviorManager` calling `BehaviorPriorityManager` calling per-bot strategies).

---

## 3. Per-bot state machine

Top-level states (the bot is in exactly one at any moment):

```
LoggingIn  →  Idle  →  Travelling  →  Questing  →  InCombat  →  Looting
                                                     ↓
                                                    Dead → Travelling (corpse run)
                                                     ↓
                                                  Resurrected → Idle

Cross-cutting (re-entrant, can interleave with above):
  InGroup, InInstance, AtVendor, AtMailbox, AtAuctionHouse, Decorating

Lifecycle:
  LoggingIn, LoggingOut
```

**Rationale**: 12 named states, each with a single dispatch function. State transitions are explicit and logged. No hidden state machines inside states. A new feature (e.g., delves, housing) gets a new state if it has its own loop, otherwise hangs off `Questing`/`InGroup`.

**Why state machine, not behavior tree**:
- The bot is physically in one mode at a time. A state machine encodes that directly.
- State transitions are the dominant complexity (entering combat, dying, group invite mid-quest); state machines model transitions as first-class. Behavior trees model them via decorators with hidden semantics.
- State machine is debuggable as one enum + one function per state. A behavior tree of 12 states is an N-deep tree of nodes scattered across files — V1 demonstrated this empirically.

**Inside each state**: small, flat dispatch. No hierarchy. If `Questing` grows past ~300 lines, it's split by quest *type* (kill/gather/escort), not by abstraction layer.

---

## 4. Combat rotation — Action Priority List (APL)

One APL per (class, spec) pair. Identical encoding shape to SimulationCraft.

Example — Beast Mastery Hunter (sketch, not final):
```
1. Use Hero/Lust if cooldowns aligned and not yet used
2. Tranquilizing Shot if dispellable enrage on target
3. Counter Shot if enemy casting interruptible spell
4. Kill Shot if target.hp_pct < 20
5. Bestial Wrath if cooldown ready
6. Barbed Shot if pet.frenzy.remains < 1.5 OR charges = max
7. Kill Command if focus >= 30
8. Cobra Shot if focus > 50
9. Auto Shot (always last)
```

**Encoding**: a `std::array<RotationRule, N>` per spec, evaluated top-to-bottom each tick. Each rule is `{ predicate(snapshot) → bool, intent(snapshot) → CastSpellIntent }`.

**Why APL, not BT or utility AI**:
- APL is empirically optimal for WoW rotations (validated by 15 years of SimC theorycrafting).
- APL is 1:1 readable as theorycrafter notes — class designers can author them.
- APL is trivially testable (snapshot in, intent out, no hidden state).
- BT and utility AI for rotation are isomorphic to APL with extra ceremony; V1 had both, neither helped.

**Where APL is NOT used**: target *selection* (covered separately — see §6), encounter mechanics (see §5), goal selection (see §6). APL only picks the next ability given a target and intent to act.

---

## 5. Encounter mechanics — Scripted state machine + reactive handlers

Per encounter (boss, tricky trash pull, BG objective phase): one `EncounterScript` class. Lives in `Combat/Encounters/`.

Skeleton:
```cpp
class EncounterScript {
public:
    virtual void OnSnapshot(BotSnapshotView const&, IntentEmitter&) = 0;
    virtual void OnEvent(BotEvent const&, IntentEmitter&) = 0;
    virtual EncounterPhase phase() const = 0;
};
```

Example — A Highmaul Kargath-style encounter (illustrative):
```cpp
class KargathEncounter : public EncounterScript {
    Phase phase = Phase::Arena;

    void OnSnapshot(SnapshotView const& s, IntentEmitter& e) override {
        if (s.boss.hp_pct < 30 && phase == Phase::Arena) phase = Phase::Pillar;
        switch (phase) {
            case Phase::Arena:  handleArena(s, e);  break;
            case Phase::Pillar: handlePillar(s, e); break;
        }
    }

    void OnEvent(BotEvent const& ev, IntentEmitter& e) override {
        if (ev.is_aoe_warning("Mauling Brew", 2000ms)) e.dodge_to_safe_spot();
        if (ev.is_cast_start("Chain Hurl"))            e.move_to_pillar();
    }
};
```

Each script is ≈ 50–150 lines. One file per encounter. Registered at startup in `EncounterRegistry::register<KargathEncounter>(boss_npc_id)`.

**When in combat with a registered encounter NPC, the active encounter script runs *alongside* the rotation APL**: it owns positioning, mechanic dodges, and tactical intents; the APL owns ability casts. They cooperate via the IntentEmitter (encounter script can override target, force movement, suppress casts during cinematics).

**Why scripted state machine, not behavior tree**:
- Encounter mechanics are temporal and phase-based, not hierarchical. State machine encodes phases natively.
- Each boss is its own bespoke logic; reusable subtree composition (BT's claimed strength) doesn't apply.
- 50–150 lines per encounter, debuggable as one file per boss.

**Coverage strategy**: not every boss needs a script for V1.0. A `DefaultEncounterScript` handles "stand at expected position, follow targeting, react to telegraphed AoE generically" for un-scripted content. Real scripts added incrementally for raids/M+ where mechanics matter.

---

## 6. Goal selection — Utility AI

Used when the bot must choose *what to do* among multiple options. Examples:
- BG: which objective to pursue (defend flag, attack node, kill priority target, escort payload)
- World quest pickups: which world quest to do
- Endgame loop: dungeon vs delve vs raid vs profession vs housing tonight
- Target selection in combat: among visible enemies, which to attack/dispel/CC

Each goal has a `score(snapshot, bot_personality, content_state) → float`. The bot picks max-score each evaluation cycle (with hysteresis to prevent thrash).

Example — BG objective scoring (Arathi-style):
```
score(DefendOurNode)   = base + (node.contested ? 60 : 0) + (allies_near_node * -5)
score(AttackEnemyNode) = base + (node_capturable ? 50 : 0) - (defenders * 8)
score(KillEnemy)       = base + (target.hp_pct < 30 ? 25 : 0) + (target_priority_class * 10)
score(GuardGY)         = base + (gy.contested ? 40 : 0)
```

**Why utility AI here, not state machine or BT**:
- Choice space is wide and continuously rebalancing — utility scoring expresses tradeoffs natively.
- Number of options is bounded (≤ 10 per decision) — scoring is fast.
- Tuning is by-eye via score weights — no recompile to rebalance.

**Where utility AI is NOT used**: rotation (APL), encounter mechanics (scripts), state machine (it's the substrate).

---

## 7. Group coordination

Per group: one `GroupSnapshot` published alongside per-bot snapshots each world tick. Contains:
- Member roles, HP, threat, position, casting status
- Loot method, marks, ready-check state
- Group leader, assist flags
- Active content (overworld, dungeon, raid, BG)

Each bot's AI reads its group snapshot **read-only** and acts on it.

**Role-specific functions**:
```cpp
namespace GroupTactics {
    void TankUpdate (SnapshotView, GroupSnapshot, IntentEmitter);
    void HealerUpdate(SnapshotView, GroupSnapshot, IntentEmitter);
    void DpsUpdate  (SnapshotView, GroupSnapshot, IntentEmitter);
}
```

Each function is pure: snapshot in, intents out, no state. The bot's AI dispatches to one based on `GroupRoleResolver`.

This is the architectural answer to V1's `BehaviorManager` calling `BehaviorPriorityManager` calling per-strategy. Three pure functions replace a registry.

---

## 8. Movement

Movement intents flow from AI → world thread, executed via `PlayerbotAPI::move_to(...)`, `move_path(...)`, `mount(...)`, etc. The actual pathfinding is **TrinityCore's MMaps**, called on the world thread from the API. AI decides *where*; the engine decides *how*.

**No bot-specific pathfinding code exists.** V1 had multiple competing pathfinders; V2 uses the engine's. Period.

**Movement-affecting effects** (roots, snares, knockbacks, fear) are tracked in the snapshot's aura list; AI reacts via intents (cleanse, trinket, alternate path).

**Mount logic**: a small table of "mount appropriate for this map/zone/level/altitude" → emitting `MountIntent(id)`. Same as a player macro.

**Stuck detection**: tick-over-tick position delta. If ε for N ticks while wanting to move, emit `JumpIntent`. If still stuck, hearth (per `FEATURES.md` §3.6).

---

## 9. Fleet layer details

### 9.1 PopulationManager
Inputs: target population curves (configured), real-player count (live), current bot population (live).
Output: scheduled spawn/despawn requests routed to `BotLifecycleManager`.
Tick rate: 1 Hz.
Logic: bog-standard target-tracking with hysteresis. ~200 LOC total.

### 9.2 LfgMediator
On `OnPlayerLfgQueue` hook: identifies role gaps in player's group, selects bot candidates from pool (level-appropriate, gear-appropriate, role-appropriate, not-already-busy), emits `QueueLfgIntent` to selected bots.
Each bot then queues *as a player would* via `PlayerbotAPI::lfg_queue(...)` — the LFG system itself merges them into the player's group through normal matchmaking.

**Critical**: LfgMediator does not direct-inject bots into the player's group. It just makes them queue. The LFG system does the matching. (`REQUIREMENTS.md` §1.1 #7 honored.)

### 9.3 BgFiller
Watches BG queues. When real-player BG queue is filling but short of pop, recruits idle bots to queue for that BG (both factions, balanced). Same player-equivalent path: bots queue, the BG matchmaker pairs them.

### 9.4 NeighborhoodPopulator
Reads neighborhood occupancy from world state. For under-populated public neighborhoods, identifies bots that *would naturally be ready* to buy a plot (sufficient gold, level/quest/currency prerequisites met), and sends a `ConsiderBuyingPlotIntent` (a *suggestion*, not a command). The bot's per-bot AI evaluates it against personality (does this bot *want* a house?), available gold reserve, current activity, and either acts on it or not.

This is the strongest expression of `REQUIREMENTS.md` §1.1 #7: even fleet-level "we need more houses bought" goes through the bot's normal decision path.

### 9.5 BotLifecycleManager
Spawn (JIT or warm pool), despawn (with mid-content protection), persistence (via TrinityCore character DB). Mid-content protection is a hard gate — `should_despawn()` returns false if `bot.in_combat || bot.in_instance || bot.in_group_with_player`.

### 9.6 AdminCommandHandler
`.playerbot spawn|despawn|list|inspect|kick|pause|resume|reload|stats|fill|cap|setlevel|setspec|wipe`.
Each maps to an internal function that *requests* lifecycle actions; commands respect the same mid-content protections.

---

## 10. PlayerbotAPI surface

The single allowed core modification (`REQUIREMENTS.md` §3). Final contents derived mechanically from feature categories — every feature in `FEATURES.md`/`SYSTEM_FEATURES.md` maps to either:
- A read accessor on the snapshot (a value the AI needs to see), or
- An action command (an intent the AI emits, executed on world thread), or
- A hook (an event the AI subscribes to via the inbox).

Pass B will produce the actual `.h` file with full signatures. Pass A guarantee: **the API is comprehensive enough that V2 code never includes `Player.h` directly**.

**Friendship grant**: `Player`, `Group`, `Map`, `Item`, `Spell`, `Quest` declare `friend class Playerbot::API;` (one type) so the API can read private state without copy-pasting accessor code into the module.

**Hooks list** (locked in scope, exact list derived from feature surface):
- Lifecycle: login, logout, level-up, death, resurrect, spec change, race/zone change
- Combat: damage dealt/taken, heal received, aura applied/removed, cast start/success/failed/interrupted
- World: loot, quest state change, group member joined/left, trade requested, whisper received
- Content: instance enter/exit, BG enter/exit/objective progress, encounter phase change
- Economy: AH expired/sold, mail received

---

## 11. Persistence

Two-store split:
- **TrinityCore's existing `characters` DB** owns the bot character (level, gear, spells, gold, inventory, position, quest log, etc.). Bots are real characters; we don't fork the character schema.
- **Module-specific tables `playerbot_*`** own *only* what TrinityCore doesn't already store: bot personality, skill tier, activity preferences, neighborhood preference, per-bot RNG seed, current activity tier.

Migrations live in `sql/playerbot_v2/`. Schema lock-down rule: every column is justified in a comment by which feature requires it. Removing a feature should remove its column.

**No mirror tables.** V1 had `playerbot_state` shadowing `characters` rows. V2 forbids this — the snapshot mechanism replaces it.

---

## 12. Module structure (placeholder — Pass B finalizes)

```
src/modules/PlayerbotV2/
├── CMakeLists.txt
├── PlayerbotV2.cpp                  # entry point, subscribes hooks, drives ticks
├── PlayerbotV2.h
├── Bot/                             # Per-bot layer
│   ├── BotAI.{cpp,h}                # state machine + tick
│   ├── BotSnapshot.{cpp,h}          # snapshot type + builder
│   ├── BotIntent.{cpp,h}            # intent variants + queue
│   ├── BotRng.{cpp,h}
│   ├── BotEventInbox.{cpp,h}
│   └── States/                      # one file per top-level state
│       ├── Idle.cpp
│       ├── Questing.cpp
│       ├── InCombat.cpp
│       └── ...
├── Combat/
│   ├── Apl/                         # one file per (class, spec)
│   │   ├── Hunter_BeastMastery.cpp
│   │   └── ...
│   ├── Encounters/                  # one file per scripted encounter
│   │   ├── DefaultEncounter.cpp
│   │   └── ...
│   └── TargetSelector.cpp           # utility-AI target selection
├── Group/
│   ├── GroupSnapshot.{cpp,h}
│   ├── GroupRoleResolver.{cpp,h}
│   └── GroupTactics.{cpp,h}         # TankUpdate / HealerUpdate / DpsUpdate
├── Movement/
│   └── (thin — most logic is in PlayerbotAPI/MMaps)
├── Fleet/
│   ├── PopulationManager.{cpp,h}
│   ├── LfgMediator.{cpp,h}
│   ├── BgFiller.{cpp,h}
│   ├── NeighborhoodPopulator.{cpp,h}
│   ├── BotLifecycleManager.{cpp,h}
│   └── AdminCommandHandler.{cpp,h}
├── Util/
│   ├── Logging.{cpp,h}
│   └── Diagnostics.{cpp,h}
└── conf/
    └── playerbot.conf.dist
```

Target: ≤ 200 source files, ≤ 50,000 LOC.

Plus, on the core side:
```
src/server/game/Playerbot/
├── PlayerbotAPI.h         # the only header V2 includes
└── PlayerbotAPI.cpp       # implementation, on world thread
```

---

## 13. Testing strategy (locked invariants from §2.3)

Every AI decision is `(snapshot, rng) → intents`. Pure function. Testable without a server.

- **Unit tests**: one per APL spec, one per encounter script, one per state-dispatch. Synthetic snapshot in, expected intents out.
- **Integration tests**: spin up a minimal world with N bots, replay scripted scenarios, assert behavioral invariants (e.g., "tank never breaks CC," "healer triages by HP%").
- **Load tests**: spawn 500/1K/2K/5K bots, run for hours, assert tick budget and zero-deadlock invariants.

---

## 14. Open architectural questions (answer before Pass B)

1. **PlayerbotV2 module location**: side-by-side (`src/modules/PlayerbotV2/`) or in-place (replace `src/modules/Playerbot/`)? **Default assumption**: side-by-side for V1.0, delete V1 module after V2 ships.
2. **Snapshot building cost**: building a 5K snapshot bundle each tick is the riskiest perf assumption. Pass B must include a benchmark plan to validate it before committing.
3. **Encounter script discovery**: hand-registered per-boss vs. data-driven from a registry table? Default assumption: hand-registered for V1, table-driven later if a critical mass of scripts emerges.
4. **Activity tier transitions**: pure snapshot-driven, or event-driven? Default: snapshot-driven (simpler, more deterministic), event hooks only as transition triggers.
5. **Fleet thread or fleet-on-world-thread?** Default: separate fleet thread for clarity; if it adds zero value at 5K-bot scale, collapse onto world thread in Pass B.
6. **Group snapshot ownership**: published per group by world thread, or built on demand by AI worker? Default: per-group published on world thread (consistency over micro-perf).
7. **APL hot-reload**: should APLs reload from disk without server restart for class tuning? Default: yes, behind config flag (cheap to implement, big quality-of-life).

---

## 15. What's deliberately not in this doc

- Specific class signatures / method names (Pass B)
- Full `PlayerbotAPI.h` field listing (Pass B, derived from feature catalogs)
- Full directory tree with every file (Pass B)
- SQL schema (Pass B)
- Build flag wiring (Pass B)
- Per-feature mapping to architecture component (Pass B, validation matrix)

---

## 16. Pass A → Pass B exit criteria

Pass A is "done" when the user reads §§1–13 and can either say:
- "Yes, this is the system; proceed to Pass B," or
- "Here's what's wrong with §X and §Y; revise and re-review."

Pass B begins after Pass A is accepted, and produces concrete contracts the implementation can be derived from mechanically.

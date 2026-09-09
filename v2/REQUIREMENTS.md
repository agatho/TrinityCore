# Playerbot V2 — Requirements Specification

**Status**: Phase 0 (requirements gathering) — IN PROGRESS
**Owner**: Project lead (johannes)
**Last updated**: 2026-04-30
**Replaces**: All `PHASE_*`, `OPTION_*`, `REFACTORING_*` plans in repo root and module root.

This is the **single source of truth** for what V2 must do, what it must not do, and how we will know it works. If this document does not say something is required, it is not required. If a future doc contradicts this one, this one wins until explicitly amended here.

---

## Phase 0 status — what is decided vs deferred

**Process rule (locked)**: features come first. Architecture comes after the feature surface is locked. Any architectural decision made before Phase 0 closes is a *candidate*, not a constraint.

| Topic | Status | Where |
|---|---|---|
| Why we are doing this | Locked | §0 |
| High-level goals (scale targets, code budget, crash isolation) | Locked | §1.1 |
| Hard non-goals (V1.0 scope exclusions) | **Pending — requires §4 review first** | §1.2 |
| Anti-goals (forbidden patterns) | **Deferred to architecture phase** | (removed) |
| CORE operating envelope (scale, latency, memory, CPU) | Locked | §2.1 |
| CORE constraints (determinism, crash isolation, config, diagnostics, build) | Locked | §2.3–2.7 |
| Threading model (specific implementation) | **Proposed — pending architecture phase** | §2.2 |
| Core API surface (existence required) | Locked | §3 |
| Core API surface (specific contents) | **Proposed — pending architecture phase** | §3 |
| Player-mirror feature surface (what an individual bot can do) | Drafted in `FEATURES.md`, **awaiting review** | `FEATURES.md` |
| System-level feature surface (population, distribution, group autonomy, LFG/BG filling, admin) | Drafted in `SYSTEM_FEATURES.md`, **awaiting review** | `SYSTEM_FEATURES.md` |
| Per-feature priority / milestone tagging | **Deferred — happens after both feature catalogs are locked** | §4 |
| Acceptance criteria | **Pending — depends on milestone tagging** | §5 |
| Process rules | Locked | §6 |
| Open questions | Locked | §7 |

**Phase 0 closes when**: §4 milestone tags are assigned, §1.2 non-goals are confirmed against the catalog, §7 open questions are answered. Until then, no implementation work begins.

---

## 0. Why we are doing this

V1 has crossed the threshold where reduction is harder than replacement. Symptoms (objective):

- ~1,861 source files, ~636K LOC for one module
- 5 overlapping AI decision layers, 11+ event buses, 30+ subsystems
- 18+ documented deadlock fix attempts; emergency `IMMEDIATE_MUTEX_FIX.cpp` and `THREADPOOL_FIX_*.cpp` at repo root
- 562 markdown files documenting iterative course-corrections
- Module-purity rule has produced large amounts of state-mirroring/synchronization code that exists only because the module cannot ask the core a question directly

V2's purpose is **not** "more features." It is: **fewer lines of code, fewer concepts, more bots, less crashing.**

---

## 1. Goals and non-goals

### 1.1 Goals (locked)
1. **5,000 concurrent bots** on a single worldserver instance, with bots indistinguishable in workload from real players for the duration of a normal session.
2. **Zero deadlocks** under sustained load. The threading model — whichever is selected in the architecture phase — must make deadlocks structurally impossible, not merely unobserved.
3. **Code budget**: V2 first release ≤ 50,000 LOC across ≤ 200 source files. Hard cap. Exceeding it is a process failure, not a feature.
4. **Crash isolation**: a bug in one bot's tick must not affect the world thread, the server, or other bots.
5. **One core API surface**: a single header (or small set) under `src/server/game/Playerbot/` exposes everything V2 needs from TrinityCore. This is the only allowed core modification beyond hooks. Specific contents to be designed in the architecture phase against the locked feature surface.
6. **Architecture follows requirements, not the reverse.** Patterns are selected after the feature catalog is locked, by asking *what does this surface demand?*
7. **Bots use all in-game systems exactly as normal players do.** No shortcuts. No admin-only command paths. No special-case substitutions. No injected state. If a normal player must earn it, complete it, walk to it, pay for it, queue for it, or wait for it — so does the bot. This rule governs every feature in `FEATURES.md` and every system feature in `SYSTEM_FEATURES.md`. The only exceptions are bot lifecycle operations that have no player equivalent (creation, despawn, admin control plane); once a bot exists in the world, all in-world behavior follows player rules.

### 1.2 Candidate non-goals for V1.0 — pending review against `FEATURES.md`
Before any of these are locked as out-of-scope for V1.0, they must be cross-checked against the full catalog in `FEATURES.md`. The list below is the *proposed* V1.0 exclusion set; user must confirm or amend.
- PvP (battlegrounds, arenas, world PvP)
- Raids (10/25-player encounter mechanics)
- Professions (gathering, crafting)
- Achievements
- Dragonriding / skyriding mechanics
- LFG/LFR queueing systems
- Pet battles, transmog, mythic+, garrisons
- Cross-realm interactions
- Mail-based gold-farming or AH market manipulation
- Voice/macro/addon emulation

### 1.3 Anti-goals — deferred
Specific architectural prohibitions (e.g., behavior trees, blackboards, generic event bus frameworks, subsystem registries) are **not** decided yet. They will be evaluated during the architecture phase against the feature surface in `FEATURES.md`. Until then:
- The empirical evidence from V1 (where these patterns contributed to the mess) is **input** to the decision, not a verdict.
- Any pattern proposed for V2 must demonstrate, with worked examples against features in `FEATURES.md`, that it earns its complexity.
- The default bias is "simpler wins ties" — but ties are decided at architecture time, not now.

The only locked anti-goals are process-level:
- **Wholesale TrinityCore refactoring** (forbidden — out of scope)
- **"Helper" managers that exist only to coordinate other managers** (forbidden — symptom of accretion)

---

## 2. Operating envelope (CORE requirements)

### 2.1 Scale (locked)
| Metric | Target | Hard limit | Measurement |
|---|---|---|---|
| Concurrent bots | 5,000 | 10,000 | Load test on production-class hardware |
| Bot login rate | 100/sec sustained | 500/sec burst | Mass-login script |
| Bot tick latency (p50) | < 200 µs | < 1 ms | Per-tick timer histogram |
| Bot tick latency (p99) | < 1 ms | < 5 ms | Per-tick timer histogram |
| World tick budget for V2 | < 10 ms at 5K bots | < 20 ms | Total time spent in V2 per world tick |
| Memory per bot | < 1.5 MB | < 3 MB | Heap profile, 1K-bot snapshot |
| CPU per bot | < 0.02% | < 0.05% | Sustained, 5K bots, idle/light combat mix |

These numbers are constraints the architecture must satisfy, not architecture decisions themselves. They will be re-validated against the test hardware (§7 open question 5) before being treated as binding.

### 2.2 Threading model — *proposed, pending architecture phase*
Whichever threading approach is selected in the architecture phase MUST satisfy these **invariants** (locked):
- **Single-writer principle for game state**: every piece of mutable game state has exactly one owning thread at any moment.
- **No deadlock surface**: the model must be provably deadlock-free by construction, not by convention.
- **No shared mutex contention in the hot path** at the 5K-bot scale.
- **Crash isolation**: per-bot exceptions cannot cascade.

A *candidate* implementation that satisfies these invariants:
- World thread owns all `Player`/`Unit` mutations
- AI worker pool runs decision-making against immutable per-tick snapshots
- Lock-free intent queue (MPSC) per bot drains on world thread
- Snapshots replaced atomically (single pointer swap)

This candidate is **not** locked. Alternatives (e.g., world-thread-only with cooperative scheduling, fiber-based, actor-model) will be evaluated against the feature surface during architecture phase. Whichever is chosen, it must satisfy the locked invariants above.

### 2.3 Deterministic execution (locked)
- All bot decisions must be expressible as pure functions of `(snapshot, RNG state)`.
- RNG is per-bot, seeded from bot ID, not global.
- Same snapshot + seed must produce same intent. This is testable and is tested.

### 2.4 Crash isolation (locked)
- Each bot's tick runs inside a try/catch boundary. An uncaught exception logs, increments a counter, and skips that bot for one tick.
- Three consecutive tick failures despawn the bot and alert.
- No `assert()` in V2 code paths that run per-tick. Use logged invariant checks.

### 2.5 Configuration (locked)
- All tuning lives in `playerbot.conf` under `Playerbot.*` keys.
- Hot-reload supported for non-structural keys (timeouts, thresholds, weights).
- No magic numbers in code. Anything a designer might want to tune is a config key.

### 2.6 Diagnostics (locked, build-time required, run-time toggleable)
- Per-bot state inspector: `.playerbot inspect <name>` GM command shows current state, last 8 intents, RNG seed, snapshot age.
- Performance counters: tick count, exception count, avg/p50/p99 tick latency, intent queue depth, snapshot publish rate.
- Optional per-bot tick trace, ring-buffer 256 entries, dumpable on demand.
- Health endpoint: aggregate counters served on a TCP port for external monitoring.

### 2.7 Build & shippability (locked)
- Single compilation unit per .cpp where reasonable; no template-heavy headers driving 30-minute builds.
- V2 module compiles in < 5 minutes on the dev machine, full clean.
- Zero warnings at `/W4` (MSVC) and `-Wall -Wextra` (GCC/Clang).
- One binary, one config; no dynamic plugin loading.

---

## 3. Core API surface (TrinityCore-side change)

**Locked**: V2 requires exactly one core modification — a thin facade under `src/server/game/Playerbot/` exposing the state and actions the module needs. This is the only allowed core change beyond hooks.

**Locked rule**: V2 code does not include `Player.h`/`Unit.h`/etc. directly. It includes the API. If something is missing from the API, we add it to the API. We do not reach around it.

**Proposed (pending architecture phase, validated against `FEATURES.md`)**: the specific contents of the API surface. The candidate listing below is *illustrative* — the final surface will be derived from "what does each feature in `FEATURES.md` require?"

### 3.1 Read accessors (candidate)
- Identity: `GUID`, `name`, `level`, `class`, `race`, `spec`, `faction`
- Stats: `hp`, `max_hp`, `power(type)`, `max_power(type)`, all primary/secondary stats
- Position: `map_id`, `x/y/z/o`, `area_id`, `zone_id`, `is_indoors`, `is_swimming`, `is_flying`
- Combat state: `in_combat`, `victim_guid`, `target_guid`, `attackers[]`, `threat[]`
- Auras: `auras[]` (id, stacks, remaining_ms, caster, dispel_type)
- Cooldowns: `is_ready(spell_id)`, `cooldown_remaining_ms(spell_id)`, `gcd_remaining_ms`
- Inventory: `items[]` (slot, id, count, durability), `bag_free_slots`, `gold`
- Equipment: `equipped[slot]`, item-level summary
- Spells: `known_spells[]`, `is_known(spell_id)`
- Group: `group_guid`, `members[]`, `role(member)`, `loot_method`
- Quest log: `quests[]` (id, state, objectives[])
- Movement: `is_moving`, `velocity`, `path_target`

### 3.2 Action commands (candidate)
- Combat: `cast_spell(spell_id, target?)`, `cancel_cast`, `start_attack(target)`, `stop_attack`
- Movement: `move_to(x,y,z)`, `move_path(points[])`, `stop`, `jump`, `mount(id?)`, `dismount`
- Items: `use_item(slot)`, `equip_item(slot)`, `loot(corpse_guid)`, `pick_loot_item(idx)`, `release_corpse`, `revive_at_corpse`
- Vendor: `vendor_buy(npc_guid, slot, count)`, `vendor_sell(slot, count)`, `repair_all(npc_guid)`
- Quest: `quest_accept(npc, id)`, `quest_complete(npc, id)`, `quest_abandon(id)`
- Group/social: `group_invite(name)`, `group_accept`, `group_leave`, `whisper(name, text)`, `say(text)`, `party_chat(text)`
- Trade: `trade_initiate(target)`, `trade_add_item(slot)`, `trade_set_gold(amount)`, `trade_accept`, `trade_cancel`
- Bank/mail: `bank_deposit(slot)`, `bank_withdraw(slot)`, `mail_send(to, items, gold, subject, body)`
- Auction: `ah_post(slot, count, bid, buyout, hours)`, `ah_buy(auction_id)`, `ah_cancel(auction_id)`

### 3.3 Hooks (candidate)
- `OnPlayerLogin`, `OnPlayerLogout`
- `OnDamageDealt`, `OnDamageTaken`, `OnHealReceived`
- `OnAuraApplied`, `OnAuraRemoved`
- `OnSpellCastStart`, `OnSpellCastSuccess`, `OnSpellCastFailed`
- `OnLoot`, `OnQuestStateChanged`, `OnLevelUp`
- `OnGroupMemberJoined`, `OnGroupMemberLeft`
- `OnDeath`, `OnResurrect`

### 3.4 Friendship grant (candidate)
`Player`, `Group`, `Map` declare `friend class Playerbot::API;` to permit the API to read private state without copy-pasting accessor implementations into the module.

**The above is illustrative**. The locked deliverable is "an API exists, and it covers what `FEATURES.md` demands." Final field list is derived later.

---

## 4. Feature surface (GAME + SYSTEM requirements)

The feature surface is split across **two complementary catalogs**:

- **[`FEATURES.md`](./FEATURES.md)** — *player-mirror surface*: every distinct kind of action, state, and interaction an individual bot can perform as a player (movement, combat, quests, inventory, etc.).
- **[`SYSTEM_FEATURES.md`](./SYSTEM_FEATURES.md)** — *system-level surface*: how the bot ecosystem behaves at scale (population sizing, level/class distribution, autonomous group formation, player-invite handling, LFG/BG filling, admin control plane, etc.).

This document does **not** duplicate either surface. It only carries:
- Milestone tagging (V1.0 / V1.1 / V2 / LATER), assigned **after** both catalogs are locked
- MUST / SHOULD / MAY priority within each milestone

**Status**: milestone tagging is deferred until the user has reviewed and amended both catalogs. Premature tagging (before the surfaces are complete) would re-introduce the failure mode this Phase-0 reset is trying to avoid.

**Process for closing this section**:
1. User reads `FEATURES.md` and `SYSTEM_FEATURES.md` end-to-end and edits both (add missing items, delete unwanted items, split bullets that are actually multiple capabilities, fill in the "Acknowledged gaps" sections).
2. Once both catalogs are locked, this section is filled with milestone+priority tags per feature, drawing from both.
3. Then §1.2 non-goals are reconciled against the tagged catalogs.
4. Then §5 acceptance criteria are derived from V1.0 MUSTs (including system-level acceptance, e.g., "BG fills both factions correctly with N bots").
5. Then architecture phase begins, evaluated against the *union* of both surfaces.

---

## 5. Acceptance criteria — pending milestone tagging

Acceptance criteria for V1.0, V1.1, and V2 will be derived directly from §4 milestone tagging once that is complete. The criteria below are *illustrative shape only*, not locked:

> *Example shape (not binding)*: "V1.0 ships when (1) all V1.0-MUST features in §4 work, (2) the locked CORE envelope numbers in §2.1 are met at <some-bot-count> sustained, (3) the §1.1 code budget is honored, (4) the build constraints in §2.7 hold."

Specific bot counts, durations, and feature subsets will be filled in after §4 is tagged.

---

## 6. Process rules (locked)

### 6.1 Documentation
- This file is the only living spec. `FEATURES.md` is the only living feature catalog. Amendments edit those files directly with a dated changelog entry.
- Implementation notes, design discussions, and post-mortems go in `v2/notes/` and are deleted when superseded. No `PHASE_*` files. No `OPTION_*` files. No competing master plans.
- Code comments explain *why*, never *what*. If you find yourself writing a comment that explains the line above it, rename the variable/function instead.

### 6.2 Code review gates
- Any PR that adds a new event bus, manager, or registry is auto-rejected unless this doc is amended first to authorize it.
- Any PR exceeding the file-count or LOC budget triggers a mandatory consolidation pass before merge.
- Any new mutex requires written justification and ownership thread documented in the file.

### 6.3 Kill criteria
We abandon V2 and try a different approach if any of the following occur:
- Six months elapsed and V1.0 acceptance criteria not met
- File count exceeds 250 before V1.0
- A deadlock is observed in V2 and traced to the threading model itself (not a bug in a single component)
- The "rewrite is finally cleaner" feeling does not materialize after V1.0 ships

If we hit kill criteria, we revert to V1 and adopt a different strategy (e.g., aggressive reduction of V1, or moving to a different upstream).

### 6.4 What we will reuse from V1
**Not source files. Knowledge.**
- The 562 markdown files are mined as *requirements input* — they tell us what problems exist. They are not ported.
- Specific tested algorithms (e.g., a working pathfinding adapter, a correct loot-roll calculator) may be lifted as standalone files if they meet V2's quality bar.
- SQL schema migrations under `sql/playerbot/` are reviewed and possibly preserved.
- Nothing else. No "let me port this manager, it's almost right." That path is V1.

---

## 7. Open questions (must be answered before architecture phase begins)

1. **Reference class for V1.0 fully-implemented spec?** Hunter (ranged + pet) or Warrior (melee + rage) or other. To be answered after §4 milestone tagging confirms a single-class V1.0 is the chosen path.
2. **Does V2 reuse V1's database schema or start fresh?** Migration cost vs. clean schema. Decision after `FEATURES.md` review reveals what state must be persisted.
3. **Does V2 live in `src/modules/PlayerbotV2/` (side-by-side) or replace `src/modules/Playerbot/` directly (in-place)?**
4. **Build flag `-DBUILD_PLAYERBOT_V2=ON` separate from `-DBUILD_PLAYERBOT=ON`?** Allows comparison runs.
5. **What is the test server hardware target?** "Production-class" must be defined in absolute terms (CPU model, core count, RAM) before §2.1 numbers are validated.
6. **Single-class V1.0 vs multi-class V1.0?** Single class enables clean pipeline test; two-class (e.g., tank + healer) enables real group testing. Tradeoff to be decided after §4 is tagged.
7. **What player content phase is the bot expected to inhabit?** Retail end-game gear/level, or progression through a leveling experience? Decision affects equipment scoring, quest selection, and class roster.

---

## 8. Changelog

- **2026-04-30** — Initial draft (with embedded architecture decisions and milestone tagging).
- **2026-04-30** — Restructured: features-first per project lead's direction. Architecture decisions deferred to architecture phase. Milestone tagging deferred until `FEATURES.md` is reviewed and locked. Added Phase 0 status table.
- **2026-04-30** — Added `SYSTEM_FEATURES.md` to cover system-level features (population management, autonomous group formation, player-invite handling, LFG/BG filling, admin control plane). The feature surface now lives in two complementary catalogs; both must be locked before milestone tagging or architecture work begins.
- **2026-05-01** — Added §1.1 #7: bots use all in-game systems exactly as normal players do. Project-wide invariant — no shortcuts, no admin-only paths, no special-case substitutions, no injected state in-world. Governs every feature in both catalogs.

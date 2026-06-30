# Playerbot V2 — Module Layout

**Status**: Pass B
**Last updated**: 2026-05-01
**Decisions answered from Pass A §14**:
- #1 module location → **side-by-side** at `src/modules/PlayerbotV2/`. V1 (`src/modules/Playerbot/`) remains untouched until V2 ships.
- #5 fleet thread → **separate thread** for V1.0. Easy to collapse onto world thread later if perf data shows it adds nothing; not easy to split if the call shapes ossify against world-thread assumptions.

## 1. Top-level location

```
TrinityCore/
├── src/
│   ├── modules/
│   │   ├── Playerbot/                  # V1, frozen, deleted after V2 ships
│   │   └── PlayerbotV2/                # V2 — this module
│   └── server/
│       └── game/
│           └── Playerbot/              # NEW — core-side API surface
│               ├── PlayerbotAPI.h
│               └── PlayerbotAPI.cpp
└── sql/
    └── playerbot_v2/                   # NEW — V2 schema migrations
        ├── 0001_init.sql
        ├── 0002_personality.sql
        └── ...
```

## 2. PlayerbotV2 module tree

Hard cap: 200 files, 50,000 LOC. Each file's purpose is documented inline. Files marked `(stub)` are placeholders shipped empty in the first commit and filled per milestone.

```
src/modules/PlayerbotV2/
├── CMakeLists.txt                          # See BUILD.md
├── PCH.h                                   # Precompiled header (TrinityCore PCH + V2 essentials)
├── PlayerbotV2.{cpp,h}                     # Module entry: registers hooks, owns ServiceLocator
├── PlayerbotV2_Module.{cpp,h}              # TrinityCore module adapter (one of two integration shims)
│
├── Bot/                                    # Per-bot layer (CONTRACTS.md §2)
│   ├── BotAI.{cpp,h}                       # Top-level state machine, tick entry point
│   ├── BotSnapshot.{cpp,h}                 # Snapshot type
│   ├── BotSnapshotBuilder.{cpp,h}          # Build snapshot from Player* (world thread only)
│   ├── BotSnapshotView.{cpp,h}             # Ergonomic facade over const snapshot
│   ├── BotIntent.{cpp,h}                   # Intent variant + queue
│   ├── BotIntentEmitter.{cpp,h}            # Push-only handle to per-bot intent queue
│   ├── BotEventInbox.{cpp,h}               # Per-bot event ring buffer
│   ├── BotRng.{cpp,h}                      # Per-bot deterministic RNG
│   ├── BotPersonality.{cpp,h}              # Skill tier, verbosity, prefs
│   ├── BotActivityTier.{cpp,h}             # Combat/Active/Idle/Hibernate tier transitions
│   └── States/                             # One file per top-level state
│       ├── StateBase.{cpp,h}               # State dispatch interface
│       ├── State_LoggingIn.cpp
│       ├── State_LoggingOut.cpp
│       ├── State_Idle.cpp
│       ├── State_Travelling.cpp
│       ├── State_Questing.cpp
│       ├── State_InCombat.cpp
│       ├── State_Looting.cpp
│       ├── State_Dead.cpp
│       ├── State_Resurrecting.cpp
│       ├── State_AtVendor.cpp              # Cross-cutting; can interleave
│       ├── State_AtMailbox.cpp             # Cross-cutting
│       ├── State_AtAuctionHouse.cpp        # Cross-cutting
│       ├── State_InGroup.cpp               # Cross-cutting
│       ├── State_InInstance.cpp            # Cross-cutting
│       └── State_Decorating.cpp            # Cross-cutting (housing)
│
├── Combat/
│   ├── ApRotation.{cpp,h}                  # APL evaluator
│   ├── ApRule.{cpp,h}                      # Single rule type
│   ├── ApRegistry.{cpp,h}                  # (class,spec) → APL lookup
│   ├── TargetSelector.{cpp,h}              # Utility-AI target choice
│   ├── ThreatModel.{cpp,h}                 # Read threat from snapshot
│   ├── DispelDecisions.{cpp,h}             # Pure functions for dispel/decurse
│   ├── InterruptDecisions.{cpp,h}          # Pure functions for interrupt
│   ├── CcDecisions.{cpp,h}                 # Pure functions for crowd control
│   ├── DefensiveDecisions.{cpp,h}          # Defensive cooldown logic
│   ├── Apl/                                # One file per (class, spec). Sample below.
│   │   ├── Apl_Hunter_BeastMastery.cpp
│   │   ├── Apl_Hunter_Marksmanship.cpp
│   │   ├── Apl_Hunter_Survival.cpp
│   │   ├── Apl_Warrior_Arms.cpp
│   │   ├── Apl_Warrior_Fury.cpp
│   │   ├── Apl_Warrior_Protection.cpp
│   │   ├── Apl_Mage_Fire.cpp
│   │   ├── Apl_Mage_Frost.cpp
│   │   ├── Apl_Mage_Arcane.cpp
│   │   ├── Apl_Priest_Discipline.cpp
│   │   ├── Apl_Priest_Holy.cpp
│   │   ├── Apl_Priest_Shadow.cpp
│   │   ├── Apl_Rogue_Assassination.cpp
│   │   ├── Apl_Rogue_Outlaw.cpp
│   │   ├── Apl_Rogue_Subtlety.cpp
│   │   ├── Apl_Druid_Balance.cpp
│   │   ├── Apl_Druid_Feral.cpp
│   │   ├── Apl_Druid_Guardian.cpp
│   │   ├── Apl_Druid_Restoration.cpp
│   │   ├── Apl_Paladin_Holy.cpp
│   │   ├── Apl_Paladin_Protection.cpp
│   │   ├── Apl_Paladin_Retribution.cpp
│   │   ├── Apl_Shaman_Elemental.cpp
│   │   ├── Apl_Shaman_Enhancement.cpp
│   │   ├── Apl_Shaman_Restoration.cpp
│   │   ├── Apl_Warlock_Affliction.cpp
│   │   ├── Apl_Warlock_Demonology.cpp
│   │   ├── Apl_Warlock_Destruction.cpp
│   │   ├── Apl_DeathKnight_Blood.cpp
│   │   ├── Apl_DeathKnight_Frost.cpp
│   │   ├── Apl_DeathKnight_Unholy.cpp
│   │   ├── Apl_Monk_Brewmaster.cpp
│   │   ├── Apl_Monk_Mistweaver.cpp
│   │   ├── Apl_Monk_Windwalker.cpp
│   │   ├── Apl_DemonHunter_Havoc.cpp
│   │   ├── Apl_DemonHunter_Vengeance.cpp
│   │   ├── Apl_Evoker_Devastation.cpp
│   │   ├── Apl_Evoker_Preservation.cpp
│   │   └── Apl_Evoker_Augmentation.cpp
│   └── Encounters/
│       ├── EncounterScript.{cpp,h}         # Base interface
│       ├── EncounterRegistry.{cpp,h}       # NPC ID → script lookup
│       ├── DefaultEncounter.cpp            # Generic AoE-dodge / target-follow
│       └── (per-boss scripts added incrementally; one file each)
│
├── Group/                                  # CONTRACTS.md §3
│   ├── GroupSnapshot.{cpp,h}
│   ├── GroupSnapshotBuilder.{cpp,h}
│   ├── GroupRoleResolver.{cpp,h}
│   ├── GroupTactics.{cpp,h}                # TankUpdate / HealerUpdate / DpsUpdate dispatch
│   ├── TankTactics.cpp
│   ├── HealerTactics.cpp
│   ├── DpsTactics.cpp
│   ├── GroupLootDecisions.{cpp,h}          # Need/Greed/Pass per item
│   ├── GroupReadyCheck.{cpp,h}
│   └── GroupMarks.{cpp,h}                  # Raid icon assignment
│
├── Movement/
│   ├── MovementIntents.{cpp,h}             # MoveTo, Follow, Mount, Hearth, etc.
│   ├── StuckDetector.{cpp,h}               # Tick-over-tick position delta
│   ├── MountChooser.{cpp,h}                # Map/zone/level → mount id
│   ├── HazardAvoidance.{cpp,h}             # Lava, ground AoE, fatigue water
│   └── FollowController.{cpp,h}            # Follow-leader logic with hysteresis
│
├── Quest/
│   ├── QuestPicker.{cpp,h}                 # Utility-AI: which quest to do next
│   ├── QuestObjectiveRouter.{cpp,h}        # Dispatch by objective type
│   ├── ObjectiveKill.cpp
│   ├── ObjectiveGather.cpp
│   ├── ObjectiveInteract.cpp
│   ├── ObjectiveDeliver.cpp
│   ├── ObjectiveEscort.cpp
│   └── QuestRewardChooser.{cpp,h}          # Pick reward best for spec
│
├── Inventory/
│   ├── InventoryAuditor.{cpp,h}            # What's bag-vs-bank vs sold
│   ├── EquipmentScorer.{cpp,h}             # Stat-aware ilvl scoring per spec
│   ├── EquipmentManager.{cpp,h}            # Decide swaps
│   ├── ConsumableManager.{cpp,h}           # Food, bandages, potions, scrolls
│   └── VendorPolicy.{cpp,h}                # Sell rules, buy rules, repair threshold
│
├── Economy/
│   ├── BankPolicy.{cpp,h}                  # Deposit/withdraw decisions
│   ├── MailPolicy.{cpp,h}                  # Read inbox, take items, send mail
│   ├── AhPolicy.{cpp,h}                    # Browse, bid, post, cancel
│   └── TradePolicy.{cpp,h}                 # Accept/reject trade offers
│
├── Social/
│   ├── ChatHandler.{cpp,h}                 # Inbound whisper/say/party parsing
│   ├── ChatResponder.{cpp,h}               # Personality-driven responses
│   ├── CommandParser.{cpp,h}               # !follow, !attack, etc.
│   ├── EmoteResponder.{cpp,h}
│   ├── FriendsHandler.{cpp,h}
│   ├── IgnoreHandler.{cpp,h}
│   ├── GuildHandler.{cpp,h}
│   └── CalendarHandler.{cpp,h}
│
├── Pets/
│   ├── HunterPet.{cpp,h}                   # Tame, summon, revive, feed, commands
│   ├── WarlockDemon.{cpp,h}
│   ├── DkGhoul.{cpp,h}
│   ├── MageElemental.{cpp,h}
│   ├── ShamanElemental.{cpp,h}
│   ├── DruidGuardian.{cpp,h}
│   └── BattlePets.{cpp,h}                  # Collection + battles (later milestone)
│
├── Mounts/
│   └── MountManager.{cpp,h}                # Owned mounts, equipment slot, summon decision
│
├── Professions/
│   ├── ProfessionPolicy.{cpp,h}            # Train, level, specialize
│   ├── GatheringPolicy.{cpp,h}             # Mining/herb/skinning routing
│   ├── CraftingPolicy.{cpp,h}              # Recipe selection, queue
│   └── Cooking_Fishing.{cpp,h}
│
├── Talents/
│   ├── TalentPicker.{cpp,h}                # Class + spec + hero talent loadouts
│   ├── PvpTalentPicker.{cpp,h}
│   └── GlyphPicker.{cpp,h}                 # Where applicable
│
├── Reputation/
│   └── RepGrindPolicy.{cpp,h}              # Choose factions to grind
│
├── Death/
│   ├── DeathHandler.{cpp,h}                # Release / corpse run / accept res
│   └── ReleaseDecision.{cpp,h}             # When to release vs. wait for res
│
├── PvP/
│   ├── BgObjectiveScorer.{cpp,h}           # Utility AI per BG type
│   ├── BgAlteracValley.cpp
│   ├── BgArathiBasin.cpp
│   ├── BgEyeOfTheStorm.cpp
│   ├── BgWarsongGulch.cpp
│   ├── BgTwinPeaks.cpp
│   ├── BgSilvershardMines.cpp
│   ├── BgTempleOfKotmogu.cpp
│   ├── BgSeethingShore.cpp
│   ├── BgEpicAlteracValley.cpp
│   ├── BgEpicIsleOfConquest.cpp
│   ├── BgEpicWintergrasp.cpp
│   ├── ArenaBehavior.{cpp,h}               # 2v2/3v3/Solo Shuffle
│   └── DuelBehavior.{cpp,h}
│
├── Instance/
│   ├── DungeonRunner.{cpp,h}               # Trash pacing, boss positioning
│   ├── RaidRunner.{cpp,h}                  # Larger group coordination
│   ├── DelveRunner.{cpp,h}                 # 1–5 player delve flow
│   └── ScenarioRunner.{cpp,h}              # Solo & group scenarios
│
├── Housing/                                # SYSTEM_FEATURES.md §6.6
│   ├── NeighborhoodChoice.{cpp,h}          # Which neighborhood to join
│   ├── PlotPurchase.{cpp,h}                # Decide when to buy
│   ├── DecorationPolicy.{cpp,h}            # Interior decoration decisions
│   ├── YardPolicy.{cpp,h}                  # Exterior decoration decisions
│   ├── DecorationCollection.{cpp,h}        # Track owned decorations
│   ├── HouseVisitorBehavior.{cpp,h}        # Greet, emote, react
│   └── OpenHouseEvents.{cpp,h}             # Occasional showcase
│
├── Fleet/                                  # SYSTEM_FEATURES.md surface, runs on fleet thread
│   ├── FleetTick.{cpp,h}                   # 1Hz scheduler
│   ├── PopulationManager.{cpp,h}           # Target curves, auto-scale
│   ├── PopulationCurves.{cpp,h}            # Per-faction/level/class targets, time-of-day
│   ├── BotLifecycleManager.{cpp,h}         # Spawn/despawn with mid-content protection
│   ├── BotPool.{cpp,h}                     # Warm pool + JIT factory
│   ├── BotIdentityFactory.{cpp,h}          # Name + appearance generation
│   ├── BotAccountAllocator.{cpp,h}         # Pseudo-account distribution
│   ├── LfgMediator.{cpp,h}                 # Player LFG → fill bots
│   ├── BgFiller.{cpp,h}                    # BG queue → fill both factions
│   ├── ArenaFiller.{cpp,h}
│   ├── NeighborhoodPopulator.{cpp,h}       # Neighborhood density quotas
│   ├── AhMarketPresence.{cpp,h}            # Always-some-bots-posting
│   └── AdminCommandHandler.{cpp,h}         # `.playerbot` GM commands
│
├── Persistence/
│   ├── BotPersistence.{cpp,h}              # Read/write playerbot_v2_* tables
│   ├── BotPersonalityStore.{cpp,h}
│   ├── BotPreferencesStore.{cpp,h}
│   ├── PlayerbotMigrationMgr.{cpp,h}       # Schema migration runner
│   └── (no shadow tables — see SCHEMA.md)
│
├── Threading/
│   ├── AiWorkerPool.{cpp,h}                # The N worker threads
│   ├── FleetThread.{cpp,h}                 # The single fleet thread
│   ├── IntentQueue.{cpp,h}                 # Lock-free MPSC
│   ├── SnapshotPublisher.{cpp,h}           # Atomic ptr swap mechanism
│   └── TickScheduler.{cpp,h}               # Decides which bot ticks this frame
│
├── Diagnostics/
│   ├── PerfCounters.{cpp,h}                # Tick latency histogram, etc.
│   ├── BotInspector.{cpp,h}                # `.playerbot inspect <name>`
│   ├── HealthEndpoint.{cpp,h}              # TCP port for monitoring
│   └── TickTracer.{cpp,h}                  # Ring-buffer per-bot trace
│
├── Util/
│   ├── ObjectGuidExt.{cpp,h}
│   ├── DistanceMath.{cpp,h}
│   ├── Logging.{cpp,h}
│   ├── ConfigReader.{cpp,h}                # Hot-reload config
│   └── StableHash.h                        # For deterministic ordering
│
├── Tests/                                  # Unit tests (Catch2 or doctest)
│   ├── CMakeLists.txt
│   ├── Test_Snapshot.cpp
│   ├── Test_IntentQueue.cpp
│   ├── Test_StateMachine.cpp
│   ├── Test_Apl_Hunter.cpp
│   ├── Test_TargetSelector.cpp
│   ├── Test_GroupTactics.cpp
│   ├── Test_LfgMediator.cpp
│   ├── Test_PlotPurchase.cpp
│   └── Test_DeterministicReplay.cpp
│
└── conf/
    └── playerbot.conf.dist                 # Documented in CONFIG.md
```

## 3. Core-side files (the only TrinityCore modification)

```
src/server/game/Playerbot/
├── PlayerbotAPI.h                          # Public surface — V2 includes this and only this
├── PlayerbotAPI.cpp                        # Implementation, runs on world thread
├── PlayerbotAPI_Detail.h                   # Internal types (not for module consumption)
├── PlayerbotHooks.h                        # Hook-firing inline functions called from core
└── PlayerbotHooks.cpp
```

Friendship grants are added to:
- `src/server/game/Entities/Player/Player.h` — `friend class Playerbot::API;`
- `src/server/game/Entities/Unit/Unit.h` — same
- `src/server/game/Maps/Map.h` — same
- `src/server/game/Groups/Group.h` — same
- `src/server/game/Spells/Spell.h` — same
- `src/server/game/Items/Item.h` — same
- `src/server/game/Quests/Quest.h` — same

Each grant is exactly one line. No other core file is modified except the hook insertion sites in §4.

## 4. Hook insertion sites (counted, justified)

Hooks are inline-noop calls when V2 is not compiled. Total insertion sites in core ≤ 30.

| Hook | Site (file:function) | Justification |
|---|---|---|
| `OnPlayerLogin` | `Player.cpp:Player::OnLogin` | Bot needs to know it's in-world |
| `OnPlayerLogout` | `Player.cpp:Player::OnLogout` | Bot lifecycle |
| `OnLevelUp` | `Player.cpp:Player::GiveLevel` | Talent/spell unlock reactions |
| `OnDeath` | `Unit.cpp:Unit::Kill` | Death state transition |
| `OnResurrect` | `Player.cpp:Player::ResurrectPlayer` | Resume state machine |
| `OnDamageDealt` | `Unit.cpp:Unit::DealDamage` | Threat/snapshot trigger |
| `OnDamageTaken` | `Unit.cpp:Unit::DealDamage` (target side) | Combat tier promotion |
| `OnHealReceived` | `Unit.cpp:Unit::Heal` | Healer feedback |
| `OnAuraApplied` | `SpellAuras.cpp:Aura::HandleApply` | Reactive AI |
| `OnAuraRemoved` | `SpellAuras.cpp:Aura::HandleRemove` | Same |
| `OnSpellCastStart` | `Spell.cpp:Spell::cast` | Interrupt opportunities |
| `OnSpellCastSuccess` | `Spell.cpp:Spell::finish` | Resource/cooldown updates |
| `OnSpellCastFailed` | `Spell.cpp:Spell::SendCastResult` | Retry decisions |
| `OnLoot` | `LootHandler.cpp:WorldSession::HandleAutostoreLootItemOpcode` | Inventory updates |
| `OnQuestStateChanged` | `QuestHandler.cpp:Player::AddQuest`, `RewardQuest`, `AbandonQuest` | Quest state machine |
| `OnGroupMemberJoined` | `Group.cpp:Group::AddMember` | Group snapshot rebuild |
| `OnGroupMemberLeft` | `Group.cpp:Group::RemoveMember` | Same |
| `OnTradeRequested` | `TradeHandler.cpp` | Accept/reject decision |
| `OnWhisperReceived` | `ChatHandler.cpp` | Bot social response |
| `OnInstanceEnter` | `Map.cpp:InstanceMap::AddPlayerToMap` | Instance state machine |
| `OnInstanceExit` | `Map.cpp:InstanceMap::RemovePlayerFromMap` | Same |
| `OnBgEnter` / `OnBgExit` / `OnBgObjectiveProgress` | `Battleground.cpp` | BG behaviors |
| `OnEncounterPhaseChange` | (where applicable, in scripted boss code) | Encounter scripts |
| `OnAhAuctionExpired` / `OnAhAuctionSold` | `AuctionHouseMgr.cpp` | Mail-driven AH bots |
| `OnMailReceived` | `MailHandler.cpp` | Mail processing |
| `OnLfgQueued` | `LFGMgr.cpp` | LFG mediator trigger |
| `OnPlayerLfgQueued` | Same, but distinguishes player-initiated | Bot-fill trigger |
| `OnPlotPurchased` | (housing code, when present in 12.0+) | Neighborhood populator feedback |

Each insertion site is a single line: `Playerbot::Hooks::FireXxx(args...);`. When V2 is not built, this resolves to `inline void {} ` and is dead-code-eliminated.

## 5. CMake registration

The module is registered in `src/modules/CMakeLists.txt`:

```cmake
if(BUILD_PLAYERBOT_V2)
    add_subdirectory(PlayerbotV2)
endif()
```

The core-side surface in `src/server/game/Playerbot/` is registered conditionally in the relevant game `CMakeLists.txt`:

```cmake
if(BUILD_PLAYERBOT_V2)
    target_sources(game PRIVATE
        Playerbot/PlayerbotAPI.cpp
        Playerbot/PlayerbotHooks.cpp
    )
    target_compile_definitions(game PRIVATE TRINITY_PLAYERBOT_V2=1)
endif()
```

See `BUILD.md` for full wiring.

## 6. File-count budget tracking

Counts above (rough):
- Bot/: 23
- Combat/: 50 (incl. 39 APL files)
- Group/: 11
- Movement/: 5
- Quest/: 7
- Inventory/: 5
- Economy/: 4
- Social/: 8
- Pets/: 7
- Mounts/: 1
- Professions/: 4
- Talents/: 3
- Reputation/: 1
- Death/: 2
- PvP/: 14
- Instance/: 4
- Housing/: 7
- Fleet/: 13
- Persistence/: 4
- Threading/: 5
- Diagnostics/: 4
- Util/: 5
- Tests/: 10
- conf/: 1
- top-level: 4

**Total: ~196 files. Within the 200-file budget with no headroom.** Pass B may consolidate before V1.0 ships.

If the budget is exceeded during implementation, consolidation candidates (in priority order):
1. Merge per-BG behavior files into one `BgObjectiveScorer.cpp` with a per-BG dispatch
2. Merge per-objective quest files into one `QuestObjectiveRouter.cpp`
3. Combine `*Decisions.{cpp,h}` files in Combat/ into one `CombatDecisions.cpp`

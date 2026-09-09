# Playerbot V2 — Build & CMake Wiring

The module is built via the single CMake option `BUILD_PLAYERBOT_V2`. (The
legacy V1 module and its `BUILD_PLAYERBOT` flag have been removed; this is the
only Playerbot build flag.)

## 1. Top-level CMake option

In `CMakeLists.txt` at repository root, add:

```cmake
option(BUILD_PLAYERBOT_V2 "Build the Playerbot V2 module (next-generation)" OFF)
```

Default OFF. Opt-in.

## 2. Configuration scripts

The `configure_*.bat` scripts at the repository root pass `-DBUILD_PLAYERBOT_V2=1`.
A minimal manual configure:

```bat
cmake -S . -B build -A x64 ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DBUILD_PLAYERBOT_V2=ON ^
    -DTOOLS=ON
```

```bash
#!/usr/bin/env bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_PLAYERBOT_V2=ON \
    -DTOOLS=ON
```

## 3. Module CMake registration

In `src/modules/CMakeLists.txt`:

```cmake
if(BUILD_PLAYERBOT_V2)
    add_subdirectory(PlayerbotV2)
endif()
```

## 4. PlayerbotV2/CMakeLists.txt

```cmake
# src/modules/PlayerbotV2/CMakeLists.txt

# ---- Compile-time toggles ---------------------------------------------------
option(PLAYERBOT_V2_ENABLE_TESTS    "Build PlayerbotV2 unit tests"   OFF)
option(PLAYERBOT_V2_ENABLE_TRACING  "Enable per-bot tick tracing"    OFF)
option(PLAYERBOT_V2_INTENT_LOG      "Enable intent log table writes" OFF)

# ---- Source globs (explicit, no recursive globs) ----------------------------
set(PLAYERBOT_V2_SOURCES
    PlayerbotV2.cpp
    PlayerbotV2_Module.cpp

    # Bot/
    Bot/BotAI.cpp
    Bot/BotSnapshot.cpp
    Bot/BotSnapshotBuilder.cpp
    Bot/BotSnapshotView.cpp
    Bot/BotIntent.cpp
    Bot/BotIntentEmitter.cpp
    Bot/BotEventInbox.cpp
    Bot/BotRng.cpp
    Bot/BotPersonality.cpp
    Bot/BotActivityTier.cpp

    # Bot/States/
    Bot/States/StateBase.cpp
    Bot/States/State_LoggingIn.cpp
    Bot/States/State_LoggingOut.cpp
    Bot/States/State_Idle.cpp
    Bot/States/State_Travelling.cpp
    Bot/States/State_Questing.cpp
    Bot/States/State_InCombat.cpp
    Bot/States/State_Looting.cpp
    Bot/States/State_Dead.cpp
    Bot/States/State_Resurrecting.cpp
    Bot/States/State_AtVendor.cpp
    Bot/States/State_AtMailbox.cpp
    Bot/States/State_AtAuctionHouse.cpp
    Bot/States/State_InGroup.cpp
    Bot/States/State_InInstance.cpp
    Bot/States/State_Decorating.cpp

    # Combat/
    Combat/ApRotation.cpp
    Combat/ApRule.cpp
    Combat/ApRegistry.cpp
    Combat/TargetSelector.cpp
    Combat/ThreatModel.cpp
    Combat/DispelDecisions.cpp
    Combat/InterruptDecisions.cpp
    Combat/CcDecisions.cpp
    Combat/DefensiveDecisions.cpp

    # Combat/Apl/  -- 39 files, one per (class, spec)
    Combat/Apl/Apl_Hunter_BeastMastery.cpp
    Combat/Apl/Apl_Hunter_Marksmanship.cpp
    Combat/Apl/Apl_Hunter_Survival.cpp
    Combat/Apl/Apl_Warrior_Arms.cpp
    Combat/Apl/Apl_Warrior_Fury.cpp
    Combat/Apl/Apl_Warrior_Protection.cpp
    Combat/Apl/Apl_Mage_Fire.cpp
    Combat/Apl/Apl_Mage_Frost.cpp
    Combat/Apl/Apl_Mage_Arcane.cpp
    Combat/Apl/Apl_Priest_Discipline.cpp
    Combat/Apl/Apl_Priest_Holy.cpp
    Combat/Apl/Apl_Priest_Shadow.cpp
    Combat/Apl/Apl_Rogue_Assassination.cpp
    Combat/Apl/Apl_Rogue_Outlaw.cpp
    Combat/Apl/Apl_Rogue_Subtlety.cpp
    Combat/Apl/Apl_Druid_Balance.cpp
    Combat/Apl/Apl_Druid_Feral.cpp
    Combat/Apl/Apl_Druid_Guardian.cpp
    Combat/Apl/Apl_Druid_Restoration.cpp
    Combat/Apl/Apl_Paladin_Holy.cpp
    Combat/Apl/Apl_Paladin_Protection.cpp
    Combat/Apl/Apl_Paladin_Retribution.cpp
    Combat/Apl/Apl_Shaman_Elemental.cpp
    Combat/Apl/Apl_Shaman_Enhancement.cpp
    Combat/Apl/Apl_Shaman_Restoration.cpp
    Combat/Apl/Apl_Warlock_Affliction.cpp
    Combat/Apl/Apl_Warlock_Demonology.cpp
    Combat/Apl/Apl_Warlock_Destruction.cpp
    Combat/Apl/Apl_DeathKnight_Blood.cpp
    Combat/Apl/Apl_DeathKnight_Frost.cpp
    Combat/Apl/Apl_DeathKnight_Unholy.cpp
    Combat/Apl/Apl_Monk_Brewmaster.cpp
    Combat/Apl/Apl_Monk_Mistweaver.cpp
    Combat/Apl/Apl_Monk_Windwalker.cpp
    Combat/Apl/Apl_DemonHunter_Havoc.cpp
    Combat/Apl/Apl_DemonHunter_Vengeance.cpp
    Combat/Apl/Apl_Evoker_Devastation.cpp
    Combat/Apl/Apl_Evoker_Preservation.cpp
    Combat/Apl/Apl_Evoker_Augmentation.cpp

    # Combat/Encounters/
    Combat/Encounters/EncounterScript.cpp
    Combat/Encounters/EncounterRegistry.cpp
    Combat/Encounters/DefaultEncounter.cpp

    # Group/
    Group/GroupSnapshot.cpp
    Group/GroupSnapshotBuilder.cpp
    Group/GroupRoleResolver.cpp
    Group/GroupTactics.cpp
    Group/TankTactics.cpp
    Group/HealerTactics.cpp
    Group/DpsTactics.cpp
    Group/GroupLootDecisions.cpp
    Group/GroupReadyCheck.cpp
    Group/GroupMarks.cpp

    # Movement/
    Movement/MovementIntents.cpp
    Movement/StuckDetector.cpp
    Movement/MountChooser.cpp
    Movement/HazardAvoidance.cpp
    Movement/FollowController.cpp

    # Quest/
    Quest/QuestPicker.cpp
    Quest/QuestObjectiveRouter.cpp
    Quest/ObjectiveKill.cpp
    Quest/ObjectiveGather.cpp
    Quest/ObjectiveInteract.cpp
    Quest/ObjectiveDeliver.cpp
    Quest/ObjectiveEscort.cpp
    Quest/QuestRewardChooser.cpp

    # Inventory/
    Inventory/InventoryAuditor.cpp
    Inventory/EquipmentScorer.cpp
    Inventory/EquipmentManager.cpp
    Inventory/ConsumableManager.cpp
    Inventory/VendorPolicy.cpp

    # Economy/
    Economy/BankPolicy.cpp
    Economy/MailPolicy.cpp
    Economy/AhPolicy.cpp
    Economy/TradePolicy.cpp

    # Social/
    Social/ChatHandler.cpp
    Social/ChatResponder.cpp
    Social/CommandParser.cpp
    Social/EmoteResponder.cpp
    Social/FriendsHandler.cpp
    Social/IgnoreHandler.cpp
    Social/GuildHandler.cpp
    Social/CalendarHandler.cpp

    # Pets/
    Pets/HunterPet.cpp
    Pets/WarlockDemon.cpp
    Pets/DkGhoul.cpp
    Pets/MageElemental.cpp
    Pets/ShamanElemental.cpp
    Pets/DruidGuardian.cpp
    Pets/BattlePets.cpp

    # Mounts/
    Mounts/MountManager.cpp

    # Professions/
    Professions/ProfessionPolicy.cpp
    Professions/GatheringPolicy.cpp
    Professions/CraftingPolicy.cpp
    Professions/Cooking_Fishing.cpp

    # Talents/
    Talents/TalentPicker.cpp
    Talents/PvpTalentPicker.cpp
    Talents/GlyphPicker.cpp

    # Reputation/
    Reputation/RepGrindPolicy.cpp

    # Death/
    Death/DeathHandler.cpp
    Death/ReleaseDecision.cpp

    # PvP/
    PvP/BgObjectiveScorer.cpp
    PvP/BgAlteracValley.cpp
    PvP/BgArathiBasin.cpp
    PvP/BgEyeOfTheStorm.cpp
    PvP/BgWarsongGulch.cpp
    PvP/BgTwinPeaks.cpp
    PvP/BgSilvershardMines.cpp
    PvP/BgTempleOfKotmogu.cpp
    PvP/BgSeethingShore.cpp
    PvP/BgEpicAlteracValley.cpp
    PvP/BgEpicIsleOfConquest.cpp
    PvP/BgEpicWintergrasp.cpp
    PvP/ArenaBehavior.cpp
    PvP/DuelBehavior.cpp

    # Instance/
    Instance/DungeonRunner.cpp
    Instance/RaidRunner.cpp
    Instance/DelveRunner.cpp
    Instance/ScenarioRunner.cpp

    # Housing/
    Housing/NeighborhoodChoice.cpp
    Housing/PlotPurchase.cpp
    Housing/DecorationPolicy.cpp
    Housing/YardPolicy.cpp
    Housing/DecorationCollection.cpp
    Housing/HouseVisitorBehavior.cpp
    Housing/OpenHouseEvents.cpp

    # Fleet/
    Fleet/FleetTick.cpp
    Fleet/PopulationManager.cpp
    Fleet/PopulationCurves.cpp
    Fleet/BotLifecycleManager.cpp
    Fleet/BotPool.cpp
    Fleet/BotIdentityFactory.cpp
    Fleet/BotAccountAllocator.cpp
    Fleet/LfgMediator.cpp
    Fleet/BgFiller.cpp
    Fleet/ArenaFiller.cpp
    Fleet/NeighborhoodPopulator.cpp
    Fleet/AhMarketPresence.cpp
    Fleet/AdminCommandHandler.cpp

    # Persistence/
    Persistence/BotPersistence.cpp
    Persistence/BotPersonalityStore.cpp
    Persistence/BotPreferencesStore.cpp
    Persistence/PlayerbotMigrationMgr.cpp

    # Threading/
    Threading/AiWorkerPool.cpp
    Threading/FleetThread.cpp
    Threading/IntentQueue.cpp
    Threading/SnapshotPublisher.cpp
    Threading/TickScheduler.cpp

    # Diagnostics/
    Diagnostics/PerfCounters.cpp
    Diagnostics/BotInspector.cpp
    Diagnostics/HealthEndpoint.cpp
    Diagnostics/TickTracer.cpp

    # Util/
    Util/ObjectGuidExt.cpp
    Util/DistanceMath.cpp
    Util/Logging.cpp
    Util/ConfigReader.cpp
)

# ---- Static library ---------------------------------------------------------
add_library(playerbot_v2 STATIC ${PLAYERBOT_V2_SOURCES})

set_target_properties(playerbot_v2 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(playerbot_v2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/server/game/Playerbot   # PlayerbotAPI.h
)

target_link_libraries(playerbot_v2 PUBLIC
    game            # TrinityCore game lib (provides PlayerbotAPI symbols when V2 enabled)
    common
)

target_compile_definitions(playerbot_v2 PRIVATE
    TRINITY_PLAYERBOT_V2=1
)

if(PLAYERBOT_V2_ENABLE_TRACING)
    target_compile_definitions(playerbot_v2 PRIVATE PLAYERBOT_V2_TRACING=1)
endif()
if(PLAYERBOT_V2_INTENT_LOG)
    target_compile_definitions(playerbot_v2 PRIVATE PLAYERBOT_V2_INTENT_LOG=1)
endif()

# ---- Strict warnings (per REQUIREMENTS.md §2.7) -----------------------------
if(MSVC)
    target_compile_options(playerbot_v2 PRIVATE /W4 /WX)
else()
    target_compile_options(playerbot_v2 PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# ---- PCH --------------------------------------------------------------------
target_precompile_headers(playerbot_v2 PRIVATE PCH.h)

# ---- Linker into worldserver ------------------------------------------------
# Add to worldserver's link list (in the apporopriate worldserver CMakeLists.txt):
#   if(BUILD_PLAYERBOT_V2)
#       target_link_libraries(worldserver PRIVATE playerbot_v2)
#   endif()

# ---- Tests ------------------------------------------------------------------
if(PLAYERBOT_V2_ENABLE_TESTS)
    add_subdirectory(Tests)
endif()
```

## 5. Core-side wiring (`src/server/game/`)

In `src/server/game/CMakeLists.txt` (or wherever the `game` library sources are listed):

```cmake
if(BUILD_PLAYERBOT_V2)
    target_sources(game PRIVATE
        Playerbot/PlayerbotAPI.cpp
        Playerbot/PlayerbotHooks.cpp
    )
    target_compile_definitions(game PUBLIC TRINITY_PLAYERBOT_V2=1)
    target_include_directories(game PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Playerbot
    )
endif()
```

When V2 is OFF, `PlayerbotAPI.cpp` and `PlayerbotHooks.cpp` are not compiled, and `Hooks::*` calls from core-side code resolve to inline-empty functions in `PlayerbotHooks.h` (guarded by `#if !TRINITY_PLAYERBOT_V2`).

## 6. Hook insertion site discipline

Each of the ≤30 hook insertion sites in core (per `MODULE_LAYOUT.md` §4) is a single line:

```cpp
Playerbot::Hooks::OnDamageDealt(this, victim, damage, spellInfo ? spellInfo->Id : 0);
```

The header `PlayerbotHooks.h` provides:

```cpp
#pragma once

#if TRINITY_PLAYERBOT_V2
namespace Playerbot::Hooks {
    // Real declarations — bodies in PlayerbotHooks.cpp dispatch to V2::Module
    void OnDamageDealt(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
    // ... full list
}
#else
namespace Playerbot::Hooks {
    inline void OnDamageDealt(Unit*, Unit*, int32, uint32) {}
    // ... full list, all inline-empty
}
#endif
```

When V2 is OFF, the core code carries the call sites but the calls are zero-cost (compiler eliminates them). When V2 is ON, calls dispatch to `Module::Instance().OnDamageDealt(...)`.

## 7. Build performance constraints

Per `REQUIREMENTS.md` §2.7:
- Full clean build of V2 module: < 5 minutes on dev machine.
- Zero warnings at `/W4` (MSVC) and `-Wall -Wextra -Wpedantic` (GCC/Clang). `-Werror` for V2 sources.
- Unity / jumbo build: NOT enabled by default. May be added later if compile time exceeds budget; deferred decision.
- PCH: required (`PCH.h` carries TrinityCore PCH + V2's heavy STL includes).

## 8. CI integration

Two CI jobs added:
- `ci_playerbot_v2_build` — builds with `BUILD_PLAYERBOT_V2=ON`. Verifies clean compile, zero warnings.
- `ci_playerbot_v2_lint` — runs the include-what-you-use rule "no V2 source includes Player.h directly," and confirms hook insertion site count ≤ 30 via grep.

Optional later:
- `ci_playerbot_v2_test` — runs `Tests/` suite.
- `ci_playerbot_v2_load` — multi-hour smoke test of N bots (probably manual / nightly).

## 9. Compile-time sanity checks

In `PlayerbotV2.cpp`, at file scope:

```cpp
#if !TRINITY_PLAYERBOT_V2
#error "PlayerbotV2 module compiled without TRINITY_PLAYERBOT_V2 defined; CMake misconfiguration"
#endif

#if __cplusplus < 202002L
#error "PlayerbotV2 requires C++20"
#endif

static_assert(Playerbot::kApiVersion == 1,
              "PlayerbotV2 requires PlayerbotAPI version 1");
```

## 10. What's locked vs open

**Locked**: build flag name (`BUILD_PLAYERBOT_V2`), file globs (explicit, no recursive globs), warnings policy, hook insertion-site budget (≤ 30), C++ standard (C++20), strict separation of `playerbot` and `playerbot_v2` libraries.

**Open**:
- Whether to enable unity/jumbo builds — depends on actual compile-time data.
- Final `Tests/` framework choice (Catch2 vs doctest) — both fit; will pick at first test PR.
- Static analyzer integration (clang-tidy ruleset) — desirable, deferred.

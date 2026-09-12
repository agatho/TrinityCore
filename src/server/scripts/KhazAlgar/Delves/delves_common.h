/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_DELVES_COMMON_H
#define TRINITY_DELVES_COMMON_H

#include "DelveInstance.h"
#include "EventMap.h"
#include "InstanceScript.h"
#include "Position.h"

class Creature;
class GameObject;
struct ScenarioStepEntry;

namespace Delves
{

/*
 * Retail ids shared by every delve script. Evidence: C:\sniff\tcharvest\out\delve_research\REPORT.md
 * (five captures: gulf 69497, eversong 69497, deatholme 66562, shadowmoon 68974, 69273 Sites) -
 * the section/tick is quoted next to each value.
 */

// (WS_DELVE_COMPLETE 25316, WS_DELVE_ENCOUNTER_IN_PROGRESS 24836 and CURRENCY_VOIDLIGHT_MARL 3316 live in DelvesDefines.h)

// REPORT 2.3: scenario 3424 "Delves" (Type 8, Flags 2) starts on a fresh ScenarioGUID ~750 ms after the delve
// scenario completes (gulf 1063114, eversong 2612665, deatholme 743181). It has no `scenarios` row because it
// has no map of its own - DelveInstanceScript::StartRewardScenario starts it explicitly.
constexpr uint32 DELVE_REWARD_SCENARIO_ID = 3424;
constexpr uint32 REWARD_STEP_TREASURE_ROOM = 17119;                 // OrderIndex 0 "Treasure Room" - criteria 64984 GameEvent 88888
constexpr uint32 REWARD_STEP_COLLECT_REWARD_LEAVE_O_BOT = 17120;    // OrderIndex 1 "Collect Your Reward!" - criteria 67376 GameEvent 91282
constexpr uint32 REWARD_STEP_COLLECT_REWARD_NO_LEAVE_O_BOT = 17121; // OrderIndex 2 "Collect Your Reward!" - criteria 69831 GameEvent 93004

// REPORT 2.2: criteria 60399 = GameEvent 85913, the generic "delve boss slain" event shared by 50+ delve trees
// (last step of 3177 / 3154 / 3184).
constexpr uint32 GAME_EVENT_DELVE_BOSS_SLAIN = 85913;
// REPORT 2.3 / 5: second Heavy Trunk -> PROGRESS 64984 (GameEvent 88888) -> 3424 step 17120 -> Leave-O-Bot spawns
constexpr uint32 GAME_EVENT_TREASURE_FOUND = 88888;
// REPORT 2.3 / 5: criteria 67376 GameEvent 91282 "(Optional) Exit Delve with Leave-O-Bot after collecting rewards"
constexpr uint32 GAME_EVENT_LEAVE_O_BOT_USED = 91282;
// REPORT 2.3 / 5: criteria 69831 GameEvent 93004 "(Optional) Exit Delve after collecting rewards" (GO 408227, eversong 2650933)
constexpr uint32 GAME_EVENT_LEAVE_DELVE_USED = 93004;

// REPORT 5.2: SMSG_ACHIEVEMENT_EARNED 40436 "You're Getting a Delve!" at the completion tick (eversong 2611916)
constexpr uint32 ACHIEVEMENT_YOURE_GETTING_A_DELVE = 40436;

// REPORT 2.3 / 4: reward objects
constexpr uint32 GO_FRAGMENT_OF_REVELATION = 572806;    // gulf 1062329 (+9 ms after SMSG_BOSS_KILL)
constexpr uint32 GO_HEAVY_TRUNK_A = 584517;             // gulf 1062703 (+383 ms), eversong 2612261, deatholme 742784
constexpr uint32 GO_HEAVY_TRUNK_B = 584519;             // same tick as 584517; using this one fired GameEvent 88888 in all three runs
constexpr uint32 GO_MISLAID_CURIOSITY = 584752;         // REPORT 2.4: "Discovered Treasure" trigger, 8-10 per run (spell 1259272 personal summons)
constexpr uint32 GO_LEAVE_DELVE = 408227;               // REPORT 5.6: used right before eversong's NEW_WORLD (2650933)
constexpr uint32 NPC_LEAVE_O_BOT = 205496;              // REPORT 2.3: Vehicle "Leave-O-Bot 7000", display 139036, spawns after step 17120
constexpr uint32 HEAVY_TRUNK_ANIM_KIT = 13792;          // REPORT 2.3: SMSG_GAME_OBJECT_ACTIVATE_ANIM_KIT 13792 on trunk use (gulf 1072114)

// misc_decode.txt: Heavy Trunk use -> SMSG_SET_CURRENCY 3316 Voidlight Marl, Change 10, LostSrc 9 (= CurrencyGainSource::Loot)
// (gulf 1072860 qty 2481 change 10; eversong 2626130 qty 435 change 10).
constexpr uint32 HEAVY_TRUNK_VOIDLIGHT_MARL = 10;
// REPORT 2.4 / misc_decode: SET_CURRENCY 3253 "EVERGREEN Delves - Tracker - Mislaid Curiosity - Weekly Cap" Change 1 per curiosity
// (gulf 303655 / 393572 / 714763, deatholme 773101)
constexpr uint32 CURRENCY_MISLAID_CURIOSITY_WEEKLY = 3253;
// REPORT 2.4: SMSG_DISPLAY_PLAYER_CHOICE 822 "Discovered Treasure" (eversong 2121802, deatholme 155033)
constexpr uint32 PLAYER_CHOICE_DISCOVERED_TREASURE = 822;

// REPORT 3.1: the companion is a PLAYER-cast summon: 1247560 "Summon" (12.1 gulf 103987, eversong 1848985, deatholme 56090;
// SpellEffect 28 SPELL_EFFECT_SUMMON, MiscValue 248567, SummonProperties 6425) - 12.0.7 used 1247562 (shadowmoon 114847).
constexpr uint32 NPC_DELVE_COMPANION_VALEERA = 248567;
constexpr uint32 SPELL_SUMMON_DELVE_COMPANION = 1247560;
constexpr uint32 SPELL_SUMMON_DELVE_COMPANION_12_0_7 = 1247562;

// SetData ids understood by DelveInstanceScript (used by the reward object scripts in delve_objects.cpp)
enum DelveInstanceData : uint32
{
    DATA_TREASURE_ROOM_OPENED = 1000,   // a Heavy Trunk was used (fallback when the 3424 GameEvent cannot be observed)
};

// The player-owned delve companion (a TempSummon whose summoner is the player), or nullptr.
Creature* FindDelveCompanionOf(Player const* player);

/*
 * Base InstanceScript for all delve instances.
 * Extends InstanceScript with delve-specific lifecycle (revives, companion, checkpoints, scenario-driven
 * completion and the 3424 reward scenario). Per-delve scripts inherit from this and define boss encounters,
 * objectives, and environmental mechanics.
 */
class DelveInstanceScript : public InstanceScript
{
public:
    DelveInstanceScript(InstanceMap* map, uint8 tier);
    ~DelveInstanceScript() override;

    // InstanceScript overrides
    void OnPlayerEnter(Player* player) override;
    void OnPlayerLeave(Player* player) override;
    void Update(uint32 diff) override;
    // THE loop wiring: player deaths decrement the shared revive pool (fail at 0), and the death of the
    // template's FinalBossEntry creature raises GameEvent 85913 (the last step of every delve scenario).
    void OnUnitDeath(Unit* unit) override;
    void SetData(uint32 type, uint32 data) override;
    void ProcessEvent(WorldObject* obj, uint32 eventId, WorldObject* invoker) override;
    // Mirrors the boss encounter into ws 24836 (REPORT 1.6: 1 at SMSG_ENCOUNTER_START, 0 at SMSG_ENCOUNTER_END)
    bool SetBossState(uint32 id, EncounterState state) override;

    // Delve-specific hooks for subclasses to override
    virtual void OnDelveStart() { }
    virtual void OnDelveComplete() { }
    virtual void OnDelveFailed() { }
    virtual void OnCheckpointReached(uint32 /*checkpointId*/) { }
    // Fired when the delve scenario's current step changes (nullptr = the scenario finished).
    virtual void OnScenarioStepChanged(ScenarioStepEntry const* /*step*/) { }
    // Which creature's death is the delve's final boss kill. Default: delve_template.finalBossEntry.
    virtual bool IsFinalBoss(Creature const* creature) const;

    // Scenario integration
    void OnScenarioComplete();

    // Death handling
    void OnPlayerDeath(Player* player);

    // Accessors
    DelveInstance* GetDelveInstance() { return _delveInstance.get(); }
    DelveInstance const* GetDelveInstance() const { return _delveInstance.get(); }

    // Object placement helpers. The per-map reward object placements decoded from the captures live in
    // delves_common.cpp (DelveCompletionSpawns).
    GameObject* SpawnDelveGameObject(uint32 entry, Position const& pos);
    Creature* SpawnLeaveOBot();

protected:
    void EnsureScenarioAttached();
    void HandleScenarioCompleted();
    void SpawnHeavyTrunks();
    void StartRewardScenario();
    void SummonCompanionFor(Player* player);
    void DespawnCompanionOf(Player const* player);

    std::unique_ptr<DelveInstance> _delveInstance;

private:
    EventMap _events;
    GuidUnorderedSet _grantedEntrants;
    std::vector<ObjectGuid> _pendingCompanionSummons;
    ScenarioStepEntry const* _lastObservedStep = nullptr;
    bool _scenarioObserved = false;
    bool _completionHandled = false;
    bool _leaveOBotSpawned = false;
    bool _hasFinalBossDeathPos = false;
    Position _finalBossDeathPos;
};

} // namespace Delves

#endif // TRINITY_DELVES_COMMON_H

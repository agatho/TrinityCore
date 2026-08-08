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

/*
 * The Shadow Enclave (Midnight S1 delve) - scenario objective creatures.
 * Map 2952, Difficulty 208 (Delves), Scenario 3154, LFGDungeons 3069, AreaTable 16594.
 *
 * WHY THESE NEED SCRIPTS AT ALL
 * -----------------------------
 * Every step of scenario 3154 is driven by Criteria Type 92
 * (CriteriaType::AnyoneTriggerGameEventScenario, DBCEnums.h:639 - "Anyone will Trigger game event
 * {GameEvents} (Scenario Only)"), whose Asset is a GameEvents id, NOT a creature entry. GameEvents
 * is not shipped in the client DB2 set and no event_scripts / smart_scripts row in integ_world
 * references any of these ids, so nothing in data connects "this creature died" to "this step
 * advances". The server has to raise the event explicitly - that is what these scripts do, and it
 * is the whole of what they do.
 *
 * SCENARIO 3154 STEP TABLE
 * ------------------------
 * Read out of the 12.0.7 client DB2s (ScenarioStep.db2 -> CriteriaTree.db2 -> Criteria.db2) and
 * corroborated on the wire by C:\sniff\alliance_deatholme_delve\dumps\
 * dump_12.0.1.66562_2026-03-26_08-04-06.pkt ("Deatholme delve" = The Shadow Enclave), which
 * contains a complete run and whose SMSG_SCENARIO_STATE step order is
 * 16032 -> 16028 -> 16029 -> 16030 -> 16031 -> SMSG_SCENARIO_COMPLETED(3154):
 *
 *   Order 0  Step 16032  tree 214779/214780  criteria 107837  event 99761  amount 1
 *            "Pursue Antenorian"                                       -- NOT scripted here, no
 *                                                                         ScriptName exists for it
 *   Order 1  Step 16028  tree 214781/214782  criteria 107838  event 99763  amount 2
 *            Void Focus destroyed          -> npc_void_focus_se       (creature 250266)
 *   Order 2  Step 16029  tree 214768
 *              221264 "Rituals stopped"  amount 3 -> 214769 criteria 107834 event 99755
 *                                           -> npc_darkcaller         (250242 / 251898 / 251899)
 *              214770 "Cultists purged"  amount 200, weighted trash events 87796/87797/87798/87795
 *                                           -- NOT scripted here, see OPEN ITEMS below
 *   Order 3  Step 16030  tree 214775/214776  criteria 107835  event 99756  amount 3
 *            "Antenorian's Devoted slain" (creature 250275)
 *                                           -- NOT scripted here, no ScriptName exists for it
 *   Order 4  Step 16031  tree 214777/214778  criteria  60399  event 85913  amount 1
 *            Antenorian slain              -> npc_lord_antenorian     (creature 246717)
 *
 * Event 85913 is a shared "delve boss slain" event reused by 32 criteria trees across many delves,
 * so it is safe to raise from the boss's death: it only satisfies whichever delve scenario is
 * actually running on the map.
 *
 * The final boss identity is not guesswork either. In the same capture:
 *   SMSG_ENCOUNTER_START                EncounterID 3368, DifficultyID 208
 *   SMSG_INSTANCE_ENCOUNTER_ENGAGE_UNIT map 2952, entry 246717
 *   SMSG_BOSS_KILL                      DungeonEncounterID 3368
 *   SMSG_QUEST_UPDATE_ADD_CREDIT        map 2952, entry 246717 (quest 86636 "Void Walk With Me")
 *   SMSG_SCENARIO_COMPLETED             ScenarioID 3154
 *   SMSG_QUERY_CREATURE_RESPONSE        entry 246717 = "Lord Antenorian"
 * and DungeonEncounter.db2 id 3368 is Name "Antenorian", MapID 2952.
 *
 * OPEN ITEMS (deliberately not implemented - no evidence, so no invented behaviour)
 * --------------------------------------------------------------------------------
 *  * No combat kit. Not one spell id for Lord Antenorian, the Darkcallers or the Void Focus is
 *    recoverable from the client DB2s or from any capture on this machine, so these creatures get
 *    no ability rotation. They keep TC's default melee behaviour; nothing is faked.
 *  * "Cultists purged" (tree 214770) needs per-trash-mob credit against four weighted events with
 *    no creature->event mapping in any source. It stays unimplemented.
 *  * Step 0 "Pursue Antenorian" (event 99761) has no creature to hang off - it is almost certainly
 *    an area trigger, which no source on this machine identifies.
 *  * MOST IMPORTANTLY: map 2952 has ZERO creature and ZERO gameobject spawns in integ_world, and
 *    no spawn file for it exists anywhere in this repo or in any other world DB on this box. These
 *    scripts are correct but unreachable until the roster is imported. The 66562 capture above
 *    carries the full 167-entry creature roster with SMSG_UPDATE_OBJECT positions on map 2952 and
 *    is the obvious source for that import.
 */

#include "Creature.h"
#include "DBCEnums.h"
#include "InstanceScript.h"
#include "Log.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"

namespace
{

// Criteria.db2 Asset values (GameEvents ids) for scenario 3154's Type-92 criteria. See the step
// table in the file header for the full derivation.
enum ShadowEnclaveScenarioEvents : uint32
{
    EVENT_VOID_FOCUS_DESTROYED  = 99763,    // step order 1, criteria 107838, amount 2
    EVENT_RITUAL_STOPPED        = 99755,    // step order 2, criteria 107834, amount 3
    EVENT_ANTENORIAN_SLAIN      = 85913,    // step order 4, criteria  60399, amount 1
};

// Matches BOSS_LORD_ANTENORIAN in instance_shadow_enclave_delve.cpp (SetBossNumber(1)).
constexpr uint32 BOSS_LORD_ANTENORIAN = 0;

// creature_text GroupID for Lord Antenorian's death yell, inserted by
// sql/updates/world/master/2026_04_29_03_world.sql (CreatureID 246717, GroupID 0, Type 14 = Yell).
constexpr uint8 SAY_ANTENORIAN_DEATH = 0;

// Raises a scenario game event for every player in the delve instance. Delve maps are
// InstanceType 5 (Scenario), which is the gate CriteriaHandler applies to Type-92 criteria.
void RaiseScenarioEvent(Creature* source, uint32 gameEventId)
{
    InstanceScript* instance = source->GetInstanceScript();
    if (!instance)
    {
        TC_LOG_DEBUG("scripts.delves",
            "The Shadow Enclave: creature {} (entry {}) died outside an instance script; scenario event {} not raised.",
            source->GetGUID().ToString(), source->GetEntry(), gameEventId);
        return;
    }

    instance->DoUpdateCriteria(CriteriaType::AnyoneTriggerGameEventScenario, gameEventId, 0, source);

    TC_LOG_DEBUG("scripts.delves",
        "The Shadow Enclave: creature entry {} died, raised scenario game event {}.",
        source->GetEntry(), gameEventId);
}

// ---------------------------------------------------------------------------------------------
// Void Focus - creature 250266. Scenario 3154 step order 1 needs two of them destroyed.
// ---------------------------------------------------------------------------------------------
struct npc_void_focus_se : public ScriptedAI
{
    npc_void_focus_se(Creature* creature) : ScriptedAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        RaiseScenarioEvent(me, EVENT_VOID_FOCUS_DESTROYED);
    }
};

// ---------------------------------------------------------------------------------------------
// Darkcallers - creatures 250242 (Lysaille), 251898 (Thelamorn), 251899 (Cimberon).
// Scenario 3154 step order 2, sub-tree 221264 "Rituals stopped", needs all three.
// ---------------------------------------------------------------------------------------------
struct npc_darkcaller : public ScriptedAI
{
    npc_darkcaller(Creature* creature) : ScriptedAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        RaiseScenarioEvent(me, EVENT_RITUAL_STOPPED);
    }
};

// ---------------------------------------------------------------------------------------------
// Lord Antenorian - creature 246717, the delve's final boss (DungeonEncounter 3368).
// ---------------------------------------------------------------------------------------------
struct npc_lord_antenorian : public ScriptedAI
{
    npc_lord_antenorian(Creature* creature) : ScriptedAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_ANTENORIAN_DEATH);

        RaiseScenarioEvent(me, EVENT_ANTENORIAN_SLAIN);

        // Completion itself is handled by Delves::DelveInstanceScript::OnUnitDeath via
        // delve_template.finalBossEntry; this only closes the encounter frame.
        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetBossState(BOSS_LORD_ANTENORIAN, DONE);
    }
};

} // anonymous namespace

void AddSC_shadow_enclave_encounters()
{
    RegisterCreatureAI(npc_void_focus_se);
    RegisterCreatureAI(npc_darkcaller);
    RegisterCreatureAI(npc_lord_antenorian);
}

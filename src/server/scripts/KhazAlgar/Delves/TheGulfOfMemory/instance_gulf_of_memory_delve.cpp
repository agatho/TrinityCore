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
 * The Gulf of Memory - map 2964, difficulty 208, scenario 3177, LFGDungeons 3070, AreaTable 16595, entered from
 * Harandar (2694). Evidence: REPORT.md 2.2 / 4 / 5 and the gulf 12.1.0.69497 capture.
 *
 * Scenario 3177 steps: 16081 "Gather Junk" -> 16083 "Free Webbed Haranir" -> 16084 "Remembered horrors defeated" ->
 * 16082 "Mul'tha'ul defeated" (criteria 60399 / GameEvent 85913). The boss (250939, DungeonEncounter 3359) is NOT a
 * static spawn: his create block arrives 373 ms after step 16082 becomes current (SCENARIO_STATE 864714 -> create
 * 865087) at the position below, so he is summoned when the scenario reaches that step.
 *
 * Steps 0-2 (candle spell 1266697 x4, GameEvents 99942 x8 / 99943 x4, weighted kills 250912-250919 + 92589 x10) are
 * work item 8 and not scripted here; the base class completes the delve on the boss kill regardless.
 */

#include "delves_common.h"
#include "DB2Structure.h"
#include "Map.h"
#include "ScriptMgr.h"

namespace
{

static char const* const GulfOfMemoryScriptName = "instance_gulf_of_memory_delve";

enum GulfOfMemoryData
{
    BOSS_MULTHAUL = 0,

    MAX_ENCOUNTER
};

enum GulfOfMemoryIds : uint32
{
    NPC_MULTHAUL                  = 250939,   // "Mul'tha'ul" <Lord of the Deeps>, Elite, hp x10.0 (census)
    DUNGEON_ENCOUNTER_MULTHAUL    = 3359,     // DungeonEncounter.db2, MapID 2964 (SMSG_ENCOUNTER_START 969466, SMSG_BOSS_KILL 1062320)
    SCENARIO_STEP_MULTHAUL        = 16082,    // ScenarioStep OrderIndex 3 of 3177
};

// gulf 865087: create block of 250939, creature position quad (self-validated decode, matches 2026_08_08_41_world.sql)
Position const MulthaulSpawnPos = { -198.668f, 645.146f, 176.693f, 6.2232f };

DungeonEncounterData const encounters[] =
{
    { BOSS_MULTHAUL, {{ DUNGEON_ENCOUNTER_MULTHAUL }} }
};

class instance_gulf_of_memory_delve : public InstanceMapScript
{
public:
    instance_gulf_of_memory_delve() : InstanceMapScript(GulfOfMemoryScriptName, 2964) { }

    struct instance_gulf_of_memory_delve_InstanceScript : public Delves::DelveInstanceScript
    {
        instance_gulf_of_memory_delve_InstanceScript(InstanceMap* map)
            : DelveInstanceScript(map, 1 /* tier resolved at OnPlayerEnter from m_delveSelectedTier */)
        {
            SetBossNumber(MAX_ENCOUNTER);
            LoadDungeonEncounterData(encounters);
        }

        void OnScenarioStepChanged(ScenarioStepEntry const* step) override
        {
            if (!step || step->ID != SCENARIO_STEP_MULTHAUL || _bossSummoned)
                return;

            _bossSummoned = true;
            instance->SummonCreature(NPC_MULTHAUL, MulthaulSpawnPos);
        }

    private:
        bool _bossSummoned = false;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_gulf_of_memory_delve_InstanceScript(map);
    }
};

} // anonymous namespace

void AddSC_instance_gulf_of_memory_delve()
{
    new instance_gulf_of_memory_delve();
}

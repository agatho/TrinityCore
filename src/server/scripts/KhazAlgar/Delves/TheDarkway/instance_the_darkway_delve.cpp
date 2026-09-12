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
 * The Darkway - map 3003, difficulty 208, scenario 3184, LFGDungeons 3083, AreaTable 16642, entered from map 0.
 * Evidence: REPORT.md 2.2 / 6.5 and the shadowmoon 12.0.7.68974 capture, which ENDS INSIDE the delve during step
 * 16101 - so the boss fight, the completion and the reward objects were never observed for this map.
 *
 * Scenario 3184 steps: 16133 "Technician Mireille spoken to" (GameEvent 98477) -> 16100 "Sabotaged Ley Lines
 * examined" (GameEvent 101992) -> 16102 "Voidbreaker Oglok slain" (kill 252102) -> 16101 "Ley Line Focusers"
 * (CastSpell 1256207 x6 / GameEvent 101861 x6) -> 16103 "Infiltrator Gulkat slain" (criteria 60399 / GameEvent 85913).
 *
 * The final boss is DungeonEncounter 3361 "Infiltrator Gulkat" (DungeonEncounter.db2, MapID 3003). The world DB has
 * THREE creature_template rows of that name - 251015, 251600, 256817 - and no capture shows which one the delve
 * spawns, so delve_template.finalBossEntry stays 0 and the death of any of the three counts (IsFinalBoss override).
 * Reward object placements are unknown for this map; DelveInstanceScript falls back to the boss's death position.
 */

#include "delves_common.h"
#include "Creature.h"
#include "ScriptMgr.h"

namespace
{

static char const* const TheDarkwayScriptName = "instance_the_darkway_delve";

enum TheDarkwayData
{
    BOSS_INFILTRATOR_GULKAT = 0,

    MAX_ENCOUNTER
};

enum TheDarkwayIds : uint32
{
    DUNGEON_ENCOUNTER_INFILTRATOR_GULKAT = 3361,
};

// creature_template rows named "Infiltrator Gulkat" (tch_ws_world) - the capture never reached him
constexpr uint32 InfiltratorGulkatEntries[] = { 251015, 251600, 256817 };

DungeonEncounterData const encounters[] =
{
    { BOSS_INFILTRATOR_GULKAT, {{ DUNGEON_ENCOUNTER_INFILTRATOR_GULKAT }} }
};

class instance_the_darkway_delve : public InstanceMapScript
{
public:
    instance_the_darkway_delve() : InstanceMapScript(TheDarkwayScriptName, 3003) { }

    struct instance_the_darkway_delve_InstanceScript : public Delves::DelveInstanceScript
    {
        instance_the_darkway_delve_InstanceScript(InstanceMap* map)
            : DelveInstanceScript(map, 1 /* tier resolved at OnPlayerEnter from m_delveSelectedTier */)
        {
            SetBossNumber(MAX_ENCOUNTER);
            LoadDungeonEncounterData(encounters);
        }

        bool IsFinalBoss(Creature const* creature) const override
        {
            for (uint32 entry : InfiltratorGulkatEntries)
                if (creature->GetEntry() == entry)
                    return true;
            return false;
        }

        void OnUnitDeath(Unit* unit) override
        {
            // No boss AI is bound (entry unknown): close the encounter here so SMSG_BOSS_KILL 3361 goes out.
            if (Creature const* creature = unit ? unit->ToCreature() : nullptr)
                if (IsFinalBoss(creature) && GetBossState(BOSS_INFILTRATOR_GULKAT) != DONE)
                    SetBossState(BOSS_INFILTRATOR_GULKAT, DONE);

            DelveInstanceScript::OnUnitDeath(unit);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_the_darkway_delve_InstanceScript(map);
    }
};

} // anonymous namespace

void AddSC_instance_the_darkway_delve()
{
    new instance_the_darkway_delve();
}

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
 * The Gulf of Memory (map 2964) - Mul'tha'ul, creature 250939, DungeonEncounter 3359.
 *
 * gulf 12.1.0.69497: SMSG_ENCOUNTER_START 3359 / INSTANCE_ENCOUNTER_START / ENGAGE_UNIT at 969466 (ws 24836 = 1),
 * SMSG_INSTANCE_ENCOUNTER_DISENGAGE_UNIT + SMSG_ENCOUNTER_END + SMSG_BOSS_KILL 3359 + INSTANCE_ENCOUNTER_END at
 * 1062320 (ws 24836 = 0). BossAI + InstanceScript::SetBossState produce exactly those frames; the scenario
 * completion that follows (PROGRESS 60399 -> SCENARIO_COMPLETED 3177) is raised by DelveInstanceScript::OnUnitDeath
 * through delve_template.finalBossEntry = 250939.
 *
 * No combat kit: not one of his spells is recoverable from the client DB2s or from the capture's SPELL_START frames
 * by caster entry at this point (REPORT.md 4 lists only the creature census). He keeps default melee behaviour.
 */

#include "ScriptedCreature.h"
#include "ScriptMgr.h"

namespace
{

// Matches BOSS_MULTHAUL in instance_gulf_of_memory_delve.cpp (SetBossNumber(1)).
constexpr uint32 BOSS_MULTHAUL = 0;

struct boss_multhaul : public BossAI
{
    boss_multhaul(Creature* creature) : BossAI(creature, BOSS_MULTHAUL) { }
};

} // anonymous namespace

void AddSC_gulf_of_memory_encounters()
{
    RegisterCreatureAI(boss_multhaul);
}

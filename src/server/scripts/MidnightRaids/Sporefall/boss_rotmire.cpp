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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "sporefall.h"

// Sporefall boss: Rotmire -- DungeonEncounter 3159 (DB2 @68887, CONFIRMED).
// Creature entry + abilities are CAPTURE-BLOCKED (see sporefall.h). This stub registers a BossAI
// by ScriptName so it compiles and links; bind it via creature_template.ScriptName once the boss
// creature entry is captured from a live run. NOTE: Map 1592 is a dev-shell (DevMapE) -- data may
// not load in-world even after binding.
struct boss_rotmire : public BossAI
{
    boss_rotmire(Creature* creature) : BossAI(creature, DATA_ROTMIRE) { }

    // TODO(CAPTURE-BLOCKED): JustEngagedWith / spell scheduler / abilities from a live capture.
};

void AddSC_boss_rotmire()
{
    RegisterSporefallCreatureAI(boss_rotmire);
}

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
#include "voidscar_arena.h"

// Boss 2: Atroxus -- DungeonEncounter 3286 (DB2 @68887, CONFIRMED).
// "Imprisoned by Charonus, this starving behemoth... its sheer size is almost as dangerous as the
// poison it spews." Creature entry + abilities are CAPTURE-BLOCKED (see voidscar_arena.h).
struct boss_atroxus : public BossAI
{
    boss_atroxus(Creature* creature) : BossAI(creature, DATA_ATROXUS) { }

    // TODO(CAPTURE-BLOCKED): poison mechanic + size/positional abilities from a live capture.
};

void AddSC_boss_atroxus()
{
    RegisterVoidscarArenaCreatureAI(boss_atroxus);
}

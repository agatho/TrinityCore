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

// Boss 3 (final): Charonus, "Overseer Charonus" -- DungeonEncounter 3287 (DB2 @68887, CONFIRMED).
// "The overly confident and ostentatious owner of the Voidscar Arena... demands [Azerothians] for
// his grand collection." Creature entry + abilities are CAPTURE-BLOCKED (see voidscar_arena.h).
struct boss_charonus : public BossAI
{
    boss_charonus(Creature* creature) : BossAI(creature, DATA_CHARONUS) { }

    // TODO(CAPTURE-BLOCKED): final-encounter mechanics from a live capture.
};

void AddSC_boss_charonus()
{
    RegisterVoidscarArenaCreatureAI(boss_charonus);
}

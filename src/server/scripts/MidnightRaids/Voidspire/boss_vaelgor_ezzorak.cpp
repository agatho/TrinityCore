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
#include "the_voidspire.h"

// The Voidspire boss 3: Vaelgor & Ezzorak (council-style encounter) -- DungeonEncounter 3178
// (DB2 @68887, CONFIRMED). Creature entries + abilities are CAPTURE-BLOCKED (see the_voidspire.h).
// This stub registers a BossAI by ScriptName so it compiles and links; bind it via
// creature_template.ScriptName once the boss creature entries are captured from a live run.
// NOTE: this is a two-add council; once captured, both creature entries share DATA_VAELGOR_EZZORAK.
struct boss_vaelgor_ezzorak : public BossAI
{
    boss_vaelgor_ezzorak(Creature* creature) : BossAI(creature, DATA_VAELGOR_EZZORAK) { }

    // TODO(CAPTURE-BLOCKED): JustEngagedWith / shared-health council logic / spell scheduler.
};

void AddSC_boss_vaelgor_ezzorak()
{
    RegisterTheVoidspireCreatureAI(boss_vaelgor_ezzorak);
}

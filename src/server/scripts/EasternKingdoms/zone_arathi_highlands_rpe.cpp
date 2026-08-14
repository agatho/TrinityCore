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
#include "Player.h"
#include "QuestDef.h"
#include "Spell.h"
#include "SpellInfo.h"

/*######
## Arathi Returning Player Experience ("Catch Up"), map 2927
######*/

enum ArathiRpe
{
    // UNVERIFIED: taken from a third-party capture of retail 12.0.7.68453 and not confirmed
    // against our own data.
    MAP_ARATHI_RPE                  = 2927,
    QUEST_TO_GOSHEK_FARM            = 90883,
    NPC_CREDIT_ARATHI_RPE_MOUNT     = 239009
};

// Quest 90883 has a kill-credit objective that retail satisfies when the player mounts up:
// the capture shows SMSG_QUEST_UPDATE_ADD_CREDIT for QuestID 90883 / ObjectID 239009 with an
// empty VictimGUID right after the mount spell resolves.
//
// This deliberately does NOT live in AuraEffect::HandleAuraMounted - that runs for every mount
// application of every player on the server. Spell::_cast is the cast-completion point (the
// same moment retail sends SPELL_GO), and the checks below keep the credit confined to the RPE
// map and to a character actually on that quest.
class player_arathi_rpe_mount_credit : public PlayerScript
{
public:
    player_arathi_rpe_mount_credit() : PlayerScript("player_arathi_rpe_mount_credit") { }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (player->GetMapId() != MAP_ARATHI_RPE)
            return;

        if (player->GetQuestStatus(QUEST_TO_GOSHEK_FARM) != QUEST_STATUS_INCOMPLETE)
            return;

        if (!spell->GetSpellInfo()->HasAura(SPELL_AURA_MOUNTED))
            return;

        player->KilledMonsterCredit(NPC_CREDIT_ARATHI_RPE_MOUNT);
    }
};

void AddSC_arathi_highlands_rpe()
{
    new player_arathi_rpe_mount_credit();
}

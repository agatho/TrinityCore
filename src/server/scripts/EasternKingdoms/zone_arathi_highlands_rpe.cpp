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
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerChoice.h"
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
    NPC_CREDIT_ARATHI_RPE_MOUNT     = 239009,

    // Finale: PlayerChoice 902 is the last beat of the Catch Up experience (content branch's
    // player_choice/player_choice_response 902 rows); quest 90911 carries the player up to it.
    QUEST_ARATHI_RPE_FINALE          = 90911,
    PLAYERCHOICE_ARATHI_RPE_FINALE   = 902
};

// Faction capitals to send the player to once the Catch Up finale choice has been made. These are
// the same literal coordinates CharacterHandler.cpp::HandleCharRaceOrFactionChangeOpcode already
// uses to reset a character's homebind after a faction change (Stormwind / Orgrimmar), reused here
// rather than duplicated as a new pair of magic numbers.
enum ArathiRpeLeaveDestination
{
    MAP_EASTERN_KINGDOMS = 0,
    MAP_KALIMDOR         = 1
};

constexpr float ARATHI_RPE_LEAVE_ALLIANCE_X = -8867.68f;
constexpr float ARATHI_RPE_LEAVE_ALLIANCE_Y = 673.373f;
constexpr float ARATHI_RPE_LEAVE_ALLIANCE_Z = 97.9034f;

constexpr float ARATHI_RPE_LEAVE_HORDE_X = 1633.33f;
constexpr float ARATHI_RPE_LEAVE_HORDE_Y = -4439.11f;
constexpr float ARATHI_RPE_LEAVE_HORDE_Z = 15.7588f;

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

// PlayerChoice 902 is the RPE finale: the client sends CMSG_CHOICE_RESPONSE, QuestHandler.cpp
// resolves it against player_choice/player_choice_response and calls sScriptMgr->OnPlayerChoiceResponse,
// which dispatches by playerchoice_template.ScriptId to a PlayerChoiceScript (not a PlayerScript -
// there is no PlayerScript::OnPlayerChoiceResponse hook in this fork's ScriptMgr, only
// PlayerChoiceScript::OnResponse; see ScriptMgr.cpp ScriptMgr::OnPlayerChoiceResponse).
//
// IMPORTANT for the content branch: player_choice_template row 902 needs
// ScriptName = "playerchoice_arathi_rpe_finale" for this to fire.
class playerchoice_arathi_rpe_finale : public PlayerChoiceScript
{
public:
    playerchoice_arathi_rpe_finale() : PlayerChoiceScript("playerchoice_arathi_rpe_finale") { }

    void OnResponse(WorldObject* /*object*/, Player* player, PlayerChoice const* choice, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/) override
    {
        if (choice->ChoiceId != PLAYERCHOICE_ARATHI_RPE_FINALE)
            return;

        if (player->GetMapId() != MAP_ARATHI_RPE)
            return;

        // Retail's capture shows the finale choice answered while quest 90911 is still
        // QUEST_STATUS_INCOMPLETE (the choice widget is the quest's closing beat, not a reward of
        // turning it in elsewhere), so credit it here rather than depending on a turn-in NPC.
        if (player->GetQuestStatus(QUEST_ARATHI_RPE_FINALE) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_ARATHI_RPE_FINALE);

        // player_choice_response.RewardQuestID (authored per-response on the content branch) is
        // the destination quest for whichever option the player picked - Dragonflight 65435/65436,
        // TWW-Recap 93929, or TWW 92405 depending on faction/catch-up bucket. Core only puts
        // RewardQuestID on the wire for client display (see QuestPackets.cpp) - nothing grants it
        // server side - so do that explicitly here instead of duplicating the per-response quest
        // ids in this file.
        if (response->RewardQuestID)
        {
            if (Quest const* destination = sObjectMgr->GetQuestTemplate(*response->RewardQuestID))
            {
                if (player->CanTakeQuest(destination, false) && !player->GetQuestRewardStatus(destination->GetQuestId()))
                    player->AddQuest(destination, nullptr);
            }
            else
                TC_LOG_ERROR("scripts", "playerchoice_arathi_rpe_finale: response {} RewardQuestID {} is not a valid quest template",
                    response->ResponseId, *response->RewardQuestID);
        }

        // "Leave Catch Up": there is no in-game gossip/"Leave" affordance yet (TODO - add one, or
        // capture retail's own exit flow), so for a first playable pass just send the player home
        // to their faction capital as soon as the finale choice is made.
        if (player->GetTeamId() == TEAM_ALLIANCE)
            player->TeleportTo(MAP_EASTERN_KINGDOMS, ARATHI_RPE_LEAVE_ALLIANCE_X, ARATHI_RPE_LEAVE_ALLIANCE_Y, ARATHI_RPE_LEAVE_ALLIANCE_Z, 0.0f);
        else
            player->TeleportTo(MAP_KALIMDOR, ARATHI_RPE_LEAVE_HORDE_X, ARATHI_RPE_LEAVE_HORDE_Y, ARATHI_RPE_LEAVE_HORDE_Z, 0.0f);
    }
};

void AddSC_arathi_highlands_rpe()
{
    new player_arathi_rpe_mount_credit();
    new playerchoice_arathi_rpe_finale();
}

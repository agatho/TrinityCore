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

/* ScriptData
Name: smallactivities_commandscript
%Complete: 100
Comment: TEMPORARY DEBUG/DEV commands for the Midnight 12.0.7 small world activities.
Category: commandscripts
EndScriptData */

//
// TEMPORARY DEBUG/DEV COMMANDS — REMOVE OR RBAC-TIGHTEN WHEN REAL ACTIVATION LANDS.
//
// The real activation triggers for these activities are WORLD-DB / CAPTURE-BLOCKED
// (see the per-manager headers + C:\dumps\MIDNIGHT_SMALL_ACTIVITIES_BLUEPRINT.md):
//   - Ritual Sites: the ritual encounter + its quests are world-DB; this stands in
//     for a completed site and fires the DB2-anchored renown/rep/title grant.
//   - Abyss Anglers: the underwater dive scenario is capture-blocked; this stands in
//     for a completed dive and fires the DB2-anchored Angler Pearls payout.
//   - Darkspear Dash: the race + quests + game_event are world-DB; this stands in for
//     finishing the race and grants the DB2-anchored "Darkspear Dasher" title.
//
// All commands are gated behind RBAC_PERM_COMMAND_DEBUG (GM/dev only) and are hard
// no-ops unless the owning activity is enabled (its table is seeded). See the managers.
//
//   .ritualsite complete            -> RitualSiteMgr::CompleteRitualSite
//   .abyssangler reward [pearls]    -> AbyssAnglersMgr::AwardDiveReward
//   .darkspeardash title            -> DarkspearDashMgr::GrantDasherTitle
//

#include "ScriptMgr.h"
#include "AbyssAnglersMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DarkspearDashMgr.h"
#include "Language.h"
#include "Player.h"
#include "RBAC.h"
#include "RitualSiteMgr.h"
#include "WorldSession.h"
#include <optional>

using namespace Trinity::ChatCommands;

class smallactivities_commandscript : public CommandScript
{
public:
    smallactivities_commandscript() : CommandScript("smallactivities_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable ritualSiteTable =
        {
            { "complete", HandleRitualSiteCompleteCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };
        static ChatCommandTable abyssAnglerTable =
        {
            { "reward", HandleAbyssAnglerRewardCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };
        static ChatCommandTable darkspearDashTable =
        {
            { "title", HandleDarkspearDashTitleCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "ritualsite",   ritualSiteTable   },
            { "abyssangler",  abyssAnglerTable  },
            { "darkspeardash", darkspearDashTable },
        };
        return commandTable;
    }

    static Player* SelectTarget(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
        }
        return target;
    }

    // .ritualsite complete — DEBUG stand-in for a completed Ritual Site.
    static bool HandleRitualSiteCompleteCommand(ChatHandler* handler)
    {
        Player* target = SelectTarget(handler);
        if (!target)
            return false;

        if (!sRitualSiteMgr->IsEnabled())
        {
            handler->SendSysMessage("Ritual Sites are idle (ritual_site_template not seeded).");
            return true;
        }

        sRitualSiteMgr->CompleteRitualSite(target);
        handler->PSendSysMessage("Ritual Site completed for {} (renown/rep/title grant fired).", target->GetName());
        return true;
    }

    // .abyssangler reward [pearls] — DEBUG stand-in for a completed dive.
    static bool HandleAbyssAnglerRewardCommand(ChatHandler* handler, std::optional<uint32> pearls)
    {
        Player* target = SelectTarget(handler);
        if (!target)
            return false;

        if (!sAbyssAnglersMgr->IsEnabled())
        {
            handler->SendSysMessage("Abyss Anglers are idle (abyss_angler_dive_template not seeded).");
            return true;
        }

        sAbyssAnglersMgr->AwardDiveReward(target, pearls.value_or(0));
        handler->PSendSysMessage("Abyss Anglers dive reward paid to {}.", target->GetName());
        return true;
    }

    // .darkspeardash title — DEBUG stand-in for finishing the race.
    static bool HandleDarkspearDashTitleCommand(ChatHandler* handler)
    {
        Player* target = SelectTarget(handler);
        if (!target)
            return false;

        sDarkspearDashMgr->GrantDasherTitle(target);
        handler->PSendSysMessage("Granted 'Darkspear Dasher' title to {} (RESEARCH-ONLY skeleton).", target->GetName());
        return true;
    }
};

void AddSC_smallactivities_commandscript()
{
    new smallactivities_commandscript();
}

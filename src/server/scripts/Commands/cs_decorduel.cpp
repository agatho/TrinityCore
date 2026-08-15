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
Name: decorduel_commandscript
%Complete: 100
Comment: Debug driver for the Decor Duels (#12) housing minigame SEAM. Exercises
         the achievement-crediting seam; the round wire itself is CAPTURE-BLOCKED.
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DecorDuelMgr.h"
#include "Language.h"
#include "Player.h"
#include "RBAC.h"

using namespace Trinity::ChatCommands;

class decorduel_commandscript : public CommandScript
{
public:
    decorduel_commandscript() : CommandScript("decorduel_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable decorduelCommandTable =
        {
            { "status", HandleDecorDuelStatusCommand, rbac::RBAC_PERM_COMMAND_ACHIEVEMENT_ADD, Console::Yes },
            { "credit", HandleDecorDuelCreditCommand, rbac::RBAC_PERM_COMMAND_ACHIEVEMENT_ADD, Console::No  },
        };
        static ChatCommandTable commandTable =
        {
            { "decorduel", decorduelCommandTable },
        };
        return commandTable;
    }

    // .decorduel status — report the seam state (enabled/disabled + CAPTURE-BLOCKED map id).
    static bool HandleDecorDuelStatusCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("Decor Duels seam: {} (minigame map id {} — 0 = CAPTURE-BLOCKED).",
            sDecorDuelMgr.IsEnabled() ? "ENABLED" : "disabled (scaffolding only)",
            sDecorDuelMgr.GetMinigameMapId());
        handler->SendSysMessage("Round wire / roles' spells / map id are CAPTURE-BLOCKED. The achievement-crediting "
            "seam (category 15574) is available via '.decorduel credit <achievementId>'.");
        return true;
    }

    // .decorduel credit <achievementId> — drive the achievement seam onto the selected player.
    static bool HandleDecorDuelCreditCommand(ChatHandler* handler, uint32 achievementId)
    {
        Player* target = handler->getSelectedPlayer();
        if (!target)
            target = handler->GetPlayer();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sDecorDuelMgr.IsDecorDuelAchievement(achievementId))
        {
            handler->PSendSysMessage("{} is not a Decor Duel (category 15574) achievement "
                "(expected 61792, 61793, or 61878-61887).", achievementId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sDecorDuelMgr.CreditAchievement(target, achievementId);
        handler->PSendSysMessage("Decor Duel achievement {} credited to {}.", achievementId, target->GetName());
        return true;
    }
};

void AddSC_decorduel_commandscript()
{
    new decorduel_commandscript();
}

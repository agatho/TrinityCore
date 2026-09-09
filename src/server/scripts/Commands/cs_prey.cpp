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
Name: prey_commandscript
%Complete: 100
Comment: TEMPORARY DEBUG/DEV command for the Midnight S1 Prey/Voidforge economy slice.
Category: commandscripts
EndScriptData */

//
// TEMPORARY DEBUG/DEV COMMAND — REMOVE OR RBAC-TIGHTEN WHEN REAL ACTIVATION LANDS.
//
// The real hunt-activation trigger (Hunt Table opcode, npc 245824) is CAPTURE-BLOCKED
// (blueprint §7 ask #1). This command stands in for it so the whole reward/renown chain
// (GrantJourneyProgress + CompleteHunt) is exercisable in-game on a disposable test DB.
// It is gated behind RBAC_PERM_COMMAND_DEBUG (GM/dev only) and is a hard no-op unless the
// Prey system is enabled (prey_hunt_template seeded). See PreyMgr for the grant mechanism.
//
//   .prey grant <normal|hard|nightmare>   (aliases: n / h / nm, or 0 / 1 / 2)
//

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Language.h"
#include "Player.h"
#include "PreyMgr.h"
#include "RBAC.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>

using namespace Trinity::ChatCommands;

class prey_commandscript : public CommandScript
{
public:
    prey_commandscript() : CommandScript("prey_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable preyCommandTable =
        {
            { "grant", HandlePreyGrantCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "prey", preyCommandTable },
        };
        return commandTable;
    }

    // .prey grant <difficulty>  — DEBUG stand-in for the capture-blocked Hunt Table.
    static bool HandlePreyGrantCommand(ChatHandler* handler, std::string difficultyStr)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string arg = difficultyStr;
        std::transform(arg.begin(), arg.end(), arg.begin(), [](unsigned char c) { return char(std::tolower(c)); });

        PreyDifficulty difficulty;
        if (arg == "normal" || arg == "n" || arg == "0")
            difficulty = PreyDifficulty::Normal;
        else if (arg == "hard" || arg == "h" || arg == "heroic" || arg == "1")
            difficulty = PreyDifficulty::Hard;
        else if (arg == "nightmare" || arg == "nm" || arg == "2")
            difficulty = PreyDifficulty::Nightmare;
        else
        {
            handler->SendSysMessage("Usage: .prey grant <normal|hard|nightmare>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sPreyMgr->IsEnabled())
        {
            handler->SendSysMessage("Prey system is DISABLED (prey_hunt_template absent or empty) - no rewards granted.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Drive the full chain, same order the real completion wire would:
        // Journey progress (currency 3387 + faction-2764 renown) then the direct
        // per-difficulty rewards (Dawncrests, + Nightmare Nebulous Voidcore) and the
        // weekly-state record. Amounts are PLACEHOLDER (TODO CAPTURE-BLOCKED).
        sPreyMgr->GrantJourneyProgress(target, difficulty);
        sPreyMgr->CompleteHunt(target, difficulty);

        handler->PSendSysMessage("[DEBUG] Prey rewards granted to %s at difficulty '%s' (PLACEHOLDER amounts).",
            target->GetName().c_str(), arg.c_str());
        return true;
    }
};

void AddSC_prey_commandscript()
{
    new prey_commandscript();
}

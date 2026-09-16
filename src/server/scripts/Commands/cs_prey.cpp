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
Comment: Inspect and redraw the weekly Prey hunt rotation
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "PreyMgr.h"
#include "RBAC.h"
#include <algorithm>

using namespace Trinity::ChatCommands;

class prey_commandscript : public CommandScript
{
public:
    prey_commandscript() : CommandScript("prey_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable preyCommandTable =
        {
            { "rotation", HandlePreyRotationCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
            { "rotate",   HandlePreyRotateCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "prey", preyCommandTable },
        };
        return commandTable;
    }

    // .prey rotation - the hunts on the Hunt Table this week, by slot
    static bool HandlePreyRotationCommand(ChatHandler* handler)
    {
        std::vector<std::pair<int32, PreyHuntTarget const*>> active;
        for (PreyHuntTarget const& target : sPreyMgr->GetTargets())
            if (int32 slot = sPreyMgr->GetSlot(target))
                active.emplace_back(slot, &target);

        std::ranges::sort(active, [](auto const& left, auto const& right) { return std::tie(left.first, left.second->WorldStateId) < std::tie(right.first, right.second->WorldStateId); });

        handler->PSendSysMessage("Prey rotation: %u hunt targets known, %u active.", uint32(sPreyMgr->GetTargets().size()), uint32(active.size()));
        for (auto const& [slot, target] : active)
            handler->PSendSysMessage("  slot %d: %s (world state %d%s)", slot, target->Name.c_str(), target->WorldStateId, target->IsSpecial ? ", special" : "");

        return true;
    }

    // .prey rotate - draw a new weekly rotation now
    static bool HandlePreyRotateCommand(ChatHandler* handler)
    {
        if (sPreyMgr->GetTargets().empty())
        {
            handler->SendSysMessage("No Prey hunt targets are loaded.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sPreyMgr->Rotate();
        return HandlePreyRotationCommand(handler);
    }
};

void AddSC_prey_commandscript()
{
    new prey_commandscript();
}

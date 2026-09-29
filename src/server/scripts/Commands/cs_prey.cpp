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
Comment: Inspect and redraw the weekly Prey hunt rotation, inspect and drive a running hunt
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Player.h"
#include "PreyHunt.h"
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
            { "hunt",     HandlePreyHuntCommand,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "progress", HandlePreyProgressCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "reveal",   HandlePreyRevealCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "ambush",   HandlePreyAmbushCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
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
    static Player* GetHuntingPlayer(ChatHandler* handler, PreyHunt const*& hunt)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        hunt = player ? Prey::GetActiveHunt(player) : nullptr;
        if (!hunt)
        {
            handler->SendSysMessage("The selected player has no Prey hunt in the quest log.");
            handler->SetSentErrorMessage(true);
            return nullptr;
        }

        return player;
    }

    static int64 GetElement(Player const* player, uint32 element)
    {
        return std::visit([](auto value) { return int64(value); }, player->GetDataElementCharacter(element));
    }

    // .prey hunt - the selected player's running hunt
    static bool HandlePreyHuntCommand(ChatHandler* handler)
    {
        PreyHunt const* hunt = nullptr;
        Player* player = GetHuntingPlayer(handler, hunt);
        if (!player)
            return false;

        PreyHuntZone const* zone = Prey::GetHuntZone(player);
        handler->PSendSysMessage("Prey hunt: quest %u, %s (target %u, difficulty %u), prey creature %u",
            hunt->QuestId, hunt->Target->Name.c_str(), uint32(hunt->Target->TargetIndex), uint32(AsUnderlyingType(hunt->Difficulty)), hunt->PreyEntry);
        handler->PSendSysMessage("  zone %s (%s), state %u, progress %u / %u (warm %u, hot %u)",
            zone ? zone->Name.c_str() : "<none>", Prey::IsInHuntZone(player) ? "inside" : "outside", uint32(Prey::GetHuntState(player)),
            uint32(GetElement(player, Prey::ELEMENT_PROGRESS)), uint32(GetElement(player, Prey::ELEMENT_FINAL_THRESHOLD)),
            uint32(GetElement(player, Prey::ELEMENT_WARM_THRESHOLD)), uint32(GetElement(player, Prey::ELEMENT_HOT_THRESHOLD)));
        return true;
    }

    // .prey progress #amount - adds hunt progress as an ambush or world quest would
    static bool HandlePreyProgressCommand(ChatHandler* handler, int32 amount)
    {
        PreyHunt const* hunt = nullptr;
        Player* player = GetHuntingPlayer(handler, hunt);
        if (!player)
            return false;

        Prey::AddProgress(player, amount);
        return HandlePreyHuntCommand(handler);
    }

    // .prey reveal - turns the hunt final
    static bool HandlePreyRevealCommand(ChatHandler* handler)
    {
        PreyHunt const* hunt = nullptr;
        Player* player = GetHuntingPlayer(handler, hunt);
        if (!player)
            return false;

        Prey::RevealPrey(player, true);
        return HandlePreyHuntCommand(handler);
    }

    // .prey ambush - ambushes the selected player now
    static bool HandlePreyAmbushCommand(ChatHandler* handler)
    {
        PreyHunt const* hunt = nullptr;
        Player* player = GetHuntingPlayer(handler, hunt);
        if (!player)
            return false;

        player->RemoveAurasDueToSpell(Prey::SPELL_AMBUSH_BLOCKER);
        player->CastSpell(player, Prey::SPELL_SUMMON_HUNTED_REMNANT, true);
        player->CastSpell(player, Prey::SPELL_ACTIVE_AMBUSH, true);
        player->CastSpell(player, Prey::SPELL_AMBUSH, true);
        return true;
    }
};

void AddSC_prey_commandscript()
{
    new prey_commandscript();
}

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
Name: postal_commandscript
%Complete: 100
Comment: TEMP GM debug driver for the "Going Postal" (housing) mail-race minigame.
         Starts/advances/completes a race for testing while the real in-world
         completion wire (checkpoint coords / "[DNT] Postal Race Complete" spell)
         is CAPTURE-BLOCKED. RBAC_PERM_COMMAND_DEBUG.
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "GoingPostalMgr.h"
#include "Language.h"
#include "Player.h"
#include "RBAC.h"

using namespace Trinity::ChatCommands;

class postal_commandscript : public CommandScript
{
public:
    postal_commandscript() : CommandScript("postal_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable postalCommandTable =
        {
            { "status",     HandlePostalStatusCommand,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
            { "routes",     HandlePostalRoutesCommand,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "start",      HandlePostalStartCommand,      rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "checkpoint", HandlePostalCheckpointCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "complete",   HandlePostalCompleteCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "abandon",    HandlePostalAbandonCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
        };
        static ChatCommandTable commandTable =
        {
            { "postal", postalCommandTable },
        };
        return commandTable;
    }

    static Player* SelfOrSelected(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayer();
        if (!target)
            target = handler->GetPlayer();
        return target;
    }

    // .postal status — manager enable state + route count.
    static bool HandlePostalStatusCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("Going Postal: {} — {} route(s) loaded.",
            sGoingPostalMgr.IsEnabled() ? "ENABLED" : "disabled",
            sGoingPostalMgr.GetRouteCount());
        handler->SendSysMessage("Checkpoint coords + in-world completion wire are CAPTURE-BLOCKED; use "
            "'.postal start <1-3>' then '.postal complete [elapsedMs]' to drive a race.");
        return true;
    }

    // .postal routes — list the target's faction routes + personal bests.
    static bool HandlePostalRoutesCommand(ChatHandler* handler)
    {
        Player* target = SelfOrSelected(handler);
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        GoingPostalTeam const team = GoingPostalMgr::GetPlayerTeam(target);
        auto const routes = sGoingPostalMgr.GetRoutesForTeam(team);
        if (routes.empty())
        {
            handler->PSendSysMessage("No Going Postal routes seeded for {}'s faction.", target->GetName());
            return true;
        }

        for (GoingPostalRoute const* route : routes)
        {
            std::optional<uint32> const best = sGoingPostalMgr.GetPersonalBest(target->GetGUID(), route->id);
            handler->PSendSysMessage("  route {} (id {}, currency {}): best {} ms{}.",
                route->routeIndex, route->id, route->currencyId,
                best ? std::to_string(*best) : std::string("none"),
                route->HasCheckpoints() ? "" : " [no checkpoints — coords CAPTURE-BLOCKED]");
        }
        return true;
    }

    // .postal start <routeIndex 1-3> — begin a race for the target.
    static bool HandlePostalStartCommand(ChatHandler* handler, uint8 routeIndex)
    {
        Player* target = SelfOrSelected(handler);
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        GoingPostalTeam const team = GoingPostalMgr::GetPlayerTeam(target);
        if (sGoingPostalMgr.StartRace(target, team, routeIndex))
            handler->PSendSysMessage("Going Postal: started route {} for {}.", routeIndex, target->GetName());
        else
        {
            handler->PSendSysMessage("Going Postal: could not start route {} for {} (unknown/unavailable route).",
                routeIndex, target->GetName());
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    // .postal checkpoint — force-advance the next checkpoint (auto-completes on last).
    static bool HandlePostalCheckpointCommand(ChatHandler* handler)
    {
        Player* target = SelfOrSelected(handler);
        if (!target || !sGoingPostalMgr.IsRacing(target))
        {
            handler->SendSysMessage("Going Postal: target is not racing.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (sGoingPostalMgr.ForceAdvanceCheckpoint(target))
            handler->SendSysMessage("Going Postal: advanced one checkpoint.");
        else
            handler->SendSysMessage("Going Postal: no checkpoints to advance (coords CAPTURE-BLOCKED). "
                "Use '.postal complete' to finish.");
        return true;
    }

    // .postal complete [elapsedMs] — finish the race; optional deterministic time.
    static bool HandlePostalCompleteCommand(ChatHandler* handler, Optional<uint32> elapsedMs)
    {
        Player* target = SelfOrSelected(handler);
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        GoingPostalResult const result = sGoingPostalMgr.CompleteRace(target,
            elapsedMs ? std::optional<uint32>(*elapsedMs) : std::nullopt);

        switch (result)
        {
            case GoingPostalResult::RecordedBest:
                handler->PSendSysMessage("Going Postal: {} set a NEW personal best (currency granted).", target->GetName());
                break;
            case GoingPostalResult::Completed:
                handler->PSendSysMessage("Going Postal: {} finished but did not beat their best.", target->GetName());
                break;
            case GoingPostalResult::NotRacing:
                handler->SendSysMessage("Going Postal: target is not racing.");
                handler->SetSentErrorMessage(true);
                return false;
            case GoingPostalResult::Disabled:
            default:
                handler->SendSysMessage("Going Postal: route unavailable.");
                handler->SetSentErrorMessage(true);
                return false;
        }
        return true;
    }

    // .postal abandon — cancel the active race without recording.
    static bool HandlePostalAbandonCommand(ChatHandler* handler)
    {
        Player* target = SelfOrSelected(handler);
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }
        sGoingPostalMgr.AbandonRace(target);
        handler->PSendSysMessage("Going Postal: race abandoned for {}.", target->GetName());
        return true;
    }
};

void AddSC_postal_commandscript()
{
    new postal_commandscript();
}

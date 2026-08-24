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
 */

/* ScriptData
Name: delvenemesis_commandscript
%Complete: 100
Comment: TEMPORARY DEBUG/DEV command for the Midnight Delve Nemesis layer.
Category: commandscripts
EndScriptData */

//
// TEMPORARY DEBUG/DEV COMMAND - REMOVE OR RBAC-TIGHTEN AT INTEGRATION.
//
// On this baseline branch the Nemesis seam (OnDelveStarted / OnPactswornKilled /
// OnDelveCompleted) has no caller, because the delve manager/instance/rewards code
// lives only on feature/delves. This command drives the seam directly so the whole
// Nemesis loop - Pactsworn spawn spine, per-run kill counter, Nemesis Strongbox
// quality scaling, and the Nullaeus solo-challenge achievement tail (61797/61798/
// 61799) - is exercisable in-game on a disposable test DB, with zero capture-blocked
// wire. Gated behind RBAC_PERM_COMMAND_DEBUG (GM/dev only). At integration this file
// is deleted and the seam is wired to the real delve code (see NemesisMgr.h banner).
//
//   .delvenemesis start <tier>        - OnDelveStarted on your current map (spawns Pactsworn if tier>=4)
//   .delvenemesis pactsworn           - OnPactswornKilled (increments this run's counter)
//   .delvenemesis complete <tier>     - OnDelveCompleted on your current map (Strongbox + Nullaeus if map 2966)
//   .delvenemesis nullaeus <tier> <solo 0|1> - OnNullaeusDefeated directly (test the solo achievements)
//   .delvenemesis status              - report this run's Pactsworn kill count
//

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Language.h"
#include "Map.h"
#include "NemesisMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

class delvenemesis_commandscript : public CommandScript
{
public:
    delvenemesis_commandscript() : CommandScript("delvenemesis_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable nemesisCommandTable =
        {
            { "start",     HandleStartCommand,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "pactsworn", HandlePactswornCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "complete",  HandleCompleteCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "nullaeus",  HandleNullaeusCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "status",    HandleStatusCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "delvenemesis", nemesisCommandTable },
        };
        return commandTable;
    }

    static bool HandleStartCommand(ChatHandler* handler, uint8 tier)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target || !target->GetMap())
            return false;

        sNemesisMgr->OnDelveStarted(target->GetMap(), tier);
        handler->PSendSysMessage("[DEBUG] Nemesis: delve started on map %u (instance %u) at tier %u.",
            target->GetMap()->GetId(), target->GetMap()->GetInstanceId(), uint32(tier));
        return true;
    }

    static bool HandlePactswornCommand(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target || !target->GetMap())
            return false;

        sNemesisMgr->OnPactswornKilled(target);
        handler->PSendSysMessage("[DEBUG] Nemesis: Pactsworn killed. Run total now %u.",
            sNemesisMgr->GetPactswornKills(target->GetMap()->GetInstanceId()));
        return true;
    }

    static bool HandleCompleteCommand(ChatHandler* handler, uint8 tier)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target || !target->GetMap())
            return false;

        bool isSolo = !target->GetGroup();
        sNemesisMgr->OnDelveCompleted(target, tier, target->GetMap()->GetId(), isSolo);
        handler->PSendSysMessage("[DEBUG] Nemesis: delve completed (map %u, tier %u, solo %s).",
            target->GetMap()->GetId(), uint32(tier), isSolo ? "yes" : "no");
        return true;
    }

    static bool HandleNullaeusCommand(ChatHandler* handler, uint8 tier, uint8 solo)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;

        sNemesisMgr->OnNullaeusDefeated(target, tier, solo != 0);
        handler->PSendSysMessage("[DEBUG] Nemesis: Nullaeus defeated (tier %u, solo %s). Achievements resolved.",
            uint32(tier), solo ? "yes" : "no");
        return true;
    }

    static bool HandleStatusCommand(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target || !target->GetMap())
            return false;

        handler->PSendSysMessage("[DEBUG] Nemesis: instance %u Pactsworn kills = %u (Torment's Rise map = %s).",
            target->GetMap()->GetInstanceId(), sNemesisMgr->GetPactswornKills(target->GetMap()->GetInstanceId()),
            sNemesisMgr->IsTormentsRiseMap(target->GetMap()->GetId()) ? "yes" : "no");
        return true;
    }
};

void AddSC_delvenemesis_commandscript()
{
    new delvenemesis_commandscript();
}

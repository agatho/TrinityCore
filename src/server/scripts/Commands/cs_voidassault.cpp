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
Name: voidassault_commandscript
%Complete: 100
Comment: TEMPORARY DEBUG/DEV command for the Midnight Void Assaults core loop.
Category: commandscripts
EndScriptData */

//
// TEMPORARY DEBUG/DEV COMMAND — REMOVE OR RBAC-TIGHTEN WHEN REAL ACTIVATION LANDS.
//
// The real Void Assault activation trigger (the open-world Void Strike POI / portal-world
// window) is CAPTURE-BLOCKED (blueprint §7 asks #1 & #3). This command stands in for it so
// the whole core loop — assault meter WS 29616 (+500/step), the cap flip to the Void
// Incursion scenario phase (3173/3174), and the Field Accolade 3405 / Voidlight Marl 3316
// reward tail — is exercisable in-game on a disposable test DB, without any capture-blocked
// wire.
//
// It is gated behind RBAC_PERM_COMMAND_DEBUG (GM/dev only) and is a hard no-op unless the
// Void Assault system is enabled (void_assault_template seeded). See VoidAssaultMgr for the
// meter/scenario/reward mechanism. At integration this command is deleted; the two entry
// points it drives (OnVoidStrikeCompleted / CompleteAssault) fold onto ZoneEventMgr.
//
//   .voidassault strike     — one Void Strike: +500 to meter 29616, flip to Incursion at cap
//   .voidassault complete   — grant the completion reward (Field Accolade + Voidlight Marl)
//   .voidassault status     — report the primary assault's meter / phase / escalation state
//

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Language.h"
#include "Player.h"
#include "RBAC.h"
#include "VoidAssaultMgr.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

class voidassault_commandscript : public CommandScript
{
public:
    voidassault_commandscript() : CommandScript("voidassault_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable voidAssaultCommandTable =
        {
            { "strike",   HandleVoidAssaultStrikeCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "complete", HandleVoidAssaultCompleteCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "status",   HandleVoidAssaultStatusCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "voidassault", voidAssaultCommandTable },
        };
        return commandTable;
    }

    // .voidassault strike — DEBUG stand-in for a completed Void Strike world-objective.
    static bool HandleVoidAssaultStrikeCommand(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sVoidAssaultMgr->IsEnabled())
        {
            handler->SendSysMessage("Void Assault system is DISABLED (void_assault_template absent or empty) - no-op.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Stand in for the capture-blocked activation wire: force the primary assault
        // active off its weekly timer, then feed one Void Strike into the meter.
        sVoidAssaultMgr->DebugForceActivatePrimary();
        sVoidAssaultMgr->OnVoidStrikeCompleted(target);

        uint32 const id = sVoidAssaultMgr->GetPrimaryAssaultId();
        handler->PSendSysMessage("[DEBUG] Void Strike applied. Assault %u meter now %d (WS 29616).",
            id, sVoidAssaultMgr->GetMeter(id));
        return true;
    }

    // .voidassault complete — DEBUG stand-in for a completed Void Incursion.
    static bool HandleVoidAssaultCompleteCommand(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sVoidAssaultMgr->IsEnabled())
        {
            handler->SendSysMessage("Void Assault system is DISABLED (void_assault_template absent or empty) - no rewards granted.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sVoidAssaultMgr->CompleteAssault(target);

        handler->PSendSysMessage("[DEBUG] Void Incursion completed for %s: granted Field Accolade 3405 + Voidlight Marl 3316 (PLACEHOLDER amounts).",
            target->GetName().c_str());
        return true;
    }

    // .voidassault status — report the primary assault's live meter / phase / escalation.
    static bool HandleVoidAssaultStatusCommand(ChatHandler* handler)
    {
        if (!sVoidAssaultMgr->IsEnabled())
        {
            handler->SendSysMessage("Void Assault system is DISABLED (void_assault_template absent or empty).");
            return true;
        }

        uint32 const id = sVoidAssaultMgr->GetPrimaryAssaultId();
        if (!id)
        {
            handler->SendSysMessage("Void Assault system enabled but no meter-driven assault is configured.");
            return true;
        }

        handler->PSendSysMessage("[DEBUG] Void Assault %u: active=%s meter=%d (WS 29616).",
            id, sVoidAssaultMgr->IsAssaultActive(id) ? "yes" : "no", sVoidAssaultMgr->GetMeter(id));
        return true;
    }
};

void AddSC_voidassault_commandscript()
{
    new voidassault_commandscript();
}

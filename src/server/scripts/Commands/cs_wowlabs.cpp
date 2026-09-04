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

// Plunderstorm / WoW Labs GM commands (design: scratchpad plunderstorm_design.md, P3).
//
// The live client reaches a WoW Labs match by the fast-login reconnect handshake, which can only be exercised
// against the real client. `.wowlabs enter` is the server-verifiable path: it reserves a solo match and drops
// the caller straight onto their own instance of MAP_WOWLABS, so the map factory / instance plumbing (P3) can
// be tested on the running realm without the client handoff. `.wowlabs leave` clears the binding and sends the
// player home.

#include "Chat.h"
#include "ChatCommand.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "WowLabsMatchMgr.h"
#include <vector>

using namespace Trinity::ChatCommands;

class wowlabs_commandscript : public CommandScript
{
public:
    wowlabs_commandscript() : CommandScript("wowlabs_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable wowlabsCommandTable =
        {
            { "enter",   HandleWowLabsEnterCommand,   rbac::RBAC_PERM_COMMAND_GO, Console::No },
            { "join",    HandleWowLabsJoinCommand,    rbac::RBAC_PERM_COMMAND_GO, Console::No },
            { "leave",   HandleWowLabsLeaveCommand,   rbac::RBAC_PERM_COMMAND_GO, Console::No },
            { "status",  HandleWowLabsStatusCommand,  rbac::RBAC_PERM_COMMAND_GO, Console::No },
            { "ability", HandleWowLabsAbilityCommand, rbac::RBAC_PERM_COMMAND_GO, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "wowlabs", wowlabsCommandTable },
        };
        return commandTable;
    }

    static bool HandleWowLabsEnterCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        // Reserve a solo test match for this account and bind the player to its instance so MapManager::CreateMap
        // gives them their own instance of MAP_WOWLABS.
        std::vector<WowLabsMatchMgr::MatchMember> roster = { { player->GetSession()->GetBattlenetAccountGUID(), player->GetName() } };
        WowLabsMatchMgr::Match* match = sWowLabsMatchMgr->CreateMatch(std::move(roster), 0 /*Solo*/);
        player->SetWowLabsInstanceId(match->InstanceId);

        // Drop onto the real map: the "Circle of Inner Binding" AreaPOI on map 2695, the storm's final-ring
        // centre (needs map 2695's terrain extracted from the client for the ground to be there).
        if (!player->TeleportTo(WowLabsMatchMgr::MAP_ID, -1527.48f, -2165.09f, 17.37f, 0.0f, TELE_TO_NONE, match->InstanceId))
        {
            player->SetWowLabsInstanceId(0);
            sWowLabsMatchMgr->RemoveMatch(match->Id);
            handler->PSendSysMessage("WoW Labs: teleport to map {} failed - are the map data files for it extracted?", WowLabsMatchMgr::MAP_ID);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("WoW Labs: entering match {} (instance {}) on map {}.", match->Id, match->InstanceId, WowLabsMatchMgr::MAP_ID);
        return true;
    }

    static bool HandleWowLabsJoinCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        WowLabsMatchMgr::Match* match = sWowLabsMatchMgr->GetNewestJoinableMatch();
        if (!match)
        {
            handler->PSendSysMessage("WoW Labs: no open match to join - use .wowlabs enter to create one.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sWowLabsMatchMgr->AddMemberToMatch(match, player->GetSession()->GetBattlenetAccountGUID(), player->GetName());
        player->SetWowLabsInstanceId(match->InstanceId);

        if (!player->TeleportTo(WowLabsMatchMgr::MAP_ID, -1527.48f, -2165.09f, 17.37f, 0.0f, TELE_TO_NONE, match->InstanceId))
        {
            player->SetWowLabsInstanceId(0);
            handler->PSendSysMessage("WoW Labs: teleport to map {} failed - are the map data files for it extracted?", WowLabsMatchMgr::MAP_ID);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("WoW Labs: joined match {} (instance {}) on map {}.", match->Id, match->InstanceId, WowLabsMatchMgr::MAP_ID);
        return true;
    }

    static bool HandleWowLabsLeaveCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (WowLabsMatchMgr::Match* match = sWowLabsMatchMgr->FindByInstanceId(player->GetWowLabsInstanceId()))
        {
            sWowLabsMatchMgr->ClearMatchAbilities(player, match);   // strip picked-up abilities before leaving
            sWowLabsMatchMgr->RemoveMatch(match->Id);
        }

        player->SetWowLabsInstanceId(0);

        // Home the player back to their bind point, off the match map.
        player->TeleportTo(player->m_homebind);
        handler->PSendSysMessage("WoW Labs: left the match, returning home.");
        return true;
    }

    static bool HandleWowLabsStatusCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        WowLabsMatchMgr::Match* match = sWowLabsMatchMgr->FindByInstanceId(player->GetWowLabsInstanceId());
        if (!match)
        {
            handler->PSendSysMessage("WoW Labs: not in a match.");
            return true;
        }

        char const* phase = "reserved";
        switch (match->MatchPhase)
        {
            case WowLabsMatchMgr::Phase::Prematch: phase = "prematch"; break;
            case WowLabsMatchMgr::Phase::Active:   phase = "active";   break;
            case WowLabsMatchMgr::Phase::Ended:    phase = "ended";    break;
            default: break;
        }

        uint64 const key = player->GetGUID().GetCounter();
        uint32 const level = match->Level.count(key) ? match->Level[key] : 1;
        uint32 const kills = match->Kills.count(key) ? match->Kills[key] : 0;
        uint32 const plunder = match->PlunderEarned.count(key) ? match->PlunderEarned[key] : 0;
        handler->PSendSysMessage("WoW Labs match {} (instance {}): phase {}, active {}s.",
            match->Id, match->InstanceId, phase, match->ActiveElapsedMs / 1000);
        handler->PSendSysMessage("  you: level {}, {} kills, {} Plunder banked (paid at match end).",
            level ? level : 1, kills, plunder);

        float cx, cy, radius;
        if (sWowLabsMatchMgr->ComputeCircle(match, cx, cy, radius))
        {
            float const dist = player->GetDistance2d(cx, cy);
            handler->PSendSysMessage("  circle: centre ({:.0f}, {:.0f}) radius {:.0f}; you are {:.0f} away ({}).",
                cx, cy, radius, dist, dist > radius ? "OUTSIDE - taking storm damage" : "inside");
        }
        return true;
    }

    // .wowlabs ability [id] - pick up an ability (a specific pool id, or a random one). Repeat the same id to
    // watch it stack up a rank. Mirrors the retail loot-pickup / stack-to-upgrade mechanic.
    static bool HandleWowLabsAbilityCommand(ChatHandler* handler, Optional<uint32> abilityId)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        WowLabsMatchMgr::Match* match = sWowLabsMatchMgr->FindByInstanceId(player->GetWowLabsInstanceId());
        if (!match)
        {
            handler->PSendSysMessage("WoW Labs: not in a match - use .wowlabs enter first.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (abilityId)
        {
            uint8 const rank = sWowLabsMatchMgr->GrantAbility(player, match, *abilityId);
            if (!rank)
            {
                handler->PSendSysMessage("WoW Labs: no ability with id {} in the pool.", *abilityId);
                handler->SetSentErrorMessage(true);
                return false;
            }
            WowLabsMatchMgr::AbilityDef const* def = sWowLabsMatchMgr->FindAbility(*abilityId);
            handler->PSendSysMessage("WoW Labs: {} is now rank {}/{}.", def ? def->Name : "ability", rank, WowLabsMatchMgr::ABILITY_RANKS);
        }
        else
        {
            sWowLabsMatchMgr->GrantRandomAbility(player, match);
            handler->PSendSysMessage("WoW Labs: picked up a random ability. Pool ids:");
            for (WowLabsMatchMgr::AbilityDef const& a : sWowLabsMatchMgr->GetAbilityPool())
                handler->PSendSysMessage("  {} = {} ({})", a.Id, a.Name, a.Offensive ? "offensive" : "utility");
        }
        return true;
    }
};

void AddSC_wowlabs_commandscript()
{
    new wowlabs_commandscript();
}

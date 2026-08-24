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

// =============================================================================
// Vaeli "<Postal Worker>" (creature 233064) — "Going Postal" mail-race start NPC.
// -----------------------------------------------------------------------------
// Talking to Vaeli offers the player their faction's available routes (1..3);
// choosing one starts a timed race via GoingPostalMgr::StartRace. The player's
// personal bests are shown inline.
//
// EVIDENCE: creature 233064 + the 6 route currencies are DB2-confirmed @68887.
// Vaeli's REAL world-DB gossip menu ids and her spawn are CAPTURE-BLOCKED (world
// DB, not DB2), so this script drives a code-built gossip menu (DEFAULT menu id)
// rather than a captured menu id — a documented stand-in. The in-world race
// COMPLETION wire (the "[DNT] Postal Race Complete - Cover" spell / final
// checkpoint trigger) is CAPTURE-BLOCKED: completion is checkpoint-driven once
// coords are seeded, or debug-driven via ".postal complete".
// =============================================================================

#include "ScriptMgr.h"
#include "Chat.h"
#include "CreatureAI.h"
#include "GoingPostalMgr.h"
#include "GossipDef.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "WorldSession.h"

namespace
{
    // Gossip action base: START + routeIndex (1..3) selects a faction route.
    constexpr uint32 GOSSIP_ACTION_POSTAL_START_BASE = 4300; // + routeIndex

    std::string FormatRaceTime(uint32 ms)
    {
        uint32 const totalSeconds = ms / 1000;
        uint32 const minutes = totalSeconds / 60;
        uint32 const seconds = totalSeconds % 60;
        uint32 const millis  = ms % 1000;
        return Trinity::StringFormat("{}:{:02}.{:03}", minutes, seconds, millis);
    }
}

struct npc_going_postal : public CreatureAI
{
    npc_going_postal(Creature* creature) : CreatureAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override { }

    bool OnGossipHello(Player* player) override
    {
        GoingPostalTeam const team = GoingPostalMgr::GetPlayerTeam(player);
        std::vector<GoingPostalRoute const*> const routes = sGoingPostalMgr.GetRoutesForTeam(team);

        // No routes seeded for this faction → fall through to the default gossip /
        // quest pathway so we never leave the player with an empty menu.
        if (routes.empty())
            return false;

        InitGossipMenuFor(player, 0);
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        for (GoingPostalRoute const* route : routes)
        {
            std::optional<uint32> const best = sGoingPostalMgr.GetPersonalBest(player->GetGUID(), route->id);
            std::string const label = Trinity::StringFormat("Run {} (best: {})",
                route->name.empty() ? Trinity::StringFormat("Route {}", route->routeIndex) : route->name,
                best ? FormatRaceTime(*best) : "none");

            AddGossipItemFor(player, GossipOptionNpc::None, label,
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_POSTAL_START_BASE + route->routeIndex);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        CloseGossipMenuFor(player);

        if (action <= GOSSIP_ACTION_POSTAL_START_BASE || action > GOSSIP_ACTION_POSTAL_START_BASE + 3)
            return true;

        uint8 const routeIndex = uint8(action - GOSSIP_ACTION_POSTAL_START_BASE);
        GoingPostalTeam const team = GoingPostalMgr::GetPlayerTeam(player);

        if (sGoingPostalMgr.StartRace(player, team, routeIndex))
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "Going Postal: your delivery run has begun! Reach every checkpoint as fast as you can.");
            TC_LOG_DEBUG("misc", "npc_going_postal: {} started route index {} from Vaeli.",
                player->GetName(), routeIndex);
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "Going Postal: that route is not available right now.");
        }

        return true;
    }
};

void AddSC_npc_going_postal()
{
    RegisterCreatureAI(npc_going_postal);
}

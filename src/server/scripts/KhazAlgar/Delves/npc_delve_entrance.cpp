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

/*
 * Delve entrance gossip handler.
 * Assign to delve entrance NPCs via creature_template.ScriptName = 'npc_delve_entrance'
 *
 * The NPC shows a list of available delves. Once the client-side Blizzard
 * delve UI is working (needs scenario/map data), this will be replaced by
 * the native difficulty picker flow.
 */

#include "ScriptMgr.h"
#include "Creature.h"
#include "DelveMgr.h"
#include "DelvesDefines.h"
#include "DelvesRewards.h"
#include "DelvesSeason.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

using namespace Delves;

namespace
{

enum DelveGossipSender
{
    SENDER_DELVE_SELECT = 100,
};

struct npc_delve_entranceAI : public ScriptedAI
{
    npc_delve_entranceAI(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        ClearGossipMenuFor(player);

        // List all available delves from templates
        for (DelveTemplate const& tmpl : sDelveMgr->GetAllDelveTemplates())
        {
            std::string label = Trinity::StringFormat("Enter Delve (MapID {})", tmpl.MapId);
            AddGossipItemFor(player, GossipOptionNpc::None, label, SENDER_DELVE_SELECT, tmpl.MapId);
        }

        if (sDelveMgr->GetAllDelveTemplates().empty())
            AddGossipItemFor(player, GossipOptionNpc::None, "No delves configured", 0, 0);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
        uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        if (sender != SENDER_DELVE_SELECT || action == 0)
            return true;

        uint32 mapId = action;
        DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplate(mapId);
        if (!tmpl)
            return true;

        TC_LOG_DEBUG("scripts.delves", "Player {} entering delve mapId={}", player->GetName(), mapId);

        player->TeleportTo(mapId, tmpl->CompanionSpawnX, tmpl->CompanionSpawnY,
            tmpl->CompanionSpawnZ, tmpl->CompanionSpawnO);
        return true;
    }
};

} // anonymous namespace

void AddSC_npc_delve_entrance()
{
    RegisterCreatureAI(npc_delve_entranceAI);
}

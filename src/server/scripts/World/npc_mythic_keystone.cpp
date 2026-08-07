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

#include "ScriptMgr.h"
#include "ChallengeModeMgr.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemDefines.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"

// Lindormi <Mythic Keystones> (12.x NPC 244792): lowers the player's keystone one level per request, below any
// floor, keeping the same dungeon and the current week's affixes. Assign via creature_template.ScriptName
// 'npc_lindormi' (the creature itself is world content).
enum LindormiGossip
{
    GOSSIP_ACTION_LOWER_KEYSTONE = 1
};

struct npc_lindormi : public ScriptedAI
{
    npc_lindormi(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        ClearGossipMenuFor(player);

        if (Item* keystone = sChallengeModeMgr.GetKeystone(player))
            if (keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL) > sChallengeModeMgr.GetKeystoneMinLevel())
                AddGossipItemFor(player, GossipOptionNpc::None, "Lower my keystone by one level.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_LOWER_KEYSTONE);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        if (action == GOSSIP_ACTION_LOWER_KEYSTONE)
        {
            if (Item* keystone = sChallengeModeMgr.GetKeystone(player))
            {
                uint32 const level = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL);
                uint32 const dungeon = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID);
                if (level > sChallengeModeMgr.GetKeystoneMinLevel())
                    sChallengeModeMgr.StampKeystone(keystone, dungeon, level - 1);
            }
        }

        return true;
    }
};

void AddSC_npc_mythic_keystone()
{
    RegisterCreatureAI(npc_lindormi);
}

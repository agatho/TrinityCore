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
#include "Creature.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

struct npc_pet_battle_trainer : public ScriptedAI
{
    npc_pet_battle_trainer(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        // Build the database defined menu first - this also prepares the quest menu.
        // Battle pet trainers carry the retail "Trainer" and "Vendor" gossip options in
        // gossip_menu_option (menus 14400 / 14991); the Trainer one resolves through
        // creature_trainer to the trainer that sells "Battle Pet Training" (spell 125610),
        // which is what unlocks the battle pet system. Replacing the menu outright, as this
        // script used to do, hid both options and made the trainers unable to teach anything.
        player->PrepareGossipMenu(me, player->GetGossipMenuForSource(me), true);

        // Append the pet duel option on top of whatever the database provides.
        AddGossipItemFor(player, GossipOptionNpc::None, "I'd like to battle your pets.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_BATTLE);

        player->SendPreparedGossip(me);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        // Only consume the option this script added. Returning false lets the core handle every
        // database defined option (Trainer, Vendor, sub menus) in Player::OnGossipSelect - options
        // loaded from gossip_menu_option carry Sender = MenuID and Action = OrderIndex.
        if (player->PlayerTalkClass->GetGossipOptionSender(gossipListId) != GOSSIP_SENDER_MAIN ||
            player->PlayerTalkClass->GetGossipOptionAction(gossipListId) != GOSSIP_ACTION_BATTLE)
            return false;

        CloseGossipMenuFor(player);
        player->GetSession()->StartNPCPetBattle(me);
        return true;
    }
};

void AddSC_pet_battle_trainer()
{
    RegisterCreatureAI(npc_pet_battle_trainer);
}

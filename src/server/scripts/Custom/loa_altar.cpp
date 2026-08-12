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
 * Midnight Zul'Aman — Altar of Blessings.
 *
 * Bound (via creature_template.ScriptName = 'npc_altar_of_blessings') to the
 * altar creature 256508 "Altar of Blessings" in Amani'Zar Village. After the
 * unlock quest 93792 "Blessings of the Loa" the altar lets a player worship a
 * combination of one major loa (Akil'zon / Halazzi / Jan'alai / Nalorakk) and
 * one minor loa to receive a temporary, Zul'Aman-only blessing buff — only one
 * held at a time. The concrete (major, minor) -> spell mapping and the labels
 * come from LoaBlessingMgr (loa_blessing_option world table).
 */

#include "ScriptMgr.h"
#include "Creature.h"
#include "GossipDef.h"
#include "LoaBlessingMgr.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedGossip.h"

enum LoaAltarGossip
{
    // Reserved action ids (kept clear of the per-option range below).
    ACTION_REMOVE_BLESSING  = GOSSIP_ACTION_INFO_DEF + 900,
    ACTION_INFO_ONLY        = GOSSIP_ACTION_INFO_DEF + 901
};

class npc_altar_of_blessings : public CreatureScript
{
public:
    npc_altar_of_blessings() : CreatureScript("npc_altar_of_blessings") { }

    struct npc_altar_of_blessingsAI : public PassiveAI
    {
        npc_altar_of_blessingsAI(Creature* creature) : PassiveAI(creature) { }

        bool OnGossipHello(Player* player) override
        {
            InitGossipMenuFor(player, 0);

            if (!LoaBlessingMgr::IsInZulAman(player))
            {
                AddGossipItemFor(player, GossipOptionNpc::None,
                    "De loa's power only flows here in Zul'Aman.",
                    GOSSIP_SENDER_MAIN, ACTION_INFO_ONLY);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
                return true;
            }

            std::vector<LoaBlessingOption> const& options = sLoaBlessingMgr->GetOptions();

            uint32 action = GOSSIP_ACTION_INFO_DEF;
            for (LoaBlessingOption const& opt : options)
            {
                // TODO(CAPTURE-BLOCKED): hide options whose opt.UnlockConditionId
                // PlayerCondition is unmet (minor loa unlocked via side quests).
                AddGossipItemFor(player, GossipOptionNpc::None, opt.Name, GOSSIP_SENDER_MAIN, action);
                ++action;
            }

            if (options.empty())
                AddGossipItemFor(player, GossipOptionNpc::None,
                    "De altar is silent... (no blessings seeded).",
                    GOSSIP_SENDER_MAIN, ACTION_INFO_ONLY);
            else
                AddGossipItemFor(player, GossipOptionNpc::None,
                    "Remove my current blessing.",
                    GOSSIP_SENDER_MAIN, ACTION_REMOVE_BLESSING);

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            ClearGossipMenuFor(player);
            CloseGossipMenuFor(player);

            if (action == ACTION_INFO_ONLY)
                return true;

            if (action == ACTION_REMOVE_BLESSING)
            {
                sLoaBlessingMgr->RemoveHeldBlessing(player);
                return true;
            }

            // Per-option actions are GOSSIP_ACTION_INFO_DEF + <vector index>.
            if (action >= GOSSIP_ACTION_INFO_DEF && action < ACTION_REMOVE_BLESSING)
            {
                size_t index = action - GOSSIP_ACTION_INFO_DEF;
                if (LoaBlessingOption const* opt = sLoaBlessingMgr->GetOptionByIndex(index))
                    sLoaBlessingMgr->ApplyBlessing(player, opt->MajorLoa, opt->MinorLoa);
            }

            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_altar_of_blessingsAI(creature);
    }
};

void AddSC_loa_altar()
{
    new npc_altar_of_blessings();
}

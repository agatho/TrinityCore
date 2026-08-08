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
 *
 * Routes the shared "Enter Delve" NPC (creature entry 212407) to the correct
 * delve template based on the spawn's GossipMenuID (set per-spawn via
 * gossip_menu_addon SQL). Sends the 11-tier gossip menu with TIER_SPELL_IDS
 * encoded into each option's SpellID — the retail client renders the native
 * Blizzard_DelvesDifficultyPicker UI when it sees a SMSG_GOSSIP_MESSAGE whose
 * addon row carries a non-zero LfgDungeonsID.
 *
 * Adapted from stevebone/DoomCore (DelveSystem.cpp:npc_enter_delve, sniff
 * 12.0.1.66527). Naming kept as `npc_delve_entrance` for branch continuity
 * (the legacy custom-list implementation lived under this script name).
 *
 * Set creature_template.ScriptName = 'npc_delve_entrance' on creature 212407.
 */

#include "Creature.h"
#include "DelveMgr.h"
#include "DelvesDefines.h"
#include "DelvesRewards.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "Log.h"
#include "Map.h"
#include "NPCPackets.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

using namespace Delves;

namespace
{

enum DelveGossipSenders
{
    SENDER_DELVE_TIER = 1,
};

struct npc_delve_entranceAI : public ScriptedAI
{
    npc_delve_entranceAI(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        if (!player || !player->GetSession())
            return true;

        // Primary lookup: gossip menu id from spawn/template data.
        uint32 gossipMenuId = me->GetGossipMenuId();
        DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplateByGossipMenuId(gossipMenuId);

        // Fallback 1: NPC is inside the delve instance — match on map.
        if (!tmpl)
            tmpl = sDelveMgr->GetDelveTemplate(me->GetMapId());

        // Fallback 2 (proximity): the entrance NPC sits near its delve's
        // overworld exit position. Match on closest exit within 200 yards.
        if (!tmpl)
        {
            float bestDistSq = 200.0f * 200.0f;
            for (DelveTemplate const& candidate : sDelveMgr->GetAllDelveTemplates())
            {
                if (candidate.ExitX == 0.0f && candidate.ExitY == 0.0f)
                    continue;
                float dx = me->GetPositionX() - candidate.ExitX;
                float dy = me->GetPositionY() - candidate.ExitY;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    tmpl = &candidate;
                }
            }
        }

        if (!tmpl)
        {
            TC_LOG_ERROR("scripts.delves",
                "npc_delve_entrance: no DelveTemplate found for GossipMenuID {} / MapID {} on NPC {} (pos {:.1f} {:.1f})",
                gossipMenuId, me->GetMapId(), me->GetEntry(), me->GetPositionX(), me->GetPositionY());
            return true;
        }

        TC_LOG_DEBUG("scripts.delves",
            "npc_delve_entrance: player {} clicked NPC {} (GossipMenuID {} -> Map {})",
            player->GetName(), me->GetEntry(), gossipMenuId, tmpl->MapId);

        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(tmpl->GossipMenuId);

        WorldPackets::NPC::GossipMessage gossipMessage;
        gossipMessage.GossipGUID      = me->GetGUID();
        gossipMessage.GossipID        = tmpl->GossipMenuId;
        gossipMessage.LfgDungeonsID   = tmpl->LfgDungeonsId;
        gossipMessage.BroadcastTextID = tmpl->BroadcastTextId;

        // Tier gating: only tiers up to the account's HighestTierUnlocked are selectable (retail: tier N+1
        // unlocks by completing tier N with a life remaining; tiers 1-3 are open by default). Locked tiers
        // are still listed, greyed out, matching the retail picker.
        DelveProgress progress;
        DelvesRewards::LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
        uint8 const highestUnlocked = std::min<uint8>(std::max<uint8>(progress.HighestTierUnlocked, 3), MAX_DELVE_TIER);

        for (uint32 i = 0; i < MAX_DELVE_TIER; ++i)
        {
            auto& opt = gossipMessage.GossipOptions.emplace_back();
            opt.GossipOptionID = int32(tmpl->FirstTierGossipOptionId) - int32(i);
            opt.OrderIndex     = i;
            opt.OptionNPC      = GossipOptionNpc::None;
            opt.Text           = TIER_NAMES[i];
            opt.SpellID        = TIER_SPELL_IDS[i];
            opt.Status         = i < highestUnlocked ? GossipOptionStatus::Available : GossipOptionStatus::Locked;
        }

        player->PlayerTalkClass->GetInteractionData().StartInteraction(
            me->GetGUID(), PlayerInteractionType::Gossip);
        player->GetSession()->SendPacket(gossipMessage.Write());

        _delveTemplate = tmpl;
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        if (!player)
            return true;

        CloseGossipMenuFor(player);

        if (gossipListId >= MAX_DELVE_TIER)
            return true;

        if (!_delveTemplate)
            _delveTemplate = sDelveMgr->GetDelveTemplateByGossipMenuId(me->GetGossipMenuId());

        if (!_delveTemplate)
        {
            TC_LOG_ERROR("scripts.delves",
                "npc_delve_entrance::OnGossipSelect: no DelveTemplate for NPC {} GossipMenuID {}",
                me->GetEntry(), me->GetGossipMenuId());
            return true;
        }

        uint8 tier = uint8(gossipListId + 1);  // gossipListId is 0-based, tier is 1..11

        // Server-side tier gate (the greyed-out menu is cosmetic; a modified client could send any index).
        {
            DelveProgress progress;
            DelvesRewards::LoadProgress(player->GetSession()->GetBattlenetAccountId(), progress);
            if (tier > std::max<uint8>(progress.HighestTierUnlocked, 3))
            {
                TC_LOG_DEBUG("scripts.delves", "npc_delve_entrance: player {} rejected for locked tier {} (unlocked {}).",
                    player->GetName(), tier, progress.HighestTierUnlocked);
                return true;
            }
        }

        TC_LOG_DEBUG("scripts.delves",
            "npc_delve_entrance: player {} selected tier {} -> teleporting to map {}",
            player->GetName(), tier, _delveTemplate->MapId);

        // Drive the Blizzard_DelvesDifficultyPicker / in-delve HUD via WorldStates.
        player->SendUpdateWorldState(WS_DELVE_TIER, tier);
        player->SendUpdateWorldState(WS_DELVE_IN_DELVE_FLAG, 1);
        player->SendUpdateWorldState(WS_DELVE_MAP_ID, _delveTemplate->MapId);
        player->SendUpdateWorldState(WS_DELVE_TIER_SPELL, TIER_SPELL_IDS[gossipListId]);
        if (_delveTemplate->WorldState26903)
            player->SendUpdateWorldState(WS_DELVE_UNKNOWN_26903, _delveTemplate->WorldState26903);

        // Persist the selected tier on the player so DelveInstance::OnPlayerEnter
        // can read it (we already track this from CMSG_SELECT_DELVE_ENTRANCE_TIER —
        // populating it here covers the gossip-driven entry path too).
        player->m_delveSelectedMapId = _delveTemplate->MapId;
        player->m_delveSelectedTier  = tier;

        // Keep the client-side JamDelveData progression mirror's last-selected
        // delve current for the gossip-driven entry path too.
        DelvesRewards::PublishProgress(player);

        player->TeleportTo(_delveTemplate->MapId,
            _delveTemplate->EntryX, _delveTemplate->EntryY,
            _delveTemplate->EntryZ, _delveTemplate->EntryO,
            TELE_TO_SEAMLESS);

        return true;
    }

private:
    DelveTemplate const* _delveTemplate = nullptr;
};

struct go_leave_delve : public GameObjectAI
{
    go_leave_delve(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (!player)
            return true;

        DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplate(player->GetMapId());

        TC_LOG_DEBUG("scripts.delves",
            "go_leave_delve: player {} using leave portal (map {})",
            player->GetName(), player->GetMapId());

        player->ClearDelveData(int32(player->GetMapId()));

        if (tmpl)
        {
            player->SendUpdateWorldState(WS_DELVE_TIER, 0);
            player->SendUpdateWorldState(WS_DELVE_IN_DELVE_FLAG, 0);
            player->SendUpdateWorldState(WS_DELVE_MAP_ID, 0);
            player->SendUpdateWorldState(WS_DELVE_TIER_SPELL, 0);
            player->SendUpdateWorldState(WS_DELVE_UNKNOWN_26903, 0);

            // Seamless teleport to the overworld entrance position. Map 0 is
            // a placeholder — the DelveTemplate doesn't store the overworld
            // map id explicitly because every delve in the current data set
            // exits to the Khaz Algar continent (map 2552). We use the
            // player's previous outside-instance map as the destination.
            uint32 destMap = player->GetMap()->Instanceable() ? 2552 : player->GetMapId();
            player->TeleportTo(destMap, tmpl->ExitX, tmpl->ExitY, tmpl->ExitZ, tmpl->ExitO,
                TELE_TO_SEAMLESS);
        }
        else
        {
            // Fallback for delve maps with no template registered — go to bind.
            player->TeleportTo(player->m_homebind);
        }

        return true;
    }
};

} // anonymous namespace

void AddSC_npc_delve_entrance()
{
    RegisterCreatureAI(npc_delve_entranceAI);
    RegisterGameObjectAI(go_leave_delve);
}

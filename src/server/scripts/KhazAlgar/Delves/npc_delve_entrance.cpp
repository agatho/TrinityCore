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
 * Delve entrance - the shared "Enter Delve" creature (212407 on the Midnight/12.1 maps, 251896 for The Darkway).
 *
 * Three entry flavours were captured (C:\sniff\tcharvest\out\delve_research\REPORT.md 1.1):
 *   12.0.1 (deatholme)  classic gossip menu 40277, 11 tier options -> CMSG_GOSSIP_SELECT_OPTION   -> OnGossipHello/Select
 *   12.0.7 (shadowmoon) click -> CMSG_TIERED_ENTRANCE_OPEN -> SMSG_TIERED_ENTRANCE_OPEN_RESPONSE   -> DelvesHandler.cpp
 *   12.1   (gulf, eversong) NO client request: walking into range makes the server send
 *          SMSG_NPC_INTERACTION_OPEN_RESULT(entrance guid, type 79) + SMSG_TIERED_ENTRANCE_OPEN_RESPONSE
 *          (gulf 88718 and again 98205 after a CMSG_CLOSE_INTERACTION at 95751 when the player walked away) -> MoveInLineOfSight
 * All three end in CMSG_SELECT_DELVE_ENTRANCE_TIER / a gossip option and the same seamless transfer, which is
 * DelveMgr::EnterDelve (REPORT 1.4 - 1.6).
 *
 * The entrance is resolved PER SPAWN (two spawns of 212407 were visible at once in gulf, REPORT 1.2) through
 * DelveMgr::GetDelveTemplateForEntrance.
 *
 * Set creature_template.ScriptName = 'npc_delve_entrance' on creatures 212407 and 251896.
 */

#include "delves_common.h"
#include "Creature.h"
#include "DelveMgr.h"
#include "DelvesDefines.h"
#include "DelvesRewards.h"
#include "GameEventSender.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "Log.h"
#include "Map.h"
#include "NPCPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

using namespace Delves;

namespace
{

// REPORT 1.1: the picker opens "by proximity"; the distance itself is not on the wire. Bounded from above by gulf:
// the character logged in 17.8 yd from the entrance (NEW_WORLD 73023 at 34.17, 799.27 vs the spawn at 16.71, 800.37)
// and the picker did NOT open until 88718, after walking - so the range is below 17.8 yd. INTERACTION_DISTANCE-like.
constexpr float ENTRANCE_AUTO_OPEN_RANGE = 15.0f;
// Hysteresis for "walked away" (gulf 95751 CMSG_CLOSE_INTERACTION; the client closes it itself, this is the fallback)
constexpr float ENTRANCE_AUTO_CLOSE_RANGE = 20.0f;
constexpr Milliseconds ENTRANCE_RANGE_CHECK_INTERVAL = 1s;

struct npc_delve_entranceAI : public ScriptedAI
{
    npc_delve_entranceAI(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_PASSIVE);
    }

    // 12.1 proximity auto-open. CreatureAI::MoveInLineOfSight_Safe is invoked for every unit that moves within the
    // creature's sight; the base implementation only aggroes, which an immune-to-PC gossip NPC never does.
    void MoveInLineOfSight(Unit* who) override
    {
        Player* player = who ? who->ToPlayer() : nullptr;
        if (!player || !player->IsAlive() || !me->IsWithinDistInMap(player, ENTRANCE_AUTO_OPEN_RANGE))
            return;

        // once per approach; the set is pruned in UpdateAI when the player leaves ENTRANCE_AUTO_CLOSE_RANGE
        if (!_playersInRange.insert(player->GetGUID()).second)
            return;

        // SMSG_NPC_INTERACTION_OPEN_RESULT(type 79) + SMSG_TIERED_ENTRANCE_OPEN_RESPONSE (gulf 88718 / 98205)
        sDelveMgr->OpenEntranceByProximity(player, me);
    }

    void UpdateAI(uint32 diff) override
    {
        _rangeCheckTimer += Milliseconds(diff);
        if (_rangeCheckTimer < ENTRANCE_RANGE_CHECK_INTERVAL)
            return;
        _rangeCheckTimer = 0ms;

        for (auto itr = _playersInRange.begin(); itr != _playersInRange.end();)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, *itr);
            if (player && me->IsWithinDistInMap(player, ENTRANCE_AUTO_CLOSE_RANGE))
            {
                ++itr;
                continue;
            }

            if (player)
                sDelveMgr->CloseEntranceByProximity(player, me);
            itr = _playersInRange.erase(itr);
        }
    }

    // 12.0.1 gossip flavour (deatholme 50489: SMSG_GOSSIP_MESSAGE menu 40277, LfgDungeonsID 3069, 11 options with
    // the tier spells) - kept for clients that talk to the NPC instead of receiving the tiered picker.
    bool OnGossipHello(Player* player) override
    {
        if (!player || !player->GetSession())
            return true;

        DelveTemplate const* tmpl = sDelveMgr->GetDelveTemplateForEntrance(me);
        if (!tmpl)
        {
            TC_LOG_ERROR("scripts.delves",
                "npc_delve_entrance: no DelveTemplate found for NPC {} on MapID {} (pos {:.1f} {:.1f})",
                me->GetEntry(), me->GetMapId(), me->GetPositionX(), me->GetPositionY());
            return true;
        }

        TC_LOG_DEBUG("scripts.delves",
            "npc_delve_entrance: player {} clicked NPC {} -> delve map {} (gossip menu {})",
            player->GetName(), me->GetEntry(), tmpl->MapId, tmpl->GossipMenuId);

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
            _delveTemplate = sDelveMgr->GetDelveTemplateForEntrance(me);

        if (!_delveTemplate)
        {
            TC_LOG_ERROR("scripts.delves",
                "npc_delve_entrance::OnGossipSelect: no DelveTemplate for NPC {} on MapID {}",
                me->GetEntry(), me->GetMapId());
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
            "npc_delve_entrance: player {} selected tier {} -> entering map {}",
            player->GetName(), tier, _delveTemplate->MapId);

        // deatholme 53049-54033: GOSSIP_COMPLETE, PHASE_SHIFT x2, PLAYER_CHOICE_CLEAR, NEW_WORLD 2952 - the same
        // transfer as CMSG_SELECT_DELVE_ENTRANCE_TIER (REPORT 1.5), so the same code path.
        sDelveMgr->EnterDelve(player, *_delveTemplate, tier);
        return true;
    }

private:
    DelveTemplate const* _delveTemplate = nullptr;
    GuidUnorderedSet _playersInRange;
    Milliseconds _rangeCheckTimer = 0ms;
};

// GO 408227 "Leave Delve" (type 22 spellcaster, spell 460683 dummy, playerCast) - the non-bot exit. REPORT 5.6:
// eversong used it at 2650933 right before its NEW_WORLD back to map 0; scenario 3424 step 17121 "(Optional) Exit
// Delve after collecting rewards" is criteria 69831 = GameEvent 93004.
struct go_leave_delve : public GameObjectAI
{
    go_leave_delve(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (!player)
            return true;

        TC_LOG_DEBUG("scripts.delves",
            "go_leave_delve: player {} using leave portal (map {})",
            player->GetName(), player->GetMapId());

        GameEvents::Trigger(GAME_EVENT_LEAVE_DELVE_USED, player, me);

        // Load screen + seamless transfer to the exit coordinates on the map the player came from (the previous
        // hardcoded map 2552 was wrong for every Midnight delve: 2694 / 0).
        sDelveMgr->LeaveDelve(player);
        return true;
    }
};

} // anonymous namespace

void AddSC_npc_delve_entrance()
{
    // RegisterCreatureAI() stringizes the type name, so `RegisterCreatureAI(npc_delve_entranceAI)`
    // registered this under "npc_delve_entranceAI", while creature_template.ScriptName says
    // "npc_delve_entrance". Register under the name the database actually uses.
    new GenericCreatureScript<npc_delve_entranceAI>("npc_delve_entrance");
    RegisterGameObjectAI(go_leave_delve);
}

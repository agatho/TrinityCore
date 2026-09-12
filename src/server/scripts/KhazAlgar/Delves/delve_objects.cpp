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
 * Delve reward and exit objects shared by every delve map. Every behaviour is quoted against
 * C:\sniff\tcharvest\out\delve_research\REPORT.md (section / capture tick); nothing here is invented beyond the
 * fallbacks that are marked as such.
 *
 *   go_heavy_trunk                          584517 / 584519 "Heavy Trunk"      REPORT 2.3, 5.4
 *   go_mislaid_curiosity                    584752 "Mislaid Curiosity"          REPORT 2.4
 *   npc_leave_o_bot                         205496 "Leave-O-Bot 7000"           REPORT 5.6
 *   playerchoice_delve_discovered_treasure  PlayerChoice 822                    REPORT 2.4
 *   item_delve_curio                        271132 / 249219 / 249222            REPORT 3.2
 */

#include "delves_common.h"
#include "DelveMgr.h"
#include "DelvesPackets.h"
#include "GameEventSender.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerChoice.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "TaskScheduler.h"

using namespace Delves;

namespace
{

// REPORT 4 / toasts_items.txt (gulf 1073180): the Heavy Trunk's curio - item 271132, ItemContext 25, BonusListIDs
// [13677], toast Method 3. 12.0.x runs gave the season-1 curios 249219 (eversong 2626599) / 249222 (deatholme).
constexpr uint32 ITEM_DELVE_CURIO_12_1 = 271132;
constexpr uint8 ITEM_DELVE_CURIO_CONTEXT = 25;
constexpr int32 ITEM_DELVE_CURIO_BONUS_LIST = 13677;

// REPORT 2.3: use 1071948 -> despawn 1073361 (1.4 s); 1075729 -> 1077012 (1.3 s)
constexpr Milliseconds HEAVY_TRUNK_DESPAWN_DELAY = 1300ms;

// REPORT 5.6 (gulf): CMSG_SPELL_CLICK 1118881 -> SMSG_SPELL_VISUAL_LOAD_SCREEN 1120847 (+2.0 s). DelveMgr::LeaveDelve
// owns everything from the load screen on; this is the ride on the bot before it.
constexpr Milliseconds LEAVE_O_BOT_RIDE_DURATION = 2s;

// ---------------------------------------------------------------------------------------------
// Heavy Trunk - GO type 3 (chest), lock 1634, consumable. Two per completion.
//   584517 (used first in all three runs): SMSG_GAME_OBJECT_ACTIVATE_ANIM_KIT 13792 -> SMSG_SET_CURRENCY 3316 +10
//          (gulf 1072860, eversong 2626130) -> curio item toast (gulf 1073180 item 271132) -> despawn.
//   584519: anim kit 13792 -> SCENARIO_PROGRESS_UPDATE 64984 (GameEvent 88888) -> 3424 step 17120 -> despawn ->
//          Leave-O-Bot 205496 spawns (gulf 1075729 / 1076552 / 1077012 / 1077344).
// Both reach GameObject::Use through the client's open-lock cast (EffectOpenLock -> Use), which asks OnGossipHello
// first; CMSG_GAME_OBJ_REPORT_USE (seen for both trunks) lands in OnReportUse.
// ---------------------------------------------------------------------------------------------
struct go_heavy_trunk : public GameObjectAI
{
    go_heavy_trunk(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        Open(player);
        return true;
    }

    bool OnReportUse(Player* player) override
    {
        Open(player);
        return false;   // keep the UseGameobject criteria update
    }

private:
    void Open(Player* player)
    {
        if (_opened || !player)
            return;
        _opened = true;

        me->SetAnimKitId(HEAVY_TRUNK_ANIM_KIT, true);

        if (me->GetEntry() == GO_HEAVY_TRUNK_A)
        {
            // SET_CURRENCY 3316 Change 10, LostSrc 9 = CurrencyGainSource::Loot
            player->AddCurrency(CURRENCY_VOIDLIGHT_MARL, HEAVY_TRUNK_VOIDLIGHT_MARL, CurrencyGainSource::Loot);
            GiveCurio(player);
        }
        else
        {
            // "Treasure found" - criteria 64984 of 3424 step 17119; the instance script spawns the Leave-O-Bot on it
            GameEvents::Trigger(GAME_EVENT_TREASURE_FOUND, player, me);
            if (InstanceScript* instance = me->GetInstanceScript())
                instance->SetData(DATA_TREASURE_ROOM_OPENED, 1);
        }

        me->DespawnOrUnsummon(HEAVY_TRUNK_DESPAWN_DELAY);
    }

    static void GiveCurio(Player* player)
    {
        if (!sObjectMgr->GetItemTemplate(ITEM_DELVE_CURIO_12_1))
        {
            TC_LOG_DEBUG("scripts.delves", "go_heavy_trunk: curio item {} does not exist in this client's item store, skipped.", ITEM_DELVE_CURIO_12_1);
            return;
        }

        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_DELVE_CURIO_12_1, 1) != EQUIP_ERR_OK)
        {
            TC_LOG_DEBUG("scripts.delves", "go_heavy_trunk: {} has no room for curio {}.", player->GetName(), ITEM_DELVE_CURIO_12_1);
            return;
        }

        std::vector<int32> bonusListIDs = { ITEM_DELVE_CURIO_BONUS_LIST };
        if (Item* item = player->StoreNewItem(dest, ITEM_DELVE_CURIO_12_1, true, GenerateItemRandomBonusListId(ITEM_DELVE_CURIO_12_1),
            GuidSet(), static_cast<ItemContext>(ITEM_DELVE_CURIO_CONTEXT), &bonusListIDs))
            player->SendNewItem(item, 1, true, false);
    }

    bool _opened = false;
};

// ---------------------------------------------------------------------------------------------
// Mislaid Curiosity - GO type 50 (gathering node), 8-10 per run. REPORT 2.4: using one is the trigger of
// SMSG_DISPLAY_PLAYER_CHOICE 822 "Discovered Treasure" (eversong 2121802: "Stomach Turner", MawPower spell 1305224)
// and of SET_CURRENCY 3253 +1 (deatholme 773101 right after CMSG_GAME_OBJ_REPORT_USE on 584752; gulf 303655 ...).
// Not implemented: the loot variant seen in gulf (toasts Method 3: 260882 / 254748 / 260878 + 3316 +3..+5) - the
// choice-vs-loot rule was not observable.
// ---------------------------------------------------------------------------------------------
struct go_mislaid_curiosity : public GameObjectAI
{
    go_mislaid_curiosity(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        Use(player);
        return true;
    }

    bool OnReportUse(Player* player) override
    {
        Use(player);
        return false;
    }

private:
    void Use(Player* player)
    {
        if (!player)
            return;

        // weekly tracker (Flags 4, Max 8736 on the wire); once per object
        if (_trackedFor.insert(player->GetGUID()).second)
            player->AddCurrency(CURRENCY_MISLAID_CURIOSITY_WEEKLY, 1, CurrencyGainSource::Loot);

        // The object stays until the choice is answered (playerchoice_delve_discovered_treasure despawns it) so the
        // interaction source remains valid; a player who walks away can come back and choose.
        player->SendPlayerChoice(me->GetGUID(), PLAYER_CHOICE_DISCOVERED_TREASURE);
    }

    GuidUnorderedSet _trackedFor;
};

// ---------------------------------------------------------------------------------------------
// PlayerChoice 822 "Discovered Treasure" - the delve power is a MawPower response (REPORT 2.4): on response cast the
// power's spell on the player (CMSG_CHOICE_RESPONSE 822/1 -> SMSG_PLAYER_CHOICE_CLEAR; 1305224 is aura 42
// PROC_TRIGGER_SPELL "Occasionally inflict Stomach Turner on enemies in combat").
// ---------------------------------------------------------------------------------------------
class playerchoice_delve_discovered_treasure : public PlayerChoiceScript
{
public:
    playerchoice_delve_discovered_treasure() : PlayerChoiceScript("playerchoice_delve_discovered_treasure") { }

    void OnResponse(WorldObject* object, Player* player, PlayerChoice const* /*choice*/, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/) override
    {
        if (response->MawPower && response->MawPower->SpellID > 0)
        {
            uint32 spellId = uint32(response->MawPower->SpellID);
            if (sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                player->CastSpell(player, spellId, true);
            else
                TC_LOG_ERROR("scripts.delves", "playerchoice_delve_discovered_treasure: MawPower spell {} of response {} does not exist.", spellId, response->ResponseId);
        }

        // the curiosity is consumed by the choice (deatholme: the object is gone after 773101)
        if (GameObject* curiosity = object ? object->ToGameObject() : nullptr)
            if (curiosity->GetEntry() == GO_MISLAID_CURIOSITY)
                curiosity->DespawnOrUnsummon(1s);
    }
};

// ---------------------------------------------------------------------------------------------
// Leave-O-Bot 7000 - vehicle 205496 (VehicleId 8163), npc_spellclick_spells 411497 (aura 236 CONTROL_VEHICLE + aura 260
// screen effect 1900 + trigger 436004). REPORT 5.6: CMSG_SPELL_CLICK (gulf 1118881) -> load screen 79917 (+2.0 s) ->
// ... -> SMSG_NEW_WORLD to the originating map's exit coordinates (1129673). Criteria 67376 of 3424 step 17120 is
// GameEvent 91282 "(Optional) Exit Delve with Leave-O-Bot after collecting rewards".
// ---------------------------------------------------------------------------------------------
struct npc_leave_o_bot : public ScriptedAI
{
    npc_leave_o_bot(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_PASSIVE);
    }

    void OnSpellClick(Unit* clicker, bool /*spellClickHandled*/) override
    {
        Player* player = clicker ? clicker->ToPlayer() : nullptr;
        if (!player)
            return;

        GameEvents::Trigger(GAME_EVENT_LEAVE_O_BOT_USED, player, me);

        ObjectGuid playerGuid = player->GetGUID();
        _scheduler.Schedule(LEAVE_O_BOT_RIDE_DURATION, [this, playerGuid](TaskContext /*task*/)
        {
            if (Player* rider = ObjectAccessor::GetPlayer(*me, playerGuid))
                sDelveMgr->LeaveDelve(rider);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};

// ---------------------------------------------------------------------------------------------
// Curio items. REPORT 3.2: SMSG_SHOW_DELVES_COMPANION_CONFIGURATION_UI carries the uint32 ItemID of the curio the
// player just used (gulf 1096575: 271132 after CMSG_USE_ITEM 1091943/1094892; eversong 2688539: 249219; deatholme
// 75171: 249222) - it is item-driven, not gossip-driven. The item's own spell still runs (return false).
// ---------------------------------------------------------------------------------------------
class item_delve_curio : public ItemScript
{
public:
    item_delve_curio() : ItemScript("item_delve_curio") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/, ObjectGuid /*castId*/) override
    {
        WorldPackets::Delves::ShowDelvesCompanionConfigurationUI packet;
        packet.Unknown = item->GetEntry();
        player->SendDirectMessage(packet.Write());
        return false;
    }
};

} // anonymous namespace

void AddSC_delve_objects()
{
    RegisterGameObjectAI(go_heavy_trunk);
    RegisterGameObjectAI(go_mislaid_curiosity);
    RegisterCreatureAI(npc_leave_o_bot);
    new playerchoice_delve_discovered_treasure();
    new item_delve_curio();
}

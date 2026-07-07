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

#include "WorldSession.h"
#include "CraftingOrderMgr.h"
#include "CraftingOrderPackets.h"
#include "CharacterDatabase.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"

// Pushes SMSG_CRAFTING_ORDER_UPDATE_STATE to the online parties interested in an order (its customer and, once
// assigned, its crafter) so their open browse windows reflect the new state without a manual refresh.
static void BroadcastCraftingOrderState(CraftingOrders::Order const& order)
{
    WorldPackets::CraftingOrders::CraftingOrderUpdateState update;
    update.OrderID = order.OrderID;
    update.OrderState = uint8(order.State);
    update.CrafterGUID = order.CrafterGUID;
    update.SkillLineAbilityID = order.SkillLineAbilityID;
    update.OrderType = uint8(order.Type);
    WorldPacket const* built = update.Write();

    if (Player* customer = ObjectAccessor::FindConnectedPlayer(order.CustomerGUID))
        customer->SendDirectMessage(built);
    if (!order.CrafterGUID.IsEmpty() && order.CrafterGUID != order.CustomerGUID)
        if (Player* crafter = ObjectAccessor::FindConnectedPlayer(order.CrafterGUID))
            crafter->SendDirectMessage(built);
}

void WorldSession::HandleCraftingOrderCreate(WorldPackets::CraftingOrders::CraftingOrderCreate& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Validate the recipe: the SkillLineAbility must exist (client sends a real recipe).
    SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(packet.SkillLineAbilityID);
    if (!ability)
    {
        TC_LOG_DEBUG("network", "CMSG_CRAFTING_ORDER_CREATE: {} sent an unknown SkillLineAbilityID {}",
            player->GetGUID().ToString(), packet.SkillLineAbilityID);
        return;
    }

    CraftingOrders::Order order;
    order.SkillLineAbilityID = packet.SkillLineAbilityID;
    order.Type = CraftingOrders::OrderType(packet.OrderType);
    order.MinQuality = packet.MinQuality;
    order.TipAmount = packet.TipAmount;
    order.CustomerNotes = packet.CustomerNotes;
    // Personal orders (and the client-provided target) carry the intended crafter.
    order.CrafterGUID = packet.TargetGUID;

    // Default posting lifetime. The client's per-order duration selection is not yet mapped to a wire field,
    // so a fixed default is used for now (refined in a later phase).
    int64 const now = GameTime::GetGameTime();
    order.EndDate = now + 30 * DAY;

    // Reagents the customer provided (Vectors[0]). Field semantics are not yet confirmed, so they are stored
    // positionally; reagent validation/escrow is handled in a later phase. The wire is consumed byte-exact.
    for (WorldPackets::CraftingOrders::CraftingReagentSlot const& slot : packet.Vectors[0])
    {
        CraftingOrders::OrderReagent reagent;
        reagent.ItemID = int32(slot.Field1);
        reagent.Quantity = slot.Field2;
        reagent.Slot = slot.Extra.value_or(0);
        order.Reagents.push_back(reagent);
    }

    // The tip is escrowed up front (like an auction deposit): the customer must have the gold, and it is held by
    // the order until it is fulfilled (paid to the crafter) or dies (refunded). This is the only place gold leaves
    // the customer; every terminal transition releases exactly the escrowed amount, so no gold is created or lost.
    uint64 const tip = packet.TipAmount;
    if (tip && !player->HasEnoughMoney(tip))
    {
        WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
        result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::MissingCurrency;
        SendPacket(result.Write());
        return;
    }

    uint64 const id = sCraftingOrderMgr.CreateOrder(player, std::move(order));

    WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
    result.Result = id ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotCreate;
    result.CraftingOrderID = id;
    SendPacket(result.Write());

    if (!id)
        return;

    if (tip)
    {
        player->ModifyMoney(-int64(tip));
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        player->SaveInventoryAndGoldToDB(trans);
        CharacterDatabase.CommitTransaction(trans);
    }

    TC_LOG_DEBUG("network", "CMSG_CRAFTING_ORDER_CREATE: {} posted order {} for recipe {} (tip {})",
        player->GetGUID().ToString(), id, packet.SkillLineAbilityID, packet.TipAmount);
}

void WorldSession::HandleCraftingOrderClaim(WorldPackets::CraftingOrders::CraftingOrderClaim& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    bool const ok = sCraftingOrderMgr.ClaimOrder(packet.OrderID, player->GetGUID());

    WorldPackets::CraftingOrders::CraftingOrderClaimResult result;
    result.Result = ok ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotClaim;
    result.CraftingOrderID = packet.OrderID;
    SendPacket(result.Write());

    if (ok)
        if (CraftingOrders::Order const* order = sCraftingOrderMgr.GetOrder(packet.OrderID))
            BroadcastCraftingOrderState(*order);
}

void WorldSession::HandleCraftingOrderCancel(WorldPackets::CraftingOrders::CraftingOrderCancel& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    bool const ok = sCraftingOrderMgr.CancelOrder(packet.OrderID, player->GetGUID());

    WorldPackets::CraftingOrders::CraftingOrderCancelResult result;
    result.Result = ok ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotCancel;
    result.CraftingOrderID = packet.OrderID;
    SendPacket(result.Write());
}

void WorldSession::HandleCraftingOrderRelease(WorldPackets::CraftingOrders::CraftingOrderRelease& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    bool const ok = sCraftingOrderMgr.ReleaseOrder(packet.OrderID, player->GetGUID());

    WorldPackets::CraftingOrders::CraftingOrderReleaseResult result;
    result.Result = ok ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotRelease;
    result.CraftingOrderID = packet.OrderID;
    SendPacket(result.Write());

    if (ok)
        if (CraftingOrders::Order const* order = sCraftingOrderMgr.GetOrder(packet.OrderID))
            BroadcastCraftingOrderState(*order);
}

void WorldSession::HandleCraftingOrderReject(WorldPackets::CraftingOrders::CraftingOrderReject& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    bool const ok = sCraftingOrderMgr.RejectOrder(packet.OrderID, player->GetGUID(), std::move(packet.Reason));

    WorldPackets::CraftingOrders::CraftingOrderRejectResult result;
    result.Result = ok ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotReject;
    result.CraftingOrderID = packet.OrderID;
    SendPacket(result.Write());

    if (ok)
        if (CraftingOrders::Order const* order = sCraftingOrderMgr.GetOrder(packet.OrderID))
            BroadcastCraftingOrderState(*order);
}

void WorldSession::HandleCraftingOrderFulfill(WorldPackets::CraftingOrders::CraftingOrderFulfill& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    CraftingOrders::Order const* order = sCraftingOrderMgr.GetOrder(packet.OrderID);
    bool ok = order && order->State == CraftingOrders::OrderState::Claimed && order->CrafterGUID == player->GetGUID();

    if (ok)
    {
        // No crafted item rides the fulfil wire — derive the recipe's output (SkillLineAbility -> spell ->
        // SPELL_EFFECT_CREATE_ITEM) and mail it to the customer, mirroring the client's craft-then-fulfil flow.
        uint32 outItemId = 0;
        int32 outCount = 1;
        if (SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(order->SkillLineAbilityID))
            if (SpellInfo const* recipe = sSpellMgr->GetSpellInfo(ability->Spell, DIFFICULTY_NONE))
                for (SpellEffectInfo const& effect : recipe->GetEffects())
                    if (effect.IsEffect(SPELL_EFFECT_CREATE_ITEM))
                    {
                        outItemId = effect.ItemType;
                        outCount = std::max<int32>(1, effect.CalcValue(player));
                        break;
                    }

        ObjectGuid const customerGuid = order->CustomerGUID;
        if (outItemId)
        {
            if (Item* crafted = Item::CreateItem(outItemId, uint32(outCount), ItemContext::NONE, player))
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                crafted->SaveToDB(trans);
                MailDraft("Crafting Order Complete", "Your crafted item is enclosed.")
                    .AddItem(crafted)
                    .SendMailTo(trans, MailReceiver(ObjectAccessor::FindConnectedPlayer(customerGuid), customerGuid.GetCounter()),
                        MailSender(player, MAIL_STATIONERY_DEFAULT), MAIL_CHECK_MASK_COPIED);
                CharacterDatabase.CommitTransaction(trans);
            }
        }

        // Transition Claimed -> Fulfilled and release the escrowed tip to the crafter.
        ok = sCraftingOrderMgr.FulfillOrder(packet.OrderID, player->GetGUID());
    }

    WorldPackets::CraftingOrders::CraftingOrderFulfillResult result;
    result.Result = ok ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotFulfill;
    result.CraftingOrderID = packet.OrderID;
    SendPacket(result.Write());

    if (ok)
        if (CraftingOrders::Order const* fulfilled = sCraftingOrderMgr.GetOrder(packet.OrderID))
            BroadcastCraftingOrderState(*fulfilled);
}

// Projects a stored order into the client's JamCraftingOrder wire form (customer-provided reagents + the
// optional recraft/output/npc sub-structs are sent absent — byte-exact for a basic public order).
static WorldPackets::CraftingOrders::CraftingOrderData BuildCraftingOrderData(CraftingOrders::Order const& order)
{
    WorldPackets::CraftingOrders::CraftingOrderData data;
    data.OrderID = order.OrderID;
    data.SkillLineAbilityID = order.SkillLineAbilityID;
    data.OrderState = int32(order.State);
    data.OrderType = uint8(order.Type);
    data.MinQuality = uint8(order.MinQuality);
    data.EndDate = order.EndDate;
    data.ClaimEndDate = order.ClaimEndDate;
    data.TipAmount = order.TipAmount;
    data.HouseCutAmount = order.HouseCutAmount;
    data.Flags = order.Flags;
    data.CustomerGUID = order.CustomerGUID;
    data.CrafterGUID = order.CrafterGUID;
    data.CustomerNotes = order.CustomerNotes;
    return data;
}

void WorldSession::HandleCraftingOrderListMyOrders(WorldPackets::CraftingOrders::CraftingOrderListMyOrders& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::CraftingOrders::CraftingOrderListOrdersResponse response;
    for (CraftingOrders::Order const* order : sCraftingOrderMgr.ListOrdersByCustomer(player->GetGUID()))
        response.Orders.push_back(BuildCraftingOrderData(*order));

    SendPacket(response.Write());
}

void WorldSession::HandleCraftingOrderListCrafterOrders(WorldPackets::CraftingOrders::CraftingOrderListCrafterOrders& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::CraftingOrders::CraftingOrderListOrdersResponse response;
    for (CraftingOrders::Order const* order : sCraftingOrderMgr.ListClaimableForRecipe(packet.SkillLineAbilityID))
        response.Orders.push_back(BuildCraftingOrderData(*order));

    SendPacket(response.Write());
}

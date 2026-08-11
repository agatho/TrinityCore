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
#include "DatabaseEnv.h"
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

    // A personal order aimed at a crafter the customer has ignored is refused (client result CrafterIsIgnored).
    // The ignore set is populated by CMSG_CRAFTING_ORDER_UPDATE_IGNORE_LIST.
    if (!packet.TargetGUID.IsEmpty() && sCraftingOrderMgr.IsIgnoring(player->GetGUID(), packet.TargetGUID))
    {
        WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
        result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::CrafterIsIgnored;
        SendPacket(result.Write());
        return;
    }

    CraftingOrders::OrderType const orderType = CraftingOrders::OrderType(packet.OrderType);

    // Server-side re-validation of the client-gated invariants (a modified client can bypass the UI checks, G12):
    //  - a Personal order must name the crafter it is directed at (InvalidTarget otherwise);
    //  - the tip must be positive (the client blocks tip <= 0; a 0-tip order carries no escrow and is refused).
    if (orderType == CraftingOrders::OrderType::Personal && packet.TargetGUID.IsEmpty())
    {
        WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
        result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::InvalidTarget;
        SendPacket(result.Write());
        return;
    }

    if (!packet.TipAmount)
    {
        WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
        result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::CannotCreate;
        SendPacket(result.Write());
        return;
    }

    CraftingOrders::Order order;
    order.SkillLineAbilityID = packet.SkillLineAbilityID;
    order.Type = orderType;
    order.MinQuality = packet.MinQuality;
    order.TipAmount = packet.TipAmount;
    order.CustomerNotes = packet.CustomerNotes;
    // Personal orders (and the client-provided target) carry the intended crafter.
    order.CrafterGUID = packet.TargetGUID;

    // Default posting lifetime. The client's per-order duration selection is not yet mapped to a wire field,
    // so a fixed default is used for now (refined in a later phase).
    int64 const now = GameTime::GetGameTime();
    order.EndDate = now + 30 * DAY;

    // Customer-provided reagents are NOT stored or advertised (G5b interim). Real reagent escrow (validate + destroy
    // at create, consume at fulfil, refund on terminal) is deferred: the create-wire reagent field semantics
    // (CraftingReagentSlot.Field1/Field2) are still UNCONFIRMED, so destroying the customer's items off them would risk
    // consuming the wrong items. Until escrow lands we deliberately drop them rather than persist a phantom manifest and
    // advertise it as customer-provided (Flags=1) - which is the cross-player reagent-theft/deception path (G5). The
    // wire is still consumed byte-exact by CraftingOrderCreate::Read; we simply do not act on packet.Vectors[0] here.

    // The tip is escrowed up front (like an auction deposit): the customer must have the gold, and it is held by the
    // order until it is fulfilled (paid to the crafter) or dies (refunded). This is the only place gold leaves the
    // customer; every terminal transition releases exactly the escrowed amount, so no gold is created or lost.
    uint64 const tip = packet.TipAmount;
    if (!player->HasEnoughMoney(tip))
    {
        WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
        result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::MissingCurrency;
        SendPacket(result.Write());
        return;
    }

    // Atomic escrow-at-create (anti-abuse G3): debit the tip in memory, then commit the gold write and the order/reagent
    // rows in ONE transaction. Previously the order INSERT committed in its own transaction BEFORE a separate gold-debit
    // transaction, so a crash in between persisted a tip-bearing order that was never paid for - re-minted on the next
    // terminal transition after LoadFromDB. Now a crash before the single commit leaves NEITHER the debit NOR the order.
    player->ModifyMoney(-int64(tip));

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    player->SaveInventoryAndGoldToDB(trans);
    uint64 const id = sCraftingOrderMgr.CreateOrder(player, std::move(order), trans);
    if (!id)
    {
        // CreateOrder never queued anything on failure; undo the in-memory debit and drop the uncommitted transaction.
        player->ModifyMoney(int64(tip));

        WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
        result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::CannotCreate;
        SendPacket(result.Write());
        return;
    }
    CharacterDatabase.CommitTransaction(trans);

    WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
    result.Result = WorldPackets::CraftingOrders::CraftingOrderResult::Ok;
    result.CraftingOrderID = id;
    SendPacket(result.Write());

    TC_LOG_DEBUG("network", "CMSG_CRAFTING_ORDER_CREATE: {} posted order {} for recipe {} (tip {})",
        player->GetGUID().ToString(), id, packet.SkillLineAbilityID, packet.TipAmount);
}

void WorldSession::HandleCraftingOrderClaim(WorldPackets::CraftingOrders::CraftingOrderClaim& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    bool const ok = sCraftingOrderMgr.ClaimOrder(packet.OrderID, player);

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

    // Capture the criteria inputs before the fulfil transition (the order may be reaped shortly after it turns
    // terminal). Recipe authorization, item derivation, the Claimed->Fulfilled state write, item delivery and the tip
    // payout are all done atomically inside FulfillOrder (state-first, single transaction) so a crash cannot dupe the
    // crafted item or the tip (anti-abuse G1/G4/G11).
    CraftingOrders::Order const* order = sCraftingOrderMgr.GetOrder(packet.OrderID);
    CraftingOrders::OrderType const orderType = order ? order->Type : CraftingOrders::OrderType::Public;
    uint32 const skillLineAbilityId = order ? uint32(order->SkillLineAbilityID) : 0u;

    bool const ok = sCraftingOrderMgr.FulfillOrder(packet.OrderID, player);

    if (ok)
    {
        // CriteriaType::FulfillAnyCraftingOrder (245) - plain counter ("Crafting Orders fulfilled").
        // CriteriaType::FulfillCraftingOrderType (246) - Asset = {CraftingOrderType}; real Criteria rows
        // carry 0/1/2/3, which is exactly CraftingOrders::OrderType (Public/Guild/Personal/Npc).
        // miscValue2 carries the SkillLineAbility so ModifierTreeType::CraftingOrderSkillLineAbility (347)
        // can discriminate the recipe.
        player->UpdateCriteria(CriteriaType::FulfillAnyCraftingOrder, 1, skillLineAbilityId);
        player->UpdateCriteria(CriteriaType::FulfillCraftingOrderType, uint32(orderType), skillLineAbilityId);
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

// Projects a stored order into the client's JamCraftingOrder wire form. The scalar head is byte-exact vs the live
// NPC-order sniff; customer-provided reagents are currently NOT emitted (orders carry none while reagent escrow is
// deferred, G5b), so the reagent vector serializes empty. The optional recraft/output/customerNpc sub-structs are also
// sent absent, which is byte-exact for a basic public player order. (The player-order customerPlayer path below is
// RE/reflection-verified only - the sniff is NPC-order-only and cannot exercise it, G14.)
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

    // Player-placed orders carry a customerPlayer sub-struct so the client can display who ordered. NPC/patron
    // orders (OrderType::Npc) would instead use customerNpc, which is not produced yet.
    if (order.Type != CraftingOrders::OrderType::Npc && order.CustomerGUID.IsPlayer())
    {
        data.HasCustomerPlayer = true;
        if (order.CustomerAccountId)
            data.CustomerWowAccount = ObjectGuid::Create<HighGuid::WowAccount>(order.CustomerAccountId);
    }

    data.Reagents.reserve(order.Reagents.size());
    for (CraftingOrders::OrderReagent const& reagent : order.Reagents)
    {
        WorldPackets::CraftingOrders::CraftingOrderReagentData& wire = data.Reagents.emplace_back();
        wire.OwnerGUID = order.CustomerGUID;        // the customer supplied it (matches the sniffed ownerGUID)
        wire.Quantity = reagent.Quantity;
        wire.ReagentItemID = reagent.ItemID;
        wire.ReagentCurrencyID = reagent.CurrencyID;
        wire.Slot = reagent.Slot;
        // Flags default 1 (customer-provided); orderItemID/type/itemGUID/qualityID stay 0 for an unclaimed posting.
    }
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

// CMSG_NPC_CRAFTING_ORDER_REQUEST (empty body): the client opened an NPC (patron) work-order board and wants the
// available NPC orders. Answered with the same LIST_ORDERS_RESPONSE the browse handlers use, filtered to NPC orders.
// Content-agnostic: whatever OrderType::Npc orders exist in the pool are returned (currently none — NPC-order content
// is a separate authoring task — so this is a truthful empty list, exactly like a browse that matched nothing).
void WorldSession::HandleNpcCraftingOrderRequest(WorldPackets::CraftingOrders::NpcCraftingOrderRequest& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::CraftingOrders::CraftingOrderListOrdersResponse response;
    for (CraftingOrders::Order const* order : sCraftingOrderMgr.ListNpcOrders())
        response.Orders.push_back(BuildCraftingOrderData(*order));

    SendPacket(response.Write());
}

// CMSG_CRAFTING_ORDER_GET_NPC_REWARD_INFO: the client asks for the reward preview of the NPC orders it is browsing.
// The reply carries reward info only for orders the server actually knows about (NPC orders in the pool); each such
// order emits a zero-length reward list until reward content is attached (the reward blob is authored NPC-order
// content, not synthesized here). With no NPC-order content this is a bare-header reply — the honest "no reward data"
// answer for orders the server does not have.
void WorldSession::HandleCraftingOrderGetNpcRewardInfo(WorldPackets::CraftingOrders::CraftingOrderGetNpcRewardInfo& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::CraftingOrders::CraftingOrderNpcRewardInfo response;
    response.ContextField = packet.ContextField;
    for (WorldPackets::CraftingOrders::NpcRewardInfoRequest const& req : packet.Orders)
    {
        CraftingOrders::Order const* order = sCraftingOrderMgr.GetOrder(req.OrderID);
        if (order && order->Type == CraftingOrders::OrderType::Npc)
        {
            WorldPackets::CraftingOrders::NpcRewardInfoEntry& entry = response.Entries.emplace_back();
            entry.OrderID = order->OrderID;
        }
    }

    SendPacket(response.Write());
}

// CMSG_CRAFTING_ORDER_UPDATE_IGNORE_LIST: the client sends the player's full crafting-order ignore list (the set of
// crafters they don't want to deal with). Stored wholesale on the manager; a personal order aimed at an ignored
// crafter is subsequently refused in HandleCraftingOrderCreate (CrafterIsIgnored).
void WorldSession::HandleCraftingOrderUpdateIgnoreList(WorldPackets::CraftingOrders::CraftingOrderUpdateIgnoreList& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    sCraftingOrderMgr.SetIgnoreList(player->GetGUID(), std::move(packet.IgnoredPlayers));
}

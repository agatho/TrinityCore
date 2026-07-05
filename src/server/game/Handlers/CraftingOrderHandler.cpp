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
#include "DB2Stores.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include "World.h"

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

    uint64 const id = sCraftingOrderMgr.CreateOrder(player, std::move(order));

    WorldPackets::CraftingOrders::CraftingOrderCreateResult result;
    result.Result = id ? WorldPackets::CraftingOrders::CraftingOrderResult::Ok
                       : WorldPackets::CraftingOrders::CraftingOrderResult::CannotCreate;
    result.CraftingOrderID = id;
    SendPacket(result.Write());

    if (!id)
        return;

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
}

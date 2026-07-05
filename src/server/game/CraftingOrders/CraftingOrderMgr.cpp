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

#include "CraftingOrderMgr.h"
#include "GameTime.h"
#include "Player.h"

namespace
{
    constexpr uint32 EXPIRE_CHECK_INTERVAL_MS = 30 * IN_MILLISECONDS;
}

CraftingOrderMgr& CraftingOrderMgr::Instance()
{
    static CraftingOrderMgr instance;
    return instance;
}

void CraftingOrderMgr::LoadFromDB()
{
    // P1: load persisted orders (crafting_orders / _reagents) and seed _nextOrderId past the max stored id.
    // P0 keeps the registry in memory only.
}

void CraftingOrderMgr::Update(uint32 diff)
{
    _expireTimer += diff;
    if (_expireTimer < EXPIRE_CHECK_INTERVAL_MS)
        return;
    _expireTimer = 0;

    int64 const now = GameTime::GetGameTime();
    for (auto itr = _orders.begin(); itr != _orders.end(); )
    {
        CraftingOrders::Order& order = itr->second;
        bool const postingExpired = order.State == CraftingOrders::OrderState::Created && order.EndDate && now >= order.EndDate;
        bool const claimExpired = order.State == CraftingOrders::OrderState::Claimed && order.ClaimEndDate && now >= order.ClaimEndDate;

        if (claimExpired)
        {
            // Crafter missed the deadline: return the order to the open pool (P4 will also notify + re-list).
            order.State = CraftingOrders::OrderState::Created;
            order.CrafterGUID.Clear();
            order.ClaimEndDate = 0;
            ++itr;
        }
        else if (postingExpired)
        {
            // P4: refund the customer's escrowed tip/reagents before erasing.
            order.State = CraftingOrders::OrderState::Expired;
            itr = _orders.erase(itr);
        }
        else
            ++itr;
    }
}

uint64 CraftingOrderMgr::CreateOrder(Player* customer, CraftingOrders::Order order)
{
    if (!customer)
        return 0;

    order.OrderID = _nextOrderId++;
    order.CustomerGUID = customer->GetGUID();
    order.CustomerAccountId = customer->GetSession()->GetAccountId();
    order.State = CraftingOrders::OrderState::Created;

    uint64 const id = order.OrderID;
    _orders[id] = std::move(order);
    return id;
}

bool CraftingOrderMgr::ClaimOrder(uint64 orderId, ObjectGuid crafter)
{
    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order || !order->IsClaimable())
        return false;

    // Personal orders may only be claimed by their designated crafter.
    if (order->Type == CraftingOrders::OrderType::Personal && order->CrafterGUID != crafter)
        return false;

    order->State = CraftingOrders::OrderState::Claimed;
    order->CrafterGUID = crafter;
    return true;
}

bool CraftingOrderMgr::ReleaseOrder(uint64 orderId, ObjectGuid crafter)
{
    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order || order->State != CraftingOrders::OrderState::Claimed || order->CrafterGUID != crafter)
        return false;

    order->State = CraftingOrders::OrderState::Created;
    order->ClaimEndDate = 0;
    if (order->Type != CraftingOrders::OrderType::Personal)
        order->CrafterGUID.Clear();
    return true;
}

bool CraftingOrderMgr::CancelOrder(uint64 orderId, ObjectGuid customer)
{
    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order || order->CustomerGUID != customer)
        return false;
    if (order->State != CraftingOrders::OrderState::Created)
        return false;   // can't cancel once a crafter has claimed it

    RemoveOrder(orderId);
    return true;
}

void CraftingOrderMgr::RemoveOrder(uint64 orderId)
{
    _orders.erase(orderId);
}

CraftingOrders::Order* CraftingOrderMgr::GetOrder(uint64 orderId)
{
    auto itr = _orders.find(orderId);
    return itr != _orders.end() ? &itr->second : nullptr;
}

CraftingOrders::Order const* CraftingOrderMgr::GetOrder(uint64 orderId) const
{
    auto itr = _orders.find(orderId);
    return itr != _orders.end() ? &itr->second : nullptr;
}

std::vector<CraftingOrders::Order const*> CraftingOrderMgr::ListClaimableForRecipe(int32 skillLineAbilityID) const
{
    std::vector<CraftingOrders::Order const*> result;
    for (auto const& [id, order] : _orders)
        if (order.IsClaimable() && (!skillLineAbilityID || order.SkillLineAbilityID == skillLineAbilityID))
            result.push_back(&order);
    return result;
}

std::vector<CraftingOrders::Order const*> CraftingOrderMgr::ListOrdersByCustomer(ObjectGuid customer) const
{
    std::vector<CraftingOrders::Order const*> result;
    for (auto const& [id, order] : _orders)
        if (order.CustomerGUID == customer)
            result.push_back(&order);
    return result;
}

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
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include <algorithm>

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
    _orders.clear();
    _nextOrderId = 1;

    if (PreparedQueryResult result = CharacterDatabase.Query(CharacterDatabase.GetPreparedStatement(CHAR_SEL_CRAFTING_ORDERS)))
    {
        do
        {
            Field* f = result->Fetch();
            CraftingOrders::Order order;
            order.OrderID            = f[0].GetUInt64();
            order.SkillLineAbilityID = f[1].GetInt32();
            order.State              = CraftingOrders::OrderState(f[2].GetInt8());
            order.Type               = CraftingOrders::OrderType(f[3].GetUInt8());
            order.MinQuality         = f[4].GetUInt32();
            order.EndDate            = f[5].GetInt64();
            order.ClaimEndDate       = f[6].GetInt64();
            order.TipAmount          = f[7].GetUInt64();
            order.HouseCutAmount     = f[8].GetUInt64();
            order.Flags              = f[9].GetInt32();
            if (uint64 low = f[10].GetUInt64())
                order.CustomerGUID = ObjectGuid::Create<HighGuid::Player>(low);
            if (uint64 low = f[11].GetUInt64())
                order.CrafterGUID = ObjectGuid::Create<HighGuid::Player>(low);
            order.CustomerAccountId  = f[12].GetUInt32();
            order.CustomerNotes      = f[13].GetString();

            _nextOrderId = std::max<uint64>(_nextOrderId, order.OrderID + 1);
            _orders[order.OrderID] = std::move(order);
        } while (result->NextRow());
    }

    if (PreparedQueryResult result = CharacterDatabase.Query(CharacterDatabase.GetPreparedStatement(CHAR_SEL_CRAFTING_ORDER_REAGENTS)))
    {
        do
        {
            Field* f = result->Fetch();
            uint64 const orderId = f[0].GetUInt64();
            auto itr = _orders.find(orderId);
            if (itr == _orders.end())
                continue;

            CraftingOrders::OrderReagent reagent;
            reagent.Slot       = f[1].GetUInt8();
            reagent.ItemID     = f[2].GetInt32();
            reagent.CurrencyID = f[3].GetInt32();
            reagent.Quantity   = f[4].GetUInt32();
            itr->second.Reagents.push_back(reagent);
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} crafting orders.", _orders.size());
}

void CraftingOrderMgr::SaveOrderToDB(CraftingOrders::Order const& order) const
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CRAFTING_ORDER);
    uint8 i = 0;
    stmt->setUInt64(i++, order.OrderID);
    stmt->setInt32(i++, order.SkillLineAbilityID);
    stmt->setInt8(i++, int8(order.State));
    stmt->setUInt8(i++, uint8(order.Type));
    stmt->setUInt32(i++, order.MinQuality);
    stmt->setInt64(i++, order.EndDate);
    stmt->setInt64(i++, order.ClaimEndDate);
    stmt->setUInt64(i++, order.TipAmount);
    stmt->setUInt64(i++, order.HouseCutAmount);
    stmt->setInt32(i++, order.Flags);
    stmt->setUInt64(i++, order.CustomerGUID.GetCounter());
    stmt->setUInt64(i++, order.CrafterGUID.GetCounter());
    stmt->setUInt32(i++, order.CustomerAccountId);
    stmt->setString(i++, order.CustomerNotes);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CRAFTING_ORDER_REAGENTS);
    stmt->setUInt64(0, order.OrderID);
    trans->Append(stmt);

    for (CraftingOrders::OrderReagent const& reagent : order.Reagents)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CRAFTING_ORDER_REAGENT);
        stmt->setUInt64(0, order.OrderID);
        stmt->setUInt8(1, reagent.Slot);
        stmt->setInt32(2, reagent.ItemID);
        stmt->setInt32(3, reagent.CurrencyID);
        stmt->setUInt32(4, reagent.Quantity);
        trans->Append(stmt);
    }

    CharacterDatabase.CommitTransaction(trans);
}

void CraftingOrderMgr::DeleteOrderFromDB(uint64 orderId) const
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CRAFTING_ORDER);
    stmt->setUInt64(0, orderId);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CRAFTING_ORDER_REAGENTS);
    stmt->setUInt64(0, orderId);
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
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
            SaveOrderToDB(order);
            ++itr;
        }
        else if (postingExpired)
        {
            // P4: refund the customer's escrowed tip/reagents before erasing.
            uint64 const expiredId = order.OrderID;
            itr = _orders.erase(itr);
            DeleteOrderFromDB(expiredId);
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
    CraftingOrders::Order& stored = (_orders[id] = std::move(order));
    SaveOrderToDB(stored);
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
    SaveOrderToDB(*order);
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
    SaveOrderToDB(*order);
    return true;
}

bool CraftingOrderMgr::RejectOrder(uint64 orderId, ObjectGuid crafter, std::string reason)
{
    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order)
        return false;

    // A crafter may reject an order they have claimed, or a personal order directed specifically at them.
    bool const claimedByCrafter = order->State == CraftingOrders::OrderState::Claimed && order->CrafterGUID == crafter;
    bool const personalForCrafter = order->Type == CraftingOrders::OrderType::Personal && order->CrafterGUID == crafter;
    if (!claimedByCrafter && !personalForCrafter)
        return false;

    order->State = CraftingOrders::OrderState::Rejected;
    order->ClaimEndDate = 0;
    SaveOrderToDB(*order);

    TC_LOG_DEBUG("network", "CraftingOrderMgr: order {} rejected by {} (reason: {})",
        orderId, crafter.ToString(), reason.empty() ? "none" : reason);
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
    if (_orders.erase(orderId))
        DeleteOrderFromDB(orderId);
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

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

#ifndef CraftingOrderMgr_h__
#define CraftingOrderMgr_h__

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <unordered_map>
#include <vector>

class Player;

// Modern crafting/work-order system: a customer posts an order for a recipe (with a tip and optionally provided
// reagents); a crafter browses, claims, and fulfills it; the output is mailed back. Orders are persistent (they
// survive restarts) and globally browsable, unlike the ephemeral LFG registry.
namespace CraftingOrders
{
    // Order lifecycle. Exact wire values are sniff-pending; these names track the client's orderState field.
    enum class OrderState : int32
    {
        None        = 0,
        Created     = 1,   // posted, unclaimed
        Claimed     = 2,   // a crafter reserved it
        Fulfilled   = 3,   // crafted + delivered
        Rejected    = 4,   // crafter declined after claiming
        Cancelled   = 5,   // customer withdrew
        Expired     = 6,
    };

    // Who the order is open to. Wire values sniff-pending.
    enum class OrderType : uint8
    {
        Public      = 0,   // any eligible crafter
        Guild       = 1,   // guildmates only
        Personal    = 2,   // a specific crafter (crafterGUID)
        Npc         = 3,   // NPC-issued work order
    };

    // A reagent the customer provided with the order (item or currency), mirrors CraftingReagentBase.
    struct OrderReagent
    {
        int32 ItemID = 0;
        int32 CurrencyID = 0;
        uint32 Quantity = 0;
        uint8 Slot = 0;
    };

    // One posted order. Field set mirrors the client's JamCraftingOrder (reflection-recovered).
    struct Order
    {
        uint64 OrderID = 0;
        int32 SkillLineAbilityID = 0;          // the recipe
        OrderState State = OrderState::Created;
        OrderType Type = OrderType::Public;
        uint32 MinQuality = 0;
        int64 EndDate = 0;                      // posting expiry
        int64 ClaimEndDate = 0;                 // crafter's deadline once claimed
        uint64 TipAmount = 0;
        uint64 HouseCutAmount = 0;
        int32 Flags = 0;
        ObjectGuid CustomerGUID;
        ObjectGuid CrafterGUID;                 // assigned crafter (personal orders / after claim)
        uint32 CustomerAccountId = 0;
        std::string CustomerNotes;
        std::vector<OrderReagent> Reagents;

        bool IsClaimable() const { return State == OrderState::Created; }
    };
}

class TC_GAME_API CraftingOrderMgr
{
public:
    static CraftingOrderMgr& Instance();

    void LoadFromDB();
    void Update(uint32 diff);                   // expiry ticker

    // Post a new order; returns the new order id (0 on failure). Escrow of tip/reagents is the caller's job (P1).
    uint64 CreateOrder(Player* customer, CraftingOrders::Order order);
    bool ClaimOrder(uint64 orderId, ObjectGuid crafter);
    bool ReleaseOrder(uint64 orderId, ObjectGuid crafter);   // un-claim, back to the pool
    bool RejectOrder(uint64 orderId, ObjectGuid crafter, std::string reason);   // crafter declines
    bool CancelOrder(uint64 orderId, ObjectGuid customer);
    bool FulfillOrder(uint64 orderId, ObjectGuid crafter);   // crafter delivers: Claimed -> Fulfilled, tip paid out
    void RemoveOrder(uint64 orderId);

    CraftingOrders::Order* GetOrder(uint64 orderId);
    CraftingOrders::Order const* GetOrder(uint64 orderId) const;

    // Browse: orders a given crafter may claim for a recipe/profession (0 = any), and a customer's own orders.
    std::vector<CraftingOrders::Order const*> ListClaimableForRecipe(int32 skillLineAbilityID) const;
    std::vector<CraftingOrders::Order const*> ListOrdersByCustomer(ObjectGuid customer) const;

    // NPC (patron) work orders: server-issued orders any eligible crafter can pick up. Content-agnostic — returns
    // whatever OrderType::Npc orders exist in the pool (currently none until NPC-order content is authored).
    std::vector<CraftingOrders::Order const*> ListNpcOrders() const;

private:
    CraftingOrderMgr() = default;

    void SaveOrderToDB(CraftingOrders::Order const& order) const;
    void DeleteOrderFromDB(uint64 orderId) const;

    uint64 _nextOrderId = 1;
    uint32 _expireTimer = 0;
    std::unordered_map<uint64 /*orderId*/, CraftingOrders::Order> _orders;
};

#define sCraftingOrderMgr CraftingOrderMgr::Instance()

#endif // CraftingOrderMgr_h__

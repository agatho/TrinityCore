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
#include "CharacterCache.h"
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
#include "WorldSession.h"
#include <algorithm>

namespace
{
    constexpr uint32 EXPIRE_CHECK_INTERVAL_MS = 30 * IN_MILLISECONDS;

    // Delivers a crafting-order tip by mail (payout to the crafter on fulfil, or refund to the customer when the
    // order dies unfulfilled). Works whether the recipient is online or not. The tip is escrowed from the customer
    // at create time, so this only ever moves already-reserved gold — it never creates any.
    // Appends a tip-money mail to the caller's transaction (does NOT commit): lets the payout ride the same atomic
    // transaction as the state write (used by the fulfil path so item+state+tip are all-or-nothing, anti-abuse G4).
    void MailCraftingOrderTip(CharacterDatabaseTransaction trans, ObjectGuid recipient, uint64 amount, char const* subject, char const* body)
    {
        if (!amount || recipient.IsEmpty())
            return;

        MailDraft(subject, body)
            .AddMoney(amount)
            .SendMailTo(trans, MailReceiver(ObjectAccessor::FindConnectedPlayer(recipient), recipient.GetCounter()),
                MailSender(MAIL_NORMAL, UI64LIT(0), MAIL_STATIONERY_DEFAULT), MAIL_CHECK_MASK_COPIED);
    }

    void MailCraftingOrderTip(ObjectGuid recipient, uint64 amount, char const* subject, char const* body)
    {
        if (!amount || recipient.IsEmpty())
            return;

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        MailCraftingOrderTip(trans, recipient, amount, subject, body);
        CharacterDatabase.CommitTransaction(trans);
    }
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
    SaveOrderToDB(order, trans);
    CharacterDatabase.CommitTransaction(trans);
}

// Appends the order + reagent write to the caller's transaction without committing, so it can be batched atomically
// with a gold debit / other statements (see CreateOrder / FulfillOrder, anti-abuse G3/G4).
void CraftingOrderMgr::SaveOrderToDB(CraftingOrders::Order const& order, CharacterDatabaseTransaction trans) const
{
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
            // Order lapsed unclaimed: refund the customer's escrowed tip, then erase.
            MailCraftingOrderTip(order.CustomerGUID, order.TipAmount,
                "Crafting Order Expired", "Your crafting order expired unclaimed. Your tip has been refunded.");
            uint64 const expiredId = order.OrderID;
            itr = _orders.erase(itr);
            DeleteOrderFromDB(expiredId);
        }
        else
            ++itr;
    }
}

// Recipe-knowledge + skill-floor gate. Any player can send an arbitrary SkillLineAbilityID on the claim/fulfil wire;
// without this a crafter who does not know the recipe could claim it and mint its output for free (anti-abuse G1).
bool CraftingOrderMgr::CanCraft(Player* crafter, CraftingOrders::Order const& order)
{
    if (!crafter)
        return false;

    SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(order.SkillLineAbilityID);
    if (!ability)
        return false;

    // The crafter must actually know the recipe spell (learned/trained it) - not merely send its id.
    if (ability->Spell <= 0 || !crafter->HasSpell(uint32(ability->Spell)))
        return false;

    // ...and meet the recipe's skill-rank floor for the recipe's skill line, when it carries one.
    if (ability->SkillLine && ability->MinSkillLineRank > 0 && crafter->GetSkillValue(ability->SkillLine) < ability->MinSkillLineRank)
        return false;

    // NOTE: order.MinQuality (the customer's minimum requested crafted quality) is intentionally NOT gated here: the
    // achievable craft quality depends on the crafter's live skill/tools/reagents and a full craft simulation, none of
    // which is resolvable at claim time. Enforcing recipe-knowledge + skill-floor is the defensible server-side gate.
    return true;
}

uint64 CraftingOrderMgr::CreateOrder(Player* customer, CraftingOrders::Order order, CharacterDatabaseTransaction trans)
{
    if (!customer)
        return 0;

    order.OrderID = _nextOrderId++;
    order.CustomerGUID = customer->GetGUID();
    order.CustomerAccountId = customer->GetSession()->GetAccountId();
    order.State = CraftingOrders::OrderState::Created;

    uint64 const id = order.OrderID;
    CraftingOrders::Order& stored = (_orders[id] = std::move(order));
    // Append to the caller's transaction (which also carries the gold debit) - committed atomically by the caller.
    SaveOrderToDB(stored, trans);
    return id;
}

bool CraftingOrderMgr::ClaimOrder(uint64 orderId, Player* crafter)
{
    if (!crafter)
        return false;

    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order || !order->IsClaimable())
        return false;

    ObjectGuid const crafterGuid = crafter->GetGUID();

    // Self-order wash guard: a player must not claim their own order, nor an order posted by any character on the
    // same game account. Otherwise post Public -> self-claim -> self-fulfil round-trips a free minted item and the
    // full tip back to the same wallet at zero net cost, and farms the fulfil achievement (anti-abuse G2).
    if (order->CustomerGUID == crafterGuid)
        return false;
    if (order->CustomerAccountId && order->CustomerAccountId == crafter->GetSession()->GetAccountId())
        return false;

    // Recipe authorization: the crafter must actually know the recipe (re-checked again at fulfil) - G1.
    if (!CanCraft(crafter, *order))
        return false;

    // Personal orders may only be claimed by their designated crafter.
    if (order->Type == CraftingOrders::OrderType::Personal && order->CrafterGUID != crafterGuid)
        return false;

    // Guild orders may only be claimed by a member of the customer's guild (anti-abuse G8).
    if (order->Type == CraftingOrders::OrderType::Guild)
    {
        ObjectGuid::LowType const customerGuild = sCharacterCache->GetCharacterGuildIdByGuid(order->CustomerGUID);
        if (!customerGuild || crafter->GetGuildId() != customerGuild)
            return false;
    }

    // Give the crafter a bounded window to fulfil a claimed order. Without this ClaimEndDate stays 0, so the
    // claim-expiry branch in Update() (which requires ClaimEndDate != 0) never fires: a claimed-then-abandoned
    // order would sit in state Claimed forever with the customer's tip locked and the customer unable to cancel
    // (CancelOrder only allows the Created state). CLAIM_DURATION is a safety-release window (tunable) - it just
    // bounds abandonment; a crafter who completes normally never hits it.
    constexpr int64 CLAIM_DURATION = 24 * HOUR;
    order->State = CraftingOrders::OrderState::Claimed;
    order->CrafterGUID = crafterGuid;
    order->ClaimEndDate = GameTime::GetGameTime() + CLAIM_DURATION;
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

    // A crafter may reject an order they have claimed, or a personal order directed specifically at them - but ONLY
    // while it is still in a non-terminal state. A Fulfilled or already-Rejected order must not be rejectable:
    // reject refunds the escrowed tip, and the personal-order branch used to omit the state check, so a targeted
    // crafter could spam CMSG_CRAFTING_ORDER_REJECT against their own already-fulfilled personal order and mint the
    // tip to the customer on every call (gold duplication). Escrow is released exactly once per terminal transition.
    bool const claimedByCrafter = order->State == CraftingOrders::OrderState::Claimed && order->CrafterGUID == crafter;
    bool const personalForCrafter = order->Type == CraftingOrders::OrderType::Personal && order->CrafterGUID == crafter
        && (order->State == CraftingOrders::OrderState::Created || order->State == CraftingOrders::OrderState::Claimed);
    if (!claimedByCrafter && !personalForCrafter)
        return false;

    order->State = CraftingOrders::OrderState::Rejected;
    order->ClaimEndDate = 0;
    SaveOrderToDB(*order);

    // The order is declined and terminal: return the customer's escrowed tip.
    MailCraftingOrderTip(order->CustomerGUID, order->TipAmount,
        "Crafting Order Declined", "The crafter declined your order. Your tip has been refunded.");

    TC_LOG_DEBUG("network", "CraftingOrderMgr: order {} rejected by {} (reason: {})",
        orderId, crafter.ToString(), reason.empty() ? "none" : reason);
    return true;
}

bool CraftingOrderMgr::FulfillOrder(uint64 orderId, Player* crafter)
{
    if (!crafter)
        return false;

    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order || order->State != CraftingOrders::OrderState::Claimed || order->CrafterGUID != crafter->GetGUID())
        return false;

    // Re-validate recipe authorization at fulfil, not only at claim: a crafter could unlearn the recipe (or the claim
    // guard could be bypassed) between claim and fulfil. No output is minted for a recipe the crafter cannot craft (G1).
    if (!CanCraft(crafter, *order))
        return false;

    // Derive the recipe's output (SkillLineAbility -> spell -> SPELL_EFFECT_CREATE_ITEM) BEFORE mutating state so a
    // failure to resolve it aborts the whole fulfil cleanly (order stays Claimed, nothing sent).
    uint32 outItemId = 0;
    uint32 outCount = 1;
    if (SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(order->SkillLineAbilityID))
        if (SpellInfo const* recipe = sSpellMgr->GetSpellInfo(ability->Spell, DIFFICULTY_NONE))
            for (SpellEffectInfo const& effect : recipe->GetEffects())
                if (effect.IsEffect(SPELL_EFFECT_CREATE_ITEM))
                {
                    outItemId = effect.ItemType;
                    outCount = uint32(std::max<int32>(1, effect.CalcValue(crafter)));
                    break;
                }

    ObjectGuid const customerGuid = order->CustomerGUID;
    // House cut is skimmed from the tip and sunk; the crafter is paid the remainder (defaults to the full tip while
    // HouseCutAmount is 0). Deducting it makes a self/collusive wash trade net-negative rather than net-zero (G11).
    uint64 const houseCut = std::min(order->HouseCutAmount, order->TipAmount);
    uint64 const payout = order->TipAmount - houseCut;

    // ONE transaction carries the entire fulfilment: the Claimed->Fulfilled state write (idempotency guard), the
    // crafted item + its delivery mail, and the tip payout. Either every row commits or none does, so a crash mid-way
    // leaves the order Claimed with no item minted and no tip paid; the replay then completes it exactly once. State
    // is written first, so once this commits any replay hits the state!=Claimed guard above and is a no-op (G4).
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    order->State = CraftingOrders::OrderState::Fulfilled;
    order->ClaimEndDate = 0;
    SaveOrderToDB(*order, trans);

    if (outItemId)
    {
        if (Item* crafted = Item::CreateItem(outItemId, outCount, ItemContext::NONE, crafter))
        {
            crafted->SaveToDB(trans);
            MailDraft("Crafting Order Complete", "Your crafted item is enclosed.")
                .AddItem(crafted)
                .SendMailTo(trans, MailReceiver(ObjectAccessor::FindConnectedPlayer(customerGuid), customerGuid.GetCounter()),
                    MailSender(crafter, MAIL_STATIONERY_DEFAULT), MAIL_CHECK_MASK_COPIED);
        }
    }

    MailCraftingOrderTip(trans, order->CrafterGUID, payout,
        "Crafting Order Payment", "Payment for a completed crafting order is enclosed.");

    CharacterDatabase.CommitTransaction(trans);
    return true;
}

bool CraftingOrderMgr::CancelOrder(uint64 orderId, ObjectGuid customer)
{
    CraftingOrders::Order* order = GetOrder(orderId);
    if (!order || order->CustomerGUID != customer)
        return false;
    if (order->State != CraftingOrders::OrderState::Created)
        return false;   // can't cancel once a crafter has claimed it

    // Refund the customer's escrowed tip before the order is erased.
    MailCraftingOrderTip(order->CustomerGUID, order->TipAmount,
        "Crafting Order Cancelled", "You cancelled your crafting order. Your tip has been refunded.");

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

std::vector<CraftingOrders::Order const*> CraftingOrderMgr::ListNpcOrders() const
{
    std::vector<CraftingOrders::Order const*> result;
    for (auto const& [id, order] : _orders)
        if (order.Type == CraftingOrders::OrderType::Npc && order.IsClaimable())
            result.push_back(&order);
    return result;
}

void CraftingOrderMgr::SetIgnoreList(ObjectGuid owner, std::vector<ObjectGuid> ignored)
{
    if (ignored.empty())
        _ignoreLists.erase(owner);
    else
        _ignoreLists[owner] = std::move(ignored);
}

bool CraftingOrderMgr::IsIgnoring(ObjectGuid owner, ObjectGuid other) const
{
    auto it = _ignoreLists.find(owner);
    if (it == _ignoreLists.end())
        return false;
    return std::find(it->second.begin(), it->second.end(), other) != it->second.end();
}

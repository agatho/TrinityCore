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

#include "ItemConversionMgr.h"
#include "Config.h"
#include "ConditionMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"

ItemConversionMgr& ItemConversionMgr::Instance()
{
    static ItemConversionMgr instance;
    return instance;
}

void ItemConversionMgr::Initialize()
{
    _conversionsByItem.clear();
    _outputsByConversion.clear();

    // Eligibility: ItemConversionEntry.db2 lists the input items of each conversion set.
    for (ItemConversionEntryEntry const* entry : sItemConversionEntryStore)
        if (entry->ItemID > 0 && entry->ItemConversionID)
            _conversionsByItem[uint32(entry->ItemID)].push_back(entry->ItemConversionID);

    // Output side: server content (the class/slot -> tier piece mapping is not in any client DB2).
    uint32 outputCount = 0;
    if (QueryResult result = WorldDatabase.Query("SELECT ItemConversionID, ClassID, InventoryType, InputItemID, OutputItemID FROM item_conversion_output"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 const conversionId = fields[0].GetUInt32();

            OutputRow row;
            row.ClassID = fields[1].GetUInt8();
            row.InventoryType = fields[2].GetUInt8();
            row.InputItemID = fields[3].GetUInt32();
            row.OutputItemID = fields[4].GetUInt32();

            if (!sObjectMgr->GetItemTemplate(row.OutputItemID))
            {
                TC_LOG_ERROR("sql.sql", "Table item_conversion_output: conversion {} output item {} does not exist, skipped.",
                    conversionId, row.OutputItemID);
                continue;
            }

            _outputsByConversion[conversionId].push_back(row);
            ++outputCount;
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} item conversion sets ({} eligible items, {} output mappings).",
        sItemConversionStore.GetNumRows(), _conversionsByItem.size(), outputCount);
}

uint32 ItemConversionMgr::GetConversionForItem(Player const* player, Item const* item) const
{
    auto itr = _conversionsByItem.find(item->GetEntry());
    if (itr == _conversionsByItem.end())
        return 0;

    for (uint32 conversionId : itr->second)
    {
        ItemConversionEntry const* conversion = sItemConversionStore.LookupEntry(conversionId);
        if (!conversion)
            continue;

        // Gate on the conversion set's player condition (season/quest gating on retail).
        if (conversion->PlayerConditionID)
            if (PlayerConditionEntry const* condition = sPlayerConditionStore.LookupEntry(uint32(conversion->PlayerConditionID)))
                if (!ConditionMgr::IsPlayerMeetingCondition(player, condition))
                    continue;

        return conversionId;
    }

    return 0;
}

uint32 ItemConversionMgr::GetConversionOutput(Player const* player, uint32 itemConversionId, Item const* item) const
{
    auto itr = _outputsByConversion.find(itemConversionId);
    if (itr == _outputsByConversion.end())
        return 0;

    uint8 const classId = uint8(player->GetClass());
    uint8 const invType = uint8(item->GetTemplate()->GetInventoryType());

    // Exact input mapping wins; otherwise the most specific (class, invType) wildcard row.
    uint32 wildcardOutput = 0;
    int32 bestSpecificity = -1;
    for (OutputRow const& row : itr->second)
    {
        if (row.InputItemID)
        {
            if (row.InputItemID == item->GetEntry())
                return row.OutputItemID;
            continue;
        }

        if (row.ClassID && row.ClassID != classId)
            continue;
        if (row.InventoryType && row.InventoryType != invType)
            continue;

        int32 const specificity = (row.ClassID ? 2 : 0) + (row.InventoryType ? 1 : 0);
        if (specificity > bestSpecificity)
        {
            bestSpecificity = specificity;
            wildcardOutput = row.OutputItemID;
        }
    }

    return wildcardOutput;
}

uint32 ItemConversionMgr::GetChargeCurrencyId() const
{
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Catalyst.CurrencyId", 3378)); // Dawnlight Manaflux
}

uint32 ItemConversionMgr::GetChargeCost() const
{
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Catalyst.Cost", 1));
}

void ItemConversionMgr::UpdateCharges(Player* player) const
{
    uint32 const currencyId = GetChargeCurrencyId();
    if (!currencyId || !sCurrencyTypesStore.LookupEntry(currencyId))
        return;

    // Retail drip: 1 charge when the season starts, +1 every CycleWeeks, hard-capped at MaxCharges lifetime
    // accrual (no catch-up beyond the cap). FirstWeekTimestamp anchors the drip; 0 disables automatic charges.
    int64 const firstWeek = sConfigMgr->GetInt64Default("ChallengeMode.Catalyst.FirstWeekTimestamp", 0);
    if (!firstWeek)
        return;

    int64 const now = GameTime::GetGameTime();
    if (now < firstWeek)
        return;

    uint32 const cycleWeeks = std::max(uint32(sConfigMgr->GetIntDefault("ChallengeMode.Catalyst.CycleWeeks", 2)), 1u);
    uint32 const maxCharges = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Catalyst.MaxCharges", 8));

    uint32 const accrued = std::min<uint32>(1 + uint32((now - firstWeek) / (int64(WEEK) * cycleWeeks)), maxCharges);

    // Tracked quantity is the lifetime-earned amount, so spending charges never re-grants them.
    uint32 const earned = player->GetCurrencyTrackedQuantity(currencyId);
    if (earned < accrued)
        player->AddCurrency(currencyId, accrued - earned, CurrencyGainSource::Script);
}

Item* ItemConversionMgr::PerformConversion(Player* player, Item* item) const
{
    uint32 const conversionId = GetConversionForItem(player, item);
    if (!conversionId)
        return nullptr;

    uint32 const outputItemId = GetConversionOutput(player, conversionId, item);
    if (!outputItemId || outputItemId == item->GetEntry())
        return nullptr;

    uint32 const currencyId = GetChargeCurrencyId();
    uint32 const cost = GetChargeCost();
    if (currencyId && cost)
        if (player->GetCurrencyQuantity(currencyId) < cost)
            return nullptr;

    // Snapshot what survives the conversion before the input is destroyed: the bonus lists carry item level /
    // upgrade track / sockets / tertiaries; the context keeps reward-track semantics. The output's own stat
    // profile comes from its template.
    std::vector<int32> const bonuses = item->GetBonusListIDs();
    ItemContext const context = item->GetContext();
    uint32 const inputItemId = item->GetEntry();
    uint8 const bagSlot = item->GetBagSlot();
    uint8 const slot = item->GetSlot();

    player->DestroyItem(bagSlot, slot, true);
    item = nullptr; // destroyed above

    ItemPosCountVec dest;
    Item* converted = nullptr;
    if (player->CanStoreNewItem(bagSlot, slot, dest, outputItemId, 1) == EQUIP_ERR_OK
        || player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, outputItemId, 1) == EQUIP_ERR_OK)
        converted = player->StoreNewItem(dest, outputItemId, true, 0, GuidSet(), context, &bonuses);

    if (!converted)
    {
        TC_LOG_ERROR("entities.player.items", "ItemConversionMgr: failed to store converted item {} for player {}.",
            outputItemId, player->GetGUID().ToString());
        return nullptr;
    }

    if (currencyId && cost)
        player->RemoveCurrency(currencyId, int32(cost), CurrencyDestroyReason::Vendor);

    TC_LOG_INFO("entities.player.items", "ItemConversionMgr: player {} converted item {} -> {} (conversion set {}).",
        player->GetGUID().ToString(), inputItemId, outputItemId, conversionId);
    return converted;
}

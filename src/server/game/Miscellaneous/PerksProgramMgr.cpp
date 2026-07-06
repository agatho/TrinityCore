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

#include "PerksProgramMgr.h"
#include "DB2Stores.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "PerksProgramPackets.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

PerksProgramMgr* PerksProgramMgr::instance()
{
    static PerksProgramMgr instance;
    return &instance;
}

void PerksProgramMgr::BuildVendorList()
{
    _vendorItems.clear();

    // Spells that teach a mount, so a vendor item's teaching effect can be mapped back to its mount.
    std::unordered_set<int32> mountSpells;
    for (MountEntry const* mount : sMountStore)
        if (mount->SourceSpellID > 0)
            mountSpells.insert(mount->SourceSpellID);

    // Default modified appearance (modifier 0) per item, for transmog/ensemble vendor items.
    std::unordered_map<uint32, int32> itemAppearance;
    for (ItemModifiedAppearanceEntry const* appearance : sItemModifiedAppearanceStore)
        if (appearance->ItemAppearanceModifierID == 0)
            itemAppearance.try_emplace(appearance->ItemID, int32(appearance->ID));

    // The trading post shows the current interval. Without a live rotation calendar we take the
    // newest interval present in the data (the group(s) with the highest PerksMonth) as "current";
    // if the interval tables are empty we fall back to offering the whole catalogue.
    int32 currentMonth = -1;
    for (PerksActivityThresholdGroupEntry const* group : sPerksActivityThresholdGroupStore)
        currentMonth = std::max(currentMonth, group->PerksMonth);

    std::unordered_set<uint32> currentThresholds;
    if (currentMonth >= 0)
    {
        std::unordered_set<uint32> currentGroups;
        for (PerksActivityThresholdGroupEntry const* group : sPerksActivityThresholdGroupStore)
            if (group->PerksMonth == currentMonth)
                currentGroups.insert(group->ID);

        for (PerksActivityThresholdEntry const* threshold : sPerksActivityThresholdStore)
            if (currentGroups.count(threshold->PerksActivityThresholdGroupID))
                currentThresholds.insert(threshold->ID);
    }

    std::unordered_set<uint32> allowedVendorItems;
    for (PerksVendorItemXIntervalEntry const* link : sPerksVendorItemXIntervalStore)
        if (currentThresholds.empty() || currentThresholds.count(link->PerksActivityThresholdID))
            allowedVendorItems.insert(link->PerksVendorItemID);

    for (PerksVendorItemEntry const* entry : sPerksVendorItemStore)
    {
        if (!allowedVendorItems.empty() && !allowedVendorItems.count(entry->ID))
            continue;

        WorldPackets::PerksProgram::PerksVendorItem vendorItem;
        vendorItem.VendorItemID = int32(entry->ID);
        vendorItem.Price = entry->Cost;
        vendorItem.OriginalPrice = entry->Cost;
        vendorItem.DoesNotExpire = true;

        if (entry->ItemID)
        {
            if (auto itr = itemAppearance.find(entry->ItemID); itr != itemAppearance.end())
                vendorItem.ItemModifiedAppearanceID = itr->second;

            if (sDB2Manager.IsToyItem(entry->ItemID))
                vendorItem.ToyID = entry->ItemID;

            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry->ItemID))
                for (ItemEffectEntry const* effect : proto->Effects)
                    if (mountSpells.count(effect->SpellID))
                        vendorItem.MountID = effect->SpellID;
        }

        _vendorItems.push_back(std::move(vendorItem));
    }

    _loaded = true;
    TC_LOG_INFO("server.loading", ">> Built {} Perks Program vendor items (perks month {}).",
        _vendorItems.size(), currentMonth);
}

std::vector<WorldPackets::PerksProgram::PerksVendorItem> const& PerksProgramMgr::GetCurrentVendorItems()
{
    if (!_loaded)
        BuildVendorList();

    return _vendorItems;
}

WorldPackets::PerksProgram::PerksVendorItem const* PerksProgramMgr::GetVendorItem(int32 vendorItemId)
{
    for (WorldPackets::PerksProgram::PerksVendorItem const& item : GetCurrentVendorItems())
        if (item.VendorItemID == vendorItemId)
            return &item;

    return nullptr;
}

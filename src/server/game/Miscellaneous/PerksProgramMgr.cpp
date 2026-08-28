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
#include "GameTime.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "PerksProgramPackets.h"
#include "Util.h"
#include "World.h"
#include <algorithm>
#include <ctime>
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
    _catalogue.clear();

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
        // Every catalogue row is resolved, not just the ones offered this rotation: the client asks for the
        // display data of items it bought in EARLIER rotations (CMSG_PERKS_PROGRAM_ITEMS_REFRESHED), and the
        // expensive lookup tables above are only built once here.
        bool const offeredNow = allowedVendorItems.empty() || allowedVendorItems.count(entry->ID) != 0;

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

        _catalogue[int32(entry->ID)] = vendorItem;
        if (offeredNow)
            _vendorItems.push_back(std::move(vendorItem));
    }

    _loaded = true;
    ++_listingGeneration;
    TC_LOG_INFO("server.loading", ">> Built {} Perks Program vendor items (perks month {}), {} in the full catalogue.",
        _vendorItems.size(), currentMonth, _catalogue.size());
}

// The offering is a per-rotation set, so the listing stays valid exactly as long as the Trading Post period it
// was built for does. Rebuilding when that period rolls over is the one runtime moment the offering changes.
//
// With the DB2 data alone the rebuilt set is usually identical -- BuildVendorList derives "current" from the
// highest PerksMonth present in the tables, which does not advance by itself. That is a property of the data,
// not of this check: whatever supplies a live rotation later (a calendar table, a rotation script calling
// Reload()) passes through here, and everything downstream already reacts correctly. Reload() clears _loaded,
// which lands in the same rebuild and bumps the same generation, so an operator reload reaches open windows too.
//
// Pointer lifetime: GetVendorItem hands out pointers into _vendorItems, which a rebuild invalidates. That is
// safe because the period is derived from GameTime, which is fixed for the whole world tick: the stamp is
// updated in the same call that rebuilds, so at most ONE rebuild can happen per tick and it happens on the
// first access of that tick -- before any handler has been given a pointer to hold.
void PerksProgramMgr::EnsureCurrent()
{
    uint64 periodStart = 0;
    uint64 periodEnd = 0;
    GetCurrentPeriod(periodStart, periodEnd);

    if (_loaded && _listingPeriod == periodStart)
        return;

    BuildVendorList();
    _listingPeriod = periodStart;
}

uint32 PerksProgramMgr::GetListingGeneration()
{
    EnsureCurrent();
    return _listingGeneration;
}

std::vector<WorldPackets::PerksProgram::PerksVendorItem> const& PerksProgramMgr::GetCurrentVendorItems()
{
    EnsureCurrent();

    return _vendorItems;
}

WorldPackets::PerksProgram::PerksVendorItem const* PerksProgramMgr::GetCatalogueVendorItem(int32 vendorItemId)
{
    EnsureCurrent();

    auto itr = _catalogue.find(vendorItemId);
    return itr != _catalogue.end() ? &itr->second : nullptr;
}

void PerksProgramMgr::GetCurrentPeriod(uint64& periodStart, uint64& periodEnd) const
{
    // The Trading Post period is the current calendar month, rolling over on the 1st at
    // PerksProgram.ResetHour UTC -- NOT at midnight. Every captured SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE
    // carries a non-midnight boundary pair: 15:00 UTC in the 12.0.x recordings (builds 65940-69273)
    // and 04:00 UTC in the two 12.1 recordings (69382/69404). That split is all the measurement shows,
    // and it falls exactly on the build boundary; the default 4 is simply the 12.1 value.
    // UNVERIFIED: WHY the hour differs. Region is the obvious candidate, but no recording carries a region
    // marker, so the split cannot be attributed to one -- it is a config value because it is observed to
    // vary, not because it is proven to be regional.
    int32 const resetHour = sWorld->getIntConfig(CONFIG_PERKS_PROGRAM_RESET_HOUR);

    time_t now = GameTime::GetGameTime();
    tm date;
    gmtime_r(&now, &date);
    date.tm_mday = 1;
    date.tm_hour = resetHour;
    date.tm_min = 0;
    date.tm_sec = 0;

    // Before the reset hour on the 1st we are still inside the PREVIOUS month's period; stepping the
    // month back keeps periodStart <= now < periodEnd, which is what the client's countdown assumes.
    if (now < timegm(&date))
    {
        if (--date.tm_mon < 0)
        {
            date.tm_mon = 11;
            --date.tm_year;
        }
    }
    periodStart = uint64(timegm(&date));

    if (++date.tm_mon > 11)
    {
        date.tm_mon = 0;
        ++date.tm_year;
    }
    periodEnd = uint64(timegm(&date));
}

WorldPackets::PerksProgram::PerksVendorItem const* PerksProgramMgr::GetVendorItem(int32 vendorItemId)
{
    for (WorldPackets::PerksProgram::PerksVendorItem const& item : GetCurrentVendorItems())
        if (item.VendorItemID == vendorItemId)
            return &item;

    return nullptr;
}

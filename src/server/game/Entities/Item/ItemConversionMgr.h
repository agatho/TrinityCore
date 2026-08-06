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

#ifndef ItemConversionMgr_h__
#define ItemConversionMgr_h__

#include "Define.h"
#include <unordered_map>
#include <vector>

class Item;
class Player;

// Matrix Catalyst-style item conversion (retail "Item Interaction" conversions, 12.x Matrix Catalyst).
// The conversion FRAMEWORK is data-driven from the client's ItemConversion.db2 / ItemConversionEntry.db2:
// which items are eligible for which conversion set, gated by the set's PlayerCondition. The conversion
// OUTPUT (which tier piece an input becomes, per class and slot) is not present in any client DB2 -- it is
// server content, resolved from the item_conversion_output world table. Charges are a tracked currency
// (12.x: Dawnlight Manaflux 3378) with a biweekly accrual, both config-tunable (ChallengeMode.Catalyst.*).
class TC_GAME_API ItemConversionMgr
{
public:
    ItemConversionMgr(ItemConversionMgr const&) = delete;
    ItemConversionMgr& operator=(ItemConversionMgr const&) = delete;

    static ItemConversionMgr& Instance();

    // Builds the DB2 eligibility lookup and loads item_conversion_output. Call after DB2 + world DB load.
    void Initialize();

    // The conversion set usable by this player for this item (eligibility from ItemConversionEntry.db2,
    // gated by the set's PlayerCondition), or 0 when the item is not convertible.
    uint32 GetConversionForItem(Player const* player, Item const* item) const;

    // The output item the input converts into: item_conversion_output rows for the conversion set, matched by
    // explicit InputItemID first, then by (ClassID, InventoryType) with 0 acting as a wildcard. 0 = no mapping.
    uint32 GetConversionOutput(Player const* player, uint32 itemConversionId, Item const* item) const;

    // --- charges (Dawnlight Manaflux) ---
    uint32 GetChargeCurrencyId() const;
    uint32 GetChargeCost() const;
    // Lazily grants accrued charges (1 at season start, +1 per CycleWeeks, capped at MaxCharges) by topping the
    // currency's tracked (lifetime-earned) quantity up to the accrual expected this week. Call on login.
    void UpdateCharges(Player* player) const;

    // Full conversion: eligibility + output + charge cost, then replaces the item in place, preserving its
    // bonus lists (item level / upgrade track / sockets / tertiary) and context. Returns the new item or nullptr.
    Item* PerformConversion(Player* player, Item* item) const;

private:
    ItemConversionMgr() = default;
    ~ItemConversionMgr() = default;

    struct OutputRow
    {
        uint8 ClassID = 0;          // 0 = any class
        uint8 InventoryType = 0;    // 0 = any inventory type
        uint32 InputItemID = 0;     // 0 = any eligible input
        uint32 OutputItemID = 0;
    };

    std::unordered_map<uint32 /*itemId*/, std::vector<uint32> /*itemConversionIds*/> _conversionsByItem;
    std::unordered_map<uint32 /*itemConversionId*/, std::vector<OutputRow>> _outputsByConversion;
};

#define sItemConversionMgr ItemConversionMgr::Instance()

#endif // ItemConversionMgr_h__

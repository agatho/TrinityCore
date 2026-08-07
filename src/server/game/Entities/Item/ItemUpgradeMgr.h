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

#ifndef ItemUpgradeMgr_h__
#define ItemUpgradeMgr_h__

#include "Define.h"
#include <unordered_map>
#include <vector>

class Item;
class Player;
struct ItemBonusListGroupEntryEntry;

// Retail 12.x gear upgrade system ("Dawncrest spending"). Fully data-driven from client DB2s:
// ItemBonusListGroupEntry defines each upgrade track (group = track, SequenceValue = rank, ItemBonusListID =
// the rank's bonus list); the per-step cost resolves ItemLogicalCostGroupID -> ItemLogicalCost (filtered by
// the item's inventory-type slot mask) -> ItemExtendedCost (crest currency + amount + gold). The client casts
// the item's upgrade spell (SPELL_EFFECT_INCREASE_ITEM_BONUS_LIST_GROUP_STEP); pricing/preview is computed
// client-side from the same DB2s plus the ActivePlayerData ItemUpgradeHighWatermark array.
class TC_GAME_API ItemUpgradeMgr
{
public:
    // Number of watermark slot classes the client tracks (ActivePlayerData::ItemUpgradeHighWatermark).
    static constexpr uint32 WATERMARK_SLOTS = 17;

    ItemUpgradeMgr(ItemUpgradeMgr const&) = delete;
    ItemUpgradeMgr& operator=(ItemUpgradeMgr const&) = delete;

    static ItemUpgradeMgr& Instance();

    // Builds the bonus-list -> track-entry reverse lookup. Call after DB2 load.
    void Initialize();

    struct UpgradeStep
    {
        ItemBonusListGroupEntryEntry const* CurrentRank = nullptr;
        ItemBonusListGroupEntryEntry const* NextRank = nullptr;
        uint32 CurrencyID = 0;
        uint32 CurrencyCount = 0;
        uint64 Money = 0;
    };

    // Resolves the item's upgrade track position and the next step's cost (discounts applied for player).
    // Returns false when the item has no track or is already at the final rank.
    bool GetNextStep(Player const* player, Item const* item, UpgradeStep& step) const;

    // Performs one upgrade step: validates, charges crests + gold, swaps the rank bonus list in place
    // (sockets/enchants/tertiaries preserved), refreshes stats and raises the slot watermark.
    bool PerformUpgrade(Player* player, Item* item) const;

    // Sets an item's track position to an explicit ItemBonusListGroupEntry (SPELL_EFFECT 266; no cost).
    bool SetGroupEntry(Player* player, Item* item, uint32 itemBonusListGroupEntryId) const;

    // Watermark (per-slot-class highest paid-for item level, drives the client's discount display and the
    // crest waiver). Loaded at login into ActivePlayerData; persisted per character.
    void LoadWatermarks(Player* player) const;

private:
    ItemUpgradeMgr() = default;
    ~ItemUpgradeMgr() = default;

    bool ApplyRankChange(Player* player, Item* item, ItemBonusListGroupEntryEntry const* from, ItemBonusListGroupEntryEntry const* to) const;
    void RaiseWatermark(Player* player, Item const* item) const;
    static uint32 WatermarkSlot(uint32 inventoryType);

    std::unordered_map<int32 /*bonusListId*/, ItemBonusListGroupEntryEntry const*> _entryByBonusList;
    std::unordered_map<uint32 /*groupId*/, std::vector<ItemBonusListGroupEntryEntry const*>> _entriesByGroup; // SequenceValue-sorted
};

#define sItemUpgradeMgr ItemUpgradeMgr::Instance()

#endif // ItemUpgradeMgr_h__

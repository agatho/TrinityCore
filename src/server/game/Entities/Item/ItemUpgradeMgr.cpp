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

#include "ItemUpgradeMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Player.h"
#include "StringConvert.h"
#include <algorithm>
#include <sstream>

ItemUpgradeMgr& ItemUpgradeMgr::Instance()
{
    static ItemUpgradeMgr instance;
    return instance;
}

void ItemUpgradeMgr::Initialize()
{
    _entryByBonusList.clear();
    _entriesByGroup.clear();

    for (ItemBonusListGroupEntryEntry const* entry : sItemBonusListGroupEntryStore)
    {
        // Only rows that are part of a cost-bearing progression are upgrade ranks; rows without a logical
        // cost group still define the track position (rank 1 is typically free/implicit).
        _entryByBonusList.emplace(entry->ItemBonusListID, entry);
        _entriesByGroup[entry->ItemBonusListGroupID].push_back(entry);
    }

    for (auto& [groupId, entries] : _entriesByGroup)
        std::sort(entries.begin(), entries.end(), [](ItemBonusListGroupEntryEntry const* a, ItemBonusListGroupEntryEntry const* b)
        {
            return a->SequenceValue < b->SequenceValue;
        });

    TC_LOG_INFO("server.loading", ">> Loaded {} item upgrade tracks ({} rank bonus lists).",
        _entriesByGroup.size(), _entryByBonusList.size());
}

bool ItemUpgradeMgr::GetNextStep(Player const* player, Item const* item, UpgradeStep& step) const
{
    // Locate the item's track position: the (single) bonus list on the item that belongs to a bonus list group.
    for (int32 bonusListId : item->GetBonusListIDs())
    {
        auto itr = _entryByBonusList.find(bonusListId);
        if (itr == _entryByBonusList.end())
            continue;

        step.CurrentRank = itr->second;
        break;
    }

    if (!step.CurrentRank)
        return false;   // no upgrade track (crafted/PvP/old gear)

    auto groupItr = _entriesByGroup.find(step.CurrentRank->ItemBonusListGroupID);
    if (groupItr == _entriesByGroup.end())
        return false;

    std::vector<ItemBonusListGroupEntryEntry const*> const& ranks = groupItr->second;
    auto rankItr = std::find(ranks.begin(), ranks.end(), step.CurrentRank);
    if (rankItr == ranks.end() || rankItr + 1 == ranks.end())
        return false;   // already at the final rank

    step.NextRank = *(rankItr + 1);

    // Step cost: the next rank's logical cost group, filtered by the item's inventory-type slot mask, resolved
    // through ItemExtendedCost (crest currency + amount; Money carries the gold fee).
    if (step.NextRank->ItemLogicalCostGroupID)
    {
        uint32 const invTypeBit = 1u << uint32(item->GetTemplate()->GetInventoryType());
        for (ItemLogicalCostEntry const* logicalCost : sItemLogicalCostStore)
        {
            if (logicalCost->ItemLogicalCostGroupID != uint32(step.NextRank->ItemLogicalCostGroupID))
                continue;
            if (logicalCost->InventoryTypeSlotMask && !(uint32(logicalCost->InventoryTypeSlotMask) & invTypeBit))
                continue;

            if (ItemExtendedCostEntry const* extendedCost = sItemExtendedCostStore.LookupEntry(uint32(logicalCost->ItemExtendedCostID)))
            {
                step.Money = extendedCost->Money;
                for (std::size_t i = 0; i < extendedCost->CurrencyID.size(); ++i)
                {
                    if (extendedCost->CurrencyID[i] && extendedCost->CurrencyCount[i])
                    {
                        step.CurrencyID = extendedCost->CurrencyID[i];
                        step.CurrencyCount = extendedCost->CurrencyCount[i];
                        break;
                    }
                }
            }
            break;
        }
    }

    // Config fallback for stripped/absent cost data: flat crests of the season crest tier + flat gold.
    if (!step.CurrencyID)
    {
        step.CurrencyID = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Upgrade.FallbackCurrencyId", 0));
        step.CurrencyCount = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Upgrade.FallbackCurrencyCount", 20));
        if (!step.Money)
            step.Money = uint64(sConfigMgr->GetIntDefault("ChallengeMode.Upgrade.FallbackMoney", 150 * GOLD));
    }

    if (player && step.CurrencyID && step.CurrencyCount)
    {
        // Warband ".. of the Dawn" achievement: 50% crest discount for the matching crest type
        // ("currencyId:achievementId" pairs, retail 12.x defaults).
        std::string const pairs = sConfigMgr->GetStringDefault("ChallengeMode.Upgrade.DiscountAchievements",
            "3383:61809,3341:42767,3343:42768,3345:42769,3347:42770");
        std::stringstream ss(pairs);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            std::size_t const sep = token.find(':');
            if (sep == std::string::npos)
                continue;
            Optional<uint32> currencyId = Trinity::StringTo<uint32>(std::string_view(token).substr(0, sep));
            Optional<uint32> achievementId = Trinity::StringTo<uint32>(std::string_view(token).substr(sep + 1));
            if (currencyId && achievementId && *currencyId == step.CurrencyID && player->HasAchieved(*achievementId))
            {
                step.CurrencyCount = std::max<uint32>(step.CurrencyCount / 2, 1);
                break;
            }
        }

        // Per-slot high-watermark: crests are waived (gold only) up to the highest item level this slot class
        // has already paid for (warband on retail; per character here).
        uint32 const slot = WatermarkSlot(uint32(item->GetTemplate()->GetInventoryType()));
        if (slot < WATERMARK_SLOTS && player->m_activePlayerData->ItemUpgradeHighWatermark[slot] > float(item->GetItemLevel(player)))
            step.CurrencyCount = 0;
    }

    return true;
}

bool ItemUpgradeMgr::PerformUpgrade(Player* player, Item* item) const
{
    UpgradeStep step;
    if (!GetNextStep(player, item, step))
        return false;

    if (step.CurrencyID && step.CurrencyCount && player->GetCurrencyQuantity(step.CurrencyID) < step.CurrencyCount)
        return false;
    if (step.Money && !player->HasEnoughMoney(int64(step.Money)))
        return false;

    if (!ApplyRankChange(player, item, step.CurrentRank, step.NextRank))
        return false;

    if (step.CurrencyID && step.CurrencyCount)
        player->RemoveCurrency(step.CurrencyID, int32(step.CurrencyCount), CurrencyDestroyReason::Vendor);
    if (step.Money)
        player->ModifyMoney(-int64(step.Money));

    RaiseWatermark(player, item);

    TC_LOG_DEBUG("entities.player.items", "ItemUpgradeMgr: player {} upgraded item {} (bonus {} -> {}), cost {}x{} + {} money.",
        player->GetGUID().ToString(), item->GetEntry(), step.CurrentRank->ItemBonusListID, step.NextRank->ItemBonusListID,
        step.CurrencyCount, step.CurrencyID, step.Money);
    return true;
}

bool ItemUpgradeMgr::SetGroupEntry(Player* player, Item* item, uint32 itemBonusListGroupEntryId) const
{
    ItemBonusListGroupEntryEntry const* target = sItemBonusListGroupEntryStore.LookupEntry(itemBonusListGroupEntryId);
    if (!target)
        return false;

    // Find the item's current entry within the same group (may be absent when the spell force-sets a track).
    ItemBonusListGroupEntryEntry const* current = nullptr;
    for (int32 bonusListId : item->GetBonusListIDs())
    {
        auto itr = _entryByBonusList.find(bonusListId);
        if (itr != _entryByBonusList.end() && itr->second->ItemBonusListGroupID == target->ItemBonusListGroupID)
        {
            current = itr->second;
            break;
        }
    }

    return ApplyRankChange(player, item, current, target);
}

bool ItemUpgradeMgr::ApplyRankChange(Player* player, Item* item, ItemBonusListGroupEntryEntry const* from, ItemBonusListGroupEntryEntry const* to) const
{
    if (!to || from == to)
        return false;

    std::vector<int32> bonuses = item->GetBonusListIDs();
    if (from)
        std::erase(bonuses, from->ItemBonusListID);
    if (std::find(bonuses.begin(), bonuses.end(), to->ItemBonusListID) == bonuses.end())
        bonuses.push_back(to->ItemBonusListID);

    // Swap the rank bonus in place: unapply equipped stats, rebuild the bonus data from scratch (SetBonuses
    // appends, so reset first), reapply. Sockets/enchants/gems/tertiaries live in other bonus lists /
    // enchantment slots and are untouched.
    uint8 const slot = item->GetSlot();
    bool const equipped = item->IsEquipped();
    if (equipped)
        player->_ApplyItemMods(item, slot, false);

    item->_bonusData.Initialize(item->GetTemplate());
    item->SetBonuses(std::move(bonuses));

    if (equipped)
        player->_ApplyItemMods(item, slot, true);

    item->SetState(ITEM_CHANGED, player);
    return true;
}

void ItemUpgradeMgr::RaiseWatermark(Player* player, Item const* item) const
{
    uint32 const slot = WatermarkSlot(uint32(item->GetTemplate()->GetInventoryType()));
    if (slot >= WATERMARK_SLOTS)
        return;

    float const newLevel = float(item->GetItemLevel(player));
    if (player->m_activePlayerData->ItemUpgradeHighWatermark[slot] >= newLevel)
        return;

    player->SetUpdateFieldValue(player->m_values.ModifyValue(&Player::m_activePlayerData)
        .ModifyValue(&UF::ActivePlayerData::ItemUpgradeHighWatermark, slot), newLevel);

    CharacterDatabase.PExecute("REPLACE INTO character_item_upgrade_watermark (guid, slotClass, itemLevel) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), slot, uint32(newLevel));
}

void ItemUpgradeMgr::LoadWatermarks(Player* player) const
{
    if (QueryResult result = CharacterDatabase.PQuery("SELECT slotClass, itemLevel FROM character_item_upgrade_watermark WHERE guid = {}",
        player->GetGUID().GetCounter()))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 const slot = fields[0].GetUInt32();
            if (slot < WATERMARK_SLOTS)
                player->SetUpdateFieldValue(player->m_values.ModifyValue(&Player::m_activePlayerData)
                    .ModifyValue(&UF::ActivePlayerData::ItemUpgradeHighWatermark, slot), float(fields[1].GetUInt32()));
        } while (result->NextRow());
    }
}

uint32 ItemUpgradeMgr::WatermarkSlot(uint32 inventoryType)
{
    // Slot-class mapping for the 17-entry client watermark array (best-effort; verify indices on capture).
    switch (inventoryType)
    {
        case INVTYPE_HEAD:          return 0;
        case INVTYPE_NECK:          return 1;
        case INVTYPE_SHOULDERS:     return 2;
        case INVTYPE_CLOAK:         return 3;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:          return 4;
        case INVTYPE_WRISTS:        return 5;
        case INVTYPE_HANDS:         return 6;
        case INVTYPE_WAIST:         return 7;
        case INVTYPE_LEGS:          return 8;
        case INVTYPE_FEET:          return 9;
        case INVTYPE_FINGER:        return 10;
        case INVTYPE_TRINKET:       return 11;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND: return 12;
        case INVTYPE_SHIELD:
        case INVTYPE_HOLDABLE:
        case INVTYPE_WEAPONOFFHAND: return 13;
        case INVTYPE_2HWEAPON:      return 14;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:   return 15;
        default:                    return 16;
    }
}

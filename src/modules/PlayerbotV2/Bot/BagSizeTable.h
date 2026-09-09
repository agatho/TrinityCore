// BagSizeTable - curated table of "universally available" general-purpose
// bag entries by capacity. Used by the vendor-visit FSM's bag-upgrade phase.
//
// All entries here are class ITEM_CLASS_CONTAINER, subclass 0 (generic bag).
// Profession-specific bags (Herb / Mining / Engineering / etc) are handled
// separately and depend on the bot's professions.
//
// Prices are vendor-buy approximations; actual cost depends on faction-
// reputation discount and vendor markup. The bag-upgrade rule applies a
// 20% safety margin on bot.gold so a slight under-estimate doesn't trip
// the buy.

#pragma once

#include "Define.h"
#include <array>

namespace Playerbot {

struct BagSizeRow
{
    uint8  capacity;       // Slot count
    uint32 item_entry;     // ItemTemplate.entry; vendor-buyable in starter zones
    uint32 approx_price;   // Vendor copper, no discount
};

// Sorted ascending by capacity. The buy logic walks the array picking the
// largest entry whose price ≤ gold * 0.5 (keep half gold for other needs).
// Entries beyond level-30ish content require crafting / AH; not stocked
// at NPC vendors. The lookup gracefully degrades to the largest entry
// whose price the bot can afford.
inline constexpr std::array<BagSizeRow, 6> kBagSizeTable = {{
    {  6,  4496,        500 },   // Linen Bag             — ~5s
    {  8,  5572,       1500 },   // Wool Bag              — ~15s
    { 10, 10050,       5000 },   // Mageweave Bag         — ~50s
    { 14, 14046,      30000 },   // Runecloth Bag         — ~3g
    { 16, 21841,      50000 },   // Netherweave Bag       — ~5g
    { 20, 41599,     100000 },   // Frostweave Bag        — ~10g
}};

} // namespace Playerbot

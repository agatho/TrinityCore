// BotGearGenerator - Pure function: given (level, class, spec), returns a list
// of equipment-slot/item-entry pairs that gear the bot to the bracket's
// expected ilvl. Phase B3 of WORLD_POPULATION_PLAN.md.
//
// Algorithm:
//   1. Compute target ilvl from level (linear ramp, capped at max-level ilvl).
//   2. For each equipment slot:
//      - Filter ItemTemplate by RequiredLevel <= bot.level, ItemLevel ~= target,
//        AllowableClass match, ArmorType match (cloth/leather/mail/plate per class),
//        InventoryType match (slot fits), Bonding != BoP-raid-only.
//      - Pick deterministic from filtered list via bot-id hash.
//   3. Return slot/entry tuples.
//
// Cached at module init: per (class, level_bracket, slot) candidate pools so
// per-bot lookups are O(1).

#pragma once

#include "Bot/BotTypes.h"
#include <vector>

namespace Playerbot::V2::Gear {

struct GearItem
{
    uint8  slot;        // EquipmentSlots::EQUIPMENT_SLOT_*
    uint32 item_entry;  // ItemTemplate.entry
};

struct GearGenerationContext
{
    uint8  level;
    uint8  cls;
    uint16 spec;
    uint64 bot_id;      // for deterministic selection
};

// Build per-class candidate pools at module init. Idempotent. Walks
// sObjectMgr->GetItemTemplateStore() exactly once.
void Initialize();

// Pure-function gear generator. Returns one entry per equipment slot the
// bot can fill given its class. Empty vector if generator not initialized
// or no candidates exist.
std::vector<GearItem> GenerateGearFor(GearGenerationContext const& ctx);

// Approximate ilvl target for a character level (linear L1=1 -> L80=600).
// Exposed so the hygiene re-gear pass can detect under-geared bots (avg
// equipped effective ilvl well below this target).
uint16 TargetIlvlForLevel(uint8 level);

} // namespace Playerbot::V2::Gear

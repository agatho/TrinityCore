// ObsidianSanctumScript — Obsidian Sanctum raid (map 615, WotLK 10/25).
// Single boss Sartharion + 3 drake mini-bosses (Tenebron, Shadron, Vesperon).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/ChamberOfAspects/ObsidianSanctum/boss_sartharion.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ObsidianSanctumScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 615; }
    char const* name() const override { return "obsidian_sanctum"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            31219,  // Acolyte of Vesperon
            31218,  // Acolyte of Shadron
            30461,  // Tenebron drake
            30451,  // Shadron drake
            30452,  // Vesperon drake
        };
        a.mandatory_interrupt_spells = {
            56909,  // Cleave
            56908,  // Flame Breath
            56910,  // Tail Lash
            57571,  // Lava Strike
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            61632,  // Berserk
            60639,  // Twilight Revenge
            61251,  // Power of Vesperon
            58105,  // Power of Shadron
            30616,  // Flame Tsunami
        };
        // Boss progression — entries from TC's obsidian_sanctum.h.
        a.bosses = {
            30452,  // Tenebron
            30451,  // Shadron
            30449,  // Vesperon
            28860,  // Sartharion (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeObsidianSanctumScript()
{
    return std::make_unique<ObsidianSanctumScript>();
}

} // namespace Playerbot

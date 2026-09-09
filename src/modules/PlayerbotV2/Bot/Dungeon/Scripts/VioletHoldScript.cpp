// VioletHoldScript — The Violet Hold (map 608, WotLK 75-80).
// Dalaran prison wave-defense dungeon. 7 possible bosses spawn from
// portals; Cyanigosa is the final dragon. Sinclari NPC must survive.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/VioletHold/boss_*.cpp (all 7 bosses)

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class VioletHoldScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 608; }
    char const* name() const override { return "violet_hold"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            30661,  // Azure Sorcerer (caster trash — focus)
            30663,  // Azure Spellbinder
            30662,  // Azure Magus
            30666,  // Azure Mage Slayer
        };
        a.mandatory_interrupt_spells = {
            // Erekem (shaman boss)
            54481,  // Chain Heal — heals other Erekem casters
            54479,  // Earth Shield buff
            54493,  // Windfury buff
            54516,  // Bloodlust haste buff
            // Ichoron (water elemental)
            54237,  // Water Blast
            54241,  // Water Bolt Volley
            // Xevozz (ethereal)
            54202,  // Arcane Barrage Volley
            54226,  // Arcane Buffet
            // Cyanigosa (final dragon)
            58694,  // Arcane Vacuum
            58688,  // Uncontrollable Energy
            // Zuramat
            54361,  // Void Shift
            // Lavanthor
            54235,  // Firebolt
        };
        a.cc_priority_entries = {
            30661,  // Azure Sorcerer
            30662,  // Azure Magus
        };
        a.dangerous_auras = {
            // Boss area-effects
            58693,  // Cyanigosa Blizzard zone
            58690,  // Cyanigosa Tail Sweep cone
            54462,  // Moragg Howling Screech AoE
            54282,  // Lavanthor Flame Breath cone
            54249,  // Lavanthor Lava Burn area
            54396,  // Moragg Optic Link tether
            54306,  // Ichoron Protective Bubble (don't melee)
            54379,  // Ichoron Burst when bubble pops
            54524,  // Zuramat Shroud of Darkness
            54343,  // Zuramat Void Shifted
        };
        // Boss progression — NPC entries from TC's violet_hold.h.
        // VH spawns a subset (4 of 6 mini-bosses + Cyanigosa final)
        // per run from portals; listing all six so the tank-advance
        // finds whichever is currently in the room.
        a.bosses = {
            29266,  // Xevozz
            29312,  // Lavanthor
            29313,  // Ichoron
            29314,  // Zuramat the Obliterator
            29315,  // Erekem
            29316,  // Moragg
            31134,  // Cyanigosa (final)
        };
        // Progression waypoints — Violet Hold is a single-room defense
        // dungeon; bots stay at the central platform between portals.
        a.progression_waypoints = {
            { 1815.0f, 803.0f,  44.4f },   // entry portal
            { 1879.0f, 803.0f,  44.4f },   // central platform (defense)
            { 1879.0f, 768.0f,  44.4f },   // Cyanigosa fight room
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeVioletHoldScript()
{
    return std::make_unique<VioletHoldScript>();
}

} // namespace Playerbot

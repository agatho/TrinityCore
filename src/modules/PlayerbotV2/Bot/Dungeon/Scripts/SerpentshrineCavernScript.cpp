// SerpentshrineCavernScript — Serpentshrine Cavern raid (map 548, TBC 25-man).
// 6 bosses: Hydross, The Lurker Below, Leotheras the Blind,
// Fathom-Lord Karathress, Morogrim Tidewalker, Lady Vashj.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/CoilfangReservoir/SerpentShrine/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SerpentshrineCavernScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 548; }
    char const* name() const override { return "serpentshrine_cavern"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Vashj adds verified vs TC boss_lady_vashj.cpp (ENCHANTED_ELEMENTAL
        // 21958, TAINTED_ELEMENTAL 22009, COILFANG_STRIDER 22056,
        // COILFANG_ELITE 22055, TOXIC_SPOREBAT 22140) — all script-spawned.
        a.high_priority_kill_entries = {
            21966,  // Fathom-Guard Sharkkis
            21965,  // Fathom-Guard Tidalvess
            21964,  // Fathom-Guard Caribdis
            22055,  // Coilfang Elite (Vashj phase 2)
            22056,  // Coilfang Strider (Vashj phase 2)
            22140,  // Toxic Sporebat (Vashj phase 3)
            22009,  // Tainted Elemental (drops Tainted Core)
            21958,  // Enchanted Elemental (Vashj)
        };
        a.mandatory_interrupt_spells = {
            // Hydross
            38235,  // Water Tomb
            38246,  // Vile Sludge
            // Lurker
            37433,  // Spout
            37478,  // Geyser
            37660,  // Whirl
            37138,  // Waterbolt
            // Leotheras
            37640,  // Whirlwind
            37674,  // Chaos Blast (demon form)
            37676,  // Insidious Whisper
            // Karathress
            38441,  // Cataclysmic Bolt (Sharkkis)
            38445,  // Sear Nova (Caribdis)
            38449,  // Blessing of the Tides
            38234,  // Frost Shock (Tidalvess)
            38236,  // Spitfire Totem
            38306,  // Poison Cleansing Totem
            38304,  // Earthbind Totem
            // Morogrim
            37730,  // Tidal Wave
            38049,  // Watery Grave
            37764,  // Earthquake
            // Vashj
            38280,  // Static Charge
            38509,  // Shock Blast
            40088,  // Forked Lightning
            38316,  // Entangle
            40095,  // Poison Bolt
            38112,  // Magic Barrier
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Hydross
            38215, 38216, 38217, 38218, 38231, 40584,  // Mark of Hydross
            38219, 38220, 38221, 38222, 38230, 40583,  // Mark of Corruption
            38246,  // Vile Sludge
            // Lurker
            37433,  // Spout
            37478,  // Geyser
            // Leotheras
            37676,  // Insidious Whisper
            37749,  // Consuming Madness
            // Karathress
            38455,  // Power of Sharkkis
            38452,  // Power of Tidalvess
            38451,  // Power of Caribdis
            // Morogrim
            38049,  // Watery Grave
            37764,  // Earthquake
            37871,  // Globule Explosion
            // Vashj
            38575,  // Toxic Spores
            38044,  // Surge
            38112,  // Magic Barrier
        };
        // Boss progression — SSC has 6 encounters.
        // NPC entries from TC's CoilfangReservoir/SerpentShrine boss scripts.
        a.bosses = {
            21216,  // Hydross the Unstable
            21217,  // The Lurker Below
            21215,  // Leotheras the Blind
            21214,  // Fathom-Lord Karathress
            21213,  // Morogrim Tidewalker
            21212,  // Lady Vashj (final)
        };
        // Progression waypoints — DB-truth boss spawn positions on map 548
        // (world.creature), one per encounter in order.
        a.progression_waypoints = {
            { -239.8f, -366.5f,  -0.7f },   // Hydross the Unstable
            {   40.4f, -417.1f, -21.6f },   // The Lurker Below
            {  370.1f, -430.5f,  29.5f },   // Leotheras the Blind
            {  452.8f, -539.9f,  -7.5f },   // Fathom-Lord Karathress
            {  355.8f, -721.0f, -13.9f },   // Morogrim Tidewalker
            {   29.4f, -924.1f,  42.9f },   // Lady Vashj (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSerpentshrineCavernScript()
{
    return std::make_unique<SerpentshrineCavernScript>();
}

} // namespace Playerbot

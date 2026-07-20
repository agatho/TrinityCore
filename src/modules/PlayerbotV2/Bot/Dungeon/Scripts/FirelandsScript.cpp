// FirelandsScript — Firelands raid (map 720, Cata 10/25).
// 7 bosses canonically: Beth'tilac, Lord Rhyolith, Alysrazor, Shannox, Baleroc,
// Majordomo Staghelm, Ragnaros (return). TC has scripted bosses for only
// Alysrazor and Baleroc; the other 5 fall back to generic combat behavior.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/Firelands/boss_alysrazor.cpp
//   src/server/scripts/Kalimdor/Firelands/boss_baleroc.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class FirelandsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 720; }
    char const* name() const override { return "firelands"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            53574,  // Smouldering Hatchling (Alysrazor)
        };
        a.mandatory_interrupt_spells = {
            // Alysrazor
            100093, // Fire It Up
            100094, // Fieroblast Trash
            100095, // Fieroclast Barrage
            // Baleroc
            99350,  // Inferno Blade
            99351,  // Inferno Strike
            99352,  // Decimation Blade
            99353,  // Decimating Strike
            99259,  // Shards of Torment
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Alysrazor
            100109, // Fire Channeling
            100071, 100072, 100073, 100074,  // Molten Barrage L/R
            // Baleroc
            99342,  // Blades of Baleroc
            99252,  // Blaze of Glory
            99256,  // Torment
            99257,  // Tormented
            99254,  // Torment Active
            99516,  // Countdown Aura
            99369,  // Incendiary Soul
        };
        // Boss progression — Firelands has 7 encounters.
        a.bosses = {
            52498,  // Beth'tilac
            52558,  // Lord Rhyolith
            52530,  // Alysrazor
            53691,  // Shannox
            53494,  // Baleroc, the Gatekeeper
            52571,  // Majordomo Staghelm
            52409,  // Ragnaros (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeFirelandsScript()
{
    return std::make_unique<FirelandsScript>();
}

} // namespace Playerbot

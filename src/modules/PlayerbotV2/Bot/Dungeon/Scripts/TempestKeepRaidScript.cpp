// TempestKeepRaidScript — The Eye (Tempest Keep) raid (map 550, TBC 25-man).
// 4 bosses: Al'ar, Void Reaver, High Astromancer Solarian, Kael'thas Sunstrider.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/TempestKeep/Eye/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TempestKeepRaidScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 550; }
    char const* name() const override { return "tempest_keep_eye"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            19551,  // Embers of Al'ar
            18928,  // Phoenix Hatchling
            18928,  // Solarium Oracle (placeholder)
            20064,  // Thaladred the Darkener (Kael'thas advisor)
            20060,  // Lord Sanguinar
            20062,  // Grand Astromancer Capernian
            20063,  // Master Engineer Telonicus
        };
        a.mandatory_interrupt_spells = {
            // Al'ar
            34121,  // Flame Buffet
            34229,  // Flame Quills
            35181,  // Dive Bomb
            35410,  // Melt Armor
            // Void Reaver
            34162,  // Pounding
            34172,  // Arcane Orb
            25778,  // Knock Away
            // Solarian
            33031,  // Arcane Missiles
            42783,  // Wrath of the Astromancer
            33009,  // Blinding Light
            34322,  // Fear
            39329,  // Void Bolt
            33387,  // Solarium Great Heal (priest add cast — interrupt!)
            25054,  // Solarium Holy Smite
            33390,  // Solarium Arcane Torrent
            // Kael'thas
            36805,  // Fireball
            36819,  // Pyroblast
            36834,  // Arcane Disruption
            36815,  // Shock Barrier
            36797,  // Mind Control
            36735,  // Summon Flame Strike
            35941,  // Gravity Lapse
            35873,  // Nether Beam
        };
        a.cc_priority_entries = {
            18928,  // Solarium Priest (heal-out interrupts)
        };
        a.dangerous_auras = {
            // Al'ar
            34229,  // Flame Quills
            35380,  // Flame Patch
            34133,  // Ember Blast (when Ember dies)
            // Void Reaver
            34162,  // Pounding
            // Solarian
            42783,  // Wrath of the Astromancer (bomb debuff)
            42784,  // Wrath DoT
            25824,  // Spotlight
            // Kael'thas
            36834,  // Arcane Disruption
            35941,  // Gravity Lapse
            36815,  // Shock Barrier
            36373, 36375, 36092, 36354,  // Kael "explodes" telegraphs
            34480,  // Gravity Lapse Periodic
            35873,  // Nether Beam
            36196,  // Pure Nether Beam
            35865,  // Summon Nether Vapor
        };
        // Boss progression — The Eye (TK raid) has 4 encounters.
        a.bosses = {
            19514,  // Al'ar (Phoenix God)
            19516,  // Void Reaver
            18805,  // High Astromancer Solarian
            19622,  // Kael'thas Sunstrider (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTempestKeepRaidScript()
{
    return std::make_unique<TempestKeepRaidScript>();
}

} // namespace Playerbot

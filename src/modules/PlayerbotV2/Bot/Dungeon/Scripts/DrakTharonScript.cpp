// DrakTharonScript — Drak'Tharon Keep (map 600, WotLK 74-80).
// 4 bosses: Trollgore, Novos the Summoner, King Dred, Tharon'ja.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/DraktharonKeep/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DrakTharonScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 600; }
    char const* name() const override { return "drak_tharon"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            27742,  // Drakkari Raptor (King Dred adds)
        };
        a.mandatory_interrupt_spells = {
            // King Dred
            48920,  // Grievous Bite
            48873,  // Mangling Slash (tank debuff)
            48849,  // Fearsome Roar
            // Tharon'ja
            49527,  // Curse of Life
            49518,  // Rain of Fire
            49528,  // Shadow Volley
            49544,  // Eye Beam (phase 2)
            49537,  // Lightning Breath
            // Novos
            49034,  // Blizzard (Crystal Handler casters)
            49037,  // Frostbolt
            49198,  // Arcane Blast
            50089,  // Wrath of Misery
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Trollgore
            22686,  // Bellowing Roar (fear)
            49710,  // Gut Rip
            13738,  // Rend
            // Tharon'ja
            49548,  // Poison Cloud zone
            53463,  // Return Flesh (party stun phase)
            // Novos
            47346,  // Arcane Field
            52106,  // Beam Channel (telegraph)
            // King Dred
            48878,  // Piercing Slash debuff (-75% armor)
        };
        // Boss progression — NPC entries from TC's drak_tharon_keep.h.
        a.bosses = {
            26630,  // Trollgore
            26631,  // Novos the Summoner
            27483,  // King Dred
            26632,  // The Prophet Tharon'ja (final)
        };
        // Progression waypoints — DTK is a linear troll ziggurat.
        a.progression_waypoints = {
            { -379.0f, -722.0f,  28.5f },   // entry
            { -382.0f, -603.0f,  28.5f },   // Trollgore plaza
            { -323.0f, -710.0f,  28.5f },   // Novos summoning circle
            { -494.0f, -763.0f,  28.0f },   // King Dred arena
            { -340.0f, -842.0f,  28.5f },   // Tharon'ja altar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDrakTharonScript()
{
    return std::make_unique<DrakTharonScript>();
}

} // namespace Playerbot

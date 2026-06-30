// RuinsOfAhnQirajScript — Ruins of Ahn'Qiraj raid (map 509, classic 20-man).
// 6 bosses: Kurinnaxx, General Rajaxx, Moam, Buru the Gorger, Ayamiss the Hunter,
// Ossirian the Unscarred.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/RuinsOfAhnQiraj/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RuinsOfAhnQirajScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 509; }
    char const* name() const override { return "ruins_of_ahnqiraj"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            15514,  // Buru Egg / Hatchling
        };
        a.mandatory_interrupt_spells = {
            // Kurinnaxx
            25646,  // Mortal Wound
            25814,  // Wide Slash
            25648,  // Sand Trap
            // Rajaxx
            6713,   // Disarm
            8269,   // Frenzy
            25599,  // Thundercrash
            // Moam
            25671,  // Drain Mana
            25672,  // Arcane Eruption
            25685,  // Energize
            15550,  // Trample
            // Buru
            20512,  // Creeping Plague
            96,     // Dismember
            // Ayamiss
            25749,  // Stinger Spray
            25748,  // Poison Stinger
            25725,  // Paralyze
            // Ossirian
            25189,  // Cyclone
            25160,  // Sand Storm
            25188,  // Stomp
            25195,  // Curse of Tongues
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Kurinnaxx
            25646,  // Mortal Wound
            25648,  // Sand Trap
            // Rajaxx
            25599,  // Thundercrash
            8269,   // Frenzy
            // Moam
            25671,  // Drain Mana
            25672,  // Arcane Eruption (10-stack release)
            // Buru
            20512,  // Creeping Plague
            25640,  // Thorns
            // Ayamiss
            25749,  // Stinger Spray
            25725,  // Paralyze
            // Ossirian
            25160,  // Sand Storm
            25176,  // Supreme
            25177, 25178, 25180,  // Weakness Fire/Frost/Nature
        };
        // Boss progression — AQ20 has 6 encounters.
        a.bosses = {
            15348,  // Kurinnaxx
            15341,  // General Rajaxx
            15340,  // Moam
            15370,  // Buru the Gorger
            15369,  // Ayamiss the Hunter
            15339,  // Ossirian the Unscarred (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRuinsOfAhnQirajScript()
{
    return std::make_unique<RuinsOfAhnQirajScript>();
}

} // namespace Playerbot

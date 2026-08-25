// MaraudonScript — Maraudon (map 349, vanilla 40-49).
// Earth-themed dungeon in Desolace. 3 wings. Bosses include Celebras
// the Cursed, Noxxion, Princess Theradras, Landslide, Tinkerer
// Gizlock, Lord Vyletongue.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/Maraudon/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MaraudonScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 349; }
    char const* name() const override { return "maraudon"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            13282,  // Noxxion's Spawn
            13321,  // Theradras Healer add
        };
        a.mandatory_interrupt_spells = {
            // Celebras the Cursed
            21807,  // Wrath
            12747,  // Entangling Roots
            21968,  // Corrupt Forces
            // Landslide / earth boss
            18670,  // Knock Away
            21808,  // Landslide
            // Noxxion
            21687,  // Toxic Volley
            22916,  // Uppercut
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Princess Theradras
            21909,  // Dust Field
            21832,  // Boulder
            3391,   // Thrash
            21869,  // Repulsive Gaze
            // Lord Vyletongue
            3815,   // Poison Cloud
            3490,   // Frenzied Rage
            // General trample
            5568,   // Trample
        };
        // Boss progression — NPC entries from TC's maraudon.h
        // (Princess Theradras, Celebras, Noxxion from WoWHead).
        a.bosses = {
            12258,  // Razorlash
            13601,  // Tinkerer Gizlock
            12236,  // Lord Vyletongue
            13282,  // Noxxion
            13596,  // Rotgrip
            12225,  // Celebras the Cursed
            12201,  // Princess Theradras (final)
        };
        // Progression waypoints — Maraudon has 3 entrances (Orange,
        // Purple, deep portal) but bots enter via Princess Theradras
        // path. Walk: outer ring → Razorlash → Tinkerer → split (orange
        // for Noxxion / purple for Vyletongue) → Rotgrip pool → Celebras
        // gate → Theradras throne room.
        a.progression_waypoints = {
            {  955.0f,  291.0f,  -43.4f },   // entry tunnel
            {  978.4f,  301.0f,  -29.0f },   // Razorlash
            { 1031.0f,  302.0f,  -45.0f },   // Tinkerer Gizlock
            { 1064.0f,  362.5f,  -76.0f },   // Lord Vyletongue (purple)
            {  882.4f,  308.0f,  -75.5f },   // Noxxion (orange)
            { 1095.6f,  474.1f, -110.4f },   // Rotgrip pool
            { 1108.0f,  539.0f, -120.6f },   // Celebras gate
            {  815.0f,  580.0f, -129.3f },   // Theradras throne (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMaraudonScript()
{
    return std::make_unique<MaraudonScript>();
}

} // namespace Playerbot

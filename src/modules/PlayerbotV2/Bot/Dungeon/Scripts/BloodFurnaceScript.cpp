// BloodFurnaceScript — The Blood Furnace (map 542, TBC 61-67).
// 3 bosses: The Maker, Broggok, Keli'dan the Breaker.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/HellfireCitadel/BloodFurnace/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BloodFurnaceScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 542; }
    char const* name() const override { return "blood_furnace"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17000,  // Channeler add (Keli'dan summons)
            17068,  // Broggok wave orc
        };
        a.mandatory_interrupt_spells = {
            // The Maker
            38153,  // Acid Spray
            30925,  // Exploding Breaker
            25772,  // Domination (mind control)
            // Broggok
            30913,  // Slime Spray
            30917,  // Poison Bolt
            // Keli'dan
            30938,  // Corruption
            30935,  // Evocation (boss mana regen)
            33132,  // Fire Nova
            28599,  // Shadow Bolt Volley
            12739,  // Shadow Bolt (cult adds)
            39123,  // Channeling (cult channel)
        };
        a.cc_priority_entries = {
            17000,  // Cult Channeler
        };
        a.dangerous_auras = {
            // Broggok
            30916,  // Poison Cloud (persistent zone)
            30914,  // Poison Cloud passive
            // Keli'dan
            30940,  // Burning Nova (room-wide AoE)
            37370,  // Vortex (pull)
            30937,  // Mark of Shadow debuff
            // The Maker
            20276,  // Knockdown
        };
        // Boss progression — NPC entries from TC's blood_furnace.h.
        a.bosses = {
            17381,  // The Maker
            17380,  // Broggok
            17377,  // Keli'dan the Breaker (final)
        };
        // Progression waypoints — BF is a linear forge dungeon: entry →
        // upper level (The Maker) → cellblock event (Broggok adds) →
        // Broggok arena → ritual room (Keli'dan).
        a.progression_waypoints = {
            {  362.6f,  53.4f,  -10.0f },   // entry
            {  427.4f, 110.8f,    8.6f },   // upper hall
            {  474.8f,  93.0f,    8.7f },   // The Maker
            {  455.0f, -67.0f,    8.7f },   // cellblock event
            {  444.5f, -94.5f,  -19.4f },   // Broggok arena
            {  311.3f, -84.0f,  -20.7f },   // Keli'dan ritual room
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBloodFurnaceScript()
{
    return std::make_unique<BloodFurnaceScript>();
}

} // namespace Playerbot

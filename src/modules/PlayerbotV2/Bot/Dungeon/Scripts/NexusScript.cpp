// NexusScript — The Nexus (map 576, WotLK 71-77).
// 4 bosses: Anomalus, Grand Magus Telestra, Ormorok the Tree-Shaper,
// Keristrasza.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Nexus/Nexus/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class NexusScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 576; }
    char const* name() const override { return "nexus"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            26918,  // Chaotic Rift (Anomalus)
            26920,  // Crazed Mana Wraith (rift adds)
            26928,  // Telestra copies
        };
        a.mandatory_interrupt_spells = {
            // Anomalus
            47688,  // Chaotic Energy Burst
            // Telestra
            47772,  // Ice Nova
            47773,  // Firebomb
            47756,  // Gravity Well
            // Ormorok
            48096,  // Crystalfire Breath
            // Keristrasza
            50155,  // Tail Sweep
            50997,  // Crystal Chains
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Keristrasza
            47854,  // Frozen Prison (root)
            48094,  // Intense Cold debuff (move!)
            48095,  // Intense Cold triggered damage
            48179,  // Crystallize freeze
            48096,  // Crystalfire Breath cone
            8599,   // Enrage
            // Anomalus
            47748,  // Rift Shield (rift invuln — don't attack while up)
            // Ormorok
            47731,  // Spell ID for Crystal Spikes telegraph
        };
        // Boss progression — NPC entries from TC's instance scripts.
        a.bosses = {
            26731,  // Grand Magus Telestra
            26763,  // Anomalus
            26794,  // Ormorok the Tree-Shaper
            26723,  // Keristrasza (final)
        };
        // Progression waypoints — Nexus is a circular arcane prison
        // with 3 wings radiating from the central hub. Keristrasza
        // only unlocks when all 3 wing bosses are dead.
        a.progression_waypoints = {
            { 477.0f,  90.0f, -16.0f },   // entry hub
            { 522.0f,  90.0f, -16.0f },   // Telestra wing
            { 540.0f,  78.0f, -16.0f },   // Anomalus wing
            { 477.0f, 152.0f, -16.0f },   // Ormorok wing
            { 763.0f,  30.0f, -16.0f },   // Keristrasza ice chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeNexusScript()
{
    return std::make_unique<NexusScript>();
}

} // namespace Playerbot

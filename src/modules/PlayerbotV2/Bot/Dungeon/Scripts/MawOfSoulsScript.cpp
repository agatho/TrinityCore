// MawOfSoulsScript — Maw of Souls (map 1492, Legion 110).
// 3 bosses: Ymiron the Fallen King, Harbaron, Helya.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/BrokenIsles/MawOfSouls/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MawOfSoulsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1492; }
    char const* name() const override { return "maw_of_souls"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            96920,   // Tentacle (Helya)
        };
        a.mandatory_interrupt_spells = {
            // Ymiron
            243029,  // Soul Siphon
            194665,  // Soul Siphon Channel
            193211,  // Dark Slash
            193364,  // Screams of the Dead
            193977,  // Winds of Northrend
            // Harbaron
            193460,  // Bane Aura
            193463,  // Bane Missile
            193510,  // Arise Fallen Enabler
            193594,  // Arise Fallen Summon
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Ymiron
            193465,  // Bane AreaTrigger (heroic)
            193513,  // Bane Damage
            200194,  // Bane Nova
            // Harbaron
            203816,  // Vigor stacking
        };
        // Boss progression — entries from TC's maw_of_souls.h.
        a.bosses = {
            96756,   // Ymiron, the Fallen King
            96754,   // Harbaron
            96759,   // Helya (final)
        };
        // Progression waypoints — Maw of Souls is a Stormheim
        // longship voyage with 3 distinct deck sections.
        a.progression_waypoints = {
            { 1300.0f,  1740.0f,  104.0f },   // entry foredeck
            { 1395.0f,  1730.0f,  104.0f },   // Ymiron throne deck
            { 1530.0f,  1700.0f,  120.0f },   // Harbaron sky bridge
            { 1675.0f,  1665.0f,   90.0f },   // Helya void chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMawOfSoulsScript()
{
    return std::make_unique<MawOfSoulsScript>();
}

} // namespace Playerbot

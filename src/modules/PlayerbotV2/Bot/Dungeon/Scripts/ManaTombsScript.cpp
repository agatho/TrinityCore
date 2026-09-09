// ManaTombsScript — Mana-Tombs (map 557, TBC 64-70).
// Auchindoun wing. 3 bosses: Pandemonius, Tavarok, Nexus-Prince Shaffar.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/Auchindoun/ManaTombs/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ManaTombsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 557; }
    char const* name() const override { return "mana_tombs"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            18472,  // Ethereal Beacon (Shaffar adds)
            18430,  // Ethereal Apprentice (Beacon spawn)
        };
        a.mandatory_interrupt_spells = {
            // Nexus-Prince Shaffar
            32364,  // Frostbolt
            32363,  // Fireball
            32365,  // Frost Nova
            32371,  // Ethereal Beacon
            15254,  // Arcane Bolt
            // Ethereal Apprentice adds
            32369,  // Apprentice Firebolt
            32370,  // Apprentice Frostbolt
            // Pandemonius
            32325,  // Void Blast
            32358,  // Dark Shell (boss invuln)
        };
        a.cc_priority_entries = {
            18472,
        };
        a.dangerous_auras = {
            // Pandemonius (interrupt-immune during Dark Shell)
            32358,  // Dark Shell (don't waste damage)
            // Tavarok
            38361,  // Double Breath
        };
        // Boss progression — entries from TC mana_tombs.h + WoWHead.
        // Yor is heroic-only, event-summoned via Ethereum Prison Key
        // (0 spawn rows anywhere) — boss-recognition only, navigator
        // falls back to waypoints.
        a.bosses = {
            18341,  // Pandemonius
            18343,  // Tavarok
            18344,  // Nexus-Prince Shaffar (final)
            22930,  // Yor (heroic bonus, event-summoned)
        };
        // Progression waypoints — Mana-Tombs is the western Auchindoun
        // wing, a square crypt with a central pillar: entry → Pandemonius
        // chamber → ethereal corridor → Tavarok altar → Shaffar's lab.
        a.progression_waypoints = {
            { -103.5f,   46.0f,  -33.5f },   // entry
            {  -49.0f,   34.5f,  -33.5f },   // Pandemonius
            {   78.0f,   90.0f,  -33.5f },   // ethereal corridor
            {   58.0f,  130.0f,  -33.5f },   // Tavarok altar
            {   95.0f,  -56.0f,  -33.5f },   // Shaffar lab
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeManaTombsScript()
{
    return std::make_unique<ManaTombsScript>();
}

} // namespace Playerbot

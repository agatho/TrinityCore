// CourtOfStarsScript — Court of Stars (map 1571, Legion 100-110).
// Suramar nightborne city dungeon — RP-heavy detective intro.
//   * Patrol Captain Gerdo — Conflagration (interrupt) + adds.
//   * Talixae Flamewreath — Embers of Aluneth + adds.
//   * Advisor Melandrus (final) — Slicing Maelstrom + Image of Slicing Maelstrom.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class CourtOfStarsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1571; }
    char const* name() const override { return "court_of_stars"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            106693,  // Astral Compass / Embers of Aluneth
            105701,  // Twilight Familiar (Talixae)
            107486,  // Image of Slicing Maelstrom (Melandrus copy)
        };
        a.mandatory_interrupt_spells = {
            207630,  // Conflagration (Gerdo)
            211470,  // Charging Embers (Talixae)
            215999,  // Slicing Maelstrom (Melandrus)
            207979,  // Shockwave (Gerdo)
            207948,  // Withering Soul (Twilight Familiar)
        };
        a.cc_priority_entries = {
            105701,
            106693,
        };
        a.dangerous_auras = {
            215999,  // Slicing Maelstrom zone
            207630,  // Conflagration ground
            211470,  // Charging Embers zone
            207962,  // Mass Suppress (Gerdo)
        };
        // Boss progression — Court of Stars has 3 encounters.
        // Progression waypoints — CoS is a stealth-or-fight Suramar
        // city quarter: entry plaza → Gerdo patrol → Talixae's manor →
        // Melandrus arena. Stealth bots can skip Gerdo entirely.
        a.progression_waypoints = {
            { 729.0f, 2854.0f, 134.0f },   // entry
            { 753.0f, 2870.0f, 134.0f },   // Gerdo patrol route
            { 870.0f, 2918.0f, 134.0f },   // Talixae manor
            { 871.0f, 2842.0f, 138.0f },   // Melandrus arena
        };
        a.bosses = {
            104215,  // Patrol Captain Gerdo
            104217,  // Talixae Flamewreath
            104218,  // Advisor Melandrus (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeCourtOfStarsScript()
{
    return std::make_unique<CourtOfStarsScript>();
}

} // namespace Playerbot

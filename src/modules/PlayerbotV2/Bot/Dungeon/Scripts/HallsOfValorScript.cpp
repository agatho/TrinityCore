// HallsOfValorScript — Halls of Valor (map 1477, Legion 110).
// Stormheim Vrykul mythic dungeon.
//   * Hymdall + Hyrja — paired fight; both eventually together.
//   * Fenryr — pet phase + pack adds.
//   * God-King Skovald — Strike of Anger (interrupt) +
//     Felblaze Rush (telegraphed line).
//   * Odyn (final) — Shield Of Light, Storm Phase, Resounding
//     Echo (interrupt critical).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfValorScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1477; }
    char const* name() const override { return "halls_of_valor"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            95675,   // Wolf Pup (Fenryr)
            95674,   // Olmyr the Enlightened (Odyn add)
        };
        a.mandatory_interrupt_spells = {
            193092,  // Strike of Anger (Skovald)
            193007,  // Volcanic Tantrum (Hymdall)
            192861,  // Resounding Echo (Odyn)
            193234,  // Felblaze Rush (Skovald)
        };
        a.dangerous_auras = {
            193234,  // Felblaze Rush line
            192861,  // Resounding Echo zone
        };
        // Boss progression — Halls of Valor has 4 encounters.
        // Hymdall + Hyrja are paired Storm Drake mini-bosses fought separately.
        a.bosses = {
            94960,   // Hymdall
            95675,   // Hyrja
            95674,   // Fenryr
            94960,   // God-King Skovald (collision with Hymdall id — verify)
            95675,   // Odyn (final, also Hyrja entry collision)
        };
        // Progression waypoints — HoV is a sprawling Stormheim hall:
        // entry plaza → Hymdall gauntlet → Hyrja's altar → Fenryr's
        // wolf pen → Skovald arena → Odyn's throne.
        a.progression_waypoints = {
            { -322.0f, -290.0f, 233.0f },   // entry
            { -211.0f, -257.0f, 247.0f },   // Hymdall plaza
            {  -32.0f, -267.0f, 247.0f },   // Hyrja altar
            { -154.0f, -474.0f, 247.0f },   // Fenryr pen
            { -150.0f, -373.0f, 271.0f },   // Skovald arena
            {   59.0f, -373.0f, 290.0f },   // Odyn throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfValorScript()
{
    return std::make_unique<HallsOfValorScript>();
}

} // namespace Playerbot

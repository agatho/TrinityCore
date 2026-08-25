// PlaguefallScript — Plaguefall (map 2289, SL 50-60).
// Maldraxxus plague-themed dungeon.
//   * Globgrog — Slime Wave (telegraphed line).
//   * Doctor Ickus — adds (Slime Slug); priority kill.
//   * Domina Venomblade — Venompiercer (interrupt) + Sanity drain.
//   * Margrave Stradama (final) — phase 2 Hailfire (telegraphed
//     ground AoE).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class PlaguefallScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2289; }
    char const* name() const override { return "plaguefall"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            166296,  // Slime Slug (Doctor Ickus)
        };
        a.mandatory_interrupt_spells = {
            324209,  // Slime Wave (Globgrog)
            322942,  // Venompiercer (Domina)
            322957,  // Hailfire (Stradama)
            322999,  // Plague Burst (Doctor Ickus)
        };
        a.dangerous_auras = {
            324209,  // Slime Wave line
            322957,  // Hailfire zone
        };
        // Boss progression — Plaguefall has 5 encounters (4 bosses + a
        // Globgrog precursor that's grouped with the first boss).
        a.bosses = {
            164255,  // Globgrog
            164967,  // Doctor Ickus
            164266,  // Domina Venomblade
            164267,  // Margrave Stradama (final)
        };
        // Progression waypoints — Plaguefall is a Maldraxxus swamp
        // estate with a slime jump-pad mid-run.
        a.progression_waypoints = {
            { -1735.0f, -1739.0f,  92.0f },   // entry
            { -1735.0f, -1622.0f,  92.0f },   // Globgrog swamp
            { -1735.0f, -1531.0f,  92.0f },   // Ickus lab
            { -1830.0f, -1505.0f,  92.0f },   // Venomblade garden
            { -1923.0f, -1521.0f,  92.0f },   // Stradama courtyard
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakePlaguefallScript()
{
    return std::make_unique<PlaguefallScript>();
}

} // namespace Playerbot

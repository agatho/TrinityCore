// AlgetharAcademyScript — Algeth'ar Academy (map 2526, DF 60-70).
// Thaldraszus magical academy.
//   * Vexamus — Rotational Drag.
//   * Crawth — Tackle (charge); cone breaths.
//   * Echo of Doragosa — Splice Reality.
//   * Algeth'ar Echoknight — Sweeping Strikes.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AlgetharAcademyScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2526; }
    char const* name() const override { return "algethar_academy"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            376521,  // Rotational Drag (Vexamus)
            376631,  // Splice Reality (Doragosa)
            376713,  // Sweeping Strikes (Echoknight)
            376833,  // Tackle (Crawth)
        };
        a.dangerous_auras = {
            376521,  // Rotational Drag zone
        };
        // Boss progression — Algeth'ar Academy has 4 encounters.
        // Correct template IDs (boss-recognition); map 2526 has no boss
        // spawn rows — bosses are event-summoned, navigator falls back
        // to waypoints.
        a.bosses = {
            194181,  // Vexamus
            196482,  // Overgrown Ancient
            191736,  // Crawth
            190609,  // Echo of Doragosa (final)
        };
        // Progression waypoints — Algeth'ar Academy is a Thaldraszus
        // magical academy with multi-floor layout.
        a.progression_waypoints = {
            { -3220.0f, -4940.0f,  100.0f },   // entry
            { -3296.0f, -4878.0f,  102.0f },   // Vexamus library
            { -3204.0f, -4801.0f,  104.0f },   // Ancient garden
            { -3128.0f, -4862.0f,  106.0f },   // Crawth aviary
            { -3066.0f, -4798.0f,  110.0f },   // Doragosa chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAlgetharAcademyScript()
{
    return std::make_unique<AlgetharAcademyScript>();
}

} // namespace Playerbot

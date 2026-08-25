// AuchindounScript — Auchindoun (map 1182, WoD 95-100).
// Modern reworked Auchindoun (separate map from TBC wings).
//   * Vigilant Kaathar — Reverberating Hymn (interrupt critical).
//   * Soulbinder Nyami — Soul Imbalance (random target target).
//   * Azzakel — Doom Lord summons (priority kill).
//   * Teron'gor (final) — Caustic Energy + Felflame.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AuchindounScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1182; }
    char const* name() const override { return "auchindoun_wod"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            76206,  // Doom Lord (Azzakel)
        };
        a.mandatory_interrupt_spells = {
            153722,  // Reverberating Hymn (Kaathar) — critical
            154003,  // Soul Imbalance (Nyami)
            153874,  // Caustic Energy (Teron'gor)
            153755,  // Doom Lord Summon (Azzakel)
        };
        a.dangerous_auras = {
            154003,  // Soul Imbalance debuff
            153874,  // Caustic Energy zone
        };
        // Boss progression — entries from TC's auchindoun.h.
        a.bosses = {
            75839,   // Vigilant Kaathar
            76177,   // Soulbinder Nyami
            87218,   // Azzakel
            77734,   // Teron'gor (final)
        };
        // Progression waypoints — Auchindoun (WoD) is a linear soul
        // temple: entry → Kaathar gate → soul corridor → Nyami arena →
        // Azzakel sky → Teron'gor sanctum.
        a.progression_waypoints = {
            { -116.0f, 4407.0f, -36.0f },   // entry
            {  -75.0f, 4444.0f, -36.0f },   // Kaathar gate
            {  -22.0f, 4429.0f, -36.0f },   // Nyami arena
            {   58.0f, 4404.0f, -25.0f },   // Azzakel platform
            {  121.0f, 4406.0f, -34.0f },   // Teron'gor sanctum
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAuchindounScript()
{
    return std::make_unique<AuchindounScript>();
}

} // namespace Playerbot

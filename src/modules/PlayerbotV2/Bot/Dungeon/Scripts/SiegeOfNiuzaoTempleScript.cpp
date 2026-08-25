// SiegeOfNiuzaoTempleScript — Siege of Niuzao Temple (map 1011, MoP 87-90).
// Pandaren Townlong Steppes Mantid invasion dungeon.
//   * Vizier Jin'bak — Smoldering Resin + Massive Eruption.
//   * Commander Vo'jak — Battle Cry + adds.
//   * General Pa'valak — Throw Spear + Chains of Pa'valak.
//   * Wing Leader Ner'onok (final) — Wind Aura + Quills.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SiegeOfNiuzaoTempleScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1011; }
    char const* name() const override { return "siege_of_niuzao_temple"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            61567,   // Krik'thik Demolisher (Vo'jak)
            61485,   // Niuzao Templar (Vo'jak adds)
            64315,   // Krik'thik Bombardier
        };
        a.mandatory_interrupt_spells = {
            119001,  // Smoldering Resin (Jin'bak)
            117823,  // Throw Spear (Pa'valak)
            117961,  // Battle Cry (Vo'jak)
            118730,  // Quills (Ner'onok)
            118915,  // Chains of Pa'valak
        };
        a.cc_priority_entries = {
            61567,
            61485,
            64315,
        };
        a.dangerous_auras = {
            119001,  // Smoldering Resin pool
            118730,  // Quills cone
            118915,  // Chains zone
        };
        // Boss progression — Siege of Niuzao Temple has 4 encounters.
        // Progression waypoints — Siege of Niuzao Temple: pandaren
        // siege defense; outdoor wall progression.
        a.progression_waypoints = {
            { -2010.0f, 1140.0f, 380.0f },   // entry
            { -2090.0f, 1192.0f, 376.0f },   // Jin'bak resin pit
            { -2010.0f, 1290.0f, 384.0f },   // Vo'jak gauntlet
            { -2050.0f, 1410.0f, 400.0f },   // Pa'valak siege
            { -1990.0f, 1480.0f, 420.0f },   // Ner'onok rooftop
        };
        a.bosses = {
            61567,  // Vizier Jin'bak
            61634,  // Commander Vo'jak
            61485,  // General Pa'valak
            62205,  // Wing Leader Ner'onok (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSiegeOfNiuzaoTempleScript()
{
    return std::make_unique<SiegeOfNiuzaoTempleScript>();
}

} // namespace Playerbot

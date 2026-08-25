// CathedralOfEternalNightScript — Cathedral of Eternal Night (map 1677,
// Legion 7.2 100-110). Tomb of Sargeras side dungeon.
//   * Agronox — Toxic Spores + Choking Pollen.
//   * Thrashbite the Scornful — Boneshatter Strike + Sappy Smackdown.
//   * Domatrax — Demonic Upsurge + Nether Beam.
//   * Mephistroth (final) — Shadow Bolt Volley + Felblade.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class CathedralOfEternalNightScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1677; }
    char const* name() const override { return "cathedral_of_eternal_night"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            119169,  // Fulminating Lasher (Agronox add — kill before it empowers)
            120770,  // Felguard Destroyer (demon invaders)
            120374,  // Felguard Destroyer (portal adds variant)
        };
        a.mandatory_interrupt_spells = {
            239006,  // Choking Pollen (Agronox)
            239059,  // Demonic Upsurge (Domatrax)
            238999,  // Shadow Bolt Volley (Mephistroth)
            239215,  // Nether Beam (Domatrax)
            238989,  // Sappy Smackdown (Thrashbite)
        };
        a.cc_priority_entries = {
            120770,  // Felguard Destroyer
            120374,  // Felguard Destroyer (portal adds variant)
        };
        a.dangerous_auras = {
            239006,  // Choking Pollen zone
            238505,  // Toxic Spores ground
            239006,
            239215,  // Nether Beam
        };
        // Boss progression — Cathedral of Eternal Night has 4 encounters.
        a.bosses = {
            117193,  // Agronox
            117194,  // Thrashbite the Scornful
            118804,  // Domatrax
            116944,  // Mephistroth (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeCathedralOfEternalNightScript()
{
    return std::make_unique<CathedralOfEternalNightScript>();
}

} // namespace Playerbot

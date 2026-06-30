// TrialOfTheCrusaderScript — Trial of the Crusader raid (map 649, WotLK 10/25).
// 5 encounters: Northrend Beasts (3-phase), Lord Jaraxxus, Faction Champions,
// Twin Val'kyr, Anub'arak.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TrialOfTheCrusaderScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 649; }
    char const* name() const override { return "trial_of_the_crusader"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            34800,  // Snobold Vassal (Gormok)
            34607,  // Burrower (Anub'arak)
            34605,  // Scarab (Anub'arak)
            34606,  // Frost Sphere
            34660,  // Spike (Anub'arak)
            34826,  // Mistress of Pain (Jaraxxus)
            34815,  // Felflame Infernal
        };
        a.mandatory_interrupt_spells = {
            // Gormok the Impaler
            66331,  // Impale
            66342,  // Staggering Stomp
            // Acidmaw / Dreadscale
            66776,  // Acid Spit
            66689,  // Burning Bile
            66819,  // Burning Spittle
            // Icehowl
            66689,  // Massive Crash
            // Lord Jaraxxus
            66532,  // Fel Fireball
            66565,  // Fel Lightning
            66541,  // Incinerate Flesh
            66536,  // Legion Flame
            66481,  // Touch of Jaraxxus
            // Twin Val'kyr
            65792,  // Twin Spike (cleave)
            66001,  // Touch of Light
            66001,  // Touch of Darkness
            // Anub'arak
            66012,  // Freezing Slash
            66013,  // Penetrating Cold
            66118,  // Leeching Swarm
            67630,  // Pursuing Spikes
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Gormok
            66331,  // Impale
            66406,  // Snobold Vassal Buff
            66331,  // Body Slam aftermath
            // Snakes
            66689,  // Burning Bile
            66770,  // Slime Pool
            66881,  // Acid Pool
            // Icehowl
            66689,  // Arctic Breath
            // Jaraxxus
            66547,  // Nether Power
            66237,  // Incinerate Flesh
            66536,  // Legion Flame ground
            // Twin Val'kyr
            65686, 67222,  // Light Touched
            65687, 67223,  // Dark Touched
            65950,  // Powering Up
            // Anub'arak
            67574,  // Mark
            66013,  // Penetrating Cold
            66169,  // Spike Call
            66193,  // Permafrost
        };
        // Boss progression — NPC entries from TC's trial_of_the_crusader.h.
        // Northrend Beasts is a 3-stage encounter (Gormok → Snakes →
        // Icehowl); listing all three so tank-advance finds whichever
        // is currently active. Faction Champions has no fixed entry —
        // adds-driven encounter — so it's skipped.
        a.bosses = {
            34796,  // Gormok the Impaler (Northrend Beasts P1)
            34799,  // Acidmaw / Dreadscale (P2 jormungar duo)
            34797,  // Icehowl (P3)
            34780,  // Lord Jaraxxus
            34497,  // Fjola Lightbane (Twin Val'kyr)
            34496,  // Eydis Darkbane (Twin Val'kyr)
            34564,  // Anub'arak (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTrialOfTheCrusaderScript()
{
    return std::make_unique<TrialOfTheCrusaderScript>();
}

} // namespace Playerbot

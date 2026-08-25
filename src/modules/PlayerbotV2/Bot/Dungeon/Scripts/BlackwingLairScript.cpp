// BlackwingLairScript — Blackwing Lair raid (map 469, classic 40-man).
// 8 bosses: Razorgore, Vaelastrasz, Broodlord, Firemaw, Ebonroc, Flamegor,
// Chromaggus, Nefarian.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/BlackrockMountain/BlackwingLair/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackwingLairScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 469; }
    char const* name() const override { return "blackwing_lair"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            12422,  // Death Talon Dragonspawn
            12420,  // Death Talon Wyrmguard
            13020,  // Chromatic Drakonid
            14302,  // Blackwing Mage
        };
        a.mandatory_interrupt_spells = {
            // Razorgore
            22425,  // Fireball Volley
            23023,  // Conflagration
            22540,  // Cleave
            24375,  // War Stomp
            42013,  // Mind Control
            // Vaelastrasz
            23461,  // Flame Breath
            23462,  // Fire Nova
            15847,  // Tail Swipe
            18173,  // Burning Adrenaline
            19983,  // Cleave
            // Broodlord
            26350,  // Cleave
            23331,  // Blast Wave
            24573,  // Mortal Strike
            25778,  // Knockback
            // Drake bosses (Firemaw / Ebonroc / Flamegor)
            22539,  // Shadowflame
            23339,  // Wing Buffet
            23341,  // Flame Buffet
            // Chromaggus
            23308,  // Incinerate
            23310,  // Time Lapse
            // Nefarian
            22677,  // Shadow Bolt
            22665,  // Shadow Bolt Volley
            22667,  // Shadow Command
            22678,  // Fear
            22686,  // Bellowing Roar
            20691,  // Cleave
            23364,  // Tail Lash
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Razorgore (when MC'd or via egg destroy)
            19873,  // Egg Destroy (channeled by MC'd players)
            23023,  // Conflagration
            // Vael
            23513,  // Essence of the Red (energy boost)
            18173,  // Burning Adrenaline
            // Broodlord
            23331,  // Blast Wave
            24573,  // Mortal Strike
            22247,  // Suppression Device aura
            // Drakes
            22539,  // Shadowflame
            23341,  // Flame Buffet
            // Chromaggus
            23170, 23173, 23172, 23153,  // Brood Afflictions
            23310,  // Time Lapse
            23308,  // Incinerate
            // Nefarian
            22663,  // Nefarian's Barrier
            22992,  // Shadowflame Initial
            22686,  // Bellowing Roar
            7068,   // Veil of Shadow
            // Class Calls
            23410, 23397, 23398, 23401, 23418, 23425, 23427, 23436, 23414, 49576,
        };
        // Boss progression — entries from TC's blackwing_lair.h.
        a.bosses = {
            12435,  // Razorgore the Untamed
            13020,  // Vaelastrasz the Corrupt
            12017,  // Broodlord Lashlayer
            11983,  // Firemaw
            14601,  // Ebonroc
            11981,  // Flamegor
            14020,  // Chromaggus
            11583,  // Nefarian (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackwingLairScript()
{
    return std::make_unique<BlackwingLairScript>();
}

} // namespace Playerbot

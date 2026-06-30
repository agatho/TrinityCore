// BlackfathomDeepsScript — Blackfathom Deeps (map 48, vanilla 24-32).
// Underwater Naga / Murloc dungeon. Bosses: Ghamoo-ra, Lady Sarevess,
// Gelihast, Lorgus Jett, Old Serra'kis, Twilight Lord Kelris, Aku'mai.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/BlackfathomDeeps/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackfathomDeepsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 48; }
    char const* name() const override { return "blackfathom_deeps"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            // Twilight Lord Kelris
            15587,  // Mind Blast
            8399,   // Sleep
            8734,   // Blackfathom Channeling
            // Naga adds
            6533,   // Net
        };
        a.cc_priority_entries = {
            74353,  // Twilight Aquamancer (caster)
            74380,  // Twilight Storm Mender (healer)
        };
        a.dangerous_auras = {
            // Old Serra'kis
            3815,   // Poison Cloud
            3490,   // Frenzied Rage
        };
        // Boss progression — world DB map 48 holds the revamped BFD
        // (Twilight cult roster, 74xxx entries); classic-era entries
        // (Sarevess 4831/Kelris 4832 etc.) have no spawns, except
        // Aku'mai who IS spawned under his classic entry 4829.
        // All entries below are spawn-verified on map 48, ordered by
        // spawn-position progression (entrance pools → altar depths).
        a.bosses = {
            74446,  // Ghamoo-Ra
            74476,  // Domina
            74565,  // Subjugator Kor'ul
            74505,  // Thruk
            74518,  // Executioner Gore
            74728,  // Twilight Lord Bathiel
            4829,   // Aku'mai (final)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 48, revamped BFD), in encounter order. Coarse route
        // skeleton; the pathfinder handles corridors between them.
        a.progression_waypoints = {
            { -445.1f,  212.9f,  -52.7f },   // Ghamoo-Ra
            { -309.3f,  407.3f,  -56.6f },   // Domina
            { -422.8f,   23.2f,  -48.1f },   // Subjugator Kor'ul
            { -746.1f,    8.0f,  -30.0f },   // Thruk
            { -771.6f,  -57.7f,  -29.8f },   // Executioner Gore
            { -818.8f, -150.1f,  -25.8f },   // Twilight Lord Bathiel
            { -848.6f, -462.2f,  -33.9f },   // Aku'mai the Devourer (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackfathomDeepsScript()
{
    return std::make_unique<BlackfathomDeepsScript>();
}

} // namespace Playerbot

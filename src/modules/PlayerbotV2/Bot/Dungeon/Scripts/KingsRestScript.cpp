// KingsRestScript — King's Rest (map 1762, BfA 110-120).
// Pyramid tomb. 4 bosses: The Golden Serpent, Mchimba the Embalmer,
// Council of Tribes, Dazar.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Zandalar/KingsRest/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class KingsRestScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1762; }
    char const* name() const override { return "kings_rest"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            135365,  // Spectral Soldier (Dazar)
        };
        a.mandatory_interrupt_spells = {
            // Golden Serpent
            265773,  // Spit Gold
            265781,  // Serpentine Gust
            265923,  // Lucre's Call
        };
        a.cc_priority_entries = {
            134756,
        };
        a.dangerous_auras = {
            // Golden Serpent
            265910,  // Tail Thrash
            265914,  // Molten Gold Damage
            265915,  // Molten Gold Aura
            265991,  // Luster
        };
        // Progression waypoints — King's Rest is a Zandalari crypt
        // beneath the pyramid: 4-section descent.
        a.progression_waypoints = {
            { -113.0f, 2517.0f,  35.0f },   // entry
            {  -83.0f, 2548.0f,  35.0f },   // Golden Serpent vault
            { -195.0f, 2599.0f,  30.0f },   // Council tribe chamber
            { -250.0f, 2452.0f,  18.0f },   // Mchimba embalmer
            {  -67.0f, 2440.0f,  18.0f },   // King Dazar throne
        };
        // Boss progression — entries from TC's kings_rest.h.
        // Council of Tribes is a multi-boss encounter (Akaali / Zanazal /
        // Kula); the encounter resolves when all three die. Listing all
        // three so the tank-advance scan finds whichever is closest.
        a.bosses = {
            135322,  // The Golden Serpent
            135470,  // Akaali the Conqueror (Council)
            135472,  // Zanazal the Wise (Council)
            135475,  // Kula the Butcher (Council)
            134993,  // Mchimba the Embalmer
            136160,  // King Dazar (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeKingsRestScript()
{
    return std::make_unique<KingsRestScript>();
}

} // namespace Playerbot

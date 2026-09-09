// DeOtherSideScript — De Other Side (map 2291, SL 50-60).
// Mueh'zala's other dimensions; phased rooms.
//   * Hakkar the Soulflayer (Stranglethorn echo) — Cauterize.
//   * The Manastorms (Mechagnomes echo) — adds + Power Surge.
//   * Dealer Xy'exa — Volatile Stockpile (telegraphed).
//   * Mueh'zala (final) — Phase shifts; Demon Soul; Throw Boulder.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DeOtherSideScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2291; }
    char const* name() const override { return "de_other_side"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            324691,  // Cauterize (Hakkar)
            322942,  // Power Surge (Manastorms)
            322943,  // Volatile Stockpile (Xy'exa)
            324567,  // Throw Boulder (Mueh'zala)
        };
        a.dangerous_auras = {
            322943,  // Volatile Stockpile zone
            324567,  // Throw Boulder telegraph
        };
        // Boss progression — De Other Side has 4 encounters. The Manastorms
        // is a duo (Mama + Millhouse); listing both so tank-advance finds
        // whichever is closer.
        a.bosses = {
            164558,  // Hakkar the Soulflayer
            164556,  // The Manastorms — Millhouse Manastorm
            164555,  // The Manastorms — Millificent Manastorm
            164450,  // Dealer Xy'exa
            167561,  // Mueh'zala (final)
        };
        // Progression waypoints — DOS is an Ardenweald + multi-world
        // dungeon with portal-driven travel between Zul'Gurub, Dalaran,
        // Mechagon, and Atal'Dazar realms. Portals handled by snapshot
        // enemy detection; waypoints just land the tank in each realm.
        a.progression_waypoints = {
            { -1370.0f, 1740.0f, 7980.0f },   // entry hub
            { -1376.0f, 1786.0f, 7980.0f },   // Hakkar realm (ZG)
            { -1264.0f, 1796.0f, 7986.0f },   // Manastorms realm
            { -1257.0f, 1715.0f, 7989.0f },   // Xy'exa realm
            { -1370.0f, 1700.0f, 7980.0f },   // Mueh'zala arena
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDeOtherSideScript()
{
    return std::make_unique<DeOtherSideScript>();
}

} // namespace Playerbot

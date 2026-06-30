// IronDocksScript — Iron Docks (map 1195, WoD 90-100).
// Gorgrond shipyard dungeon.
//   * Fleshrender Nok'gar — Ogre Pet (Dreadfang) + Bombs.
//   * Grimrail Enforcer (Makogg, Neesa, Ahri'ok) — three-tier mini-boss.
//   * Oshir — Trample + Lacerate (cone).
//   * Skulloc (final) — Cannon Barrage + Mole Machines.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class IronDocksScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1195; }
    char const* name() const override { return "iron_docks"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            81297,   // Dreadfang (Nok'gar's wolf mount)
            83613,   // Koramar (Skulloc galleon add)
            83616,   // Zoggosh (Skulloc galleon add)
        };
        a.mandatory_interrupt_spells = {
            174404,  // Cannon Barrage (Skulloc)
            173324,  // Trample (Oshir)
            173309,  // Lacerate (Oshir)
            172981,  // Bombs Away (Nok'gar)
        };
        a.cc_priority_entries = {
            81279,   // Grom'kar Flameslinger (ranged caster trash)
            86809,   // Grom'kar Incinerator (caster trash)
        };
        a.dangerous_auras = {
            174404,  // Cannon Barrage zone
            173324,  // Trample path
            173309,  // Lacerate cone
            172981,  // Bombs Away pool
        };
        // Boss progression — Iron Docks has 4 encounters (Enforcers
        // is a multi-add encounter). Entries spawn-verified on map
        // 1195 (Nok'gar 81305 is script-spawned, riding Dreadfang).
        a.bosses = {
            81305,  // Fleshrender Nok'gar
            80805,  // Makogg Emberblade (Grimrail Enforcers)
            80808,  // Neesa Nox (Grimrail Enforcers)
            80816,  // Ahri'ok Dugru (Grimrail Enforcers)
            79852,  // Oshir
            83612,  // Skulloc (final)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 1195), in encounter order. Nok'gar (81305) is script-
        // spawned (no creature row) and is skipped; the Enforcers
        // trio spawn together on the dock. Coarse route skeleton; the
        // pathfinder handles corridors between them.
        a.progression_waypoints = {
            { 6508.2f, -1128.1f,   5.0f },   // Makogg Emberblade (Grimrail Enforcers)
            { 6502.4f, -1133.3f,   5.0f },   // Neesa Nox (Grimrail Enforcers)
            { 6514.4f, -1134.0f,   5.0f },   // Ahri'ok Dugru (Grimrail Enforcers)
            { 6939.0f, -1103.3f,   4.7f },   // Oshir
            { 6729.3f,  -978.0f,  23.1f },   // Skulloc (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeIronDocksScript()
{
    return std::make_unique<IronDocksScript>();
}

} // namespace Playerbot

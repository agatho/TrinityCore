// FreeholdScript — Freehold (map 1754, BfA 110-120).
// Pirate-themed Tiragarde Sound.
//   * Skycap'n Kragg — Cannonball Spitter (telegraphed line).
//   * Council o' Captains — Trio fight; CC priority.
//   * Ring of Booty (mini-boss) — wave random NPCs.
//   * Harlan Sweete (final) — Coin Sweep (frontal cone) +
//     Throw Coins (telegraphed projectiles).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class FreeholdScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1754; }
    char const* name() const override { return "freehold"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            257891,  // Cannonball Spitter (Skycap'n)
            258777,  // Coin Sweep (Harlan)
            258323,  // Throw Coins (Harlan)
            257784,  // Powderkeg (Council)
        };
        a.cc_priority_entries = {
            127111,  // Irontide Oarsman (Freehold trash; dungeon difficulty rows verified)
        };
        a.dangerous_auras = {
            257891,  // Cannonball line
            258777,  // Coin Sweep cone
        };
        // Progression waypoints — Freehold is a pirate town: outdoor
        // streets with 4 distinct encounter zones.
        a.progression_waypoints = {
            { -1305.0f, -700.0f,  9.0f },   // entry
            { -1240.0f, -742.0f, 11.0f },   // Kragg landing
            { -1126.0f, -858.0f, 11.0f },   // Council town square
            { -1180.0f, -800.0f,  9.0f },   // Trothak ring (side)
            { -1043.0f, -738.0f, 11.0f },   // Harlan tavern
        };
        // Boss progression — Freehold has 4 encounters. Council o' Captains
        // is a trio (Eudora/Raoul/Jolly — only two of three are fought; listing all).
        // Trothak is a side encounter sometimes labeled "Ring of Booty";
        // canonical encounter set is 4.
        a.bosses = {
            126832,  // Skycap'n Kragg
            126848,  // Council o' Captains — Captain Eudora
            126847,  // Council o' Captains — Captain Raoul
            126845,  // Council o' Captains — Captain Jolly
            126969,  // Trothak (Ring of Booty mini-encounter)
            126983,  // Harlan Sweete (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeFreeholdScript()
{
    return std::make_unique<FreeholdScript>();
}

} // namespace Playerbot

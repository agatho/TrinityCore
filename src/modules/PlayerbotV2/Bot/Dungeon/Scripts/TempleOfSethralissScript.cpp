// TempleOfSethralissScript — Temple of Sethraliss (map 1877, BfA 110-120).
// Vol'dun snake-cult dungeon.
//   * Adderis & Aspix — twin pull, kill in tandem.
//   * Merektha — Poison Nova + Snake Charm.
//   * Galvazzt — Galvanizing Field (move out) + Static Shield.
//   * Avatar of Sethraliss (final) — Heart of Sethraliss revival event +
//     Cataclysmic Strike.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TempleOfSethralissScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1877; }
    char const* name() const override { return "temple_of_sethraliss"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            134364,  // Plague Doctor add (Sethraliss)
            134923,  // Toxic Saurid (snake cultist)
        };
        a.mandatory_interrupt_spells = {
            263793,  // Poison Nova (Merektha)
            263959,  // Cataclysm cast (Sethraliss heart)
            263957,  // Galvanizing Field telegraph (Galvazzt)
            263953,  // Snake Charm (Merektha)
        };
        a.cc_priority_entries = {
            134364,
            134923,
        };
        a.dangerous_auras = {
            263957,  // Galvanizing Field zone
            268016,  // Blinding Sand pool
            268007,  // Plague Bomb (Plague Doctor)
        };
        // Progression waypoints — Temple of Sethraliss is a Vol'dun
        // serpent shrine with outdoor courtyards.
        a.progression_waypoints = {
            { 1740.0f,   33.0f,  85.0f },   // entry
            { 1655.0f,   55.0f, 100.0f },   // Adderis & Aspix arena
            { 1573.0f,    9.0f, 102.0f },   // Merektha snake pit
            { 1438.0f,   45.0f, 122.0f },   // Galvazzt platform
            { 1320.0f,   70.0f, 122.0f },   // Avatar of Sethraliss heart
        };
        // Boss progression — Temple of Sethraliss has 4 encounters
        // (Adderis+Aspix is a duo first slot).
        a.bosses = {
            133379,  // Adderis (Adderis & Aspix duo)
            133944,  // Aspix
            133384,  // Merektha
            133389,  // Galvazzt
            133392,  // Avatar of Sethraliss (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTempleOfSethralissScript()
{
    return std::make_unique<TempleOfSethralissScript>();
}

} // namespace Playerbot

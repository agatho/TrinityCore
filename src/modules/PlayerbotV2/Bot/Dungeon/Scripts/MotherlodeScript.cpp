// MotherlodeScript — The MOTHERLODE!! (map 1594, BfA 110-120).
// Goblin Venture Co mining-themed dungeon in Drustvar.
//   * Coin-Operated Crowd Pummeler — Pummel + Pinch (charge).
//   * Azerokk — Earthrending Slam (zone) + Earthen Adds priority kill.
//   * Rixxa Fluxflame — Azerite Catalyst (charge) + Propellant Blast.
//   * Mogul Razdunk (final) — Drill Smash (zone) + Goblin All-In-1 turret.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MotherlodeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1594; }
    char const* name() const override { return "motherlode"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            129802,  // Earthen Reverberation add (Azerokk)
            132236,  // Caustic Spotter
        };
        a.mandatory_interrupt_spells = {
            257337,  // Shocking Claw (CO Pummeler)
            262711,  // Azerite Volley (Rixxa)
            263202,  // Iced Spritzer (Rixxa cleanse)
            259572,  // Drill Smash (Razdunk)
            259856,  // Chemical Burn (Rixxa)
        };
        a.cc_priority_entries = {
            129802,
            132236,
        };
        a.dangerous_auras = {
            262794,  // Azerite Catalyst zone
            259572,  // Drill Smash ground
            263041,  // Propellant Blast
            259853,  // Chemical Burn pool
        };
        // Progression waypoints — The MOTHERLODE!! is a goblin
        // azerite mine with elevator phase transitions.
        a.progression_waypoints = {
            {  819.0f, -208.0f,  -49.0f },   // entry
            {  769.0f, -224.0f,  -48.0f },   // Coin-Op arena
            {  680.0f, -187.0f,  -83.0f },   // Azerokk mine pit
            {  698.0f, -312.0f, -103.0f },   // Rixxa chemical hall
            {  775.0f, -329.0f, -157.0f },   // Razdunk final platform
        };
        // Boss progression — The MOTHERLODE!! has 5 encounters.
        a.bosses = {
            129214,  // Coin-Operated Crowd Pummeler
            129227,  // Azerokk
            133430,  // Rixxa Fluxflame
            129231,  // Mogul Razdunk (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMotherlodeScript()
{
    return std::make_unique<MotherlodeScript>();
}

} // namespace Playerbot

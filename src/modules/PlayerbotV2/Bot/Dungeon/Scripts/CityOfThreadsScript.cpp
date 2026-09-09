// CityOfThreadsScript — City of Threads (map 2669, TWW 70-80).
// Azj-Kahet nerubian capital — Manaforge dungeon. 4 bosses:
//   * Orator Krix'vizk  — Chains of Oppression / Subjugate / Terrorize
//   * Fangs of the Queen — twin nerubian fight (no TC boss script yet)
//   * The Coaglamation  — gossamer mechanics (no TC boss script yet)
//   * Izo, the Grand Splicer — final boss (no TC boss script yet)
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/KhazAlgar/CityOfThreads/boss_orator_krix_vizk.cpp
// Only the Orator's encounter is implemented; other bosses fall through
// to generic engagement logic until TC ships them.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class CityOfThreadsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2669; }
    char const* name() const override { return "city_of_threads"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
        };
        a.mandatory_interrupt_spells = {
            // Orator Krix'vizk
            454689,  // Oration — boss buff cast; kick to prevent stack
            434722,  // Subjugate — single-target CC on player
            434779,  // Terrorize — fear cast
            434829,  // Vociferous Indoctrination — channel damage
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Orator Krix'vizk
            434691,  // Chains of Oppression — ground tether telegraph
            434710,  // Chains of Oppression damage — caster zone
            434832,  // Vociferous Indoctrination damage zone
            434923,  // Lingering Influence — area trigger
            434926,  // Lingering Influence damage
            448561,  // Shadows of Doubt — debuff cleanse via movement
        };
        // Boss progression — entries from TC's
        // src/server/scripts/KhazAlgar/CityOfThreads/city_of_threads.h.
        // Used by tank-advance for cross-room navigation when no enemies
        // are within the snapshot's 30y nearby_enemies window.
        a.bosses = {
            216619,  // Orator Krix'vizk
            216648,  // The Fangs of the Queen — Nx half (twin fight)
            216649,  // The Fangs of the Queen — Vx half
            216320,  // The Coaglamation
            216658,  // Izo, the Grand Splicer (final)
        };
        // Progression waypoints — City of Threads is an Azj-Kahet
        // nerubian city with web-bridge transitions.
        a.progression_waypoints = {
            { -445.0f,  900.0f, 1390.0f },   // entry
            { -360.0f,  979.0f, 1395.0f },   // Krix'vizk plaza
            { -250.0f, 1112.0f, 1395.0f },   // Fangs twin arena
            { -150.0f, 1240.0f, 1413.0f },   // Coaglamation pit
            {  -47.0f, 1370.0f, 1413.0f },   // Izo grand throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeCityOfThreadsScript()
{
    return std::make_unique<CityOfThreadsScript>();
}

} // namespace Playerbot

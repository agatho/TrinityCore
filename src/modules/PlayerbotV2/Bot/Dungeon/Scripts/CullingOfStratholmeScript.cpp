// CullingOfStratholmeScript — Culling of Stratholme (map 595, WotLK 75-80).
// Caverns of Time #4. Timed wave-defense + escort + boss. 4 encounters:
// Meathook, Salramm the Fleshcrafter, Chrono-Lord Epoch, Mal'Ganis.
// Heroic bonus: Infinite Corruptor (timed).
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/CavernsOfTime/CullingOfStratholme/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class CullingOfStratholmeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 595; }
    char const* name() const override { return "culling_of_stratholme"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            27737,  // Risen Zombie (city wave)
            28249,  // Devouring Ghoul (city wave)
            32273,  // Infinite Corruptor (timed bonus boss)
            27742,  // Infinite Adversary
            27743,  // Infinite Hunter
        };
        a.mandatory_interrupt_spells = {
            // Salramm
            52708,  // Steal Flesh
            58845,  // Curse of Twisted Flesh
            52451,  // Summon Ghouls
            // Meathook
            52723,  // Vampiric Touch
            // Chrono-Lord Epoch
            52772,  // Curse of Exertion
            52766,  // Time Warp
            58848,  // Time Stop
            // Infinite Corruptor (bonus)
            60588,  // Corrupting Blight
            60590,  // Void Strike
            60422,  // Corruption of Time channel
        };
        a.cc_priority_entries = {
            27742,  // Infinite Adversary (caster)
        };
        a.dangerous_auras = {
            // Salramm
            58841,  // Frenzy (enrage)
            52711,  // Steal Flesh debuff
            // Chrono-Lord
            52736,  // Time Step Dummy zone
            // Corruptor
            60451,  // Corruption of Time target
        };
        // Boss progression — NPC entries from TC instance script.
        a.bosses = {
            26529,  // Meathook
            26530,  // Salramm the Fleshcrafter
            26532,  // Chrono-Lord Epoch
            26533,  // Mal'Ganis (final)
            32273,  // Infinite Corruptor (heroic-only bonus boss)
        };
        // Progression waypoints — CoS is event-driven (Arthas escort)
        // with houses-to-clear waves. Waypoints land tank at each wave
        // staging point; Arthas NPC's path drives the event tempo.
        a.progression_waypoints = {
            { 1735.0f,  1284.0f, 140.0f },   // entry plaza
            { 1690.0f,  1271.0f, 141.0f },   // Meathook street
            { 2080.0f,  1287.0f, 141.0f },   // Salramm cul-de-sac
            { 2233.0f,  1390.0f, 130.0f },   // square (Epoch)
            { 2280.0f,  1547.0f, 134.0f },   // Mal'Ganis manor
            { 2381.0f,  1265.0f, 132.0f },   // Infinite Corruptor (heroic)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeCullingOfStratholmeScript()
{
    return std::make_unique<CullingOfStratholmeScript>();
}

} // namespace Playerbot

// AraKaraScript — Ara-Kara, City of Echoes (map 2660, TWW 70-80).
// Azj-Kahet nerubian temple-city dungeon.
//   * Avanoxx — Insatiable Hunger (debuff stacks) + Gossamer Onslaught.
//   * Anub'zekt — Eye of the Swarm (zone) + Burrow Charge.
//   * Ki'katal the Harvester — Cosmic Singularity (interrupt) + Black Blood.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AraKaraScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2660; }
    char const* name() const override { return "ara_kara"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            213937,  // Anub'azal Webmage (Avanoxx pulls)
            213334,  // Bloodstained Webmage (Ki'katal)
            217531,  // Ixin (Avanoxx)
        };
        a.mandatory_interrupt_spells = {
            434713,  // Insatiable Hunger (Avanoxx)
            433740,  // Cosmic Singularity (Ki'katal)
            436322,  // Eye of the Swarm telegraph (Anub'zekt)
            434589,  // Black Blood Eruption (Ki'katal)
            433778,  // Eye of the Swarm cast
        };
        a.cc_priority_entries = {
            213937,
            213334,
        };
        a.dangerous_auras = {
            434713,  // Insatiable Hunger pool
            433740,  // Cosmic Singularity zone
            436322,  // Eye of the Swarm
            434589,  // Black Blood Eruption
        };
        // Boss progression — TWW S1 dungeon, no TC instance script.
        // Entries from WoWHead — verify in-game if needed.
        a.bosses = {
            213179,  // Avanoxx
            215405,  // Anub'zekt
            215407,  // Ki'katal the Harvester (final)
        };
        // Progression waypoints — Ara-Kara is an Azj-Kahet nerubian
        // cave with linear chamber progression.
        a.progression_waypoints = {
            {  555.0f, -1370.0f, 1413.0f },   // entry
            {  490.0f, -1310.0f, 1413.0f },   // Avanoxx chamber
            {  448.0f, -1207.0f, 1399.0f },   // Anub'zekt arena
            {  381.0f, -1067.0f, 1390.0f },   // Ki'katal sanctum
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAraKaraScript()
{
    return std::make_unique<AraKaraScript>();
}

} // namespace Playerbot

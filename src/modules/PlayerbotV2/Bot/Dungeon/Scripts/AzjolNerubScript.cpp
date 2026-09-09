// AzjolNerubScript — Azjol-Nerub (map 601, WotLK 72-78).
// 3 bosses: Krik'thir the Gatewatcher, Hadronox, Anub'arak.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/AzjolNerub/AzjolNerub/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AzjolNerubScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 601; }
    char const* name() const override { return "azjol_nerub"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            29105,  // Watcher Narjil (Krik'thir gate)
            29104,  // Watcher Silthik (Krik'thir gate)
            28922,  // Crypt Fiend (Hadronox add)
        };
        a.mandatory_interrupt_spells = {
            // Krik'thir
            53030,  // Leech Poison
            53418,  // Pierce Armor (debuff)
            // Hadronox (acid)
            53400,  // Acid Cloud
            // Anub'arak
            53617,  // Poison Bolt (assassin add)
            53520,  // Carrion Beetles (summons adds)
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Anub'arak
            59432,  // Pound damage zone (frontal)
            53456,  // Impale Aura (burrow phase ground)
            53455,  // Impale Visual telegraph
            53454,  // Impale Damage
            53467,  // Leeching Swarm
            // Krik'thir
            57731,  // Web Grab (silence/pull)
        };
        // Boss progression — NPC entries from TC's azjol_nerub.h.
        a.bosses = {
            28684,  // Krik'thir the Gatewatcher
            28921,  // Hadronox
            29120,  // Anub'arak (final)
        };
        // Progression waypoints — AN is a vertical descent.
        a.progression_waypoints = {
            { 549.6f, 254.0f, 222.2f },   // entry web
            { 415.0f, 211.0f, 224.4f },   // Krik'thir gate
            { 519.0f, 553.5f, 731.9f },   // Hadronox platform
            { 552.0f, 251.0f, 224.0f },   // Anub'arak pit
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAzjolNerubScript()
{
    return std::make_unique<AzjolNerubScript>();
}

} // namespace Playerbot

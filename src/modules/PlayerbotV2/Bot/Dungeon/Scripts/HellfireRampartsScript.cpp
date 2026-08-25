// HellfireRampartsScript — Hellfire Ramparts (map 543, TBC 60-66).
// 3 bosses: Watchkeeper Gargolmar, Omor the Unscarred, Vazruden + Nazan.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/HellfireCitadel/HellfireRamparts/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HellfireRampartsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 543; }
    char const* name() const override { return "hellfire_ramparts"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
        };
        a.mandatory_interrupt_spells = {
            // Omor the Unscarred
            30637,  // Orbital Strike
            30638,  // Shadow Whip
            31901,  // Demonic Shield
            30686,  // Shadow Bolt
            30707,  // Summon Fiendish Hound
            // Watchkeeper Gargolmar
            30641,  // Mortal Wound
            34645,  // Surge
            // Vazruden / Nazan
            34653,  // Fireball
            30926,  // Cone of Fire
            39427,  // Bellowing Roar (fear)
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Omor
            30695,  // Treacherous Aura (debuff requires movement)
            // Nazan
            23971,  // Summon Liquid Fire (ground effect)
            30928,  // Summon Liquid Fire (heroic)
            30926,  // Cone of Fire telegraph
            19823,  // Fire Nova visual
            // Gargolmar
            30621,  // Kidney Shot (player stun)
        };
        // Boss progression — NPC entries from TC's hellfire_ramparts.h.
        // Vazruden encounter spawns Nazan (the dragon mount) during P2;
        // both entries listed so tank-advance finds whichever is alive.
        a.bosses = {
            17306,  // Watchkeeper Gargolmar
            17308,  // Omor the Unscarred
            17307,  // Vazruden the Herald
            17537,  // Vazruden (P2 dismounted)
            17536,  // Nazan
        };
        // Progression waypoints — Ramparts is a linear outdoor cliff
        // dungeon: entry → Gargolmar's plateau → bridge → Omor's keep →
        // courtyard for Vazruden's gunship event.
        a.progression_waypoints = {
            { -1356.6f,  1632.7f,  68.5f },   // entry
            { -1432.3f,  1672.7f,  68.0f },   // Gargolmar plateau
            { -1485.0f,  1768.0f,  68.7f },   // bridge
            { -1485.0f,  1838.0f,  82.3f },   // Omor's keep
            { -1380.0f,  1869.0f,  90.0f },   // Vazruden courtyard
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHellfireRampartsScript()
{
    return std::make_unique<HellfireRampartsScript>();
}

} // namespace Playerbot

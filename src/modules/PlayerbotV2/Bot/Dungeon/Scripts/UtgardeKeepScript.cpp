// UtgardeKeepScript — Utgarde Keep (map 574, WotLK 70-75).
// 3 bosses: Prince Keleseth, Skarvald & Dalronn, Ingvar the Plunderer.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/UtgardeKeep/UtgardeKeep/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UtgardeKeepScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 574; }
    char const* name() const override { return "utgarde_keep"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            // Frost Tomb summon entry — should be the creature spawned
            // by spell 42714 (boss_keleseth.cpp:39 SPELL_FROST_TOMB_SUMMON).
            // Previous value 23561 was a Naxxramas skeleton entry (Kel'Thuzad
            // adds) — wrong dungeon. Real entry needs DB lookup; left
            // empty so killing the Frost Tomb falls to proximity engagement.
            // Previous value 24201 was Dalronn the Controller (a BOSS, listed
            // correctly in bosses[] below) — also dropped.
        };
        a.mandatory_interrupt_spells = {
            // Keleseth
            48400,  // Frost Tomb
            43667,  // Shadowbolt
            50657,  // Shadow Fissure
            42702,  // Decrepify
            // Skarvald & Dalronn (duo)
            48583,  // Stone Strike
            43649,  // Shadow Bolt (Dalronn)
            43650,  // Debilitate
            // Ingvar
            42730,  // Woe Strike
            42729,  // Dreadful Roar
            42795,  // Feign Death (phase trigger)
            42863,  // Scourge Resurrection
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Ingvar
            42669,  // Smash
            42723,  // Dark Smash (cone)
            42724,  // Cleave
            42708,  // Staggering Roar (AoE)
            42750,  // Shadow Axe damage zone
            // Skarvald
            48193,  // Enrage
            // Keleseth
            42672,  // Frost Tomb stun
        };
        // Boss progression — NPC entries from TC's utgarde_keep.h.
        // Skarvald & Dalronn is a duo encounter; either entry resolves
        // the room — tank-advance finds whichever is closer.
        a.bosses = {
            23953,  // Prince Keleseth
            24200,  // Skarvald the Constructor
            24201,  // Dalronn the Controller
            23954,  // Ingvar the Plunderer (final)
        };
        // Progression waypoints — UK is a linear Vrykul keep.
        a.progression_waypoints = {
            { 156.5f, 256.0f,  41.4f },   // entry
            { 211.0f, 281.0f,  42.0f },   // Keleseth crypt
            { 264.0f, 312.0f,  -5.0f },   // forge descent
            { 296.0f, 235.0f,  -5.0f },   // Skarvald/Dalronn arena
            { 213.0f, 188.0f,  43.0f },   // Ingvar throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUtgardeKeepScript()
{
    return std::make_unique<UtgardeKeepScript>();
}

} // namespace Playerbot

// PitOfSaronScript — Pit of Saron (map 658, WotLK 80).
// Frozen Halls #2. 3 bosses: Forgemaster Garfrost, Ick + Krick (duo),
// Scourgelord Tyrannus.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/FrozenHalls/PitOfSaron/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class PitOfSaronScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 658; }
    char const* name() const override { return "pit_of_saron"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
        };
        a.mandatory_interrupt_spells = {
            // Garfrost
            68788,  // Throw Saronite
            68771,  // Thundering Stomp
            68778,  // Chilling Wave
            70381,  // Deep Freeze
            // Ick + Krick
            69021,  // Mighty Kick (Ick)
            69028,  // Shadow Bolt (Krick)
            69024,  // Toxic Waste (Krick)
            69012,  // Explosive Barrage (Krick)
            69263,  // Explosive Barrage (Ick)
            68989,  // Poison Nova
            68987,  // Pursuit (fixate)
            // Tyrannus
            69155,  // Forceful Smash
            69167,  // Unholy Power
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Garfrost
            70326,  // Permafrost stack debuff
            // Ick + Krick
            69015,  // Explosive Barrage summon
            69017,  // Exploding Orb visual
            69019,  // Explosive Barrage damage
            // Tyrannus
            69172,  // Overlord's Brand
            69189,  // Overlord's Brand Damage (reflect)
            69275,  // Mark of Rimefang
            69246,  // Hoarfrost
        };
        // Boss progression — NPC entries from TC's pit_of_saron.h.
        // Ick+Krick is a duo encounter; both entries listed so tank-
        // advance finds whichever is closer.
        a.bosses = {
            36494,  // Forgemaster Garfrost
            36476,  // Ick (duo)
            36477,  // Krick (duo)
            36658,  // Scourgelord Tyrannus (final)
        };
        // Progression waypoints — PoS is an outdoor ICC quarry.
        a.progression_waypoints = {
            { 463.0f, 213.0f, 528.7f },   // entry
            { 716.0f, 187.0f, 530.0f },   // Garfrost forge
            { 952.0f, 117.0f, 528.6f },   // Ick/Krick path
            { 945.0f, 232.0f, 575.5f },   // gauntlet ramp
            { 826.0f, 145.0f, 590.3f },   // Tyrannus arena
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakePitOfSaronScript()
{
    return std::make_unique<PitOfSaronScript>();
}

} // namespace Playerbot

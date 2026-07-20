// UtgardePinnacleScript — Utgarde Pinnacle (map 575, WotLK 80).
// 4 bosses: Svala Sorrowgrave, Gortok Palehoof, Skadi the Ruthless,
// King Ymiron.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/UtgardeKeep/UtgardePinnacle/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UtgardePinnacleScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 575; }
    char const* name() const override { return "utgarde_pinnacle"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            26687,  // Tribute Skeleton (Ymiron summons)
            26683,  // Skadi adds
        };
        a.mandatory_interrupt_spells = {
            // Gortok Palehoof
            48260,  // Arcing Smash
            48256,  // Withering Roar
            48140,  // Chain Lightning (Skarvald-style add)
            48144,  // Terrifying Roar (fear)
            48133,  // Poison Breath
            // Skadi
            50234,  // Crush
            50255,  // Poisoned Spear (DoT)
            50228,  // Whirlwind
            // Skadi adds
            48639,  // Hamstring
            48640,  // Strike
            49084,  // Shadow Bolt (Witch Doctor)
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Skadi (drake breath phase)
            47579,  // Freezing Cloud (drake)
            47594,  // Freezing Cloud right area
            47574,  // Freezing Cloud left area
            // Gortok minibosses
            48261,  // Impale damage zone
            48137,  // Mortal Wound stack
            48130,  // Gore (charge)
            48131,  // Stomp
            48136,  // Acid Splatter zone
            48105,  // Grievous Wound DoT
        };
        // Boss progression — NPC entries from TC's utgarde_pinnacle.h.
        a.bosses = {
            26668,  // Svala Sorrowgrave
            26687,  // Gortok Palehoof
            26693,  // Skadi the Ruthless
            26861,  // King Ymiron (final)
        };
        // Progression waypoints — UP is the upper tower of Utgarde.
        a.progression_waypoints = {
            { 211.0f, -296.0f,  111.0f },   // entry
            { 296.0f, -355.0f,   91.5f },   // Svala altar
            { 250.5f, -489.0f,  102.4f },   // Palehoof arena
            { 401.0f, -528.0f,  117.6f },   // Skadi bridge
            { 547.0f, -135.0f,  144.0f },   // Ymiron throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUtgardePinnacleScript()
{
    return std::make_unique<UtgardePinnacleScript>();
}

} // namespace Playerbot

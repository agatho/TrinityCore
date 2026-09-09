// BrackenhideHollowScript — Brackenhide Hollow (map 2522, DF 60-70).
// Gnoll-themed dungeon in the Ohn'ahran Plains.
//   * Hackclaw's War-Band — multi-add fight, kill Tricktotem first.
//   * Treemouth — captive rescue mechanic.
//   * Gutshot — Disembowel cleave (move) + Hide Pile.
//   * Decatriarch Wratheye (final) — Decay Curse (dispel) + Withering Burst.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BrackenhideHollowScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2520; }  // all 4 bosses spawn on 2520, not 2522 (DB audit 2026-07-21)
    char const* name() const override { return "brackenhide_hollow"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            186125,  // Tricktotem (totem caster)
            186122,  // Rira Hackclaw (heal-buff caster)
            185656,  // Filth Caller add (Wratheye)
        };
        a.mandatory_interrupt_spells = {
            373767,  // Bone Bolt (Tricktotem)
            373896,  // Withering Burst (Wratheye)
            373912,  // Withering Curse (Wratheye)
            386546,  // Decay Aura cleanse (Wratheye)
        };
        a.cc_priority_entries = {
            186125,  // Tricktotem
            185656,  // Filth Caller
        };
        a.dangerous_auras = {
            373896,  // Withering Burst zone
            385817,  // Disembowel front cone (Gutshot)
        };
        // Boss progression — Brackenhide Hollow has 4 encounters.
        // Hackclaw's War-Band is a multi-add fight using lead NPC entry.
        // Correct template IDs; bosses are event-summoned (0 spawn rows
        // on map 2522) — navigator falls back to waypoints.
        a.bosses = {
            186122,  // Hackclaw's War-Band (Rira Hackclaw as fight-lead)
            186120,  // Treemouth
            186116,  // Gutshot
            186121,  // Decatriarch Wratheye (final)
        };
        // Progression waypoints — Brackenhide Hollow is an Ohn'ahran
        // Plains gnoll camp with outdoor + cave sections.
        a.progression_waypoints = {
            {  280.0f, -785.0f, -1.0f },   // entry
            {  340.0f, -707.0f,  6.0f },   // Hackclaw camp
            {  434.0f, -603.0f, 15.0f },   // Treemouth pit
            {  535.0f, -534.0f, 22.0f },   // Gutshot stairs
            {  640.0f, -464.0f, 38.0f },   // Wratheye altar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBrackenhideHollowScript()
{
    return std::make_unique<BrackenhideHollowScript>();
}

} // namespace Playerbot

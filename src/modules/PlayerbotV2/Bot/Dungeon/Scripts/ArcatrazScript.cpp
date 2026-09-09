// ArcatrazScript — The Arcatraz (map 552, TBC 67-72).
// Tempest Keep wing. 4 bosses: Zereketh the Unbound, Dalliah the
// Doomsayer, Wrath-Scryer Soccothrates, Harbinger Skyriss.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/TempestKeep/arcatraz/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ArcatrazScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 552; }
    char const* name() const override { return "arcatraz"; }

    // Skyriss is summoned by activating the three Warden's Shield consoles —
    // a clientless bot squad can't trigger it, so his encounter never leaves
    // NOT_STARTED (0 static spawns, verified). Exclude from the full-clear
    // completion gate so the run reads complete after the other 3 bosses.
    std::vector<uint32_t> event_summoned_bosses() const override { return { 20912 }; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            21436,  // Skyriss Mirror Image
        };
        a.mandatory_interrupt_spells = {
            // Skyriss
            36924,  // Mind Rend
            37162,  // Domination
            // Dalliah
            36173,  // Gift of the Doomsayer
            36142,  // Whirlwind
            36144,  // Heal
            39016,  // Shadow Wave (heroic)
            // Soccothrates
            35759,  // Felfire Shock
            36512,  // Knock Away
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Skyriss
            39415,  // Fear
            36929,  // Mind Rend Image (illusion damage)
            // Dalliah
            36142,  // Whirlwind zone
        };
        // Boss progression — TC arcatraz.h has Dalliah + Soccothrates;
        // Zereketh (20870) and Skyriss (20912) entries from WoWHead /
        // BossAI registration.
        a.bosses = {
            20870,  // Zereketh the Unbound
            20885,  // Dalliah the Doomsayer
            20886,  // Wrath-Scryer Soccothrates
            20912,  // Harbinger Skyriss (final)
        };
        // Progression waypoints — Arcatraz is the Tempest Keep north
        // prison satellite, a 4-floor tower with lifts. Final fight
        // Skyriss includes 2 escape-pod adds.
        a.progression_waypoints = {
            {  159.0f,  152.5f,  -16.0f },   // entry
            {  179.0f,   97.0f,  -16.0f },   // Zereketh floor
            {  227.0f,   17.0f,    0.0f },   // Dalliah level
            {  314.0f,  -34.0f,   28.0f },   // Soccothrates level
            {  430.0f,  -83.0f,   65.5f },   // Skyriss top
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeArcatrazScript()
{
    return std::make_unique<ArcatrazScript>();
}

} // namespace Playerbot

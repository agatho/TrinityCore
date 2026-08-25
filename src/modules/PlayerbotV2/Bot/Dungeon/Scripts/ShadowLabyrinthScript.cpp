// ShadowLabyrinthScript — Shadow Labyrinth (map 555, TBC 70).
// Auchindoun heroic-tier. 4 bosses: Ambassador Hellmaw, Blackheart the
// Inciter, Grandmaster Vorpil, Murmur.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/Auchindoun/ShadowLabyrinth/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ShadowLabyrinthScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 555; }
    char const* name() const override { return "shadow_labyrinth"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            21503,  // Voidwalker Minion (Vorpil)
        };
        a.mandatory_interrupt_spells = {
            // Hellmaw
            33551,  // Corrosive Acid
            33547,  // Fear
            30231,  // Banish
            // Blackheart
            33676,  // Incite Chaos
            33709,  // Charge
            33707,  // War Stomp
            // Vorpil
            33841,  // Shadowbolt Volley
            33617,  // Rain of Fire
            38791,  // Banish
            // Murmur
            33657,  // Resonance
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Blackheart
            33684,  // Incite Chaos (party debuffs)
            // Vorpil
            33563,  // Draw Shadows (pull)
            33846,  // Shadow Nova
            33783,  // Empowering Shadows
            // Hellmaw
            34970,  // Enrage
        };
        // Boss progression — TC shadow_labyrinth.h has 3 of 4 entries;
        // Murmur (18708) from WoWHead.
        a.bosses = {
            18731,  // Ambassador Hellmaw
            18667,  // Blackheart the Inciter
            18732,  // Grandmaster Vorpil
            18708,  // Murmur (final)
        };
        // Progression waypoints — Shadow Labyrinth is the eastern
        // Auchindoun wing: long descent through 4 chambers.
        a.progression_waypoints = {
            { -180.0f,  -65.5f,   7.0f },   // entry
            { -100.0f, -160.0f,  -2.0f },   // Hellmaw circle
            {   25.0f, -218.5f,  -2.0f },   // Blackheart hall
            {  220.0f, -160.5f,   2.6f },   // Vorpil platform
            {  310.0f,  -89.0f,  18.6f },   // Murmur's chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeShadowLabyrinthScript()
{
    return std::make_unique<ShadowLabyrinthScript>();
}

} // namespace Playerbot

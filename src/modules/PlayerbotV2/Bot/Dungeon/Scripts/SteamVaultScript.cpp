// SteamvaultScript — The Steamvault (map 545, TBC 67-72).
// Coilfang heroic-tier wing. 3 bosses: Hydromancer Thespia, Mekgineer
// Steamrigger, Warlord Kalithresh.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Outland/CoilfangReservoir/SteamVault/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SteamvaultScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 545; }
    char const* name() const override { return "steamvault"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17713,  // Coilfang Water Elemental (Thespia adds)
            17876,  // Steamrigger Mechanic
            17977,  // Naga Distiller (Kalithresh buff source)
        };
        a.mandatory_interrupt_spells = {
            // Thespia
            31481,  // Lung Burst
            31718,  // Enveloping Winds
            34449,  // Water Bolt Volley
            // Steamrigger
            31485,  // Super Shrink Ray
            31486,  // Saw Blade
            35107,  // Electrified Net
            17201,  // Dispel Magic
            31532,  // Repair (mechanic adds)
            31534,  // Spell Reflection
            // Kalithresh
            39061,  // Impale
            37081,  // Warlord's Rage
            31543,  // Warlord's Rage (Naga Distiller)
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Thespia
            25033,  // Lightning Cloud (persistent zone)
            // Kalithresh
            36453,  // Warlord's Rage Proc
        };
        // Boss progression — NPC entries from TC's steam_vault.h.
        a.bosses = {
            17797,  // Hydromancer Thespia
            17796,  // Mekgineer Steamrigger
            17798,  // Warlord Kalithresh (final)
        };
        // Progression waypoints — Steamvault is a 3-section pump house:
        // entry → naga halls → Thespia pool → Steamrigger workshop →
        // Kalithresh's tank room.
        a.progression_waypoints = {
            {  -32.5f, -224.5f, -16.6f },   // entry
            {  -38.5f, -302.0f, -17.0f },   // Thespia pool
            {   24.6f, -286.0f, -25.0f },   // workshop ramp
            {   38.0f, -177.0f, -17.0f },   // Steamrigger
            {  -39.0f,  -94.5f, -17.0f },   // Kalithresh tank room
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSteamvaultScript()
{
    return std::make_unique<SteamvaultScript>();
}

} // namespace Playerbot

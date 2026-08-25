// GnomereganScript — Gnomeregan (map 90, vanilla 24-34).
// Mechanical / Troggs / Leper Gnome dungeon. Notable:
//   * Mekgineer Thermaplugg final fight: bombs from machinist
//     vendors + adds spawn from Walking Bombs. Adds (entry 7395
//     "Walking Bomb") must be priority killed before they detonate.
//   * Crowd Pummeler 9-60 — fires Pummel (interruptible, knockback).
//   * Grubbis pulls Viscous Fallout — single-target tank-and-spank
//     after first phase clears.
//   * Snipe (Crowd Pummeler) — interruptible ranged nuke.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GnomereganScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 90; }
    char const* name() const override { return "gnomeregan"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            7395,   // Walking Bomb (Thermaplugg adds)
            6228,   // Mechanical Yeti add (Crowd Pummeler trash)
        };
        a.mandatory_interrupt_spells = {
            12555,  // Pummel (Crowd Pummeler)
        };
        a.dangerous_auras = {
            12241,  // Bomb explosion (Walking Bombs) — step away
        };
        // Boss progression — Gnomeregan has multiple bosses.
        a.bosses = {
            7361,   // Grubbis
            7079,   // Viscous Fallout
            6235,   // Electrocutioner 6000
            6229,   // Crowd Pummeler 9-60
            7800,   // Mekgineer Thermaplugg (final)
            6228,   // Dark Iron Ambassador (rare bonus, entry varies)
        };
        // Progression waypoints — Gnomeregan is a multi-level mechanical
        // dungeon. Path: entry tram → workshop → Grubbis pit → toxic
        // floor (Viscous Fallout) → engineering → Electrocutioner →
        // arena (Crowd Pummeler) → Thermaplugg's final stage. Some
        // verticality; Detour will reject any off-mesh hops harmlessly.
        a.progression_waypoints = {
            {  -315.0f,  -159.0f,  -42.0f },   // tram landing
            {  -513.0f,  -106.0f,  -150.0f },  // workshop floor
            {  -332.0f,    27.0f,  -152.0f },  // Grubbis arena
            {  -403.0f,    79.0f,  -141.0f },  // toxic level (Viscous)
            {  -496.0f,    14.0f,  -157.0f },  // Electrocutioner platform
            {  -341.0f,    65.0f,  -150.0f },  // Crowd Pummeler arena
            {  -382.0f,   -28.0f,  -154.0f },  // Thermaplugg's chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGnomereganScript()
{
    return std::make_unique<GnomereganScript>();
}

} // namespace Playerbot

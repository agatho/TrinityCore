// UlduarScript — Ulduar raid (map 603, WotLK 10/25).
// 14 bosses across antechamber + iron council + pillars + descent + Algalon.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Ulduar/Ulduar/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UlduarScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 603; }
    char const* name() const override { return "ulduar"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            33121,  // Iron Construct (Ignis add, Slag Imbued / Brittle)
            33329,  // Heart of the Deconstructor (XT-002 exposed heart)
            33343,  // XS-013 Scrapbot (XT-002 add — heals boss on contact)
            33344,  // XM-024 Pummeller (XT-002 add)
            32933,  // Left Arm (Kologarn — kill to free grabbed players)
            33846,  // Dark Rune Sentinel (Razorscale add)
            33113,  // Flame Leviathan
        };
        a.mandatory_interrupt_spells = {
            // Flame Leviathan
            62376,  // Battering Ram
            62374,  // Pursued
            // Ignis
            62680,  // Flame Jets
            62546,  // Scorch
            62717,  // Slag Pot
            // Razorscale
            63236,  // Devouring Flame
            62666,  // Wing Buffet
            63815,  // Fireball
            64821,  // Fuse Armor
            // XT-002
            63018,  // Searing Light
            63024,  // Gravity Bomb
            62776,  // Tympanic Tantrum
            // Iron Council (Assembly of Iron)
            61888,  // Fusion Punch (Steelbreaker)
            // Hodir
            61968,  // Flash Freeze
            62469,  // Freeze
            62234,  // Icicle
            // Thorim
            62042,  // Stormhammer
            62131,  // Chain Lightning
            62130,  // Unbalancing Strike
            // Freya
            62623,  // Sunbeam
            62437,  // Ground Tremor
            62283,  // Roots
            // Mimiron
            62997,  // Plasma Blast (MK-II)
            63666,  // Napalm Shell
            63631,  // Shock Blast
            // General Vezax
            62662,  // Mark of the Faceless (placeholder — may be different)
            // Yogg-Saron
            64164,  // Lunatic Gaze
            // Algalon
            64412,  // Phase Punch
            64443,  // Big Bang
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Flame Leviathan
            62402,  // Searing Flame
            62292,  // Blaze
            62288,  // Tar
            63575,  // Smoke Trail
            // Ignis
            62680,  // Flame Jets
            62548,  // Strength of the Creator (boss buff)
            62836,  // Slag Imbued
            62717,  // Slag Pot
            // Razorscale
            63236,  // Devouring Flame
            64774,  // Fused Armor
            // XT-002
            63018,  // Searing Light
            63024,  // Gravity Bomb
            65737,  // Heartbreak (hard mode)
            // Iron Council
            61888,  // Fusion Punch
            // Hodir
            61968,  // Flash Freeze
            61969,  // Block of Ice
            61970,  // Summon Block of Ice
            62038,  // Biting Cold
            62469,  // Freeze
            // Thorim
            62042,  // Stormhammer
            62131,  // Chain Lightning
            62130,  // Unbalancing Strike
            62279,  // Lightning Charge
            62186,  // Lightning Orb Charged
            // Freya
            62623,  // Sunbeam
            62437,  // Ground Tremor
            62283,  // Roots
            62519,  // Attuned to Nature (boss buff)
            62285,  // Thorn Swarm
            // Mimiron
            62997,  // Plasma Blast
            63666,  // Napalm Shell
            63631,  // Shock Blast
            63679,  // Heat Wave Aura
            // Yogg-Saron
            64164,  // Lunatic Gaze
            // Algalon
            64412,  // Phase Punch
            64443,  // Big Bang
            65686,  // Universe Implodes
        };
        // Boss progression — NPC entries from TC's ulduar.h.
        a.bosses = {
            33113,  // Flame Leviathan
            33118,  // Ignis the Furnace Master
            33186,  // Razorscale
            33293,  // XT-002 Deconstructor
            32867,  // Iron Council: Steelbreaker
            32927,  // Iron Council: Runemaster Molgeim
            32857,  // Iron Council: Stormcaller Brundir
            32930,  // Kologarn
            33515,  // Auriaya
            32845,  // Hodir
            32865,  // Thorim
            32906,  // Freya
            33350,  // Mimiron
            33271,  // General Vezax
            33288,  // Yogg-Saron
            32871,  // Algalon the Observer
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 603), in encounter order. Yogg-Saron and Algalon are
        // script-spawned (no creature rows) and are omitted.
        a.progression_waypoints = {
            {  420.7f,  -15.4f, 409.8f },   // Flame Leviathan
            {  586.5f,  378.8f, 360.9f },   // Ignis the Furnace Master
            {  587.1f, -203.2f, 441.2f },   // Razorscale
            {  886.3f,  -12.1f, 409.6f },   // XT-002 Deconstructor
            { 1587.2f,  121.0f, 427.3f },   // Iron Council: Steelbreaker
            { 1589.5f,  107.0f, 427.4f },   // Iron Council: Runemaster Molgeim
            { 1567.9f,  129.1f, 427.3f },   // Iron Council: Stormcaller Brundir
            { 1797.2f,  -24.4f, 448.7f },   // Kologarn
            { 1942.3f,   43.5f, 411.4f },   // Auriaya
            { 2000.8f, -204.3f, 432.8f },   // Hodir
            { 2131.0f, -297.6f, 438.3f },   // Thorim
            { 2338.5f,  -52.3f, 425.6f },   // Freya
            { 2742.5f, 2561.0f, 364.4f },   // Mimiron
            { 1852.8f,   81.4f, 342.5f },   // General Vezax
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUlduarScript()
{
    return std::make_unique<UlduarScript>();
}

} // namespace Playerbot

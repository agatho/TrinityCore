// NaxxramasScript — Naxxramas raid (map 533, vanilla 40-man + WotLK 10/25).
// 15 bosses across 4 wings + Frostwyrm Lair.
//   Spider: Anub'Rekhan, Faerlina, Maexxna
//   Plague: Noth, Heigan, Loatheb
//   Construct: Patchwerk, Grobbulus, Gluth, Thaddius
//   Horsemen: Razuvious, Gothik, Four Horsemen
//   Frostwyrm: Sapphiron, Kel'Thuzad
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Naxxramas/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class NaxxramasScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 533; }
    char const* name() const override { return "naxxramas"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            16573,  // Crypt Guard (Anub'Rekhan)
            17055,  // Spiderling
            16486,  // Web Wrap (Maexxna — frees trapped player)
            16506,  // Naxxramas Worshipper (Faerlina)
            16505,  // Naxxramas Follower (Faerlina)
            16803,  // Death Knight Understudy (Razuvious — MC target)
            16360,  // Zombie Chow (Gluth)
            15928,  // Feugen (Thaddius)
            15929,  // Stalagg (Thaddius)
            16441,  // Guardian of Icecrown (Kel'Thuzad)
        };
        a.mandatory_interrupt_spells = {
            // Patchwerk
            28308, 59192,  // Hateful Strike
            // Anub'Rekhan
            28783, 56090,  // Impale
            // Faerlina
            28796,  // Poison Bolt Volley
            28794,  // Rain of Fire
            28798,  // Frenzy
            // Maexxna
            29484, 54125,  // Web Spray
            28776,  // Necrotic Poison
            // Loatheb
            29998, 55011,  // Decrepit Fever
            29865,  // Deathbloom
            // Heigan
            29310,  // Spell Disruption
            // Razuvious
            26613,  // Unbalancing Strike
            29107,  // Disrupting Shout
            55550,  // Jagged Knife
            // Gothik
            29317,  // Shadow Bolt
            27831, 55638,  // Shadow Bolt Volley
            // Sapphiron
            28522,  // Icebolt
            28524,  // Frost Breath
            // Kel'Thuzad
            28478,  // Frostbolt
            28479,  // Frostbolt Volley
            27810,  // Shadow Fissure
            27819,  // Detonate Mana
            // Thaddius
            28089,  // Polarity Shift
            28167,  // Chain Lightning
            28299,  // Ball Lightning
            // Gluth
            28374, 54426,  // Decimate
            28371,  // Enrage
        };
        a.cc_priority_entries = {
            16506,  // Naxxramas Worshipper
            16505,  // Naxxramas Follower
        };
        a.dangerous_auras = {
            // Patchwerk
            32309,  // Slime Bolt
            // Anub'Rekhan
            28785, 54021,  // Locust Swarm
            // Faerlina
            28732,  // Widow's Embrace
            // Maexxna
            28622,  // Web Wrap
            28741,  // Poison Shock
            // Loatheb
            29204,  // Inevitable Doom
            55593,  // Necrotic Aura
            // Heigan
            29371,  // Eruption (dance ground effect)
            // Gothik
            55604, 55645,  // Death Plague
            // Four Horsemen
            28832, 28833, 28834, 28835,  // Marks
            28882, 57369,  // Unholy Shadow
            28884, 57467,  // Meteor
            28863, 57463,  // Void Zone
            // Razuvious
            29125,  // Hopeless
            // Sapphiron
            28531,  // Frost Aura
            29327,  // Wing Buffet Periodic
            // Kel'Thuzad
            27808, 29879,  // Frost Blast
            28410,  // Chains
            // Thaddius
            28134,  // Stalagg Power Surge
            28135,  // Feugen Static Field
            // Gluth
            29307,  // Infected Wound
        };
        // Boss progression — NPC entries from TC's naxxramas.h
        // (Maexxna/Patchwerk/Noth/Loatheb from WoWHead, TC header omits).
        // 15 encounters across 4 wings + Frostwyrm Lair. Tank-advance
        // picks whichever is closest+alive; the snapshot's dedup in
        // MergeAdvice strips duplicates if any sneak in.
        a.bosses = {
            // Arachnid Quarter
            15956,  // Anub'Rekhan
            15953,  // Grand Widow Faerlina
            15952,  // Maexxna
            // Plague Quarter
            15954,  // Noth the Plaguebringer
            15936,  // Heigan the Unclean
            16011,  // Loatheb
            // Military Quarter
            16061,  // Instructor Razuvious
            16060,  // Gothik the Harvester
            16064,  // Thane Korth'azz (Four Horsemen)
            16065,  // Lady Blaumeux (Four Horsemen)
            30549,  // Baron Rivendare (Four Horsemen)
            16063,  // Sir Zeliek (Four Horsemen)
            // Construct Quarter
            16028,  // Patchwerk
            15931,  // Grobbulus
            15932,  // Gluth
            15928,  // Thaddius
            // Frostwyrm Lair
            15989,  // Sapphiron
            15990,  // Kel'Thuzad (final)
        };
        // Progression waypoints — boss spawn positions from
        // world.creature (map 533), in encounter order. The Four
        // Horsemen share one room, so one waypoint covers all four.
        a.progression_waypoints = {
            // Arachnid Quarter
            { 3308.6f, -3476.3f, 287.2f },   // Anub'Rekhan
            { 3353.2f, -3620.1f, 261.1f },   // Grand Widow Faerlina
            { 3511.4f, -3921.6f, 299.5f },   // Maexxna
            // Plague Quarter
            { 2671.6f, -3489.1f, 261.4f },   // Noth the Plaguebringer
            { 2793.9f, -3707.4f, 276.6f },   // Heigan the Unclean
            { 2909.0f, -3997.4f, 274.2f },   // Loatheb
            // Military Quarter
            { 2758.9f, -3107.1f, 267.7f },   // Instructor Razuvious
            { 2642.1f, -3387.0f, 285.5f },   // Gothik the Harvester
            { 2520.5f, -2955.4f, 245.6f },   // Four Horsemen room
            // Construct Quarter
            { 3256.4f, -3230.3f, 294.1f },   // Patchwerk
            { 3227.6f, -3378.3f, 311.3f },   // Grobbulus
            { 3283.1f, -3157.0f, 297.8f },   // Gluth
            { 3513.8f, -2926.6f, 302.9f },   // Thaddius
            // Frostwyrm Lair
            { 3522.4f, -5236.8f, 137.7f },   // Sapphiron
            { 3749.7f, -5114.1f, 142.1f },   // Kel'Thuzad (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeNaxxramasScript()
{
    return std::make_unique<NaxxramasScript>();
}

} // namespace Playerbot

// StratholmeScript — Stratholme (map 329, vanilla 55-60).
// Two wings: Live (Scarlet) and Undead (Baron). Major bosses:
// Magistrate Barthilas, Cannon Master Willey, Postmaster Malown,
// Baroness Anastari, Ramstein the Gorger, Baron Rivendare, etc.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/Stratholme/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class StratholmeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 329; }
    char const* name() const override { return "stratholme"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            10495,  // Black Guard Sentry (Ramstein adds)
            11197,  // Baron's Skeletons
            10916,  // Postmaster trinket add
        };
        a.mandatory_interrupt_spells = {
            // Baron Rivendare
            17393,  // Shadowbolt
            15708,  // Mortal Strike
            17473,  // Raise Dead
            // Baroness Anastari
            16565,  // Banshee Wail
            16867,  // Banshee Curse
            18327,  // Silence
            17244,  // Possess
            // Balnazzar
            17286,  // Crusader's Hammer (AOE stun)
            17281,  // Crusader Strike
            17284,  // Holy Strike
            17399,  // Shadow Shock
            17287,  // Mind Blast
            13704,  // Psychic Scream
            12098,  // Sleep
            15690,  // Mind Control
            // Maleki the Pallid
            17503,  // Frostbolt
            20743,  // Drain Life
            17243,  // Drain Mana
            16869,  // Ice Tomb
            // Hearthsinger Forresten
            // Magistrate Barthilas (Cannon Master Willey trash)
            10101,  // Knock Away
            15615,  // Pummel
            // Nerub'enkan
            4962,   // Encasing Webs
            6016,   // Pierce Armor
            31602,  // Crypt Scarabs
            17235,  // Raise Undead Scarab
        };
        a.cc_priority_entries = {
            10416,  // Crimson Sorcerer
            10417,  // Crimson Priest
        };
        a.dangerous_auras = {
            // Postmaster Malown adds
            16793,  // Draining Blow
            10887,  // Crowd Pummel
            14099,  // Mighty Blow
            16791,  // Furious Anger
            // Cannonmaster
            16496,  // Shoot
            // Baron + adds
            17467,  // Unholy Aura
            17246,  // Possessed (Anastari debuff)
            // Ramstein
            5568,   // Trample
            17307,  // Knockout
            // Timmy
            17470,  // Ravenous Claw
            // Stratholme courier
            7713,   // Wailing Dead
            6253,   // Backhand
            8552,   // Curse of Weakness
            12889,  // Curse of Tongues
            17831,  // Call of the Grave
        };
        // Boss progression — NPC entries from TC's stratholme.h.
        // Stratholme has two wings (Scarlet live + Undead Baron); the
        // tank-advance scan finds whichever boss is closest as the
        // bot moves through the dungeon.
        a.bosses = {
            10558,  // Hearthsinger Forresten
            11032,  // Commander Malor
            10811,  // Instructor Galford
            10516,  // The Unforgiven
            10439,  // Ramstein the Gorger
            10440,  // Baron Rivendare (final, Undead wing)
        };
        // Progression waypoints — Strat is two wings sharing one map_id.
        // Bots default to the Undead Baron path (more frequently run for
        // Baron's mount). Scarlet wing falls back to boss-Cell scan from
        // the Service Entrance. Coords approximate from public dumps.
        a.progression_waypoints = {
            { 4034.5f, -3401.5f, 117.0f },   // Slaughter Square entry
            { 4047.6f, -3360.7f, 119.7f },   // gauntlet street
            { 4042.5f, -3263.7f, 115.0f },   // Unforgiven plaza
            { 4040.0f, -3186.0f, 115.0f },   // Ramstein gate
            { 4060.0f, -3300.0f, 115.0f },   // Baron's gauntlet
            { 4032.7f, -3360.5f, 119.7f },   // approach
            { 4072.0f, -3402.0f, 115.0f },   // Baron Rivendare's throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeStratholmeScript()
{
    return std::make_unique<StratholmeScript>();
}

} // namespace Playerbot

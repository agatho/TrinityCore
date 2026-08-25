// ScholomanceScript — Scholomance (map 289, vanilla revamped 30-40).
// Necromancy-themed Western Plaguelands. Bosses: Darkmaster Gandling,
// Doctor Theolen Krastinov, Lady Illucia Barov, Lord Alexei Barov,
// Kirtonos the Herald, Ras Frostwhisper, etc.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/Scholomance/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ScholomanceScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 289; }
    char const* name() const override { return "scholomance"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            16215,  // Risen Aberration (Gandling)
            10906,  // Spectral Tutor
        };
        a.mandatory_interrupt_spells = {
            // Darkmaster Gandling
            15790,  // Arcane Missiles
            12040,  // Shadow Shield
            18702,  // Curse
            17950,  // Shadow Portal
            // Lady Illucia Barov
            18671,  // Curse of Agony
            12542,  // Fear
            17234,  // Shadow Shock
            12528,  // Silence
            // Lord Alexei Barov
            17831,  // Call of Graves
            11672,  // Corruption
            10917,  // Flash Heal
            10929,  // Renew
            9889,   // Healing Touch
            // Ras Frostwhisper
            17228,  // Shadow Bolt
            12889,  // Curse of Tongues
            14515,  // Dominate Mind
            // Kirtonos the Herald
            18144,  // Swoop
            12882,  // Wing Flap
            8379,   // Disarm
            6016,   // Pierce Armor
            16467,  // Kirtonos Transform
            // Vectus
            18103,  // Backhand
        };
        a.cc_priority_entries = {
            10408,  // Diseased Surgeon
        };
        a.dangerous_auras = {
            // Generic Scholo
            8269,   // Frenzy
            16509,  // Rend
            // Lord Alexei
            17773,  // Illusion
            // The Ravenian
            24673,  // Curse of Blood
        };
        // Boss progression — Scholomance has ~7-8 encounters
        // (some are random spawns / Cata revamp differs from classic).
        a.bosses = {
            10901,  // Lorekeeper Polkelt
            10503,  // Jandice Barov
            11261,  // Doctor Theolen Krastinov
            10504,  // Lord Alexei Barov
            10502,  // Lady Illucia Barov
            10507,  // The Ravenian
            10432,  // Vectus
            10508,  // Ras Frostwhisper
            1853,   // Darkmaster Gandling (final)
        };
        // Progression waypoints — Scholo (classic) is multi-floor with
        // Gandling teleports bots to side rooms throughout the run.
        // Bots default to ground-floor sequential path; teleports are
        // handled by snapshot's nearby_enemies detection.
        a.progression_waypoints = {
            {  297.5f,   65.6f,  76.5f },   // entry hall
            {  254.0f,   85.0f,  76.5f },   // Polkelt's library
            {  299.4f,  -34.0f, 110.2f },   // upstairs to Jandice
            {  216.5f,  -36.0f, 105.0f },   // Krastinov's lab
            {  259.0f,   30.0f, 105.0f },   // Barov twins
            {  290.0f,   91.0f, 105.0f },   // Ravenian/Vectus rooms
            {  213.6f,  -22.0f, 137.0f },   // Ras Frostwhisper top
            {  186.3f,  118.0f, 130.0f },   // Gandling final
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeScholomanceScript()
{
    return std::make_unique<ScholomanceScript>();
}

} // namespace Playerbot

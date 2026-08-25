// TempleOfAhnQirajScript — Temple of Ahn'Qiraj raid (map 531, classic 40-man).
// 9 bosses: Skeram, Bug Trio (Vem/Yauj/Kri), Sartura, Fankriss, Viscidus,
// Huhuran, Twin Emperors (Vek'lor/Vek'nilash), Ouro, C'Thun.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class TempleOfAhnQirajScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 531; }
    char const* name() const override { return "temple_of_ahnqiraj"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            15543, 15544, 15511,  // Bug Trio members (Yauj/Vem/Kri)
            15984,                // Sartura's Royal Guard
            15725, 15726, 15728, 15334, 15802,  // C'Thun tentacles (claw/eye/giant claw/giant eye/flesh)
        };
        a.mandatory_interrupt_spells = {
            // Skeram
            26192,  // Arcane Explosion
            26194,  // Earth Shock
            747,    // Summon Images
            // Bug Trio
            25807,  // Heal (Yauj)
            26350,  // Cleave
            25812,  // Toxic Volley
            // Sartura
            26083,  // Whirlwind
            // Fankriss
            28467,  // Mortal Wound
            28858,  // Root
            // Viscidus
            25993,  // Poison Shock
            25991,  // Poison Bolt Volley
            // Huhuran
            26052,  // Poison Bolt
            26050,  // Acid Spit
            26180,  // Wyvern Sting
            26053,  // Noxious Poison
            // Twin Emperors
            7393,   // Heal Brother
            568,    // Arcane Burst
            26006,  // Shadow Bolt
            26607,  // Blizzard
            26613,  // Unbalancing Strike
            // Ouro
            26102,  // Sand Blast
            26103,  // Sweep
            // C'Thun
            26143,  // Mind Flay
            26029,  // Dark Glare
            26134,  // Green Beam
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Skeram
            26192,  // Arcane Explosion
            // Sartura
            26083,  // Whirlwind
            26027,  // Knockback
            // Fankriss
            28467,  // Mortal Wound
            28858,  // Root
            // Viscidus
            25991,  // Poison Bolt Volley
            26575,  // Toxin
            // Huhuran
            26180,  // Wyvern Sting
            26051,  // Frenzy
            26053,  // Noxious Poison
            // Twin Emperors
            802,    // Mutate Bug
            804,    // Explode Bug
            // Ouro
            26102,  // Sand Blast
            26100,  // Ground Rupture
            // C'Thun
            26029,  // Dark Glare
            26134,  // Green Beam
            26476,  // Digestive Acid (stomach)
            26332,  // Mouth Tentacle
            26139,  // Ground Rupture
        };
        // Boss progression — entries from TC's temple_of_ahnqiraj.h.
        // C'Thun has two phases (Eye 15589 / Body 15727) — listing both.
        a.bosses = {
            15263,  // Prophet Skeram
            15544,  // Vem (Bug Trio)
            15511,  // Lord Kri (Bug Trio)
            15543,  // Princess Yauj (Bug Trio)
            15516,  // Battleguard Sartura
            15510,  // Fankriss the Unyielding
            15299,  // Viscidus
            15509,  // Princess Huhuran
            15275,  // Emperor Vek'nilash
            15276,  // Emperor Vek'lor
            15517,  // Ouro
            15589,  // The Eye of C'Thun
            15727,  // C'Thun (final body phase)
        };
        // Progression waypoints — boss spawn positions from
        // world.creature (map 531), in encounter order. Bug Trio share
        // one chamber (one waypoint); Ouro is script-spawned (no
        // creature row — skipped); Eye/Body of C'Thun share a spawn.
        a.progression_waypoints = {
            { -8346.0f, 2081.0f,  125.7f },   // Prophet Skeram
            { -8598.5f, 2165.3f,   -4.0f },   // Bug Trio chamber (Vem spawn)
            { -8302.5f, 1657.7f,  -29.8f },   // Battleguard Sartura
            { -8085.4f, 1196.7f,  -92.0f },   // Fankriss the Unyielding
            { -7992.4f,  908.2f,  -52.6f },   // Viscidus
            { -8515.8f, 1693.7f,  -90.5f },   // Princess Huhuran
            { -9023.7f, 1176.2f, -104.2f },   // Emperor Vek'nilash
            { -8868.3f, 1206.0f, -104.2f },   // Emperor Vek'lor
            { -8578.8f, 1986.2f,  100.3f },   // C'Thun (Eye + final body phase)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeTempleOfAhnQirajScript()
{
    return std::make_unique<TempleOfAhnQirajScript>();
}

} // namespace Playerbot

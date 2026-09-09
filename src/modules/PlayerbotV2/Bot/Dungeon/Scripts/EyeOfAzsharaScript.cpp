// EyeOfAzsharaScript — Eye of Azshara (map 1456, Legion 110).
// 5 bosses: Warlord Parjesh, Lady Hatecoil, Serpentrix, King Deepbeard,
// Wrath of Azshara.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/BrokenIsles/EyeOfAzshara/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class EyeOfAzsharaScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1456; }
    char const* name() const override { return "eye_of_azshara"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            95876,   // Hydra Spawn (Serpentrix)
        };
        a.mandatory_interrupt_spells = {
            // Serpentrix
            193051,  // Call the Seas
            193018,  // Gaseous Bubbles
            193093,  // Ground Slam
            193152,  // Quake
            // King Deepbeard
            193245,  // Gain Energy
            188169,  // Razor Shards
            188114,  // Shatter
            // Wrath of Azshara
            193977,  // Winds of Northrend
            193460,  // Bane Aura
            193463,  // Bane Missile
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Serpentrix
            193054,  // Call the Seas Area Trigger
            193055,  // Call the Seas Damage
            193047,  // Gaseous Explosion
            193056,  // Ground Slam Missile
            197550,  // Frenzy
            // King Deepbeard
            198024,  // Crystalline Ground
            198028,  // Crystalline Ground Damage
            215898,  // Crystalline Ground Periodic
            215929,  // Rupturing Skitter
            // Wrath of Azshara
            193513,  // Bane Damage
            200194,  // Bane Nova
        };
        // Boss progression — entries from TC's eye_of_azshara.h.
        a.bosses = {
            91784,   // Warlord Parjesh
            91789,   // Lady Hatecoil
            91797,   // King Deepbeard
            91808,   // Serpentrix
            96028,   // Wrath of Azshara (final)
        };
        // Progression waypoints — Eye of Azshara is an outdoor Naga
        // island in Azsuna. Open-world layout; 4 sub-camps + final.
        a.progression_waypoints = {
            { -1490.0f,  -737.0f,  18.0f },   // entry beach
            { -1610.0f,  -650.0f,  18.0f },   // Parjesh camp
            { -1730.0f,  -540.0f,  20.0f },   // Hatecoil cove
            { -1825.0f,  -445.0f,  16.0f },   // Deepbeard cave
            { -1960.0f,  -335.0f,  18.0f },   // Serpentrix lagoon
            { -2080.0f,  -245.0f,  25.0f },   // Wrath of Azshara altar
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeEyeOfAzsharaScript()
{
    return std::make_unique<EyeOfAzsharaScript>();
}

} // namespace Playerbot

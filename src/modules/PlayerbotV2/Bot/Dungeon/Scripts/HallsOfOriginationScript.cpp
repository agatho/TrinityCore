// HallsOfOriginationScript — Halls of Origination (map 644, Cata 80-85).
// Uldum Titan facility. 7 bosses: Temple Guardian Anhuur, Earthrager
// Ptah, Anraphet, Isiset, Ammunae, Setesh, Rajh.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/HallsOfOrigination/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class HallsOfOriginationScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 644; }
    char const* name() const override { return "halls_of_origination"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            39811,  // Temple Adept (Anhuur beam)
            42498,  // Sun Beam (Rajh)
        };
        a.mandatory_interrupt_spells = {
            // Anhuur
            77437,  // Destruction Protocol
            76184,  // Alpha Beams
            // Anraphet
            75609,  // Crumbling Ruin
            75604,  // Nemesis Strike
            // Isiset
            75622,  // Omega Stance
            77370,  // Flame Bolt
            // Ptah
            83650,  // Raging Smash
            75491,  // Sandstorm
            // Rajh
            75592,  // Divine Reckoning
            75115,  // Burning Light
            75322,  // Reverberating Hymn
            74938,  // Shield of Light (interrupt boss invuln)
            75194,  // Searing Light
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Ptah
            75519,  // Ptah Explosion
            75550,  // Quicksand zone
            // Anraphet
            76912,  // Alpha Beams Back Cast
            // Rajh
            74930,  // Beam of Light Left
            76573,  // Beam of Light Right
            76599,  // Activate Beacons
        };
        // Boss progression — Halls of Origination has 7 encounters.
        a.bosses = {
            39425,  // Temple Guardian Anhuur
            39428,  // Earthrager Ptah
            39788,  // Anraphet
            39587,  // Isiset
            39731,  // Ammunae
            39732,  // Setesh
            39378,  // Rajh (final)
        };
        // Progression waypoints — HoO is a 7-boss Uldum titan dungeon
        // with a central plaza and 4 outer wings (Isiset, Ammunae,
        // Setesh, Rajh) that all must be cleared before final.
        a.progression_waypoints = {
            { -380.0f,   80.0f, -213.0f },   // entry
            { -340.0f,  170.0f, -213.0f },   // Anhuur pool
            { -311.0f,   60.0f, -213.0f },   // Ptah chamber
            { -226.0f,   77.0f, -213.0f },   // Anraphet platform
            { -107.0f,  -10.0f, -184.0f },   // Isiset wing
            { -107.0f,  166.0f, -184.0f },   // Ammunae wing
            {  -34.0f,   80.0f, -184.0f },   // Setesh wing
            {  -10.0f,   80.0f, -110.0f },   // Rajh top platform
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeHallsOfOriginationScript()
{
    return std::make_unique<HallsOfOriginationScript>();
}

} // namespace Playerbot

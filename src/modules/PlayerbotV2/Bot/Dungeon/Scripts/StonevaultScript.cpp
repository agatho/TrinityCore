// StonevaultScript — The Stonevault (map 2661, TWW 70-80).
// Earthen titan vault. 4 bosses:
//   * E.D.N.A. — Refracting Beam (interrupt) + Volatile Spike (avoid)
//                + Seismic Smash (avoid frontal) + Stone Shield (interrupt buff)
//   * Skarmorak — Crystalline Smash (avoid) + Fortified Shell (interrupt)
//                 + Crystalline Eruption (avoid) + Void Discharge (move)
//   * Master Machinists (Brokk + Dorlita) — duo fight
//   * Void Speaker Eirich — final void boss
//
// Spell IDs from TC source: src/server/scripts/KhazAlgar/TheStoneVault/
//   boss_edna.cpp, boss_skarmorak.cpp. Master Machinists and Eirich
//   boss scripts aren't in this TC build yet so their advice falls
//   through to generic logic until TC ships them.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class StonevaultScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2652; }   // The Stonevault (was cross-wired to Cinderbrew 2661; audit B32, DB-verified: E.D.N.A. spawns on 2652)
    char const* name() const override { return "stonevault"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // High-priority kill targets. Trash adds that can wipe healer.
        // Bosses use creature entry 210108 (Edna), 210156 (Skarmorak),
        // 213216 (Dorlita), 213217 (Brokk), 213119 (Eirich) — not
        // listed here since bots tank-pull bosses via IsDungeonBoss
        // flag, not the priority list.
        a.high_priority_kill_entries = {
        };
        a.mandatory_interrupt_spells = {
            // E.D.N.A.
            424795,  // Refracting Beam — big boss damage cast
            424893,  // Stone Shield — boss self-shield, kick to prevent
            // Skarmorak
            423200,  // Fortified Shell — boss damage-reduction shield
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // E.D.N.A.
            424888,  // Seismic Smash — frontal cone damage
            424879,  // Earth Shatterer — AoE telegraph
            424908,  // Volatile Spike Missile — targeted impact zone
            424913,  // Volatile Explosion — damage zone after impact
            // Skarmorak
            422233,  // Crystalline Smash — frontal cone damage
            443494,  // Crystalline Eruption — AoE ground effect
            423324,  // Void Discharge — debuff requiring movement
            423572,  // Unstable Energy Area — ground effect
            423557,  // Unstable Fragments — projectile zones
        };
        // Boss progression — entries from TC's
        // src/server/scripts/KhazAlgar/TheStoneVault/the_stonevault.h.
        // The Master Machinists encounter is a duo (Dorlita + Brokk);
        // either entry resolves as the encounter and the tank-advance
        // rule finds whichever is closer.
        a.bosses = {
            210108,  // E.D.N.A.
            210156,  // Skarmorak
            213216,  // Speaker Dorlita (Master Machinist)
            213217,  // Speaker Brokk (Master Machinist)
            213119,  // Void Speaker Eirich (final)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 2652), in encounter order. Previous values were fabricated
        // (z ~1320 vs actual ~351-365; audit B34). Coarse route skeleton;
        // the pathfinder handles the corridors between chambers.
        a.progression_waypoints = {
            {  -36.5f,    0.0f, 361.8f },   // E.D.N.A. (210108)
            {   27.8f, -249.5f, 359.2f },   // Skarmorak (210156)
            { -241.4f,  295.2f, 351.3f },   // Speaker Dorlita (213216)
            { -230.6f,  301.4f, 351.3f },   // Speaker Brokk (213217)
            { -248.5f,   -0.1f, 365.1f },   // Void Speaker Eirich (213119)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeStonevaultScript()
{
    return std::make_unique<StonevaultScript>();
}

} // namespace Playerbot

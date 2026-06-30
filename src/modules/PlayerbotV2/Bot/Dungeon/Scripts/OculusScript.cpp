// OculusScript — The Oculus (map 578, WotLK 78-80).
// Vehicle dungeon. 4 bosses: Drakos the Interrogator, Varos
// Cloudstrider, Mage-Lord Urom, Ley-Guardian Eregos.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/Nexus/Oculus/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class OculusScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 578; }
    char const* name() const override { return "oculus"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            // Drakos
            50754,  // Summon Unstable Sphere
            51336,  // Magic Pull
            // Varos
            50804,  // Arcane Barrage
            51153,  // Arcane Volley
            57959,  // Planar Anomalies
            51162,  // Planar Shift
            51175,  // Summon Ley Whelp
            57963,  // Summon Planar Anomalies
            57976,  // Planar Blast
            // Mage-Lord Urom
            53813,  // Arcane Shield
            50476,  // Summon Menagerie
            51103,  // Frostbomb
            51602,  // Evocate
            // Eregos
            50785,  // Energize Cores
            51054,  // Call Amplify Magic
            49549,  // Ice Beam
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Varos
            51170,  // Enraged Assault
            // Urom
            58025,  // Frost Buffet
            // Eregos
            51019,  // Arcane Beam Periodic
            51017,  // Summon Arcane Beam
        };
        // Boss progression — NPC entries from TC's oculus.h.
        a.bosses = {
            27654,  // Drakos the Interrogator
            27447,  // Varos Cloudstrider
            27655,  // Mage-Lord Urom
            27656,  // Ley-Guardian Eregos (final)
        };
        // Progression waypoints — Oculus has 3 floating ring tiers.
        // Drakos on ground floor; Varos/Urom/Eregos require drake mounts.
        a.progression_waypoints = {
            { 1014.0f,  1051.0f, 359.7f },   // entry ground (Drakos)
            { 1219.0f,   936.0f, 519.0f },   // ring 1 (Varos)
            {  977.0f,  1042.0f, 359.6f },   // ring 2 (Urom)
            { 1336.0f,   929.0f, 405.8f },   // ring 3 (Eregos)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeOculusScript()
{
    return std::make_unique<OculusScript>();
}

} // namespace Playerbot

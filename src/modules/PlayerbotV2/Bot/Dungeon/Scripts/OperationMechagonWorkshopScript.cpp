// OperationMechagonWorkshopScript — Operation: Mechagon (map 2097,
// BfA 8.2 mega-dungeon, full). M+ in TWW S3 splits it into Junkyard +
// Workshop entrances; same map id, so a single script covers both halves.
// Junkyard:
//   * King Gobbamak — Smash + adds.
//   * Gunker — Sludge Slam + adds.
//   * Trixie & Naeno — twin biker fight.
//   * HK-8 Aerial Oppression Unit — Cluster Bombs + Fire Cannon.
// Workshop:
//   * Tussle Tonks — Platinum Pummeler + Gnomercy 4.U. twin mech fight.
//   * K.U.-J.0. — Venting Flames + Junkyard D.0.G.s.
//   * Machinist's Garden — Head Machinist Sparkflux.
//   * King Mechagon (final) — pilots Aerial Unit R-21/X (P1) then
//     Omega Buster (P2); Magneto-Arm + Take-Off.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class OperationMechagonWorkshopScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2097; }
    char const* name() const override { return "operation_mechagon"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            151773,  // Junkyard D.0.G. (K.U.-J.0. — fixates a player)
            150394,  // Armored Vaultbot (Junkyard — explodes on death)
            144294,  // Mechagon Tinkerer (Workshop trash — summons mechs)
        };
        a.mandatory_interrupt_spells = {
            293792,  // Fire Cannon (HK-8)
            291928,  // Magneto-Arm (Mechagon)
            295008,  // Cluster Bombs (HK-8)
            292267,  // Pulverizing Punch (Pummeler)
            295132,  // Sludge Slam (Gunker)
            299475,  // Smash (Gobbamak)
            293930,  // Heaven's Devise (Aerial)
        };
        a.cc_priority_entries = {
            151773,  // Junkyard D.0.G.
            150394,  // Armored Vaultbot
        };
        a.dangerous_auras = {
            293792,  // Fire Cannon zone
            295008,  // Cluster Bombs pool
            291928,  // Magneto-Arm pull zone
            295132,  // Sludge Slam ground
            295275,  // Venting Flames (KU-J0)
        };
        // Boss progression — Operation: Mechagon has 8 encounters across
        // Junkyard + Workshop halves. NPC entries verified against
        // world.creature_template (TC ships no Mechagon boss scripts).
        // Multi-creature encounters list every member so tank-advance
        // finds whichever is currently active: Tussle Tonks is the
        // Pummeler+Gnomercy duo, King Mechagon pilots Aerial Unit
        // R-21/X (P1) then Omega Buster (P2).
        a.bosses = {
            // Junkyard
            150159,  // King Gobbamak
            150222,  // Gunker
            150712,  // Trixie Tazer (Trixie & Naeno)
            150190,  // HK-8 Aerial Oppression Unit
            // Workshop
            144244,  // The Platinum Pummeler (Tussle Tonks)
            145185,  // Gnomercy 4.U. (Tussle Tonks)
            144246,  // K.U.-J.0.
            144248,  // Head Machinist Sparkflux (Machinist's Garden)
            150396,  // Aerial Unit R-21/X (King Mechagon P1)
            144249,  // Omega Buster (King Mechagon P2)
            154817,  // King Mechagon (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeOperationMechagonWorkshopScript()
{
    return std::make_unique<OperationMechagonWorkshopScript>();
}

} // namespace Playerbot

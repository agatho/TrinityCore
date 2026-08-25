// IsleOfConquestScript — Isle of Conquest (BattlemasterList id 30 / BATTLEGROUND_IC).
// 40v40. Capture Workshop / Hangar / Docks / Refinery / Quarry, drive
// siege vehicles to break the enemy keep gate, then kill the General
// (NPC_HIGH_COMMANDER_HALFORD_WYRMBANE 34924 or NPC_OVERLORD_AGMAR 34922
// — the literal win condition per TC battleground_isle_of_conquest.cpp:
// 386-395 OnUnitKilled).
//
// Authoritative TC sources:
//   src/server/scripts/Battlegrounds/IsleOfConquest/isle_of_conquest.h
//     :23-24 — General NPC entries
//     :27-33 — vehicle creature entries
//     :76-87 — gate-state worldstates
//   src/server/scripts/Battlegrounds/IsleOfConquest/battleground_isle_of_conquest.cpp
//     :182-191, :221-227 — node banners + keep gate entries
//     :386-395           — OnUnitKilled (win-condition)
// V1 author-attested coords (NOT TC-validated; DB-driven on TC):
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Siege/
//     IsleOfConquestData.h:128-134 (nodes), :354-364 (Generals).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class IsleOfConquestScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 30; }  // BATTLEGROUND_IC
    char const* name() const override { return "isle_of_conquest"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 40-slot raid layout. Workshop is the strategic key — it yields
        // Siege Engines, the only vehicle that meaningfully damages keep
        // gates. Defenders weighted toward it via per-node priority=3.
        //   * 16 Attackers — push Workshop → Siege Engine → enemy gate → General.
        //   * 10 Defenders — Workshop + keep gate sentinels + node holds.
        //   *  7 Roamers   — counter-flips + intercept enemy siege.
        //   *  7 Healers   — front line + rear / rez cycle.
        a.role_by_slot.assign(40, BgRole::Free);
        for (uint8_t i = 0;  i < 16; ++i) a.role_by_slot[i] = BgRole::Attacker;
        for (uint8_t i = 16; i < 26; ++i) a.role_by_slot[i] = BgRole::Defender;
        for (uint8_t i = 26; i < 33; ++i) a.role_by_slot[i] = BgRole::Roamer;
        for (uint8_t i = 33; i < 40; ++i) a.role_by_slot[i] = BgRole::Healer;
        a.auto_use_go_types = { 42, 24 };  // CAPTURE_POINT + legacy FLAGSTAND
        // IoC banners are GO type 1/10 — no type-42/24 GOs exist on map 628
        // (audit B26). DB-verified banner/goober entries (incl. node banners
        // and the Seaforium charges used to breach gates):
        a.auto_use_go_entries = {
            195130, 195132, 195133, 195144, 195145, 195149, 195150, 195151,
            195152, 195153, 195154, 195155, 195156, 195157, 195158, 195334,
            195335, 195336, 195337, 195338, 195339, 195340, 195341, 195342,
            195343, 195391, 195392, 195393, 195394, 195396, 195397, 195398,
            195399, 195237, 195332, 195333,
        };
        // Endgame target = enemy General coords. Win condition per TC
        // OnUnitKilled (battleground_isle_of_conquest.cpp:386-395). The
        // bot's nearby-enemy aggro takes over once the General is in
        // sight; the coord just routes the AllIn push to the right keep.
        //   * NPC_HIGH_COMMANDER_HALFORD_WYRMBANE 34924 — Alliance Boss
        //   * NPC_OVERLORD_AGMAR                  34922 — Horde Boss
        // Coords from V1 IsleOfConquestData.h:354-364 (author-attested,
        // not TC-validated; TC's General spawns are DB-driven).
        // Gate-aware endgame: when any enemy gate is still standing,
        // attackers push to the gate FIRST; only once all 3 gates are
        // down do they push the General. Gate coords are the REAL DB
        // gameobject spawn positions on map 628 (the V1 coords here were
        // wrong, and the front/west/east → entry mapping is NOT contiguous):
        //   Alliance Front 195698 @ (413.479,-833.95,48.524)
        //            West  195699 @ (351.615,-762.75,48.916)
        //            East  195700 @ (351.024,-903.326,48.925)
        //   Horde    Front 195494 @ (1150.90,-762.606,47.508)
        //            West  195495 @ (1217.90,-676.948,47.634)
        //            East  195496 @ (1218.74,-851.155,48.253)
        // siege_target_go_entries = the ENEMY keep gates this bot's siege
        // vehicle fires at (cast_vehicle_at the gate's live position via the
        // bg_vehicle_fire_gate rule). Any vehicle spell that lands on the
        // General then instakills it (boss SpellHit, boss_ioc_horde_alliance
        // .cpp:66-74) — the unit-fire rule handles that once gates are down.
        if (s.is_horde())
        {
            a.home_base_x = 1189.0f; a.home_base_y = -737.0f; a.home_base_z = 48.0f;
            a.siege_target_go_entries = { 195698, 195699, 195700 };  // Alliance gates
            // Once ANY Alliance gate is breached the keep is open — commit to the
            // GENERAL (real IoC: one breach gives keep access). endgame_unconditional
            // makes the coordinator issue PushEndgame so free attackers pour through
            // the breach on foot (the verified AV-style push + engage-on-LoS); without
            // it push_boss stays false in a balanced match (sd~0) and the General is
            // never targeted. Until a breach, aim at the front gate so siege vehicles
            // drive up and break it (the fire rule targets whichever gate is nearest).
            const bool any_gate_down =
                s.bg_ioc_gate_destroyed(BotSnapshot::IocGateFrontA) ||
                s.bg_ioc_gate_destroyed(BotSnapshot::IocGateWestA)  ||
                s.bg_ioc_gate_destroyed(BotSnapshot::IocGateEastA);
            if (any_gate_down)
            {
                // Push INTO the command room next to the General so foot bots
                // fight him down like players do (he's a boss with Honor Guards +
                // a combat kit + a 25y Rage leash from home — meant to be DPS'd,
                // not cheesed). His exact pedestal (225,-832,60.9) is off-mesh, but
                // the command-room FLOOR around him at z~60 IS meshed and reachable
                // via the ramp up from the breach (verified: breach->room 33 polys
                // OK). Aim ~10y from him at floor height so the engage-on-LoS rule
                // closes to melee/caster range inside the Rage leash.
                a.endgame_target_x = 235.0f; a.endgame_target_y = -832.0f;
                a.endgame_target_z = 60.0f;
                a.endgame_unconditional = true;
            }
            else
            {
                a.endgame_target_x = 413.479f; a.endgame_target_y = -833.95f;
                a.endgame_target_z = 48.524f;
            }
            a.endgame_creature_entry = 34924;  // NPC_HIGH_COMMANDER_HALFORD_WYRMBANE
        }
        else
        {
            a.home_base_x = 345.0f; a.home_base_y = -857.0f; a.home_base_z = 48.0f;
            a.siege_target_go_entries = { 195494, 195495, 195496 };  // Horde gates
            // Mirror the Alliance side: any breached Horde gate -> push the General.
            const bool any_gate_down =
                s.bg_ioc_gate_destroyed(BotSnapshot::IocGateFrontH) ||
                s.bg_ioc_gate_destroyed(BotSnapshot::IocGateWestH)  ||
                s.bg_ioc_gate_destroyed(BotSnapshot::IocGateEastH);
            if (any_gate_down)
            {
                // Push into Overlord Agmar's command room so foot bots fight him
                // down. Unlike the Alliance pedestal, his exact spot snaps to a
                // reachable poly (verified: breached-front-gate -> (1295,-765,70)
                // 31 polys OK, ramp up from the east), so target it directly.
                a.endgame_target_x = 1295.0f; a.endgame_target_y = -765.0f;
                a.endgame_target_z = 70.0f;
                a.endgame_unconditional = true;
            }
            else
            {
                a.endgame_target_x = 1150.90f; a.endgame_target_y = -762.606f;
                a.endgame_target_z = 47.508f;
            }
            a.endgame_creature_entry = 34922;  // NPC_OVERLORD_AGMAR
        }
        // 7 capturable nodes, prioritised by strategic value. Coords are
        // the banner-GO spawn positions from the world DB on map 628
        // (B26b: the previous V1-attributed coords were corrupted — the
        // Docks/Hangar/Quarry Y values pointed at the wrong ends of the
        // island, e.g. "Hangar" at y=-123 vs the real banner at y=-1000).
        // These MUST stay in sync with the IOC_WS_NODES worldstate-harvest
        // table in BotSnapshotBuilder.cpp — consumers cross-reference
        // node_states against advice nodes within 5 yards.
        //   * Workshop  (p=3) — yields Siege Engines (34776/35069),
        //                       the only effective gate-breakers.
        //   * Hangar    (p=2) — gunship cannons for gate strafing.
        //   * Docks     (p=2) — Glaive Throwers + Catapults.
        //   * Refinery  (p=1) — passive honor (spell 68719).
        //   * Quarry    (p=1) — passive honor (spell 68720).
        //   * Keep GYs  (p=1) — own = late-game defense anchor; enemy
        //                       becomes cappable once its gates fall.
        a.nodes = {
            {  776.23f,  -804.28f,   6.45f, "Workshop",                3 },
            {  807.78f, -1000.07f, 132.38f, "Hangar",                  2 },
            {  726.39f,  -360.21f,  17.82f, "Docks",                   2 },
            { 1269.50f,  -400.81f,  37.63f, "Refinery",                1 },
            {  251.02f, -1159.32f,  17.24f, "Quarry",                  1 },
            {  299.15f,  -784.59f,  48.92f, "Alliance Keep Graveyard", 1 },
            { 1284.76f,  -705.67f,  48.92f, "Horde Keep Graveyard",    1 },
        };
        // Vehicle entries — ALL confirmed against TC isle_of_conquest.h:27-33.
        //   34775 Demolisher           (NPC_DEMOLISHER)
        //   34776 Siege Engine A       (NPC_SIEGE_ENGINE_A)
        //   35069 Siege Engine H       (NPC_SIEGE_ENGINE_H)
        //   34802 Glaive Thrower A     (NPC_GLAIVE_THROWER_A)
        //   35273 Glaive Thrower H     (NPC_GLAIVE_THROWER_H)
        //   34793 Catapult             (NPC_CATAPULT)
        //   34944 Keep Cannon          (NPC_KEEP_CANNON — defender side)
        // NOTE: 34944 Keep Cannon DELIBERATELY EXCLUDED from the auto-mount
        // list. It is a STATIONARY defensive turret bolted to the keep wall —
        // it cannot drive. Live [ioc] diag showed Workshop-attackers spawning
        // at the keep mounting the nearest vehicle (always a keep cannon, ~17y
        // away) and then sitting in idle:bg_vehicle_drive_to_gate forever (1756
        // mounts, 0 of them a real Siege Engine), starving the Workshop assault
        // and the gate breach. Mountable list is now offensive vehicles only;
        // Siege Engines (34776/35069) come from capturing the Workshop.
        a.vehicle_creature_entries = { 34775, 34776, 35069, 34802, 35273, 34793 };
        // Per-entry seat-0 PRIMARY spells, sourced from the live world DB
        // creature_template_spell action bars (index 0 = the vehicle's
        // primary attack). These supersede the old 50652/66809/65775 guesses,
        // none of which are this server's IoC vehicle bars:
        //   34775 Demolisher        → 67440 (boulder, gate-breaker)
        //   34776 Siege Engine A    → 67796 (ram, the prime gate-breaker)
        //   35069 Siege Engine H    → 67796
        //   34802 Glaive Thrower A  → 66456
        //   35273 Glaive Thrower H  → 67034
        //   34944 Keep Cannon       → 67452 (defender turret)
        //   34793 Catapult          → 66296 (anti-personnel; its index-0
        //                             66218 is the player-LAUNCH utility, not
        //                             a weapon — the Catapult is NOT a gate-
        //                             breaker so it's excluded from sieging).
        a.vehicle_seat_spell_by_entry = {
            { 34775u, 67440u }, { 34776u, 67796u }, { 35069u, 67796u },
            { 34802u, 66456u }, { 35273u, 67034u }, { 34944u, 67452u },
            { 34793u, 66296u },
        };
        // Fallback for any unmapped vehicle: the Siege Engine ram (also the
        // gate-breaker) rather than a bogus id; server drops a mismatched
        // cast harmlessly.
        a.vehicle_seat_spell = 67796;
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeIsleOfConquestScript()
{
    return std::make_unique<IsleOfConquestScript>();
}

} // namespace Playerbot

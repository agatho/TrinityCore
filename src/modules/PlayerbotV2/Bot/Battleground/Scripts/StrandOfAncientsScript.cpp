// StrandOfAncientsScript — Strand of the Ancients
// (BattlemasterList id 9 / BATTLEGROUND_SA, map 607). Attack/defense
// alternation: attackers drive Demolishers + ram south wall, then the
// inner gates, then cap the Titan Relic. Defenders camp gates + relic.
//
// Round-aware: snapshot exposes BG_SA_ATTACKER_TEAM (worldstate 3690,
// TC battleground_strand_of_the_ancients.cpp:166). The script swaps
// role mix AND endgame target / home base by phase — on the defender
// round, the script's Attacker-role bots would otherwise march to their
// OWN relic when AllIn-biased.
//
// Authoritative TC sources:
//   src/server/scripts/Battlegrounds/StrandOfTheAncients/battleground_strand_of_the_ancients.cpp
//     :74-82  — gate / Titan Relic GO entries
//     :141-146 — gate-state worldstates (3614/3617/3620/3623/3638/3849)
//     :166    — BG_SA_ATTACKER_TEAM worldstate
//     :250-251 — Demolisher + Antipersonnel Turret entries
// V1 author-attested coords (not TC-validated; DB-driven):
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Siege/
//     StrandOfTheAncientsData.h:186-188 (relic), :238-245 (gates),
//     :251-257 (graveyards), :263-268 (demolisher mount points).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class StrandOfAncientsScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 9; }  // BATTLEGROUND_SA
    char const* name() const override { return "strand_of_ancients"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // sota_attacker_team: 0=Alliance attacks, 1=Horde attacks, -1=N/A.
        const int8 atk = s.bg_sota_attacker_team();
        const bool my_team_attacks =
            atk >= 0 && ((s.is_horde() && atk == 1) || (!s.is_horde() && atk == 0));

        if (my_team_attacks)
        {
            // Attacker round: 15-slot offense-heavy mix. Vehicle pilots
            // (demolishers via vehicle_creature_entries auto-mount),
            // foot infantry, healers riding the push.
            a.role_by_slot = {
                BgRole::Attacker, BgRole::Healer,   BgRole::Attacker,
                BgRole::Attacker, BgRole::Roamer,   BgRole::Attacker,
                BgRole::Attacker, BgRole::Attacker, BgRole::Healer,
                BgRole::Roamer,   BgRole::Attacker, BgRole::Attacker,
                BgRole::Roamer,   BgRole::Attacker, BgRole::Healer,
            };
        }
        else
        {
            // Defender round: heavy relic camp. Roamers intercept
            // vehicles + skirmish; defenders cover gates and relic.
            a.role_by_slot = {
                BgRole::Defender, BgRole::Healer,   BgRole::Defender,
                BgRole::Roamer,   BgRole::Defender, BgRole::Defender,
                BgRole::Roamer,   BgRole::Defender, BgRole::Healer,
                BgRole::Defender, BgRole::Defender, BgRole::Roamer,
                BgRole::Defender, BgRole::Defender, BgRole::Healer,
            };
        }

        // Demolishers (NPC_DEMOLISHER = 28781, TC :250). Seat-0 ram/boulder
        // is spell 50652 (verified the long-standing SoTA demolisher fire
        // spell). The bg_vehicle_fire_gate rule casts it AT the gate position.
        a.vehicle_creature_entries = { 28781 };
        a.vehicle_seat_spell       = 50652;
        a.auto_use_go_types        = { 42, 24 };  // CAPTURE_POINT + FLAGSTAND
        // SoTA banners + the TITAN RELIC (the win condition!) are GO type
        // 1/10 — no type-42/24 GOs exist on map 607 (audit B26). Without
        // these entries, attackers could breach every gate yet never cap.
        // Relic entries 194082 (Horde-attacker) / 194083 (Alliance-attacker)
        // spawn at (837.065,-107.537,127.025); they become interactable only
        // after the Ancient (Last) gate falls (MakeObjectsInteractable).
        a.auto_use_go_entries = {
            191305, 191306, 191307, 191308, 191309, 191310,
            194082, 194083,   // Titan Relic (A/H) — the win object
        };

        // 5 graveyards flipping inward as attackers push. Priority on
        // intermediate GYs biases the Attacker rule toward the next
        // unflipped tier without needing per-tick gate-state telemetry.
        a.nodes = {
            { 1597.0f, -106.0f,  8.0f, "Beach GY",           0 },
            { 1338.0f, -298.0f, 32.0f, "West GY",            1 },  // post-tier-1
            { 1338.0f,  245.0f, 32.0f, "East GY",            1 },  // post-tier-1
            { 1119.0f,  -24.0f, 67.0f, "South GY",           2 },  // post-tier-2
            {  830.0f,  -24.0f, 93.0f, "Defender Start GY",  0 },
        };

        // Round-aware home / endgame swap. Without this, the defender-
        // round Attacker-role bots walk to their OWN relic on AllIn bias.
        if (my_team_attacks)
        {
            // ENEMY gates this team's demolishers fire at (real DB spawn
            // positions on map 607). Listed across all defense lines; the
            // bg_vehicle_fire_gate rule picks the nearest STANDING one, and
            // only the current line's gate is reachable (later lines are
            // collision-walled until the earlier line falls). Entries:
            //   First : Green 190722 (1412.74,106.993,29.878),
            //           Blue  190724 (1431.13,-218.684,32.105)
            //   Second: Red   190726 (1229.67,-211.30,56.436),
            //           Purple190723 (1214.78,81.561,54.582)
            //   Third : Yellow190727 (1055.90,-107.628,83.428)
            //   Last  : Ancient192549(878.033,-108.191,117.832)
            a.siege_target_go_entries =
                { 190722, 190724, 190726, 190723, 190727, 192549 };
            // On-foot fallback: SEAFORIUM bomb pickup GO (190753 Alliance /
            // 194086 Horde — type-22 SPELLCASTER granting charge aura 52415).
            // Footmen grab a charge then carry it to a gate. (Add the pickup
            // GO so auto-use grants the charge; the place-cast 52410 is the
            // demolisher's backup, not modeled per-bot yet.)
            a.auto_use_go_entries.push_back(s.is_horde() ? 194086u : 190753u);

            // SoTA scores 0/0 all match (no AddPoint), so the score bias
            // never leaves Normal — drive attackers to the breach/relic
            // UNCONDITIONALLY (BG audit SoTA blocker), not just under AllIn.
            a.endgame_unconditional = true;

            // Gate-state-aware tier targeting → aim endgame at the specific
            // EARLIEST STANDING gate's real position (so foot attackers and
            // the demolisher drive-to-gate fallback both converge there).
            // bg_sota_gate_state: DESTROYED encodes as 3 (Horde-attacker) OR
            // 6 (Alliance-attacker); 0 = unknown → treat as still up.
            auto gate_down = [&](BotSnapshot::SotaGateId g) -> bool {
                const uint32 st = s.bg_sota_gate_state(g);
                return st == 3u || st == 6u;
            };
            float gx, gy, gz;
            if (!gate_down(BotSnapshot::SotaGateGreen))
            { gx = 1412.74f; gy =  106.993f; gz =  29.878f; }
            else if (!gate_down(BotSnapshot::SotaGateBlue))
            { gx = 1431.13f; gy = -218.684f; gz =  32.105f; }
            else if (!gate_down(BotSnapshot::SotaGateRed))
            { gx = 1229.67f; gy = -211.30f;  gz =  56.436f; }
            else if (!gate_down(BotSnapshot::SotaGatePurple))
            { gx = 1214.78f; gy =   81.561f; gz =  54.582f; }
            else if (!gate_down(BotSnapshot::SotaGateYellow))
            { gx = 1055.90f; gy = -107.628f; gz =  83.428f; }
            else
            { gx =  878.033f; gy = -108.191f; gz = 117.832f; }  // Ancient Gate / relic chamber
            a.endgame_target_x = gx;
            a.endgame_target_y = gy;
            a.endgame_target_z = gz;
            // Home = beach landing.
            a.home_base_x = 1597.0f;
            a.home_base_y = -106.0f;
            a.home_base_z =    8.0f;
        }
        else
        {
            // Defenders camp the Titan Relic (real spawn 837.065,-107.537,
            // 127.025 — the V1 836,-24,94 centerline was off by ~80y in Y
            // and ~30y in Z).
            a.endgame_target_x = 837.065f;
            a.endgame_target_y = -107.537f;
            a.endgame_target_z = 127.025f;
            a.home_base_x = 837.065f;
            a.home_base_y = -107.537f;
            a.home_base_z = 127.025f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeStrandOfAncientsScript()
{
    return std::make_unique<StrandOfAncientsScript>();
}

} // namespace Playerbot

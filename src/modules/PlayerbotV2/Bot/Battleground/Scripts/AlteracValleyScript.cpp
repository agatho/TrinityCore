// AlteracValleyScript — Alterac Valley (BattlemasterList id 1 / BATTLEGROUND_AV, map 30).
// 40v40 reinforcement push. Decided by towers / captains / end-boss, not
// zerg: each tower destroyed drains 75 reinforcements; each captain kill
// gives +100 reinforcements to the killing team; killing Drek'thar /
// Vandar Stormpike ends the BG.
//
// Authoritative data from V1 (production-tested):
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Siege/
//     AlteracValleyData.h:35 (REINF_GAIN_PER_CAPTAIN), :128-130 (Vandar),
//     :134-136 (Drek'Thar), :149-164 (Captains), :191+ (towers),
//     :451-457 (spawns).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class AlteracValleyScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 1; }  // BATTLEGROUND_AV
    char const* name() const override { return "alterac_valley"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 40 slots. AV is decided by reinforcement bleed (kills + tower
        // destroys + captain kills) — not zerg. Real AV runs ~6-8
        // healers per side; 8 healer slots ensure statistical coverage
        // even after class-fit fallback (a non-healer-spec bot in a
        // Healer slot demotes to Roamer via the class-fixup rule).
        //   * 18 Attackers — front-line push.
        //   * 6  Defenders — home keep + forward GY anchor + tower defense.
        //   * 8  Roamers   — counter-flips + tower defense rotation.
        //   * 8  Healers   — split across the line.
        a.role_by_slot.assign(40, BgRole::Free);
        for (uint8_t i = 0;  i < 18; ++i) a.role_by_slot[i] = BgRole::Attacker;
        for (uint8_t i = 18; i < 24; ++i) a.role_by_slot[i] = BgRole::Defender;
        for (uint8_t i = 24; i < 32; ++i) a.role_by_slot[i] = BgRole::Roamer;
        for (uint8_t i = 32; i < 40; ++i) a.role_by_slot[i] = BgRole::Healer;
        // AV banners are GO type 1 (BUTTON) / type 10 (GOOBER) — there are
        // NO type-42/24 GOs on map 30 (audit B26), so node interaction was
        // impossible. Enumerate the DB-verified banner entries instead
        // (every per-state Alliance/Horde/Contested variant on map 30).
        a.auto_use_go_entries = {
            178364, 178365, 178388, 178389, 178393, 178394, 178925, 178927,
            178929, 178932, 178935, 178936, 178940, 178943, 178944, 178945,
            178946, 178947, 178948, 178955, 178956, 178957, 178958, 179284,
            179285, 179286, 179287, 179304, 179305, 179308, 179310, 179435,
            179436, 179439, 179440, 179441, 179442, 179443, 179444, 179445,
            179446, 179449, 179450, 179453, 179454, 179458, 179465, 179466,
            179467, 179468, 179470, 179471, 179472, 179473, 179481, 179482,
            179483, 179484, 180418, 180419, 180420,
        };

        // Endgame targets + node table rebuilt from DB-verified spawns
        // (audit B27: the old V1-attributed coords were corrupted — the
        // Horde "Vanndar" target was DREK'THAR'S OWN ROOM, Alliance
        // structures sat inside the Horde base, and both captains were
        // ~120-230y off; ~70% of objective movement was mis-targeted).
        // world.creature: Vanndar 11948 (722.4,-11.0,50.7); Drek'Thar
        // 11946 (-1370.9,-220.2,98.5); Balinda 11949 (-57.8,-286.6,15.6);
        // Galvangar 11947 (-545.2,-165.4,57.8). Node coords = banner GO
        // spawn clusters on map 30.
        if (s.is_horde())
        {
            // Horde spawn — Frostwolf Stadium (V1 HORDE_SPAWNS[0]).
            a.home_base_x = -1437.00f; a.home_base_y = -610.00f; a.home_base_z = 51.16f;
            // Endgame: Vanndar Stormpike in Dun Baldar (NORTH end, +X).
            a.endgame_target_x = 722.4f;
            a.endgame_target_y = -11.0f;
            a.endgame_target_z =  50.7f;
            a.endgame_creature_entry = 11948;  // VANNDAR_ENTRY
        }
        else
        {
            // Alliance spawn (V1 ALLIANCE_SPAWNS[0]).
            a.home_base_x = 873.98f; a.home_base_y = -491.79f; a.home_base_z = 96.54f;
            // Endgame: Drek'Thar in Frostwolf Keep (SOUTH end, -X).
            a.endgame_target_x = -1370.9f;
            a.endgame_target_y =  -220.2f;
            a.endgame_target_z =    98.5f;
            a.endgame_creature_entry = 11946;  // DREKTHAR_ENTRY
        }
        // Non-deficit endgame trigger (BG audit AV). The generic endgame
        // redirect only fires under Bias_AllIn (own reinforcements >=200
        // behind), so a LEADING or TIED team never boss-rushed and AV dragged
        // to the reinforcement floor. AV reinforcements map into the bg score;
        // when the ENEMY drops below ~200 of 600 its towers are mostly down
        // (-75 each) so its general has shed its tower buffs and is killable —
        // push the boss to SEAL the win instead of trickling reinforcements.
        // (>0 guard avoids firing on an un-harvested 0 before the match warms.)
        {
            const uint32 enemy_reinf =
                s.is_horde() ? s.bg_score_alliance() : s.bg_score_horde();
            if (enemy_reinf > 0 && enemy_reinf < 200)
                a.endgame_unconditional = true;
        }
        // 15 static nodes: 7 GYs (p=0) + 8 towers/bunkers (p=2, -75 reinf
        // drain each); captains appended below (p=3, +100 reinf for the
        // killer). All coords = banner-GO spawn positions from world DB.
        a.nodes = {
            // Graveyards — priority 0 (rez logistics only).
            {   669.0f, -294.0f,  30.0f, "Stormpike GY",            0 },
            {   638.5f,  -31.5f,  46.0f, "Stormpike Aid Station",   0 },
            {    73.7f, -426.0f,  61.0f, "Stonehearth GY",          0 },
            {  -202.5f, -113.0f,  78.0f, "Snowfall GY (neutral)",   0 },
            {  -612.5f, -397.0f,  61.0f, "Iceblood GY",             0 },
            { -1082.5f, -347.0f,  55.0f, "Frostwolf GY",            0 },
            { -1402.0f, -307.0f,  89.0f, "Frostwolf Relief Hut",    0 },
            // Alliance bunkers — priority 2 (-75 reinf on destruction).
            {   556.0f,  -84.0f,  52.0f, "Dun Baldar North Bunker", 2 },
            {   678.0f, -139.0f,  64.0f, "Dun Baldar South Bunker", 2 },
            {   203.0f, -361.0f,  56.0f, "Icewing Bunker",          2 },
            {  -154.0f, -446.0f,  45.0f, "Stonehearth Bunker",      2 },
            // Horde towers — priority 2.
            // Labels corrected (BG audit §2): the DB captain garrisons prove
            // the names were reversed — Commander Dardosh (13140) garrisons
            // the (-572,-263) tower = ICEBLOOD; Wing Commander Slidore (13438)
            // garrisons the (-768,-363) tower = TOWER POINT.
            {  -572.0f, -263.0f,  75.0f, "Iceblood Tower",          2 },
            {  -768.0f, -363.0f,  91.0f, "Tower Point",             2 },
            { -1303.0f, -317.0f, 114.0f, "East Frostwolf Tower",    2 },
            { -1298.0f, -267.0f, 114.0f, "West Frostwolf Tower",    2 },
        };
        // Captains — priority 3 (+100 reinforcements for the killing
        // team). Permanently-killable: drop from the node list once the
        // snapshot reports the captain dead so bots stop pathing to a
        // corpse. Coords = actual creature spawns (the old values
        // duplicated bunker/GY coords ~230y away).
        if (s.bg_av_balinda_alive())
            a.nodes.push_back({  -57.8f, -286.6f, 15.6f, "Balinda Stonecaster (A)", 3 });
        if (s.bg_av_galvangar_alive())
            a.nodes.push_back({ -545.2f, -165.4f, 57.8f, "Captain Galvangar (H)", 3 });
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeAlteracValleyScript()
{
    return std::make_unique<AlteracValleyScript>();
}

} // namespace Playerbot

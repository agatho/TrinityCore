// ArenaScript — generic 2v2/3v3 arena handler.
// Different from BG: no objectives, no FlagCarrier, no nodes. Pure
// deathmatch. Bot strategy: stick tight to team (3-bot pack),
// focus enemy healer, use offensive cooldowns aggressively, kite
// around pillars when low HP.
//
// Per-map pillar / hazard data is not modelled here — see
// BATTLEGROUND_PLAN arena notes. All registered arenas share this
// generic logic via the ArenaScript class parameterised by id+name.
//
// Registered arenas (BattlemasterList id → human name):
//   classic / TBC / WotLK:
//     4   Nagrand Arena
//     5   Blade's Edge Arena
//     6   All Arenas (skirmish queue)
//     8   Ruins of Lordaeron
//    10   Dalaran Sewers
//    11   Ring of Valor
//   MoP/WoD:
//    719  Tol'Viron Arena
//    757  The Tiger's Peak
//   Legion:
//    808  Black Rook Hold Arena
//    816  Ashamane's Fall
//   "v2" rebrands (Brawl / Solo Shuffle re-ids on classic maps):
//    868  Ruins of Lordaeron 2
//    869  Dalaran Sewers 2
//    870  Tol'Viron 2
//    871  Tiger's Peak 2
//    872  Black Rook Hold Arena 2
//    873  Nagrand Arena 2
//    874  Ashamane's Fall 2
//    875  Blade's Edge Arena 2
//   BfA/SL/DF:
//    897  Hook Point
//    902  Tiger's Peak 3
//    903  Mugambala
//    906  Ashamane's Fall 3
//    907  Blade's Edge Arena 3
//    908  Blade's Edge (v2 mesh)
//    909  Dalaran Sewers 3
//    910  Nagrand Arena 3
//    911  Ruins of Lordaeron 3
//    912  Tol'Viron Arena 3
//    913  Black Rook Hold Arena 3
//   1025  The Robodrome
//   1041  Empyrean Domain
//
// One ArenaScript instance per id (the manager is keyed by id; the
// behavior is identical so we register the same shared logic).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

// Per-map arena geometry. Coords are APPROXIMATE from map geometry /
// V1 module history — TC stores arena spawn/object data in DB (gameobject
// / creature tables), not source, so there's no file:line citation for
// these. Anyone tightening a particular arena should sniff coords in-game
// and update the table.
struct ArenaGeometry {
    std::vector<BattlegroundAdvice::ArenaPillar>  pillars;
    std::vector<BattlegroundAdvice::ArenaHazard>  hazards;
    float rally_x = 0.f, rally_y = 0.f, rally_z = 0.f;
};
static ArenaGeometry const& GeometryFor(uint16_t id)
{
    static const ArenaGeometry empty;
    // Ring of Valor (id 11) — 4 pillar elevators rise at ~60s. Each
    // pillar footprint is roughly 5y radius. Bots that stand on one
    // get dismounted to its top and become easy ranged targets. Center
    // pad is the safe waiting spot.
    if (id == 11) {
        static const ArenaGeometry rov = {
            /*pillars*/ {
                { 763.0f, -284.0f, 28.3f, "center_pad", 0 },
            },
            /*hazards*/ {
                { 753.0f, -294.0f, 28.3f, 5.5f, "rov_pillar_sw", 60000u, 0u },
                { 753.0f, -274.0f, 28.3f, 5.5f, "rov_pillar_nw", 60000u, 0u },
                { 773.0f, -274.0f, 28.3f, 5.5f, "rov_pillar_ne", 60000u, 0u },
                { 773.0f, -294.0f, 28.3f, 5.5f, "rov_pillar_se", 60000u, 0u },
            },
            /*rally*/ 763.0f, -284.0f, 28.3f
        };
        return rov;
    }
    // Dalaran Sewers (id 10) — two waterfalls along long axis push
    // players off ledges. Avoid the falling-water zones.
    if (id == 10) {
        static const ArenaGeometry ds = {
            /*pillars*/ {
                { 1262.0f, 778.0f, 7.5f, "alliance_crates", 0 },
                { 1320.0f, 802.0f, 7.5f, "horde_crates",    0 },
            },
            /*hazards*/ {
                { 1316.6f, 816.1f, 7.5f, 6.0f, "waterfall_north", 0u, 0u },
                { 1268.6f, 749.3f, 7.5f, 6.0f, "waterfall_south", 0u, 0u },
            },
            /*rally*/ 1292.0f, 790.0f, 7.5f
        };
        return ds;
    }
    // Nagrand (id 4) — dual cubbies flanking center. No hazards.
    if (id == 4 || id == 873 || id == 910) {
        static const ArenaGeometry na = {
            /*pillars*/ {
                { 4055.0f, 2921.0f, 13.0f, "center",         0 },
                { 4024.0f, 2921.0f, 13.0f, "alliance_cubby", 1 },
                { 4086.0f, 2921.0f, 13.0f, "horde_cubby",    1 },
            },
            /*hazards*/ {},
            /*rally*/ 4055.0f, 2921.0f, 13.0f
        };
        return na;
    }
    // Ruins of Lordaeron (id 8) — center pillar + side cubbies.
    if (id == 8 || id == 868 || id == 911) {
        static const ArenaGeometry rl = {
            /*pillars*/ {
                { 1290.0f, 1585.0f, 32.0f, "center_pillar",  0 },
                { 1268.0f, 1605.0f, 32.0f, "alliance_cubby", 1 },
                { 1311.0f, 1565.0f, 32.0f, "horde_cubby",    1 },
            },
            /*hazards*/ {},
            /*rally*/ 1290.0f, 1585.0f, 32.0f
        };
        return rl;
    }
    // Blade's Edge (id 5) — bridge box high-ground breaks LoS both ways.
    if (id == 5 || id == 875 || id == 907 || id == 908) {
        static const ArenaGeometry be = {
            /*pillars*/ {
                { 6238.0f, 263.0f, 9.5f, "bridge_top", 2 },
                { 6238.0f, 263.0f, 4.0f, "bridge_under_center", 0 },
            },
            /*hazards*/ {},
            /*rally*/ 6238.0f, 263.0f, 4.0f
        };
        return be;
    }
    return empty;
}

class ArenaScript final : public BattlegroundScript
{
public:
    ArenaScript(uint16_t id, char const* nm) : id_(id), name_(nm) {}
    uint16_t bg_type_id() const override { return id_; }
    char const* name() const override { return name_; }

    BattlegroundAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        BattlegroundAdvice a;
        // 5-slot layout (max 3v3 bracket; wraps modulo for 2v2). All
        // offense / healer mix; no FC role since arenas have no flag.
        // Roamer drives the focus_fire / focus_healer / kite / cast-fake
        // rules in State_Idle without an objective-pull from node logic.
        a.role_by_slot = {
            BgRole::Roamer,
            BgRole::Healer,
            BgRole::Roamer,
            BgRole::Attacker,
            BgRole::Attacker,
        };
        // Tight escort — healer shadows the squad.
        a.escort_friendly_carrier = false;  // no carrier
        a.chase_enemy_carrier = false;
        // Per-map pillars / hazards / rally. The `idle:arena_position`
        // rule consumes these — bots avoid known dangerous footprints
        // (RoV pillar elevators, Dalaran waterfalls) and move to a
        // sensible advance point when the start gate opens. Maps with
        // no geometry registered fall back to the prior no-op behavior.
        ArenaGeometry const& geo = GeometryFor(id_);
        a.arena_pillars     = geo.pillars;
        a.arena_hazards     = geo.hazards;
        a.opening_rally_x   = geo.rally_x;
        a.opening_rally_y   = geo.rally_y;
        a.opening_rally_z   = geo.rally_z;
        return a;
    }

private:
    uint16_t id_;
    char const* name_;
};

} // anonymous

// Each arena map gets its own ArenaScript instance — the manager is
// keyed by bg_type_id so we need distinct instances even though the
// logic is identical. Helper macro keeps the registration list short.
#define ARENA_FACTORY(suffix, ID, NAME)                                \
    std::unique_ptr<BattlegroundScript> Make##suffix##ArenaScript()    \
    { return std::make_unique<ArenaScript>(uint16_t(ID), NAME); }

// Classic / TBC / WotLK arenas (BattlemasterList 4..11).
ARENA_FACTORY(Nagrand,           4,    "arena_nagrand")
ARENA_FACTORY(BladesEdge,        5,    "arena_blades_edge")
ARENA_FACTORY(AllArenas,         6,    "arena_all_skirmish")
ARENA_FACTORY(RuinsOfLordaeron,  8,    "arena_ruins_of_lordaeron")
ARENA_FACTORY(DalaranSewers,     10,   "arena_dalaran_sewers")
ARENA_FACTORY(RingOfValor,       11,   "arena_ring_of_valor")
// MoP / WoD arenas.
ARENA_FACTORY(TolViron,          719,  "arena_tolviron")
ARENA_FACTORY(TigersPeak,        757,  "arena_tigers_peak")
// Legion arenas.
ARENA_FACTORY(BlackRookHold,     808,  "arena_black_rook_hold")
ARENA_FACTORY(AshamanesFall,     816,  "arena_ashamanes_fall")
// "v2" rebrands (Brawl / Solo Shuffle re-ids on classic / Legion maps).
ARENA_FACTORY(RuinsOfLordaeron2, 868,  "arena_ruins_of_lordaeron_2")
ARENA_FACTORY(DalaranSewers2,    869,  "arena_dalaran_sewers_2")
ARENA_FACTORY(TolViron2,         870,  "arena_tolviron_2")
ARENA_FACTORY(TigersPeak2,       871,  "arena_tigers_peak_2")
ARENA_FACTORY(BlackRookHold2,    872,  "arena_black_rook_hold_2")
ARENA_FACTORY(Nagrand2,          873,  "arena_nagrand_2")
ARENA_FACTORY(AshamanesFall2,    874,  "arena_ashamanes_fall_2")
ARENA_FACTORY(BladesEdge2,       875,  "arena_blades_edge_2")
// BfA / Shadowlands / Dragonflight arenas.
ARENA_FACTORY(HookPoint,         897,  "arena_hook_point")
ARENA_FACTORY(TigersPeak3,       902,  "arena_tigers_peak_3")
ARENA_FACTORY(Mugambala,         903,  "arena_mugambala")
ARENA_FACTORY(AshamanesFall3,    906,  "arena_ashamanes_fall_3")
ARENA_FACTORY(BladesEdge3,       907,  "arena_blades_edge_3")
ARENA_FACTORY(BladesEdgeV2Mesh,  908,  "arena_blades_edge_v2_mesh")
ARENA_FACTORY(DalaranSewers3,    909,  "arena_dalaran_sewers_3")
ARENA_FACTORY(Nagrand3,          910,  "arena_nagrand_3")
ARENA_FACTORY(RuinsOfLordaeron3, 911,  "arena_ruins_of_lordaeron_3")
ARENA_FACTORY(TolViron3,         912,  "arena_tolviron_3")
ARENA_FACTORY(BlackRookHold3,    913,  "arena_black_rook_hold_3")
ARENA_FACTORY(Robodrome,         1025, "arena_robodrome")
ARENA_FACTORY(EmpyreanDomain,    1041, "arena_empyrean_domain")

#undef ARENA_FACTORY

} // namespace Playerbot

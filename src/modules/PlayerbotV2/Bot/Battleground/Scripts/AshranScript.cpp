// AshranScript — Ashran (BattlemasterList id 1020 / BATTLEGROUND_EB_A,
// map 1191). WoD 100v100 epic / world-PvP zone. Multi-front: 3 Road of
// Glory control points + 8 side events + faction commanders.
//
// Authoritative coords from V1
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Epic/
//     AshranData.h:118 (event interval), :138-149 (Volrath/Tremblade),
//     :156-160 (control points), :171-173 (faction spawns).
//
// Known schema limit: the 8 side events rotate on a 5-min interval and
// are only "hot" while live — static node priority can't reflect that.
// Until a snapshot field exposes the live event id, events are listed
// at priority=1 as a tactical backdrop; control points (p=2) and the
// faction commanders (p=3) drive the primary push.
//
// TODO: Ashran event awareness. This branch's TC tree has no
// per-map Ashran source (no src/server/scripts/Battlegrounds/Ashran/
// or Zones/BattlegroundAshran.cpp); the WoD Ashran implementation is
// stub-grade. To make events truly hot the snapshot would need a
// `bg.ashran_active_event` (uint8) field populated from a worldstate
// — but the TC implementation doesn't currently emit one. Realistic
// path: extend TC's Ashran (or wait for upstream) to expose an active-
// event worldstate, then mirror the SoTA gate-state pattern here
// (BotSnapshot::bg field + builder population + advice-cache key).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class AshranScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 1020; }  // BATTLEGROUND_EB_A
    char const* name() const override { return "ashran"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 100-slot raid. The framework wraps role_by_slot mod size, so
        // a 20-entry table over 100 slots yields:
        //   ~50 Attacker / 15 Defender / 15 Roamer / 20 Healer.
        // Healer share ~18% mirrors live Ashran raid comp (the prior
        // 10-entry table produced only ~10 healers across the whole
        // 100-slot raid — too thin for a sustained zone grind).
        a.role_by_slot = {
            BgRole::Attacker, BgRole::Attacker, BgRole::Attacker, BgRole::Healer,
            BgRole::Attacker, BgRole::Roamer,   BgRole::Healer,   BgRole::Attacker,
            BgRole::Defender, BgRole::Healer,   BgRole::Attacker, BgRole::Attacker,
            BgRole::Roamer,   BgRole::Healer,   BgRole::Attacker, BgRole::Defender,
            BgRole::Attacker, BgRole::Roamer,   BgRole::Defender, BgRole::Attacker,
        };
        a.auto_use_go_types = { 42, 24 };

        if (s.is_horde())
        {
            // Warspear (Horde spawn) — V1 HORDE_SPAWN_X/Y/Z.
            a.home_base_x = 3970.0f; a.home_base_y = -4100.0f; a.home_base_z = 55.0f;
            // Endgame: Grand Marshal Tremblade (entry 82877) — V1 :147-149.
            a.endgame_target_x = 5178.0f;
            a.endgame_target_y = -4117.0f;
            a.endgame_target_z =    1.0f;
            a.endgame_creature_entry = 82877;  // Grand Marshal Tremblade
        }
        else
        {
            // Stormshield (Alliance spawn) — V1 ALLIANCE_SPAWN_X/Y/Z.
            a.home_base_x = 5200.0f; a.home_base_y = -4100.0f; a.home_base_z = 1.0f;
            // Endgame: High Warlord Volrath (entry 82882) — V1 :142-144.
            a.endgame_target_x = 4001.0f;
            a.endgame_target_y = -4088.0f;
            a.endgame_target_z =   52.0f;
            a.endgame_creature_entry = 82882;  // High Warlord Volrath
        }
        // 13 nodes: 3 control points (p=2 — capture flips Road of Glory
        // progress) + 8 side-event centers (p=1 — only hot when an
        // event is live; static priority is the best we can do without
        // event-aware snapshot) + 2 faction commanders (p=3 — endgame).
        a.nodes = {
            // Control points — Road of Glory (V1 :156-160).
            { 4982.0f, -4171.0f, 15.0f, "Stormshield Stronghold (A)", 2 },
            { 4585.0f, -4117.0f, 32.0f, "Crossroads (center)",        2 },
            { 4188.0f, -4063.0f, 50.0f, "Warspear Stronghold (H)",    2 },
            // Side-event centers — V1 EventPositions::*_CENTER. Coords
            // approximate from V1 data; events rotate every 5 min.
            { 4700.0f, -4250.0f, 28.0f, "Race for Supremacy",         1 },
            { 4750.0f, -4300.0f, 25.0f, "Ring of Conquest",           1 },
            { 4400.0f, -4250.0f, 35.0f, "Seat of Omen (boss)",        1 },
            { 4650.0f, -3950.0f, 30.0f, "Empowered Ore",              1 },
            { 4500.0f, -4000.0f, 34.0f, "Ancient Artifact",           1 },
            { 4800.0f, -4350.0f, 22.0f, "Stadium Racing",             1 },
            { 4350.0f, -3900.0f, 42.0f, "Ogre Fires",                 1 },
            { 4200.0f, -4200.0f, 48.0f, "Brute Assault",              1 },
            // Faction leaders — V1 VOLRATH_*/TREMBLADE_*.
            { 5178.0f, -4117.0f,  1.0f, "Grand Marshal Tremblade (A)", 3 },
            { 4001.0f, -4088.0f, 52.0f, "High Warlord Volrath (H)",    3 },
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeAshranScript()
{
    return std::make_unique<AshranScript>();
}

} // namespace Playerbot

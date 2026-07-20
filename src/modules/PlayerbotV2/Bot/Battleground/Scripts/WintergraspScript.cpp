// WintergraspScript — Battle for Wintergrasp
// (BattlemasterList ids 1017 + 1030 / BATTLEGROUND_EB_BW + EB_BW2).
// Epic 40v40 push-the-fortress adapted from the live Wintergrasp
// outdoor PvP zone. Map 571 (Northrend), zone 4197.
//
// Bot strategy: attackers grab vehicles at the 4 workshops, push to
// the keep gates, then to the Titan Relic in the keep interior.
// Defenders sit on workshops + keep cannons.
//
// Important note: there is NO authoritative BattlefieldWG source in
// this branch — coords below are approximate (workshop coords match
// long-standing TC/AC conventions; gates + relic are best-effort).
// Vehicle entries are well-known retail IDs (27881 / 28094 / 28312 /
// 32627). The vehicle_seat_spell_by_entry uses Hurl Boulder (50652)
// for the Demolisher only; the Catapult/Siege Engine overrides are
// best-effort, with SpellMgr rejection as safety net (bot stays
// mounted, owner can drive).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class WintergraspScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 1017; }  // BATTLEGROUND_EB_BW
    char const* name() const override { return "wintergrasp_battle"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 40-slot raid. WG-as-BG is decided by wall breach → relic.
        //   * 18 Attackers — vehicle pilots + gate breachers + relic push.
        //   * 6  Defenders — keep cannon line + workshop sentinels.
        //   * 8  Roamers   — workshop counter-flips.
        //   * 8  Healers   — split frontline / rear.
        a.role_by_slot.assign(40, BgRole::Free);
        for (uint8_t i = 0;  i < 18; ++i) a.role_by_slot[i] = BgRole::Attacker;
        for (uint8_t i = 18; i < 24; ++i) a.role_by_slot[i] = BgRole::Defender;
        for (uint8_t i = 24; i < 32; ++i) a.role_by_slot[i] = BgRole::Roamer;
        for (uint8_t i = 32; i < 40; ++i) a.role_by_slot[i] = BgRole::Healer;
        a.auto_use_go_types = { 42, 24 };

        // Endgame: Titan Relic in the keep interior. Approximate coord
        // (no authoritative BattlefieldWG.cpp in this branch).
        constexpr float RELIC_X = 5440.0f, RELIC_Y = 2840.0f, RELIC_Z = 419.0f;
        if (s.is_horde())
        {
            a.home_base_x = 5032.454f; a.home_base_y = 3711.382f; a.home_base_z = 372.468f;
            a.endgame_target_x = RELIC_X;
            a.endgame_target_y = RELIC_Y;
            a.endgame_target_z = RELIC_Z;
        }
        else
        {
            a.home_base_x = 5140.790f; a.home_base_y = 2179.120f; a.home_base_z = 390.950f;
            a.endgame_target_x = RELIC_X;
            a.endgame_target_y = RELIC_Y;
            a.endgame_target_z = RELIC_Z;
        }

        // Nodes: workshops (p=2 — siege vehicle spawns) + keep gates
        // (p=3 — Attacker primary target after vehicles built) +
        // graveyards (p=0) + Titan Relic (p=4 — win condition).
        a.nodes = {
            // Workshops — siege vehicle spawn points.
            { 5104.750f, 2300.940f, 368.579f, "Sunken Ring Workshop",   2 },
            { 5099.120f, 3466.036f, 368.484f, "Broken Temple Workshop", 2 },
            { 4314.648f, 2408.522f, 392.642f, "Eastpark Workshop",      2 },
            { 4331.716f, 3235.695f, 390.251f, "Westpark Workshop",      2 },
            // Keep gates — Attacker primary after vehicles. Coords
            // approximate (no TC BattlefieldWG source).
            { 5328.0f, 2842.0f, 408.0f, "Keep South Gate (approx)", 3 },
            { 5400.0f, 3100.0f, 408.0f, "Keep East Gate (approx)",  3 },
            { 5400.0f, 2580.0f, 408.0f, "Keep West Gate (approx)",  3 },
            // Graveyards — rez logistics only.
            { 5537.986f, 2897.493f, 517.057f, "Wintergrasp Keep GY", 0 },
            { 5032.454f, 3711.382f, 372.468f, "Horde GY",            0 },
            { 5140.790f, 2179.120f, 390.950f, "Alliance GY",         0 },
            // Titan Relic — priority 4 (win condition; only Attacker
            // on AllIn picks this up via endgame_target redirect).
            { RELIC_X, RELIC_Y, RELIC_Z, "Titan Relic (approx)",     4 },
        };

        // WG vehicles (retail well-known entries):
        //   27881 Catapult     — seat-0 ability varies; default fallback.
        //   28094 Demolisher   — seat-0 Hurl Boulder (50652) confirmed.
        //   28312 Siege Engine A
        //   32627 Siege Engine H
        // Per-entry overrides are unverified for Catapult / Siege
        // Engine — left empty rather than fabricated. Bot stays
        // mounted under the default fallback even if the cast no-ops.
        a.vehicle_creature_entries = { 27881, 28094, 28312, 32627 };
        a.vehicle_seat_spell       = 50652;  // Hurl Boulder (Demolisher seat-0)
        a.vehicle_seat_spell_by_entry.clear();
        return a;
    }
};

class WintergraspBattleScript2 final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 1030; }  // BATTLEGROUND_EB_BW2
    char const* name() const override { return "wintergrasp_battle_v2"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        // Variant 1030 shares the WG map + mechanics.
        WintergraspScript primary;
        return primary.get_advice(s);
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeWintergraspScript()
{
    return std::make_unique<WintergraspScript>();
}

std::unique_ptr<BattlegroundScript> MakeWintergraspScript2()
{
    return std::make_unique<WintergraspBattleScript2>();
}

} // namespace Playerbot

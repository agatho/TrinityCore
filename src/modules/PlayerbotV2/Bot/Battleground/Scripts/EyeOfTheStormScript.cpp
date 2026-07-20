// EyeOfTheStormScript — Eye of the Storm (BattlemasterList id 7 / BATTLEGROUND_EY).
// Hybrid: 4 control-zone towers + central Netherstorm Flag. Flag carrier
// caps at any friendly-owned tower (via AreaTrigger 33; no GO-use required).
//
// Authoritative TC sources (12.0.1 branch):
//   src/server/scripts/Battlegrounds/EyeOfTheStorm/battleground_eye_of_the_storm.cpp
//     :88-97  — tower / flag GO entries
//     :207-216 — BattlegroundEYControlZoneHandler (towers are CONTROL_ZONE)
//     :266-269 — handler bindings per tower entry
//     :639-646 — GAMEOBJECT_TYPE_CONTROL_ZONE (29) dispatch
//     :92     — BG_OBJECT_FLAG2_EY_ENTRY = 208977 (NEW_FLAG type, ie 36)
//     :101    — AREATRIGGER_CAPTURE_FLAG = 33 (capture mechanic)
// V1 coords for towers + center flag:
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Domination/
//     EyeOfTheStormData.h:64-91

#include "../BattlegroundScript.h"

namespace Playerbot {

namespace {

class EyeOfTheStormScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 7; }  // BATTLEGROUND_EY
    char const* name() const override { return "eye_of_the_storm"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        BattlegroundAdvice a;
        // 10v10 (modern TC). Slot 0 sprints to mid for the Netherstorm
        // Flag; slots 1-2 escort. Attacker pushes towers; Defender pins
        // a friendly-owned tower so the flag carrier can cap on contact.
        // Healers split between FC and the offensive cluster.
        a.role_by_slot = {
            BgRole::FlagCarrier,    // slot 0 — grabs Netherstorm Flag
            BgRole::FCEscort,
            BgRole::FCEscort,
            BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Defender,
            BgRole::Defender,
            BgRole::Healer,
            BgRole::Healer,
        };
        a.escort_friendly_carrier = true;
        a.chase_enemy_carrier     = true;
        a.chase_melee_only        = true;  // see WarsongGulchScript.cpp
        // Mobility-FC preference. EotS Netherstorm flag is a long open
        // run from mid to a friendly tower — Rogue stealth + Druid
        // Travel Form + DH double-jump + Hunter Disengage all clear
        // the corridor faster than a clothie would. Hunter (3) added
        // since EotS has more LoS than WSG's flag tunnel.
        a.fc_class_preference     = { 3u, 4u, 11u, 12u };
        // Towers capture by PRESENCE (GAMEOBJECT_TYPE_CONTROL_ZONE 29 has
        // NO case in GameObject::Use() — emitting use_game_object on a tower
        // is a server no-op that just burns a dispatch slot + the 3s BgUseGo
        // cooldown). The Attacker rule parks bots inside the control-zone
        // radius to tick the tower over; no click is involved (BG audit S4).
        // Central Netherstorm Flag pickup. NOTE 2026-06-22: the flag is BROKEN in
        // this server's world data and CANNOT be carried by anyone (bot or human)
        // until it's restored — entry 208977 "Netherstorm Flag" has NO spawn on
        // map 566, and its gameobject_template type is 24 (FLAGSTAND) whereas the
        // core flag state-machine (GameObject::GetFlagState / GetFlagCarrierGUID)
        // only works for NEW_FLAG (36). EotS still scores via the 4 CONTROL_ZONE
        // towers (verified). We advertise all plausible flag GO types so that the
        // INSTANT the world data is fixed (spawn 208977 on 566 as type 36 with
        // newflag.pickupSpell 34976), bots pick it up with no further code change.
        // Flag CAPTURE proper is AreaTrigger 33 once the carrier stands inside a
        // friendly tower — no GO-use call. (37 = dropped-flag re-grab.)
        a.auto_use_go_types = { 24, 36, 37 };
        // Netherstorm Flag pickup pedestal (mid). Authoritative from V1
        // EyeOfTheStormData.h:88-90 — confirmed near-equidistant from
        // all 4 towers (prior coord was a tower-top X≈2055, off by ~120y).
        a.enemy_flag_x = 2174.78f;
        a.enemy_flag_y = 1569.05f;
        a.enemy_flag_z = 1159.96f;
        // own_flag deliberately left at {0,0,0}: EotS caps at any
        // friendly tower, not a fixed pedestal. The FlagCarrier path
        // falls through to "closest own-team node" when own_flag is
        // sentinel-zero (State_Idle.cpp:3286-3318).
        //
        // 4 towers at the "* Cap Pt" CONTROL_ZONE GO spawns (184080-184083,
        // live world DB map 566). BG audit N70: the old V1-attributed
        // "Mage Tower" (1807,1540) was the HORDE FLOATING START PLATFORM
        // ~295y from any tower, and "Draenei Ruins" (2284,1577) sat 183y
        // out in mid-field — half the tower rotation was mis-targeted.
        // These MUST stay in sync with the EOTS_WS_NODES worldstate-
        // harvest table in BotSnapshotBuilder.cpp (5y cross-reference).
        // Priority=1 tilts the Attacker tiebreaker toward the mid-closer
        // pair when ownership buckets tie (State_Idle.cpp:3561-3568).
        a.nodes = {
            { 2024.6f, 1742.8f, 1195.2f, "Fel Reaver Ruins", 0 },
            { 2050.5f, 1372.2f, 1194.6f, "Blood Elf Tower",  0 },
            { 2301.0f, 1386.9f, 1197.2f, "Draenei Ruins",    1 },
            { 2282.1f, 1760.0f, 1189.7f, "Mage Tower",       1 },
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeEyeOfTheStormScript()
{
    return std::make_unique<EyeOfTheStormScript>();
}

} // namespace Playerbot

// WarsongGulchScript — Warsong Gulch (BattlemasterList id 2 / BATTLEGROUND_WS).
// Classic CTF on the modern map (TC registers WSG on map 2106; legacy
// map 489 still referenced in some DBC paths).
//
// Authoritative TC sources (12.0.1 branch):
//   src/server/scripts/Battlegrounds/WarsongGulch/battleground_warsong_gulch.cpp
//     :123-124 — flag GO entries 227740 (Horde) / 227741 (Alliance)
//     :43-50   — pickup spells (23333..23336)
//   src/server/game/Miscellaneous/SharedDefines.h
//     :3195    — GAMEOBJECT_TYPE_NEW_FLAG = 36
//     :3196    — GAMEOBJECT_TYPE_NEW_FLAG_DROP = 37
// V1 coords:
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/CTF/
//     WarsongGulchData.h:38-47 (flag spawns), :123-149 (defender posts)

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class WarsongGulchScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 2; }  // BATTLEGROUND_WS
    char const* name() const override { return "warsong_gulch"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // WSG/TP score = flag CAPS (0-3), not points: a 2-cap swing is
        // decisive (BG audit N55/N65 -- the 200-point default could
        // never trigger here).
        a.score_bias_threshold = 2;
        // 10v10. Slot 0 is FC (stealth-class bias is a deferred schema
        // add — see fc_class_preference TODO). FlagCarrier and FCEscort
        // are dynamic-handoff capable upstream (idle:acts_as_fc backup
        // path lets Roamer/Attacker step in when the primary dies).
        a.role_by_slot = {
            BgRole::FlagCarrier,
            BgRole::FCEscort,
            BgRole::Defender,     // flagroom anchor — ramp top / balcony
            BgRole::Defender,
            BgRole::Roamer,
            BgRole::Roamer,
            BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Healer,
            BgRole::Healer,
        };
        a.escort_friendly_carrier = true;
        a.chase_enemy_carrier     = true;
        // Clothie casters (Priest/Mage/Warlock/Evoker) shouldn't sprint
        // into a moving FC at 1v1 range — they feed honor and get peeled
        // off their objectives. Melee + hybrid + Hunter can chase.
        a.chase_melee_only        = true;
        // Stealth-FC meta: Rogue (4) and Druid (11 — feral Travel Form)
        // are the canonical WSG carriers. Stealth opens a defended
        // flagroom; Druid Travel keeps the run from being kited dead.
        // The dispatcher promotes preferred-class bots to FlagCarrier
        // regardless of slot hash; non-preferred bots that would have
        // hashed into the FC slot fall back to Roamer.
        a.fc_class_preference     = { 4u, 11u };
        // Modern WSG: flag pedestals are NEW_FLAG (36); dropped flag
        // is NEW_FLAG_DROP (37). Legacy FLAGDROP (26) is NOT spawned
        // on map 2106 — including it was a no-op that masked the
        // missing 37 (dropped-flag auto-return).
        a.auto_use_go_types = { 36, 37 };
        // Flag spawn coords from V1 WarsongGulchData.h:38-47
        // (Alliance 1540.423,1481.325,351.818; Horde 916.023,1433.805,346.037).
        constexpr float ALLY_FX  = 1540.423f, ALLY_FY  = 1481.325f, ALLY_FZ  = 351.818f;
        constexpr float HORDE_FX =  916.023f, HORDE_FY = 1433.805f, HORDE_FZ =  346.037f;
        if (s.is_horde())
        {
            a.own_flag_x   = HORDE_FX; a.own_flag_y   = HORDE_FY; a.own_flag_z   = HORDE_FZ;
            a.enemy_flag_x = ALLY_FX;  a.enemy_flag_y = ALLY_FY;  a.enemy_flag_z = ALLY_FZ;
            // Defender anchor = Horde ramp top (V1 :159) — covers
            // tunnel-mouth + the only ranged sightline. NOT the flag
            // pedestal: planting Defenders on the pedestal eats AoE and
            // hands stealth carriers a global pickup.
            a.home_base_x = 948.68f;  a.home_base_y = 1458.65f; a.home_base_z = 345.903f;
        }
        else
        {
            a.own_flag_x   = ALLY_FX;  a.own_flag_y   = ALLY_FY;  a.own_flag_z   = ALLY_FZ;
            a.enemy_flag_x = HORDE_FX; a.enemy_flag_y = HORDE_FY; a.enemy_flag_z = HORDE_FZ;
            // Alliance ramp top (V1 :133).
            a.home_base_x = 1507.04f; a.home_base_y = 1456.85f; a.home_base_z = 352.013f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeWarsongGulchScript()
{
    return std::make_unique<WarsongGulchScript>();
}

} // namespace Playerbot

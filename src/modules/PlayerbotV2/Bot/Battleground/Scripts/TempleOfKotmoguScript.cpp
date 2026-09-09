// TempleOfKotmoguScript — Temple of Kotmogu
// (BattlemasterList id 699 / BATTLEGROUND_TK, map 998).
// MoP 10v10 orb-bearer BG. 4 orbs at corners; pick up to receive a
// stacking score-per-tick whose multiplier scales with distance from
// map center (Small/Medium/Large aura rings — center pays best).
//
// Authoritative TC sources (12.0.1):
//   src/server/scripts/Battlegrounds/TempleOfKotmogu/battleground_temple_of_kotmogu.cpp
//     :80-83  — orb GO entries 212091..212094 (NEW_FLAG type)
//     :96-99  — orb spawn coords (PurpleOrb / GreenOrb / BlueOrb / OrangeOrb)
//     :306    — OnFlagTaken pickup handler
//     :640    — RegisterBattlegroundMapScript(...,998)

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class TempleOfKotmoguScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 699; }  // BATTLEGROUND_TK
    char const* name() const override { return "temple_of_kotmogu"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // OrbCarrier role (specific to Kotmogu) walks to the enemy_flag
        // pickup, picks up the orb, then holds at home_base (map center)
        // to accumulate the SmallAura distance-from-center score
        // multiplier — no return-home-to-cap phase. Four OrbCarrier
        // slots keep the grab race competitive even when bots respawn
        // staggered.
        a.role_by_slot = {
            BgRole::OrbCarrier, BgRole::OrbCarrier,
            BgRole::OrbCarrier, BgRole::OrbCarrier,
            BgRole::FCEscort,    BgRole::FCEscort,
            BgRole::Healer,      BgRole::Healer,
            BgRole::Roamer,      BgRole::Roamer,
        };
        a.escort_friendly_carrier = true;
        a.chase_enemy_carrier     = true;
        // BG audit N69: the orbs are gameobject_template type 24
        // (FLAGSTAND) — entries 212091-212094 ("Orb of Power"), spawned
        // dynamically by the BG map script — NOT the NEW_FLAG (36) this
        // script previously requested, so the auto-use rule could never
        // match one. Match by entry (exact) plus type 24 for the pickup.
        a.auto_use_go_types   = { 24 };
        a.auto_use_go_entries = { 212091, 212092, 212093, 212094 };
        // 4 orb spawns at the corners (TC :96-99). Carriers spread
        // across the four corners via the FlagCarrier path's
        // pickup-target selection; Roamers chase enemy carriers
        // (chase_enemy_carrier) so contested orbs get pressured.
        a.nodes = {
            { 1850.16f, 1250.11f, 13.21f, "Orange Orb (SE)" },
            { 1716.95f, 1250.02f, 13.33f, "Blue Orb (SW)"   },
            { 1716.89f, 1416.62f, 13.21f, "Green Orb (NW)"  },
            { 1850.22f, 1416.82f, 13.34f, "Purple Orb (NE)" },
        };
        // BG audit N03/N33: the OrbCarrier movement path consumes
        // enemy_flag_x/y — which this script never set, leaving 4 of 10
        // slots motionless all match. Hash the bot's guid across the 4
        // orb spawns so each carrier heads to a DIFFERENT corner (stable
        // per bot, no per-tick ping-pong). Once adjacent, the auto-use
        // entry match above performs the actual grab.
        {
            const uint64_t gh = s.guid().GetCounter();
            auto const& orb = a.nodes[gh % a.nodes.size()];
            a.enemy_flag_x = orb.x;
            a.enemy_flag_y = orb.y;
            a.enemy_flag_z = orb.z;
        }
        // Map center — midpoint of the 4 orb spawns. Carriers hold
        // here for the SmallAura score multiplier (TC :121-123).
        a.home_base_x = 1783.59f;
        a.home_base_y = 1333.42f;
        a.home_base_z = 13.25f;
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeTempleOfKotmoguScript()
{
    return std::make_unique<TempleOfKotmoguScript>();
}

} // namespace Playerbot

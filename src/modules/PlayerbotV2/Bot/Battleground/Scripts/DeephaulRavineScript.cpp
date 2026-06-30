// DeephaulRavineScript — Deephaul Ravine (BattlemasterList id 1110, map 2656).
// TWW 10v10 hybrid: a central Deephaul Crystal (carry to score, Kotmogu-
// style hold) + two mine carts on rails (escort to score, Silvershard-
// style control zones).
//
// Authoritative TC sources (12.0.1):
//   src/server/scripts/Battlegrounds/DeephaulRavine/battleground_deephaul_ravine.cpp
//     :73-74  — cart creature entries (MineCartEast 214690 / MineCartWest 217346)
//     :106    — GameObjects::DeephaulCrystal 422413 (type 36 NEW_FLAG, DB-verified)
//     :136-137— cart spawn positions (WestMineCartSpawn / EastMineCartSpawn)
//     :139-158— Earthen cart clusters (Horde NE ~4170,-2800; Alliance SW ~3955,-3095)
//     :1100   — RegisterBattlegroundMapScript(..., 2656)
// World DB (map 2656): Deephaul Crystal GO spawn at (4063, -2949, 205).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class DeephaulRavineScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 1110; }  // Deephaul Ravine
    char const* name() const override { return "deephaul_ravine"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 10v10. The crystal is the swing objective. NOTE: DHR scores the
        // crystal on DELIVERY, not on hold — OnCaptureFlag awards +100 only
        // when the carrier reaches its faction CapturePoint AreaTrigger
        // (battleground_deephaul_ravine.cpp:366-387, AT entries 30/31 at the
        // faction base). It is a carry-and-deliver flag, NOT a Kotmogu hold.
        // Carts are steady income. Split: 1 crystal carrier + 2 escorts mid,
        // 4 cart escorts (2 per cart via the rank spread), 1 roamer, 2 healers.
        a.role_by_slot = {
            BgRole::FlagCarrier,   // crystal runner
            BgRole::FCEscort,
            BgRole::FCEscort,
            BgRole::Attacker,      // cart pressure (nodes below)
            BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Roamer,
            BgRole::Healer,
            BgRole::Healer,
        };
        a.escort_friendly_carrier = true;
        a.chase_enemy_carrier     = true;
        // Crystal pickup: GAMEOBJECT_TYPE_NEW_FLAG (36), entry 422413 —
        // visible in the snapshot since the wave-14 GO-filter fix.
        a.auto_use_go_types   = { 36 };
        a.auto_use_go_entries = { 422413 };
        // Crystal spawn (world DB gameobject row on map 2656). The
        // FlagCarrier path walks here; the auto-use match performs the
        // pickup. Respawns mid after each capture.
        a.enemy_flag_x = 4063.0f;
        a.enemy_flag_y = -2949.0f;
        a.enemy_flag_z =  205.0f;
        // Carts: moving objectives — follow_creature_entry resolves the
        // LIVE cart position from nearby units (Silvershard mechanism);
        // the static coords are the TC-script spawn points used until a
        // cart is in scan range.
        a.nodes = {
            { 3875.00f, -3150.00f, 240.29f, "East Cart", 0, /*follow*/ 214690u },
            { 4250.36f, -2751.07f, 239.47f, "West Cart", 0, /*follow*/ 217346u },
        };
        // Faction home bases = own Earthen-cart cluster (TC script
        // positions). Used as the carrier hold/retreat anchor: the
        // crystal scores while HELD, so the carrier hugs its own base
        // under escort rather than standing mid.
        if (s.is_horde())
        {
            a.home_base_x = 4170.0f; a.home_base_y = -2800.0f; a.home_base_z = 240.9f;
        }
        else
        {
            a.home_base_x = 3955.0f; a.home_base_y = -3095.0f; a.home_base_z = 240.9f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeDeephaulRavineScript()
{
    return std::make_unique<DeephaulRavineScript>();
}

} // namespace Playerbot

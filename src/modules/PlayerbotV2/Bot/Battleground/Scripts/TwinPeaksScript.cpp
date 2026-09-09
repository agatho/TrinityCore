// TwinPeaksScript — Twin Peaks (BattlemasterList id 108, map 726).
// Cataclysm CTF, mechanically identical to WSG.
//
// Authoritative TC sources:
//   src/server/scripts/Battlegrounds/TwinPeaks/battleground_twin_peaks.cpp
//     :50-51 — flag GO entries 227740 (Horde) / 227741 (Alliance)
//   src/server/game/Miscellaneous/SharedDefines.h:3195-3196
//     NEW_FLAG=36, NEW_FLAG_DROP=37
// V1 coords:
//   src/modules/Playerbot/AI/Coordination/Battleground/Scripts/CTF/
//     TwinPeaksData.h:80-89 (flag spawns), :165, :187 (defender posts)

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class TwinPeaksScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 108; }
    char const* name() const override { return "twin_peaks"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // WSG/TP score = flag CAPS (0-3), not points: a 2-cap swing is
        // decisive (BG audit N55/N65 -- the 200-point default could
        // never trigger here).
        a.score_bias_threshold = 2;
        a.role_by_slot = {
            BgRole::FlagCarrier,
            BgRole::FCEscort,
            BgRole::Defender,
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
        a.chase_melee_only        = true;  // see WarsongGulchScript.cpp
        // Stealth FC — Rogue + Druid (see WarsongGulchScript.cpp).
        a.fc_class_preference     = { 4u, 11u };
        // NEW_FLAG (36) pedestals + NEW_FLAG_DROP (37) dropped flag.
        // Legacy FLAGDROP (26) does not spawn on map 726.
        a.auto_use_go_types = { 36, 37 };
        // V1 TwinPeaksData.h:80-89.
        constexpr float ALLY_FX  = 2118.210f, ALLY_FY  = 191.621f, ALLY_FZ  = 44.052f;
        constexpr float HORDE_FX = 1578.339f, HORDE_FY = 344.063f, HORDE_FZ =  2.419f;
        if (s.is_horde())
        {
            a.own_flag_x   = HORDE_FX; a.own_flag_y   = HORDE_FY; a.own_flag_z   = HORDE_FZ;
            a.enemy_flag_x = ALLY_FX;  a.enemy_flag_y = ALLY_FY;  a.enemy_flag_z = ALLY_FZ;
            // Horde fortress balcony (V1 :187) — covers main entrance + back ramp.
            a.home_base_x = 1578.34f; a.home_base_y = 338.06f; a.home_base_z = 9.42f;
        }
        else
        {
            a.own_flag_x   = ALLY_FX;  a.own_flag_y   = ALLY_FY;  a.own_flag_z   = ALLY_FZ;
            a.enemy_flag_x = HORDE_FX; a.enemy_flag_y = HORDE_FY; a.enemy_flag_z = HORDE_FZ;
            // Wildhammer balcony (V1 :165), elevated above pedestal.
            a.home_base_x = 2118.21f; a.home_base_y = 185.62f; a.home_base_z = 51.05f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeTwinPeaksScript()
{
    return std::make_unique<TwinPeaksScript>();
}

} // namespace Playerbot

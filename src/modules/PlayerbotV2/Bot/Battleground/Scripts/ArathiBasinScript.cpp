// ArathiBasinScript — Arathi Basin (BattlemasterList id 3 / BATTLEGROUND_AB).
// Resource race. 5 nodes: Stables, Lumber Mill, Blacksmith, Mine, Farm.
// Capture 4+ to gain ticks; first to 1500 wins. Canonical strategy is
// 4-cap: defend 4, soft-attack the 5th (typically the contested mid).
//
// Authoritative coords + strategic weights from V1
// src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Domination/
// ArathiBasinData.h:54-81 (positions), :110-121 (GetNodeStrategicValue),
// :159, :168 (faction spawns).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class ArathiBasinScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 3; }  // BATTLEGROUND_AB
    char const* name() const override { return "arathi_basin"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 15v15. Justified per slot: 5 nodes need 5 bodies to contest
        // re-caps; Healer count keeps offensive zerg + two anchor points
        // alive. 3 Roamers act as the swing force on contested nodes.
        a.role_by_slot = {
            BgRole::Defender, BgRole::Defender, BgRole::Defender,
            BgRole::Defender, BgRole::Defender,
            BgRole::Attacker, BgRole::Attacker, BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Roamer,   BgRole::Roamer,   BgRole::Roamer,
            BgRole::Healer,   BgRole::Healer,   BgRole::Healer,
        };
        // Live AB nodes on map 2107 are 5 dynamically-spawned CAPTURE_POINT
        // (42) banners (entries 227420 / 227522 / 227536 / 227538 / 227544).
        // FLAGSTAND (24) dropped (BG audit §2): map 2107 has ZERO type-24 GOs,
        // so it was an inert per-tick scan. (The old comment's 180087-180091
        // are legacy type-10 banners, not what spawns live.)
        a.auto_use_go_types = { 42 };
        // Priority mirrors V1 GetNodeStrategicValue:
        //   Blacksmith = center pivot (most contested)
        //   Lumber Mill = high-ground LoS / strong vantage
        //   Stables / Mine / Farm = perimeter
        // Attacker rule (State_Idle.cpp:3583-3596) uses priority as
        // tie-breaker within ownership bucket — bots prefer BS/LM flips.
        a.nodes = {
            { 1166.785f, 1200.132f,  -56.70f, "Stables",     0 },
            {  856.141f, 1148.902f,   11.18f, "Lumber Mill", 1 },
            {  977.017f, 1046.534f,  -44.80f, "Blacksmith",  2 },
            { 1146.923f,  848.277f, -110.52f, "Mine",        0 },
            {  806.218f,  874.217f,  -55.99f, "Farm",        0 },
        };
        // Faction spawn (V1 ArathiBasinData.h:159,168). Defender
        // fallback anchor when no node is in range / contested.
        if (s.is_horde())
        {
            a.home_base_x =  686.57f; a.home_base_y =  683.04f; a.home_base_z = -12.59f;
        }
        else
        {
            a.home_base_x = 1285.96f; a.home_base_y = 1281.62f; a.home_base_z = -15.67f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeArathiBasinScript()
{
    return std::make_unique<ArathiBasinScript>();
}

} // namespace Playerbot

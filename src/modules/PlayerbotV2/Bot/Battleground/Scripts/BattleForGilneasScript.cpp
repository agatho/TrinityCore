// BattleForGilneasScript — Battle for Gilneas
// (BattlemasterList id 120, map 761). Cata 3-node mini-Arathi.
// Capture 2+ of 3 (Lighthouse / Waterworks / Mines) to score.
//
// Authoritative coords + weights from V1
// src/modules/Playerbot/AI/Coordination/Battleground/Scripts/Domination/
// BattleForGilneasData.h:52-67 (positions), :92-101 (GetNodeStrategicValue),
// :137, :146 (faction spawns).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class BattleForGilneasScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 120; }
    char const* name() const override { return "battle_for_gilneas"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 10v10. Canonical BfG strategy is 2-cap (defend 2, contest the
        // 3rd, usually Waterworks).
        //   * 3 Defenders — one per node.
        //   * 3 Attackers — push the contested third node.
        //   * 2 Roamers   — counter-flip patrol.
        //   * 2 Healers   — split between offense / defense.
        a.role_by_slot = {
            BgRole::Defender, BgRole::Defender, BgRole::Defender,
            BgRole::Attacker, BgRole::Attacker, BgRole::Attacker,
            BgRole::Roamer,   BgRole::Roamer,
            BgRole::Healer,   BgRole::Healer,
        };
        // BfG node banners are CAPTURE_POINT (42): Lighthouse 228050,
        // Waterworks 228052, Mines 228053 (DB-verified on map 761). FLAGSTAND
        // (24) dropped (BG audit §2): map 761 has ZERO type-24 GOs — it was an
        // inert scan. (The old comment's 208522-208524 are type-31 portal
        // doodads, not the banners.)
        a.auto_use_go_types = { 42 };
        // priority weights mirror V1 GetNodeStrategicValue: Waterworks
        // (center, contested) = 2; homes = 0. Attacker tiebreaker biases
        // toward Waterworks flips.
        a.nodes = {
            { 1057.73f, 1278.29f,   3.19f, "Lighthouse", 0 },
            {  980.07f,  948.17f,  12.72f, "Waterworks", 2 },
            // Mines from the live CAPTURE_POINT spawn 228053 on map 761
            // (BG audit N73: the V1 coord was 122y south, off the node).
            { 1251.00f,  958.30f,   5.70f, "Mines",      0 },
        };
        // Defender fallback anchor. The Horde value was (1330,736) which is
        // OFF the playable field (map-761 GO y-min ≈ 760.6) — a Defender that
        // fell back there walked out of bounds (BG audit §2). Moved on-field
        // to (1330,970,6.5), on the line between the Horde Gate (1396,977) and
        // the Mines node (1251,958).
        if (s.is_horde())
        {
            a.home_base_x = 1330.0f; a.home_base_y =  970.0f; a.home_base_z =  6.5f;
        }
        else
        {
            a.home_base_x = 1052.0f; a.home_base_y = 1396.0f; a.home_base_z =  6.0f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeBattleForGilneasScript()
{
    return std::make_unique<BattleForGilneasScript>();
}

} // namespace Playerbot

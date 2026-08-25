// DeepwindGorgeScript — Deepwind Gorge (BattlemasterList id 754 / BATTLEGROUND_DG).
// Originally a hybrid 15v15 cart-rush; current modern variants (Domination
// 1037/1039 aliased to 754 in BattlegroundScript.cpp:113-115) ship as a
// pure 3-node 10v10 domination BG. The cart mechanic is deprecated in the
// shipped client — treat DG as 3-node domination here. Cart-hauler
// behaviour is deferred until a snapshot field exposes bg.mine_carts
// (V1 cart data preserved in src/modules/Playerbot/.../Domination/
// DeepwindGorgeData.h:159-374).
//
// Map id: 1105 (NOT 998 — 998 is Temple of Kotmogu, common confusion).
// V1 coords: src/modules/Playerbot/AI/Coordination/Battleground/Scripts/
// Domination/DeepwindGorgeData.h:50-63 (nodes), :475-483 (faction spawns).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class DeepwindGorgeScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 754; }  // BATTLEGROUND_DG
    char const* name() const override { return "deepwind_gorge"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 10v10 (TEAM_SIZE=10 per V1 DeepwindGorgeData.h:31). FlagCarrier /
        // FCEscort REMOVED — DG-domination is not CTF; the snapshot's
        // bg.friendly/enemy_flag_carrier fields are never populated for
        // this map. The prior 15-slot vector also overshot raid size:
        // slots 10-14 were silently truncated, mis-tuning role density.
        // Shape mirrors BfG (3 nodes, 10v10).
        a.role_by_slot = {
            BgRole::Defender, BgRole::Defender, BgRole::Defender,
            BgRole::Attacker, BgRole::Attacker, BgRole::Attacker,
            BgRole::Roamer,   BgRole::Roamer,
            BgRole::Healer,   BgRole::Healer,
        };
        // CAPTURE_POINT (42) covers modern banners; FLAGSTAND (24) kept
        // for legacy / brawl-variant spawn data. Removed NEW_FLAG (36)
        // and FLAGDROP (26) — DG has no flag GOs.
        a.auto_use_go_types = { 42, 24 };
        // 3 mine nodes. Pandaren Mine sits in the contested middle, so
        // priority=1 biases Attacker tie-breaks toward it.
        a.nodes = {
            { 1600.53f,  945.24f, 20.0f, "Pandaren Mine", 1 },
            { 1447.27f, 1110.36f, 15.0f, "Goblin Mine",   0 },
            { 1753.79f,  780.12f, 18.0f, "Center Mine",   0 },
        };
        // V1 spawn coords (DeepwindGorgeData.h:475-483). Used by the
        // Defender role as anchor when no node is in range / contested.
        if (s.is_horde())
        {
            a.home_base_x = 1850.0f; a.home_base_y =  800.0f; a.home_base_z = 12.0f;
        }
        else
        {
            a.home_base_x = 1350.0f; a.home_base_y = 1100.0f; a.home_base_z = 10.0f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeDeepwindGorgeScript()
{
    return std::make_unique<DeepwindGorgeScript>();
}

} // namespace Playerbot

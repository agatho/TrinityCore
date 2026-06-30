// SilvershardMinesScript — Silvershard Mines
// (BattlemasterList id 708 / BATTLEGROUND_SM, map 727). MoP 10v10
// resource-race BG: first team to 1500 points wins (battleground_silvershard_
// mines.cpp:134 ResourceValues::Max = 1500). Three mine carts
// auto-travel from a central spawn cluster along the rails toward five
// depot endpoints. Carts are CONTROLLED BY PROXIMITY — there's no
// click, no push, no ride; standing in the capture zone around a cart
// tilts it to your team and trickles points. Players can also click
// two creature-based track switches (Eastern + Northern crossroads)
// to redirect carts onto adjacent paths.
//
// Bot strategy: stack on the nearest cart and contest proximity. With
// only 3 carts among 10 players per side, the right shape is "3 small
// cart squads + healers anchored mid".
//
// Authoritative coords from TC core
// (src/server/scripts/Battlegrounds/SilvershardMines/battleground_silvershard_mines.cpp):
//   MineCartSouth (739.29, 203.76, 319.54)
//   MineCartEast  (744.51, 183.20, 319.54)
//   MineCartNorth (759.32, 198.33, 319.53)
// All three spawn in the central mine at Z≈319.5 — NOT the surface Z=380+
// the prior coords referenced (those were cosmetic-only carts visible from
// above ground).

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class SilvershardMinesScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 708; }  // BATTLEGROUND_SM
    char const* name() const override { return "silvershard_mines"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        BattlegroundAdvice a;
        // 10v10 — three cart squads (3 each) + one flex Healer anchored
        // mid for cross-cart support. No FlagCarrier role (no flags).
        // No Defender (no static base to defend — depots score for whoever
        // happens to be near the cart at the moment of capture, not for a
        // standing defender). Roamer rule's "closest contested node" logic
        // covers cart contesting cleanly when we feed it the 3 cart spawns
        // as nodes.
        a.role_by_slot = {
            BgRole::Roamer, BgRole::Roamer, BgRole::Roamer,   // South cart squad
            BgRole::Roamer, BgRole::Roamer, BgRole::Roamer,   // East cart squad
            BgRole::Roamer, BgRole::Roamer, BgRole::Roamer,   // North cart squad
            BgRole::Healer,                                   // flex mid healer
        };
        // No GO auto-use: the "track switches" in Silvershard are CLICKABLE
        // CREATURES (StringId bg_silvershard_mines_track_switch_east /
        // _north in TC core), not GameObjects. The bot's auto_use_go path
        // operates on GO type IDs; using it here would scan for GOs that
        // aren't there. A creature-cast intent would be needed to flip
        // switches, but bots don't currently have that wiring — proximity
        // control alone is enough to play the BG passably.
        a.auto_use_go_types = {};
        // Three cart spawn positions (all Z≈319). The carts MOVE on
        // rails after spawn; follow_creature_entry=60140 (NPC_MINE_CART
        // entry, used 3 times) tells the consumer to re-resolve each
        // node's position from the nearest live Creature with this entry
        // — so the bot tracks the cart as it travels along the rail.
        // Static x/y/z is the spawn-point fallback used before the cart
        // creature is in nearby_units range.
        a.nodes = {
            { 739.30f, 203.76f, 319.54f, "Cart South", /*priority*/ 0, /*follow_creature_entry*/ 60140u },
            { 744.52f, 183.20f, 319.54f, "Cart East",  /*priority*/ 0, /*follow_creature_entry*/ 60140u },
            { 759.32f, 198.33f, 319.53f, "Cart North", /*priority*/ 0, /*follow_creature_entry*/ 60140u },
        };
        // No home_base / endgame_target — Silvershard has no static
        // defendable point and no enemy boss to push toward. Carts
        // travel to 5 different depots depending on switch state;
        // chasing a fixed "enemy depot" coord would just walk bots
        // away from the gameplay center.
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeSilvershardMinesScript()
{
    return std::make_unique<SilvershardMinesScript>();
}

} // namespace Playerbot

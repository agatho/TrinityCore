// SeethingShoreScript — Seething Shore (BattlemasterList id 894 / BATTLEGROUND_SS).
// BfA pre-launch BG. Race to capture random Azerite extraction nodes
// that parachute in from the air. 10v10; nodes spawn in waves; score by
// holding nodes and turning in Azerite.
//
// Authoritative coords from TC source:
//   src/server/scripts/Battlegrounds/SeethingShore/battleground_seething_shore.cpp
// Playable area: X≈1200..1430, Y≈2620..2940. Anywhere outside is water /
// fatigue zone — bots placed there die from fatigue.

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class SeethingShoreScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 894; }  // BATTLEGROUND_SS
    char const* name() const override { return "seething_shore"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // 10v10 layout. SS is a wave-spawn race: Azerite nodes drop from
        // the air every ~30s in 2-3 locations selected from the 6 landing
        // zones. Roamer rule (contested>neutral priority) chases the next
        // wave automatically because the Builder feeds bg_node_states with
        // any active capture point. No static defense — the only role
        // that "holds" anything is the bot mid-cap on top of an Azerite
        // node, which the Roamer already does.
        //
        //   * 4 Roamers   — chase the wave; the closest-contested rule
        //                   organically distributes across active spawns.
        //   * 3 Attackers — push toward the largest contested cluster
        //                   (where the enemy team is also stacked).
        //   * 2 Healers   — split between the two roving subgroups.
        //   * 1 Defender  — last-bot-standing fallback (holds home_base
        //                   when no nodes are live so they don't drown).
        a.role_by_slot = {
            BgRole::Roamer, BgRole::Roamer, BgRole::Roamer, BgRole::Roamer,
            BgRole::Attacker, BgRole::Attacker, BgRole::Attacker,
            BgRole::Healer,   BgRole::Healer,
            BgRole::Defender,
        };
        // Azerite extraction nodes are GAMEOBJECT_TYPE_CAPTURE_POINT (42)
        // — TC battleground_seething_shore.cpp:662-682 binds the capture
        // assault handler via that type (60s proximity cap, gameobject_
        // template Data0=60000). FLAGSTAND (24) does not appear on this map.
        a.auto_use_go_types = { 42 };
        // The 12 AZERITE FISSURE candidate locations (creature 125253,
        // static spawns on map 1803 — battleground_seething_shore.cpp:69-72,
        // :83). The BG activates 3 at a time in waves; on activation the
        // controller summons a type-42 capture GO at the fissure position.
        // The PREVIOUS coords here were the 6 air-supply BUFF-CRATE drops
        // (creature 133542 "Air Supply Ground Dummy") — NOT capturable — so
        // bots never reached a real node (BG audit SS blocker). Bots patrol
        // the full fissure set; the auto-use(42) pass caps whichever is live,
        // and the generic capture-point harvest routes Roamers/Attackers to
        // any active node. Verified against world.creature (id 125253, map
        // 1803, 12 rows). Playable area X≈1110..1465, Y≈2570..2920.
        a.nodes = {
            { 1113.92f, 2886.99f, 38.456f, "Fissure NW Cliff"  },
            { 1126.65f, 2781.24f, 30.861f, "Fissure West"      },
            { 1243.15f, 2721.47f, 11.902f, "Fissure SW Flats"  },
            { 1257.41f, 2882.73f, 27.951f, "Fissure North"     },
            { 1259.16f, 2571.42f,  8.586f, "Fissure South"     },
            { 1339.56f, 2785.68f,  2.559f, "Fissure Center"    },
            { 1343.72f, 2919.95f, 32.870f, "Fissure NE Ridge"  },
            { 1361.73f, 2643.27f,  4.468f, "Fissure SE Flats"  },
            { 1390.24f, 2570.77f,  6.464f, "Fissure SE Beach"  },
            { 1441.06f, 2700.15f,  9.505f, "Fissure East"      },
            { 1454.09f, 2598.38f, 15.206f, "Fissure SE Cliff"  },
            { 1461.36f, 2823.24f, 31.677f, "Fissure NE Cliff"  },
        };
        // Faction "safe inland" anchors for the Defender last-bot fallback,
        // placed on solid ground at real fissure positions (bots that stand
        // still at the water edge drown). Horde north / Alliance south.
        if (s.is_horde())
        {
            a.home_base_x = 1257.41f; a.home_base_y = 2882.73f; a.home_base_z = 27.951f;
        }
        else
        {
            a.home_base_x = 1361.73f; a.home_base_y = 2643.27f; a.home_base_z = 4.468f;
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeSeethingShoreScript()
{
    return std::make_unique<SeethingShoreScript>();
}

} // namespace Playerbot

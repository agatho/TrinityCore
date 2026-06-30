// AhnkahetScript — Ahn'kahet: The Old Kingdom (map 619, WotLK 73-79).
// 4 bosses: Elder Nadox, Prince Taldaram, Jedoga Shadowseeker, Herald
// Volazj.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/AzjolNerub/Ahnkahet/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AhnkahetScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 619; }
    char const* name() const override { return "ahnkahet"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            30178,  // Ahn'kahar Swarmer (Nadox add)
            30176,  // Ahn'kahar Guardian (Nadox add)
        };
        a.mandatory_interrupt_spells = {
            // Volazj (final boss) — wipes the run if Insanity proceeds
            57496,  // Insanity
            57941,  // Mind Flay
            57942,  // Shadow Bolt Volley
            // Nadox — swarm casts
            56281,  // Swarm Buff
            56354,  // Sprint
            // Amanitar (mini boss) — root/disable
            57094,  // Bash
            57095,  // Entangling Roots
            57088,  // Venom Bolt Volley
        };
        a.cc_priority_entries = {
            30178,  // Swarmer (CC swarms before tank picks up)
        };
        a.dangerous_auras = {
            // Volazj Insanity — phased illusions appear
            57508,  // Insanity target marker
            57496,  // Insanity main effect
            // Amanitar mushrooms
            57061,  // Poisonous Mushroom Poison Cloud
            56741,  // Poisonous Mushroom Visual Aura
            // Nadox area
            56130,  // Brood Plague
            59465,  // Brood Rage (boss enrage)
        };
        // Boss progression — NPC entries from TC's ahnkahet.h.
        a.bosses = {
            29309,  // Elder Nadox
            29308,  // Prince Taldaram
            29310,  // Jedoga Shadowseeker
            30258,  // Amanitar (optional mini-boss)
            29311,  // Herald Volazj (final)
        };
        // Progression waypoints — Ahn'kahet is a 4-room nerubian dungeon.
        a.progression_waypoints = {
            { 372.0f, -714.0f, -16.4f },   // entry
            { 460.0f, -769.0f, -16.0f },   // Nadox cavern
            { 374.0f, -842.0f, -27.0f },   // Amanitar tunnel
            { 226.0f, -797.0f, -16.0f },   // Taldaram blood pool
            { 250.0f, -842.0f, -16.0f },   // Jedoga altar
            { 296.0f, -752.0f, -29.0f },   // Volazj sanctum
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAhnkahetScript()
{
    return std::make_unique<AhnkahetScript>();
}

} // namespace Playerbot

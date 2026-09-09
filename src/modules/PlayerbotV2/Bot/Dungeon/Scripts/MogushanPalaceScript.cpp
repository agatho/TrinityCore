// MogushanPalaceScript — Mogu'shan Palace (map 994, MoP 90).
//   * Trial of the King — Random one of: Ming the Cunning (Lightning),
//     Kuai the Brute (Slam), Haiyan the Unstoppable (Stomp), Xin
//     the Weaponmaster (Whirlwind).
//   * Gekkan — Bodyguards (4 adds, priority kill).
//   * Xin the Weaponmaster (final) — Weapon Wheel rotation; bots
//     react to weapon change.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MogushanPalaceScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 994; }
    char const* name() const override { return "mogushan_palace"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            61444,  // Trial of the King — Ming
            61442,  // Kuai
            61445,  // Haiyan
            61216,  // Glintrok Hexxer (Gekkan bodyguard — healer)
            61239,  // Glintrok Oracle (Gekkan bodyguard)
            61240,  // Glintrok Skulker (Gekkan bodyguard)
            61242,  // Glintrok Ironhide (Gekkan bodyguard)
        };
        a.mandatory_interrupt_spells = {
            117428,  // Lightning Prison (Ming)
            117462,  // Slam (Kuai)
            117485,  // Stomp (Haiyan)
            118077,  // Wound (Xin)
        };
        a.dangerous_auras = {
            117428,  // Lightning Prison root
            118077,  // Wound stack
        };
        // Boss progression — Mogu'shan Palace has 3 encounters.
        // Trial of the King is a random 1-of-4 (Ming/Kuai/Haiyan are
        // alternate spawns; canonically only Xin completes as the slot).
        // Progression waypoints — Mogu'shan Palace is a Vale of
        // Eternal Blossoms palace with central throne room.
        a.progression_waypoints = {
            { -50.0f, -250.0f, 152.0f },   // entry
            {  10.0f, -300.0f, 152.0f },   // Trial of the King throne
            {  -5.0f, -380.0f, 152.0f },   // Gekkan arena
            {  10.0f, -430.0f, 165.0f },   // Xin's vault
        };
        a.bosses = {
            61444,  // Trial of the King (Ming)
            61442,  // Trial of the King (Kuai)
            61445,  // Trial of the King (Haiyan)
            61243,  // Gekkan
            61398,  // Xin the Weaponmaster (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMogushanPalaceScript()
{
    return std::make_unique<MogushanPalaceScript>();
}

} // namespace Playerbot

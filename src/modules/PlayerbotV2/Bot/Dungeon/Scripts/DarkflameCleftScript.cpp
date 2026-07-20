// DarkflameCleftScript — Darkflame Cleft (map 2651, TWW 70-80).
// Ringing Deeps kobold cave dungeon — candle / dark theme. 4 bosses:
//   * Ol' Waxbeard, Blazikon, The Candle King, The Darkness
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/KhazAlgar/DarkflameCleft/boss_the_candle_king.cpp
// Only Candle King is implemented in this TC build; other bosses use
// generic engagement logic.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DarkflameCleftScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2651; }
    char const* name() const override { return "darkflame_cleft"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
        };
        a.mandatory_interrupt_spells = {
            // The Candle King
            421648,  // Cursed Wax — stuns a player
            426145,  // Paranoid Mind — fear cast
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // The Candle King
            421653,  // Cursed Wax stun debuff (avoid the wax pool)
            421277,  // Darkflame Pickaxe cast — telegraphed impact
            421282,  // Darkflame Pickaxe damage zone
            420691,  // Molten Wax area trigger
            421067,  // Molten Wax damage
            421250,  // Throw Darkflame marker
            421145,  // Throw Darkflame missile impact
            426127,  // Darklight debuff — avoid when carried
        };
        // Boss progression — from TC's darkflame_cleft.h.
        a.bosses = {
            210153,  // Ol' Waxbeard
            208743,  // Blazikon
            208745,  // The Candle King
            210797,  // The Darkness (final)
        };
        // Progression waypoints — Darkflame Cleft is a Ringing Deeps
        // candle-kobold cavern with twisted tunnels.
        a.progression_waypoints = {
            {  370.0f,  170.0f,  100.0f },   // entry
            {  447.0f,  259.0f,  100.0f },   // Waxbeard mine
            {  588.0f,  227.0f,   85.0f },   // Blazikon forge
            {  712.0f,  165.0f,   72.0f },   // Candle King throne
            {  812.0f,  205.0f,   60.0f },   // The Darkness depths
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDarkflameCleftScript()
{
    return std::make_unique<DarkflameCleftScript>();
}

} // namespace Playerbot

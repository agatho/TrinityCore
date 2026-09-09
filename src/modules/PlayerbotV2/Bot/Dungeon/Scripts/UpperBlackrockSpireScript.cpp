// UpperBlackrockSpireScript — Upper Blackrock Spire (map 1358, WoD revamp 90-100).
// 5-man revamp of the classic raid wing.
//   * Orebender Gor'ashan — Conductive Pylons + Energy Discharge.
//   * Kyrak — Plague Wave + adds.
//   * Commander Tharbek — Reinforcements + Heavy Crash.
//   * Ragewing the Untamed — Burning Lung + Roaring Flame.
//   * Warlord Zaela (final) — Mounted Charge + Drake Adds.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UpperBlackrockSpireScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1358; }
    char const* name() const override { return "upper_blackrock_spire"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            76801,   // Ragewing Whelp (Ragewing adds)
            76179,   // Black Iron Grunt (Tharbek reinforcement waves)
            77180,   // Emberscale Ironflight (Zaela drake adds)
        };
        a.mandatory_interrupt_spells = {
            156238,  // Conductive Discharge (Gor'ashan)
            155080,  // Plague Wave (Kyrak)
            155670,  // Burning Lung (Ragewing)
            154989,  // Heavy Crash (Tharbek)
            165290,  // Roaring Flame (Ragewing)
        };
        a.cc_priority_entries = {
            76157,   // Black Iron Leadbelcher (ranged, Tharbek waves)
            76179,   // Black Iron Grunt (Tharbek waves)
        };
        a.dangerous_auras = {
            156238,  // Conductive Discharge zone
            155080,  // Plague Wave pool
            154989,  // Heavy Crash ground
            165290,  // Roaring Flame zone
            // Progression waypoints — Upper Blackrock Spire (WoD remake)
            // is a vertical Iron Horde stronghold with drake event.
            // (waypoint defs below outside the aura list)
        };
        // Boss progression — UBRS revamp has 5 encounters.
        a.bosses = {
            76413,  // Orebender Gor'ashan
            76021,  // Kyrak
            79912,  // Commander Tharbek
            76585,  // Ragewing the Untamed
            77120,  // Warlord Zaela (final)
        };
        // Progression waypoints — UBRS (WoD revamp) is a vertical
        // Iron Horde stronghold with drake-mount finale.
        a.progression_waypoints = {
            { -82.0f,  -422.0f,   97.0f },   // entry
            { -83.0f,  -526.0f,   97.0f },   // Gor'ashan forge
            {  47.0f,  -636.0f,   77.0f },   // Kyrak alchemy
            {  61.0f,  -494.0f,  168.0f },   // Tharbek arena
            { 167.0f,  -480.0f,  223.0f },   // Ragewing rookery
            { 100.0f,  -417.0f,  254.0f },   // Zaela's throne
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUpperBlackrockSpireScript()
{
    return std::make_unique<UpperBlackrockSpireScript>();
}

} // namespace Playerbot

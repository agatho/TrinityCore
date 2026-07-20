// WaycrestManorScript — Waycrest Manor (map 1862, BfA 110-120).
// Drust-themed haunted manor. Bosses: Heartsbane Triad, Soulbound
// Goliath, Raal the Gluttonous, Lord & Lady Waycrest, Gorak Tul.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/KulTiras/WaycrestManor/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class WaycrestManorScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1862; }
    char const* name() const override { return "waycrest_manor"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            131611,  // Witch Sister (Heartsbane Triad)
            131626,  // Heartsbane Soulcharmer
        };
        a.mandatory_interrupt_spells = {
            // Heartsbane Triad
            260701,  // Bramble Bolt
            260697,  // Bramble Bolt Enhanced
            260741,  // Jagged Nettles
            260700,  // Ruinous Bolt
            260696,  // Ruinous Bolt Enhanced
            260699,  // Soul Bolt
            260698,  // Soul Bolt Enhanced
            260907,  // Soul Manipulation
            260773,  // Dire Ritual
            // Lord & Lady Waycrest
            261438,  // Wasting Strike
            268281,  // Wracking Bolt
            268278,  // Wracking Chord Selector
        };
        a.cc_priority_entries = {
            131611,
            131626,
        };
        a.dangerous_auras = {
            // Heartsbane (auras applied to the boss)
            268122,  // Aura of Thorns
            // Progression waypoints — Waycrest Manor is a multi-wing
            // mansion with kitchen + library + chapel + crypt branches
            // requiring all 3 wing bosses dead before Waycrests unlock.
            // (waypoints defined below outside this aura list)
            268088,  // Aura of Dread
            268086,  // Aura of Dread Damage
            268077,  // Aura of Apathy
            268080,  // Aura of Apathy Debuff
            261265,  // Ironbark Shield
            261266,  // Runic Ward
            261264,  // Soul Armor
            260702,  // Unstable Runic Mark Damage
            // Lord & Lady
            261440,  // Virulent Pathogen Damage
            261441,  // Virulent Pathogen Infect Area
            261447,  // Putrid Vitality
            268385,  // Contagious Remnants
            268306,  // Discordant Cadenza
            268308,  // Discordant Cadenza Damage
            268271,  // Wracking Chord Damage
        };
        // Boss progression — entries from TC's waycrest_manor.h.
        // Heartsbane Triad is a multi-boss encounter (Briar + Malady +
        // Solena). Note: TC has both BOSS_RAAL_THE_GLUTTONOUS and
        // BOSS_GORAK_TUL pointing at 131863 — almost certainly a TC bug,
        // but using as-is since the bot just scans for any alive entry.
        a.bosses = {
            131825,  // Sister Briar (Heartsbane Triad)
            131823,  // Sister Malady (Heartsbane Triad)
            131824,  // Sister Solena (Heartsbane Triad)
            131667,  // Soulbound Goliath
            131863,  // Raal the Gluttonous / Gorak Tul (shared in TC data)
            131545,  // Lady Waycrest
            131527,  // Lord Waycrest
        };
        // Progression waypoints — Waycrest Manor has 3 wing branches
        // (Kitchen / Library / Chapel) that must all complete before
        // Lord+Lady. Bots default to clockwise: Kitchen → Library →
        // Chapel → main hall.
        a.progression_waypoints = {
            { -10037.0f,  5460.0f,  35.0f },   // entry foyer
            { -10117.0f,  5429.0f,  20.0f },   // Kitchen (Raal)
            { -10025.0f,  5396.0f,  35.0f },   // Library (Soulbound)
            {  -9913.0f,  5470.0f,  21.0f },   // Chapel (Triad)
            {  -9974.0f,  5410.0f,  35.0f },   // main hall (Waycrests)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeWaycrestManorScript()
{
    return std::make_unique<WaycrestManorScript>();
}

} // namespace Playerbot

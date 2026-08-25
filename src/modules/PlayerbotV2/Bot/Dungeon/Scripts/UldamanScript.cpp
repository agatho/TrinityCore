// UldamanScript — Uldaman (map 70, vanilla 35-45).
// Titan-themed dwarf dungeon in Badlands. Bosses: Baelog (+ Eric &
// Olaf trio), Revelosh, Ironaya, Obsidian Sentinel, Ancient Stone
// Keeper, Galgann Firehammer, Grimlok, Archaedas.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/Uldaman/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class UldamanScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 70; }
    char const* name() const override { return "uldaman"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            7309,   // Earthen Sentinel
            4854,   // Stone Keeper
        };
        a.mandatory_interrupt_spells = {
            // Archaedas (final boss)
            6524,   // Ground Tremor
            10347,  // Archaedas Awaken
            10340,  // Boss Aggro
            10258,  // Awaken Vault Walker
            10252,  // Awaken Earthen Guardian
            // Ironaya
            8374,   // Arcing Smash
            10101,  // Knock Away
            11876,  // War Stomp
        };
        a.cc_priority_entries = {
            4855,
        };
        a.dangerous_auras = {
            // Stone Keepers / Walkers
            9874,   // Self Destruct
        };
        // Boss progression — entries from TC's uldaman.h + WoWHead.
        a.bosses = {
            2748,   // Baelog (with Eric/Olaf — Lost Dwarves)
            6910,   // Revelosh
            7228,   // Ironaya
            7023,   // Obsidian Sentinel
            7206,   // Ancient Stone Keeper
            7291,   // Galgann Firehammer
            4854,   // Grimlok
            2748,   // Archaedas (final — note 2748 collision with Baelog
                    //   in source data; TC may differentiate elsewhere)
        };
        // Progression waypoints — Uldaman (vanilla) is a long linear
        // descent: entry shaft → Lost Dwarves camp → mining tunnels →
        // Revelosh hall → Ironaya gate → Stone Keeper hall → Galgann's
        // forge → Grimlok's lair → Archaedas's vault (final).
        a.progression_waypoints = {
            { -228.0f,  56.0f,  -45.0f },   // entry
            { -190.0f,  37.0f,  -77.0f },   // Lost Dwarves camp
            { -134.0f,  18.0f,  -78.0f },   // mining tunnels
            { -190.0f, 226.0f,  -42.6f },   // Revelosh hall
            { -218.0f, 305.0f,  -56.0f },   // Ironaya gate
            { -191.0f, 314.0f,  -71.2f },   // Stone Keeper hall
            {  -68.0f, 369.0f,  -94.0f },   // Galgann's forge
            {   80.0f, 327.0f,  -98.5f },   // Grimlok's lair
            {   75.0f, 153.0f,  -88.0f },   // Archaedas vault
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeUldamanScript()
{
    return std::make_unique<UldamanScript>();
}

} // namespace Playerbot

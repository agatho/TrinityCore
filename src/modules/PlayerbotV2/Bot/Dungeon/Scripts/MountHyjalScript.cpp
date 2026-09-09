// MountHyjalScript — Battle for Mount Hyjal raid (map 534, TBC 25-man).
// 5 bosses + waves of adds: Rage Winterchill, Anetheron, Kaz'rogal, Azgalor,
// Archimonde.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/CavernsOfTime/BattleForMountHyjal/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class MountHyjalScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 534; }
    char const* name() const override { return "battle_for_mount_hyjal"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            17818,  // Towering Infernal (Anetheron summon)
            17905,  // Banshee
            17899,  // Shadowy Necromancer
            17907,  // Frost Wyrm
            17898,  // Abomination
            17897,  // Crypt Fiend
            17906,  // Gargoyle
            18095,  // Doomfire (Archimonde)
            18104,  // Doomfire Spirit
        };
        a.mandatory_interrupt_spells = {
            // Winterchill
            31249,  // Icebolt
            31250,  // Frost Nova
            31258,  // Death and Decay
            // Anetheron
            31306,  // Carrion Swarm
            31298,  // Sleep
            // Kaz'rogal
            31436,  // Cleave
            31480,  // War Stomp
            // Azgalor
            31340,  // Rain of Fire
            31347,  // Doom
            31344,  // Howl of Azgalor
            31345,  // Cleave
            31406,  // Cripple
            31408,  // War Stomp
            // Archimonde
            31984,  // Finger of Death
            32014,  // Air Burst
            31972,  // Grip of the Legion
            31970,  // Fear
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Winterchill
            31256,  // Frost Armor
            31258,  // Death and Decay
            // Anetheron
            31299,  // Inferno (infernal summon — also boss aura)
            31303,  // Immolation
            31298,  // Sleep
            38196,  // Vampiric Aura
            // Kaz'rogal
            31447,  // Mark of Kaz'rogal
            31463,  // Mark Damage
            // Azgalor
            31347,  // Doom
            31340,  // Rain of Fire
            31344,  // Howl of Azgalor
            // Archimonde
            31945,  // Doomfire (ground effect)
        };
        // Boss progression — Battle for Mount Hyjal has 5 wave bosses.
        // NPC entries from TC's BattleForMountHyjal boss scripts.
        a.bosses = {
            17767,  // Rage Winterchill
            17808,  // Anetheron
            17888,  // Kaz'rogal
            17842,  // Azgalor
            17968,  // Archimonde (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeMountHyjalScript()
{
    return std::make_unique<MountHyjalScript>();
}

} // namespace Playerbot

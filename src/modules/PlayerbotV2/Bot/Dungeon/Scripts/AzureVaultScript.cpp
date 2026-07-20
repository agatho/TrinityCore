// AzureVaultScript — The Azure Vault (map 2515, DF 60-70).
// Azure Span dungeon. 4 bosses: Leymor, Azureblade, Telash Greywing,
// Umbrelskul.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/DragonIsles/AzureVault/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class AzureVaultScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2515; }
    char const* name() const override { return "azure_vault"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            193674,  // Awakened Bramble (Leymor)
            193791,  // Telash Frost add
        };
        a.mandatory_interrupt_spells = {
            // Leymor
            375729,  // Stasis
            375749,  // Arcane Eruption
            374364,  // Ley Line Sprouts
            374720,  // Consuming Stomp
            374567,  // Explosive Brand
            374789,  // Infused Strike
            375591,  // Sappy Burst
            375596,  // Erratic Growth Channel
            375652,  // Wild Eruption
            // Telash Greywing
            386781,  // Frost Bomb Cast
            387151,  // Icy Devastator
            387928,  // Absolute Zero Cast
        };
        a.cc_priority_entries = {
        };
        a.dangerous_auras = {
            // Leymor
            374731,  // Consuming Stomp Damage
            386660,  // Erupting Fissure
            374570,  // Explosive Brand Damage
            388654,  // Volatile Sapling
            374161,  // Ley Line Sprout AreaTrigger
            375738,  // Stasis Ritual Missile
            375650,  // Wild Eruption Missile
            // Telash
            386881,  // Frost Bomb Aura
            386910,  // Frost Bomb Damage
            387149,  // Frozen Ground AreaTrigger
            388008,  // Absolute Zero Damage
            388065,  // Vault Rune AT Aura
        };
        // Boss progression — entries from TC's azure_vault.h.
        a.bosses = {
            186644,  // Leymor
            199614,  // Telash Greywing
            186739,  // Umbrelskul (or Azureblade, depending on path)
            186738,  // Azureblade
        };
        // Progression waypoints — Azure Vault is an Azure Span dragon
        // archive with vertical stair levels.
        a.progression_waypoints = {
            { -3850.0f, -7170.0f,  -800.0f },   // entry
            { -3700.0f, -7048.0f,  -800.0f },   // Leymor nest
            { -3603.0f, -6961.0f,  -780.0f },   // Telash spire
            { -3500.0f, -6890.0f,  -760.0f },   // Umbrelskul vault
            { -3400.0f, -6820.0f,  -740.0f },   // Azureblade chamber
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeAzureVaultScript()
{
    return std::make_unique<AzureVaultScript>();
}

} // namespace Playerbot

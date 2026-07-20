// VaultOfArchavonScript — Vault of Archavon raid (map 624, WotLK 10/25 PvP-loot).
// 4 bosses: Archavon, Emalon, Koralon, Toravon.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Northrend/VaultOfArchavon/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class VaultOfArchavonScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 624; }
    char const* name() const override { return "vault_of_archavon"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            33998,  // Tempest Minion (Emalon — kill before overcharge stacks)
        };
        a.mandatory_interrupt_spells = {
            // Archavon
            58678,  // Rock Shards
            58960,  // Crushing Leap
            58663,  // Stomp
            58666,  // Impale
            // Emalon
            64213,  // Chain Lightning
            64216,  // Lightning Nova
            64218,  // Overcharge
            64363,  // Shock
            // Koralon
            66665,  // Burning Breath
            66725,  // Meteor Fists
            66684,  // Flame Cinder A
            // Toravon
            72090,  // Freezing Ground
            72034,  // Whiteout
            71993,  // Frozen Mallet
        };
        a.cc_priority_entries = {};
        a.dangerous_auras = {
            // Archavon
            58695, 58696,  // Rock Shards Damage L+R
            // Emalon
            64217,  // Overcharged (stacking minion buff)
            64219,  // Overcharged Blast
            64216,  // Lightning Nova
            // Koralon
            66665,  // Burning Breath
            66721,  // Burning Fury
            66765,  // Meteor Fists Damage
            66809,  // FW Meteor Fists Damage
            66681, 66684,  // Flame Cinder
            // Toravon
            72090,  // Freezing Ground
            72091,  // Frozen Orb
            72081,  // Frozen Orb Damage
            72067,  // Frozen Orb Aura
        };
        // Boss progression — Vault of Archavon has 4 encounters.
        a.bosses = {
            31125,  // Archavon the Stone Watcher
            33993,  // Emalon the Storm Watcher
            35013,  // Koralon the Flame Watcher
            38433,  // Toravon the Ice Watcher (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeVaultOfArchavonScript()
{
    return std::make_unique<VaultOfArchavonScript>();
}

} // namespace Playerbot

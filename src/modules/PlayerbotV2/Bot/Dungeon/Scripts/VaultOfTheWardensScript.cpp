// VaultOfTheWardensScript — Vault of the Wardens (map 1493, Legion 100-110).
// Wardens prison dungeon — Illidari escape mid-Legion.
//   * Tirathon Saltheril — Eye Beam telegraph + adds.
//   * Inquisitor Tormentorum — Tormenting Strikes + Fel Mortar.
//   * Ash'Golm — Volcanic Tantrum (zone) + Smouldering Aura.
//   * Glazer (Eye) — Petrifying Gaze (look away mechanic).
//   * Cordana Felsong (final) — Mind Shatter + Shadow Bolt Volley.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class VaultOfTheWardensScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 1493; }
    char const* name() const override { return "vault_of_the_wardens"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            96587,   // Felsworn Infester (Tormentorum freed prisoner)
            96584,   // Immoliant Fury (Tormentorum freed prisoner)
            100351,  // Avatar of Vengeance (Cordana)
        };
        a.mandatory_interrupt_spells = {
            193364,  // Mind Shatter (Cordana)
            191374,  // Eye Beam (Tirathon)
            192399,  // Volcanic Tantrum (Ash'Golm)
            193380,  // Shadow Bolt Volley (Cordana)
            191576,  // Petrifying Gaze (Glazer)
        };
        a.cc_priority_entries = {
            96584,   // Immoliant Fury
            96480,   // Viletongue Belcher
            96587,   // Felsworn Infester
        };
        a.dangerous_auras = {
            191576,  // Petrifying Gaze cone
            192399,  // Volcanic Tantrum zone
            191382,  // Crystalline Ground (Ash'Golm)
            193395,  // Shadow Marked (Cordana)
        };
        // Boss progression — Vault of the Wardens has 5 encounters.
        // Progression waypoints — boss spawn positions from world.creature
        // (map 1493), in encounter order. Pathfinder routes the corridors.
        a.progression_waypoints = {
            { 4325.1f, -451.5f,  280.8f },   // Tirathon Saltheril
            { 4450.8f, -393.6f,  126.1f },   // Inquisitor Tormentorum
            { 4239.1f, -451.3f,  105.9f },   // Ash'golm
            { 4451.0f, -673.3f,  116.2f },   // Glazer
            { 4033.2f, -297.2f, -281.5f },   // Cordana Felsong
        };
        a.bosses = {
            95885,   // Tirathon Saltheril
            96015,   // Inquisitor Tormentorum
            95886,   // Ash'golm
            95887,   // Glazer
            95888,   // Cordana Felsong (final)
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeVaultOfTheWardensScript()
{
    return std::make_unique<VaultOfTheWardensScript>();
}

} // namespace Playerbot

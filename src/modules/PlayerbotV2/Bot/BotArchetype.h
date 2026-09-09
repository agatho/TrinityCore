// BotArchetype - Per-bot "play archetype": WHAT a bot does (role/activity
// emphasis + economic behavior) and WHEN it intends to be online (target
// session length). Distinct from BotPersonality, which is HOW a bot plays
// (skill / aggression / verbosity / risk). Together they make the fleet
// heterogeneous: a casual soloer, a hardcore raider, a social guildie, a
// gatherer/AH-flipper, a PvPer, and an altoholic explorer all coexist —
// the basis of a believable living server and of economy / role variety.
//
// Stored in playerbot_v2_character.archetype_id (migration 0012). Rolled
// deterministically from the per-bot rng_seed on first spawn (so the same
// bot always rolls the same archetype across restarts) and written back.
//
// Data-driven: the curated archetype table lives in BotArchetype.cpp as a
// static array so role/activity/econ/session values are tunable in one
// place without touching the roll logic.

#pragma once

#include "BotPersonality.h"   // ActivityPref categories — activity_weights aligns to them
#include "BotTypes.h"
#include <array>

namespace Playerbot {

// Economic posture. Drives gather/sell/AH behavior (future consumers in the
// economy subsystem). Hoarder banks mats and rarely sells; Balanced sells
// surplus at vendors / lists occasionally; Reseller actively flips on the AH.
enum class EconProfile : uint8 { Hoarder = 0, Balanced = 1, Reseller = 2 };

// Number of activity-weight slots. Aligned to the BotPersonality activity
// preference categories the fleet reasons about: Solo / Group / PvP /
// Profession / Social. (ActivityPref also defines Housing/All bits, but the
// weighted-activity model only spreads across these five primary play modes.)
inline constexpr uint8 kArchetypeActivityCount = 5;

// Stable indices into BotArchetype::activity_weights. Kept in lock-step with
// the ActivityPref bit order so a consumer can map a weight slot back to the
// matching ActivityPref bit when it needs the bitfield form.
namespace ArchetypeActivity {
    constexpr uint8 Solo       = 0;
    constexpr uint8 Group      = 1;
    constexpr uint8 Pvp        = 2;
    constexpr uint8 Profession = 3;
    constexpr uint8 Social     = 4;
}

struct BotArchetype
{
    // Index into the curated table (kArchetypeTable). 0 = CasualSolo, the
    // default for an un-rolled / un-migrated bot (matches the SQL column
    // default), so a bot that has never been rolled reads as a sensible
    // casual soloer rather than garbage.
    uint8 archetype_id = 0;

    // Role emphasis: [Tank, Healer, Dps]. Sums to ~1.0. Used by the
    // population rebalancer to pick which hybrid bots to respec into a
    // starved tank/healer role (highest-affinity first) and by future
    // role-selection logic. A pure-DPS archetype has {0,0,1}.
    std::array<float, 3> role_affinity{ 0.f, 0.f, 1.f };

    // Activity emphasis, indexed by ArchetypeActivity::*. Sums to ~1.0.
    // Idle rules can bias toward the bot's dominant activity (e.g. a
    // GathererFlipper spends most of its time on Profession, a PvPer on
    // Pvp) without a thread crossing — the dominant slot is mirrored into
    // the snapshot's ArchetypeState.
    std::array<float, kArchetypeActivityCount> activity_weights{
        0.6f, 0.2f, 0.05f, 0.1f, 0.05f };

    EconProfile econ_profile = EconProfile::Balanced;

    // Intended play-session length in minutes. The session-rhythm logout
    // layer (documented followup; not yet wired) will use this with
    // cumulative_session_minutes to decide when a bot "logs off for the
    // night". Stored now so the data is available when that layer lands.
    uint16 target_session_minutes = 90;

    // Index of the highest activity_weights slot (ArchetypeActivity::*).
    // Convenience for consumers that only want the dominant activity.
    uint8 dominant_activity() const
    {
        uint8 best = 0;
        for (uint8 i = 1; i < kArchetypeActivityCount; ++i)
            if (activity_weights[i] > activity_weights[best]) best = i;
        return best;
    }
};

// Named archetype ids — these MUST match the row order of kArchetypeTable in
// BotArchetype.cpp (archetype_id == table index). Persisted in the DB, so
// never reorder existing entries; append new archetypes at the end.
enum class ArchetypeId : uint8
{
    CasualSolo        = 0,
    HardcoreRaider    = 1,
    SocialGuildie     = 2,
    GathererFlipper   = 3,
    PvPer             = 4,
    AltoholicExplorer = 5,
    Count
};

// Number of curated archetypes (table row count). Kept in sync with the
// static table via a static_assert in BotArchetype.cpp.
inline constexpr uint8 kArchetypeCount = static_cast<uint8>(ArchetypeId::Count);

// Look up a curated archetype by id. Out-of-range ids clamp to CasualSolo
// (id 0) so a corrupt / future-migrated DB value never reads garbage.
BotArchetype ArchetypeById(uint8 archetype_id);

// Human-readable name for an archetype id (diagnostics / .playerbot inspect).
char const* ArchetypeName(uint8 archetype_id);

// Deterministic weighted roll from a per-bot seed (use SeedForBot(id)). The
// distribution roughly mimics a real population: most bots are casual
// soloers / social guildies, with a smaller tail of hardcore raiders,
// gatherer-flippers, PvPers, and altoholics. Same seed always returns the
// same archetype so a bot's identity is reproducible across restarts.
// Takes the full 64-bit seed (SeedForBot returns uint64) to match
// RandomPersonality and preserve all seed entropy.
BotArchetype RollArchetype(uint64 rng_seed);

} // namespace Playerbot

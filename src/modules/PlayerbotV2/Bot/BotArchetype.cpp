#include "BotArchetype.h"

#include <cstdint>

namespace Playerbot {

namespace {

// Curated archetype table. Row index == archetype_id == ArchetypeId enum
// value — NEVER reorder; append only (the id is persisted in the DB).
//
// role_affinity     = {Tank, Healer, Dps}            (sums ~1.0)
// activity_weights  = {Solo, Group, Pvp, Prof, Social} (sums ~1.0, ArchetypeActivity order)
// econ_profile      = economic posture
// target_session    = intended minutes online per session
//
// `weight` is the relative roll weight used by RollArchetype — it is NOT a
// field of BotArchetype, just the population distribution shape.
struct ArchetypeRow
{
    BotArchetype proto;
    uint32       weight;
    char const*  name;
};

constexpr ArchetypeRow kArchetypeTable[] = {
    // CasualSolo — the fleet bulk. Mostly solos, a little grouping, some
    // profession dabbling. Short-to-medium sessions. Balanced economy.
    { BotArchetype{
        /*archetype_id*/   static_cast<uint8>(ArchetypeId::CasualSolo),
        /*role_affinity*/  { 0.10f, 0.10f, 0.80f },
        /*activity*/       { 0.55f, 0.20f, 0.05f, 0.15f, 0.05f },
        /*econ*/           EconProfile::Balanced,
        /*session_min*/    60 },
      /*weight*/ 34, "CasualSolo" },

    // HardcoreRaider — group/raid focused, high tank+heal willingness, long
    // sessions, hoards consumables/mats for raids.
    { BotArchetype{
        static_cast<uint8>(ArchetypeId::HardcoreRaider),
        { 0.30f, 0.25f, 0.45f },
        { 0.10f, 0.70f, 0.05f, 0.10f, 0.05f },
        EconProfile::Hoarder,
        180 },
      14, "HardcoreRaider" },

    // SocialGuildie — chat/guild oriented, moderate grouping, balanced
    // economy, medium sessions. Drives the "living guild" feel.
    { BotArchetype{
        static_cast<uint8>(ArchetypeId::SocialGuildie),
        { 0.12f, 0.18f, 0.70f },
        { 0.20f, 0.30f, 0.05f, 0.10f, 0.35f },
        EconProfile::Balanced,
        90 },
      18, "SocialGuildie" },

    // GathererFlipper — profession/economy focused, active AH reseller,
    // mostly solo, medium-long sessions. The economy's supply + price-setting
    // engine.
    { BotArchetype{
        static_cast<uint8>(ArchetypeId::GathererFlipper),
        { 0.05f, 0.05f, 0.90f },
        { 0.40f, 0.05f, 0.02f, 0.48f, 0.05f },
        EconProfile::Reseller,
        120 },
      12, "GathererFlipper" },

    // PvPer — battleground/world-PvP focused, pure DPS lean, medium sessions,
    // balanced economy (buys consumables, sells PvP drops).
    { BotArchetype{
        static_cast<uint8>(ArchetypeId::PvPer),
        { 0.08f, 0.12f, 0.80f },
        { 0.15f, 0.15f, 0.60f, 0.05f, 0.05f },
        EconProfile::Balanced,
        90 },
      12, "PvPer" },

    // AltoholicExplorer — wide solo exploration + profession dabbling, short
    // bursty sessions (the "log in, poke around, log off" player). Hoards
    // because alts squirrel away mats across characters.
    { BotArchetype{
        static_cast<uint8>(ArchetypeId::AltoholicExplorer),
        { 0.10f, 0.10f, 0.80f },
        { 0.60f, 0.10f, 0.05f, 0.20f, 0.05f },
        EconProfile::Hoarder,
        45 },
      10, "AltoholicExplorer" },
};

static_assert(sizeof(kArchetypeTable) / sizeof(kArchetypeTable[0]) == kArchetypeCount,
              "kArchetypeTable row count must equal kArchetypeCount / ArchetypeId::Count");

// splitmix64 mix — same construction as BotPersonality / BotRng so the roll
// is stable and well-dispersed. Salted so the archetype roll does not
// correlate with the personality rolls drawn from the same per-bot seed.
uint64_t mix(uint64_t seed, uint64_t salt)
{
    uint64_t z = (seed + salt + 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

} // anonymous

BotArchetype ArchetypeById(uint8 archetype_id)
{
    if (archetype_id >= kArchetypeCount)
        return kArchetypeTable[0].proto;   // clamp to CasualSolo
    return kArchetypeTable[archetype_id].proto;
}

char const* ArchetypeName(uint8 archetype_id)
{
    if (archetype_id >= kArchetypeCount)
        return "(unknown)";
    return kArchetypeTable[archetype_id].name;
}

BotArchetype RollArchetype(uint64 rng_seed)
{
    uint32 total = 0;
    for (auto const& row : kArchetypeTable)
        total += row.weight;
    if (total == 0)
        return kArchetypeTable[0].proto;   // degenerate guard

    // Salt 0xA17 ("ART") keeps this stream independent of the personality
    // dimension salts (0x10A..0x60A in BotPersonality.cpp).
    const uint32 roll = static_cast<uint32>(mix(rng_seed, 0xA17ULL) % total);

    uint32 acc = 0;
    for (auto const& row : kArchetypeTable)
    {
        acc += row.weight;
        if (roll < acc)
            return row.proto;
    }
    return kArchetypeTable[kArchetypeCount - 1].proto;   // fp-safety fallback
}

} // namespace Playerbot

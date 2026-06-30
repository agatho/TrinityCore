#include "BotPersonality.h"

#include <cstdint>

namespace Playerbot {

BotPersonality DefaultPersonality()
{
    return BotPersonality{};   // Defaults declared in the struct
}

namespace {

// Independent splitmix64 stream per dimension: seed mixed with a salt unique
// to each personality field so picks across dimensions don't correlate. The
// per-bot seed produces stable picks across server restarts.
uint64_t mix(uint64_t seed, uint64_t salt)
{
    uint64_t z = (seed + salt + 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint32_t pick_pct(uint64_t seed, uint64_t salt)   // 0..99
{
    return uint32_t(mix(seed, salt) % 100ull);
}

} // anonymous

BotPersonality RandomPersonality(uint64_t seed)
{
    BotPersonality p;     // start from defaults

    // Aggression: Passive 5 / Defensive 25 / Normal 50 / Aggressive 20.
    // Drives engage HP gate, engage range, level band (see State_Idle.cpp).
    {
        const uint32_t r = pick_pct(seed, 0x10A);
        p.aggression = r < 5  ? Aggression::Passive
                     : r < 30 ? Aggression::Defensive
                     : r < 80 ? Aggression::Normal
                              : Aggression::Aggressive;
    }

    // RiskTolerance: Cautious 10 / Careful 30 / Normal 40 / Reckless 20.
    // Drives wander step distance + OoC consume thresholds.
    {
        const uint32_t r = pick_pct(seed, 0x20A);
        p.risk_tolerance = r < 10 ? RiskTolerance::Cautious
                          : r < 40 ? RiskTolerance::Careful
                          : r < 80 ? RiskTolerance::Normal
                                   : RiskTolerance::Reckless;
    }

    // Verbosity: Silent 5 / Terse 15 / Normal 60 / Chatty 15 / Roleplay 5.
    // Roleplay enables ambient inn emotes; others tune chat-response length.
    {
        const uint32_t r = pick_pct(seed, 0x30A);
        p.verbosity = r < 5  ? Verbosity::Silent
                    : r < 20 ? Verbosity::Terse
                    : r < 80 ? Verbosity::Normal
                    : r < 95 ? Verbosity::Chatty
                             : Verbosity::Roleplay;
    }

    // Politeness: Rude 10 / Neutral 70 / Polite 20.
    {
        const uint32_t r = pick_pct(seed, 0x40A);
        p.politeness = r < 10 ? Politeness::Rude
                      : r < 80 ? Politeness::Neutral
                              : Politeness::Polite;
    }

    // Loyalty: Flighty 15 / Normal 70 / Devoted 15.
    {
        const uint32_t r = pick_pct(seed, 0x50A);
        p.loyalty = r < 15 ? Loyalty::Flighty
                  : r < 85 ? Loyalty::Normal
                           : Loyalty::Devoted;
    }

    // SkillTier: Novice 15 / Competent 60 / Expert 20 / Elite 5.
    // Competent is the bulk of the population; Elite is the long tail.
    {
        const uint32_t r = pick_pct(seed, 0x60A);
        p.skill_tier = r < 15 ? SkillTier::Novice
                     : r < 75 ? SkillTier::Competent
                     : r < 95 ? SkillTier::Expert
                              : SkillTier::Elite;
    }

    // Other dims (activity_pref, response_delay_ms, response_jitter_ms,
    // mistake_rate) keep the struct defaults — they tune chat/whisper
    // behaviors that don't yet have downstream consumers in the V2 module.
    return p;
}

} // namespace Playerbot

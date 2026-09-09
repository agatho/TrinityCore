// BotPersonality - Static-ish per-bot tuning. Stored in playerbot_v2_personality.

#pragma once

#include "BotTypes.h"

namespace Playerbot {

enum class SkillTier : uint8 { Novice = 0, Competent = 1, Expert = 2, Elite = 3 };
enum class Verbosity : uint8 { Silent = 0, Terse = 1, Normal = 2, Chatty = 3, Roleplay = 4 };
enum class Aggression : uint8 { Passive = 0, Defensive = 1, Normal = 2, Aggressive = 3 };
enum class RiskTolerance : uint8 { Cautious = 0, Careful = 1, Normal = 2, Reckless = 3 };
enum class Politeness : uint8 { Rude = 0, Neutral = 1, Polite = 2 };
enum class Loyalty : uint8 { Flighty = 0, Normal = 1, Devoted = 2 };

// Bitfield over activity preferences.
namespace ActivityPref {
    constexpr uint8 Solo       = 1 << 0;
    constexpr uint8 Group      = 1 << 1;
    constexpr uint8 Pvp        = 1 << 2;
    constexpr uint8 Profession = 1 << 3;
    constexpr uint8 Social     = 1 << 4;
    constexpr uint8 Housing    = 1 << 5;
    constexpr uint8 All        = 0x3F;
}

struct BotPersonality
{
    SkillTier     skill_tier        = SkillTier::Competent;
    Verbosity     verbosity         = Verbosity::Normal;
    Aggression    aggression        = Aggression::Normal;
    RiskTolerance risk_tolerance    = RiskTolerance::Normal;
    Politeness    politeness        = Politeness::Neutral;
    Loyalty       loyalty           = Loyalty::Normal;
    uint8         activity_pref     = ActivityPref::All;
    uint16        response_delay_ms = 300;
    uint16        response_jitter_ms = 100;
    uint8         mistake_rate      = 2;     // percent
};

// Default personality for warm-pool bots; stored personalities override.
BotPersonality DefaultPersonality();

// Per-bot deterministic personality picked from a seed (use SeedForBot(id)).
// Distributions roughly mimic a real player population: most bots are
// Normal across the board, with a long tail of Aggressive / Cautious /
// Roleplay / etc. Same seed always returns the same personality so a bot's
// behavior is reproducible across server restarts. Used by auto-spawn
// (where there's no operator to set personality manually) and by the
// .playerbot mark code path so a freshly-marked existing character also
// gets a flavored personality (operator can override later via whisper).
BotPersonality RandomPersonality(uint64_t seed);

} // namespace Playerbot

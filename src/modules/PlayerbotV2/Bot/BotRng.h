// BotRng - Per-bot deterministic RNG. Seeded from BotId so the same bot in
// the same snapshot produces the same intent (REQUIREMENTS.md §2.3).

#pragma once

#include "BotTypes.h"
#include <cstdint>

namespace Playerbot {

class BotRng
{
public:
    BotRng() : state_(0xC0FFEE) {}
    explicit BotRng(uint64_t seed) : state_(seed ? seed : 0xC0FFEE) {}

    // splitmix64 — fast, good distribution, easy to reason about for tests.
    uint64_t next()
    {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // Uniform integer in [lo, hi).
    int32_t int_range(int32_t lo, int32_t hi)
    {
        if (hi <= lo) return lo;
        const uint64_t span = static_cast<uint64_t>(hi - lo);
        return lo + static_cast<int32_t>(next() % span);
    }

    // [0,1) double.
    double unit()
    {
        return (next() >> 11) * (1.0 / 9007199254740992.0);
    }

    // Bernoulli — true with probability `p` in [0,1].
    bool chance(double p) { return unit() < p; }

    // Pct chance, 0..100.
    bool chance_pct(uint8_t pct) { return int_range(0, 100) < static_cast<int32_t>(pct); }

    uint64_t state() const { return state_; }

private:
    uint64_t state_;
};

inline uint64_t SeedForBot(BotId id)
{
    // Mixing constant from xxh3 — gives good per-bot dispersion.
    return id * 0x9FB21C651E98DF25ULL ^ 0x6A09E667F3BCC908ULL;
}

} // namespace Playerbot

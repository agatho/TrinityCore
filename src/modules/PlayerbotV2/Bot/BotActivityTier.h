// BotActivityTier - Snapshot-driven tier transition logic per ARCHITECTURE.md §1.4.

#pragma once

#include "BotTypes.h"

namespace Playerbot {

struct BotSnapshot;  // forward — full type in BotSnapshot.h

// Tick frequency table — lower = more reactive, more CPU. On a 5950X
// with 33% baseline CPU and ~1100 of 1237 bots sitting in Idle tier,
// halving Idle roughly doubles aggregate AI work and lifts quality
// across the fleet (faster BG pivots, smoother movement, snappier
// social timing). Active also bumped a notch since travelling/questing
// bots want quick re-evaluation. Combat unchanged — already at 10 Hz
// (network update cadence).
//
// `Cruise` (300 ms) is the middle band for the dominant long-haul
// population: a solo, open-world bot that is merely travelling or
// questing (moving OR has an objective) but is NOT human-facing
// (unowned, ungrouped-with-real, no real player nearby), NOT path-blocked,
// and NOT casting. Such a bot's movement spline is server-side; the AI
// only needs to issue the next waypoint a touch less often, so it
// tolerates 300 ms. This roughly halves snapshot-build cost for the
// questing fleet that previously pinned every moving/objective bot to
// Active (150 ms). Cruise NEVER ramps to Parked — only Idle does — so a
// travelling bot is never frozen; the moment it blocks / enters combat /
// has a player walk up, the classifier promotes it and set_tier resets
// next_snapshot=now for next-frame reactivity.
//
// `Hibernate` is the "parked" tier: a fully-AFK bot (alive, out of
// combat, stationary, unowned, not grouped-with / near any real player)
// that has classified Idle for N consecutive scheduler passes is ramped
// here so it rebuilds its snapshot at 0.5 Hz instead of 2 Hz. This is
// the single biggest snapshot-build saving at fleet scale (the long
// tail of idle city/inn bots). 2000 ms is deliberately capped at the
// AiWorkerPool staleness guard's 1000 ms ceiling × 2 — a parked bot's
// snapshot can be up to one Parked period old, which only ever causes
// the worker to *skip* that AI tick (stale_skip), never to act on bad
// data or repath; the next scheduler pass re-builds fresh. See
// AiWorkerPool.cpp staleness guard. Any wake event (combat / owner
// control / movement / a real player arriving) demotes the bot back to
// a fast tier via set_tier(), which resets next_snapshot=now so the
// rebuild happens on the very next frame (no reactivity regression).
constexpr Ms TickPeriodFor(ActivityTier t)
{
    switch (t)
    {
        case ActivityTier::Combat:    return Ms{100};
        case ActivityTier::Active:    return Ms{150};   // was 200
        case ActivityTier::Cruise:    return Ms{300};   // solo open-world traveller
        case ActivityTier::Idle:      return Ms{500};   // was 1000
        case ActivityTier::Hibernate: return Ms{2000};  // "Parked" — long-idle AFK
    }
    return Ms{500};
}

// Pure function: given snapshot, return the tier the bot SHOULD be in.
// Caller decides if a transition occurs (with hysteresis to avoid thrash).
ActivityTier ClassifyTier(BotSnapshot const& s);

} // namespace Playerbot

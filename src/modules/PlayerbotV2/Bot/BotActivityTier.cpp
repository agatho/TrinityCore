#include "BotActivityTier.h"
#include "BotSnapshot.h"

namespace Playerbot {

ActivityTier ClassifyTier(BotSnapshot const& s)
{
    // Dead bots are low-frequency: a corpse run / spirit-healer wait does
    // not need 5–10 Hz re-evaluation. (Matches the world-thread classifier
    // in PlayerbotV2.cpp.)
    if (!s.vitals.is_alive)
        return ActivityTier::Idle;

    if (s.vitals.in_combat)
        return ActivityTier::Combat;

    // Anything that demands prompt re-evaluation keeps the fast cadence:
    // active motion (walking a route / fleeing), or being in a group (a
    // grouped bot follows / assists and must stay responsive to the
    // leader). Owner-control and real-player proximity are decided by the
    // world-thread classifier (which owns the per-tick real-player cell
    // set); this snapshot-only function intentionally errs toward Active
    // so the staleness guard never under-estimates a bot's freshness need.
    if (s.movement.is_moving || s.movement.is_swimming || !s.group.group_guid.IsEmpty())
        return ActivityTier::Active;

    // Alive, out of combat, stationary, solo. Idle in a city / inn / safe
    // area is the typical case.
    return ActivityTier::Idle;

    // NOTE: this function never returns Combat-faster-than-warranted and
    // never returns the parked tier (ActivityTier::Hibernate). The parked
    // tier is reached only via the scheduler after N consecutive Idle
    // classifications (see PlayerbotV2.cpp + TickScheduler). It is used by
    // the AiWorkerPool staleness guard, which caps max-age at 1 s anyway,
    // so a parked bot is treated as Idle (500 ms × 3, capped 1 s) there.
}

} // namespace Playerbot

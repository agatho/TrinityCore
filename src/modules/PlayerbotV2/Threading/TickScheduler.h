// TickScheduler - Decides which bots tick this world frame and routes them to
// the AI worker pool. Driven by per-bot ActivityTier and the world tick budget.

#pragma once

#include "Bot/BotTypes.h"
#include <chrono>
#include <unordered_map>
#include <vector>

namespace Playerbot {

class AiWorkerPool;

class TickScheduler
{
public:
    explicit TickScheduler(AiWorkerPool& pool);

    // Called on world thread once per world tick. Selects bots whose next-tick
    // deadline has passed and submits them to the AI worker pool.
    void on_world_tick(Ms now);

    // Bot lifecycle
    void register_bot(BotId id, ActivityTier initial = ActivityTier::Idle);
    void unregister_bot(BotId id);

    // Tier transition (called by AI tick logic via the snapshot publish path).
    //
    // `classified` is the tier the per-tick classifier decided the bot
    // SHOULD be in this frame (see PlayerbotV2.cpp). The scheduler applies
    // an idle→parked ramp on top: a bot classified Idle for
    // kIdleRampToParked consecutive frames is promoted to the parked tier
    // (ActivityTier::Hibernate, 2 s) to slash snapshot-build cost on the
    // long tail of AFK city/inn bots. Any non-Idle classification (combat /
    // active / dead) resets the streak immediately, so a parked bot snaps
    // back to its fast cadence the moment it wakes. On any actual tier
    // change BOTH deadlines reset to `now` (rebuild + tick next frame), so
    // there is no reactivity regression.
    void set_tier(BotId id, ActivityTier classified, Ms now);

    // Snapshot-build cadence gate. Per-bot tier-driven throttle on
    // `BotSnapshotBuilder::Build` so Build is paid at the AI tick rate
    // (Combat 10Hz / Active 5Hz / Idle 1Hz) instead of every world frame
    // (50Hz). Returns true if it's time to rebuild for `id`; false if
    // the throttle is still holding. On true, advances the per-bot
    // deadline. Unregistered bots return false.
    bool should_build_snapshot(BotId id, Ms now);

    // Diagnostics
    size_t bot_count() const;

private:
    // Consecutive Idle classifications required before a bot is ramped to
    // the parked tier (ActivityTier::Hibernate). At Idle's 500 ms cadence
    // this is ~2.5 s of uninterrupted idleness before snapshot-build drops
    // to 0.5 Hz — short enough to reclaim the AFK long tail quickly, long
    // enough that a bot pausing mid-activity isn't parked prematurely.
    static constexpr uint8 kIdleRampToParked = 5;

    struct BotSchedule
    {
        ActivityTier tier         = ActivityTier::Idle;
        Ms           next_tick{0};
        Ms           next_snapshot{0};
        // Count of consecutive frames the classifier returned Idle for this
        // bot. Reset to 0 on any non-Idle classification. When it reaches
        // kIdleRampToParked the bot is promoted to the parked tier. Lives
        // here (per-bot schedule) rather than on BotAI so the cadence layer
        // owns all of its own state.
        uint8        idle_streak  = 0;
    };

    AiWorkerPool& pool_;
    std::unordered_map<BotId, BotSchedule> bots_;
};

} // namespace Playerbot

#include "TickScheduler.h"
#include "AiWorkerPool.h"
#include "Bot/BotActivityTier.h"

namespace Playerbot {

TickScheduler::TickScheduler(AiWorkerPool& pool) : pool_(pool) {}

void TickScheduler::on_world_tick(Ms now)
{
    // Collect due bot ids into a local buffer, then enqueue once with
    // a single mutex acquisition in AiWorkerPool::schedule_ticks.
    // At 1000+ bots × ~50 Hz × ~30% due-ratio = ~15K mutex acq/sec
    // saved per second (was: one acq per due bot).
    thread_local std::vector<BotId> due_buf;
    due_buf.clear();
    due_buf.reserve(bots_.size());
    for (auto& [id, sched] : bots_)
    {
        if (sched.next_tick.count() <= now.count())
        {
            due_buf.push_back(id);
            sched.next_tick = now + TickPeriodFor(sched.tier);
        }
    }
    pool_.schedule_ticks(due_buf);
}

void TickScheduler::register_bot(BotId id, ActivityTier initial)
{
    bots_[id] = BotSchedule{initial, Ms{0}};
}

void TickScheduler::unregister_bot(BotId id)
{
    bots_.erase(id);
}

void TickScheduler::set_tier(BotId id, ActivityTier classified, Ms now)
{
    auto it = bots_.find(id);
    if (it == bots_.end()) return;
    auto& sched = it->second;

    // Idle→Parked ramp. Track consecutive Idle classifications; once the
    // bot has been continuously Idle for kIdleRampToParked frames, park it
    // (ActivityTier::Hibernate, 2 s). The ramp keys on Idle SPECIFICALLY:
    // every other classification — Combat / Active / Cruise / dead-Idle wake
    // — falls into the else branch and resets the streak, so the bot leaves
    // (or never enters) the parked tier the instant it does anything. In
    // particular Cruise (solo open-world travel/quest, 300 ms) resets the
    // streak exactly like Active and therefore CAN NEVER be parked — a
    // travelling bot must never freeze. The streak only advances on a *true*
    // Idle classification — not on the synthesised parked tier — so an
    // already-parked-and-still-idle bot just stays parked (streak pinned at
    // the ramp threshold).
    ActivityTier effective = classified;
    if (classified == ActivityTier::Idle)
    {
        if (sched.idle_streak < kIdleRampToParked)
            ++sched.idle_streak;
        if (sched.idle_streak >= kIdleRampToParked)
            effective = ActivityTier::Hibernate;   // parked
    }
    else
    {
        sched.idle_streak = 0;   // Combat / Active / Cruise / dead-wake — never park
    }

    if (sched.tier == effective) return;
    sched.tier = effective;
    // Tier change resets BOTH deadlines so a promotion to Combat (or any
    // wake from the parked tier) takes effect on the very next world tick
    // and the snapshot rebuilds at the new rate immediately. This is the
    // reactivity guarantee: a bot entering combat / being owner-controlled /
    // starting to move rebuilds on the next frame, never on the parked
    // 2 s cadence.
    sched.next_tick = now;
    sched.next_snapshot = now;
}

bool TickScheduler::should_build_snapshot(BotId id, Ms now)
{
    auto it = bots_.find(id);
    if (it == bots_.end()) return false;
    if (it->second.next_snapshot.count() > now.count()) return false;
    // Advance using the tier's AI-tick period — snapshot cadence
    // matches AI tick cadence so an AI worker always sees fresh data
    // when its tick fires.
    it->second.next_snapshot = now + TickPeriodFor(it->second.tier);
    return true;
}

size_t TickScheduler::bot_count() const
{
    return bots_.size();
}

} // namespace Playerbot

// CalendarRules - Refactor #3 pass 4. Migrates the calendar auto-RSVP
// idle rule out of the State_Idle cascade. Bots auto-accept all pending
// calendar invites every 30 minutes; the API short-circuits to OutOfRange
// when the queue is empty so the no-op cost is negligible.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"

namespace Playerbot {

namespace {

constexpr uint32 kRsvpThrottleMs = 30u * 60u * 1000u;

bool CalendarRsvpGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32 /*now_ms*/)
{
    if (s.level() < 10) return false;
    if (s.in_combat() || s.is_casting()) return false;
    const uint32 cal_now_ms = s.published_at_ms();
    if (cal_now_ms == 0) return false;
    return (cal_now_ms - ai.last_calendar_rsvp_ms()) >= kRsvpThrottleMs;
}

bool CalendarRsvpFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 /*now_ms*/)
{
    emit.calendar_rsvp_all(/*accept=*/true);
    ai.set_last_calendar_rsvp_ms(s.published_at_ms());
    ai.set_last_rule_fired("idle:calendar_rsvp_all");
    return true;
}

} // anonymous namespace

void RegisterCalendarRules(IdleRuleRegistry& r)
{
    IdleRule rule;
    rule.name     = "idle:calendar_rsvp_all";
    rule.priority = 200;   // low — drain pending invites only when nothing more useful is pending
    rule.gate     = &CalendarRsvpGate;
    rule.fire     = &CalendarRsvpFire;
    // Calendar invites trickle in over hours; 5s gate-throttle saves
    // 24/25 gate calls without any responsiveness loss.
    rule.min_interval_ms = 5000;
    r.register_rule(std::move(rule));
}

} // namespace Playerbot

// IdleRule — registry implementation.

#include "IdleRule.h"
#include "BotSnapshotView.h"
#include "BotAI.h"
#include "BotIntentEmitter.h"
#include "Group/GroupSnapshot.h"

#include <algorithm>
#include <mutex>
#include "Log.h"   // TC_LOG_ERROR

namespace Playerbot {

void IdleRuleRegistry::register_rule(IdleRule r)
{
    // finalize() runs after every register_rule from RegisterAllIdleRules.
    // Hitting this branch means someone is registering a rule AFTER init,
    // which would race with AI workers calling tick(). Refuse instead.
    if (finalized_)
    {
        TC_LOG_ERROR("playerbot.v2",
            "[IdleRuleRegistry] register_rule('{}', prio={}) called after "
            "finalize() — refusing to mutate vector while AI workers tick. "
            "All rule registration must complete in RegisterAllIdleRules.",
            r.name, r.priority);
        return;
    }
    rules_.push_back(std::move(r));
}

void IdleRuleRegistry::finalize()
{
    std::stable_sort(rules_.begin(), rules_.end(),
        [](IdleRule const& a, IdleRule const& b) { return a.priority > b.priority; });
    finalized_ = true;
}

std::string_view IdleRuleRegistry::tick(BotSnapshotView const& s, BotAI& ai,
                                        GroupSnapshotView const& g,
                                        BotIntentEmitter& emit, uint32 now_ms,
                                        int min_priority)
{
    // finalize() is called single-threaded from RegisterAllIdleRules
    // before any worker starts. If a future change forgets to call it,
    // self-heal once + warn (still hazardous if multiple workers reach
    // here on the same first tick, but better than silent UB).
    if (!finalized_)
    {
        static std::once_flag warn_once;
        std::call_once(warn_once, [] {
            TC_LOG_ERROR("playerbot.v2",
                "[IdleRuleRegistry] tick() reached an unfinalized registry — "
                "calling finalize() defensively. RegisterAllIdleRules must "
                "call finalize() before workers tick.");
        });
        finalize();
    }
    for (auto const& r : rules_)
    {
        if (r.priority < min_priority) break;   // rules_ sorted desc — bail early
        if (!r.enabled) continue;
        if (!r.gate)    continue;
        // Per-rule gate-throttle. Rules with min_interval_ms > 0 skip
        // the gate entirely until the throttle window elapses. Saves
        // the gate-call cost for low-urgency rules (mail, calendar,
        // auction, charter) that don't need 5Hz responsiveness.
        if (r.min_interval_ms != 0)
        {
            if (r.last_gate_attempt_ms != 0
                && (now_ms - r.last_gate_attempt_ms) < r.min_interval_ms)
                continue;
            r.last_gate_attempt_ms = now_ms;
        }
        if (!r.gate(s, ai, g, now_ms)) continue;
        if (r.fire && r.fire(s, ai, g, emit, now_ms))
        {
            r.last_fired_ms = now_ms;
            return r.name;
        }
    }
    return {};
}

std::vector<IdleRuleRegistry::RuleDiagnostic>
IdleRuleRegistry::diagnose(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const& g, uint32 now_ms) const
{
    // ensure_sorted() is non-const; do a stable copy + sort instead so
    // diagnose can run from a const handle. Cheap — rule count is small.
    std::vector<IdleRule const*> ordered;
    ordered.reserve(rules_.size());
    for (auto const& r : rules_) ordered.push_back(&r);
    std::stable_sort(ordered.begin(), ordered.end(),
        [](IdleRule const* a, IdleRule const* b) { return a->priority > b->priority; });

    std::vector<RuleDiagnostic> out;
    out.reserve(ordered.size());
    for (auto const* r : ordered)
    {
        bool gate_true = false;
        if (r->enabled && r->gate)
        {
            try { gate_true = r->gate(s, ai, g, now_ms); }
            catch (...) { gate_true = false; }
        }
        out.push_back({r->name, r->priority, gate_true, r->enabled, r->last_fired_ms});
    }
    return out;
}

void RegisterAllIdleRules(IdleRuleRegistry& r)
{
    RegisterVendorRules(r);
    RegisterAuctionRules(r);
    RegisterMailRules(r);
    RegisterCalendarRules(r);
    RegisterQuestRules(r);
    RegisterBankRules(r);
    RegisterGatheringRules(r);
    RegisterGuildRules(r);
    RegisterTrainerRules(r);
    RegisterHearthRules(r);
    RegisterHunterPetRules(r);
    RegisterLootDrainRules(r);
    RegisterAmbientRules(r);
    RegisterSurvivalRules(r);
    RegisterCoreFlowRules(r);
    RegisterOocConsumableRules(r);
    RegisterMaintainRules(r);
    RegisterDungeonBgRules(r);
    RegisterPrologueRules(r);
    RegisterElevatorRules(r);
    RegisterCraftOrderRules(r);
    RegisterFarTravelRules(r);   // idle:far_same_map_travel @697
    // Future passes: RegisterTravelRules(r); RegisterDungeonRules(r);
    // RegisterBgRules(r); ...

    // CRITICAL: finalize sorts rules_ + locks against further register_rule.
    // Must run BEFORE any AI worker calls tick(). Services::Init calls
    // RegisterAllIdleRules immediately before starting the AI worker pool
    // (see Services.cpp), so finalize() runs single-threaded here.
    r.finalize();
}

} // namespace Playerbot

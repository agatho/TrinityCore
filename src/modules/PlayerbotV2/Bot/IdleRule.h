// IdleRule - Registration-table model for State_Idle rules.
// REFACTOR_3_IDLE_RULES_HANDOVER.md introduces this to replace the linear
// cascade inside State_Idle::OnTick. Each rule registers a name, priority,
// and (gate, fire) lambdas. The registry walks rules in priority order on
// every tick; the first rule whose gate returns true AND whose fire returns
// true wins the tick. The fire path is responsible for set_last_rule_fired.

#pragma once

#include "BotTypes.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Playerbot {

class BotSnapshotView;
class GroupSnapshotView;
class BotAI;
class BotIntentEmitter;

struct IdleRule
{
    // Stable name string. Compared by value in /whyidle diagnostics; used
    // as the argument to set_last_rule_fired when the rule fires. Must
    // outlive the registry (string-literal or a long-lived std::string).
    std::string name;

    // Higher priority fires first. Tie-breaking is registration order
    // (std::stable_sort is used).
    int priority = 0;

    // Predicate. Returns true if the rule *wants* to fire. Cheap — runs
    // every tick. No side effects.
    std::function<bool(BotSnapshotView const&, BotAI&,
                       GroupSnapshotView const&, uint32 now_ms)> gate;

    // Fire path. Emits intents, marks set_last_rule_fired, returns true
    // when the tick should be considered consumed. Returning false means
    // the gate said yes but fire bailed (e.g. NPC moved out of range
    // mid-tick) — the registry tries the next-priority rule.
    std::function<bool(BotSnapshotView const&, BotAI&,
                       GroupSnapshotView const&,
                       BotIntentEmitter&, uint32 now_ms)> fire;

    // Master enable. Currently true for all rules; future config flags
    // (e.g. "disable vendor_visit") can flip this without recompiling.
    bool enabled = true;

    // Diagnostics-only: timestamp of the most recent successful fire.
    // Updated by IdleRuleRegistry::tick. Read by diagnose().
    mutable uint32 last_fired_ms = 0;

    // Optional gate-throttle. When non-zero, the registry skips the gate
    // (without calling it) if (now_ms - last_gate_attempt_ms) < this.
    // Useful for low-urgency rules that don't need 5 Hz responsiveness:
    // mail drain, calendar gossip, charter drive, auction post — each
    // declares e.g. 2000ms to skip 9/10 ticks. Default 0 means "run
    // every tick" (legacy behavior). Per the IdleRule bitmask audit
    // (2026-05-21), this is the lighter-weight alternative to a global
    // bit-set short-circuit refactor — only the few high-cost
    // low-urgency rules opt in; everything else keeps its responsiveness.
    uint32 min_interval_ms = 0;

    // Throttle bookkeeping (used iff min_interval_ms != 0). Updated by
    // tick() when we decide to evaluate; read on subsequent ticks to
    // compute "has the throttle window elapsed?". Mutable because tick()
    // takes the rule by const-ref via the rules_ vector.
    mutable uint32 last_gate_attempt_ms = 0;
};

class IdleRuleRegistry
{
public:
    void register_rule(IdleRule r);

    // Returns the name of the rule that fired (empty when none did).
    // Walks rules in priority order, runs gate then fire. On the first
    // rule that both gates true AND fires true, returns its name.
    //
    // `min_priority` enables two-stage dispatch: a top-of-tick call with
    // `min_priority = 700` runs only the high-priority preemption rules
    // (hazard avoidance, instance-override). A bottom-of-tick call with
    // `min_priority = 0` runs every rule (skipping any that already fired
    // at the top — rules are stateless, so the gate just won't trigger
    // twice). This lets the inline cascade preserve its ordering for
    // rules that haven't been migrated yet, while still letting hazard
    // rules fire BEFORE the inline cascade.
    std::string_view tick(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g,
                          BotIntentEmitter& emit, uint32 now_ms,
                          int min_priority = 0);

    // Diagnostic snapshot of every registered rule, in priority order.
    // Used by `/whyidle <bot>` to dump the rule table + last-fire status.
    struct RuleDiagnostic
    {
        std::string_view name;
        int              priority;
        bool             gate_true;
        bool             enabled;
        uint32           last_fired_ms;
    };
    std::vector<RuleDiagnostic> diagnose(BotSnapshotView const& s, BotAI& ai,
                                         GroupSnapshotView const& g,
                                         uint32 now_ms) const;

    size_t size() const { return rules_.size(); }

    // Sort rules by priority desc + mark the registry immutable. MUST be
    // called from RegisterAllIdleRules at module init, AFTER every
    // register_rule call, BEFORE any AI worker starts ticking. Once
    // finalized, tick() runs lock-free across all worker threads.
    //
    // Why this is needed: a previous lazy-sort design did
    // `if (!sorted_) std::stable_sort(rules_)` inside tick(). Worker
    // threads racing on the FIRST tick concurrently sorted the same
    // vector, corrupting the heap (crash trace 2026-05-17 21:31:01
    // showed RtlFreeHeap fault inside std::_Insertion_sort_unchecked
    // assigning IdleRule::name "idle:guild_charter_drive"). Now sorting
    // happens once, single-threaded, at finalize time.
    void finalize();

private:
    std::vector<IdleRule> rules_;
    // True once finalize() has run. tick() asserts this in debug builds;
    // in release we log a warning once and call finalize() defensively
    // (still a hazard, but better than heap corruption).
    bool finalized_ = false;
};

// Per-subsystem registrars. Each lives in Bot/States/Rules/<Name>Rules.cpp
// and is called from RegisterAll.cpp at module init.
void RegisterVendorRules(IdleRuleRegistry& r);
void RegisterAuctionRules(IdleRuleRegistry& r);
void RegisterMailRules(IdleRuleRegistry& r);
void RegisterCalendarRules(IdleRuleRegistry& r);
void RegisterQuestRules(IdleRuleRegistry& r);
void RegisterBankRules(IdleRuleRegistry& r);
void RegisterGatheringRules(IdleRuleRegistry& r);
void RegisterGuildRules(IdleRuleRegistry& r);
void RegisterTrainerRules(IdleRuleRegistry& r);
void RegisterHearthRules(IdleRuleRegistry& r);
void RegisterHunterPetRules(IdleRuleRegistry& r);
void RegisterLootDrainRules(IdleRuleRegistry& r);
void RegisterAmbientRules(IdleRuleRegistry& r);
void RegisterSurvivalRules(IdleRuleRegistry& r);
void RegisterCoreFlowRules(IdleRuleRegistry& r);
void RegisterOocConsumableRules(IdleRuleRegistry& r);
void RegisterMaintainRules(IdleRuleRegistry& r);
void RegisterDungeonBgRules(IdleRuleRegistry& r);
void RegisterPrologueRules(IdleRuleRegistry& r);
void RegisterElevatorRules(IdleRuleRegistry& r);
void RegisterCraftOrderRules(IdleRuleRegistry& r);
void RegisterFarTravelRules(IdleRuleRegistry& r);
// SurvivalRules registered at priority 880-900; dispatched from OnTick's
// high-priority top-of-tick call (min_priority=700), so they fire above
// the inline cascade. Two-stage dispatch shipped REFACTOR_3 pass 13.
//
// Future passes append registrars here.

// Single entry point — wires every per-subsystem registrar in one place.
void RegisterAllIdleRules(IdleRuleRegistry& r);

} // namespace Playerbot

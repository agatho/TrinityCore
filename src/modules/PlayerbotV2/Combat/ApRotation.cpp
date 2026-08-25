#include "ApRotation.h"
#include "../Bot/BotIntentEmitter.h"   // emitted_count() for emission feedback

namespace Playerbot {

bool ApRotation::tick(ApPredicateContext const& ctx, BotIntentEmitter& emit) const
{
    return tick(ctx, emit, nullptr);
}

bool ApRotation::tick(ApPredicateContext const& ctx, BotIntentEmitter& emit,
                      char const** out_rule_name) const
{
    for (auto const& rule : rules_)
    {
        if (rule.predicate(ctx))
        {
            // EMISSION FEEDBACK (audit B02): a passing predicate is not the
            // same as a successful action. Many actions' emits are silently
            // dropped (cast emit-lockout, move dedup) — the old code returned
            // true regardless, so ONE un-emittable high-priority rule
            // consumed the whole rotation tick and every lower-priority rule
            // (fillers, resource generators) starved: hunters never reached
            // their focus generator, DKs below 70% HP blocked their entire
            // kit behind an unaffordable Death Strike, bots degraded to
            // auto-attack-only. Fall through when the action pushed nothing.
            const uint32 before = emit.emitted_count();
            rule.action(ctx, emit);
            if (emit.emitted_count() == before)
                continue;
            if (out_rule_name) *out_rule_name = rule.name;
            return true;
        }
    }
    if (out_rule_name) *out_rule_name = nullptr;
    return false;
}

bool AlwaysTrue(ApPredicateContext const&) { return true; }

} // namespace Playerbot

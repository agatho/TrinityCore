// DungeonBgRules - Refactor #3 pass 16. Wraps the extracted DungeonDispatch
// / BgDispatch free functions (in State_Idle.cpp, forward-declared via
// `Bot/States/MaintainHelpers.h`) as single registered idle rules. Each
// dispatcher's body still contains the ~30 sub-rules that fire via
// set_last_rule_fired — the registry just sees `idle:dungeon_dispatch` /
// `idle:bg_dispatch` line items in /whyidle, while sub-rule tags continue
// surfacing through /history.
//
// Priorities sit in the top-of-tick band (>=700) so they preempt the
// bottom-of-tick cascade for dungeon / BG bots.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/States/MaintainHelpers.h"

namespace Playerbot {

namespace {

bool DungeonDispatchGate(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const&, uint32)
{
    return ai.dungeon_active();
}

bool DungeonDispatchFire(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const& g,
                         BotIntentEmitter& emit, uint32)
{
    if (!::Playerbot::States::DungeonDispatch(s, ai, g, emit)) return false;
    // The dispatcher body has already called set_last_rule_fired with the
    // specific sub-rule tag. Don't overwrite it with "idle:dungeon_dispatch".
    return true;
}

bool BgDispatchGate(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&, uint32)
{
    return ai.bg_active() && s.in_battleground();
}

bool BgDispatchFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit, uint32)
{
    if (!::Playerbot::States::BgDispatch(s, ai, g, emit)) return false;
    return true;
}

// ---------- idle:autoact_dispatch ----------
// Wraps the extracted `can_autoact` cascade: quest execution rules,
// engage_nearby_mob, travel cascade (walk_to_taxi/fly_to_taxi/use_portal/
// hearth_to_distant_goal/cast_self_teleport/unstick/etc.), mount_for_travel,
// wander_to_*/travel_to_*, crafting suite (craft_bandage/learn_recipe/
// disenchant/prospect/mill/craft_skillup), and the random wander rule.
// Body lives in State_Idle.cpp; this rule fires from the BOTTOM-of-tick
// dispatch (priority < 700) at the very top of that band so its sub-rules
// preempt other low-priority migrated rules like vendor_visit. The
// dispatcher's gate (can_autoact) shorts cheap (3 boolean checks) so the
// dispatch overhead is minimal for ineligible bots.
bool AutoactDispatchGate(BotSnapshotView const& s, BotAI&,
                         GroupSnapshotView const&, uint32)
{
    // Replicate the cheap part of can_autoact as a gate; the body re-checks
    // the rest. False here -> registry skips the dispatcher entirely.
    if (s.in_combat() || s.is_casting() || s.is_moving()) return false;
    if (s.is_stunned() || s.is_rooted()) return false;
    if (s.is_in_instance() || s.is_in_dungeon()) return false;
    return true;
}

bool AutoactDispatchFire(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const& g,
                         BotIntentEmitter& emit, uint32)
{
    if (!::Playerbot::States::AutoactDispatch(s, ai, g, emit)) return false;
    return true;
}

// ---------- idle:legacy_vendor_dispatch ----------
bool LegacyVendorGate(BotSnapshotView const& s, BotAI&,
                      GroupSnapshotView const&, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    return true;
}

bool LegacyVendorFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const& g,
                      BotIntentEmitter& emit, uint32)
{
    if (!::Playerbot::States::LegacyVendorDispatch(s, ai, g, emit)) return false;
    return true;
}

} // anonymous namespace

void RegisterDungeonBgRules(IdleRuleRegistry& r)
{
    // Priority 720 / 718 — top-of-tick band. A bot in a dungeon or BG
    // preempts almost every other rule (only survival / swim_stuck / etc.
    // fire higher). The two are mutually exclusive in practice.
    {
        IdleRule rule;
        rule.name     = "idle:dungeon_dispatch";
        rule.priority = 720;
        rule.gate     = &DungeonDispatchGate;
        rule.fire     = &DungeonDispatchFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:bg_dispatch";
        rule.priority = 718;
        rule.gate     = &BgDispatchGate;
        rule.fire     = &BgDispatchFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:autoact_dispatch";
        // Priority 50 (below ambient_emote at 80) — original semantics:
        // can_autoact ran INLINE after the bottom dispatch returned empty.
        // Registry-wise that means firing last in the bottom band so the
        // ambient flavor + every higher-priority migrated rule gets first
        // pick.
        rule.priority = 50;
        rule.gate     = &AutoactDispatchGate;
        rule.fire     = &AutoactDispatchFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:legacy_vendor_dispatch";
        // Priority 60 — between autoact (50) and ambient_emote (80).
        // Mirrors the legacy ordering where the single-need vendor cascade
        // (pre-FSM fallback for vendor_visit at 500) ran INLINE after the
        // bottom-of-tick dispatch returned empty but before autoact.
        rule.priority = 60;
        rule.gate     = &LegacyVendorGate;
        rule.fire     = &LegacyVendorFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

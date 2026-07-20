// ApRotation - Action Priority List evaluator. Per ARCHITECTURE.md §4 and
// CONTRACTS.md §4.1. Each (class, spec) ships a static array of ApRule and
// registers it with ApRegistry.

#pragma once

#include "Bot/BotTypes.h"
#include "ObjectGuid.h"
#include <span>

namespace Playerbot {

class BotSnapshotView;
class GroupSnapshotView;
class BotIntentEmitter;
enum class BgRole : uint8_t;

// PvP-awareness context propagated to APL predicates. The historical
// `ApPredicateContext` was structurally PvE-only — every defensive HP
// threshold, every interrupt selector, every CC target picker was tuned
// for raid encounters where target-role doesn't matter. This struct
// gives PvP-relevant data to predicates so they can:
//   * Escalate interrupt priority when target is an enemy Healer
//   * Raise defensive thresholds when under_player_attack
//   * Bias CC pickers toward Healers / casters
//   * Apply FC peel / kite logic conditional on BG context
//
// Default-constructed (all fields zeroed) for non-PvP ticks; consumers
// should gate on `in_battleground || in_arena` before reading other
// fields.
struct ApPvpContext
{
    bool       in_battleground       = false;
    bool       in_arena              = false;
    // Bot's BG role (FlagCarrier, FCEscort, Defender, etc). 0 = Free
    // (unknown / no role).
    uint8_t    bg_role               = 0;
    // Friendly / enemy flag-carrier scalars from the snapshot (Kotmogu
    // multi-carriers expose the first detected — sufficient for most
    // peel/chase decisions).
    ObjectGuid friendly_flag_carrier;
    ObjectGuid enemy_flag_carrier;
    // True when at least one nearby_enemy is currently meleeing the bot
    // (snapshot's under_player_attack()).
    bool       under_player_attack   = false;
};

struct ApPredicateContext
{
    BotSnapshotView const&   bot;
    GroupSnapshotView const& group;
    // Owner pin from /aoe whisper (default false). When true, AoE-favouring
    // predicates (Death and Decay, Multi-Shot, Whirlwind) should fire even
    // on a single attacker — useful when the owner sees an incoming pull
    // the bot can't yet see in the snapshot. Predicates that already gate
    // on `attackers_count() >= N` should `||` this in.
    bool                     aoe_preference = false;
    // PvP awareness — see ApPvpContext above. Defaults to PvE-mode
    // (all-false) so existing predicates continue to work; PvP-aware
    // predicates read pvp.in_battleground first.
    ApPvpContext             pvp;
};

using ApPredicate = bool (*)(ApPredicateContext const&);
using ApAction    = void (*)(ApPredicateContext const&, BotIntentEmitter&);

struct ApRule
{
    ApPredicate predicate;
    ApAction    action;
    char const* name;
};

class ApRotation
{
public:
    explicit ApRotation(std::span<ApRule const> rules) : rules_(rules) {}

    // Evaluate top-to-bottom. On first matching predicate, invoke its action
    // and return true. Returns false if no rule fired (rare — the last rule
    // is usually `AlwaysTrue`).
    bool tick(ApPredicateContext const& ctx, BotIntentEmitter& emit) const;

    // Diagnostic variant: also writes the name of the rule that fired (if any)
    // into `out_rule_name`. The pointer must outlive the kRules array's static
    // strings, which is always true for the rotation tables (`constexpr char
    // const*` literals). Caller passes nullptr if the name isn't needed.
    bool tick(ApPredicateContext const& ctx, BotIntentEmitter& emit,
              char const** out_rule_name) const;

    size_t rule_count() const { return rules_.size(); }

private:
    std::span<ApRule const> rules_;
};

// Convenience predicate used as the catch-all "always fire this" rule.
bool AlwaysTrue(ApPredicateContext const&);

} // namespace Playerbot

// ApHealHelpers — shared healer-rotation helpers used by all healer APLs.
// Currently hosts ShouldCancelHealForSwap (in-flight cast cancel when a
// different group member drops critical mid-cast). Add other shared
// healer logic here as 2+ specs adopt it.

#pragma once

#include "../ApRotation.h"
#include "../../Bot/BotIntentEmitter.h"
#include "../../Bot/BotSnapshotView.h"
#include "../../Group/GroupSnapshot.h"

#include <cstdint>
#include <initializer_list>

namespace Playerbot::Combat {

// Returns true when the bot's in-flight heal should be cancelled so
// the next tick can re-aim at a freshly critical group member.
//
// Gates (all required):
//   - bot.is_casting()
//   - cast remaining >= 700ms (cancelling a near-done cast costs more
//     than re-targeting buys)
//   - spell ID is in `allowlist` (the slow heals each spec considers
//     worth cancelling; instant heals never hit the remaining-ms gate)
//   - cast target is non-empty AND != lowest_hp_on_map
//   - lowest_hp_on_map member is at <=25% HP (critical)
//   - delta(current target HP%, lowest HP%) > 30 (avoid flicker when
//     both are at similar HP)
inline bool ShouldCancelHealForSwapImpl(
    ApPredicateContext const& ctx,
    std::initializer_list<uint32_t> allowlist)
{
    if (!ctx.bot.is_casting()) return false;
    if (ctx.bot.current_cast_remaining().count() < 700) return false;
    const uint32_t sid = ctx.bot.current_cast_spell_id();
    bool in_list = false;
    for (uint32_t a : allowlist) if (a == sid) { in_list = true; break; }
    if (!in_list) return false;
    const ObjectGuid current_target = ctx.bot.current_cast_target();
    if (current_target.IsEmpty()) return false;
    auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f);
    if (!low || low->max_hp <= 0) return false;
    if (low->guid == current_target) return false;
    const int32 low_pct = (low->hp * 100) / low->max_hp;
    if (low_pct > 25) return false;
    int32 cur_pct = 100;
    if (auto const* members = ctx.group.members())
    {
        for (auto const& m : *members)
        {
            if (m.guid != current_target) continue;
            if (m.max_hp <= 0) return false;
            cur_pct = (m.hp * 100) / m.max_hp;
            break;
        }
    }
    return (cur_pct - low_pct) > 30;
}

inline void DoCancelHealForSwap(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cancel_cast();
}

} // namespace Playerbot::Combat

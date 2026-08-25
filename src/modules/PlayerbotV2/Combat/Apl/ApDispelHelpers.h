// ApDispelHelpers — shared dispel-target selection used by all healer APLs.
// Centralizes the encounter / M+ affix priority check so adding a new
// healer spec or extending the priority logic happens in one place.

#pragma once

#include "../ApRotation.h"
#include "../../Services.h"
#include "../../Bot/Dungeon/DungeonScript.h"
#include "../../Group/GroupSnapshot.h"

namespace Playerbot {

// Returns the highest-priority dispel target across the bot's group.
// Walks `advice.dispel_priority_spells` first (encounter / M+ affix
// drivers like Bursting, Raging-enrage). On miss, falls back to the
// caller-supplied type walk via `type_walk(group)`. The type walk is a
// caller-side lambda because different healer specs cover different
// dispel-type subsets (Druid Resto: Magic+Curse+Poison; Holy Priest:
// Magic+Disease; etc).
template <class TypeWalkFn>
inline GroupMemberSummary const* DispelTargetWithPriority(
    ApPredicateContext const& ctx, TypeWalkFn&& type_walk)
{
    if (Services::Initialized())
    {
        DungeonAdvice const dav = Services::Dungeons().GetAdvice(ctx.bot);
        if (!dav.dispel_priority_spells.empty())
        {
            if (GroupMemberSummary const* tgt =
                    ctx.group.priority_dispel_candidate(dav.dispel_priority_spells))
                return tgt;
        }
    }
    return type_walk(ctx.group);
}

} // namespace Playerbot

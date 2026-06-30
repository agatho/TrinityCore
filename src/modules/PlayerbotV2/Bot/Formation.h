// Formation — squad-control geometry. Translates (slot, type) → a
// follow offset (distance, angle relative to leader's facing) so each
// bot in an owner's squad takes a distinct, stable position around
// the leader.
//
// Slot ids are owner-scoped: each bot remembers its slot, the formation
// type changes via /formation, and the same slot keeps the bot in the
// same relative position even as the formation changes (slot 0 is
// always the "primary" position — directly behind for Column, hard
// flank for Wedge, opposite-pole for Circle, etc).
//
// Per OWNER_SQUAD_CONTROL_PLAN.md Phase D.

#pragma once

#include <cstdint>

namespace Playerbot {

enum class FormationType : uint8_t
{
    Free    = 0,   // Legacy — MotionMaster decides; no slot offset.
    Tight   = 1,   // All bots stack on top of leader.
    Spread  = 2,   // Ring 8y around leader, slot * 45° each.
    Line    = 3,   // Side-by-side behind leader, slots fan laterally.
    Column  = 4,   // Single-file behind leader.
    Wedge   = 5,   // V-shape behind leader; alternating flanks.
    Circle  = 6,   // Ring 12y around leader, evenly distributed.
};

struct FormationOffset
{
    float distance       = 5.0f;
    // Angle relative to leader's facing, in radians.
    // 0   = directly behind, pi/2 = right flank, pi = in front,
    // 3pi/2 = left flank.
    float angle_radians  = 0.0f;
};

// Compute the offset for `slot` in `type`. `default_distance` is the
// owner-pinned follow distance (from /follow_distance); some formations
// (Tight, Free) ignore the distance, others use it as the radius.
FormationOffset ComputeFormationOffset(
    FormationType type,
    uint8_t slot,
    float default_distance);

// Parse a token (case-insensitive) → FormationType. Returns Free for
// unrecognised input so /formation typo doesn't accidentally rearrange
// the squad.
FormationType ParseFormationType(char const* token);

// Diagnostic name. Stable strings — used in /status and /history.
char const* FormationTypeName(FormationType t);

} // namespace Playerbot

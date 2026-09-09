/*
 * SmartAiParams - parameter-semantics lookups for the SmartScript editor,
 * backed by the build-time-generated SmartAiMetadata.gen.h (parsed from core
 * SmartScriptMgr.h). Returns the documented meaning of the four numeric params
 * for a given SMART_EVENT / SMART_ACTION / SMART_TARGET value, e.g.
 * smartEventParams(8) -> "SpellID, School, CooldownMin, CooldownMax".
 *
 * Returns "" when the value is unknown. Used to populate combo-box item tooltips
 * so the operator sees what each numeric param means instead of guessing.
 */

#pragma once

namespace world_editor::app
{

[[nodiscard]] char const* smartEventParams(int value);
[[nodiscard]] char const* smartActionParams(int value);
[[nodiscard]] char const* smartTargetParams(int value);
[[nodiscard]] char const* conditionParams(int value);

} // namespace world_editor::app

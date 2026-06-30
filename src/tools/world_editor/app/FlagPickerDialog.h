/*
 * FlagPickerDialog - user-friendly named-checkbox editors for the bitmask
 * columns on a spawn (npcflag, unit_flags, unit_flags2, unit_flags3).
 *
 * Instead of typing raw hex the operator ticks "is vendor", "is flight master",
 * "can swim", etc.  The flag names + labels come from FlagMetadata.gen.h, which
 * is generated at build time from core's UnitDefines.h (// TITLE comments) so
 * they can never drift from TrinityCore.
 *
 * Each function opens a modal picker pre-seeded with `current` and returns the
 * new mask, or std::nullopt when the operator cancels.  Bits with no matching
 * checkbox (unknown/reserved) are preserved untouched.
 *
 * npcflag is 64-bit: the low 32 bits are NPCFlags, the high 32 bits NPCFlags2
 * (creature.npcflag stores both), so pickNpcFlags shows both groups.
 */

#pragma once

#include <cstdint>
#include <optional>

class QWidget;

namespace world_editor::app
{

[[nodiscard]] std::optional<uint64_t> pickNpcFlags  (QWidget* parent, uint64_t current);
[[nodiscard]] std::optional<uint32_t> pickUnitFlags (QWidget* parent, uint32_t current);
[[nodiscard]] std::optional<uint32_t> pickUnitFlags2(QWidget* parent, uint32_t current);
[[nodiscard]] std::optional<uint32_t> pickUnitFlags3(QWidget* parent, uint32_t current);

} // namespace world_editor::app

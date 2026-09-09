// ApRegistry - (class, spec) → ApRotation lookup. Registered at module init.
// Per CONTRACTS.md §4.1.

#pragma once

#include "ApRotation.h"
#include "Bot/BotTypes.h"
#include <vector>

namespace Playerbot::Combat {

// Returns the rotation for the given class+spec, or nullptr if none registered.
// `spec` is the ChrSpecialization DB2 id (uint32) — matches BotSnapshot::spec.
ApRotation const* GetRotation(uint8 cls, uint32 spec);

// Called once at module init to populate the registry.
void RegisterAllRotations();

// Called by individual Apl_*.cpp files at registration. Single-threaded; runs
// during init before AI workers start.
void RegisterRotation(uint8 cls, uint32 spec, ApRotation rotation);

// Diagnostic: enumerate every registered (class, spec, rule_count) tuple.
struct RotationListEntry { uint8 cls; uint32 spec; size_t rules; };
std::vector<RotationListEntry> ListRotations();

} // namespace Playerbot::Combat

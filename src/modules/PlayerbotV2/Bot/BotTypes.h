// BotTypes.h - Foundational ID and enum types per CONTRACTS.md §1.

#pragma once

#include "Define.h"
#include <chrono>

namespace Playerbot {

using BotId       = uint64;   // == ObjectGuid::GetCounter() of bot's character
using TickId      = uint64;   // Monotonic per world tick
using IntentId    = uint64;   // Monotonic per intent emitted (for tracing)
using SnapshotVer = uint64;   // Monotonic per snapshot publication
using Ms          = std::chrono::milliseconds;

// NOTE: numeric order is NOT load-bearing — these values are never
// array-indexed, ordered-compared (<,<=), persisted, or int-cast anywhere
// (verified fleet-wide); every use is an equality test or switch. Cruise is
// therefore inserted mid-ladder (between Active and Idle) so the values read
// as a descending-cadence ladder. If you add an ordered comparison later,
// re-audit before relying on these numbers.
enum class ActivityTier : uint8
{
    Combat    = 0,   // ≥10 Hz
    Active    = 1,   // ~6.7 Hz (150 ms) — responsiveness-critical bots
    Cruise    = 2,   // ~3.3 Hz (300 ms) — solo open-world travel/questing
    Idle      = 3,   // 2 Hz (500 ms) — ramps to Parked
    Hibernate = 4,   // 0.5 Hz (2 s) — "Parked" long-idle AFK
};

enum class BotState : uint8
{
    LoggingIn,
    Idle,
    Travelling,
    Questing,
    InCombat,
    Looting,
    Dead,
    Resurrecting,
    LoggingOut,
    // Cross-cutting (re-entrant, may layer over a primary state)
    AtVendor,
    AtMailbox,
    AtAuctionHouse,
    InGroup,
    InInstance,
    Decorating,
};

enum class Role : uint8
{
    Tank,
    Healer,
    Dps,
    Unknown,
};

enum class DispelType : uint8
{
    Magic,
    Curse,
    Poison,
    Disease,
    Bleed,
    Enrage,
    None,
};

} // namespace Playerbot

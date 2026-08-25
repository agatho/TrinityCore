// Playerbot V2 - Precompiled header
// Per BUILD.md: TrinityCore PCH essentials + V2 heavy STL.
// Keep this list short; PCH bloat hurts compile times more than it helps.

#pragma once

// STL — used everywhere in V2
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// TrinityCore foundation
#include "Define.h"
#include "ObjectGuid.h"

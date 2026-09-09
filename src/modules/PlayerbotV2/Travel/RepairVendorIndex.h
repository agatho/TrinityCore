// RepairVendorIndex — a CHEAPER QuestHubDatabase clone that indexes every
// real UNIT_NPC_FLAG_REPAIR creature spawn in the world DB so any bot can ask
// "where is the nearest faction-appropriate repair vendor on my map".
//
// Built ONCE at module init from the WORLD_SEL_REPAIR_VENDOR_SPAWNS prepared
// statement (REPAIR npcflag bit 0x1000). Read-only afterwards. Unlike
// QuestHubDatabase there is NO DBSCAN: a repair vendor is a single useful
// point, not a cluster, so we keep per-map immutable vectors of spawns and do a
// bounded 2D-nearest scan on query.
//
// THREADING CONTRACT (mirrors QuestHubDatabase exactly):
//   * Initialize() runs on the world thread at boot, takes the write lock,
//     fills the immutable vectors, then publishes them with a release store on
//     `_initialized`.
//   * GetNearestRepairVendor() is a STRICT PURE READER on the AI worker hot
//     path: acquire-load the `_initialized` fence, take a shared_lock, scan the
//     bot's same-map immutable vector, return a copy. NO mutable / lazy / memo
//     state, NO navmesh, NO DB. It is NEVER called from the snapshot Build path
//     (the freeze surface) — only from idle/dead/combat rules.
//
// Faction-filtered like QuestHub::FactionAllows: a hostile-town repair vendor
// is useless (and lethal) to a bot, so a spawn only matches a bot whose
// race-derived team the vendor is friendly to. Cross-map queries return nullopt
// — the caller falls back to the nearest quest hub (hubs have vendors) via
// Services::Hubs().

#pragma once

#include "Position.h"
#include <atomic>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot {
struct BotSnapshot;
}

namespace Playerbot::V2::Travel {

// Result of a nearest-vendor query: a routable world position on the bot's
// current map. Mirrors the {map,x,y,z} the travel-plan composer consumes.
struct RepairVendorHit
{
    uint32 map_id = 0;
    float  x = 0.f;
    float  y = 0.f;
    float  z = 0.f;
};

class RepairVendorIndex
{
public:
    RepairVendorIndex() = default;
    ~RepairVendorIndex() = default;

    RepairVendorIndex(RepairVendorIndex const&) = delete;
    RepairVendorIndex& operator=(RepairVendorIndex const&) = delete;

    // Loads all repair-vendor spawns and bins them per map. Call once during
    // module init from the world thread; safe to call again via Reload().
    bool Initialize();

    // Drop & rebuild — for live data refresh without a server restart.
    bool Reload();

    [[nodiscard]] bool IsInitialized() const { return _initialized.load(std::memory_order_acquire); }
    [[nodiscard]] size_t GetVendorCount() const;

    // Nearest SAME-MAP repair-vendor spawn the bot's race-derived team is
    // allowed to use. Returns nullopt cross-map (no same-map vendor) — the
    // caller then falls back to the nearest quest hub. STRICT PURE READER.
    [[nodiscard]] std::optional<RepairVendorHit> GetNearestRepairVendor(BotSnapshot const& snap) const;

private:
    struct RepairVendorSpawn
    {
        Position location;
        // Faction bitmask using the SAME convention as QuestHub::factionMask:
        // bit0 Alliance, bit1 Horde, bit2 Neutral. Computed once at init from
        // the spawn's FactionTemplateEntry; the query filters with the bot's
        // race_team_bit (mirror of QuestHub::FactionAllows).
        uint32 factionMask = 0;
    };

    uint32 LoadFromDB();

    // Per-map immutable vectors, published once at init. The hot read path
    // only ever indexes by the bot's map_id, so cross-map spawns never enter
    // the scan (matches QuestHub::GetDistanceFrom returning +inf cross-map).
    std::unordered_map<uint32, std::vector<RepairVendorSpawn>> _byMap;

    // _mutex guards _byMap during Initialize()/Reload() only; the post-init
    // read path goes through the atomic _initialized fence + shared_lock, the
    // same lock-free-after-publish guarantee QuestHubDatabase documents.
    mutable std::shared_mutex _mutex;
    std::atomic<bool> _initialized{false};
    size_t _count = 0;
};

} // namespace Playerbot::V2::Travel

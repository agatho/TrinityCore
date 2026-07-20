// QuestHubDatabase — clusters all quest-giver creature spawns in the world DB
// into ~hundreds of "quest hubs" (DBSCAN, 75y radius, min 2 givers/cluster) and
// answers "what's the nearest level-appropriate hub for this bot" queries.
//
// Built once at module init from creature_template + quest_template via the
// WORLD_SEL_QUEST_GIVER_SPAWNS prepared statement. Read-only afterwards;
// concurrent reads protected by a shared_mutex (write happens only at init
// and the optional Reload).
//
// Adapted from src/modules/Playerbot/Quest/QuestHubDatabase.{h,cpp} for V2:
//  - Namespace Playerbot::V2::Travel.
//  - Snapshot-based runtime API (no Player* on the AI worker hot path).
//  - Lifecycle owned by Services::Hubs() rather than a Meyer's singleton.

#pragma once

#include "Position.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot {
struct BotSnapshot;
}

namespace Playerbot::V2::Travel {

// R7 — Chromie-time leveling-zone relocation.
// ----------------------------------------------------------------------------
// When a bot exhausts every quest in its current zone (no local offers /
// turn-ins / objective) it has "out-levelled" the zone and must RELOCATE to a
// level-appropriate quest hub — often on another continent. SelectLevelingHub
// answers "which hub should this starved bot travel to?" using retail
// Chromie-Time semantics:
//   * L1-9   : no cross-continent relocation (racial starter content is
//              same-map; the same-map travel_to_hub rule already handles it).
//   * L10-59 : each bot DETERMINISTICALLY picks ONE of the 8 leveling
//              expansions from its guid (even population spread) and routes
//              through THAT expansion's continents only — so the fleet doesn't
//              funnel into one zone under level-scaling (every expansion
//              brackets 10-60 in retail).
//   * L60-69 : Dragon Isles (Dragonflight) — mandatory, all bots.
//   * L70-80 : Khaz Algar (The War Within / newest 70-80 content) — mandatory.
// The choice is made sticky by the caller (BotAI + the 0010 DB columns) with
// level-bracket hysteresis so it doesn't flip-flop every tick / restart.
struct LevelingHubChoice
{
    uint32 hub_id     = 0;
    uint32 map_id     = 0;
    uint8  bracket_lo = 0;   // inclusive level band the choice stays valid for
    uint8  bracket_hi = 0;   // (hysteresis: re-pick only when level exits this)
    [[nodiscard]] bool valid() const { return hub_id != 0; }
};

// Deterministic expansion bucket [0,8) for a bot's guid-low. Pure function of
// the guid (stable across restarts, even spread). Drives the L10-59 continent
// choice. Exposed so the caller can re-derive band-membership for hysteresis.
[[nodiscard]] uint8 LevelingExpansionBucket(uint64 guid_low);

// Continents the bot's CURRENT level band routes to, given its deterministic
// expansion bucket. Empty for the L1-9 band (no cross-continent relocation).
[[nodiscard]] std::vector<uint32> LevelingBandContinents(uint8 level, uint64 guid_low);

// True when `map_id` is one of the band's continents — i.e. a previously
// stored sticky target is still appropriate for the bot's current level band.
[[nodiscard]] bool LevelingBandAllowsContinent(uint8 level, uint64 guid_low, uint32 map_id);

// One spatial cluster of quest givers. The same hub may span any number of
// distinct creature entries (e.g. an inn with 4 different quest NPCs).
struct QuestHub
{
    uint32      hubId       = 0;
    uint32      mapId       = 0;
    uint32      zoneId      = 0;
    Position    location;             // cluster center (avg of giver positions)
    float       radius      = 50.0f;  // max distance from center to any member
    uint32      minLevel    = 0;
    uint32      maxLevel    = 0;
    // Faction bitmask: bit0 Alliance, bit1 Horde, bit2 Neutral. Computed from
    // the union of allowable_races on the hub's quests; a hub with at least
    // one Alliance-only quest sets bit0, a Horde-only quest sets bit1, etc.
    uint32      factionMask = 0;
    std::string name;
    std::vector<uint32> questIds;     // quest_template.ID values offered here
    std::vector<uint32> creatureIds;  // creature_template.entry of givers

    // Snapshot-based eligibility: bot's level is within [minLevel, maxLevel],
    // and the bot's race-derived team is allowed by factionMask.
    [[nodiscard]] bool IsAppropriateFor(BotSnapshot const& snap) const;

    // Faction-only half of IsAppropriateFor: the bot's race-derived team is
    // allowed by factionMask (no level check). Used by the R7 same-map scan when
    // it widens the level band by ±2 for L1-9 bots but must still enforce
    // faction exactly.
    [[nodiscard]] bool FactionAllows(BotSnapshot const& snap) const;

    // 2D distance from the bot's snapshot position. Returns +inf when the bot
    // is on a different map (no in-map navigation can reach the hub anyway).
    [[nodiscard]] float GetDistanceFrom(BotSnapshot const& snap) const;

    // Score combining level fit, distance, faction, and quest count. 0 if not
    // appropriate; higher = better. Used by GetQuestHubsForBot ranking.
    [[nodiscard]] float CalculateSuitabilityScore(BotSnapshot const& snap) const;

    // Geometry helper: is the given world position inside this hub's radius?
    [[nodiscard]] bool ContainsPosition(Position const& pos) const;
};

class QuestHubDatabase
{
public:
    QuestHubDatabase() = default;
    ~QuestHubDatabase() = default;

    QuestHubDatabase(QuestHubDatabase const&) = delete;
    QuestHubDatabase& operator=(QuestHubDatabase const&) = delete;

    // Loads all quest givers and clusters them into hubs. Call once during
    // module init from the world thread; safe to call again via Reload().
    bool Initialize();

    // Drop & rebuild — for live data refresh without a server restart.
    bool Reload();

    [[nodiscard]] bool IsInitialized() const { return _initialized; }
    [[nodiscard]] size_t GetQuestHubCount() const;
    [[nodiscard]] size_t GetMemoryUsage() const;

    // Returns the closest hub the bot can use. nullptr if none qualify (no
    // hubs in level range on the bot's current map).
    [[nodiscard]] QuestHub const* GetNearestQuestHub(BotSnapshot const& snap) const;

    // Top-N hubs ranked by suitability (level fit + distance + faction +
    // quest count). Used by diagnostics; rules just want the nearest one.
    [[nodiscard]] std::vector<QuestHub const*> GetQuestHubsForBot(
        BotSnapshot const& snap, uint32 maxCount = 5) const;

    // R7: pick the best level-/faction-appropriate hub for a STARVED bot on
    // the continents its current level band + deterministic expansion route
    // to (see LevelingHubChoice above). Returns an invalid choice for the
    // L1-9 band, or when no appropriate hub exists on the band's continents
    // AND no whole-bracket fallback hub exists. The returned map_id MAY equal
    // the bot's current map (same-continent relocation) — the caller only
    // synthesizes a cross-map travel goal when it differs.
    //
    // AVAILABILITY: `is_doable`, when set, must return true only for a hub the
    // bot can still take a quest at (the shared HubHasDoableQuest predicate from
    // the builder, which owns the live Player). It is applied AMONG the
    // band/Chromie-ranked candidates so an EXHAUSTED hub is never selected.
    // THREADING CONTRACT: `is_doable` is invoked AFTER the internal `_mutex`
    // shared_lock is released (it calls live Player APIs which must not run under
    // the database lock); SelectLevelingHub collects candidate hub pointers under
    // the lock, releases it, then filters. QuestHub pointers stay valid post-init
    // (immutable `_questHubs`). When `is_doable` is empty (default) all candidates
    // are considered doable (diagnostic / non-availability callers).
    [[nodiscard]] LevelingHubChoice SelectLevelingHub(
        BotSnapshot const& snap,
        std::function<bool(QuestHub const&)> const& is_doable = {}) const;

    [[nodiscard]] QuestHub const* GetQuestHubById(uint32 hubId) const;
    [[nodiscard]] std::vector<QuestHub const*> GetQuestHubsInZone(uint32 zoneId) const;
    [[nodiscard]] QuestHub const* GetQuestHubAtPosition(
        Position const& pos,
        std::optional<uint32> zoneId = std::nullopt) const;

    // Iterate every hub. Used by UnifiedTravelGraph::LoadQuestHubs to seed
    // one routable graph node per hub. Read-locked; the callback must not
    // re-enter the database (deadlock). Callback returns void; iteration
    // is total (no early-exit needed since the caller wants every hub).
    template <typename Fn>
    void ForEach(Fn&& fn) const
    {
        std::shared_lock lk(_mutex);
        for (QuestHub const& h : _questHubs)
            fn(h);
    }

private:
    struct QuestGiverData
    {
        uint32 creatureEntry;
        Position position;
        uint32 mapId;
        uint32 zoneId;
        uint32 factionTemplate;
    };

    // Init pipeline — each step logs its own progress.
    uint32 LoadQuestGiversFromDB();
    uint32 ClusterQuestGiversIntoHubs();
    void   LoadQuestDataForHubs();
    void   BuildSpatialIndex();
    void   ValidateHubData();

    std::vector<QuestHub>      _questHubs;
    std::vector<QuestGiverData> _tempQuestGivers;
    std::unordered_map<uint32, size_t> _hubIdToIndex;
    std::unordered_map<uint32, std::vector<size_t>> _zoneIndex;

    // _mutex retained for use ONLY during Initialize() under write lock;
    // the post-init read path on the hot bot-tick walk goes through the
    // atomic _initialized fence and the immutable _zoneIndex / _questHubs
    // members (see GetQuestHubAtPosition for the ordering rationale).
    mutable std::shared_mutex _mutex;
    std::atomic<bool> _initialized{false};
    size_t _memoryUsage = 0;
};

} // namespace Playerbot::V2::Travel

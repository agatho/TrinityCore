// WedgeWatchdog - Runtime self-reporting layer for stuck ("wedged") bots.
//
// #1B. The detection SIGNALS already exist (BotAI::StuckTracker, the
// oscillation leash, path_blocked_count, RuleWedge slots, last_rule_fired).
// What was missing was a unified classification + reporting layer: until a
// human noticed, a wedged bot (e.g. Uraimus parked above the den running
// back-and-forth forever) was invisible. The watchdog consumes those cheap
// per-bot signals on the WORLD THREAD at a slow cadence, classifies each
// stuck episode into a root-cause WedgeCategory, emits exactly ONE structured
// log line per episode (dedup so a 10-minute wedge isn't 600 lines), feeds the
// per-category PerfCounters tally, and keeps a live active-wedge list the
// `.playerbot wedges` GM digest reads back.
//
// THREADING: Tick() and every read MUST run on the world thread (constructed
// + driven from PlayerbotV2.cpp's lifecycle). It only reads cheap BotAI
// accessors (is_wedged / wedge_since_ms / wedge_objective_string /
// last_rule_fired / path_blocked_count / stuck_tracker) and the eventually-
// consistent snapshot — never a live Player from a worker. The active-wedge
// vector + dedup map are world-thread-only state, so they need no locking;
// `.playerbot wedges` runs on the world thread too (command handler).

#pragma once

#include "Bot/BotTypes.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot {

class BotAI;

namespace Diagnostics {

// Root-cause classification for a wedge episode. Order is load-bearing: the
// enum value is cast to size_t for PerfCounters::record_wedge bucketing and
// must match PerfCounters::kWedgeCategoryCount. None sits at index 0 so a
// "not wedged" classification maps to the harmless zero bucket.
enum class WedgeCategory : uint8
{
    None = 0,         // not wedged / unclassifiable
    Navmesh,          // path repeatedly refused on connected mesh (NoPath)
    OffMesh,          // bot/dest off the navmesh (FarFromPoly / mid-air seam)
    Travel,           // wedged executing a walk_to_* / travel leg toward a hub/anchor
    CombatLoop,       // stuck in combat re-targeting / locked-cast churn
    PickerNone,       // idle picker emitted nothing actionable (no goal selected)
    GoalUnreachable,  // has a goal (quest POI / turn-in) it provably can't reach
    NoProgress,       // ACTING (moving / looping idle rules) but making no measurable
                      // progress on the composite metric — the movement-independent
                      // complement to the displacement-gated categories above
};

// Keep in lockstep with PerfCounters::kWedgeCategoryCount.
inline constexpr size_t kWedgeCategoryCount = 8;

char const* WedgeCategoryName(WedgeCategory c);

// POD description of one bot's current wedge. Rebuilt fresh each Tick for the
// active-wedge list; also carried into the log line + perf record.
struct WedgeInfo
{
    WedgeCategory cat       = WedgeCategory::None;
    uint32        since_ms  = 0;   // GameTimeMS at which the episode began
    uint32        map       = 0;
    float         x = 0.f, y = 0.f, z = 0.f;
    uint32        zone_id   = 0;
    uint32        duration_ms = 0; // now - since_ms at classification time
    std::string   objective;       // human-readable goal/POI provenance
    std::string   last_rule;       // last APL/idle rule the bot fired
    BotId         bot       = 0;
    std::string   bot_name;        // resolved from character cache for the digest
    // Identity of the objective the picker is routing current_objective_poi to,
    // captured from the SAME snapshot that defined this wedge so self-remediation
    // blacklists the EXACT (quest_id, objective.id) the snapshot builder checks via
    // objective_blacklisted(q.quest_id, o.id). Replaces the old ai.objective_track()
    // / volatile current_quest_id reads, which drifted off the wedged POI and made
    // remediation abandon the WRONG objective (Mokrah looped 22 min abandoning
    // q24459 while wedged on q24440; Sylvaen 28 min, obj-id mismatch). obj_id==0 is
    // the (quest,0) sentinel for a TURN-IN / BREADCRUMB goal with no actionable row.
    uint32        wedge_quest_id = 0;
    uint32        wedge_obj_id   = 0;
    // True when the wedge-defining snapshot's current_objective_poi could NOT be
    // resolved (poi.valid == false) — e.g. a TALKTO/escort objective whose target
    // is a mobile/script-spawned NPC with no static spawn (Tarindrella, q28725).
    // Such an objective can NEVER be pathed to, so the bot wanders forever without
    // ever displacement-wedging. This narrowly authorises NoProgress remediation
    // (temp-blacklist → grind/repick) for the genuinely-unpursuable case ONLY —
    // distinct from an XP plateau during legit non-XP work, which never remediates.
    bool          objective_poi_invalid = false;
};

// Classifier — free function so it's unit-testable and decoupled from the
// watchdog's bookkeeping. Reads cheap BotAI accessors + the bot's latest
// snapshot (for objective text, position, zone, path telemetry). Returns a
// WedgeInfo whose cat == None when the bot is not wedged.
//
// CALIBRATION (2026-06-15, from live ground-truth sampling): the primary wedge
// gate is ACTUAL PHYSICAL DISPLACEMENT, not StuckTracker straight-line progress
// to the goal. A bot winding through a multi-leg route moves hundreds of yards
// while its straight-line distance to a far/final goal barely changes, so the
// old goal-progress gate flagged actively-travelling bots (measured ~28% false
// positives: bots moving 200-400y/100s reported as "GoalUnreachable"). The
// watchdog now passes `stationary_for_ms` (how long the bot has stayed within
// kMinDisplacement of its position anchor). A bot is wedged only when it has
// been physically stationary for >= the threshold AND its own stuck signal
// agrees (wedge_since_ms != 0, so genuinely-idle/parked bots aren't reported).
// Combat uses a longer threshold (fights legitimately hold position).
WedgeInfo ClassifyWedge(BotAI& ai, BotId id, uint32 stationary_for_ms,
                        uint32 threshold_ms, uint32 combat_threshold_ms, uint32 now_ms);

class WedgeWatchdog
{
public:
    // Wedge-confirm threshold (ms of sustained no-progress before a bot is
    // reported). Default 90s — long enough that transient navmesh flukes and
    // normal long walks don't trip it, short enough that a real wedge surfaces
    // within ~1.5 min. Overridable from config at construction.
    static constexpr uint32 kDefaultThresholdMs = 90000;
    // Combat legitimately holds position (melee/cast), so a longer gate avoids
    // flagging normal prolonged fights; only a bot stuck fighting one spot this
    // long (unkillable dummy / talk-objective mis-attacked / opener-OOR loop)
    // surfaces as CombatLoop.
    static constexpr uint32 kDefaultCombatThresholdMs = 180000;
    // A bot that moved more than this (yards, same map) since its anchor is
    // travelling, not wedged — the anchor resets and the stationary clock with it.
    static constexpr float  kDefaultMinDisplacement   = 20.0f;

    explicit WedgeWatchdog(uint32 threshold_ms = kDefaultThresholdMs)
        : m_thresholdMs(threshold_ms) {}

    // World-thread. Walk the registry, classify each in-world bot, emit one
    // structured line per NEW episode (dedup keyed per bot on episode onset),
    // record the category in PerfCounters, and rebuild the active-wedge list.
    void Tick(uint32 now_ms);

    // Live active-wedge list from the most recent Tick (world-thread read).
    std::vector<WedgeInfo> const& active() const { return m_active; }

    // Per-category running episode totals (cumulative across the session),
    // mirrored from PerfCounters for the digest so the command needs only the
    // watchdog. Index = WedgeCategory.
    std::array<uint64_t, kWedgeCategoryCount> const& category_totals() const
    { return m_categoryTotals; }

    // #7: cumulative count of objectives auto-abandoned by self-remediation
    // (GoalUnreachable / CombatLoop wedges past RemediationMs). Surfaced for
    // the `.playerbot wedges` digest so operators see the layer working.
    uint64_t remediation_total() const { return m_remediationTotal; }

    uint32 threshold_ms() const { return m_thresholdMs; }
    void   set_threshold_ms(uint32 v) { m_thresholdMs = v; }
    void   set_combat_threshold_ms(uint32 v) { m_combatThresholdMs = v; }
    void   set_min_displacement(float v) { m_minDisplacement = v; }
    // No-progress (XP-rate) detector knobs (see ConfigReader / Tick()).
    void   set_noprogress_enabled(bool v) { m_noProgressEnabled = v; }
    void   set_noprogress_ms(uint32 v) { m_noProgressMs = v; }
    void   set_noprogress_radius(float v) { m_noProgressRadius = v; }

private:
    uint32 m_thresholdMs;
    uint32 m_combatThresholdMs = kDefaultCombatThresholdMs;
    float  m_minDisplacement   = kDefaultMinDisplacement;

    // No-progress detector configuration. The window before a non-progressing
    // bot is flagged; the net-displacement radius past which it counts as
    // travelling (anchor resets) rather than stuck-in-place; an enable toggle.
    bool   m_noProgressEnabled = true;
    uint32 m_noProgressMs      = 900000;   // 15 min
    float  m_noProgressRadius  = 150.0f;
    // Bots whose archetype dominant activity is Profession (intentional
    // low/no-XP grinders) get the window multiplied by this factor.
    static constexpr uint32 kProfessionWindowMul = 3;

    // Per-bot position anchor for the displacement gate. While the bot stays
    // within m_minDisplacement of (x,y) on the same map, `ms` marks when it
    // first parked there; once it moves past the radius (or changes map) the
    // anchor resets to the new spot and the stationary clock restarts.
    struct PosAnchor { uint32 map = 0; float x = 0.f, y = 0.f; uint32 ms = 0; bool valid = false; };
    std::unordered_map<BotId, PosAnchor> m_posAnchor;

    // Per-bot COMPOSITE-progress anchor for the no-progress detector. Stores the
    // last fingerprint at which the bot made progress AND the position it held
    // then; `ms` is when that snapshot was taken. The anchor (and its clock)
    // reset whenever ANY fingerprint component advances (XP/level, quests, gold,
    // total skill points, bag-item count — so gathering/fishing/crafting/economy
    // all count as progress) OR the bot travels net > m_noProgressRadius from the
    // anchor (a long walk is progress toward a goal, not a stall). A bot is
    // no-progress-wedged only when now - ms >= the (archetype-scaled) window.
    struct ProgressAnchor
    {
        uint32 map = 0; float x = 0.f, y = 0.f;
        uint32 level = 0;          // identity.level
        uint32 xp = 0;             // identity.xp (resets on ding; level guards that)
        uint32 quests_done = 0;    // quest_log.completed_quest_count
        int32  gold = 0;           // inventory.gold (copper)
        uint32 skill_sum = 0;      // sum of progression.skills[].value
        uint32 bag_items = 0;      // inventory.bag_items.size()
        uint32 ms = 0;
        bool   valid = false;
    };
    std::unordered_map<BotId, ProgressAnchor> m_progress;

    // Per-bot dedup of the CURRENT episode. Stores the onset timestamp we last
    // reported for this bot; a fresh episode (different onset) re-reports.
    // Pruned when a bot stops being wedged so the map can't grow unbounded.
    std::unordered_map<BotId, uint32> m_reportedOnset;

    std::vector<WedgeInfo> m_active;
    std::array<uint64_t, kWedgeCategoryCount> m_categoryTotals{};
    uint64_t m_remediationTotal = 0;   // #7 objectives auto-abandoned

    // #7 follow-up — persistent STUCK-OBJECTIVE LEDGER. Each remediation
    // accumulates a per-(quest_id,obj_id) aggregate here on the world thread;
    // FlushStuckLedger UPSERTs the dirty rows into playerbot_v2_stuck_objective
    // on a slow cadence so the fleet self-documents which content strands bots
    // (queryable across restarts for later root-cause) instead of leaving only
    // the truncated [wedge_remediate] log. World-thread-only state -> no lock.
    struct StuckAgg
    {
        uint32      pending_delta = 0;   // remediations since the last DB flush
        uint64      total         = 0;   // session-cumulative (digest / debug)
        std::string category;            // last WedgeCategory name that hit it
        uint32      map = 0, zone = 0;
        float       x = 0.f, y = 0.f;
        std::string bot;                 // a representative stuck bot name
    };
    std::unordered_map<uint64, StuckAgg> m_stuckLedger;   // key = (quest_id<<32)|obj_id
    uint32 m_lastLedgerFlushMs = 0;
    static constexpr uint32 kLedgerFlushMs = 60000;       // upsert dirty rows ~1/min
    void FlushStuckLedger(uint32 now_ms);
};

} // namespace Diagnostics
} // namespace Playerbot

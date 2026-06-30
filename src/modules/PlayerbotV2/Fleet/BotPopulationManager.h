// BotPopulationManager - Distribution shaper that keeps the world stocked
// with bots at every level bracket per a configured shape. Phase A of
// WORLD_POPULATION_PLAN.md.
//
// Ticks every 60s from Module::OnWorldUpdate (configurable). Each tick:
//   1. Snapshot current online bot counts per (level_bucket, faction).
//   2. Compute target counts from config (Total / Shape / AllianceRatio).
//   3. Reconcile: prefer login of offline bots that fit; else JIT-create
//      via BotCharacterFactory; else logout LRU when over-target.
//   4. Throttle: max SpawnRatePerTick creations + LoginRatePerTick logins.
//
// Idempotence: bots with distribution_level > 0 keep their level. The
// shaper only ever raises a bot's distribution level, never lowers it.

#pragma once

#include "Bot/BotTypes.h"
#include <unordered_map>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <string>

namespace Playerbot::V2::Fleet {

class BotSetupPipeline;

enum class PopulationShape : uint8 { MaxHeavy, Pyramid, Bell, Flat };

struct PopulationBucket
{
    uint8  level_lo;
    uint8  level_hi;
    uint32 alliance_target;
    uint32 horde_target;
    uint32 alliance_actual = 0;
    uint32 horde_actual    = 0;
};

struct PopulationSnapshot
{
    uint32 total_target = 0;
    uint32 total_actual = 0;
    std::vector<PopulationBucket> buckets;
};

class BotPopulationManager
{
public:
    BotPopulationManager();
    ~BotPopulationManager();

    // Tick driver. Called from Module::OnWorldUpdate. Internally rate-limits
    // to TickIntervalSec (default 60) so per-frame calls are cheap.
    void OnWorldTick(uint32 now_ms);

    // Force an immediate reconciliation regardless of the tick gate. Used
    // by .playerbot population force / restart handlers.
    void ForceReconcile();

    // Snapshot current state for diagnostics. Computes target on demand.
    PopulationSnapshot Snapshot() const;

    // Diagnostic accessors for /health. All return last-tick values; safe
    // to read from the world thread without locks (writers are also the
    // world thread). The "ms_until_next_*" helpers return 0 if the gate
    // has already elapsed (next call will fire immediately).
    uint32 spawn_budget() const     { return spawn_budget_alli_ + spawn_budget_horde_; }
    uint32 login_budget() const     { return login_budget_alli_ + login_budget_horde_; }
    uint32 last_tick_ms() const     { return last_tick_ms_; }
    uint32 last_hygiene_ms() const  { return last_hygiene_ms_; }
    // Wall-clock ms remaining until the next reconcile / hygiene fires.
    uint32 ms_until_next_reconcile(uint32 now_ms) const;
    uint32 ms_until_next_hygiene(uint32 now_ms) const;
    uint32 ms_until_next_rebalance(uint32 now_ms) const;
    uint32 last_rebalance_ms() const { return last_rebalance_ms_; }
    // Marker count of bots whose RunFor returned true since process start.
    size_t setup_done_cache_size() const { return setup_done_cache_.size(); }

    // Run the per-bot setup pipeline for any in-world bot whose
    // distribution_level > 0 but setup_pipeline_state != AllDone. Cheap
    // when nothing pending. Called every tick alongside reconcile.
    void DriveSetupPipelines();

    // Priority-setup register. Bots in this set are drained from
    // DriveSetupPipelines FIRST and without the per-tick 10-bot cap.
    // Used by `.playerbot smoketest`: under a 1000+ bot population the
    // normal cap starves freshly-spawned smoketest bots indefinitely.
    // Auto-removes entries when their bot reaches AllDone or when
    // explicitly unregistered (e.g. test cleanup). Caller responsible
    // for unregistering on abort. Thread-safe — guarded by priority_mtx_.
    void RegisterPrioritySetup(uint64 bot_id);
    void UnregisterPrioritySetup(uint64 bot_id);

    // Hygiene cleanup (Phase E of WORLD_POPULATION_PLAN):
    //   - Delete JIT-spawned bots that haven't been used in N days.
    //   - Hard-cap protection: log out everyone if total online > 2x target.
    // Runs at most every kHygieneIntervalMs (default 1h).
    void RunHygiene(uint32 now_ms);

    // Re-gear backfill for under-geared / weaponless ONLINE bots. Separate from
    // the 1h hygiene so it runs SOON after login (the 1h pass fires once at boot
    // before the fleet is online, wasting it) and drains the historical
    // under-geared backlog at 25 bots / 5 min. Reuses the gear generator + the
    // SAFE SwapItem equip path. 2026-06-17.
    void RunGearBackfill(uint32 now_ms);

    // Failed-JIT corpse reclaim (500/pass). Called from RunHygiene and on
    // a 10-min OnWorldTick cadence while the backlog drains.
    void SweepFailedJitCorpses();

    // Kick-protection lease. JIT match-fill bots (BotQueueFiller spawn_jit)
    // ride ABOVE TotalTarget by design, but between their deferred login
    // and the moment the setup pipeline fires the queue intent they look
    // like ordinary overshoot -- no group, no BG queue, normal map -- and
    // the Reconcile overflow kick logged them out before they could ever
    // queue (observed live: [QueueFill] online_seen stuck at target while
    // spawn_jit succeeded every cycle). The filler leases protection at
    // spawn; every kick path checks it. World-thread only (no lock).
    void ProtectFromKick(uint64 guid_low, uint32 duration_ms);
    bool IsKickProtected(uint64 guid_low, uint32 now_ms) const;

    // Operator-created persistent bots (distribution_level == 0) are dev/test
    // subjects spawned via `.playerbot create`, not ambient population. The
    // reconciler never refills them (its candidate query filters
    // distribution_level > 0) and the setup pipeline stamps a positive level
    // on every shaped/ambient bot, so dist_level == 0 uniquely identifies an
    // operator bot. They must stay online for the life of the process — every
    // ambient logout path (session-rhythm, overflow LRU, hygiene hard-cap)
    // exempts them. dist_level is read from pipeline_row_cache_, which
    // DriveSetupPipelines fills for every in-world bot within ~1s of login;
    // an as-yet-uncached bot returns false (not exempt), but that sub-second
    // window is far shorter than any session/overflow eligibility horizon.
    bool IsOperatorPersistentBot(uint64 guid_low) const;

    // Defer a headless login to the next world tick(s). REQUIRED for any
    // login that immediately follows BotCharacterFactory::Create: the
    // factory's SaveToDB commits asynchronously, and a same-tick LoginBot
    // races the DB worker -- Player::LoadFromDB fails and the session is
    // kicked ("WorldSession::HandlePlayerLogin Player::LoadFromDB failed",
    // observed live: 542 JIT chars created / 1 ever entered world).
    // OnWorldTick drains kLoginDrainPerTick per tick.
    void DeferLogin(uint64 guid_low) { deferred_logins_.push_back(guid_low); }

    // BG prep-phase top-up. Walks all known BG maps via BattlemasterList,
    // finds BGs in STATUS_WAIT_JOIN with free slots, and re-issues
    // BotQueueFiller::Fill for each so additional bots join the queue and
    // get auto-pulled into the BG by the FreeSlotQueue mechanism. Keeps
    // running until the BG is fully populated (max players per team) or
    // gates open (STATUS_IN_PROGRESS). Rate-limited to once per 5s.
    void TopUpActiveBGs(uint32 now_ms);

    // Autonomous BG seeding (audit B24): with no real player ever queueing,
    // OnPlayerJoinedBgQueue never fires, BotQueueFiller never runs, and the
    // entire BG subsystem (13 scripts, idle rules, outcome learning) sits
    // dormant — wins=0/losses=0 forever. Every few minutes, when no BG is
    // active, seed one bot-vs-bot match by Fill()ing both factions of a
    // rotating bg_type at the max-level bracket. Gated by
    // Playerbot.Bg.AutoSeed.Matches in playerbot.conf (0 disables).
    void SeedBgMatches(uint32 now_ms);

    // Autonomous ARENA seeding. Same rationale as SeedBgMatches but for
    // arena skirmishes, which the BG matchmaker (BotQueueFiller) cannot
    // drive because arenas queue as a GROUP keyed by team size, not as
    // independent solo queuers. Every few minutes, when arena seeding is
    // enabled, publish one ArenaTeamForming CoordEvent per faction;
    // BotGroupBuilder forms an arena_type-sized group per side and the
    // leader queues it via the Arena queue id. Gated by
    // Playerbot.Bg.AutoSeed.Arena in playerbot.conf (0 = off, default).
    // Honors Playerbot.Bg.AutoSeed.ForceType when that id is an arena BML.
    void SeedArenaMatches(uint32 now_ms);

    // Fleet rebalance pass (proactive spec rotation): walks online bots
    // grouped by (faction, bracket), counts tank/healer/DPS, and
    // proactively switches under-rep brackets' hybrid DPS bots into
    // tank/healer specs. Reduces JIT-spawn pressure during peak queue
    // hours by getting the fleet's spec mix right BEFORE
    // BotQueueFiller::Fill demands it. Per-bot 60min cooldown + 1
    // switch per tick globally so a fleet of 2000 doesn't experience a
    // mass-respec spike. Runs at most every kRebalanceIntervalMs.
    void RunRebalance(uint32 now_ms);

    // Bypass the 5-min throttle and run a rebalance pass right now.
    // Used by the `/rebalance` diagnostic whisper to verify the cron's
    // behaviour without waiting for the next natural cycle. Returns
    // the number of switches applied.
    uint32 ForceRebalance();

    // Live per-bracket spec coverage report. Walks online bots and
    // counts tank/heal/dps per (faction, level-bracket). Used by the
    // /popstats whisper so operators can verify rebalance is producing
    // the 1:1:3 mix and spot brackets that are tank/heal-starved.
    struct BracketReport
    {
        uint32 bracket_key  = 0;   // (alliance ? 0x10000 : 0) | (level/10)
        uint16 tanks        = 0;
        uint16 healers      = 0;
        uint16 dps          = 0;
    };
    std::vector<BracketReport> BracketCoverage() const;

private:
    // guid_low -> getMSTime() expiry of the kick-protection lease.
    std::unordered_map<uint64, uint32> kick_protect_until_ms_;

    // #5 session-rhythm: guid_low -> getMSTime() the bot's CURRENT play
    // session began. Stamped lazily on first Reconcile sighting (staggered so
    // the boot cohort doesn't all expire together) and erased on session-end
    // logout so the next login gets a fresh budget. World-thread only (only
    // Reconcile touches it) -> no lock. See SessionRhythmLogout.
    std::unordered_map<uint64, uint32> session_start_ms_;

    void Reconcile();
    // #5: log out bots whose online time exceeded their archetype's
    // (jittered) target_session_minutes, so the online roster churns like a
    // human playerbase. Runs at the TOP of Reconcile so the per-bucket fill
    // replaces the vacancies in the SAME cycle (stable headcount, rotating
    // membership). Config-gated (PlayerbotV2.SessionRhythm.*).
    void SessionRhythmLogout(uint32 now_ms);
    std::vector<PopulationBucket> ComputeTargets() const;
    void PopulateActual(std::vector<PopulationBucket>& buckets) const;

    // Returns ChrCharacterData rows of offline bots matching (faction, level).
    // Used for "prefer login of existing bot" path.
    struct OfflineCandidate { uint64 char_guid; uint8 level; bool alliance; };
    std::vector<OfflineCandidate> FindOfflineCandidatesIn(uint8 lo, uint8 hi, bool alliance, uint32 cap) const;

    // JIT spawn one new bot at the given (level, faction). Returns the new
    // character's GUID counter, or 0 on failure. Decrements the faction's
    // per-cycle spawn budget on success.
    uint64 SpawnNew(uint8 level, bool alliance);

    // Login an offline bot character. Decrements the faction's per-cycle
    // login budget on success. Returns true on success.
    bool LoginExisting(uint64 char_guid, bool alliance);

    // Logout an over-target bot. LRU policy on last_seen_at.
    bool LogoutLRU(uint8 lo, uint8 hi, bool alliance);
    // Pass G: batch variant — single registry scan, up to `count`
    // victims. Replaces the prior N × full-scan loop. Returns the
    // number of bots logged out (≤ count).
    uint32 LogoutLRUMany(uint8 lo, uint8 hi, bool alliance, uint32 count);

    uint32 last_tick_ms_         = 0;
    uint32 last_hygiene_ms_      = 0;
    uint32 last_gear_backfill_ms_ = 0;
    // Per-bot cooldown for the gear backfill: a bot that PASSES the under-gear
    // gate but cannot actually be re-geared (bags full -> StoreNewItem fails, or
    // CanEquipItem refuses every slot -> swapped=0) is parked here for 30 min so
    // it does NOT re-consume the 25/pass processed-cap every pass and starve the
    // re-gearable bots downstream. Cleared as soon as a re-gear succeeds.
    std::unordered_map<uint64, uint32> gear_backfill_skip_until_;
    uint32 last_rebalance_ms_    = 0;
    uint32 last_drive_pipelines_ms_ = 0;
    uint32 last_bg_topup_ms_     = 0;
    uint32 last_bg_running_topup_ms_ = 0;  // separate timer for STATUS_IN_PROGRESS BG refill
    uint32 last_bg_seed_ms_      = 0;      // autonomous BG seeding cadence (SeedBgMatches)
    uint32 last_arena_seed_ms_   = 0;      // autonomous arena seeding cadence (SeedArenaMatches)
    uint32 bg_seed_rotation_     = 0;      // round-robins the seeded bg_type
    uint32 last_corpse_sweep_ms_ = 0;      // 10-min failed-JIT sweep cadence
    bool   bg_seed_match_seen_   = false;  // sticky rotation: advance only after a match formed
    // Per-faction reconcile budgets, reset each cycle. Splitting these
    // per side fixes faction starvation: previously both factions drew
    // from a single 25/25 pool and the bucket loop always processed
    // Alliance first, burning the whole pool before Horde got a turn.
    // Observed 2026-05-15: alli=603/1252 vs horde=73/1248 with HordePct=50.
    uint32 spawn_budget_alli_   = 0;
    uint32 spawn_budget_horde_  = 0;
    uint32 login_budget_alli_   = 0;
    uint32 login_budget_horde_  = 0;

    // In-memory cache of bots whose pipeline is finished. Necessary because
    // PersistState() uses async PExecute - the UPDATE is queued but the next
    // tick's sync PQuery (ReadState) can read BEFORE the async write commits.
    // Without this cache, RunFor would re-run every tick (re-invoking
    // DoPlaceAndTravel which TeleportTo's the bot) until the DB worker drains
    // its queue, which under load can take tens of minutes. The cache breaks
    // the loop on the very next tick after RunFor returned true.
    std::unordered_set<uint64> setup_done_cache_;

    // Priority-setup set — see RegisterPrioritySetup. Drained first and
    // without the per-tick budget cap in DriveSetupPipelines. Bounded to
    // small N (typically <100, one smoketest worth). The mutex protects
    // against the smoketest worker thread mutating the set while the
    // world thread reads it inside DriveSetupPipelines.
    std::unordered_set<uint64> priority_setup_ids_;
    std::mutex priority_mtx_;

    // Pass H: in-memory mirror of the playerbot_v2_character row so
    // DriveSetupPipelines doesn't re-SELECT once per second per in-world
    // non-done bot. Loaded lazily on first observation, refreshed when
    // we mutate state inline (jit_for_queue=NULL, AllDone transition).
    // Pipeline's own internal ReadState/PersistState are separate and
    // not eliminated by this cache.
    struct PipelineRow
    {
        uint8 state      = 0;
        uint8 dist_level = 0;
        std::string jit_tag;
    };
    std::unordered_map<uint64, PipelineRow> pipeline_row_cache_;

    // Deferred login queue. SpawnNew creates a character via async SaveToDB
    // and pushes the guid here instead of calling LoginBot directly. The
    // next OnWorldTick drains this queue - by that point the async DB
    // worker has committed the INSERT, so LoginBot's LoginQueryHolder can
    // SELECT the row. Without this, mass-spawn under load (wipefleet)
    // produced "Player::LoadFromDB ... not found in table characters"
    // because LoginBot fired before the commit landed.
    std::vector<uint64> deferred_logins_;

    // Granular pop-subphase perf accounting. Aggregated across a 60s
    // window and emitted via [PopPerf] once per window so we can identify
    // which sub-phase dominates at scale (observed 2026-05-18: pop=14s/tick
    // at 3160 bots — needed the breakdown to find the killer). Reset
    // each emit. See OnWorldTick / Reconcile / LogoutLRU for accumulation.
    uint32 perf_window_start_ms_         = 0;
    uint32 perf_window_drain_ms_         = 0;
    uint32 perf_window_pipelines_ms_     = 0;
    uint32 perf_window_hygiene_ms_       = 0;
    uint32 perf_window_rebalance_ms_     = 0;
    uint32 perf_window_bgtopup_ms_       = 0;
    uint32 perf_window_reconcile_ms_     = 0;
    uint32 perf_window_compute_targets_ms_ = 0;
    uint32 perf_window_populate_actual_ms_ = 0;
    uint32 perf_window_reconcile_side_ms_  = 0;
    uint32 perf_window_login_existing_ms_  = 0;
    uint32 perf_window_spawn_new_ms_       = 0;
    uint32 perf_window_logout_lru_ms_      = 0;

    BotSetupPipeline* pipeline_ = nullptr;
};

} // namespace Playerbot::V2::Fleet

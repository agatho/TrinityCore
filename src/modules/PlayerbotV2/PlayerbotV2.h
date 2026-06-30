// Playerbot V2 - Module entry
// Per MODULE_LAYOUT.md §2 (top-level) and CONTRACTS.md §9.

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Fleet/BotQueueFiller.h"
#include "Diagnostics/WedgeWatchdog.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
class Unit;
class Aura;
class Group;

namespace Playerbot { class SnapshotBuildPool; }

namespace Playerbot::V2 {

class Module {
public:
    static Module& instance();

    // Lifecycle
    void Init();      // Called once during worldserver startup
    void Shutdown();  // Called during worldserver shutdown

    // Per-tick driver
    void OnWorldUpdate(std::chrono::milliseconds diff);

    // Hook handlers — called by core via PlayerbotHooks.cpp.
    // Login/Logout/Death/DamageTaken/HealReceived/Whisper carry meaningful
    // dispatch (registry, loot tap, event-bus push, command parser). The
    // remaining hooks are intentionally inert — the snapshot delta covers
    // their use cases (see PlayerbotV2.cpp comments for rationale).
    void OnPlayerLogin(Player* p);
    void OnPlayerLogout(Player* p);
    void OnLevelUp(Player* p, uint8 new_level);
    void OnDeath(Unit* victim, Unit* killer);
    void OnResurrect(Player* p);
    void OnSpecChanged(Player* p, uint8 new_spec);
    void OnDamageDealt(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
    void OnDamageTaken(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
    void OnHealReceived(Unit* healer, Unit* target, int32 amount, uint32 spell_id);
    void OnAuraApplied(Unit* target, Aura* aura);
    void OnAuraRemoved(Unit* target, Aura* aura);
    void OnGroupMemberJoined(Group* g, Player* p);
    void OnGroupMemberLeft(Group* g, Player* p);
    void OnWhisperReceived(Player* sender, Player* receiver, std::string const& msg);
    void OnPartyChat(Player* sender, Group* group, std::string const& msg);
    // Phase C.3: route guild chat messages to BotChatReactor::ReactGuild.
    void OnGuildChat(Player* sender, uint64 guild_id, std::string const& msg);

    // SC-P1a: /say and /yell social reactions. Route to BotChatReactor's
    // range-gated say/yell paths so a single nearby bot can answer.
    void OnSayChat(Player* sender, std::string const& msg);
    void OnYellChat(Player* sender, std::string const& msg);
    // SC-P2c: reciprocate a nearby player's text emote (/wave, /salute, ...).
    void OnTextEmote(Player* sender, uint32 emote_id, ObjectGuid target);
    // SC-P2b: a new guild member joined — one online bot guildmate welcomes.
    void OnGuildMemberAdded(uint64 guild_id, ObjectGuid joiner_guid, std::string const& joiner_name);

    // Queue auto-fill hooks (Phase D of WORLD_POPULATION_PLAN). Called by
    // core when a Player joins a BG / LFG queue; we hand off to
    // BotQueueFiller to invite online + JIT-spawn missing roles.
    void OnPlayerJoinedBgQueue(Player* player, uint32 bg_type_id, uint8 bracket);
    void OnPlayerJoinedLfg(Player* player, uint32 dungeon_id, uint8 role_mask);

    // Fired synchronously from BattlegroundQueue::InviteGroupToBG (and its
    // reminder event). Bots get a BgPortIntent pushed immediately so they
    // port within the same tick as the invite, instead of waiting for the
    // next snapshot rebuild + idle:bg_port_accept rule firing — which under
    // load can take seconds and miss the 90s INVITE_ACCEPT_WAIT_TIME window
    // for slower-to-tick bots. Non-bots no-op.
    void OnBGInvitationReceived(Player* player, uint32 bg_instance_id, uint32 bg_type_id);

    // Fired from LFGMgr::AddProposal the instant TC sends a proposal to a
    // candidate. For bots, push LfgProposalRespondIntent within ~1 tick
    // (deterministic per-bot 0-800ms stagger via FireDuePending mechanism).
    // Group is fixed-size (5 / 25), no human-first gate needed — all
    // members must accept for the proposal to advance.
    void OnLfgProposalReceived(Player* player, uint32 proposal_id);

    // Diagnostic — fired from HandleBattleFieldPortOpcode silent-return
    // gates. Logs reason + player + bg_instance_id so "click Enter no port"
    // reports always have the exact failure cause. Inert for bots.
    void OnBGPortFailed(Player* player, uint8 reason_code, uint32 bg_instance_id);

    // Path-outcome telemetry from PlayerbotAPI::move_to. The argument is a raw
    // uint8 (matching PerfCounters::PathOutcome) to avoid leaking the V2-side
    // enum definition into core headers.
    void OnPathOutcome(uint8 outcome);

    // (Additional hooks listed in MODULE_LAYOUT.md §4 are added here as the
    // corresponding subsystems are implemented per FEATURE_MATRIX.md.)

    // Diagnostics
    bool IsInitialized() const { return initialized_; }

    // Snapshot of the in-flight TickPerf accumulator window. Lets whisper
    // commands (`/tickperf`) surface the current per-phase ms + throughput
    // counters without waiting for the next 60s log emit. All ms values
    // are TOTALS over the current window — divide by ticks_in_window for
    // averages. Counters are raw window totals.
    struct TickPerfSnapshot
    {
        uint32 ticks_in_window;
        uint32 snap_ms_total;
        uint32 snap_setup_ms_total;
        uint32 snap_build_ms_total;
        uint32 group_build_ms_total;
        uint32 snap_built_count;
        uint32 snap_skipped_count;
        uint32 intents_drained;
        uint32 bgport_ms_total;
        uint32 drain_ms_total;
        uint32 session_ms_total;
        uint32 pop_ms_total;
        uint32 sched_ms_total;
        uint32 total_ms_total;
        uint32 total_ms_max;
    };
    TickPerfSnapshot tickperf_snapshot() const;

    // #1B runtime wedge watchdog. Owned by the module (world-thread driven from
    // OnWorldUpdate at kWedgeWatchdogIntervalMs cadence). The `.playerbot
    // wedges` digest reads its active-wedge list + per-category totals. Const +
    // mutable accessors mirror how other world-thread services are reached.
    Diagnostics::WedgeWatchdog const& wedge_watchdog() const { return wedge_watchdog_; }
    Diagnostics::WedgeWatchdog&       wedge_watchdog()       { return wedge_watchdog_; }

private:
    // Drains pending bot intents and executes them via PlayerbotAPI on the
    // world thread. Returns the number of intents actually executed in this
    // call so TickPerf can surface drain throughput at scale.
    size_t DrainIntents();
    // Fire any due deferred BG ports (see OnBGInvitationReceived). Called
    // every world tick before DrainIntents so the BgPortIntent appears in
    // the queue right before draining.
    void FireDueBgPorts();
    // Fire any due deferred LFG proposal accepts (see OnLfgProposalReceived).
    // Pushes LfgProposalRespondIntent into per-bot IntentQueue once the
    // per-bot stagger elapses.
    void FireDueLfgAccepts();
    // #1C fleet-vitals sample: build one VitalsBucket from the supplied 60s
    // census aggregates + the live PerfCounters / wedge-watchdog state, push
    // it into the in-memory rolling window, async-write one persisted row, and
    // evaluate the config-driven alert thresholds (throttled). Called once per
    // 60s from the FleetStatus block in OnWorldUpdate (world thread). The
    // census args are passed in so we don't walk the registry twice.
    void SampleFleetVitals(std::chrono::milliseconds total, uint32 now_ms,
                           uint32 in_world, uint32 alive, uint32 in_combat,
                           uint32 avg_level_x100);
    // 30s cron — walks pending_lfg_refills_, drops entries whose
    // requesting player is no longer in LFG_STATE_QUEUED, and re-fires
    // BotQueueFiller::Fill with the role deficit for the rest. Fills
    // the missing roles when the initial Fill came up short.
    void TopUpPendingLfg(uint32 now_ms);

    Module() = default;
    Module(Module const&) = delete;
    Module& operator=(Module const&) = delete;

    // Deferred BG port queue. Populated by OnBGInvitationReceived; drained
    // by FireDueBgPorts. Bots MUST NOT port the same tick the invite is
    // sent — they'd flood the BG instance before the real player's
    // CMSG_BATTLEFIELD_PORT lands, the BG state would advance, and the
    // player's invite would fail silently in HandleBattleFieldPortOpcode.
    //
    // Gating policy (humans first):
    //   - Track per-BG-instance whether any human was invited (`expected_humans`).
    //   - If 0 humans invited → fire bot ports with light stagger (bot-only test BG).
    //   - If 1+ humans invited → wait until at least 1 human is IN the BG
    //     instance, then fire bots staggered 0-1500ms.
    //   - Hard expiry: drop the deferred entry if invite age > 80s (TC's
    //     90s INVITE_ACCEPT_WAIT_TIME minus a 10s safety margin).
    struct PendingBgPort {
        uint32 created_at_ms;
        uint32 bg_instance_id;
        uint64 bot_id;
        uint16 bg_type_id;
        uint32 bg_template_type_id;   // BattlegroundTypeId for sBattlegroundMgr lookup
    };
    std::vector<PendingBgPort> pending_bg_ports_;
    // Per-instance invite-state for human-first gating. Keyed by bg_instance_id.
    struct BgInviteState {
        uint32 expected_humans = 0;
        bool   any_human_seen = false;   // sticky once a human is detected in-bg
    };
    std::unordered_map<uint32, BgInviteState> bg_invite_state_;
    std::mutex                 pending_bg_ports_mtx_;

    // Deferred LFG-proposal accept queue. Mirror of pending_bg_ports_ but for
    // LFG group proposals. No human-first gate — proposals require all
    // members to accept within LFG_TIME_PROPOSAL (40s). Per-bot stagger
    // 0-800ms so we don't fire 25 LfgProposalRespondIntents the same tick.
    struct PendingLfgAccept {
        uint32 created_at_ms;
        uint64 bot_id;
        uint32 proposal_id;
    };
    std::vector<PendingLfgAccept> pending_lfg_accepts_;
    std::mutex                    pending_lfg_accepts_mtx_;

    // Periodic LFG queue top-up. Mirrors the BG TopUpActiveBGs pattern.
    // OnPlayerJoinedLfg fires Fill ONCE; if a tank/heal/dps bot fails
    // to spawn or queue (account starvation, name collision, etc.) the
    // queue stays partial and the human waits indefinitely. Observed
    // 2026-05-16: queued Ragefire, 1T+1D filled, 1H+1D still missing
    // 60s later. Solution: on a 30s cron, walk pending LFG queues,
    // detect which the player is still actively waiting in, count bots
    // already queued for that dungeon by role, and re-fire Fill with
    // the role deficit.
    struct PendingLfgRefill {
        uint64                              player_guid_low;
        Fleet::BotQueueFiller::FillRequest  req;
        uint32                              created_at_ms;
        uint32                              last_refill_ms;
    };
    std::vector<PendingLfgRefill> pending_lfg_refills_;
    std::mutex                    pending_lfg_refills_mtx_;

    bool initialized_ = false;
    // First-tick auto-resume gate: if `Playerbot.V2.AutoResumeOnBoot` is true,
    // submit a headless login for every marked-as-bot character not already
    // in-world on the first OnWorldUpdate tick. Done from world-tick rather
    // than Module::Init so the world has fully come up (maps loaded, etc).
    // Was a bool ("resume on first post-gate tick"). Now an integer cap
    // that's drained kBootSpawnPerTick at a time across many ticks.
    // Symptom that prompted the change: AutoResume's previous one-shot
    // LoginAll(100) burst hit `battlenet_account_mounts` row-lock
    // contention on shared pool bnet rows — 10 bots per pool account
    // all REPLACEing into the same bnet row simultaneously serialized,
    // queue length blew past innodb_lock_wait_timeout (50s), world
    // thread blocked on the synchronous transaction commit, FreezeDetector
    // tripped at 60s. Spreading the resume over many ticks keeps each
    // tick's bnet-write load below the lock-contention threshold.
    uint32 auto_resume_pending_ = 0;
    // First-tick auto-spawn target: if `Playerbot.V2.AutoSpawnOnBoot` > 0
    // and the marked bot count is below that target after AutoResume, the
    // module batch-creates the shortfall via BotComposition + factory.
    // Hard-capped at AUTO_SPAWN_HARD_CAP to prevent surprise mass-spawn
    // from a config typo. Decremented over multiple ticks (kBootSpawnPerTick
    // per tick) so the world thread stays responsive to human-login packets
    // during the mass-spawn pass. Decrements to 0 over time, not on first tick.
    uint32 auto_spawn_pending_ = 0;
    // Boot-time human-login priority gate.
    //
    // Symptom that prompted this: after wiping all bots, AutoSpawn fired
    // synchronously on tick #1 (~200 × SaveToDB ≈ 6 s of blocking on the
    // world thread), and the AuthServer→WorldServer human login handshake
    // sat in the session queue the whole time. Human login appeared to
    // hang; player gave up before bots finished.
    //
    // Fix: defer the boot-time AutoResume and AutoSpawn passes until one
    // of two conditions:
    //   (a) `kBootGraceMs` ms have elapsed since first OnWorldUpdate
    //       (lights-out servers still come up after a known delay), OR
    //   (b) any non-bot WorldSession is in-world (sticky — once we've
    //       seen a human, the gate stays open for the rest of the boot).
    // After the gate opens, AutoSpawn is rate-limited to
    // kBootSpawnPerTick per OnWorldUpdate so a 200-bot boot spreads over
    // ~40 s instead of slamming a single tick.
    bool          boot_gate_open_         = false;
    std::chrono::milliseconds boot_first_tick_at_{0};
    // Wall-clock total since module init for the periodic fleet-status log.
    // Logged every kFleetLogIntervalMs to give the operator a one-line
    // overnight-friendly snapshot in worldserver.log without requiring a
    // GM session. Aggregated by walking the registry + snapshots once per
    // log tick (cheap — a few hundred microseconds for typical fleet size).
    std::chrono::milliseconds fleet_log_total_{0};
    std::chrono::milliseconds last_fleet_log_at_{0};

    // #1C fleet-vitals sampler state. Co-located with the 60s FleetStatus log
    // (same cadence, same registry walk). Each sample needs the PREVIOUS
    // sample's cumulative counter values to derive per-window RATES
    // (path-fail/min, intents/sec, intents-dropped delta), so we cache them
    // here. sample_ready_ flips true after the first sample so the second
    // sample onward has a valid baseline for the deltas (the first sample
    // emits rate=0). Per-metric alert throttle timestamps suppress repeat
    // [fleet_alert] spam within ConfigReader::alert_throttle_ms.
    bool   vitals_sample_ready_      = false;
    uint64 last_path_fail_total_     = 0;   // sum of NoPath + FarFromPoly* path outcomes
    uint64 last_intents_exec_total_  = 0;
    uint64 last_intents_drop_total_  = 0;
    std::chrono::milliseconds last_vitals_sample_at_{0};   // total-clock of previous sample
    uint32 last_alert_wedged_ms_     = 0;   // GameTimeMS of last per-metric alert emit
    uint32 last_alert_p99_ms_        = 0;
    uint32 last_alert_drop_ms_       = 0;
    uint32 last_alert_pathfail_ms_   = 0;

    // #1B runtime wedge watchdog + its own slow cadence. Ticked from
    // OnWorldUpdate every kWedgeWatchdogIntervalMs (reads only cheap per-bot
    // fields + the eventually-consistent snapshot, so a 5-10s cadence is
    // plenty — a 90s wedge surfaces on the first tick after the threshold).
    Diagnostics::WedgeWatchdog wedge_watchdog_{};
    std::chrono::milliseconds  wedge_wd_total_{0};
    std::chrono::milliseconds  last_wedge_wd_at_{0};
    static constexpr std::chrono::milliseconds kWedgeWatchdogIntervalMs{7000};

    // Craft-order board maintenance cadence (#4B-2). Ages out stale Claimed
    // orders (timeout -> Fail + refund) and prunes finished rows. 60s is
    // plenty — the claim timeout is 30 min, so minute-granularity never traps
    // escrow for a meaningful window.
    std::chrono::milliseconds  craft_board_total_{0};
    std::chrono::milliseconds  last_craft_board_at_{0};
    static constexpr std::chrono::milliseconds kCraftBoardIntervalMs{60000};

    // Per-phase tick-cost EMA, surfaced in the FleetStatus log so the
    // operator can see which subsystem owns the world-thread cost at
    // scale. Reset to 0 on log emit. Updated each OnWorldUpdate call.
    // Tracks: snapshot Build (publish), BG port fire, intent drain,
    // session update, population shaper, scheduler tick, plus the
    // total tick latency.
    uint32 perf_ticks_in_window_   = 0;
    uint32 perf_snap_ms_total_     = 0;
    // Sub-buckets inside snap phase. snap_setup = DriveTeleportAck +
    // RescueOrphanedBgBot + SnapToGroundIfDrifted (per-bot world-thread
    // prep that runs even when the snapshot tier gate skips Build).
    // snap_build = BotSnapshotBuilder + publish path. group_build =
    // GroupSnapshotBuilder + publish_group. Sum ≈ perf_snap_ms_total.
    uint32 perf_snap_setup_ms_total_ = 0;
    uint32 perf_snap_build_ms_total_ = 0;
    uint32 perf_group_build_ms_total_ = 0;
    // How many bots actually rebuilt a snapshot in the window (vs. how
    // many were skipped by `should_build_snapshot` tier gate). Lets the
    // operator see "of 1000 bots, 200 rebuilt this window" to triage
    // whether the build cadence is too aggressive at scale.
    uint32 perf_snap_built_count_  = 0;
    uint32 perf_snap_skipped_count_ = 0;
    // How many intents actually executed inside DrainIntents over the
    // window (independent of bot count). Exposes "is drain saturating
    // the budget" vs "drain is cheap, snapshots dominate".
    uint32 perf_intents_drained_   = 0;
    uint32 perf_bgport_ms_total_   = 0;
    uint32 perf_drain_ms_total_    = 0;
    uint32 perf_session_ms_total_  = 0;
    uint32 perf_pop_ms_total_      = 0;
    uint32 perf_sched_ms_total_    = 0;
    uint32 perf_total_ms_total_    = 0;
    uint32 perf_total_ms_max_      = 0;   // worst-case in window

    // #5 Phase 4: fixed worker pool used to parallelize BotSnapshotBuilder::
    // Build per Map* inside the world-thread snapshot window. Lazily created +
    // started on first use when PlayerbotV2.ParallelSnapshotBuild is enabled,
    // so a box that never enables the feature pays nothing. Stopped in
    // Shutdown(). unique_ptr keeps the heavy threading header out of this one.
    std::unique_ptr<Playerbot::SnapshotBuildPool> snapshot_build_pool_;
    // Stable Map* -> worker-slot assignment so a bot's snapshots route to the
    // same thread_local recycle pool across ticks (ping-pong reuse invariant).
    // Cleared whenever the partition set is rebuilt; entries are sticky per
    // Map* for the life of that map. Map* is a stable pointer while the map is
    // loaded; a despawned map's stale key is harmless (just unused).
    std::unordered_map<void const*, uint32> map_worker_slot_;
    uint32 next_worker_slot_ = 0;
};

} // namespace Playerbot::V2

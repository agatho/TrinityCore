#include "BotAI.h"
#include "BotSnapshotView.h"
#include "BotIntentEmitter.h"
#include "BotActivityTier.h"
#include "Group/GroupSnapshot.h"
#include "States/StateBase.h"
#include "fmt/format.h"
#include "Log.h"  // TC_LOG_DEBUG for verbose-logging set_last_rule_fired
#include <cstring>  // std::strcmp for cross-TU-safe watchdog exempt match
#include <cmath>    // std::sqrt for death-blackspot deflection geometry

namespace Playerbot {

BotAI::BotAI(BotId id, BotPersonality personality, BotRng rng)
    : bot_id_(id),
      personality_(std::move(personality)),
      rng_(rng)
{}

void BotAI::set_last_rule_fired(char const* name)
{
    last_rule_fired_ = name;
    if (!name) return;

    // Watchdog accounting. Counts consecutive same-rule fires within a
    // 30 s window; if >= 60, suppress the rule for 60 s. Captures the
    // "rule fires forever because intent silently fails" wedge pattern
    // generically — see kRuleWatchdogFireCount comment in BotAI.h.
    // Excludes idle:wander (legitimate fast-firing fallback rule).
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        constexpr char const* kExempt[] = {
            "idle:wander",            // ambient fallback, fires every tick by design
            "idle:ambient_emote",     // throttled internally
            "combat:opener",          // pulses fast at engage
            // Intentional every-tick HOLD states (R6, 2026-06-03): the bot is
            // deliberately standing by — the core flies it (taxi) or carries it
            // (ship/zeppelin/elevator). These re-fire every tick BY DESIGN and
            // are not stuck loops; suppressing them is meaningless and made
            // idle:taxi_flight the single loudest line in Watchdog.log (568).
            "idle:taxi_flight",       // riding a taxi flight (DispatchIdle stand-down)
            "idle:on_transport_wait", // riding a ship/zeppelin/elevator
            // A12 (2026-06-07): two more deliberate every-tick HOLD states the R6
            // pass missed — the bot is standing AT a dock/FM node waiting for the
            // ship to dock / the flight master to enter scan range. Suppressing a
            // hold state for 60s is the wrong tool (it makes the bot abandon the
            // wait); the genuinely-unreachable-FM case is now handled by the A3
            // goal blacklist, not the watchdog.
            "idle:wait_for_transport",
            "idle:wait_for_flightmaster",
        };
        bool exempt = false;
        for (char const* e : kExempt)
            if (std::strcmp(e, name) == 0) { exempt = true; break; }  // content, not pointer (cross-TU literals don't share an address without /OPT:ICF)
        // Combat APL rule fires use spell names directly (e.g. "Frostbolt",
        // "Slam", "Tiger Palm") — those legitimately re-fire many times
        // per second during combat and shouldn't be watchdog-suppressed.
        // We can't easily enumerate every spell name, but they all share
        // the property of NOT starting with a known prefix. Watchdog only
        // applies to engineered idle:/dead:/group: rule names.
        if (!exempt)
        {
            const bool watchable =
                (name[0] == 'i' && name[1] == 'd' && name[2] == 'l' && name[3] == 'e' && name[4] == ':')
             || (name[0] == 'd' && name[1] == 'e' && name[2] == 'a' && name[3] == 'd' && name[4] == ':')
             || (name[0] == 'g' && name[1] == 'r' && name[2] == 'o' && name[3] == 'u' && name[4] == 'p' && name[5] == ':');
            if (!watchable) exempt = true;
        }
        if (!exempt)
        {
            if (rule_watchdog_name_ == name &&
                now_ms - rule_watchdog_window_start_ms_ < kRuleWatchdogWindowMs)
            {
                if (++rule_watchdog_count_ == kRuleWatchdogFireCount)
                {
                    rule_watchdog_suppress_until_[name] = now_ms + kRuleWatchdogSuppressMs;
                    // Dedicated 'playerbot.v2.watchdog' logger — operators
                    // route this to its own log file via a worldserver.conf
                    // Appender entry, so wedge alerts don't get buried in
                    // the main Playerbot.log noise (which can be 100+ MB).
                    // Conf snippet (add to worldserver.conf):
                    //   Logger.playerbot.v2.watchdog=4,Console Watchdog
                    //   Appender.Watchdog=2,4,1,Watchdog.log,w
                    TC_LOG_WARN("playerbot.v2.watchdog",
                        "bot={} rule={} fires>={} window_ms={} suppress_ms={}",
                        uint32(bot_id_), name, kRuleWatchdogFireCount,
                        kRuleWatchdogWindowMs, kRuleWatchdogSuppressMs);
                }
            }
            else
            {
                rule_watchdog_name_ = name;
                rule_watchdog_count_ = 1;
                rule_watchdog_window_start_ms_ = now_ms;
            }
        }
    }

    // Ring-buffer the last ~16 rules for /history. Skip duplicates of the
    // most recent entry (don't spam with the same rule firing every tick).
    const size_t prev = (rule_history_head_ + kRuleHistoryCap - 1) % kRuleHistoryCap;
    if (rule_history_[prev] == name) return;
    rule_history_[rule_history_head_] = name;
    rule_history_head_ = (rule_history_head_ + 1) % kRuleHistoryCap;
    if (rule_history_size_ < kRuleHistoryCap) ++rule_history_size_;
    if (verbose_logging_)
        TC_LOG_DEBUG("playerbot.v2", "bot {} rule: {}", bot_id_, name);
}

bool BotAI::whisper_rate_check(std::string const& target,
                                std::string const& text,
                                uint32 now_ms)
{
    if (text.empty()) return true;
    // 64-bit FNV-1a — fast, sufficient for de-dup. We don't need
    // collision-resistance against an adversary, only that two
    // identical replies hash the same.
    constexpr uint64_t FNV_PRIME = 1099511628211ull;
    uint64_t h = 14695981039346656037ull;
    for (char c : text) { h ^= static_cast<uint8_t>(c); h *= FNV_PRIME; }
    constexpr uint32 kDedupWindowMs = 500;
    if (last_whisper_target_ == target &&
        last_whisper_hash_   == h &&
        (now_ms - last_whisper_ms_) < kDedupWindowMs)
        return false;
    last_whisper_target_ = target;
    last_whisper_hash_   = h;
    last_whisper_ms_     = now_ms;
    return true;
}

void BotAI::transition_to(BotState s)
{
    if (s == state_) return;
    prev_state_ = state_;
    state_ = s;
    state_entered_ = Ms{0};   // Set externally by tick when it has 'now'
}

void BotAI::tick(BotSnapshotView snapshot,
                 GroupSnapshotView group,
                 BotIntentEmitter& emit)
{
    ++tick_count_;

    // --- Watchdog movement-progress credit ---
    // The per-rule watchdog (note_rule_fired) counts same-rule re-fires and
    // suppresses a rule after kRuleWatchdogFireCount, but has no position
    // signal — so a bot legitimately walking a LONG chunked route (e.g.
    // idle:pursue_quest_goal every tick across a 600y+ trek out of a starter
    // zone) trips it after ~15s and gets yanked off-course by the high-priority
    // idle:watchdog_escape. Credit real movement: if the bot has moved
    // meaningfully (net) since the anchor, it's PROGRESSING, not wedged — clear
    // the watchdog and re-anchor here, BEFORE the rules dispatch (so this tick's
    // WatchdogGate sees the cleared suppress). Oscillation-in-place (no net
    // movement from the anchor) never reaches this, so a genuinely stuck bot
    // still trips the watchdog and gets its escape. 15y > snapshot position
    // jitter, and a walking bot (~7y/s) re-anchors every ~2s — far inside the
    // 60-fires/15s suppress threshold, so pursue keeps owning the trek.
    {
        float wx, wy, wz; snapshot.position(wx, wy, wz);
        const uint32 wnow = snapshot.published_at_ms();
        const uint32 pbc  = snapshot.path_blocked_count();

        // (a) NET POSITION progress: bot moved >= 15y from the anchor.
        constexpr float kWatchdogProgressY = 15.0f;
        const float adx = wx - watchdog_anchor_x_;
        const float ady = wy - watchdog_anchor_y_;
        const bool moved = !watchdog_anchor_set_ ||
            (adx * adx + ady * ady) >= kWatchdogProgressY * kWatchdogProgressY;
        if (moved)
        {
            watchdog_anchor_x_   = wx;
            watchdog_anchor_y_   = wy;
            watchdog_anchor_set_ = true;
        }

        // (b) MOVE-SUCCESS: the bot's pathing isn't failing. The escape (a 50y
        // random teleport) is only appropriate for a genuinely NoPath-trapped
        // bot; for a bot whose moves SUCCEED but re-fires the same rule a lot
        // (long chunked trek, or two rules spline-thrashing every tick so the
        // MotionMaster spline is reset before the bot traverses it — net
        // displacement pinned near zero), the escape just disrupts it. Credit
        // move-success: only consider the bot path-wedged when the global
        // path_blocked_count grew by >= 3 within a ~10s window (the same mark
        // pursue/walk rules use via check_anchor_wedge). Re-baseline the window.
        if (wnow - watchdog_blocks_window_ms_ > 10000u)
        {
            watchdog_blocks_baseline_  = pbc;
            watchdog_blocks_window_ms_ = wnow;
        }
        const uint32 blocks_grew =
            (pbc > watchdog_blocks_baseline_) ? (pbc - watchdog_blocks_baseline_) : 0u;
        const bool pathing_failing = blocks_grew >= 3u;

        if (moved || !pathing_failing)
            clear_rule_watchdog();
    }

    // Transport boarding-Z latch. The elevator exit rule needs to
    // know what Z the bot was at when it boarded so the "did the
    // platform carry me to a different floor" decision works. Latch
    // on false→true transition; clear on true→false. Ships pass
    // through this latch too — harmless since the exit rule gates
    // on metadata kind, not raw Z.
    // Normalise newly-summoned pets to defensive. Aggressive mode (the
    // server default for hunter pets) makes the pet pull every mob in
    // its 25y leash radius — breaking CC, dragging adds onto the group,
    // chasing respawns out of LoS — all things that actively hinder
    // bot progress. Detect any pet-guid transition (Empty→guid or
    // guid→different-guid) and fire one PetSetReactStateIntent{1}.
    // The intent is GCD-free so it doesn't compete with combat casts.
    {
        const ObjectGuid cur_pet = snapshot.pet_guid();
        if (!cur_pet.IsEmpty() && cur_pet != last_known_pet_guid_)
        {
            emit.pet_set_react_state(1);   // REACT_DEFENSIVE
        }
        last_known_pet_guid_ = cur_pet;
    }

    if (snapshot.on_transport() && !was_on_transport_)
    {
        float bx, by, bz; snapshot.position(bx, by, bz);
        elevator_boarding_z_   = bz;
        transport_prev_z_      = bz;
        transport_z_stable_ms_ = 0;
        was_on_transport_      = true;
    }
    else if (!snapshot.on_transport() && was_on_transport_)
    {
        elevator_boarding_z_   = 0.f;
        transport_prev_z_      = 0.f;
        transport_z_stable_ms_ = 0;
        was_on_transport_      = false;
    }
    else if (snapshot.on_transport())
    {
        // Track Z stability while attached. The snapshot's
        // transport_stopped is always-true for type-11 elevators
        // (snapshot builder dynamic_casts to type-15 Transport class
        // and falls back to true when that fails), so we proxy
        // "platform at rest" with "world-Z hasn't moved this tick".
        // Reset stable_ms on any Z delta ≥ 0.3y; otherwise accumulate
        // the snapshot's tick interval. The elevator step_off rule
        // requires ≥500ms stable before firing, so it cannot step off
        // mid-ascent at the moment |bz - boarding_z| first crosses
        // 15y — it waits until the platform has settled at the next
        // stop frame.
        float bx, by, bz; snapshot.position(bx, by, bz);
        const float dz = std::fabs(bz - transport_prev_z_);
        if (dz < 0.3f)
        {
            // Snapshot publishes at ~5 Hz under load; clamp the
            // per-tick increment to 1000ms so a hitched snapshot
            // doesn't fake long stability.
            const uint32 inc = 200;
            transport_z_stable_ms_ =
                std::min<uint32>(transport_z_stable_ms_ + inc, 60000u);
        }
        else
        {
            transport_z_stable_ms_ = 0;
        }
        transport_prev_z_ = bz;
    }

    // Per-zone activity accumulator. Used by idle:rebind_hearth_activity
    // to detect zones the bot has "settled into" (cumulative >30min) and
    // rebind hearth there. Skip while in BG/dungeon (those are transient
    // and shouldn't accumulate hearth-relevant time). Delta is bounded
    // by the AI tick interval (typically 200-1000ms).
    if (snapshot.is_alive() && !snapshot.in_battleground() && !snapshot.is_in_dungeon())
    {
        const uint32 now_ms = snapshot.published_at_ms();
        if (last_activity_tick_ms_ != 0)
        {
            uint32 dt = now_ms - last_activity_tick_ms_;
            // Cap dt at 60s to absorb logout/login gaps that would
            // otherwise dump an hour of "activity" into the current
            // zone on the post-login first tick.
            if (dt > 60u * 1000u) dt = 60u * 1000u;
            note_zone_activity(snapshot.zone_id(), dt);
        }
        last_activity_tick_ms_ = now_ms;
    }
    else
    {
        last_activity_tick_ms_ = 0;  // pause accumulator
    }

    // Death-spiral memory clears once the gear is healthy again — a successful
    // repair (or fresh upgrades) is the natural end of a broken-gear spiral.
    // Cheap snapshot read; only matters while a spiral is actually armed.
    if (consecutive_same_spot_deaths() != 0 && snapshot.is_alive() &&
        snapshot.lowest_equipped_durability_pct() >= 90)
        reset_death_spiral();


    // 1. Reactive transitions driven by snapshot (cheap, no events).
    // Routed through transition_to() so previous_state() tracks correctly.
    const bool prev_alive = (state_ != BotState::Dead && state_ != BotState::Resurrecting);
    if (!snapshot.is_alive() && state_ != BotState::Dead && state_ != BotState::Resurrecting)
    {
        // Death transition. When dying inside an instance, count
        // toward this run's per-bot deaths tally for /diag visibility.
        if (snapshot.is_in_instance())
            note_dungeon_death();
        // Open-world death-spiral memory: ONLY out in the world (not in a
        // dungeon/raid instance, not in a BG). A normal dungeon/raid wipe is a
        // hard fight, not a broken-gear spiral, so it must never arm the
        // counter. Records the death position so repeated deaths in the same
        // spot escalate the State_Dead / State_InCombat escape.
        if (!snapshot.is_in_instance() && !snapshot.in_battleground())
        {
            note_open_world_death(snapshot.raw().position.x,
                                  snapshot.raw().position.y,
                                  snapshot.raw().position.z,
                                  snapshot.published_at_ms());
            // Owner idea (2026-06-22): after repeated deaths in the same spot, mark
            // it a travel blackspot so movement routes AROUND it next time instead
            // of walking back into the killing field. Quest stays valid.
            if (consecutive_same_spot_deaths() >= kBlackspotDeathThreshold)
                arm_death_blackspot(snapshot.raw().position.x,
                                    snapshot.raw().position.y,
                                    snapshot.published_at_ms());
        }
        // Fresh-death entry: clear the per-death recovery latches. These are
        // otherwise reset ONLY in DispatchDead's revival block (State_Dead.cpp),
        // which is gated on the snapshot observing is_alive==true. A bot that
        // reclaims its corpse / spirit-resurrects revives at 50% HP inside the
        // camp that killed it and frequently re-dies before the next snapshot —
        // and dead bots tick at Hibernate cadence (~2s), so the brief alive
        // frame is never observed. The latch corpse_recovery_emitted_ then stays
        // true across the undetected revive->re-death, DispatchDead skips its
        // release branch (guarded by !corpse_recovery_emitted()), is_ghost()
        // never flips, and the bot wedges in dead:waiting_release forever
        // (observed: Bramwell L4 Elwynn). The only reliable signal of a NEW
        // death is this Idle/Combat->Dead transition edge, so re-baseline the
        // recovery FSM here. (The revival-block reset stays as belt-and-braces.)
        set_corpse_recovery_emitted(false);
        set_release_pending_at_ms(0);
        set_ghost_since_ms(0);
        set_rez_acked(false);
        set_reincarnation_attempted(false);
        set_reincarnation_attempt_ms(0);
        set_corpse_run_last_dist(-1.0f);
        set_dead_watchdog_ms(0);   // fresh bounded-recovery window
        transition_to(BotState::Dead);
    }
    else if (snapshot.is_alive() && state_ == BotState::Dead)
        transition_to(BotState::Idle);     // post-revive demotion
    // LoggingIn is the bootstrap state. Once we have a live, alive snapshot
    // the bot is in-world and ready — promote to Idle so the regular
    // dispatch chain runs. Without this, bots that login outside combat
    // stay in LoggingIn (a stub dispatcher) forever.
    else if (snapshot.is_alive() && state_ == BotState::LoggingIn)
        transition_to(BotState::Idle);
    // Treat any active attacker as "in combat" — TC's combat-flag
    // propagation can lag by a tick when a mob aggro-transfers (e.g.
    // tank dies, mob picks a new threat target). Without this, the
    // bot's state stays Idle for that tick, the follow rule fires,
    // and the bot walks away from a mob that is actively hitting it.
    // Observed 2026-05-15: "tank dies, group walks away, mobs follow
    // and kill them".
    else if ((snapshot.in_combat() || !snapshot.raw().combat.attackers.empty()) &&
             state_ != BotState::InCombat &&
             state_ != BotState::Dead)
    {
        set_combat_entered_ms(snapshot.published_at_ms());
        transition_to(BotState::InCombat);
    }
    else if (state_ == BotState::InCombat &&
             !snapshot.in_combat() &&
             snapshot.raw().combat.attackers.empty())
    {
        // Combat just ended without dying — record a "kill" for the
        // tank-pull-pacing cooldown and increment the per-run dungeon
        // contribution counter when in an instance. Heuristic: surviving
        // a fight = group landed a kill. ONLY for fights that lasted
        // ≥1.5s: TC's combat flag flaps (aggro transfers, brief proximity
        // flags) produce InCombat→Idle transitions every few seconds, and
        // each one reset the post-kill pacing timer — a tank standing in
        // a flap-prone spot never saw the timer expire and tank_advance
        // starved (2026-06-11 Stockades stall).
        const uint32 fight_ms =
            snapshot.published_at_ms() - combat_entered_ms();
        if (combat_entered_ms() != 0 && fight_ms >= 1500)
        {
            note_kill(snapshot.published_at_ms());
            if (prev_alive && snapshot.is_in_instance())
                note_dungeon_kill();
        }
        transition_to(BotState::Idle);
    }

    // Map-change reset for per-run contribution counters. When the bot
    // crosses a map boundary (entering or leaving an instance), the
    // previous run's kill/death tally should reset so /diag reflects
    // the CURRENT run rather than lifetime totals. Last-seen map_id
    // primes on first observation so a fresh login doesn't reset
    // before any kills accrue.
    const uint32 cur_map = snapshot.map_id();
    if (last_seen_map_id_ != cur_map)
    {
        if (last_seen_map_id_ != 0)
        {
            reset_dungeon_contribution();
            // Waypoint index is per-run; reset on map change so the next
            // dungeon entry starts at progression_waypoints[0] rather
            // than continuing from wherever the previous run left off.
            set_dungeon_waypoint_index(0);
            // A map change means the bot left the spiralling area (or was
            // graveyard-ported away) — clear the open-world death-spiral memory
            // so deaths on the new map start a fresh count.
            reset_death_spiral();
        }
        // Drop stale movement on ANY real map change — including 0 -> N, since
        // map 0 is a valid map (the dungeontest staging area lives there), so
        // this clear is gated by its own primed flag rather than the != 0
        // contribution guard above. A cross-map teleport (LFG dungeon port, BG,
        // hearth, areatrigger) does NOT clear the server-side MotionMaster, so a
        // POINT/CHASE/FOLLOW generator computed on the OLD map survives and
        // re-splines toward its now-meaningless target, walking the bot
        // dead-straight into the void (observed: Dunghealer crawling toward the
        // map-0 staging XY (-8985,511) on map 36 for 13+ min). clear_generators
        // =true because StopMoving() alone leaves the generator to re-issue;
        // also drop the regroup convergence sample so a pre-port baseline can't
        // false-trigger a divergence re-issue on the new map.
        if (map_change_primed_)
        {
            reset_regroup_tracking();
            clear_dungeon_cross();   // a cross-target on the old map must not survive
            emit.stop_movement(/*clear_generators*/ true);
        }
        map_change_primed_ = true;
        last_seen_map_id_  = cur_map;
    }

    // 2. Primary state dispatch.
    dispatch_primary(snapshot, group, emit);

    // 3. Cross-cutting layers.
    dispatch_layers(snapshot, group, emit);
}

void BotAI::dispatch_primary(BotSnapshotView s, GroupSnapshotView g,
                              BotIntentEmitter& em)
{
    switch (state_)
    {
        case BotState::LoggingIn:    States::DispatchLoggingIn(*this, s, g, em);    break;
        case BotState::LoggingOut:   States::DispatchLoggingOut(*this, s, g, em);   break;
        case BotState::Idle:         States::DispatchIdle(*this, s, g, em);         break;
        case BotState::Travelling:   States::DispatchTravelling(*this, s, g, em);   break;
        case BotState::Questing:     States::DispatchQuesting(*this, s, g, em);     break;
        case BotState::InCombat:     States::DispatchInCombat(*this, s, g, em);     break;
        case BotState::Looting:      States::DispatchLooting(*this, s, g, em);      break;
        case BotState::Dead:         States::DispatchDead(*this, s, g, em);         break;
        case BotState::Resurrecting: States::DispatchResurrecting(*this, s, g, em); break;
        default:
            // Cross-cutting states are not primary; treat as Idle.
            States::DispatchIdle(*this, s, g, em);
            break;
    }
}

void BotAI::dispatch_layers(BotSnapshotView s, GroupSnapshotView g,
                             BotIntentEmitter& em)
{
    // Always-on social auto-responses. Run before in-group / state dispatch
    // so they fire regardless of whether the bot is grouped or what primary
    // state it's in. Cheap: each is a single snapshot-flag check + at most
    // one intent emit.
    //
    // Duel challenges: friendly initiators (group / guild / social-friend)
    // get auto-accepted so owner sparring works without a manual whisper;
    // strangers get auto-declined to avoid being trolled into PvP. The
    // friend flag is resolved on the world thread by the snapshot builder
    // (Player::GetSocial::HasFriend + group/guild membership), so the AI
    // worker just inspects a single bool here.
    if (s.has_duel_request())
    {
        // Context filter: never auto-accept duels inside an active
        // BG / arena / dungeon / raid. Server-side duel handlers usually
        // reject those anyway, but the bot's emit still costs a packet
        // round-trip + clutters the duel-state machine while the
        // instance is gating combat. Auto-decline regardless of friend
        // status; the owner can manually accept once the bot is out.
        const bool in_instance_or_pvp =
            s.in_battleground() || s.is_in_dungeon() || s.is_in_raid();
        if (in_instance_or_pvp)
            em.duel_decline();
        else if (s.duel_initiator_is_friend())
            em.duel_accept();
        else
            em.duel_decline();
    }
    // Trades opened on the bot are likewise declined automatically. Window
    // stays open server-side until either side cancels — without this the
    // bot would sit there with the dialog up indefinitely.
    if (s.has_trade_request())
        em.trade_decline();

    if (s.in_group())
        States::DispatchInGroup(*this, s, g, em);

    // Level-up announcement. Compares the snapshot's current level against
    // the last value we observed; on a strict increase, drops a /p chat ping
    // so the rest of the group sees the ding. Suppressed on the first tick
    // (last_seen_level_ == 0) to avoid announcing on every login. Only fires
    // when grouped — solo bots ding silently. Single emit per ding even if
    // the bot levels multiple times in the same tick (rare): the announce
    // text reports the new level only.
    {
        const uint8 cur = s.level();
        const uint8 prev = last_seen_level_;
        // Personality gate (parity with group-join greet below). Silent/
        // Terse bots level up silently; Normal+ announce. Without this,
        // bots configured as Silent still emit a /p Ding! on every level
        // — inconsistent with how the same bot stays quiet for greets.
        //
        // Ding stagger 2026-05-21: don't /say immediately. Capture the
        // level + an emit time 1.5–5.5s in the future, jittered by
        // bot_id, so a raid that levels on the same XP-grant doesn't
        // stack 25 identical /p Ding! lines inside one server tick.
        if (cur > 0 && prev > 0 && cur > prev && s.in_group()
            && personality_.verbosity >= Verbosity::Normal
            && pending_ding_level_ == 0)
        {
            const uint32 now_ms = s.published_at_ms();
            const uint32 jitter = 1500u + (uint32(s.bot_id()) * 2654435761u) % 4000u;
            pending_ding_level_     = cur;
            pending_ding_say_at_ms_ = now_ms + jitter;
        }
        if (cur != prev)
            last_seen_level_ = cur;

        // Fire the pending ding when its scheduled time arrives. Reset the
        // slot regardless of whether the announce went out (e.g. lost-group
        // edge case) so a future level still queues cleanly.
        if (pending_ding_level_ > 0)
        {
            const uint32 now_ms = s.published_at_ms();
            if (now_ms >= pending_ding_say_at_ms_)
            {
                if (s.in_group() && personality_.verbosity >= Verbosity::Normal)
                {
                    char const* prefix =
                        personality_.verbosity == Verbosity::Roleplay ? "Hark! Level " :
                        personality_.verbosity == Verbosity::Chatty   ? "Ding! "       :
                                                                        "Ding ";
                    em.say(fmt::format("{}{}", prefix, pending_ding_level_));
                }
                pending_ding_level_     = 0;
                pending_ding_say_at_ms_ = 0;
            }
        }
    }

    // Group-join greet. When the bot transitions Empty → non-Empty group
    // (joined a new party/raid), drop a single /p chat to signal presence.
    // The first observation only primes the field; without that priming step
    // every bot would say "Hi!" once per server boot the moment the snapshot
    // first sees its existing group. Re-greets on subsequent group changes
    // so a bot that switches party-to-raid or rejoins a different group
    // greets each fresh group once. Solo bots (group_guid == Empty) reset
    // the greet priming so a future re-join greets cleanly.
    //
    // Personality-aware: Silent / Terse bots stay quiet. Roleplay bots get a
    // flavour-text variant ("Hail and well met!"). The intent is to keep the
    // social layer feeling individuated rather than uniform "Hi!" spam from
    // every bot in a fresh raid.
    {
        const ObjectGuid cur_grp = s.group_guid();
        if (!group_greet_primed_)
        {
            // First ever observation — capture and skip the greet.
            last_seen_group_ = cur_grp;
            group_greet_primed_ = true;
        }
        else if (cur_grp != last_seen_group_)
        {
            if (!cur_grp.IsEmpty() && personality_.verbosity >= Verbosity::Normal)
            {
                char const* line = "Hi!";
                switch (personality_.verbosity)
                {
                    case Verbosity::Roleplay: line = "Hail and well met!"; break;
                    case Verbosity::Chatty:   line = "Hey everyone!";       break;
                    case Verbosity::Normal:   line = "Hi!";                 break;
                    default:                  line = nullptr;               break; // Silent/Terse
                }
                if (line) em.say(line);
            }
            last_seen_group_ = cur_grp;
        }
    }

    // Future: detect AtVendor / AtMailbox / AtAh / InInstance / Decorating
    // by inspecting position + nearby NPCs / objects + active interaction.
}

Role BotAI::effective_role(BotSnapshotView const& s) const
{
    return role_override_ != Role::Unknown ? role_override_ : s.my_role();
}

// --- Death blackspots (owner idea, 2026-06-22) ---------------------------------
void BotAI::arm_death_blackspot(float x, float y, uint32 now_ms)
{
    // Refresh an overlapping active spot; else reuse the oldest (most-expired) slot.
    int oldest = 0;
    uint32 oldest_exp = 0xFFFFFFFFu;
    for (int i = 0; i < 4; ++i)
    {
        DeathBlackspot& b = death_blackspots_[i];
        const float dx = x - b.x, dy = y - b.y;
        if (b.expiry_ms > now_ms && (dx * dx + dy * dy) <= b.r * b.r)
        {
            b.expiry_ms = now_ms + kBlackspotTtlMs;   // still dying here — extend
            return;
        }
        if (b.expiry_ms < oldest_exp) { oldest_exp = b.expiry_ms; oldest = i; }
    }
    death_blackspots_[oldest] = DeathBlackspot{ x, y, kBlackspotRadius, now_ms + kBlackspotTtlMs };
}

bool BotAI::in_death_blackspot(float x, float y, uint32 now_ms) const
{
    for (int i = 0; i < 4; ++i)
    {
        DeathBlackspot const& b = death_blackspots_[i];
        if (b.expiry_ms <= now_ms || b.r <= 0.f) continue;
        const float dx = x - b.x, dy = y - b.y;
        if (dx * dx + dy * dy <= b.r * b.r) return true;
    }
    return false;
}

bool BotAI::deflect_for_blackspot(float fx, float fy, float tx, float ty,
                                  uint32 now_ms, float& ox, float& oy) const
{
    for (int i = 0; i < 4; ++i)
    {
        DeathBlackspot const& b = death_blackspots_[i];
        if (b.expiry_ms <= now_ms || b.r <= 0.f) continue;
        const float dgx = tx - fx, dgy = ty - fy;
        const float glen = std::sqrt(dgx * dgx + dgy * dgy);
        if (glen < 1.0f) continue;
        const float ux = dgx / glen, uy = dgy / glen;
        const float vcx = b.x - fx, vcy = b.y - fy;
        const float dC = std::sqrt(vcx * vcx + vcy * vcy);
        // Standing INSIDE the blackspot: head straight out, away from its center.
        if (dC <= b.r)
        {
            const float awx = (dC > 0.01f) ? -vcx / dC : -ux;
            const float awy = (dC > 0.01f) ? -vcy / dC : -uy;
            ox = fx + awx * (b.r + 15.0f);
            oy = fy + awy * (b.r + 15.0f);
            return true;
        }
        const float proj = vcx * ux + vcy * uy;     // distance along the path to C
        if (proj <= 0.0f) continue;                  // blackspot is behind us
        const float perpx = vcx - proj * ux, perpy = vcy - proj * uy;
        const float perp  = std::sqrt(perpx * perpx + perpy * perpy);
        const float clear = b.r + 12.0f;
        if (perp >= clear) continue;                 // straight path already clears it
        // Deflect: offset the projected point perpendicular, AWAY from the center,
        // so the bot skirts the blackspot's edge while still advancing to the goal.
        float awx, awy;
        if (perp > 0.01f) { awx = -perpx / perp; awy = -perpy / perp; }
        else              { awx = -uy;           awy = ux; }   // center on the line: pick a side
        ox = fx + ux * proj + awx * clear;
        oy = fy + uy * proj + awy * clear;
        return true;
    }
    return false;
}

} // namespace Playerbot

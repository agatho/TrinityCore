// BotAI - The top-level state machine. CONTRACTS.md §2.6.
//
// Constructed once per bot. Lives on the AI worker thread that runs the bot.
// Pure: tick(snapshot, group, events, emit) — no global mutation, no I/O.

#pragma once

#include "BotTypes.h"
#include "BotSnapshot.h"  // CurrencyEntry — cached_currencies_ member needs full type
#include "Battleground/BattlegroundScript.h"  // BattlegroundAdvice — bg_advice_cache_ member needs full type
#include "Formation.h"
#include "BotPersonality.h"
#include "BotArchetype.h"
#include "BotRng.h"
#include "ObjectGuid.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot {

class BotSnapshotView;
class GroupSnapshotView;
class BotIntentEmitter;
struct Intent;

class BotAI
{
public:
    BotAI(BotId id, BotPersonality personality, BotRng rng);

    // Called by AI worker. Pure: snapshot in → intents out.
    // BotEventInbox parameter was removed 2026-05-21 — see PlayerbotV2.cpp
    // OnDamageTaken comment for rationale.
    void tick(BotSnapshotView snapshot,
              GroupSnapshotView group,
              BotIntentEmitter& emit);

    // State accessors
    BotState state() const { return state_; }
    BotState previous_state() const { return prev_state_; }
    void     transition_to(BotState s);

    // Inspection (used by .playerbot inspect)
    BotId               bot_id()      const { return bot_id_; }
    BotPersonality const& personality() const { return personality_; }
    // Per-bot play archetype (WHAT/WHEN). Set on login from the persisted
    // archetype_id (or rolled + written back on first spawn). Read by the
    // population rebalancer (role_affinity), idle rules (activity emphasis),
    // and surfaced in the snapshot's ArchetypeState for thread-free gating.
    BotArchetype const& archetype() const { return archetype_; }
    void                set_archetype(BotArchetype const& a) { archetype_ = a; }
    BotRng const&       rng()         const { return rng_; }
    BotRng&             rng()               { return rng_; }
    Ms                  state_age(Ms now) const { return now - state_entered_; }

    // Diagnostic: name of the most recently fired APL rule (string is owned by
    // the rotation's static rule table). nullptr if no rotation ticked yet.
    char const* last_rule_fired() const { return last_rule_fired_; }
    void        set_last_rule_fired(char const* name);

    // BG outcome dedup — the post-match leave rule fires multiple times
    // (bg_status=4 persists ~2min until TC kicks); we only want to record
    // the win/loss tally ONCE per match. Keyed by the snapshot published_at
    // timestamp of the first observation so subsequent re-fires no-op.
    bool recorded_bg_outcome_for(uint32 snap_ms) const
    { return bg_outcome_recorded_ms_ != 0 &&
             bg_outcome_recorded_ms_ + 5u*60u*1000u > snap_ms; }
    void note_bg_outcome_recorded(uint32 snap_ms)
    { bg_outcome_recorded_ms_ = snap_ms; }

    // Generic rule-fire watchdog. State_Idle has many rules whose underlying
    // intent can fail silently (vendor refuses, server rejects, target
    // immune); without per-rule dedup the rule re-fires every snapshot
    // tick, dispatch never reaches movement / quest rules below it, and
    // the bot is permanently wedged. We've added per-rule cooldowns one
    // wedge at a time (equip / vendor-buy / loot / chest / gather /
    // sell-trash / bank / pet-summon / hub / start-attack / move_to / cast).
    // The watchdog catches the next wedge automatically without writing a
    // dedicated cooldown:
    //
    //   When the same rule name fires >= kRuleWatchdogFireCount times within
    //   kRuleWatchdogWindowMs, auto-suppress that rule for kRuleWatchdogSuppressMs.
    //
    // Tuned conservatively so legitimate fast-firing rules (idle:wander
    // re-emits every snapshot; combat APL rules pulse) never trip:
    //   60 fires in 30 s = 2/sec average. Most healthy rules cap at 1/sec
    //   per tick. Wander rule is excluded explicitly because it has its
    //   own anchor logic — wedges through wander look different.
    //
    // The watchdog ALSO prints a TC_LOG_WARN the first time it suppresses
    // a rule for a bot, so the next wedge surfaces in Server.log without
    // manual /diag inspection.
    static constexpr uint32 kRuleWatchdogFireCount   = 60u;
    static constexpr uint32 kRuleWatchdogWindowMs    = 30u * 1000u;
    static constexpr uint32 kRuleWatchdogSuppressMs  = 60u * 1000u;
    bool rule_watchdog_suppressed(char const* name, uint32 now_ms) const
    {
        if (!name) return false;
        auto const it = rule_watchdog_suppress_until_.find(name);
        if (it == rule_watchdog_suppress_until_.end()) return false;
        return now_ms < it->second;
    }
    void prune_rule_watchdog(uint32 now_ms)
    {
        for (auto it = rule_watchdog_suppress_until_.begin(); it != rule_watchdog_suppress_until_.end();)
        {
            if (now_ms >= it->second) it = rule_watchdog_suppress_until_.erase(it);
            else                       ++it;
        }
    }
    // Movement-progress credit for the per-rule watchdog. The watchdog counts
    // same-rule re-fires and suppresses after kRuleWatchdogFireCount (60 in 30s
    // = ~15s of walking) — but it has NO position signal, so a bot legitimately
    // walking a LONG chunked route (idle:pursue_quest_goal every tick across a
    // 600y+ trek) trips it and gets yanked off-course by idle:watchdog_escape
    // (a 50y random escape that can land on FarFromPolyEnd). tick() calls this
    // when the bot has moved meaningfully since the anchor: a MOVING bot is
    // progressing, not wedged, so clear the spin counter AND the suppress map.
    // Oscillation-in-place (no net movement) never calls this, so a genuinely
    // stuck bot still trips the watchdog as before.
    void clear_rule_watchdog()
    {
        rule_watchdog_name_            = nullptr;
        rule_watchdog_count_           = 0;
        rule_watchdog_window_start_ms_ = 0;
        rule_watchdog_suppress_until_.clear();
    }
    // Walk the rule-history ring buffer in fire order, oldest first. Empty
    // entries are skipped. Visitor takes a (size_t index, char const* name).
    template <class F>
    void for_each_rule_history(F&& visitor) const
    {
        const size_t start = (rule_history_size_ < kRuleHistoryCap)
                           ? 0
                           : rule_history_head_;
        for (size_t i = 0; i < rule_history_size_; ++i)
        {
            const size_t idx = (start + i) % kRuleHistoryCap;
            if (char const* n = rule_history_[idx]) visitor(i, n);
        }
    }

    // Per-tick edge-trigger guards to prevent re-emitting one-shot intents
    // every tick while a server condition lingers (ready-check window,
    // mount cast in flight). State_InGroup uses these to dedup.
    bool ready_check_acked() const { return ready_check_acked_; }
    void set_ready_check_acked(bool v) { ready_check_acked_ = v; }
    bool mount_pending() const { return mount_pending_; }
    void set_mount_pending(bool v) { mount_pending_ = v; }
    bool invite_acked() const { return invite_acked_; }
    void set_invite_acked(bool v) { invite_acked_ = v; }
    bool guild_invite_acked() const { return guild_invite_acked_; }
    void set_guild_invite_acked(bool v) { guild_invite_acked_ = v; }
    // Per-proposal-id ack — keyed so a re-issued proposal (different id)
    // re-fires acceptance without waiting for the old gate to clear.
    uint32 lfg_proposal_acked_id() const { return lfg_proposal_acked_id_; }
    void   set_lfg_proposal_acked_id(uint32 id) { lfg_proposal_acked_id_ = id; }
    bool rez_acked() const { return rez_acked_; }
    void set_rez_acked(bool v) { rez_acked_ = v; }
    bool corpse_recovery_emitted() const { return corpse_recovery_emitted_; }
    void set_corpse_recovery_emitted(bool v) { corpse_recovery_emitted_ = v; }
    // Tracks when the bot first observed itself as a ghost in the current
    // death cycle. Used by the State_Dead 5-min stuck-ghost timeout: if
    // the corpse-run can't reach the corpse (terrain blocked, despawn
    // grid unreachable, etc), force a SpiritResurrect so the bot doesn't
    // ghost-wander indefinitely leaving an unclaimed corpse on the map.
    // Reset to 0 on every revival in DispatchDead's is_alive branch.
    uint32 ghost_since_ms() const { return ghost_since_ms_; }
    void set_ghost_since_ms(uint32 ms) { ghost_since_ms_ = ms; }

    // DMM-P1b: dedicated Shaman Reincarnation self-rez guard, kept separate
    // from corpse_recovery_emitted_ (which dedups the release/spirit-healer
    // choice). reincarnation_attempted_ records that we fired the self-rez
    // cast this death; reincarnation_attempt_ms_ stamps when, so State_Dead
    // can fall through to the normal release/corpse-run path if the bot is
    // still dead ~2.5s later (failed cast: no Ankh / wrong state / fizzle)
    // instead of wedging in Dead forever. Both reset to 0/false on revival.
    // Backing fields live in the isolated end-of-class block (see below).
    bool   reincarnation_attempted() const { return reincarnation_attempted_; }
    void   set_reincarnation_attempted(bool v) { reincarnation_attempted_ = v; }
    uint32 reincarnation_attempt_ms() const { return reincarnation_attempt_ms_; }
    void   set_reincarnation_attempt_ms(uint32 ms) { reincarnation_attempt_ms_ = ms; }

    // DMM-P3a: last observed ghost-to-corpse distance for the corpse-run
    // no-progress detector. The stuck-ghost timeout (ghost_since_ms_) is
    // repurposed as a no-progress timer: it's reset whenever the bot makes
    // meaningful progress toward its corpse, so a normally-advancing corpse
    // run is never force-SpiritResurrected. -1 = no baseline yet (reset on
    // revival). Backing field in the isolated end-of-class block.
    float  corpse_run_last_dist() const { return corpse_run_last_dist_; }
    void   set_corpse_run_last_dist(float d) { corpse_run_last_dist_ = d; }

    // Stalled-chase detection: when a melee bot keeps trying to gap-close on
    // a victim it can't reach (cliff, terrain, fleeing target faster than us),
    // we eventually give up to free the bot from an infinite chase loop.
    ObjectGuid stuck_chase_victim() const { return stuck_chase_victim_; }
    uint16     stuck_chase_ticks() const { return stuck_chase_ticks_; }
    void       set_stuck_chase(ObjectGuid v, uint16 ticks)
                 { stuck_chase_victim_ = v; stuck_chase_ticks_ = ticks; }

    // Last flight master GUID we sent a discover_taxi_node intent to.
    // The auto-discover-taxi rule in State_Idle dedups against this so it
    // doesn't emit a discover-intent every tick the bot lingers near a
    // FM. Cleared when the bot moves out of interact range (the rule sets
    // it to Empty when the FM is no longer the nearest).
    ObjectGuid last_taxi_discover_fm() const { return last_taxi_discover_fm_; }
    void       set_last_taxi_discover_fm(ObjectGuid g) { last_taxi_discover_fm_ = g; }

    // Last innkeeper GUID we sent a bind_homebind intent to. Dedup so the bot
    // doesn't re-bind every tick (10c per bind, server-side idempotent for
    // identical location but the gold cost is paid every cast). Cleared when
    // the bot moves out of interact range so re-approach later still binds.
    ObjectGuid last_homebind_innkeeper() const { return last_homebind_innkeeper_; }
    void       set_last_homebind_innkeeper(ObjectGuid g) { last_homebind_innkeeper_ = g; }

    // Manual cross-map travel goal (owner `/goto <map> <x> <y>` whisper).
    // The snapshot builder synthesizes current_objective_poi from it (same
    // path as the R7 hub relocation) so the FULL travel pipeline — walk /
    // taxi / portal / ship / areatrigger — drives the bot there organically
    // (never teleport). Cleared by the builder on arrival.
    struct ManualTravelGoal { uint32 map_id = 0; float x = 0.f, y = 0.f, z = 0.f; bool set = false; };
    // CROSS-THREAD: the goal is WRITTEN by the owner's /goto on the world-
    // thread chat dispatch (BotCommandParser::Dispatch) and READ + CLEARED by
    // the snapshot builder on the parallel build pool — two threads touching
    // the SAME bot concurrently. The 5-field struct can't be a lone atomic, so
    // manual_travel_mtx_ guards it. The reader returns a stable COPY under the
    // lock (a caller's `auto const&` binds to the returned prvalue, extending
    // its lifetime), so Build can never observe a half-written goal mid-assign.
    // A benign logical lost-update remains: if the owner re-issues /goto in the
    // same instant the bot clears on arrival, the new goal may be wiped —
    // harmless at human /goto cadence; the owner just re-issues.
    ManualTravelGoal manual_travel() const
    { std::lock_guard<std::mutex> lk(manual_travel_mtx_); return manual_travel_; }
    void set_manual_travel(uint32 map, float x, float y, float z)
    { std::lock_guard<std::mutex> lk(manual_travel_mtx_); manual_travel_ = { map, x, y, z, true }; }
    void clear_manual_travel()
    { std::lock_guard<std::mutex> lk(manual_travel_mtx_); manual_travel_ = {}; }

    // POI progress stall detector (same-map travel). Returns true when the
    // 2D distance to the goal has not shrunk by >10y across a 60s window.
    // Complements path_blocked_count for the bridge-route trigger: a bot
    // circling a multi-level interior (UC inner ring) produces SUCCESSFUL
    // partial paths every tick — the blocked counter never accumulates,
    // so the elevator/bridge composition never engaged and the bot paced
    // forever. Keyed by goal so a goal change resets the window.
    bool poi_progress_stalled(uint64 goal_key, float dist_now, uint32 now_ms)
    {
        if (goal_key != poi_prog_key_)
        {
            poi_prog_key_ = goal_key;
            poi_prog_dist_ = dist_now;
            poi_prog_ms_ = now_ms;
            return false;
        }
        if (dist_now + 10.0f < poi_prog_dist_)
        {
            poi_prog_dist_ = dist_now;   // real progress — slide the window
            poi_prog_ms_ = now_ms;
            return false;
        }
        return (now_ms - poi_prog_ms_) >= 60u * 1000u;
    }

    // Per-zone cumulative activity tracker. Each tick the bot is alive
    // in zone X (not in BG/dungeon), the time delta since the last
    // observation is added to that zone's bucket. When a zone's bucket
    // crosses kHearthRebindThresholdMs, the idle:rebind_hearth_activity
    // rule schedules a rebind to the nearest innkeeper in that zone.
    // We don't reset existing buckets on rebind — the threshold is
    // additive across the bot's life so a long-lived bot rebinds
    // through its progression naturally. Persistent across snapshots.
    // Bounded to ~10 entries per bot (zone count of active play); old
    // entries evicted by LRU when capacity is reached.
    static constexpr uint32 kHearthRebindThresholdMs = 10u * 60u * 1000u; // 10 min
    static constexpr size_t kMaxActivityZones        = 16;
    void note_zone_activity(uint32 zone_id, uint32 dt_ms)
    {
        if (zone_id == 0 || dt_ms == 0) return;
        for (auto& e : activity_by_zone_)
            if (e.zone_id == zone_id) { e.ms_in_zone += dt_ms; return; }
        if (activity_by_zone_.size() >= kMaxActivityZones)
        {
            auto it = std::min_element(activity_by_zone_.begin(), activity_by_zone_.end(),
                [](auto const& a, auto const& b){ return a.ms_in_zone < b.ms_in_zone; });
            *it = ZoneActivity{zone_id, dt_ms};
            return;
        }
        activity_by_zone_.push_back(ZoneActivity{zone_id, dt_ms});
    }
    uint32 ms_in_zone(uint32 zone_id) const
    {
        for (auto const& e : activity_by_zone_)
            if (e.zone_id == zone_id) return e.ms_in_zone;
        return 0;
    }
    // Zone we currently consider "home" for hearth purposes. Reset to
    // 0 means the bot still hearts to its setup-pipeline default (race
    // capital). On a successful rebind set this to the new zone to
    // prevent the rule firing again until activity accrues in a NEW
    // zone past the threshold.
    uint32 hearth_zone() const { return hearth_zone_; }
    void   set_hearth_zone(uint32 z) { hearth_zone_ = z; }

    // Once-per-session calendar RSVP gate. Bots auto-accept all pending
    // calendar invites a single time per BotAI lifetime so raid sign-ups
    // populate without spamming the calendar manager. Reset is the natural
    // logout/login cycle; per-session is fine since calendar invites
    // accumulate in mail-like persistence and won't be re-sent.
    // Last time the bot fired a calendar-RSVP-all sweep. Originally a
    // one-shot bool but that made bots ignore later raid invites; now a
    // periodic gate so invites accumulated mid-session still get accepted.
    uint32 last_calendar_rsvp_ms() const { return last_calendar_rsvp_ms_; }
    void   set_last_calendar_rsvp_ms(uint32 v) { last_calendar_rsvp_ms_ = v; }

    // Last snapshot.level() we observed. Used by the level-up announcement
    // layer to detect a fresh ding and send a single party-chat congrats.
    // Initialized to 0 so the first observation primes the field instead
    // of triggering an announcement (a fresh login would otherwise spam
    // the group with "Ding 47!" every reload).
    uint8 last_seen_level() const { return last_seen_level_; }
    void  set_last_seen_level(uint8 v) { last_seen_level_ = v; }

    // Loot-roll deferral slot — see field comment for rationale.
    uint32 next_loot_roll_fire_ms() const { return next_loot_roll_fire_ms_; }
    void   set_next_loot_roll_fire_ms(uint32 v) { next_loot_roll_fire_ms_ = v; }

    // PvE death-release delay slot — see field comment for rationale.
    uint32 release_pending_at_ms() const { return release_pending_at_ms_; }
    void   set_release_pending_at_ms(uint32 v) { release_pending_at_ms_ = v; }

    // Bounded-recovery watchdog stamp (GameTime ms of first dead-tick).
    // Independent of the snapshot pipeline so a stalled death recovery always
    // terminates instead of deadlocking a dungeon group. See State_Dead.cpp.
    uint32 dead_watchdog_ms() const { return dead_watchdog_ms_; }
    void   set_dead_watchdog_ms(uint32 v) { dead_watchdog_ms_ = v; }

    // Social-popup hesitation slots.
    uint32 pending_group_invite_accept_at_ms() const { return pending_group_invite_accept_at_ms_; }
    void   set_pending_group_invite_accept_at_ms(uint32 v) { pending_group_invite_accept_at_ms_ = v; }
    uint32 pending_lfg_proposal_accept_at_ms() const { return pending_lfg_proposal_accept_at_ms_; }
    void   set_pending_lfg_proposal_accept_at_ms(uint32 v) { pending_lfg_proposal_accept_at_ms_ = v; }

    // Quest dialog hesitation slots.
    uint32 pending_quest_accept_at_ms() const { return pending_quest_accept_at_ms_; }
    void   set_pending_quest_accept_at_ms(uint32 v) { pending_quest_accept_at_ms_ = v; }
    uint32 pending_quest_turnin_at_ms() const { return pending_quest_turnin_at_ms_; }
    void   set_pending_quest_turnin_at_ms(uint32 v) { pending_quest_turnin_at_ms_ = v; }

    // Vendor-open hesitation slot.
    uint32 pending_vendor_visit_at_ms() const { return pending_vendor_visit_at_ms_; }
    void   set_pending_vendor_visit_at_ms(uint32 v) { pending_vendor_visit_at_ms_ = v; }

    // Loot-drain pause slot.
    uint32 pending_loot_drain_at_ms() const { return pending_loot_drain_at_ms_; }
    void   set_pending_loot_drain_at_ms(uint32 v) { pending_loot_drain_at_ms_ = v; }

    // Follow-leader mount-up hesitation slot.
    uint32 pending_follow_mount_at_ms() const { return pending_follow_mount_at_ms_; }
    void   set_pending_follow_mount_at_ms(uint32 v) { pending_follow_mount_at_ms_ = v; }

    // Equip-upgrade hesitation slot.
    uint32 pending_equip_upgrade_at_ms() const { return pending_equip_upgrade_at_ms_; }
    void   set_pending_equip_upgrade_at_ms(uint32 v) { pending_equip_upgrade_at_ms_ = v; }

    // Mailbox-open hesitation slot.
    uint32 pending_mail_open_at_ms() const { return pending_mail_open_at_ms_; }
    void   set_pending_mail_open_at_ms(uint32 v) { pending_mail_open_at_ms_ = v; }

    // Last snapshot.group_guid() we observed. Empty → fresh bot or solo;
    // non-Empty → bot is currently in some group. Used by the group-join
    // greet layer in dispatch_layers: a transition from Empty → non-Empty
    // fires a single /p chat "Hi!" so the bot feels social. Re-binding
    // (group changes) re-greets in the new group. Initialized via the
    // first snapshot tick to suppress the join-greet at server start (when
    // every bot would otherwise greet on first observation).
    ObjectGuid last_seen_group() const { return last_seen_group_; }
    void       set_last_seen_group(ObjectGuid g) { last_seen_group_ = g; }
    bool       group_greet_primed() const { return group_greet_primed_; }
    void       set_group_greet_primed(bool v) { group_greet_primed_ = v; }

    // Chat-driven pause: when a non-bot group member says "wait"/"hold"/
    // "stop" in party chat, BotChatReactor sets this to GameTime ms +
    // 10s so the tank-pull rule (and any other "voluntary engage" rules)
    // hold off until the timestamp elapses. Real players use these cues
    // constantly; before this, bots ignored them and kept pulling. The
    // chat reactor still emits an "ok" / "ok holding" reply alongside
    // the pause so the speaker gets visible feedback. 0 = no pause.
    uint32 chat_pause_until_ms() const { return chat_pause_until_ms_; }
    void   set_chat_pause_until_ms(uint32 v) { chat_pause_until_ms_ = v; }

    // Phase-change edge tracker: latched true when the bot last saw
    // `any_boss_in_special`. The phase-callout idle rule fires once on
    // the false→true edge ("watch out, phase change") then sets this
    // so the same SPECIAL window doesn't re-shout every tick. Clears
    // when SPECIAL ends so the NEXT phase transition re-fires.
    bool saw_special_phase() const { return saw_special_phase_; }
    void set_saw_special_phase(bool v) { saw_special_phase_ = v; }

    // Owner-set follow distance preference. The /follow whisper picks this
    // up so owners can customize how tightly the bot tails. 0.f means
    // "use default" (5yd). Owners with melee-heavy comp prefer 4-5yd, with
    // a caster-heavy comp prefer 8-10yd to spread out for AoE / flame
    // patches. Initialized to 0; owner sets via /follow_distance.
    float follow_distance() const { return follow_distance_; }
    void  set_follow_distance(float d) { follow_distance_ = d; }

    // Owner-driven role override. Snapshot's `my_role` is derived from spec
    // (snapshot builder maps spec-id → Role). The owner can pin a different
    // role via `/setrole tank|healer|dps` — useful for off-spec play where
    // the bot's spec says one thing but the group needs another. Role::Unknown
    // means "no override; honour the snapshot". Read by group/healer logic
    // via `effective_role(snapshot)` to get the final role to act on.
    Role role_override() const { return role_override_; }
    void set_role_override(Role r) { role_override_ = r; }
    Role effective_role(BotSnapshotView const& s) const;

    // Auto-train dedup. last_trained_trainer = NPC GUID we already
    // bulk-trained at the current level. last_train_level = the level we
    // trained at (resets dedup on ding). Both 0/Empty means no trainer
    // visited yet at the current level.
    ObjectGuid last_trained_trainer() const { return last_trained_trainer_; }
    void       set_last_trained_trainer(ObjectGuid g) { last_trained_trainer_ = g; }
    uint8      last_train_level() const { return last_train_level_; }
    void       set_last_train_level(uint8 l) { last_train_level_ = l; }

    // Owner-pinned AOE preference. When true, spec rotations should bias
    // toward AoE abilities (Multi-Shot, Whirlwind, Consecration) even on
    // single targets that COULD be hit single-target. Default false.
    // Toggled via /aoe whisper.
    bool       aoe_preference() const { return aoe_preference_; }
    void       set_aoe_preference(bool b) { aoe_preference_ = b; }

    // Battle-rez dedup state. brez_target_ is the GUID of the dead member
    // we last fired Rebirth (Druid) / Raise Ally (DK) / Soulstone (Warlock)
    // at; brez_acked_ flips true once the cast lands. Re-emitting the cast
    // every combat tick would interrupt our own ~1.5s rebirth cast and waste
    // the global brez budget. Cleared once the target rezzes (drops out of
    // dead_member()) or leaves the group, allowing a fresh rez attempt on the
    // next death.
    ObjectGuid brez_target() const { return brez_target_; }
    bool       brez_acked() const { return brez_acked_; }
    void       set_brez(ObjectGuid g, bool acked) { brez_target_ = g; brez_acked_ = acked; }

    // Owner-set focus target. Independent of victim/current_target (which
    // mirror live Unit::GetTarget). Used by spec rotations that distinguish
    // a "primary kill target" from a "thing I'm controlling" — Misdirection
    // to focus, Polymorph focus, Spellsteal focus. Empty = no focus pinned.
    // Mutable by /focus whisper. Cleared by /focus clear.
    ObjectGuid focus_target() const { return focus_target_; }
    void set_focus_target(ObjectGuid g) { focus_target_ = g; }

    // ------------------------------------------------------------------
    // Owner squad control — manual / override mode.
    //
    // When the owner issues a command (follow / hold / stay / engage /
    // ...), the bot enters "manual mode" for a sliding window. While in
    // manual mode the autonomous-rule cascade (questing, vendoring,
    // wandering) is preempted by `idle:owner_*` rules at the top of
    // DispatchIdle, so the bot does what the owner just asked instead
    // of resuming a quest path mid-conversation.
    //
    // Modes:
    //   None      — autonomous (default, no override).
    //   Follow    — track owner_target with formation offset.
    //   Hold      — stand at current position; allow self-buffs / heals.
    //   Stay      — stand state; no autonomous combat engage.
    //   Engage    — attack owner_target until it dies (then auto-clear).
    //   Disengage — break combat, retreat to formation slot.
    //   Action    — single-tick custom action; clears after emit.
    //
    // Cleared by /resume, by combat-end for Engage, or by manual_mode_
    // until_ms timeout (default 30 s — long enough for follow/hold to
    // persist across short pauses, short enough that a forgotten /stay
    // doesn't strand the bot for hours).
    enum class OwnerCommand : uint8
    {
        None = 0, Follow, Hold, Stay, Engage, Disengage, Action
    };

    OwnerCommand owner_command() const { return owner_command_; }
    ObjectGuid   owner_target()  const { return owner_target_; }
    uint32       manual_mode_until_ms() const { return manual_mode_until_ms_; }
    bool         is_manual_mode(uint32 now_ms) const
    {
        return owner_command_ != OwnerCommand::None &&
               now_ms < manual_mode_until_ms_;
    }

    // Enter manual mode with the given command + target. Pass `ttl_ms`
    // = 0 to use the default (30s). Re-entering refreshes the timer
    // and replaces the command (e.g. follow → hold).
    void enter_manual(OwnerCommand cmd, ObjectGuid target,
                      uint32 now_ms, uint32 ttl_ms = 30000)
    {
        owner_command_         = cmd;
        owner_target_          = target;
        manual_mode_until_ms_  = now_ms + (ttl_ms == 0 ? 30000u : ttl_ms);
        last_owner_command_ms_ = now_ms;
    }

    // Refresh just the timer without changing the command — used by
    // commands like /verbose or /aoe which the owner intends not to
    // disturb manual mode (they're settings, not movement orders).
    void refresh_manual(uint32 now_ms, uint32 ttl_ms = 30000)
    {
        if (owner_command_ != OwnerCommand::None)
            manual_mode_until_ms_ = now_ms + (ttl_ms == 0 ? 30000u : ttl_ms);
        last_owner_command_ms_ = now_ms;
    }

    // Drop manual mode — autonomous rules resume. Also called when the
    // window expires (cheaper to leave the field stale and check the
    // ttl in is_manual_mode, but a /resume should clear immediately).
    void exit_manual()
    {
        owner_command_         = OwnerCommand::None;
        owner_target_          = ObjectGuid::Empty;
        manual_mode_until_ms_  = 0;
    }

    uint32     last_owner_command_ms() const { return last_owner_command_ms_; }
    void       set_last_owner_name(std::string name) { last_owner_name_ = std::move(name); }
    std::string const& last_owner_name() const { return last_owner_name_; }

    // Formation slot + type (Phase D). Slot is owner-scoped: a bot keeps
    // the same slot id across formation type changes, so /formation
    // wedge → /formation column reorders without re-numbering. Type is
    // shared by every bot in the owner's squad — set via /formation.
    uint8        formation_slot() const { return formation_slot_; }
    void         set_formation_slot(uint8 s) { formation_slot_ = s; }
    FormationType formation_type() const { return formation_type_; }
    void         set_formation_type(FormationType t) { formation_type_ = t; }

    // Effective formation type with auto-spread override: when the owner's
    // chosen type is Tight and the bot is in combat, return Spread instead
    // so AoE doesn't crater a stacked squad. Restored to the owner's choice
    // when combat ends. Other formation choices (Spread, Line, Column,
    // Wedge, Circle, Free) are honored as-is — the override only applies to
    // the "stack on leader" mode that's most vulnerable to splash damage.
    FormationType effective_formation_type(bool in_combat) const
    {
        if (in_combat && formation_type_ == FormationType::Tight)
            return FormationType::Spread;
        return formation_type_;
    }

    // ------------------------------------------------------------------
    // Dungeon-run mode (Phase B of GROUP_DUNGEON_PLAN.md).
    //
    // When Active, the bot drives the dungeon execution rules
    // (idle:tank_pull_next, idle:dungeon_interrupt, etc) instead of
    // waiting for an owner /follow / /come / /attack cycle. Activated
    // by:
    //   * Bot is group leader inside a dungeon AND the rest of the
    //     group is bots owned by the same account.
    //   * Owner whisper /squadrun or party /;run.
    //   * Group leadership handover to the bot while inside an
    //     instance (auto-trigger).
    // Deactivated by:
    //   * Bot exits the dungeon map.
    //   * Group disbanded.
    //   * Owner /squadstop or ;stop.
    // Paused (e.g. /run pause) suspends pulls but keeps healing /
    // defensive logic alive — used between difficult packs.
    enum class DungeonRunMode : uint8 { Off = 0, Active = 1, Paused = 2 };

    DungeonRunMode dungeon_run_mode() const { return dungeon_run_mode_; }
    void           set_dungeon_run_mode(DungeonRunMode m)
    {
        // Reset waypoint index on flip-to-Active so a fresh `;run`
        // starts the progression from the beginning of the current
        // dungeon's path. No effect on Off/Paused transitions.
        if (m == DungeonRunMode::Active && dungeon_run_mode_ != DungeonRunMode::Active)
            dungeon_waypoint_index_ = 0;
        dungeon_run_mode_ = m;
    }
    bool           dungeon_active() const
    { return dungeon_run_mode_ == DungeonRunMode::Active; }

    // Battleground-run mode (mirrors DungeonRunMode for BG context).
    // Active when bot is in a battleground; drives objective-aware
    // BG rules (flag-carrier escort, node assault, healer focus).
    enum class BgRunMode : uint8 { Off = 0, Active = 1, Paused = 2 };
    BgRunMode bg_run_mode() const { return bg_run_mode_; }
    void      set_bg_run_mode(BgRunMode m) { bg_run_mode_ = m; }
    bool      bg_active() const { return bg_run_mode_ == BgRunMode::Active; }

    // Phase J: dungeon loop-mode. When set, the auto-leave ladder
    // (use teleporter / hearth) is followed by an LFG re-queue for the
    // last dungeon the bot ran. Off by default; enabled per-bot via
    // owner whisper `/squadrun loop on`.
    bool dungeon_loop_mode() const { return dungeon_loop_mode_; }
    void set_dungeon_loop_mode(bool v) { dungeon_loop_mode_ = v; }

    // Last LFG dungeon id the bot queued / ran. Recorded by the
    // LfgQueueIntent executor and re-used by the loop-mode re-queue
    // rule. Zero = no prior queue / unknown.
    uint32 last_lfg_dungeon_id() const { return last_lfg_dungeon_id_; }
    void   set_last_lfg_dungeon_id(uint32 id) { last_lfg_dungeon_id_ = id; }

    // Phase J dedup: world-ms of the last loop-mode re-queue emit so the
    // LfgQueueIntent doesn't fire every tick while the bot is between
    // dungeons (LFG state takes a tick or two to settle, and the rule's
    // gate doesn't immediately observe the queued state). 0 = primed.
    uint32 last_loop_requeue_ms() const { return last_loop_requeue_ms_; }
    void   note_loop_requeue(uint32 now_ms) { last_loop_requeue_ms_ = now_ms; }

    // Tank-pull pacing — bot tracks the world-ms timestamp of its last
    // kill so the next pull rule can wait ≥5 s for healer mana regen.
    uint32 last_kill_ms() const { return last_kill_ms_; }
    void   note_kill(uint32 now_ms) { last_kill_ms_ = now_ms; }
    // Combat-entry stamp — set on Idle→InCombat, read on the way out so the
    // "surviving a fight = a kill" pacing heuristic can ignore sub-1.5s
    // combat-flag flaps (aggro transfers, proximity flags) that otherwise
    // reset the tank's post-kill timer every few seconds and starve
    // tank_advance.
    uint32 combat_entered_ms() const { return combat_entered_ms_; }
    void   set_combat_entered_ms(uint32 ms) { combat_entered_ms_ = ms; }
    // 10s throttle for the [tank_advance] gate/no-target diagnostics.
    bool tank_diag_due(uint32 now_ms)
    {
        if (now_ms - tank_diag_ms_ < 10'000u) return false;
        tank_diag_ms_ = now_ms;
        return true;
    }

    // Per-bot contribution counters for the current dungeon/raid run.
    // `dungeon_kills_` is the count of creatures this bot landed the
    // killing blow on while inside an instance. `dungeon_deaths_` is
    // the count of times this bot died inside an instance. Both reset
    // when the bot leaves the instance map (in dispatch_layers) so a
    // /diag readout reflects the CURRENT run, not lifetime totals.
    // Surfaced on the /diag Dungeon line for operator visibility.
    uint32 dungeon_kills()  const { return dungeon_kills_; }
    uint32 dungeon_deaths() const { return dungeon_deaths_; }
    void   note_dungeon_kill()  { if (dungeon_kills_  < 0xFFFFFFFFu) ++dungeon_kills_; }
    void   note_dungeon_death() { if (dungeon_deaths_ < 0xFFFFFFFFu) ++dungeon_deaths_; }
    void   reset_dungeon_contribution()
                                { dungeon_kills_ = 0; dungeon_deaths_ = 0; }

    // ---- Open-world death-spiral memory ----
    // A 0%-durability bot dies to trivial mobs, rezzes near the same camp, and
    // dies again — perpetually InCombat/Dead, never OOC, so the repair paths
    // never fire. This tracks REPEATED deaths in the SAME open-world spot so the
    // death-spiral-escape (State_Dead) and the broken-gear disengage
    // (State_InCombat) can break the loop. ONLY open-world deaths increment the
    // counter — the SINGLE death edge in BotAI::tick gates the call on
    // !is_in_instance() && !in_battleground() so a normal dungeon/raid wipe (a
    // legitimately hard fight) NEVER arms the spiral. A new death within ~40y of
    // the last AND within ~5min increments; otherwise it resets (the bot moved
    // on / is dying somewhere new). reset_death_spiral() is called on a
    // successful repair and on a zone change.
    static constexpr float  kSameSpotDeathRadius  = 40.0f;          // yards
    static constexpr float  kSameSpotDeathRadius2 = 40.0f * 40.0f;
    static constexpr uint32 kSameSpotDeathWindowMs = 5u * 60u * 1000u;
    uint32 consecutive_same_spot_deaths() const { return consecutive_same_spot_deaths_; }
    uint32 last_death_ms() const { return last_death_ms_; }
    void   note_open_world_death(float x, float y, float z, uint32 now_ms)
    {
        const bool first = (last_death_ms_ == 0);
        const float dx = x - last_death_x_, dy = y - last_death_y_;
        const bool same_spot =
            !first &&
            (now_ms - last_death_ms_) <= kSameSpotDeathWindowMs &&
            (dx * dx + dy * dy) <= kSameSpotDeathRadius2;
        if (same_spot)
        {
            if (consecutive_same_spot_deaths_ < 0xFFFFFFFFu)
                ++consecutive_same_spot_deaths_;
        }
        else
        {
            consecutive_same_spot_deaths_ = 1;   // this death is the first in a new spot
        }
        last_death_ms_ = now_ms;
        last_death_x_  = x; last_death_y_ = y; last_death_z_ = z;
    }
    // Cleared after a successful repair or a zone change so the bot isn't
    // treated as spiralling once it has escaped / fixed its gear.
    void   reset_death_spiral()
    { consecutive_same_spot_deaths_ = 0; last_death_ms_ = 0; }

    // --- Death blackspots (owner idea, 2026-06-22) ---
    // When the bot dies repeatedly in the SAME open-world spot (the same-spot
    // counter above reaches kBlackspotDeathThreshold), mark that area as a
    // temporary travel BLACKSPOT. The shared walker then routes AROUND it — trying
    // another way to the SAME goal — instead of marching back into the killing
    // field every time (live: Morthan L9 walking his classic Tirisfal quest through
    // a phased L50 war-camp). The quest is NOT abandoned: if a way around exists the
    // bot finds it; if not, it avoids the death zone and the NoProgress watchdog
    // retires the unreachable goal. TTL'd so the area is retried later (mobs moved /
    // bot out-leveled the camp). Armed from BotAI::tick's death edge.
    struct DeathBlackspot { float x = 0.f, y = 0.f, r = 0.f; uint32 expiry_ms = 0; };
    static constexpr uint32 kBlackspotDeathThreshold = 3;            // same-spot deaths before marking
    static constexpr float  kBlackspotRadius         = 60.0f;        // > kSameSpotDeathRadius (clear the aggro field)
    static constexpr uint32 kBlackspotTtlMs          = 8u * 60u * 1000u;
    // Add/refresh a blackspot at (x,y). Merges with an overlapping active one.
    void arm_death_blackspot(float x, float y, uint32 now_ms);
    // True if (x,y) lies inside any active (unexpired) blackspot.
    bool in_death_blackspot(float x, float y, uint32 now_ms) const;
    // If the straight path from (fx,fy) toward (tx,ty) would cross an active
    // blackspot, write a deflected waypoint that skirts its edge on the goal side
    // and return true; otherwise return false (path is clear / no blackspots).
    bool deflect_for_blackspot(float fx, float fy, float tx, float ty,
                               uint32 now_ms, float& ox, float& oy) const;

    // --- JIT BG-purpose confinement (owner directive 2026-06-22: "JIT bots
    // shouldn't do anything but their purpose (bg/bf/dungeon/raid)") ---
    // A bot the population manager spawned PURELY to fill a BG queue. Set when the
    // setup pipeline pushes its queue intent. While set (and not yet inside the
    // instance) idle:bg_jit_staging confines the bot to queue -> wait staged ->
    // port in — no questing, roaming, or grinding. That confinement is ALSO what
    // makes matches form: staged bots stay queued+ready so both factions reach
    // MinPlayers and the matchmaker can invite (they used to wander off / fight and
    // never assemble). Cleared on a hard timeout so a bot whose match never forms
    // reverts and the TotalTarget=0 LRU retires it.
    bool   is_jit_purpose()     const { return jit_purpose_bg_type_ != 0; }
    uint16 jit_bg_type()        const { return jit_purpose_bg_type_; }
    uint32 jit_purpose_set_ms() const { return jit_purpose_set_ms_; }
    void   set_bg_jit_purpose(uint16 bg_type, uint32 now_ms)
    { jit_purpose_bg_type_ = bg_type; jit_purpose_set_ms_ = now_ms; }
    void   clear_jit_purpose() { jit_purpose_bg_type_ = 0; jit_purpose_set_ms_ = 0; }

    // Last interrupt emit — used by the slot-based interrupt scheduler
    // (Phase D) to stagger multiple casters' interrupts so they don't
    // all kick the same cast simultaneously.
    uint32 last_interrupt_ms() const { return last_interrupt_ms_; }
    void   note_interrupt(uint32 now_ms) { last_interrupt_ms_ = now_ms; }

    // Last level at which the bot auto-equipped pending strict-ilvl upgrades.
    // The State_Idle auto-equip rule fires once per ding rather than every
    // tick — without this dedup the rule would re-emit EquipItemIntent every
    // tick the bot is idle (the snapshot's upgrades_pending only drops after
    // the world thread processes the equip and the next snapshot pub catches
    // it, which can take several ticks). 0 = never auto-equipped (priming).
    uint8 last_auto_equip_level() const { return last_auto_equip_level_; }
    void  set_last_auto_equip_level(uint8 v) { last_auto_equip_level_ = v; }
    // Periodic equip re-check (C10): time of the last bag walk, so bots that
    // stopped leveling still equip looted/rewarded upgrades every few minutes.
    uint32 last_auto_equip_check_ms() const { return last_auto_equip_check_ms_; }
    void   set_last_auto_equip_check_ms(uint32 v) { last_auto_equip_check_ms_ = v; }

    // ID of the most recent spell the bot successfully cast (set by the
    // IntentVisitor on Result::Ok). Drives the Monk Windwalker Combo
    // Strikes mastery — using the same melee ability twice in a row
    // breaks Mastery and loses the bonus damage, so spenders gate on
    // `last_cast != self`. Cleared on logout via reset_per_login.
    uint32 last_cast_spell_id() const { return last_cast_spell_id_; }
    void   set_last_cast_spell_id(uint32 sid) { last_cast_spell_id_ = sid; }

    // World-Z captured at the instant the bot first attaches to a
    // transport (on_transport flips false→true). Drives the elevator
    // exit rule: when on_transport && transport_z_stable, compare
    // current_Z to elevator_boarding_z_ to decide whether the
    // platform has carried us far enough to step off. Reset to 0
    // when on_transport flips back to false.
    float elevator_boarding_z() const { return elevator_boarding_z_; }
    void  set_elevator_boarding_z(float z) { elevator_boarding_z_ = z; }

    // Milliseconds the bot's world-Z has held within ±0.3y while on
    // transport — proxy for "platform is at rest at a stop frame".
    // The snapshot's transport_stopped is always-true for type-11
    // elevators (the snapshot builder dynamic_casts to the type-15
    // ship Transport class and falls back to true when that fails),
    // so the ElevatorStepOff rule cannot use it. This counter is the
    // workaround: it grows tick-by-tick while Z is stable, resets on
    // any meaningful Z change. step_off requires ≥500ms of stability.
    uint32 transport_z_stable_ms() const { return transport_z_stable_ms_; }

    // Dedup for the auto-apply-starter-talents rule: when active_talents is
    // empty (fresh bot / post-respec), the rule emits ApplyStarterTalentsIntent
    // once. Cleared when active_talents becomes populated, allowing a fresh
    // apply after a future respec leaves the loadout empty.
    bool starter_talents_acked() const { return starter_talents_acked_; }
    void set_starter_talents_acked(bool v) { starter_talents_acked_ = v; }
    // Last ApplyStarterTalents emit time — drives the convergence retry
    // (re-emit every ~2 min while the trait config stays empty) instead of
    // the old fire-once latch that left failed commits unretried forever.
    uint32 starter_talents_emit_ms() const { return starter_talents_emit_ms_; }
    void   set_starter_talents_emit_ms(uint32 v) { starter_talents_emit_ms_ = v; }

    uint8 last_applied_talent_ctx() const { return last_applied_talent_ctx_; }
    void  set_last_applied_talent_ctx(uint8 ctx) { last_applied_talent_ctx_ = ctx; }

    // Dedup for the auto-extend-starter-talents-on-ding rule. State_Idle
    // re-fires ApplyStarterTalentsIntent once per level when the bot is on
    // the starter build and dings (so newly-granted trait points get spent).
    // 0 = primed; equal to current level = already extended this level.
    uint8 last_starter_extend_level() const { return last_starter_extend_level_; }
    void  set_last_starter_extend_level(uint8 v) { last_starter_extend_level_ = v; }

    // Per-bot verbose logging toggle. Owners can enable to trace rule fires
    // to the server log (TC_LOG_DEBUG "playerbot.v2") for debugging without
    // needing to restart the server with global logging enabled.
    bool verbose_logging() const { return verbose_logging_; }
    void set_verbose_logging(bool v) { verbose_logging_ = v; }

    // Last server-time (ms) we emitted an atmospheric idle emote (a flavor
    // /wave / /cheer / /salute that fires only for Roleplay-personality bots
    // when relaxed in a rested area). Throttled to once per ~5min so a tavern
    // full of bots doesn't drown chat in /salute spam. 0 = primed.
    uint32 last_ambient_emote_ms() const { return last_ambient_emote_ms_; }
    void   set_last_ambient_emote_ms(uint32 v) { last_ambient_emote_ms_ = v; }
    // Last server-time (ms) the bot idle-rotated to "look at" a nearby NPC
    // (idle:look_at_npc). Drives a jittered 30-60s cadence so bots aren't
    // eerily still in a hub. 0 = primed (fire on first eligible tick).
    uint32 last_look_around_ms() const { return last_look_around_ms_; }
    void   set_last_look_around_ms(uint32 v) { last_look_around_ms_ = v; }
    // Last server-time (ms) the bot struck a sustained "state" emote
    // (idle:ambient_state_emote — dance/sit/sleep while idle in a hub).
    // Low-frequency; 0 = primed.
    uint32 last_ambient_state_emote_ms() const { return last_ambient_state_emote_ms_; }
    void   set_last_ambient_state_emote_ms(uint32 v) { last_ambient_state_emote_ms_ = v; }
    uint32 last_guild_babble_ms() const { return last_guild_babble_ms_; }
    void   set_last_guild_babble_ms(uint32 v) { last_guild_babble_ms_ = v; }
    uint32 last_yell_lfg_ms() const { return last_yell_lfg_ms_; }
    void   set_last_yell_lfg_ms(uint32 v) { last_yell_lfg_ms_ = v; }

    // Most recently attempted engage target + timestamp. The autonomous
    // grind rule (`idle:engage_nearby_mob`) skips this target for a few
    // seconds after the attempt so that a mob the bot can't actually reach
    // (behind a wall, off-pathmesh, blocked by terrain) doesn't trap the
    // bot in an infinite engage→fail→re-engage loop on the same GUID.
    // After the cooldown, the bot will wander away or pick a different
    // target. On a successful engage, the mob enters combat / dies and
    // disappears from `nearby_enemies` anyway, so the cooldown is moot.
    ObjectGuid last_engage_target() const { return last_engage_target_; }
    uint32     last_engage_at_ms() const { return last_engage_at_ms_; }
    // shield_ms: how long engage rules must skip this target. Default 8s
    // covers the transient case (LoS flicker, pathing hiccup). The opener
    // give-up path passes minutes — a target the bot demonstrably could
    // not reach with both casts AND approach steps must not be re-picked
    // the moment the 8s shield lapses (observed 2026-06-13: Somi/Anigo in
    // an engage→OutOfRange-spam→disengage→re-engage loop for hours).
    uint32     last_engage_shield_ms() const { return last_engage_shield_ms_; }
    void       note_engage(ObjectGuid g, uint32 ms, uint32 shield_ms = 8000)
    { last_engage_target_ = g; last_engage_at_ms_ = ms; last_engage_shield_ms_ = shield_ms; }
    // True while `g` is the most recently noted engage target AND its shield
    // window has not yet lapsed. Final-review fix (2026-07-03): the dungeon
    // pull-gate pick loops (idle:dungeon_focus_assist / idle:dungeon_dps_assist)
    // called note_engage() on gate-skip but never consulted the shield
    // themselves, so a gated candidate was re-picked — and re-probed with a
    // full tank pathfind — every single tick instead of once per shield
    // window. Callers MUST check this BEFORE probing/selecting a candidate.
    bool       engage_shielded(ObjectGuid g, uint32 now_ms) const
    {
        return !g.IsEmpty() && g == last_engage_target_ &&
               now_ms - last_engage_at_ms_ < last_engage_shield_ms_;
    }

    // ---- dungeon route-waypoint follow cursor ----
    // The winding-corridor route follower (idle:dungeon_tank_advance_boss_route,
    // pass 1) must COMMIT to one breadcrumb and walk to it, not re-pick the
    // "best" waypoint every tick. A stateless per-tick pick OSCILLATES whenever
    // the tank sits near a selection threshold: live WC 2026-07-01, the tank
    // parked ~15y from route seq1 with the nearest-crumb distance straddling the
    // old 15y re-converge cutoff, flip-flopping between "re-converge to the
    // nearest crumb" (N) and "forward-scan to a far crumb" (S) — two targets
    // 32y apart, opposite directions, ~0 net progress. The 32y target swing is
    // far past the API::move_to dedup radius (3y), so every flip RESET the POINT
    // spline and the tank jittered in place. Committing to a single crumb until
    // it is reached, then advancing the cursor by one, gives the hysteresis that
    // kills the oscillation and walks the ordered chain to the boss. Bound to the
    // map so it self-invalidates across an LFG teleport / map change.
    int32_t    dungeon_route_wp(uint32 map_id) const
    { return (dungeon_route_wp_map_ == map_id) ? dungeon_route_wp_ : -1; }
    void       set_dungeon_route_wp(int32_t idx, uint32 map_id)
    { dungeon_route_wp_ = idx; dungeon_route_wp_map_ = map_id; }
    void       clear_dungeon_route_wp() { dungeon_route_wp_ = -1; dungeon_route_wp_map_ = 0; }

    // ---- route-aware combat-advance reached-crumb latch (2026-07-03, 1b) ----
    // Hysteresis for DungeonAdvanceTarget's 8y arrive boundary. INVARIANT: once
    // the in-combat advance has observed the bot WITHIN kRouteArrive (8y) of
    // the route cursor's crumb, it must NOT re-substitute that SAME crumb as
    // its walk target — even if combat micro-movement drifts the bot back
    // beyond 8y — until the route rule ADVANCES the cursor past the recorded
    // index (or the map changes). A stateless 8y threshold would flip the walk
    // target crumb<->boss on every straddle; where the two diverge >3y each
    // flip defeats the DungeonStepAlreadyInFlight dedup and restarts the
    // spline — exactly the stateless-threshold oscillation the committed route
    // cursor above exists to prevent. Map-bound like dungeon_route_wp (the
    // getter returns the -1 sentinel on any map mismatch, so an LFG teleport
    // self-invalidates the latch); set by DungeonAdvanceTarget ONLY, compared
    // against the CURRENT cursor — a cursor advance makes the recorded index
    // stale and substitution resumes on the next crumb automatically.
    int32_t    adv_route_reached_idx(uint32 map_id) const
    { return (adv_route_reached_map_ == map_id) ? adv_route_reached_idx_ : -1; }
    void       set_adv_route_reached(int32_t idx, uint32 map_id)
    { adv_route_reached_idx_ = idx; adv_route_reached_map_ = map_id; }

    // ---- route CONSUMED latch (campaign class-B, 2026-07-20) ----
    // Set by the route follower when it declines at the arrived FINAL crumb
    // (cur == boss_i, within kRouteArrive): the chain is walked and the
    // boss-ward fallback owns the remaining approach. While consumed, the
    // in-combat/fallback crumb substitution stops re-selecting that crumb,
    // so the tank cannot be dragged back to it after the fallback steps
    // 20y boss-ward and the reached-latch release band (1i) frees the
    // ordinary latch — the live 20y patrol loop (Blackfathom, Sunken
    // Temple). Map-bound (-1 sentinel on mismatch, self-invalidating on
    // LFG teleport) exactly like dungeon_route_wp / adv_route_reached.
    // Cleared automatically when the cursor moves OFF the consumed index.
    int32_t    route_consumed_idx(uint32 map_id) const
    { return (route_consumed_map_ == map_id) ? route_consumed_idx_ : -1; }
    void       set_route_consumed(int32_t idx, uint32 map_id)
    { route_consumed_idx_ = idx; route_consumed_map_ = map_id; }

    // ---- dungeon off-mesh crossing commitment ----
    // When a dungeon bot begins crossing an off-mesh bridge (a single stable
    // far-vertex step longer than the per-tick step cap), it MUST keep targeting
    // that fixed far vertex until it lands on the far ledge. Otherwise the per-tick
    // re-evaluation switches the goal the instant the bot goes off-mesh mid-span
    // (a follower's regroup falls back to the MOVING tank; once off-mesh, every
    // path is NoPath/FarFromPoly) — which RESTARTS the POINT spline over the void
    // and strands the bot off-mesh with no recovery (Deadmines Gap-1, live
    // 2026-06-25: 4 followers wedged at ~(-213,-532) in the bridge void). Holding
    // a stable goal lets the API::move_to off-mesh dedup carry the spline whole
    // across the connection onto the landing vertex. Short TTL + reached-test keeps
    // it self-correcting (a stale commit just walks the bot to the ledge and ends).
    bool       dungeon_cross_active(uint32 now_ms) const
    { return dungeon_cross_until_ms_ != 0 && now_ms < dungeon_cross_until_ms_; }
    float      dungeon_cross_x() const { return dungeon_cross_x_; }
    float      dungeon_cross_y() const { return dungeon_cross_y_; }
    float      dungeon_cross_z() const { return dungeon_cross_z_; }
    void       set_dungeon_cross(float x, float y, float z, uint32 until_ms)
    { dungeon_cross_x_ = x; dungeon_cross_y_ = y; dungeon_cross_z_ = z;
      dungeon_cross_until_ms_ = until_ms; }
    // DIRECT crossing (DB traversal link, playerbot_nav_links): DungeonHonorCross
    // drives it with a straight no-pathfind MovePoint spline instead of a
    // pathfound move_to (which would NoPath across a real navmesh split).
    // Set ONLY by the nav-link hop; deliberately NOT cleared by set_dungeon_cross
    // so the call sites' generic re-commit of the same exit keeps the mode;
    // reset on clear_dungeon_cross (landing / relocate).
    bool       dungeon_cross_direct() const { return dungeon_cross_direct_; }
    void       set_dungeon_cross_direct(bool d) { dungeon_cross_direct_ = d; }
    void       clear_dungeon_cross() { dungeon_cross_until_ms_ = 0; cross_episode_since_ms_ = 0;
                                       dungeon_cross_direct_ = false; }

    // ── Tank chase-commit latch (2026-07-02, stage 4) ──────────────────────
    // Once a tank in combat chooses to walk a genuinely LONG-but-complete
    // corridor to its current victim (a taunt_peel target, a boss, or a
    // Task-2/3 survivor), it must COMMIT and walk the whole thing instead of
    // re-deciding every tick — the SFK wedge is oscillation, not the corridor
    // itself. Mirrors dungeon_route_wp's map-bound commit idiom just above:
    // the identity getters take the CALLER's current map_id and return the
    // "no commitment" sentinel (empty guid / 0ms) on any mismatch, so an LFG
    // teleport self-invalidates the latch instead of chasing a guid that
    // belongs to a different instance.
    ObjectGuid chase_commit_target(uint32 map_id) const
    { return (chase_commit_map_ == map_id) ? chase_commit_target_ : ObjectGuid::Empty; }
    uint32     chase_commit_since_ms(uint32 map_id) const
    { return (chase_commit_map_ == map_id) ? chase_commit_since_ms_ : 0; }
    uint32     chase_commit_last_plan_ms(uint32 map_id) const
    { return (chase_commit_map_ == map_id) ? chase_commit_last_plan_ms_ : 0; }
    void       set_chase_commit(ObjectGuid g, uint32 now_ms, uint32 map_id)
    {
        chase_commit_target_ = g;
        chase_commit_since_ms_ = now_ms ? now_ms : 1u;
        chase_commit_last_plan_ms_ = now_ms;
        chase_commit_map_ = map_id;
    }
    // The plan timestamp doubles as the caller's PRE-COMMIT probe cadence
    // (the detour probe is a full pathfind and must not run per-tick), so
    // touch also runs while NO commitment is armed and must bind the map
    // itself for the map-bound getter above to work. On a map change it
    // drops any stale identity fields FIRST, so re-binding the timestamp on
    // the new map can never resurrect a previous map's commitment through
    // the map-bound identity getters.
    void       touch_chase_commit_plan(uint32 now_ms, uint32 map_id)
    {
        if (chase_commit_map_ != map_id)
        {
            chase_commit_target_ = ObjectGuid::Empty;
            chase_commit_since_ms_ = 0;
        }
        chase_commit_last_plan_ms_ = now_ms;
        chase_commit_map_ = map_id;
    }
    void       clear_chase_commit()
    {
        chase_commit_target_ = ObjectGuid::Empty;
        chase_commit_since_ms_ = 0;
        chase_commit_last_plan_ms_ = 0;
        chase_commit_map_ = 0;
    }

    // ── Movement-objective commitment (2026-07-03, increment 1h) ───────────
    // Movement-objective commitment: the first dungeon-family rule to emit a
    // step toward a target owns movement for a short window; competing rules
    // hold instead of re-aiming (verbatim live failure 2026-07-03: route step
    // east + converge_to_fight west alternating every 150ms restarted the
    // spline 6-7x/sec — every same-target dedup layer is pairwise-defeated by
    // two objectives). Window: committed target + last-commit timestamp; a
    // SAME-target (<=3y) re-emit refreshes it. Expiry: kMoveCommitMs after the
    // last refresh — and the guard additionally requires is_moving, so a dead
    // spline reopens movement to anyone immediately.
    //
    // In practice the OWNING rule refreshes only when it re-emits — and a
    // same-target re-emit (<=3y) hits DungeonStepAlreadyInFlight FIRST at the
    // call site and skips the emit entirely, so refresh is rare. Expiry then
    // lets the OTHER objective take a window: alternation settles to a
    // >=kMoveCommitMs cadence with real walking progress each window, instead
    // of a re-aim every tick. Map-bound exactly like chase_commit/
    // dungeon_route_wp above: the getters take the CALLER's current map_id
    // and report "no commitment" on any mismatch, so an LFG teleport self-
    // invalidates it instead of chasing a stale commitment from another
    // instance.
    //
    // Increment 1k (2026-07-18): pure-XYZ arbitration is not enough when TWO
    // rules chase the SAME objective through DIFFERENT steppers. Live WC
    // crumb-27 lip: the route rule and rule (0) both target route crumb 27,
    // but from start positions ~9y apart their capped path-steppers return
    // points ~21y apart (route: direct descent step; rule (0): east-
    // switchback point) — past the 3y kOwnedRange, so the XYZ check alone
    // sees "different target" and lets both sides fight for the window every
    // 2.5s, each spline-restarting the other; the tank shuttles in place and
    // never completes the 19y descent. Fix: additionally key the commitment
    // on the OBJECTIVE (the route-crumb index the step serves), threaded in
    // by the crumb-following call sites (DungeonAdvanceTarget's out_crumb_idx,
    // the route follower's committed route_cur). -1 means "not a crumb-based
    // step" (rejoin/converge/direct-advance/off-mesh-recovery — those keep
    // the pre-1k pure-XYZ semantics unchanged). A caller with the SAME
    // objective as the in-flight commitment defers to it even when its own
    // capped step point lands >3y away — the two rules agree on WHERE they
    // are going, just not on the intermediate waypoint, so the in-flight
    // solution should be allowed to run to completion instead of being
    // fought over.
    // Increment 1m (2026-07-20): PROGRESS-STICKY ownership. The prior
    // kMoveCommitMs=2500 fixed wall-clock window (comment above) was tuned
    // for "2.5s of genuine walking per window" but live campaign evidence
    // (2026-07-19/20, 6+ dungeons: Blackfathom Deeps, Sunken Temple,
    // Zul'Farrak, LBRS, Stratholme, Maraudon — 40% of all campaign
    // failures) shows the window routinely expires MID-STRIDE: a ~20y step
    // at ~7y/s plus spline-start latency exceeds 2.5s, so ownership flips
    // to the contender every window, the new owner restarts the spline
    // toward ITS target, and the two rules oscillate forever with the bot
    // frozen (<2y net drift) for 10+ minutes — "2.5s of spline restart with
    // zero net progress" instead of the intended real walking. Fix: a
    // commitment stays active as long as the bot keeps CLOSING distance on
    // the committed target (tracked by move_commit_note_progress(), called
    // once per dungeon tick) — expire on STALLED progress (no improvement
    // for kMoveCommitStallMs), not on wall-clock alone. kMoveCommitMaxMs is
    // a hard cap so a pathological commitment (e.g. a target that can never
    // actually be reached, yet keeps registering sub-threshold "closer"
    // ticks from jitter) cannot own the spline forever.
    bool       move_commit_active(uint32 map_id, uint32 now_ms) const
    {
        if (move_commit_map_ != map_id || move_commit_ms_ == 0)
            return false;
        constexpr uint32 kMoveCommitStallMs = 3000;
        constexpr uint32 kMoveCommitMaxMs   = 15000;
        const uint32 stall_ref = move_commit_progress_ms_ ? move_commit_progress_ms_
                                                            : move_commit_ms_;
        return (now_ms - stall_ref) < kMoveCommitStallMs &&
               (now_ms - move_commit_ms_) < kMoveCommitMaxMs;
    }
    void       move_commit_target(float& x, float& y, float& z) const
    { x = move_commit_x_; y = move_commit_y_; z = move_commit_z_; }
    // Diagnostics only (increment 1m) — age of the commitment itself and
    // age since progress last improved, for the [move_owned] log line. 0
    // when there is no commitment (caller should already be gating on
    // move_commit_active/move_commit_map_ as everywhere else in this block).
    uint32     move_commit_age_ms(uint32 now_ms) const
    { return move_commit_ms_ ? (now_ms - move_commit_ms_) : 0; }
    uint32     move_commit_prog_age_ms(uint32 now_ms) const
    { return move_commit_progress_ms_ ? (now_ms - move_commit_progress_ms_) : 0; }
    // Objective the CURRENTLY-committed move serves (route-crumb index), or
    // -1 if none/not crumb-based. Map-bound like the rest of this block: a
    // map mismatch (LFG teleport to another instance) reports -1 rather than
    // a stale objective from a previous instance. Deliberately does NOT also
    // check move_commit_active/expiry — callers that care already gate on
    // move_commit_active(map, now) before consulting this, exactly like the
    // existing move_commit_target().
    int32_t    move_commit_objective(uint32 map_id) const
    { return (move_commit_map_ == map_id) ? move_commit_objective_ : -1; }
    void       note_move_commit(uint32 map_id, float x, float y, float z, uint32 now_ms,
                                int32_t objective = -1)
    {
        move_commit_x_ = x;
        move_commit_y_ = y;
        move_commit_z_ = z;
        move_commit_ms_ = now_ms ? now_ms : 1u;
        move_commit_map_ = map_id;
        move_commit_objective_ = objective;
        // Increment 1m: reset the progress clock alongside the commitment
        // itself, and reset best_d2_ to the sentinel below so the very next
        // move_commit_note_progress() call establishes the distance
        // baseline unconditionally (no bot position is known here — only
        // the target is — so "improvement" can't be judged until the first
        // post-commit position sample).
        move_commit_progress_ms_ = move_commit_ms_;
        move_commit_best_d2_ = std::numeric_limits<float>::max();
    }
    // Increment 1m (2026-07-20): called once per dungeon tick (top of
    // DungeonDispatch and DispatchInCombat's dungeon block — see those call
    // sites) with the bot's CURRENT position. Refreshes
    // move_commit_progress_ms_ whenever distance-to-the-committed-target has
    // improved by more than kMoveCommitProgressY since the best distance
    // previously observed, so move_commit_active() can tell a genuinely
    // walking owner from a stalled one. No-op when there is no commitment
    // for this map (nothing to track) — matches the map-bound self-
    // invalidation of the rest of this block.
    void       move_commit_note_progress(uint32 map_id, float cur_x, float cur_y,
                                         float cur_z, uint32 now_ms)
    {
        if (move_commit_map_ != map_id || move_commit_ms_ == 0)
            return;
        const float dx = cur_x - move_commit_x_;
        const float dy = cur_y - move_commit_y_;
        const float dz = cur_z - move_commit_z_;
        const float d2 = dx * dx + dy * dy + dz * dz;
        constexpr float kMoveCommitProgressY = 2.0f;   // closing-distance margin
        // best_d2_ == FLT_MAX sentinel: no baseline yet (fresh commitment) —
        // the first observation always counts as progress.
        bool improved;
        if (move_commit_best_d2_ >= std::numeric_limits<float>::max() * 0.5f)
            improved = true;
        else
        {
            const float d = std::sqrt(d2);
            const float best_d = std::sqrt(move_commit_best_d2_);
            improved = (best_d - d) > kMoveCommitProgressY;
        }
        if (improved)
        {
            move_commit_best_d2_ = d2;
            move_commit_progress_ms_ = now_ms ? now_ms : 1u;
        }
    }

    // ---- refused-destination memory (refusal-aware target selection,
    // 2026-07-20) ----
    // BUG THIS FIXES: PlayerbotAPI::move_to keeps a PER-DESTINATION path-fail
    // backoff — one failed pathfind poisons that destination for a TTL, and
    // every later move_to to ~that destination returns Result::Locked
    // without issuing a spline. The RULE has no idea the destination is
    // poisoned — it re-selects the SAME destination every tick, gets Locked,
    // and the backoff is continuously re-armed => a PERMANENT FREEZE over a
    // perfectly valid navmesh (live: bot 164465, dst=(1848.1,1530.5,123.4),
    // "[move_lock] reason=path_fail_backoff" repeating forever, headless
    // probes proved the mesh itself was fine). This ring lets the AI side
    // remember "that spot was just refused by the API" so the NEXT tick's
    // candidate selection (dungeon step-emit sites, route-follower crumb
    // scan) can skip it and try a DIFFERENT candidate instead of
    // re-committing to the same poisoned destination.
    //
    // 4-slot ring (oldest slot overwritten on note), TTL-gated so a stale
    // refusal eventually ages out and the destination can be retried.
    // kMoveRefusedTtlMs is deliberately a bit longer than the API's own
    // short (3s) path-fail backoff so we don't re-try INSIDE that window and
    // immediately re-poison it. Radius matches the emitter dedup (3y) so a
    // refusal at one point also covers near-identical restated destinations.
    static constexpr uint32 kMoveRefusedTtlMs    = 6000;
    static constexpr float  kMoveRefusedRadiusSq = 9.0f;   // 3y
    void note_move_refused(float x, float y, float z, uint32 now_ms)
    {
        move_refused_[move_refused_head_] = RefusedDst{x, y, z, now_ms ? now_ms : 1u};
        move_refused_head_ = (move_refused_head_ + 1) % move_refused_.size();
    }
    bool move_refused_recently(float x, float y, float z, uint32 now_ms) const
    {
        for (RefusedDst const& r : move_refused_)
        {
            if (r.ms == 0 || now_ms - r.ms >= kMoveRefusedTtlMs) continue;
            const float dx = x - r.x, dy = y - r.y, dz = z - r.z;
            if (dx * dx + dy * dy + dz * dz <= kMoveRefusedRadiusSq) return true;
        }
        return false;
    }

    // Cross EPISODE wall-clock. Unlike the three detectors below — all of which the
    // FLIP-FLOP defeats (the committed exit alternates between the two bridge
    // endpoints, so the target-relative best-distance and window clocks reset on
    // every flip, and the bot physically oscillating across the span resets the
    // own-frozen clock) — this measures wall-time since the crossing FIRST became
    // active and is immune to re-commits: set_dungeon_cross only overwrites the
    // target, it does not clear, so this keeps climbing while the bot bounces
    // boss-side<->group-side forever (live 06-29: Dungtank swung -520<->-548 at the
    // Helix Gap, hold/froz never reaching 6s). Resets only on a genuine clear
    // (landing via the d<=6 path, or any force-relocate). Caller force-completes
    // onto the committed exit once it exceeds the cap. Starts lazily on first call.
    uint32     cross_episode_ms(uint32 now_ms)
    {
        if (cross_episode_since_ms_ == 0)
        { cross_episode_since_ms_ = now_ms ? now_ms : 1u; return 0; }
        return now_ms - cross_episode_since_ms_;
    }
    void       cross_episode_reset() { cross_episode_since_ms_ = 0; }
    // Stuck-cross detector (progress-based). An off-mesh crossing can stall while
    // DungeonHonorCross re-emits move_to(exit) every tick — the bot CREEPS a few
    // yards toward the exit without ever landing, so a position-stillness test never
    // trips (observed live 2026-06-26: a strand at the Gap-1 north endpoint took
    // dozens of hold cycles to recover because the re-emitted move_to kept nudging
    // the bot). Track instead the BEST (closest) distance achieved toward the cross
    // target: real progress (closing >=3y) restarts the clock; a bot that cannot
    // get meaningfully closer for the whole window is stuck. Robust to creeping AND
    // to a long legitimate approach (a steadily-closing bot keeps resetting). Caller
    // force-completes the jump once this exceeds the timeout. dist = current 3D
    // distance to the committed exit vertex.
    uint32     cross_hold_stuck_ms(float dist, uint32 now_ms)
    {
        if (cross_hold_since_ms_ == 0 || dist < cross_hold_best_ - 3.0f)
        {
            cross_hold_since_ms_ = now_ms ? now_ms : 1u;
            cross_hold_best_ = dist;
            return 0;
        }
        return now_ms - cross_hold_since_ms_;
    }
    void       cross_hold_reset() { cross_hold_since_ms_ = 0; }
    // Own-position FROZEN clock for an active off-mesh crossing (rally/target
    // independent — a dedicated mirror of frozen_ms that DungeonRecoverStranded-
    // Follower must not clobber). cross_hold_stuck_ms above is TARGET-relative and
    // is defeated when the committed exit churns between this off-mesh endpoint
    // (self, <=6y -> the "landed" path resets the clock) and the far endpoint
    // (>6y), so it never accumulates even while the bot is physically frozen for
    // minutes (live 06-27: Dungtank+Dunghealer wedged at the Gap-1 north endpoint
    // the whole run, move_blocked, 0 cross_relocate firings). This tracks the bot's
    // OWN lack of motion (>3y resets) so it climbs regardless of the churning
    // target; a bot genuinely in-flight on the hop moves >3y and never trips it.
    uint32     cross_frozen_ms(float x, float y, float z, uint32 now_ms)
    {
        const float dx = x - cross_frozen_x_, dy = y - cross_frozen_y_, dz = z - cross_frozen_z_;
        if (cross_frozen_since_ms_ == 0 || (dx*dx + dy*dy + dz*dz) > 3.0f * 3.0f)
        {
            cross_frozen_since_ms_ = now_ms ? now_ms : 1u;
            cross_frozen_x_ = x; cross_frozen_y_ = y; cross_frozen_z_ = z;
            return 0;
        }
        return now_ms - cross_frozen_since_ms_;
    }
    void       cross_frozen_reset() { cross_frozen_since_ms_ = 0; }
    // Windowed NET-PROGRESS detector toward the (stable) committed exit. The live
    // 06-27 Gap-1 wedge that defeated BOTH clocks above: a stranded follower
    // OSCILLATES around the bridge mouth (rogue swung y -510<->-526, ~16y, exit-
    // distance bouncing 21<->37) instead of freezing — every >3y jitter resets
    // cross_frozen_ms, and every transient approach resets cross_hold_stuck_ms's
    // best-distance, so neither ever reaches its timeout though the bot NEVER
    // crosses. This compares the exit-distance NOW vs one window ago: a bot that
    // has not NET-closed the gap by >progress_yd over the whole window is wedged,
    // however it jitters within it. Use for a STABLE exit (follower regroup-cross);
    // cross_frozen_ms still covers the truly-frozen / churning-exit tank case.
    bool       cross_window_noprogress(float dist_to_exit, uint32 now_ms,
                                       uint32 window_ms, float progress_yd)
    {
        if (cross_win_ms_ == 0)
        { cross_win_ms_ = now_ms ? now_ms : 1u; cross_win_d_ = dist_to_exit; return false; }
        if (now_ms - cross_win_ms_ < window_ms) return false;
        const bool stuck = (dist_to_exit > cross_win_d_ - progress_yd);
        cross_win_ms_ = now_ms ? now_ms : 1u;   // start the next window from here
        cross_win_d_  = dist_to_exit;
        return stuck;
    }
    void       cross_window_reset() { cross_win_ms_ = 0; }

    // ---- dungeon false-combat lock ----
    // The whole group can be held InCombat indefinitely by attackers that have
    // NO seedable victim among them — every attacker is ignored (orphaned
    // Glubtok firewall platters 49039-49042 that out-live their dead boss and
    // chase 160y), untargetable (Vanessa Lightning Stalkers 49521), or immune
    // (49671 cutscene double). The seed loop leaves victim empty; the bot can
    // neither DPS its way out nor (cohered with the whole stuck body) trip the
    // strand recovery, and the in-combat boss-push only fires within 150y of a
    // boss — so a group dragged BACK to the zone-in by chasing platters sits in
    // false combat forever (observed live 06-28: all 5 pinned at the entrance
    // (-60,-373) 20+ min, prog frozen 1/6, every attacker a 49042 platter,
    // vic=0). Track how long the bot has been in confirmed false combat so the
    // escape waits out brief transients (a real target dying) before relocating
    // forward off the lock. Returns elapsed ms once latched, 0 while arming.
    uint32     false_combat_ms(bool locked_now, uint32 now_ms)
    {
        if (!locked_now) { false_combat_since_ms_ = 0; return 0; }
        if (false_combat_since_ms_ == 0)
        { false_combat_since_ms_ = now_ms ? now_ms : 1u; return 0; }
        return now_ms - false_combat_since_ms_;
    }
    void       false_combat_reset() { false_combat_since_ms_ = 0; }

    // ---- in-combat victim acquisition latch (untankable-disengage sustain) ----
    // dungeon:untankable_disengage (stage 3, mob-INITIATED aggro variant of the
    // SFK wedge) must not fire on the very first combat tick — a fresh pull needs
    // a moment for the tank to respond before it is judged untankable. opener_
    // victim_since_ms is the WRONG source for this: it is driven exclusively by
    // the OOC PrologueRules approach loop (set_opener_victim), so it goes stale
    // or reflects an unrelated target the instant proximity aggro pulls a bot
    // straight into combat with no opener pass. This is a dedicated latch on the
    // CURRENT in-combat victim guid, mirroring the false_combat_ms idiom: arms
    // to 0 the tick a NEW victim guid is first seen, then returns elapsed ms for
    // as long as the SAME guid persists. A target swap (old victim died / new
    // aggro) restarts the window honestly instead of reusing a stale timestamp.
    // The latch only sees what its caller feeds it, so the disengage rule
    // MAINTAINS it on every pass — passing ObjectGuid::Empty whenever there is
    // no live in-combat victim (out of combat, ghost combat, evade, post-
    // disengage) — which re-arms it through the guid-change branch below; the
    // rule's own fire path also calls combat_victim_latch_reset() explicitly.
    // Both paths exist so a post-shield RE-AGGRO of the SAME guid starts a
    // fresh sustain window instead of inheriting the old since-timestamp and
    // firing on the first tick of the re-engagement.
    uint32     combat_victim_since_ms(ObjectGuid victim, uint32 now_ms)
    {
        if (victim.IsEmpty() || victim != combat_victim_latch_)
        {
            combat_victim_latch_ = victim;
            combat_victim_latch_since_ms_ = 0;
        }
        if (victim.IsEmpty())
            return 0;
        if (combat_victim_latch_since_ms_ == 0)
        { combat_victim_latch_since_ms_ = now_ms ? now_ms : 1u; return 0; }
        return now_ms - combat_victim_latch_since_ms_;
    }
    void       combat_victim_latch_reset()
    { combat_victim_latch_ = ObjectGuid::Empty; combat_victim_latch_since_ms_ = 0; }

    // ---- untankable-disengage probe cadence (final-review fix, 2026-07-03) ----
    // dungeon:untankable_disengage's DungeonTankDetourExcessiveVerbose call is a
    // full pathfind from the tank; once sustain>6s it was re-issued EVERY tick
    // per non-tank bot until the verdict flipped (no cadence, unlike the other
    // dungeon detour rules). Plain last-probe timestamp, touched on every probe.
    uint32     untankable_probe_last_ms() const { return untankable_probe_last_ms_; }
    void       touch_untankable_probe(uint32 now_ms) { untankable_probe_last_ms_ = now_ms; }

    // ---- combat:opener stall tracking ----
    // The opener runs the APL on an out-of-combat victim. Two failure
    // modes need server-truth feedback rather than range guesses (the
    // opener cannot know each APL rule's range — melee vs 40y caster):
    //   * cast_oor_count: incremented by the intent executor whenever a
    //     CastSpell against this bot's selection returns OutOfRange / LoS;
    //     reset on a successful cast or an approach step. >0 means "the
    //     server says we can't reach from here" → step toward the victim.
    //   * opener_victim_since: when the opener first saw this victim.
    //     Stalled past the limit without entering combat → give up and
    //     shield the target for minutes.
    ObjectGuid opener_victim() const { return opener_victim_; }
    uint32     opener_victim_since_ms() const { return opener_victim_since_ms_; }
    void       set_opener_victim(ObjectGuid g, uint32 ms)
    { opener_victim_ = g; opener_victim_since_ms_ = ms; cast_oor_count_ = 0; }
    // Last tick the opener actually evaluated this victim. A gap means the
    // bot was in combat / casting / had no victim in between — the stall
    // window must restart, otherwise re-opening on the same GUID minutes
    // later trips the stale since-timestamp and gives up instantly.
    uint32     opener_last_seen_ms() const { return opener_last_seen_ms_; }
    void       set_opener_last_seen(uint32 ms) { opener_last_seen_ms_ = ms; }
    // Absolute opener ownership clock (2026-07-21): the FIRST tick the opener
    // took for this bot in an unbroken out-of-combat run, INDEPENDENT of the
    // victim GUID. Immune to the set_opener_victim re-stamp that defeats the
    // per-victim give-up when the selection flips (live [opener_own]: fresh=1
    // since=0ms forever). Cleared the moment the bot is genuinely in combat
    // (see reset in BotAI's combat path). 0 = not currently owning.
    uint32     opener_own_since_ms() const { return opener_own_since_ms_; }
    void       set_opener_own_since(uint32 ms) { opener_own_since_ms_ = ms; }
    void       reset_opener_own_since() { opener_own_since_ms_ = 0; }
    // Whether the opener's current victim is a dungeon boss (recorded while it is
    // still visible in the snapshot). Used so that when a VANISHING boss (e.g.
    // Admiral Ripsnarl's stealth/fog) drops out of the snapshot, the lost-victim
    // handler uses a SHORT re-engage window instead of the 30s trash blacklist —
    // the tank must re-grab the boss the instant he reappears, not idle/drift off
    // the deck for 30s while the group fragments.
    bool       opener_victim_is_boss() const { return opener_victim_is_boss_; }
    void       set_opener_victim_is_boss(bool b) { opener_victim_is_boss_ = b; }
    uint32     cast_oor_count() const { return cast_oor_count_; }
    void       note_cast_out_of_range() { ++cast_oor_count_; }
    void       reset_cast_oor() { cast_oor_count_ = 0; }

    // Per-objective stuck-detection bookkeeping. Tracks (quest_id, obj_id) of
    // the active objective and how many ticks it's been pinned without
    // progress. State_Idle's quest-execution rules increment via note_obj_tick
    // and reset via note_obj_progress. After kStuckObjLimit ticks without
    // progress, the rule can blacklist this objective and the Builder will
    // pick a different one next snapshot. Stops bots from infinitely
    // pursuing unreachable mobs / inaccessible POIs.
    //
    // `nopath_strikes` is a SEPARATE, much faster blacklist track for the
    // case where the objective is genuinely UNREACHABLE — the bot's POI walk
    // returns NoPath / FarFromPolyEnd every tick (move_to → Result::Locked,
    // path_blocked_count growing). The slow `stuck_ticks` limit (~5 min) is
    // for "reachable but slow" objectives; a literally-unpathable objective
    // (Uraimus q483 den, 580y across a NoPath gap) should be abandoned in
    // seconds so the Builder's picker swaps to a reachable quest instead of
    // the bot looping path_fail → no-progress → teleport-rescue. Strikes
    // accumulate only while the objective walk is actively NoPath-blocked and
    // reset the moment a move succeeds (path_blocked_count back to 0) or the
    // objective changes / progresses.
    struct ObjectiveTrack { uint32 quest_id; uint32 obj_id; int32 last_progress; uint32 stuck_ticks; uint32 blacklisted_until_ms; uint32 nopath_strikes; uint32 scan_miss_strikes; };
    ObjectiveTrack const& objective_track() const { return objective_track_; }
    // `scan_miss` (CombatLoop FIX D): the bot's target SELECTION produced a
    // neutral_scan_miss this tick — the objective's target entry is not present
    // (or not valid/in-range) in the snapshot's scan radius. This is a SEPARATE
    // fast-blacklist track from `walk_nopath`: it covers phased / scripted
    // (D-class) objectives whose target never materializes for the bot, where
    // the POI walk SUCCEEDS (so walk_nopath stays false and the slow stuck_ticks
    // limit ~5 min governs). ~150 strikes ≈ 30s @ 5 Hz — fast enough to recover
    // before the 180s WedgeWatchdog, slow enough to ride out a slow respawn.
    // MUST reset the moment a valid target IS in range (scan_miss=false) so a
    // doable-but-slow-respawn quest survives indefinitely.
    void       note_obj_observed(uint32 quest_id, uint32 obj_id, int32 progress, uint32 now_ms,
                                 bool walk_nopath = false, bool scan_miss = false)
    {
        if (objective_track_.quest_id != quest_id || objective_track_.obj_id != obj_id)
        {
            // Active objective changed — fresh tracking for the new one.
            // (Blacklists live in the MULTI-SLOT map and survive this swap;
            // the old single-slot design erased objective A's blacklist the
            // moment objective B was tracked, livelocking bots that had 2+
            // stuck objectives — audit C03/C18.)
            objective_track_ = ObjectiveTrack{quest_id, obj_id, progress, 0, 0, 0, 0};
            return;
        }
        if (progress > objective_track_.last_progress)
        {
            // Real progress made — reset stuck counter.
            objective_track_.last_progress = progress;
            objective_track_.stuck_ticks   = 0;
            objective_track_.nopath_strikes = 0;
            objective_track_.scan_miss_strikes = 0;
            return;
        }
        // Fast scan-MISS track (FIX D). A valid in-range target this tick
        // (scan_miss == false) immediately clears the streak — a slow-respawn
        // but doable objective never accrues toward the blacklist. A run of
        // misses blacklists the objective so the Builder picks another.
        if (scan_miss)
        {
            ++objective_track_.scan_miss_strikes;
            constexpr uint32 kScanMissStrikeLimit = 150u;   // ~30s @ 5/s, below 180s WedgeWatchdog
            constexpr uint32 kScanMissBlacklistMs = 5u * 60u * 1000u;
            if (objective_track_.scan_miss_strikes >= kScanMissStrikeLimit)
                blacklist_put(quest_id, obj_id, now_ms + kScanMissBlacklistMs);
        }
        else
        {
            objective_track_.scan_miss_strikes = 0;
        }
        // Fast NoPath track. When the objective walk is currently path-blocked
        // (the caller passes the live "last action was the objective walk AND
        // we're in a consecutive-block streak" signal), count consecutive
        // NoPath observations. A handful of ticks at the start of a long walk
        // can NoPath transiently (tile still streaming, off-mesh source the
        // FarFromPolyStart snap fixes next tick), so require a run of strikes
        // before committing to the blacklist. ~40 strikes ≈ 8s at 5 Hz — fast
        // enough to abandon before any teleport-rescue fires, slow enough to
        // ride out a transient navmesh fluke. A single non-NoPath observation
        // (move succeeded, or the bot is doing something other than the
        // objective walk) clears the streak so reachable-but-slow objectives
        // never hit this track.
        constexpr uint32 kBlacklistMs = 5u * 60u * 1000u;
        if (walk_nopath)
        {
            ++objective_track_.nopath_strikes;
            constexpr uint32 kNoPathStrikeLimit = 30u;   // ~6s @ 5/s
            if (objective_track_.nopath_strikes >= kNoPathStrikeLimit)
                blacklist_put(quest_id, obj_id, now_ms + kBlacklistMs);
        }
        else
        {
            objective_track_.nopath_strikes = 0;
        }
        ++objective_track_.stuck_ticks;
        constexpr uint32 kStuckObjLimit = 600u;       // ~5 min @ 5/s
        if (objective_track_.stuck_ticks >= kStuckObjLimit)
            blacklist_put(quest_id, obj_id, now_ms + kBlacklistMs);   // blacklist_put locks quest_mem_mtx_
    }
    // Immediately blacklist a specific objective (5 min) when a rule KNOWS it
    // is unreachable right now — idle:quest_path wedged (check_anchor_wedge) AND
    // the travel graph found no route. The slow stuck_ticks / NoPath-strike
    // tracks in note_obj_observed don't fire reliably from the wedge branch: it
    // does NOT re-emit the idle:quest_path move_to that sets last_rule_fired, so
    // walk_nopath reads false there and the strike streak never reaches its
    // limit (observed: L4 "Somi" looping 400× NoPath in Orgrimmar — SnapToGround
    // grabbed an upper WMO platform Z the ground-floor poly can't reach — without
    // the objective ever blacklisting). Forcing it here guarantees the Builder's
    // picker swaps to a reachable objective, and once same-map work is exhausted
    // the cross-map breadcrumb surfaces so flight/zeppelin travel engages.
    void       blacklist_objective_now(uint32 quest_id, uint32 obj_id, uint32 now_ms)
    {
        constexpr uint32 kBlacklistMs = 5u * 60u * 1000u;
        blacklist_put(quest_id, obj_id, now_ms + kBlacklistMs);
    }
    bool       objective_blacklisted(uint32 quest_id, uint32 obj_id, uint32 now_ms) const
    {
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        auto it = objective_blacklist_.find(BlacklistKey(quest_id, obj_id));
        return it != objective_blacklist_.end() && now_ms < it->second;
    }
    // True when the currently-tracked objective is blacklisted right now.
    // Lets the global stuck-rescue defer the teleport while the
    // fast NoPath blacklist (note_obj_observed) is already steering the bot off
    // an unreachable objective onto a reachable one — per the no-teleport-rescue
    // principle, an unreachable OBJECTIVE must resolve by blacklist+reroute, not
    // by a homebind/capital yank.
    bool       current_objective_blacklisted(uint32 now_ms) const
    {
        return objective_blacklisted(objective_track_.quest_id,
                                     objective_track_.obj_id, now_ms);
    }

    // Lateral-reroute side commitment (idle:quest_walk_lateral_reroute). When a
    // cluster of 3+ threats blocks the path corridor, the rule sidesteps ~20y
    // perpendicular and picks the side (left/right) with fewer threats. Because
    // it recomputes the pick FRESH each tick from the bot's shifted position,
    // ties / near-ties flip the chosen side every tick (L→R→L…), each flip is a
    // >3y new destination → spline reset → the bot barely moves and never
    // clears the cluster (verified Irothoth flip-flop). To stop that, once a
    // side is chosen we COMMIT it: store the side + a commit timestamp and keep
    // using it for a short hold window, only switching if the OTHER side is
    // meaningfully better (>=2 fewer threats), never on a tie. -1 = none.
    int8   lateral_side() const { return lateral_side_; }
    uint32 lateral_side_ms() const { return lateral_side_ms_; }
    // side: -1=left, +1=right (mirrors the sign convention used by the rule's
    // perpendicular offset). Stamps the commit time so the hold/give-up windows
    // can be measured against it.
    void   commit_lateral_side(int8 side, uint32 now_ms)
    {
        lateral_side_    = side;
        lateral_side_ms_ = now_ms;
    }
    void   clear_lateral_side()
    {
        lateral_side_    = 0;
        lateral_side_ms_ = 0;
    }

    // Quest blacklist. When the bot abandons a quest (because the unachievable
    // filter flips, or because owner-driven cleanup explicitly drops one),
    // record (quest_id → expiry_ms) so the snapshot's offer-scan skips that
    // quest until the blacklist expires. Without this, the bot abandon→re-
    // accept→idle→abandon loop chews ticks forever on stuck quests. Persists
    // in-memory only (per QUEST_SYSTEM_PLAN.md: in-memory is sufficient for
    // overnight fleet runs; durability across server restart is not required).
    // Default cooldown is 60 minutes; longer than the typical questing trip
    // through a hub so the bot never re-encounters a recently-dropped offer.
    static constexpr uint32 kQuestBlacklistMs = 60u * 60u * 1000u;
    void blacklist_quest(uint32 quest_id, uint32 now_ms)
    {
        if (quest_id == 0) return;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        quest_blacklist_until_ms_[quest_id] = now_ms + kQuestBlacklistMs;
    }
    bool quest_blacklisted(uint32 quest_id, uint32 now_ms) const
    {
        if (quest_id == 0) return false;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        auto const it = quest_blacklist_until_ms_.find(quest_id);
        if (it == quest_blacklist_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    // Drop expired entries. Called periodically from State_Idle (every ~5min)
    // to keep the map bounded for long-lived bots — a session that cycles
    // through 100+ stuck quests would otherwise grow indefinitely.
    void prune_quest_blacklist(uint32 now_ms)
    {
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        for (auto it = quest_blacklist_until_ms_.begin(); it != quest_blacklist_until_ms_.end();)
        {
            if (now_ms >= it->second) it = quest_blacklist_until_ms_.erase(it);
            else                       ++it;
        }
        // Also prune the soft "CanTakeQuest recently failed" cache.
        for (auto it = cant_take_quest_until_ms_.begin(); it != cant_take_quest_until_ms_.end();)
        {
            if (now_ms >= it->second) it = cant_take_quest_until_ms_.erase(it);
            else                       ++it;
        }
        last_blacklist_prune_ms_ = now_ms;
    }
    uint32 last_blacklist_prune_ms() const { return last_blacklist_prune_ms_; }

    // Soft-fail cache for CanTakeQuest. The snapshot builder iterates every
    // nearby quest-giver's quest relations 5×/sec; for each non-NONE quest
    // not in the bot's log, it calls CanTakeQuest, which walks the
    // condition system end-to-end (every prereq, level, faction, etc).
    // When the bot stands next to a giver offering a quest the bot can
    // never take (level too low / prereq incomplete / faction wrong),
    // that's thousands of redundant condition evaluations per minute and
    // a flood of TC_LOG_DEBUG("condition", ...) messages in Server.log.
    //
    // We cache "CanTakeQuest just returned false" for a short window. The
    // conditions that make it fail rarely change tick-to-tick (level only
    // changes on ding, prereq only changes on quest turn-in, faction only
    // changes on rep-up — and all those events trigger a different state
    // path that naturally invalidates). A 30s cooldown is plenty.
    static constexpr uint32 kCantTakeQuestMs = 30u * 1000u;
    // R8: prerequisite-gated quests (why=prev_quest / dep_prev) can ONLY become
    // takeable when the bot completes the prerequisite — a quest-log change,
    // which flushes this whole cache (clear_cant_take_quest_cache below). So a
    // prereq miss is effectively immutable until that flush. Re-offering it
    // every 30s was pure waste: 8,268 prev_quest refusals from only 516 distinct
    // (bot,quest) pairs = the same gated quest re-logged ~100x/session. A long
    // TTL collapses the loop; the quest-log-change flush keeps it correct (the
    // moment the prereq is done, the gated quest re-evaluates immediately).
    static constexpr uint32 kCantTakeQuestPrereqMs = 600u * 1000u;   // 10 min
    void note_cant_take_quest(uint32 quest_id, uint32 now_ms)
    {
        if (quest_id == 0) return;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        cant_take_quest_until_ms_[quest_id] = now_ms + kCantTakeQuestMs;
    }
    // TTL-explicit overload: callers that know the refusal is a prerequisite
    // miss pass kCantTakeQuestPrereqMs to suppress the re-offer loop.
    void note_cant_take_quest(uint32 quest_id, uint32 now_ms, uint32 ttl_ms)
    {
        if (quest_id == 0) return;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        cant_take_quest_until_ms_[quest_id] = now_ms + ttl_ms;
    }
    bool cant_take_quest_recent(uint32 quest_id, uint32 now_ms) const
    {
        if (quest_id == 0) return false;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        auto const it = cant_take_quest_until_ms_.find(quest_id);
        if (it == cant_take_quest_until_ms_.end()) return false;
        return now_ms < it->second;
    }

    // Flush the entire CanTakeQuest soft-fail cache. Called from the
    // snapshot builder when the quest log signature changes — a quest
    // accept / completion / abandon mutates the predicates that drove
    // the cached "false" verdict (e.g. SatisfyQuestPreviousQuest flips
    // the moment the prereq is rewarded). Without this, a follow-up
    // quest from the same NPC (Tarindrella → q 28726 after q 28725
    // turn-in) stays "can't take" for the full 30s TTL even after the
    // server records the reward, and the bot wanders away before the
    // cache expires.
    void clear_cant_take_quest_cache()
    {
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        cant_take_quest_until_ms_.clear();
        // Also clear per-NPC "no offers" cache for the same reason: a
        // newly-eligible follow-up means this giver DOES have a
        // takeable offer now, contradicting the cached "no offers".
        giver_no_offers_until_ms_.clear();
    }

    // Quest log signature used by the snapshot builder to detect
    // mutations (accept / complete / abandon) without comparing the
    // whole quest list. Cheap XOR-with-state of quest_ids; collisions
    // would just mean a missed cache flush (correctness intact, perf
    // optimisation lost for that one mutation).
    uint64 last_quest_log_signature() const { return last_quest_log_signature_; }
    void   set_last_quest_log_signature(uint64 v) { last_quest_log_signature_ = v; }

    // Per-NPC "no takeable offer" cache. When offers_from() walks every
    // quest an NPC offers and CanTakeQuest fails for ALL of them, mark
    // that NPC entry so the next snapshot skips the entire NPC instead
    // of re-walking the quest list. Observed: Somi L1 hit 1802 refusals
    // in 39 min (~46/min) because each starter NPC offers 5-10 chain
    // quests gated behind prev_quest, and the per-quest 30s cache only
    // suppresses re-logging, not re-iteration of the relation list.
    // 60s TTL — long enough to break the loop, short enough that a
    // dinged level or completed prereq is picked up quickly.
    static constexpr uint32 kGiverNoOffersMs = 60u * 1000u;
    void note_giver_no_offers(uint32 entry, uint32 now_ms)
    {
        if (entry == 0) return;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        giver_no_offers_until_ms_[entry] = now_ms + kGiverNoOffersMs;
    }
    bool giver_no_offers_recent(uint32 entry, uint32 now_ms) const
    {
        if (entry == 0) return false;
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        auto const it = giver_no_offers_until_ms_.find(entry);
        if (it == giver_no_offers_until_ms_.end()) return false;
        return now_ms < it->second;
    }

    // PERMANENT per-quest impossibility cache, for gates that can NEVER change
    // for this bot: class and race. The transient cant_take_quest cache (30s)
    // is flushed on every quest-log mutation, so a questing bot re-runs
    // CanTakeQuest on class/race-locked quests at every NPC, every snapshot
    // (~50k [quest_offer_refused] why=class/race in a log window = pure CPU +
    // log waste). A bot's class/race are immutable for its lifetime, so once a
    // quest is refused for either, it stays refused forever — record it here
    // and skip it before the expensive CanTakeQuest. Deliberately NOT cleared
    // by clear_cant_take_quest_cache() (that flush is only for the mutable
    // prev_quest/level/skill reasons).
    void note_quest_impossible(uint32 quest_id)
    {
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        if (quest_id != 0) quest_impossible_[quest_id] = 1;
    }
    bool quest_impossible(uint32 quest_id) const
    {
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        return quest_id != 0 && quest_impossible_.find(quest_id) != quest_impossible_.end();
    }

    // Per-quest reward-turnin failure backoff. API::complete_quest returns
    // Result::Locked when Player::CanRewardQuest fails (cant_reward_pre /
    // cant_reward_post). Two distinct causes hide behind that:
    //   * PERMANENT — the quest's reward item is missing from client data
    //     or otherwise un-storable forever (observed: quest 26712 "Off to
    //     the Bank" rewards 10x item 47044, which doesn't exist on this
    //     core; 120k turn-in retries in a 300MB log window because the
    //     quest stays QUEST_STATUS_COMPLETE and idle:quest_turnin re-fires
    //     every tick with no give-up path).
    //   * TRANSIENT — bags are full; freeing a slot (a vendor trip) lets
    //     the same turn-in succeed.
    // An escalating TTL serves both: a short first back-off recovers fast
    // after a vendor run, while repeated failures grow the suppression to
    // a long cap so a genuinely-broken quest stops burning CPU + log I/O.
    static constexpr uint32 kQuestRewardFailBaseMs = 30u * 1000u;        // first miss
    static constexpr uint32 kQuestRewardFailCapMs  = 15u * 60u * 1000u;  // hard ceiling
    void note_quest_reward_failed(uint32 quest_id, uint32 now_ms)
    {
        if (quest_id == 0) return;
        QuestRewardFail& f = quest_reward_fail_[quest_id];
        if (f.fails < 250) ++f.fails;                 // saturate, never wrap
        // 30s, 60s, 120s, 240s ... capped at 15 min.
        uint64 ttl = uint64(kQuestRewardFailBaseMs) << (f.fails - 1 < 9 ? f.fails - 1 : 9);
        if (ttl > kQuestRewardFailCapMs) ttl = kQuestRewardFailCapMs;
        f.until_ms = now_ms + uint32(ttl);
    }
    bool quest_reward_failed_recent(uint32 quest_id, uint32 now_ms) const
    {
        if (quest_id == 0) return false;
        auto const it = quest_reward_fail_.find(quest_id);
        if (it == quest_reward_fail_.end()) return false;
        return now_ms < it->second.until_ms;
    }

    // Last snapshot tick that observed s.is_moving() == true. Used by
    // idle:ambient_sit to require 10s of continuous standstill before
    // sitting, instead of dropping to sit on every momentary pause
    // between path segments / mob kills / snapshot ticks.
    uint32 last_moving_ms() const { return last_moving_ms_; }
    void   set_last_moving_ms(uint32 ms) { last_moving_ms_ = ms; }

    // Throttle stamp for the `[quest_no_offers]` diagnostic in
    // BotSnapshotBuilder — we want one log line per bot per ~5s, not
    // one per snapshot tick. Read/written from the builder thread.
    uint32 last_quest_diag_ms() const { return last_quest_diag_ms_; }
    void   set_last_quest_diag_ms(uint32 ms) { last_quest_diag_ms_ = ms; }

    // Throttle stamp for the `[wander_reason]` diagnostic in
    // State_Idle. One log line per bot per 60s — wander fires 1000+/min
    // fleet-wide so anything tighter would drown the log.
    uint32 last_wander_diag_ms() const { return last_wander_diag_ms_; }
    void   set_last_wander_diag_ms(uint32 ms) { last_wander_diag_ms_ = ms; }

    // Per-reagent retry cooldown. The reagent-buy rule emits
    // VendorBuyByEntryIntent against the nearest vendor; if the vendor
    // doesn't carry the item, the intent fails server-side. Without a
    // cooldown the rule would re-emit the same buy every tick. Track only
    // the most recently attempted (entry, ms) — sufficient because the
    // rule fires one buy per tick and walks recipes in order; cycling
    // through different reagents means the cooldown rotates naturally.
    uint32 last_reagent_try_entry() const { return last_reagent_try_entry_; }
    uint32 last_reagent_try_ms()    const { return last_reagent_try_ms_; }
    void   note_reagent_try(uint32 entry, uint32 ms)
    { last_reagent_try_entry_ = entry; last_reagent_try_ms_ = ms; }

    // Per-item equip retry cooldown. The idle:equip_upgrade rule walks
    // bag_items and emits EquipItemIntent for the first one whose stat
    // score beats the equipped slot's score by the margin. The intent
    // path goes through Player::CanEquipItem / EquipItem, which can fail
    // silently for many reasons (level required > bot level, wrong armor
    // proficiency, off-hand for weapon-only spec, item bound to another
    // character, etc). When equip fails the item stays in the bag and
    // scores the same upgrade next snapshot — the rule re-emits forever
    // and never lets dispatch reach the movement rules below it. Result:
    // 255+ bots welded to the PlaceInCapital teleport spot in Stormwind /
    // Orgrimmar with `LastRule = idle:equip_upgrade` and
    // `Upgrades: 2 pending` that never decrements.
    //
    // Cache (item_entry → until_ms) after every emit. The rule skips items
    // whose entry is in the cache; on the next tick it walks past the
    // un-equippable item and fires for the next candidate, OR (if no candidate
    // remains) falls through to the movement rules.
    // 2026-06-17: raised 30s → 240s so the lockout OUTLASTS the 3-min periodic
    // re-arm (kPeriodicEquipMs, State_Idle.cpp). Previously a bot holding only
    // server-REFUSED un-equippable "upgrades" (upgrades_pending counts them with
    // no can-equip filter) re-fired auto_equip@690 every 3 min — a burst that
    // perpetually STARVED the quest-seek rules below it (walk_to_known_hub@615),
    // leaving the bot frozen + questless. With the lockout > periodic interval,
    // once the rule has tried each refused entry it stays quiet (returns false)
    // and the seek rules get the tick. Successful equips leave the bag (never
    // re-counted); a post-ding now-equippable item re-qualifies within one
    // lockout window. Off the snapshot-Build thread (freeze-safe).
    static constexpr uint32 kEquipTryLockoutMs = 240u * 1000u;
    void note_equip_try(uint32 item_entry, uint32 now_ms)
    {
        if (item_entry == 0) return;
        equip_try_until_ms_[item_entry] = now_ms + kEquipTryLockoutMs;
    }
    bool equip_recently_tried(uint32 item_entry, uint32 now_ms) const
    {
        if (item_entry == 0) return false;
        auto const it = equip_try_until_ms_.find(item_entry);
        if (it == equip_try_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    void prune_equip_try_cache(uint32 now_ms)
    {
        for (auto it = equip_try_until_ms_.begin(); it != equip_try_until_ms_.end();)
        {
            if (now_ms >= it->second) it = equip_try_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // Per-bot quest-hub "tried but unproductive" cache. The
    // idle:travel_to_hub rule walks the bot to the nearest level- and
    // faction-appropriate hub; on arrival (within 30y of centroid) the
    // wander_to_quest_hub rule's 40y giver scan is supposed to take over.
    // But Stormwind / Orgrimmar / other capital clusters have many quest
    // givers whose only quests are repeatable (Brewfest, Darkmoon, profession
    // dailies) — all filtered out of `quest_offers` by IsRepeatable. Result:
    // bot sits at the centroid, snapshot reports no offers / no turnins / no
    // resolvable POI, wander emits random 25y steps, drifts > 30y, idle:
    // travel_to_hub fires again, walks back to centroid — endless oscillation
    // with 0 productive movement.
    //
    // Solution: when the rule sees the bot is already within arrival radius
    // of the picked hub AND nothing_local is still true, mark this hub
    // unproductive for this bot. Picker iterates GetQuestHubsForBot(top-N)
    // and returns the first non-blacklisted one — typically the next-nearest
    // hub OUTSIDE the city, which is exactly where the bot needs to walk
    // to find takeable quests.
    static constexpr uint32 kHubTriedLockoutMs = 5u * 60u * 1000u;  // 5 min
    void note_hub_tried(uint32 hub_id, uint32 now_ms)
    {
        if (hub_id == 0) return;
        tried_hub_until_ms_[hub_id] = now_ms + kHubTriedLockoutMs;
    }
    bool hub_recently_tried(uint32 hub_id, uint32 now_ms) const
    {
        if (hub_id == 0) return false;
        auto const it = tried_hub_until_ms_.find(hub_id);
        if (it == tried_hub_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    void prune_hub_tried_cache(uint32 now_ms)
    {
        for (auto it = tried_hub_until_ms_.begin(); it != tried_hub_until_ms_.end();)
        {
            if (now_ms >= it->second) it = tried_hub_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // ---- A4: per-corpse loot deadline ----
    // The loot drain can wedge: the corpse re-presents "pending" loot every tick
    // (un-taken gold, a quest item the bot no longer needs, or an item that won't
    // fit), so Phase-1 LootIntent re-fires forever and the Phase-2 skin is never
    // reached (idle:loot_corpse was the #1 Watchdog hot-loop — one bot+corpse
    // pair repeated 12,465x in the 4-day run). After kLootCorpseDeadlineMs of
    // continuous attempts on a corpse, give up and pop it. Records first-seen
    // per corpse; bounded + auto-clears.
    static constexpr uint32 kLootCorpseDeadlineMs = 12000;   // 12 s
    [[nodiscard]] bool loot_corpse_overdue(uint64 corpse_low, uint32 now_ms)
    {
        auto it = loot_first_seen_.find(corpse_low);
        if (it == loot_first_seen_.end())
        {
            if (loot_first_seen_.size() >= 32) loot_first_seen_.clear();
            loot_first_seen_[corpse_low] = now_ms;
            return false;
        }
        return (now_ms - it->second) >= kLootCorpseDeadlineMs;
    }
    void clear_loot_corpse_seen(uint64 corpse_low) { loot_first_seen_.erase(corpse_low); }

    // Per-corpse WALK deadline. The loot deadline above only starts once the bot
    // is IN loot range; a corpse the bot can never REACH (off-mesh, across a gap,
    // on a ledge) never trips it, so idle:move_to_corpse loops forever walking
    // toward it (live: Wylius wedged 273s). Bounds the walk: if the bot can't get
    // to the corpse within kCorpseWalkDeadlineMs, give up and try the next queued
    // corpse. Generous so a legit long walk to a reachable corpse isn't cut short.
    static constexpr uint32 kCorpseWalkDeadlineMs = 40000;   // 40 s to REACH it
    [[nodiscard]] bool loot_corpse_walk_overdue(uint64 corpse_low, uint32 now_ms)
    {
        auto it = loot_walk_first_ms_.find(corpse_low);
        if (it == loot_walk_first_ms_.end())
        {
            if (loot_walk_first_ms_.size() >= 32) loot_walk_first_ms_.clear();
            loot_walk_first_ms_[corpse_low] = now_ms;
            return false;
        }
        return (now_ms - it->second) >= kCorpseWalkDeadlineMs;
    }
    void clear_loot_corpse_walk(uint64 corpse_low) { loot_walk_first_ms_.erase(corpse_low); }

    // Per-(vendor NPC, item subclass) buy retry cooldown. The auto-restock
    // rules (idle:buy_food / idle:buy_bandage / idle:buy_potion) emit
    // VendorBuyCategoryIntent against the nearest vendor; if the vendor
    // doesn't stock that subclass the buy returns 0 items but the bot's
    // {food,bandage,potion}_count snapshot field stays unchanged. Without
    // a cooldown the rule re-fires every tick and dispatch never reaches
    // the movement rules below — same wedge pattern as the equip-upgrade
    // bug. Bandages especially: First Aid was removed in BfA 8.0, so most
    // modern vendors don't carry them at all and the rule will NEVER
    // succeed. Cache (npc_low,subclass)->until_ms for 5 minutes.
    //
    // Key packs npc_guid_low (uint64) and subclass (uint8) into uint64.
    static constexpr uint32 kVendorBuyTryLockoutMs = 5u * 60u * 1000u;
    static uint64 vendor_buy_key(uint64 npc_low, uint8 subclass)
    { return (npc_low << 8) | uint64(subclass); }
    void note_vendor_buy_try(uint64 npc_low, uint8 subclass, uint32 now_ms)
    {
        if (npc_low == 0) return;
        vendor_buy_until_ms_[vendor_buy_key(npc_low, subclass)] =
            now_ms + kVendorBuyTryLockoutMs;
    }
    bool vendor_buy_recently_tried(uint64 npc_low, uint8 subclass, uint32 now_ms) const
    {
        if (npc_low == 0) return false;
        auto const it = vendor_buy_until_ms_.find(vendor_buy_key(npc_low, subclass));
        if (it == vendor_buy_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    void prune_vendor_buy_cache(uint32 now_ms)
    {
        for (auto it = vendor_buy_until_ms_.begin(); it != vendor_buy_until_ms_.end();)
        {
            if (now_ms >= it->second) it = vendor_buy_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // Per-GO chest loot retry cooldown. The idle:loot_chest rule emits
    // UseGameObjectIntent against the nearest type=3 (CHEST) GO when the
    // bot has bag space. Many event/atmospheric chests yield no actual
    // loot (Brewfest "Fest Goblet" — gameobject_loot_template is empty;
    // BfA personal-loot crates with missing loot rows; quest-restricted
    // chests the bot doesn't qualify for). The bot's bag stays empty, so
    // bag_free_slots stays > 0 and the chest stays the nearest type-3
    // object. Result: rule re-emits every tick, dispatch never reaches
    // movement rules, bot is welded to the chest.
    //
    // Cache (chest_guid_low → until_ms) for 5 minutes. After one failed
    // attempt the rule skips that specific chest and falls through to
    // the next rule (typically wander, which moves the bot away so the
    // snapshot's nearest_object picks a different chest or none).
    static constexpr uint32 kChestLootTryLockoutMs = 5u * 60u * 1000u;
    void note_chest_loot_try(uint64 go_guid_low, uint32 now_ms)
    {
        if (go_guid_low == 0) return;
        chest_loot_until_ms_[go_guid_low] = now_ms + kChestLootTryLockoutMs;
    }
    bool chest_loot_recently_tried(uint64 go_guid_low, uint32 now_ms) const
    {
        if (go_guid_low == 0) return false;
        auto const it = chest_loot_until_ms_.find(go_guid_low);
        if (it == chest_loot_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    void prune_chest_loot_cache(uint32 now_ms)
    {
        for (auto it = chest_loot_until_ms_.begin(); it != chest_loot_until_ms_.end();)
        {
            if (now_ms >= it->second) it = chest_loot_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // Generic per-(action_kind, target_low) retry cooldown. Used by rules
    // whose underlying intent can fail silently — same wedge pattern as
    // equip_upgrade / loot_chest / vendor_buy: rule fires every tick, the
    // intent does nothing (locked node, banker rejects, no trash items in
    // bag, etc.), the snapshot field that gates the rule doesn't change,
    // and dispatch never reaches movement rules below.
    //
    // ActionKind values:
    //   0 = idle:gather           target = node GO guid_low
    //   1 = idle:vendor_sell_trash target = vendor NPC guid_low
    //   2 = idle:bank_deposit     target = banker NPC guid_low
    enum class ActionKind : uint8 {
        Gather          = 0,   // target = node GO guid_low
        SellTrash       = 1,   // target = vendor NPC guid_low
        BankDeposit     = 2,   // target = banker NPC guid_low
        QuestStartItem  = 3,   // target = item template entry
        QuestUseGo      = 4,   // target = GO guid_low (quest collect / interact)
        WanderToNode    = 5,   // target = node GO guid_low (walk-toward dedup)
        ReagentBuy      = 6,   // target = reagent item entry
        BgUseGo         = 7,   // target = BG flag/objective GO guid_low
        BgEnterVehicle  = 8,   // target = vehicle creature guid_low
        OocPotion       = 9,   // target = potion item entry
        OocBandage      = 10,  // target = bandage item entry
        CookSelf        = 11,  // target = recipe spell id
        BgPort          = 12,  // target = bg_type_id
        MailDrain       = 13,  // target = packed (mailbox_low<<24 | message_id_low)
        Repair          = 14,  // target = repair NPC guid_low
        AhPostItem      = 15,  // target = item entry id
        InviteOther     = 16,  // target = invitee guid_low
        MatShare        = 17,  // target = packed (recipient_low ^ item_entry)
        GuildBankDep    = 18,  // target = banker GO guid_low
        Smelt           = 19,  // target = smelt spell id
        PetSwap         = 20,  // target = stablemaster guid_low
        DualSpec        = 21,  // target = trainer guid_low (or 0)
        GuildChat       = 22,  // target = topic id (1=ding, 2=greet, ...)
        LearnRecipe     = 23,  // target = recipe item entry
        AltHearth       = 24,  // target = alt-hearth item entry (Garrison/Dalaran etc)
        TameBeast       = 25,  // target = beast creature guid_low
        BgCallout       = 26,  // target = packed callout key (kind|node-entry)
        QueueFillRespec = 27,  // target = bot guid_low; short cooldown — filler
        OocFood         = 28,  // target = food/drink item entry; sit-and-eat
                               // between pulls — typical food channel 25s,
                               // we lock 60s so OOC + still-wounded doesn't
                               // re-fire mid-channel.
                               // 2nd-pass nudges hybrid DPS into tank/healer when
                               // the queue needs them. MUST be separate from
                               // DualSpec (30min) so frequent queue cycles don't
                               // exhaust the pool's hybrid bots in 30 min.
        PetRecall       = 29,  // target = bot guid_low; recall pet to owner
                               // when pet is low-HP + attackers unreachable.
                               // 10s effective cooldown (see override below).
        TrainerLearn    = 30,  // target = trainer NPC guid_low; one bulk-
                               // train attempt at this trainer failed-locks
                               // it for 15 min. Stops the multi-trainer
                               // ping-pong in capital quarters where
                               // nearest_npc_with_flag(UNIT_NPC_FLAG_TRAINER)
                               // rotates between adjacent NPCs (class +
                               // profession + secondary trainers clustered
                               // a few yards apart) — without per-guid
                               // backoff, a bot whose nearest trainer can't
                               // teach anything walks back-and-forth between
                               // them indefinitely.
        GreetPlayer     = 31,  // target = passer-by player guid_low. After a
                               // /wave at a nearby non-grouped real player
                               // (idle:react_to_passerby), suppress re-greeting
                               // the same player for a long window so bots don't
                               // spam-wave anyone standing next to them.
        BgTargetSwitch  = 32,  // target = new victim guid_low. Hysteresis for
                               // the in-combat PvP priority target switch
                               // (EFC > peel > enemy healer) -- without it the
                               // switch could flip targets every combat tick as
                               // candidates drift in and out of range. 8s
                               // effective cooldown (override below).
        BgMount         = 33,  // target = bg_type_id. Retry pacing for the BG
                               // mount-up rule. Previously piggybacked on
                               // PetSwap (5 min default) while the rule's
                               // comment claimed 60s -- and the lockout armed
                               // even on SUCCESSFUL mounts, so a bot auto-
                               // dismounted by combat walked between
                               // objectives for the next 5 minutes (BG audit
                               // N43). 25s effective cooldown (override).
        Flight          = 34,  // target = 0 (per-bot global). Proactive-taxi
                               // hysteresis: one idle:fly_to_taxi ride per
                               // 5-min window (default lockout). Without it,
                               // a goal that stays unreachable after landing
                               // (NoPath POI on a mesh island) re-triggered
                               // the "goal is far -> fly" logic on every
                               // arrival and the bot PING-PONGED between
                               // flight nodes forever (observed: Uraimus,
                               // Darnassus loops).
        BgOrderMove     = 35,  // target = quantized order destination (16y
                               // grid). Move re-emit pacing for the team-
                               // coordinator order executor: one move_to
                               // per 3s toward the SAME ordered target —
                               // re-arms instantly when the coordinator
                               // moves the target (different key). 3s
                               // lockout override below.
        PveTargetSwitch = 36,  // target = kill-focus guid_low. Hysteresis
                               // for the PvE-coordinator synchronized
                               // add-burn switch — same rationale as
                               // BgTargetSwitch. 8s override below.
        RaidIcon        = 37,  // target = marked unit guid_low. Retry pacing
                               // for ingroup:auto_skull — when the executor
                               // refuses the icon set (Locked: group state /
                               // invalid target), the leader re-emitted the
                               // SAME pick every tick (~6/sec, observed on
                               // the 2026-06-11 BG orphans). 10s override.
        BgOrphanEscape  = 38,  // target = 0 (per-bot). Retry pacing for
                               // idle:bg_orphan_escape (hearth cast ~10s /
                               // teleport latency). 15s override below.
        AmbientSit      = 39,  // target = 0 (per-bot). idle:ambient_sit used
                               // to re-emit SIT every tick once settled
                               // (~6 intents/sec PER IDLE BOT, top idle
                               // intent in /diag — Uraimus @ Dolanaar inn
                               // 2026-06-11, fleet-wide thousands/sec).
                               // One SIT per 60s window (override below).
        AhBuyout        = 40,  // target = auction_id. #4B buy-side dedup: one
                               // buyout emit per listing per window so the
                               // economy rule doesn't re-emit the SAME
                               // EconomyOp::AhBuyout every snapshot tick before
                               // the executor consumes it (would double-spend
                               // gold / race the snapshot rebuild). 30s
                               // override below — long enough for the buy to
                               // settle + the AH snapshot to refresh.
        AhBid           = 41,  // target = auction_id. Same dedup for bids;
                               // a bid that didn't win (outbid) re-evaluates
                               // on the next snapshot after the lockout.
        AhBuyCommodity  = 42,  // target = item_entry (NOT an auction_id —
                               // commodities aggregate many listings into one
                               // bucket, so the dedup key is the reagent entry,
                               // not a single auction). 30s window: one
                               // commodity buy per wanted reagent per visit,
                               // so the rule doesn't re-quote/re-buy the same
                               // reagent every snapshot tick before the buy
                               // settles + the AH snapshot refreshes.
        CraftOrderPost  = 43,  // target = product item_entry. #4B-2(a) part 2:
                               // one craft-order POST per wanted intermediate
                               // per window. The order takes minutes to be
                               // claimed+fulfilled and the want signal persists
                               // across snapshots, so without a long lockout the
                               // post rule would re-post the SAME want every tick
                               // before the board's craft_orders projection
                               // catches up (my_open_order_count). 2min override.
        CraftOrderClaim = 44,  // target = 0 (per-bot global). One claim attempt
                               // per window: ClaimOpenOrder is world-thread and
                               // the claimed order only surfaces in the NEXT
                               // snapshot's claimed_* fields, so a claim emitted
                               // this tick must not be re-emitted before the
                               // snapshot rebuilds — otherwise the bot could
                               // claim several orders in a burst. 30s override.
        BgVehicleFire   = 45,  // target = enemy gate GO guid_low. IoC/SoTA siege
                               // vehicle firing its seat weapon at a gate. SHORT
                               // override (~4s): a demolisher/siege engine rams a
                               // gate continuously — the server-side seat-spell
                               // cooldown is the real gate. The 5min default made
                               // each vehicle fire ONCE then sit (and fall through
                               // to its node order, driving off the gate), so the
                               // gate never breached (live: 1-2 vehicles reached
                               // the gate, fire_idle stuck ~2-5 over a whole match).
    };
    static constexpr uint32 kActionRetryLockoutMs = 5u * 60u * 1000u;
    // Per-kind cooldown override — most actions use the 5 min default, but
    // some (server-side item CDs) need longer to avoid retry spam during
    // the natural cooldown window.
    static uint32 action_retry_lockout_for(ActionKind k)
    {
        switch (k)
        {
            case ActionKind::BgVehicleFire:
                // Siege vehicle ramming a gate fires continuously; the real
                // gate is the server-side seat-spell cooldown. 4s just dedups
                // the cast intent without throttling the breach.
                return 4u * 1000u;
            case ActionKind::AltHearth:
                // Garrison Hearthstone is 15 min, Dalaran Hearthstone is
                // 30 min server-side; pick 30 min as the upper bound so
                // we don't re-emit during the longer item's CD.
                return 30u * 60u * 1000u;
            case ActionKind::DualSpec:
                // Spec switching is heavy (talents reset, gold cost at L80+,
                // pet despawn for Hunter/Warlock). Cooldown 30 min so a
                // refused/in-combat spec apply doesn't churn talent state
                // faster than once per half-hour.
                return 30u * 60u * 1000u;
            case ActionKind::LearnRecipe:
                // Recipe items often require a profession skill level the
                // bot won't reach for hours. The 5min default would retry
                // the same item ~12 times/hour for the bot's whole life.
                // 1 hour cadence keeps the retry useful while letting
                // skill-up loops catch up between attempts.
                return 60u * 60u * 1000u;
            case ActionKind::OocFood:
                // Food / drink channel: ~25s for full regen + recently-
                // ate buff so we don't re-emit mid-channel. Server-side
                // breaks the channel on damage so this is cosmetic.
                return 60u * 1000u;
            case ActionKind::BgCallout:
                // Chat callouts: 20s per callout-key keeps the room
                // readable. "inc <node>" stays useful through repeated
                // attacks; "FC down" is naturally one-shot per match.
                return 20u * 1000u;
            case ActionKind::PetRecall:
                // Pet recall is a single COMMAND_FOLLOW emit; pet pathing
                // back takes ~3-5s. 10s lockout avoids re-emit while
                // pet is still moving. Default 5min would block recall
                // from re-firing if pet re-engaged later in a new fight.
                return 10u * 1000u;
            case ActionKind::TrainerLearn:
                // After one bulk-train at a trainer, suppress re-attempt
                // at the same NPC guid for 15 min. Long enough to outlast
                // a typical wander-away-and-back cycle and to avoid the
                // adjacent-trainer ping-pong, short enough that a real
                // level-up gain visiting the trainer again works after a
                // reasonable play window.
                return 15u * 60u * 1000u;
            case ActionKind::BgMount:
                // Fast re-mount after combat dismounts; slow enough not to
                // spam Locked mount casts on no-mount maps (WSG tunnel).
                return 25u * 1000u;
            case ActionKind::BgTargetSwitch:
                // PvP focus-switch hysteresis: long enough to commit to a
                // kill target, short enough to re-evaluate each skirmish.
                return 8u * 1000u;
            case ActionKind::PveTargetSwitch:
                // Same hysteresis for the PvE synchronized add-burn.
                return 8u * 1000u;
            case ActionKind::BgOrderMove:
                // Coordinator-order movement: re-assert the move every 3s
                // while marching on the ordered target (combat knockbacks,
                // CC and local melee shuffles interrupt the path), without
                // spamming a move_to every AI tick.
                return 3u * 1000u;
            case ActionKind::RaidIcon:
                // One icon-set attempt per target per 10s — a refused set
                // (Locked) otherwise re-emits every tick.
                return 10u * 1000u;
            case ActionKind::BgOrphanEscape:
                // Hearth cast is 10s; teleport resolution within a tick.
                // 15s between escape attempts keeps the wedge-breaker
                // responsive without re-emitting mid-cast.
                return 15u * 1000u;
            case ActionKind::AmbientSit:
                // One sit attempt per minute of settled idling. If something
                // stands the bot up (vendor interaction, brief move), it
                // re-sits within the minute — human-paced, not per-tick.
                return 60u * 1000u;
            case ActionKind::GreetPlayer:
                // Greet a given passer-by at most once per 10 min. Long enough
                // that bots don't repeatedly wave at the same person loitering
                // nearby (which reads as creepy/robotic), short enough that a
                // genuinely new encounter later in a play session still gets a
                // friendly wave.
                return 10u * 60u * 1000u;
            case ActionKind::QueueFillRespec:
                // Queue-fill 2nd-pass respec: short lockout (60s) prevents
                // back-to-back same-bot churn within one queue cycle but
                // doesn't poison the pool. The same-spec idempotence guard
                // (`cur_spec == target_spec` skip in the filler) handles
                // the Monk Coalescence reapply-assertion path.
                return 60u * 1000u;
            case ActionKind::AhBuyout:
            case ActionKind::AhBid:
            case ActionKind::AhBuyCommodity:
                // Buy-side AH ops: 30s per auction_id. Long enough for the
                // server-side buyout/bid to commit and the next AH snapshot
                // (built on-demand at the auctioneer) to drop the consumed
                // listing, short enough that a still-available listing the
                // bot still wants re-fires within the same AH visit.
                return 30u * 1000u;
            case ActionKind::CraftOrderPost:
                // One post per wanted product per 2 min. The board's
                // my_open_order_count projection (next snapshot) is the real
                // anti-duplicate gate; this lockout just bounds the re-emit
                // until the projection updates, so the rule can't spam PostOrder
                // for the same persistent want before the row is reflected.
                return 2u * 60u * 1000u;
            case ActionKind::CraftOrderClaim:
                // One claim attempt per 30s — long enough for ClaimOpenOrder to
                // commit and the next snapshot (which surfaces claimed_*) to
                // rebuild, so the bot doesn't claim a burst of orders before its
                // first claim shows up as work to fulfil.
                return 30u * 1000u;
            case ActionKind::Repair:
                // Short 5s lockout, NOT the 5min default. RepairAll has no
                // success/fail feedback to the rule, and the first attempt is
                // often fired through a wall (nearest vendor by straight-line
                // distance has no line-of-sight) where the server's interact
                // check rejects it. A 5min lockout would then strand the bot at
                // 0% gear for 5 minutes after it walks around to a LoS spot.
                // 5s lets the retry land once the bot gains LoS; once durability
                // recovers the critical_repair gate stops firing entirely, so a
                // no-op RepairAll on already-full gear never spams.
                return 5u * 1000u;
            default:
                return kActionRetryLockoutMs;
        }
    }
    static uint64 action_retry_key(ActionKind k, uint64 target_low)
    { return (uint64(k) << 56) | (target_low & 0x00FFFFFFFFFFFFFFULL); }
    // Kinds whose pacing is PER-BOT by design (documented "target = 0"):
    // the zero-target guard below must not void their lockout. Without
    // this, the AmbientSit 60s pacing was dead on arrival — target 0 made
    // note/recently_tried no-ops, so settled bots emitted SIT every tick
    // (~5/s; re-observed on the user's selfbot 2026-06-12 after the
    // 06-11 "fix" shipped). BgOrphanEscape's 15s pacing had the same hole.
    static bool action_kind_allows_zero_target(ActionKind k)
    {
        switch (k)
        {
            case ActionKind::AmbientSit:
            case ActionKind::BgOrphanEscape:
            // #4B-2(a) part 2: the craft-order CLAIM path keys on target 0
            // (per-bot global "one claim attempt per window") — the board picks
            // the order, so there is no per-order key at claim time. Without
            // this the 30s claim throttle would silently no-op and a bot could
            // re-emit CraftClaim every fire-eligible tick.
            case ActionKind::CraftOrderClaim:
            // Dungeon SELF-repair (task #8) keys on target 0 — there is no NPC,
            // it's a per-bot global "one self-repair attempt per window". The
            // vendor repair path always passes a real npc target, so it is
            // unaffected by this; only the empty-npc dungeon self-repair throttles
            // on the zero key.
            case ActionKind::Repair:
                return true;
            default:
                return false;
        }
    }
    void note_action_retry(ActionKind k, uint64 target_low, uint32 now_ms)
    {
        if (target_low == 0 && !action_kind_allows_zero_target(k)) return;
        action_retry_until_ms_[action_retry_key(k, target_low)] =
            now_ms + action_retry_lockout_for(k);
    }
    bool action_recently_tried(ActionKind k, uint64 target_low, uint32 now_ms) const
    {
        if (target_low == 0 && !action_kind_allows_zero_target(k)) return false;
        auto const it = action_retry_until_ms_.find(action_retry_key(k, target_low));
        if (it == action_retry_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    void prune_action_retry_cache(uint32 now_ms)
    {
        for (auto it = action_retry_until_ms_.begin(); it != action_retry_until_ms_.end();)
        {
            if (now_ms >= it->second) it = action_retry_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // --- critical-repair futility backoff (VendorRules idle:critical_repair) ---
    // A bot too poor to fully repair would otherwise sit on top of the vendor
    // firing critical_repair forever (gear <=20% keeps the gate open) and never
    // resume questing to earn the money it needs. These track time-at-vendor and
    // durability progress so the rule can detect "no progress -> back off and go
    // earn", then retry repairs after a cooldown. See the rule for the policy.
    uint32 crit_repair_backoff_until() const { return crit_repair_backoff_until_ms_; }
    void   set_crit_repair_backoff(uint32 until_ms) { crit_repair_backoff_until_ms_ = until_ms; }

    // --- bag-recovery futility backoff (VendorRules idle:bags_full_recover) ---
    // A bot whose bags are full of UNSELLABLE items (quest items, kept gear,
    // reagents) can't free space at a vendor, so the dedicated full-bags vendor
    // trip would preempt questing forever (loop at the vendor, or oscillate while
    // walking to a far one) and the bot never levels. A real player keeps playing
    // with full bags (turn-ins free quest-item slots; bank/AH runs handle the rest).
    // These track free-slot progress while bags are full: if no slot frees within
    // the futility window, back off so questing resumes, then retry later.
    uint32 bag_recovery_backoff_until() const { return bag_recovery_backoff_until_ms_; }
    void   set_bag_recovery_backoff(uint32 until_ms) { bag_recovery_backoff_until_ms_ = until_ms; }
    void   bag_recovery_reset()
    {
        bag_recovery_first_ms_  = 0;
        bag_recovery_free_seen_ = 255;
        bag_recovery_backoff_until_ms_ = 0;   // futile episode over (bags cleared)
        capital_run_futile_until_ms_ = 0;     // and any capital-run suppression
    }
    // Capital-bag-run futility. When the in-city walk to the banker/AH/vendor
    // cluster proves UNREACHABLE (ChunkedWalkToward gives up after its strike
    // budget — a navmesh gap at the destination), suppress idle:capital_bag_run for
    // a cooldown so the bot resumes questing (it still gains XP from kills with full
    // bags) instead of looping the unreachable walk above the quest funnel forever
    // (live 2026-06-22: Durnan/Velruun pinned in Stormwind, 200+ path_fail on the
    // same 55y path ending 6.5y short). Retries after the cooldown.
    bool   capital_run_in_futility(uint32 now_ms) const
    { return capital_run_futile_until_ms_ != 0 && now_ms < capital_run_futile_until_ms_; }
    void   capital_run_note_futile(uint32 now_ms)
    { capital_run_futile_until_ms_ = now_ms + 180000u; }   // 3 min: quest, then retry
    // Call once per tick while bags are full and recovery is the chosen path.
    // Returns true when recovery has been futile (no free slot gained) for the
    // window — the caller then sets a back-off and yields to questing.
    bool bag_recovery_note_and_check_futile(uint8 /*free_now*/, uint32 now_ms)
    {
        // Time-since-bags-became-full. The gate calls bag_recovery_reset() the moment
        // bags genuinely un-fill (the full bit clears), so this window only advances
        // while the bot is CONTINUOUSLY full. Marginal sell/refill churn (free 2<->3,
        // never actually clearing) must NOT reset it — otherwise a bot with too-small
        // bags that vendors one item, re-loots, and re-fills loops forever without
        // ever backing off. If it's been full this long, selling isn't solving it
        // (needs a bank/bigger-bag capital run) — yield to questing meanwhile.
        if (bag_recovery_first_ms_ == 0)
        {
            bag_recovery_first_ms_ = now_ms;
            return false;
        }
        constexpr uint32 kBagFutileWindowMs = 120000u;   // 2 min continuously full
        return (now_ms - bag_recovery_first_ms_) >= kBagFutileWindowMs;
    }
    // Hub-offer close-approach futility (Durnan wedge, 2026-06-21). idle:walk_to_
    // known_hub@700 close-approaches an in-scan offer giver every tick so quest_
    // accept@701 can grab it. But if the bot is positioned where the giver is in
    // 5y scan yet it can't physically close to the 3.5y accept-emit margin (micro
    // navmesh wedge at the giver), accept never fires and the hub rule loops
    // FOREVER returning true — permanently starving the lower travel band
    // (idle:far_same_map_travel@697) so a bot with a far goal never travels
    // (observed: Durnan L15, 4 in-scan offers, position+XP frozen). Mirror the
    // bag-recovery back-off: if the in-scan offer count has not DROPPED (no accept
    // = no progress) for the window, the close-approach is futile — yield for a
    // cooldown so the travel/quest cascade runs, then retry (the wedge may clear
    // once the bot is repositioned by another rule). reset() ONLY on genuine
    // clear (no offer in scan) so marginal offer churn never wipes the window.
    uint32 hub_offer_backoff_until() const { return hub_offer_backoff_until_ms_; }
    void   set_hub_offer_backoff(uint32 until_ms) { hub_offer_backoff_until_ms_ = until_ms; }
    void   hub_offer_reset()
    {
        hub_offer_since_ms_        = 0;
        hub_offer_min_count_       = 0;
        hub_offer_backoff_until_ms_ = 0;
    }
    // Reset the futility WINDOW but KEEP the back-off — called when a futile
    // episode is declared, so after the back-off expires the close-approach gets
    // a fresh window (the bot may have been repositioned by the travel rules
    // meanwhile) instead of instantly re-tripping futility on the stale window
    // and yielding forever.
    void   hub_offer_reset_window()
    {
        hub_offer_since_ms_  = 0;
        hub_offer_min_count_ = 0;
    }
    // Chunked-walk position-stall tracker (ledge/corner escape, 2026-06-21).
    // Drives ChunkedWalkToward: returns the count of consecutive ~4s windows the
    // bot made <9y progress while a chunked walk was steering it (0 = moving
    // fine). The walk helper escalates its bearing-deflection with the strike
    // count to break OFF ledges/corners that path_blocked_count alone misses
    // (Durnan climbed a Stormwind ledge at z=100 and froze, move_to never
    // "blocked"), and yields past a cap so stuck-recovery can take over. Re-bases
    // after a >10s gap (a fresh walk episode) so a bot that stopped and resumed
    // doesn't inherit stale strikes.
    uint8 walk_stall_note(float x, float y, uint32 now_ms)
    {
        constexpr float  kProgressSq   = 9.0f * 9.0f;
        constexpr uint32 kWindowMs     = 4000u;
        constexpr uint32 kEpisodeGapMs = 10000u;
        if (walk_stall_since_ms_ == 0 || (now_ms - walk_stall_last_ms_) > kEpisodeGapMs)
        {
            walk_stall_since_ms_ = now_ms ? now_ms : 1u;
            walk_stall_last_ms_  = now_ms;
            walk_stall_x_ = x; walk_stall_y_ = y; walk_stall_strikes_ = 0;
            return 0;
        }
        walk_stall_last_ms_ = now_ms;
        const float dx = x - walk_stall_x_, dy = y - walk_stall_y_;
        if (dx*dx + dy*dy >= kProgressSq)   // moved -> progress, reset
        {
            walk_stall_x_ = x; walk_stall_y_ = y;
            walk_stall_since_ms_ = now_ms ? now_ms : 1u; walk_stall_strikes_ = 0;
            return 0;
        }
        if (now_ms - walk_stall_since_ms_ >= kWindowMs)   // a full no-progress window
        {
            if (walk_stall_strikes_ < 255) ++walk_stall_strikes_;
            walk_stall_since_ms_ = now_ms ? now_ms : 1u;
        }
        return walk_stall_strikes_;
    }
    void walk_stall_reset() { walk_stall_since_ms_ = 0; walk_stall_strikes_ = 0; }
    // Call once per tick while an in-scan offer giver is being close-approached.
    // Returns true when no offer has been accepted (count never dropped) for
    // futility_ms — the caller sets a back-off and yields.
    bool hub_offer_note_and_check_futile(uint8 offer_count, uint32 now_ms, uint32 futility_ms)
    {
        if (hub_offer_since_ms_ == 0)
        {
            hub_offer_since_ms_  = now_ms ? now_ms : 1u;
            hub_offer_min_count_ = offer_count;
            return false;
        }
        if (offer_count < hub_offer_min_count_)   // an offer got accepted -> progress
        {
            hub_offer_min_count_ = offer_count;
            hub_offer_since_ms_  = now_ms ? now_ms : 1u;   // fresh window
            return false;
        }
        return (now_ms - hub_offer_since_ms_) >= futility_ms;
    }
    // Call once per tick while the bot is ADJACENT to the repair vendor. Returns
    // true when the bot has spent >= futility_ms at the vendor without its lowest
    // durability improving — i.e. it's stuck (too poor) and should yield.
    bool crit_repair_note_adjacent(uint8 cur_dura_pct, uint32 now_ms, uint32 futility_ms)
    {
        if (crit_repair_adjacent_since_ms_ == 0)
        {
            crit_repair_adjacent_since_ms_ = now_ms ? now_ms : 1u;
            crit_repair_best_dura_pct_     = cur_dura_pct;
            return false;
        }
        if (cur_dura_pct > crit_repair_best_dura_pct_)   // made progress -> reset window
        {
            crit_repair_best_dura_pct_     = cur_dura_pct;
            crit_repair_adjacent_since_ms_ = now_ms ? now_ms : 1u;
            return false;
        }
        return (now_ms - crit_repair_adjacent_since_ms_) >= futility_ms;
    }
    // Episode-level futility — call once per critical_repair fire in ANY sub-state
    // (walk / approach / adjacent). Returns true when the bot has been trying to
    // repair for >= max_ms with NO lowest-durability gain: it can't reach a vendor
    // (path-blocked, vendor across a gap) or can't afford anything. Catches the
    // long-WALK wedge the adjacent timer misses (a bot walking to an unreachable
    // repair vendor is classified Travel by the wedge-watchdog and never rescued).
    bool crit_repair_note_active(uint8 cur_dura_pct, uint32 now_ms, uint32 max_ms)
    {
        if (crit_repair_episode_since_ms_ == 0)
        {
            crit_repair_episode_since_ms_  = now_ms ? now_ms : 1u;
            crit_repair_episode_best_dura_ = cur_dura_pct;
            return false;
        }
        if (cur_dura_pct > crit_repair_episode_best_dura_)   // repaired some -> progress
        {
            crit_repair_episode_best_dura_ = cur_dura_pct;
            crit_repair_episode_since_ms_  = now_ms ? now_ms : 1u;
            return false;
        }
        return (now_ms - crit_repair_episode_since_ms_) >= max_ms;
    }
    void crit_repair_reset_episode()
    {
        crit_repair_adjacent_since_ms_ = 0;
        crit_repair_best_dura_pct_     = 0;
        crit_repair_episode_since_ms_  = 0;
        crit_repair_episode_best_dura_ = 0;
    }

private:
    // Primary state dispatch; selects which DispatchXxx is called.
    void dispatch_primary(BotSnapshotView, GroupSnapshotView,
                          BotIntentEmitter&);

    // Cross-cutting layers: InGroup, InInstance, AtVendor etc. run alongside
    // the primary state and may add intents.
    void dispatch_layers(BotSnapshotView, GroupSnapshotView,
                         BotIntentEmitter&);

    BotId          bot_id_;
    BotPersonality personality_;
    // Per-bot play archetype. Defaults to CasualSolo (id 0) until the login
    // path sets it from the persisted/rolled value (see set_archetype).
    BotArchetype   archetype_{};
    BotRng         rng_;
    BotState       state_         = BotState::LoggingIn;
    BotState       prev_state_    = BotState::LoggingIn;
    Ms             state_entered_{0};
    uint32         tick_count_    = 0;
    char const*    last_rule_fired_ = nullptr;
    // Watchdog state — see kRuleWatchdogFireCount above.
    char const*    rule_watchdog_name_       = nullptr;
    uint32         rule_watchdog_count_      = 0;
    uint32         rule_watchdog_window_start_ms_ = 0;
    // Position anchor for the watchdog movement-progress credit (clear_rule_watchdog).
    float          watchdog_anchor_x_   = 0.f;
    float          watchdog_anchor_y_   = 0.f;
    bool           watchdog_anchor_set_ = false;
    // Path-failure window for the watchdog move-success credit: baseline of the
    // global path_blocked_count at window start, re-based every ~10s (mirrors
    // check_anchor_wedge). The escape only fires when pathing is actually
    // FAILING (>=3 blocks in the window), not when a rule re-fires while the
    // bot's moves succeed (spline-thrash on a long chunked trek).
    uint32         watchdog_blocks_baseline_  = 0;
    uint32         watchdog_blocks_window_ms_ = 0;
    std::unordered_map<char const*, uint32> rule_watchdog_suppress_until_;
    bool           ready_check_acked_ = false;
    bool           mount_pending_     = false;
    bool           invite_acked_      = false;
    bool           guild_invite_acked_ = false;
    uint32         lfg_proposal_acked_id_ = 0;
    uint32         bg_outcome_recorded_ms_ = 0;   // dedup BG-end tally
    // Index into the current dungeon's progression_waypoints[]. The
    // tank-advance rule increments this when the tank gets within ~10y
    // of the current waypoint, so the next tick walks to the following
    // point. Resets to 0 on dungeon-run mode flip-to-Active. Read/written
    // exclusively on the world thread; no synchronization needed.
public:
    uint8 dungeon_waypoint_index() const { return dungeon_waypoint_index_; }
    void  set_dungeon_waypoint_index(uint8 i) { dungeon_waypoint_index_ = i; }
private:
    uint8          dungeon_waypoint_index_ = 0;
    bool           rez_acked_         = false;
    bool           corpse_recovery_emitted_ = false;
    uint32         ghost_since_ms_   = 0;
    uint32         swimming_since_ms_ = 0;
    ObjectGuid     stuck_chase_victim_;
    uint16         stuck_chase_ticks_ = 0;
    ObjectGuid     last_taxi_discover_fm_;
    ObjectGuid     last_homebind_innkeeper_;
    ManualTravelGoal manual_travel_;
    mutable std::mutex manual_travel_mtx_;   // guards manual_travel_ (parser writes / builder reads+clears, cross-thread)
    bool   travel_force_graph_ = false;
    uint64 poi_prog_key_  = 0;
    float  poi_prog_dist_ = 0.f;
    uint32 poi_prog_ms_   = 0;
    // Per-zone activity tally for the activity-driven hearth rebind.
    // See note_zone_activity / ms_in_zone above. Small bounded vector,
    // not a map (zone count per bot is single-digits in practice; LRU
    // evicts the least-active zone at 16 entries).
    struct ZoneActivity { uint32 zone_id; uint32 ms_in_zone; };
    std::vector<ZoneActivity> activity_by_zone_;
    uint32         hearth_zone_       = 0;
    uint32         last_activity_tick_ms_ = 0;
    uint32         last_calendar_rsvp_ms_ = 0;
    uint8          last_seen_level_   = 0;
    // Deferred "Ding!" announce — when level ticks up, capture the new
    // level + an emit time 1.5–5.5s in the future so an entire raid of
    // bots that all ding on the same XP-tick spread their /p chatter
    // across ~4 seconds instead of stacking inside one server tick.
    // Slot is consumed by Tick() the moment now_ms >= pending_ding_say_at_ms_
    // and reset back to zero.
    uint8          pending_ding_level_  = 0;
    uint32         pending_ding_say_at_ms_ = 0;
    // Loot-roll decision deferral. Real players see the roll popup and
    // hesitate 1–4s before pressing Need/Greed; bots used to vote inside
    // the same server tick the roll appeared, in lockstep across the
    // raid. This slot captures a per-bot decide-at timestamp so each
    // bot's vote lands at a different ms. After firing, we schedule the
    // next vote 400–1500ms later so multi-item drops (e.g. a 3-item
    // boss-kill loot table) don't auto-burst as soon as the first one
    // resolves.
    uint32         next_loot_roll_fire_ms_ = 0;
    // PvE death-release delay. Real humans look at the death screen for
    // several seconds before pressing Release — waiting on a battle-rez
    // chat ping or staring blankly. Instant release is a robot tell.
    // Captures the timestamp the bot first arrived at the recovery flow
    // for this death; release fires once now_ms >= that + 3–7s jitter.
    // Reset to 0 on revive so the next death gets a fresh window.
    // Doesn't apply in BG (spirit-guide auto-rez supersedes).
    uint32         release_pending_at_ms_ = 0;
    // Bounded-recovery watchdog: GameTime ms when the bot was first observed
    // dead for the current death. Read off the server clock (NOT the snapshot's
    // published_at_ms) so a stalled recovery sub-step (a pre-release wedge that
    // froze the release timer was observed live: Dunghunter in Deadmines, 10+
    // min in dead:release_pending while the group's tank-advance gate deadlocked
    // on group_not_ready) always terminates via a forced spirit-healer rez.
    // Reset to 0 on death-entry and on revive.
    uint32         dead_watchdog_ms_ = 0;
    // Social-popup hesitation: real humans take 0.5–2.5s to click Accept
    // on a party invite or LFG proposal. Bots that accept inside one
    // server tick are a top-3 anti-bot tell. Slot captures the
    // first-seen-ms; cleared by the rule's false-clear path when the
    // popup window closes (invite resolved or expired).
    uint32         pending_group_invite_accept_at_ms_ = 0;
    uint32         pending_lfg_proposal_accept_at_ms_ = 0;
    // Quest dialog hesitation: real humans read the quest description
    // before clicking Accept (1.5–6s) and pause to consider the reward
    // choice before Complete (1–4s). Two distinct slots so the accept
    // delay on a multi-quest hub doesn't bleed into the turn-in delay
    // afterwards.
    uint32         pending_quest_accept_at_ms_ = 0;
    uint32         pending_quest_turnin_at_ms_ = 0;
    // Vendor-open hesitation: real players stand at a vendor for 1–3s
    // (opening the window, scrolling, scanning for repair widget) before
    // any sell-trash / repair clicks land. Without this every bot in a
    // capital that has trash to sell + repair due fires both intents on
    // the same tick they walk into the 5y radius. Reset when the visit
    // FSM clears (either complete or walked out of range).
    uint32         pending_vendor_visit_at_ms_ = 0;
    // Loot-drain "look at the body" pause: when the loot-corpse queue
    // first becomes non-empty, hesitate 300–1500ms before emitting the
    // first LootIntent. Models a player walking to the corpse and
    // taking a beat before right-clicking. Slot is global to the drain
    // sequence (not per-corpse) — chain-pulls don't re-pause between
    // each corpse, only on the first one. Reset to 0 when the queue
    // drains so the next death starts a fresh window.
    uint32         pending_loot_drain_at_ms_ = 0;
    // Mount-up hesitation when following a leader: real humans take
    // 300–1200ms to press their mount keybind after seeing a teammate
    // mount up. Bots used to fire MountIntent the same tick the leader
    // mounted — five bots all begin mount-casting on the same frame, a
    // top-3 visual robot tell. Reset to 0 when `want_mount` lapses.
    uint32         pending_follow_mount_at_ms_ = 0;
    // Equip-upgrade compare hesitation: real players hover the new
    // item, read tooltip, hover the equipped slot, compare. Only
    // ~10% of upgrades equip in <500ms (instant click-drag with
    // perfect mouse). The bot used to equip the same tick a
    // higher-scored item dropped — a robot tell for anyone observing.
    // Slot resets when the rule isn't gating (in combat / dungeon /
    // BG), so a fresh upgrade after exit gets a fresh window.
    uint32         pending_equip_upgrade_at_ms_ = 0;
    // Mailbox-open hesitation: real humans click the mailbox, see the
    // list, hover one row, click "Take All". 1.5–4s before the first
    // take packet lands. Reset when the bot walks out of mailbox range.
    uint32         pending_mail_open_at_ms_ = 0;
    uint32         chat_pause_until_ms_ = 0;
    bool           saw_special_phase_   = false;
    ObjectGuid     last_seen_group_;
    bool           group_greet_primed_ = false;
    float          follow_distance_   = 0.f;
    Role           role_override_     = Role::Unknown;
    ObjectGuid     focus_target_;
    // Last trainer NPC GUID we've already bulk-trained against, paired
    // with the level we trained at. Cleared by bumping
    // last_train_level when the bot levels — so re-visiting the same
    // trainer post-ding emits a fresh bulk-train. Prevents the auto-train
    // rule from re-emitting trainer_buy_all every tick the bot lingers.
    ObjectGuid     last_trained_trainer_;
    uint8          last_train_level_ = 0;
    bool           aoe_preference_   = false;
    // Owner-squad-control state (Phase C of OWNER_SQUAD_CONTROL_PLAN.md).
    OwnerCommand   owner_command_         = OwnerCommand::None;
    ObjectGuid     owner_target_;
    uint32         manual_mode_until_ms_  = 0;
    uint32         last_owner_command_ms_ = 0;
    std::string    last_owner_name_;
    uint8          formation_slot_        = 0;
    FormationType  formation_type_        = FormationType::Free;
    DungeonRunMode dungeon_run_mode_      = DungeonRunMode::Off;
    BgRunMode      bg_run_mode_           = BgRunMode::Off;
    // Edge-tracker for BG entry/exit. Auto-activate fires on the
    // false→true transition of `in_battleground()`, not while the bot
    // remains in. Otherwise `/bgrun stop` would flip back to Active
    // every tick the bot's still in the BG.
    bool           prev_in_battleground_  = false;
    // Edge-detect helper for the LFG-dungeon auto-run arm: true while
    // (inside instance && group is LFG). Armed only on the false→true
    // transition so `/run stop` sticks for the rest of the visit instead
    // of being overwritten back to Active every tick (same bug class as
    // the level-triggered /bgrun stop no-op).
    bool           prev_lfg_dungeon_auto_ = false;
    // Edge-detect helper for "FC down" callout: whether last tick's
    // snapshot showed a non-empty friendly flag carrier. true→false
    // transition fires a single raid-chat warning.
    bool           prev_friendly_fc_set_   = false;
    // BG advice cache. At scale (1000+ BG bots × ~20 ticks/sec) the
    // per-tick GetAdvice() construction was a measurable churn source —
    // each call allocates several vectors via push_back chains in the
    // script's get_advice body. The advice is near-static once a bot is
    // in a BG: only a handful of snapshot fields (faction, SoTA gate
    // state, SoTA attacker team, IoC gate state, AV captain alive) drive
    // changes. We cache the last constructed advice keyed on those
    // fields + a 2s hard-refresh interval, rebuilding only on cache miss.
    //
    // Memory cost: ~sizeof(BattlegroundAdvice) ≈ 256 bytes per bot
    // (vectors are empty or short on most BGs). Acceptable for 1000 bots
    // (256 KB total) vs the ~20K mallocs/sec savings on a hot tick.
    struct BgAdviceCache
    {
        uint16_t bg_type_id_key = 0;
        bool     is_horde_key   = false;
        int8_t   sota_atk_key   = -2;        // -2 = invalid sentinel
        uint32_t sota_gate_key  = 0u;        // packed gate states
        uint8_t  ioc_gate_key   = 0xFFu;     // packed gate destroyed bits
        uint8_t  av_captain_key = 0xFFu;     // bit0=balinda, bit1=galvangar (1=alive)
        uint32_t last_built_ms  = 0u;
        BattlegroundAdvice cached;
    };
    // OWNED BY THE AI WORKER THREAD. BgDispatch reassigns `cached`'s heap
    // vectors on every cache miss (≤2s apart), so no other thread may read
    // this struct — world-thread consumers use published_bg_advice_ below.
    BgAdviceCache  bg_advice_cache_;
public:
    // World-thread-readable advice mirror (adversarial review 2026-06-10:
    // the snapshot builder's bg_role block read bg_advice_cache_.cached
    // directly from the world thread while the AI worker reassigned its
    // vectors — use-after-free window every cache rebuild). On each
    // rebuild the worker publishes an IMMUTABLE copy through a C++20
    // atomic<shared_ptr> (same pattern as SnapshotPublisher slots);
    // readers hold their own reference, so a concurrent re-publish only
    // drops the refcount, never the buffers under a reader.
    struct PublishedBgAdvice
    {
        uint16_t           bg_type_id = 0;
        BattlegroundAdvice advice;
    };
    void publish_bg_advice(uint16_t bg_type_id, BattlegroundAdvice const& adv)
    {
        auto p = std::make_shared<PublishedBgAdvice>();
        p->bg_type_id = bg_type_id;
        p->advice     = adv;
        published_bg_advice_.store(std::move(p), std::memory_order_release);
    }
    std::shared_ptr<PublishedBgAdvice const> published_bg_advice() const
    { return published_bg_advice_.load(std::memory_order_acquire); }
private:
    std::atomic<std::shared_ptr<PublishedBgAdvice const>> published_bg_advice_;
public:
    bool prev_in_battleground() const { return prev_in_battleground_; }
    void set_prev_in_battleground(bool v) { prev_in_battleground_ = v; }
    bool prev_lfg_dungeon_auto() const { return prev_lfg_dungeon_auto_; }
    void set_prev_lfg_dungeon_auto(bool v) { prev_lfg_dungeon_auto_ = v; }
    bool prev_friendly_fc_set() const { return prev_friendly_fc_set_; }
    void set_prev_friendly_fc_set(bool v) { prev_friendly_fc_set_ = v; }
    BgAdviceCache&       bg_advice_cache()       { return bg_advice_cache_; }
    BgAdviceCache const& bg_advice_cache() const { return bg_advice_cache_; }
private:
    // Phase J state. Loop-mode is owner-toggled per session (no DB
    // persistence — bots default to single-run on respawn). The
    // last_lfg_dungeon_id_ is recorded by the LfgQueueIntent executor
    // so the post-completion rule has a target to re-queue without
    // needing the owner to re-issue `/lfg <id>`.
    bool           dungeon_loop_mode_     = false;
    uint32         last_lfg_dungeon_id_   = 0;
    uint32         last_loop_requeue_ms_  = 0;
    // Last-applied talent build context (0=Default, 1=Raid, 2=MythicPlus,
    // 3=PvP, 4=Leveling). 0xFF = never applied. The auto-apply rule
    // edge-triggers when the detected context differs from this value.
    uint8          last_applied_talent_ctx_ = 0xFFu;
    uint32         last_kill_ms_          = 0;
    uint32         combat_entered_ms_     = 0;
    uint32         tank_diag_ms_          = 0;
    uint32         dungeon_kills_         = 0;
    uint32         dungeon_deaths_        = 0;
    // Open-world death-spiral memory (see note_open_world_death).
    uint32         consecutive_same_spot_deaths_ = 0;
    DeathBlackspot death_blackspots_[4];   // temporary travel no-go zones (see arm_death_blackspot)
    uint16         jit_purpose_bg_type_ = 0;  // BG type this JIT bot exists to fill (0 = not a JIT bot)
    uint32         jit_purpose_set_ms_  = 0;  // when the JIT purpose was assigned (staging timeout)
    uint32         last_death_ms_         = 0;
    float          last_death_x_          = 0.f;
    float          last_death_y_          = 0.f;
    float          last_death_z_          = 0.f;
    uint32         last_seen_map_id_      = 0;
    // Primes on the first observed map so a fresh login doesn't trigger the
    // map-change movement clear; thereafter true so EVERY real map change
    // (including 0 -> N — map 0 is valid) drops any stale MotionMaster mover.
    bool           map_change_primed_     = false;
    uint32         last_interrupt_ms_     = 0;
    // Whisper rate-gate. We collapse identical (target, text) whispers
    // within a 500ms window so that owner-issued commands which trigger
    // multiple replies (e.g. /follow → "now following" + manual-mode
    // ack) don't dedupe against each other, but spam-clicked /follow
    // doesn't pile up 5 identical lines in the owner's chat. Holds
    // only the very last whisper; a real ring isn't necessary at
    // typical command frequencies.
    std::string  last_whisper_target_;
    uint64_t     last_whisper_hash_       = 0;
    uint32       last_whisper_ms_         = 0;
public:
    // Returns true if (target, text) is OK to whisper now, false if
    // it would be a duplicate of the last emit within 500ms. Updates
    // the gate state on accept. Skipped (always returns true) when
    // text is empty.
    bool whisper_rate_check(std::string const& target,
                            std::string const& text,
                            uint32 now_ms);
private:
    ObjectGuid     brez_target_;
    bool           brez_acked_       = false;
    uint8          last_auto_equip_level_ = 0;
    uint32         last_auto_equip_check_ms_ = 0;
    uint32         last_cast_spell_id_    = 0;
    float          elevator_boarding_z_   = 0.f;
    float          transport_prev_z_      = 0.f;   // last-tick Z while on_transport
    uint32         transport_z_stable_ms_ = 0;     // ms since Z last moved > 0.3y
    bool           was_on_transport_      = false;
    bool           starter_talents_acked_ = false;
    uint32         starter_talents_emit_ms_ = 0;
    uint8          last_starter_extend_level_ = 0;
    bool           verbose_logging_ = false;
    uint32         last_ambient_emote_ms_ = 0;
    uint32         last_look_around_ms_   = 0;
    uint32         last_ambient_state_emote_ms_ = 0;
    uint32         last_guild_babble_ms_  = 0;
    uint32         last_yell_lfg_ms_      = 0;
    // Per-bot throttle on the snapshot builder's 500y hub-radius scan.
    // Capital-city clusters (Stormwind/Org/Dalaran) clear 200-500 bots
    // per cluster; without a throttle each bot's snapshot ran a 500y
    // Cell::VisitAllObjects every world frame (50Hz) — 10-25K wide-scans/sec.
    // 1s cadence is plenty: hub quest-givers don't move, and the smaller
    // 40y scan still captures the immediate neighborhood every tick.
    // Builder reads/writes; AI worker doesn't touch this field.
    uint32         last_hub_scan_ms_      = 0;
    // Same throttle pattern for the 200y extended gather-node scan
    // (perf audit #3 — capital-clustered gatherers also fire this
    // every frame). Nodes are static; 1Hz refresh is plenty.
    uint32         last_gather_scan_ms_   = 0;
    // Currency-walk throttle. sCurrencyTypesStore.GetNumRows() is ~700;
    // each row is one hash probe. At 2000 bots × 5Hz = ~7M lookups/sec.
    // Currency only changes on quest turn-in / vendor, so 1Hz refresh
    // is plenty. Cached values are persisted in the snapshot via the
    // previous Build's `currencies` vector — but since the snapshot is
    // rebuilt anyway, we just skip the walk when throttled and let
    // the new snapshot's `currencies` field be empty for that tick.
    // Consumers (rep / currency whisper, badge purchases) gate on
    // `!currencies.empty()` already; brief tick gaps are harmless.
    uint32         last_currency_scan_ms_ = 0;
    // Throttle for the BG siege-vehicle wide scan (BotSnapshotBuilder). IoC
    // siege engines / demolishers spawn at fixed pads 50-80y from their node —
    // outside the 40y nearby_friends scan — so a node-holding bot never sees
    // them. A throttled ~110y scan appends friendly empty BG vehicles so the
    // vehicle-mount rule can route to them. Builder-only field.
    uint32         last_bg_veh_scan_ms_   = 0;
public:
    uint32 last_hub_scan_ms() const { return last_hub_scan_ms_; }
    void   set_last_hub_scan_ms(uint32 v) { last_hub_scan_ms_ = v; }
    uint32 last_bg_veh_scan_ms() const { return last_bg_veh_scan_ms_; }
    void   set_last_bg_veh_scan_ms(uint32 v) { last_bg_veh_scan_ms_ = v; }
    uint32 last_gather_scan_ms() const { return last_gather_scan_ms_; }
    void   set_last_gather_scan_ms(uint32 v) { last_gather_scan_ms_ = v; }
    uint32 last_currency_scan_ms() const { return last_currency_scan_ms_; }
    void   set_last_currency_scan_ms(uint32 v) { last_currency_scan_ms_ = v; }
    std::vector<BotSnapshot::CurrencyEntry> const& cached_currencies() const
    { return cached_currencies_; }
    std::vector<BotSnapshot::CurrencyEntry>& mutable_cached_currencies()
    { return cached_currencies_; }
    // Reputations cache — same pattern as currencies. ReputationMgr's
    // GetStateList walks all factions the bot has any state for (often
    // 100-300 entries), each with a sFactionStore lookup + GetReputation
    // + GetRank call. Refresh at 1Hz; consumers see consistent (≤1s stale)
    // data via the per-tick copy from cache to snapshot.
    uint32 last_reputation_scan_ms() const { return last_reputation_scan_ms_; }
    void   set_last_reputation_scan_ms(uint32 v) { last_reputation_scan_ms_ = v; }
    std::vector<BotSnapshot::ReputationEntry> const& cached_reputations() const
    { return cached_reputations_; }
    std::vector<BotSnapshot::ReputationEntry>& mutable_cached_reputations()
    { return cached_reputations_; }
    // Skills cache — same pattern. The skill-array walk is 256 probes
    // for ~10-20 hits; cheap individually but adds up at 2000 bots × 5Hz.
    // Skills change at level-up + train-spell + first-kill events; 1Hz
    // is plenty.
    uint32 last_skills_scan_ms() const { return last_skills_scan_ms_; }
    void   set_last_skills_scan_ms(uint32 v) { last_skills_scan_ms_ = v; }
    std::vector<BotSnapshot::SkillEntry> const& cached_skills() const
    { return cached_skills_; }
    std::vector<BotSnapshot::SkillEntry>& mutable_cached_skills()
    { return cached_skills_; }

    // Recipe + self-teleport caches. The Builder used to walk every known
    // spell (300+) per snapshot, fetching SpellInfo + iterating each
    // spell's effects to bucket recipes vs teleports — ~150K effect
    // comparisons/sec at 140 builds/sec × 300 spells × ~3 effects.
    //
    // These lists mutate only on spell-learn / spell-unlearn events
    // (trainer visit, level-up, respec). Cache for 30s — well past the
    // typical learn-trainer-spell round trip. Snapshot freshness for
    // crafting/teleport rules is unaffected.
    uint32 last_spellcache_scan_ms() const { return last_spellcache_scan_ms_; }
    void   set_last_spellcache_scan_ms(uint32 v) { last_spellcache_scan_ms_ = v; }

    // Known-spells sorted-vector cache (perf 0.1). The Builder used to
    // filter (active && !disabled) + copy + std::sort the entire SpellMap
    // (300+ entries) every snapshot, though the result only changes on
    // learn / level-up / respec. Cache the sorted vector and reuse it.
    //
    // Invalidation is keyed on BOTH the ACTIVE-and-!disabled spell COUNT and a
    // 30s timer. The count (not raw SpellMap size) is the right key because the
    // cached value is the active/!disabled-FILTERED set: RemoveSpell(disabled=
    // true) and rank supersede flip an existing entry's active/disabled flag
    // without changing map size, so a size-only key would go stale. The count
    // still misses a same-count swap (learn A, unlearn B → identical active
    // count), so the 30s timer backstops it. The low-level [bot_spells] diag
    // dump reads from this cache too, so it stays correct.
    uint32 last_known_spells_scan_ms() const { return last_known_spells_scan_ms_; }
    void   set_last_known_spells_scan_ms(uint32 v) { last_known_spells_scan_ms_ = v; }
    // Stores the count of active && !disabled spells the cache was built from
    // (see set_cached_spellmap_size usage in BotSnapshotBuilder), not raw size.
    size_t cached_spellmap_size() const { return cached_spellmap_size_; }
    void   set_cached_spellmap_size(size_t v) { cached_spellmap_size_ = v; }
    std::vector<uint32> const& cached_known_spells() const { return cached_known_spells_; }
    std::vector<uint32>& mutable_cached_known_spells() { return cached_known_spells_; }

    // Best-mount-spell cache (perf 0.2). The Builder used to walk the whole
    // SpellMap, fetch SpellInfo, and iterate every effect's ApplyAuraName to
    // score mounts and pick the fastest usable one — every snapshot. The
    // pick only changes on mount-learn / riding-skill change. Cache the
    // resolved spell id behind the same 30s timer the recipe/known-spells
    // caches use; mount learns are far rarer than 30s.
    uint32 last_mount_scan_ms() const { return last_mount_scan_ms_; }
    void   set_last_mount_scan_ms(uint32 v) { last_mount_scan_ms_ = v; }
    uint32 cached_best_mount_spell() const { return cached_best_mount_spell_; }
    void   set_cached_best_mount_spell(uint32 v) { cached_best_mount_spell_ = v; }
    // Riding skill the cached mount pick was computed against. A change in
    // riding skill (Expert 225 unlocks flying mounts) flips which mount is
    // "best", so the cache must invalidate when riding_skill changes even
    // within the 30s window.
    uint16 cached_mount_riding_skill() const { return cached_mount_riding_skill_; }
    void   set_cached_mount_riding_skill(uint16 v) { cached_mount_riding_skill_ = v; }

    // Representative GCD-probe spell id (perf 0.3). GCD is global/category-
    // based, so the Builder need not walk the whole spellbook hunting for a
    // spell on GCD — a single known spell that carries a GCD answers the
    // question. Cache the first such id we find; resolved lazily. 0 = not
    // yet resolved (or none found, in which case the in-combat path falls
    // back to the spellbook walk). Invalidated alongside the known-spells
    // cache so a respec that removes the probe spell re-resolves it.
    uint32 cached_gcd_probe_spell() const { return cached_gcd_probe_spell_; }
    void   set_cached_gcd_probe_spell(uint32 v) { cached_gcd_probe_spell_ = v; }

    // One-shot guard for the [bot_spells] dump emitted by BotSnapshotBuilder
    // for L<=10 bots. Becomes true after the first dump; never reset, so
    // we don't re-log every tick (only one line per low-level bot per
    // server lifetime).
    bool   spellbook_diag_logged() const { return spellbook_diag_logged_; }
    void   set_spellbook_diag_logged(bool v) { spellbook_diag_logged_ = v; }
    // The last level we emitted [bot_spells] for. Lets the diag re-emit
    // on every ding so we can audit which SkillLineAbility grants land
    // at each level (Hunter's Mark, Arcane Shot, etc. auto-learn at
    // specific levels in retail and the per-level set matters for APL
    // candidate-list tuning).
    uint8  spellbook_diag_level() const { return spellbook_diag_level_; }
    void   set_spellbook_diag_level(uint8 v) { spellbook_diag_level_ = v; }
    std::vector<uint32> const& cached_recipes() const { return cached_recipes_; }
    std::vector<uint32>& mutable_cached_recipes() { return cached_recipes_; }
    std::vector<Playerbot::SelfTeleportSpell> const& cached_self_teleports() const
    { return cached_self_teleports_; }
    std::vector<Playerbot::SelfTeleportSpell>& mutable_cached_self_teleports()
    { return cached_self_teleports_; }

    // ---- Per-bot snapshot-build scratch state (Phase 4 parallel-by-Map*) ----
    // These three blocks were process-wide std::unordered_maps keyed by BotId
    // inside BotSnapshotBuilder's anonymous namespace. Under the parallel-by-
    // Map* Build they are a data-race surface (one shared map, concurrent
    // insert/erase rehashes from multiple workers). They are inherently PER-BOT
    // state, so they live on the bot's own BotAI: each Map's bots build on a
    // single worker, so a given BotAI is touched by exactly one thread during
    // Build — single-writer, lock-free, byte-identical behavior.

    // combat-duration tracking (was g_combat_start_ms / g_combat_exit_ms).
    // 0 = "not currently timing" (start) / "never left / still in combat"
    // (exit). Values are GameTimeMS-based int64 with unsigned-wrap subtraction,
    // exactly as the old process-wide maps did.
    int64_t combat_start_ms() const { return combat_start_ms_; }
    void    set_combat_start_ms(int64_t v) { combat_start_ms_ = v; }
    int64_t combat_exit_ms() const { return combat_exit_ms_; }
    void    set_combat_exit_ms(int64_t v) { combat_exit_ms_ = v; }

    // is_moving displacement-corroboration anchor (was s_moveCorroborate).
    // mc_valid_ marks the anchor populated; reset to false when the bot stops.
    struct MoveCorroborate { bool valid = false; float x = 0, y = 0, z = 0; uint32 since_ms = 0; };
    MoveCorroborate const& move_corroborate() const { return move_corroborate_; }
    void set_move_corroborate(float x, float y, float z, uint32 since_ms)
    { move_corroborate_ = MoveCorroborate{true, x, y, z, since_ms}; }
    void clear_move_corroborate() { move_corroborate_ = MoveCorroborate{}; }

    // [picker_choice] log-dedup signature (was s_pickerChoiceLogged). Pure
    // diagnostic: last logged signature so the line is emitted only when the
    // decision tuple changes for this bot.
    uint64 picker_choice_log_sig() const { return picker_choice_log_sig_; }
    void   set_picker_choice_log_sig(uint64 v) { picker_choice_log_sig_ = v; }
private:
    std::vector<BotSnapshot::CurrencyEntry>   cached_currencies_;
    std::vector<BotSnapshot::ReputationEntry> cached_reputations_;
    std::vector<BotSnapshot::SkillEntry>      cached_skills_;
    std::vector<uint32>                       cached_recipes_;
    std::vector<Playerbot::SelfTeleportSpell> cached_self_teleports_;
    std::vector<uint32>                       cached_known_spells_;
    uint32 last_reputation_scan_ms_ = 0;
    uint32 last_skills_scan_ms_     = 0;
    uint32 last_spellcache_scan_ms_ = 0;
    uint32 last_known_spells_scan_ms_ = 0;
    size_t cached_spellmap_size_      = 0;
    uint32 last_mount_scan_ms_        = 0;
    uint32 cached_best_mount_spell_   = 0;
    uint16 cached_mount_riding_skill_ = 0;
    uint32 cached_gcd_probe_spell_    = 0;
    // Phase 4 per-bot snapshot-build scratch (relocated from builder statics).
    int64_t          combat_start_ms_     = 0;
    int64_t          combat_exit_ms_      = 0;
    MoveCorroborate  move_corroborate_{};
    uint64           picker_choice_log_sig_ = 0;
    bool   spellbook_diag_logged_   = false;
    uint8  spellbook_diag_level_    = 0;
    uint64 last_quest_log_signature_ = 0;
    ObjectGuid     last_engage_target_;
    uint32         last_engage_at_ms_ = 0;
    uint32         last_engage_shield_ms_ = 8000;
    // Committed route-waypoint cursor + the map it belongs to (self-invalidates
    // on map change). See dungeon_route_wp() for the anti-oscillation rationale.
    int32_t        dungeon_route_wp_ = -1;
    uint32         dungeon_route_wp_map_ = 0;
    // Route-aware combat-advance reached-crumb latch + its map (self-
    // invalidates on map change). See adv_route_reached_idx() above.
    int32_t        adv_route_reached_idx_ = -1;
    uint32         adv_route_reached_map_ = 0;
    // Route-consumed latch + its map (see route_consumed_idx() above).
    int32_t        route_consumed_idx_ = -1;
    uint32         route_consumed_map_ = 0;
    // Tank chase-commit latch fields (see chase_commit_target() above).
    ObjectGuid     chase_commit_target_;
    uint32         chase_commit_since_ms_     = 0;
    uint32         chase_commit_last_plan_ms_ = 0;
    uint32         chase_commit_map_          = 0;
    // Movement-objective commitment fields (see move_commit_active() above).
    float          move_commit_x_   = 0.f;
    float          move_commit_y_   = 0.f;
    float          move_commit_z_   = 0.f;
    uint32         move_commit_ms_  = 0;   // 0 = no commitment
    uint32         move_commit_map_ = 0;
    // Objective (route-crumb index) the committed move serves; -1 = none/
    // not crumb-based. See move_commit_objective() above (increment 1k).
    int32_t        move_commit_objective_ = -1;
    // PROGRESS-STICKY fields (increment 1m, 2026-07-20). best_d2_ = the
    // smallest squared distance to move_commit_{x,y,z}_ observed since the
    // commitment was (re-)made; FLT_MAX = no baseline sample yet.
    // progress_ms_ = the timestamp that best distance was last improved.
    // See move_commit_note_progress()/move_commit_active() above.
    float          move_commit_best_d2_     = 0.f;
    uint32         move_commit_progress_ms_ = 0;
    // Refused-destination ring (see move_refused_recently() above).
    struct RefusedDst { float x = 0.f, y = 0.f, z = 0.f; uint32 ms = 0; };
    std::array<RefusedDst, 4> move_refused_{};
    size_t                    move_refused_head_ = 0;
    float          dungeon_cross_x_ = 0.f;
    float          dungeon_cross_y_ = 0.f;
    float          dungeon_cross_z_ = 0.f;
    uint32         dungeon_cross_until_ms_ = 0;
    bool           dungeon_cross_direct_ = false;   // DB nav-link crossing (see accessor)
    // Cross EPISODE wall-clock — survives set_dungeon_cross re-commits; reset only
    // when the crossing is genuinely cleared (landed / relocated). See cross_episode_ms.
    uint32         cross_episode_since_ms_ = 0;
    // No-progress strand detector for an in-flight off-mesh crossing (see
    // cross_hold_stuck_ms). Tracks the closest distance achieved toward the cross
    // target; the clock runs from the last time real progress (>=3y closer) was made.
    uint32         cross_hold_since_ms_ = 0;
    float          cross_hold_best_ = 0.f;
    // Dedicated own-position frozen clock for the cross-hold recovery (see
    // cross_frozen_ms) — kept separate from frozen_x_/y_/z_ which the strand
    // recovery owns and resets on cohesion.
    uint32         cross_frozen_since_ms_ = 0;
    float          cross_frozen_x_ = 0.f;
    float          cross_frozen_y_ = 0.f;
    float          cross_frozen_z_ = 0.f;
    // Windowed net-progress detector state (cross_window_noprogress).
    uint32         cross_win_ms_ = 0;
    float          cross_win_d_  = 0.f;
    // Dungeon false-combat lock dwell (see false_combat_ms).
    uint32         false_combat_since_ms_ = 0;
    // In-combat victim acquisition latch (see combat_victim_since_ms).
    ObjectGuid     combat_victim_latch_;
    uint32         combat_victim_latch_since_ms_ = 0;
    // Probe cadence for dungeon:untankable_disengage (see untankable_probe_last_ms).
    uint32         untankable_probe_last_ms_ = 0;
    ObjectGuid     opener_victim_;
    uint32         opener_victim_since_ms_ = 0;
    uint32         opener_own_since_ms_ = 0;   // absolute OOC opener clock
    uint32         opener_last_seen_ms_ = 0;
    bool           opener_victim_is_boss_ = false;
    uint32         cast_oor_count_ = 0;
    ObjectiveTrack objective_track_{0, 0, 0, 0, 0, 0, 0};
    // Committed lateral-reroute side (see commit_lateral_side). 0 = none/cleared,
    // -1 = left, +1 = right. lateral_side_ms_ is the GameTime ms the side was
    // last committed, used for the hold + give-up windows in the rule.
    int8   lateral_side_    = 0;
    uint32 lateral_side_ms_ = 0;
    // Ring buffer of recently shared quest IDs. Dedups the auto-share rule
    // in State_InGroup so a single accepted quest doesn't get re-shared
    // every tick. Single-id tracking would alternate sharing two quests
    // forever (always sees "this quest != last shared one"). Capacity 16
    // covers typical quest log of 25 with rotation tolerance — re-sharing
    // a quest after it falls off the ring is harmless server-side (member
    // either has it or refuses).
    static constexpr size_t kSharedRingCap = 16;
    std::array<uint32, kSharedRingCap> shared_ring_{};
    size_t shared_ring_head_ = 0;
    uint32 last_reagent_try_entry_ = 0;
    uint32 last_reagent_try_ms_    = 0;
    // idle:buy_bag retry cooldown. Many starter-zone vendors don't stock
    // bags; without this, the rule re-fires every tick at the nearest
    // VENDOR npc and dominates the rule order, blocking quest rules from
    // running. Track (npc_guid, ms) so a fresh vendor on the next zone
    // gets a fresh attempt while the just-tried one waits out the timer.
public:
    ObjectGuid last_bag_buy_npc() const { return last_bag_buy_npc_; }
    uint32     last_bag_buy_ms() const { return last_bag_buy_ms_; }
    void       note_bag_buy_try(ObjectGuid g, uint32 ms)
    { last_bag_buy_npc_ = g; last_bag_buy_ms_ = ms; }

    // Atomic vendor-visit FSM state. ITEM_VENDOR_SYSTEM_PLAN.md Phase 2
    // collapses idle:repair / sell_trash / buy_bag / buy_food / buy_bandage
    // / buy_reagent into one rule that walks phases sequentially while the
    // bot is parked at a single vendor. Phases (matches plan bitmask):
    //   0  = idle (not currently visiting)
    //   1  = repair          (bit 0)
    //   2  = sell            (bit 1) - greys + soulbound junk via SellTrash
    //   3  = bag upgrade     (bit 2)
    //   4  = food/drink      (bit 3)
    //   5  = bandage         (bit 4)
    //   6  = reagent         (bit 5)
    //   0xFF = visit done; rule won't re-enter for vendor_visit_lockout
    //
    // npc_low locks the FSM to one NPC so a 200ms snapshot blip that
    // surfaces a different vendor doesn't mid-trip jump the bot to a new
    // NPC. Reset to 0 when phase rolls to 0/0xFF.
    //
    // The 30-second lockout after 0xFF prevents an immediate re-entry when
    // all phases drained but a follow-up snapshot tick still shows the
    // bot at the vendor in range; new needs (e.g., a fresh loot drop that
    // raised the sell bitmask) will still re-arm via vendor_visit_clear().
    static constexpr uint32 kVendorVisitLockoutMs = 30u * 1000u;
    uint8      vendor_visit_phase() const { return vendor_visit_phase_; }
    uint64     vendor_visit_npc_low() const { return vendor_visit_npc_low_; }
    uint32     vendor_visit_until_ms() const { return vendor_visit_until_ms_; }
    void       set_vendor_visit_phase(uint8 p, uint64 npc_low, uint32 now_ms)
    {
        vendor_visit_phase_   = p;
        vendor_visit_npc_low_ = (p == 0 || p == 0xFF) ? 0 : npc_low;
        if (p == 0xFF)
            vendor_visit_until_ms_ = now_ms + kVendorVisitLockoutMs;
    }
    bool       vendor_visit_in_lockout(uint32 now_ms) const
    {
        return vendor_visit_phase_ == 0xFF && now_ms < vendor_visit_until_ms_;
    }
    void       vendor_visit_clear()
    {
        vendor_visit_phase_   = 0;
        vendor_visit_npc_low_ = 0;
        vendor_visit_until_ms_ = 0;
    }

    // Per-founder guild-charter FSM state. Set by BotGuildMgr when a
    // founder is elected (see Fleet/BotGuildMgr.cpp). Read by the
    // idle:guild_charter_drive rule on this bot's idle ticks; cleared
    // on FSM done / abort.
    //
    // Phases (matches Phase A.2 design in docs/GUILD_PLAN.md):
    //   0     = not a founder / no active charter
    //   1     = walk_to_petitioner  (path to UNIT_NPC_FLAG_PETITIONER)
    //   2     = buy_charter         (interact + BotBuyGuildCharter)
    //   3     = seek_signatures     (wander; sign nearby guildless bots)
    //   4     = walk_back_to_petitioner
    //   5     = submit_charter      (BotTurnInGuildCharter)
    //   0xFE  = abort (charter lost / FSM timeout)
    //   0xFF  = done — guild founded successfully
    //
    // `guild_charter_petition_low_` holds the charter item's guid_low
    // once phase 2 completes; phase 3 uses it as the SignPetition target.
    // `guild_charter_name_` holds the reserved name from BotGuildNamePool.
    // `guild_charter_started_ms_` is the FSM birth time; total budget is
    // 30 in-game min before the FSM aborts.
    //
    // `guild_charter_petitioner_low_` is the resolved petitioner NPC's
    // creature guid_low — pinned at FSM start so phase 1 and phase 4
    // walk to the same NPC (no mid-trip jumping between vendors).
    static constexpr uint32 kGuildCharterBudgetMs = 30u * 60u * 1000u;
    uint8      guild_charter_phase() const { return guild_charter_phase_; }
    uint64     guild_charter_petition_low() const { return guild_charter_petition_low_; }
    uint64     guild_charter_petitioner_low() const { return guild_charter_petitioner_low_; }
    std::string const& guild_charter_name() const { return guild_charter_name_; }
    uint32     guild_charter_started_ms() const { return guild_charter_started_ms_; }
    void       set_guild_charter(uint8 phase, std::string name,
                                 uint64 petitioner_low, uint32 now_ms)
    {
        guild_charter_phase_           = phase;
        guild_charter_name_            = std::move(name);
        guild_charter_petitioner_low_  = petitioner_low;
        guild_charter_started_ms_      = now_ms;
        guild_charter_petition_low_    = 0;
    }
    void       advance_guild_charter_phase(uint8 phase)
    {
        guild_charter_phase_ = phase;
    }
    void       set_guild_charter_petition_low(uint64 item_low)
    {
        guild_charter_petition_low_ = item_low;
    }
    void       clear_guild_charter()
    {
        guild_charter_phase_           = 0;
        guild_charter_petition_low_    = 0;
        guild_charter_petitioner_low_  = 0;
        guild_charter_name_.clear();
        guild_charter_started_ms_      = 0;
    }
    bool       guild_charter_active() const
    {
        return guild_charter_phase_ != 0 &&
               guild_charter_phase_ != 0xFE &&
               guild_charter_phase_ != 0xFF;
    }

    // Phase C chat-suite state.
    //   guild_chat_last_ding_level_   - last level the bot announced ding for.
    //     Idle rule fires GuildChatIntent when s.level() > this, then bumps
    //     it. Per-level dedup; ding-on-relog suppressed by stamping at login.
    //   guild_chat_login_greeted_ms_  - 0 until the bot emits its login
    //     greet; non-zero after. Skip greet when re-stamped post-login by
    //     the login flow (handled by clear_guild_chat_state on session
    //     reconstruct).
    //   guild_chat_last_smalltalk_ms_ - timestamp of the last smalltalk
    //     emit from this bot. 30min per-bot cooldown.
    //   guild_chat_last_brag_ms_      - 10min per-bot cooldown on quest brag.
    //   guild_chat_last_lfm_ms_       - per-LFG-session anti-spam on
    //     LFM pulse (30s between pulses).
    uint8  guild_chat_last_ding_level()  const { return guild_chat_last_ding_level_; }
    void   set_guild_chat_last_ding_level(uint8 lvl) { guild_chat_last_ding_level_ = lvl; }
    uint32 guild_chat_login_greeted_ms() const { return guild_chat_login_greeted_ms_; }
    void   note_guild_chat_login_greet(uint32 now_ms) { guild_chat_login_greeted_ms_ = now_ms; }
    uint32 guild_chat_last_smalltalk_ms() const { return guild_chat_last_smalltalk_ms_; }
    void   note_guild_chat_smalltalk(uint32 now_ms) { guild_chat_last_smalltalk_ms_ = now_ms; }
    uint32 guild_chat_last_brag_ms() const { return guild_chat_last_brag_ms_; }
    void   note_guild_chat_brag(uint32 now_ms) { guild_chat_last_brag_ms_ = now_ms; }
    uint32 guild_chat_last_lfm_ms() const { return guild_chat_last_lfm_ms_; }
    void   note_guild_chat_lfm(uint32 now_ms) { guild_chat_last_lfm_ms_ = now_ms; }

    // C.2: per-bot trackers consumed by the brag + lfm_pulse rules.
    //   guild_chat_last_seen_quest_count_  - last observed snapshot value
    //     of `completed_quest_count`. When it ticks up, the brag rule
    //     considers firing (10% probability per delta). 0xFFFF =
    //     unset (first tick stamps without bragging).
    //   guild_chat_lfg_queue_entered_ms_   - timestamp when the bot
    //     entered the LFG queue. 0 = not in queue. Reset to 0 when
    //     snapshot reports not in queue. Set when 0 and snapshot
    //     reports queued. LFM rule fires when current - entered > 2 min.
    uint16 guild_chat_last_seen_quest_count() const { return guild_chat_last_seen_quest_count_; }
    void   set_guild_chat_last_seen_quest_count(uint16 c) { guild_chat_last_seen_quest_count_ = c; }
    uint32 guild_chat_lfg_queue_entered_ms() const { return guild_chat_lfg_queue_entered_ms_; }
    void   set_guild_chat_lfg_queue_entered_ms(uint32 ms) { guild_chat_lfg_queue_entered_ms_ = ms; }
    uint32 guild_chat_last_hangout_ms() const { return guild_chat_last_hangout_ms_; }
    void   note_guild_chat_hangout(uint32 ms) { guild_chat_last_hangout_ms_ = ms; }
    void   clear_guild_chat_state()
    {
        guild_chat_last_ding_level_  = 0;
        guild_chat_login_greeted_ms_ = 0;
        guild_chat_last_smalltalk_ms_ = 0;
        guild_chat_last_brag_ms_      = 0;
        guild_chat_last_lfm_ms_       = 0;
    }
private:
    ObjectGuid last_bag_buy_npc_;
    uint32     last_bag_buy_ms_  = 0;
    uint8      vendor_visit_phase_   = 0;
    uint64     vendor_visit_npc_low_ = 0;
    uint32     vendor_visit_until_ms_ = 0;
    uint8      guild_charter_phase_           = 0;
    uint64     guild_charter_petition_low_    = 0;
    uint64     guild_charter_petitioner_low_  = 0;
    std::string guild_charter_name_;
    uint32     guild_charter_started_ms_      = 0;
    uint8      guild_chat_last_ding_level_    = 0;
    uint32     guild_chat_login_greeted_ms_   = 0;
    uint32     guild_chat_last_smalltalk_ms_  = 0;
    uint32     guild_chat_last_brag_ms_       = 0;
    uint32     guild_chat_last_lfm_ms_        = 0;
    uint16     guild_chat_last_seen_quest_count_ = 0xFFFF;  // 0xFFFF = unset (first-tick will stamp)
    uint32     guild_chat_lfg_queue_entered_ms_  = 0;       // 0 = not in queue
    uint32     guild_chat_last_hangout_ms_       = 0;       // ad-hoc tavern hangout proposer
    // The five quest-memory maps below are accessed from TWO threads: the
    // snapshot builder (world thread) READS them, and State_Idle dispatch
    // (worker thread) WRITES them (and the builder also writes quest_impossible_
    // / giver_no_offers_). A prior comment assumed "single writer = safe", but a
    // writer's insert can REHASH the bucket array mid-find on the reader →
    // null-bucket ACCESS_VIOLATION (crash 2026-06-15 at pop=500, in
    // objective_blacklist_.find). quest_mem_mtx_ serializes EVERY access to the
    // five cross-thread maps: objective_blacklist_, quest_blacklist_until_ms_,
    // cant_take_quest_until_ms_, giver_no_offers_until_ms_, quest_impossible_.
    // Critical sections are tiny (find/insert/erase); the lock is per-bot so
    // contention is limited to a single bot's builder-vs-worker overlap.
    mutable std::mutex quest_mem_mtx_;

    // quest_id → ms-of-day-when-cooldown-expires. Guarded by quest_mem_mtx_.
    std::unordered_map<uint32, uint32> quest_blacklist_until_ms_;
    // Parallel map for the short "CanTakeQuest recently failed" cache.
    // Same threading model as quest_blacklist_until_ms_.
    std::unordered_map<uint32, uint32> cant_take_quest_until_ms_;
    std::unordered_map<uint32, uint32> giver_no_offers_until_ms_;
    // Permanent class/race-impossible quests — see note_quest_impossible.
    // NOT cleared on quest-log mutation (the gates are immutable for this bot).
    std::unordered_map<uint32, uint8> quest_impossible_;
    // Per-quest reward-turnin failure backoff (see note_quest_reward_failed).
    // Same single-writer (worker thread) threading model as the maps above.
    struct QuestRewardFail { uint32 until_ms = 0; uint8 fails = 0; };
    std::unordered_map<uint32, QuestRewardFail> quest_reward_fail_;
    uint32 last_blacklist_prune_ms_ = 0;
    // Diagnostic throttle stamp for the `[quest_no_offers]` log line in
    // BotSnapshotBuilder. Single uint32 since the diagnostic logs once
    // per bot per 5s regardless of quest_id.
    uint32 last_quest_diag_ms_ = 0;
    uint32 last_moving_ms_     = 0;     // for idle:ambient_sit sustained-idle gate
    // Throttle for the `[wander_reason]` diagnostic — 1 line per bot
    // per 60s. Wander fires ~1000+/min fleet-wide.
    uint32 last_wander_diag_ms_ = 0;
    // Cached corpse → graveyard distance. Recomputing every snapshot tick
    // walks `WorldSafeLocsEntry` against `sConditionMgr` for every nearby
    // graveyard, evaluating per-graveyard team / faction / level conditions.
    // For a dead bot with multiple snapshots/sec that's hundreds of
    // condition evaluations and a flood of TC_LOG_DEBUG("condition") lines.
    // The corpse position is fixed for the lifetime of the corpse, so we
    // cache (corpse_x, corpse_y, corpse_z, distance). Builder reuses the
    // cached value when the snapshot's corpse position matches; recomputes
    // (and updates the cache) when the corpse moves (release/reclaim) or
    // when a fresh death occurs.
public:
    struct CachedGraveDist
    {
        bool   valid = false;
        uint32 map_id = 0;
        float  corpse_x = 0.f;
        float  corpse_y = 0.f;
        float  corpse_z = 0.f;
        float  distance = 0.f;
    };
    CachedGraveDist const& cached_grave_dist() const { return cached_grave_dist_; }
    void set_cached_grave_dist(uint32 map_id, float cx, float cy, float cz, float dist)
    {
        cached_grave_dist_ = CachedGraveDist{true, map_id, cx, cy, cz, dist};
    }
    void clear_cached_grave_dist() { cached_grave_dist_ = CachedGraveDist{}; }

    // Optimistic per-spell emit cooldown. Modern WoW puts most abilities on
    // a server-side cooldown the moment they cast — but the snapshot that
    // reflects that cooldown is published on the next world tick (~200 ms
    // later). Between emit and snapshot reflection, the AI worker may tick
    // again on the SAME stale snapshot and re-emit the same cast intent;
    // the API rejects with SPELL_FAILED_NOT_READY (91) and the rule fires
    // again next tick. Result: 4-5 redundant emits per real cooldown cycle,
    // each producing a Server.log rejection line.
    //
    // Lock the spell out on the AI side for ~1.5 s after emit (covers the
    // GCD plus snapshot publish lag). The next snapshot's real cooldown
    // table catches up before the lockout expires, so the AI never sees
    // a "ready" window the server disagrees with.
    static constexpr uint32 kCastEmitLockoutMs       = 1500u;
    // Longer back-off used when the API REJECTED a cast (vs the short
    // 1.5s GCD-race lockout). Covers SPELL_FAILED_BAD_TARGETS (no
    // valid target), SPELL_FAILED_NOT_READY (server CD that the
    // snapshot didn't see), SPELL_FAILED_LINE_OF_SIGHT, etc. After
    // 10s the rule may try again — by then either the target moved
    // into range / a fresh target appeared / the CD elapsed, or
    // the rule should pick a different spell.
    //
    // Audit 2026-05-17: 329k cast rejections in one log file. Top
    // offenders (Soulstone, Create Healthstone, Revive Pet, Arcane
    // Intellect, Prayer of Mending) all churn at the 1.5s lockout
    // floor. The 10s back-off cuts the duty cycle 6×.
    static constexpr uint32 kCastRejectLockoutMs = 10000u;
    void note_cast_emitted(uint32 spell_id, uint32 now_ms)
    {
        if (spell_id == 0) return;
        cast_emit_until_ms_[spell_id] = now_ms + kCastEmitLockoutMs;
    }
    void note_cast_rejected(uint32 spell_id, uint32 now_ms)
    {
        if (spell_id == 0) return;
        const uint32 longer = now_ms + kCastRejectLockoutMs;
        auto& slot = cast_emit_until_ms_[spell_id];
        if (slot < longer) slot = longer;
    }
    bool cast_recently_emitted(uint32 spell_id, uint32 now_ms) const
    {
        if (spell_id == 0) return false;
        auto const it = cast_emit_until_ms_.find(spell_id);
        if (it == cast_emit_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    // Periodic prune; called from State_Idle's prune cadence to bound the
    // map size for long-lived bots.
    void prune_cast_emit_cache(uint32 now_ms)
    {
        for (auto it = cast_emit_until_ms_.begin(); it != cast_emit_until_ms_.end();)
        {
            if (now_ms >= it->second) it = cast_emit_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // Per-bot last-move-to dedup. State_Idle rules (idle:wander, idle:travel_
    // to_hub, idle:wander_to_quest_hub, idle:wander_to_service, etc.) re-fire
    // every snapshot tick (~250-500ms) and re-emit MoveToIntent with nearly-
    // identical coordinates. PlayerbotAPI::move_to ultimately calls
    // MotionMaster::MovePoint, which RESETS the spline and triggers a fresh
    // Detour pathfind. Re-issuing the same dest every tick produces visible
    // stutter (bot starts → stops → replans → starts) and — critically —
    // breaks Detour's path-around-obstacle logic: by the time Detour curves
    // away from a wall, the spline is reset before the bot reaches the
    // curving segment, and the next tick the bot collides head-on with the
    // wall it should have walked around.
    //
    // Dedup gate: if a recent move_to went to ~the same XYZ (< 3y), drop the
    // new emit silently. Skipping a meaningful re-target (dest changed > 3y)
    // is fine — that's a real new path. Skipping during a stall (bot stuck,
    // rule retries same dest) is also fine — re-emitting the same MovePoint
    // can't fix being wedged; that needs the StuckTracker → Tier 1+ escalation.
    static constexpr uint32 kMoveToEmitLockoutMs = 1500u;
    static constexpr float  kMoveToDedupRadiusSq = 9.0f;  // 3.0y squared
    void note_move_to_emitted(float x, float y, float z, uint32 now_ms)
    {
        last_move_to_x_      = x;
        last_move_to_y_      = y;
        last_move_to_z_      = z;
        last_move_to_until_  = now_ms + kMoveToEmitLockoutMs;
        last_move_to_valid_  = true;
    }
    bool move_to_recently_emitted(float x, float y, float z, uint32 now_ms) const
    {
        if (!last_move_to_valid_) return false;
        if (now_ms >= last_move_to_until_) return false;
        const float dx = x - last_move_to_x_;
        const float dy = y - last_move_to_y_;
        const float dz = z - last_move_to_z_;
        return (dx*dx + dy*dy + dz*dz) < kMoveToDedupRadiusSq;
    }
    // Dungeon regroup convergence tracking — see idle:dungeon_regroup_follow_tank.
    // A follower that is genuinely moving (is_moving=true) but whose distance to
    // the tank is flat/increasing across a real interval is being driven by a
    // stale/wrong mover (e.g. a destination that survived a teleport); the bare
    // !is_moving gate can never see that. Judged over >=kRegroupSampleMs so a
    // freshly issued path isn't fought mid-spline; kRegroupDivergeEps2 (~2y)
    // tolerates the lateral drift of rounding a corner.
    static constexpr uint32 kRegroupSampleMs    = 1000u;
    static constexpr float  kRegroupDivergeEps2 = 4.0f;  // ~2.0y squared
    float  last_regroup_dist2() const { return last_regroup_dist2_; }
    uint32 last_regroup_ms()    const { return last_regroup_ms_; }
    void   note_regroup_sample(float dist2, uint32 now_ms)
    { last_regroup_dist2_ = dist2; last_regroup_ms_ = now_ms; }
    void   reset_regroup_tracking() { last_regroup_dist2_ = 0.f; last_regroup_ms_ = 0; }

    // ---- stranded-follower stuck clock (mirrors cross_hold_stuck_ms) ----
    // A dungeon follower that comes to rest where EVERY path back to the tank
    // NoPaths — chased a mob onto a navmesh-disconnected perch (the FoeReaper-
    // descent z59 ledge), fell into a void pocket, or any strand the regroup
    // move_to is refused from — accumulates dwell here. best-distance semantics:
    // closing >=3y restarts the clock, so a follower that is simply WALKING a
    // long corridor (steadily closing) never trips it; only a bot that cannot
    // get meaningfully closer for the whole window registers as stuck. The
    // caller relocates once this exceeds the timeout AND a path to the tank is
    // confirmed unreachable. dist = current 3D distance to the tank.
    uint32 regroup_stuck_ms(float dist, uint32 now_ms)
    {
        if (regroup_stuck_since_ms_ == 0 || dist < regroup_stuck_best_ - 3.0f)
        {
            regroup_stuck_since_ms_ = now_ms ? now_ms : 1u;
            regroup_stuck_best_     = dist;
            return 0;
        }
        return now_ms - regroup_stuck_since_ms_;
    }
    void   regroup_stuck_reset() { regroup_stuck_since_ms_ = 0; }

    // ---- own-position FROZEN clock (rally-independent) ----
    // How long THIS bot's own position has not moved beyond ~3y. Unlike
    // regroup_stuck_ms (which tracks distance to the rally), this is immune to a
    // MOVING rally: the strand-recovery medoid is the group's centre of mass, so
    // while the group descends/advances the rally itself moves, making
    // dist-to-rally fluctuate and resetting regroup_stuck_ms every time the rally
    // happens to step closer — which masked a member genuinely frozen in place
    // (the mage stuck at the entrance the WHOLE descent while the group moved off,
    // 2026-06-27). A frozen straggler is detected by its OWN lack of motion
    // regardless of where the group is. A bot that is actually walking (own
    // position changing) never accumulates, so a long corridor walk is never cut.
    uint32 frozen_ms(float x, float y, float z, uint32 now_ms)
    {
        const float dx = x - frozen_x_, dy = y - frozen_y_, dz = z - frozen_z_;
        if (frozen_since_ms_ == 0 || (dx*dx + dy*dy + dz*dz) > 3.0f * 3.0f)
        {
            frozen_since_ms_ = now_ms ? now_ms : 1u;
            frozen_x_ = x; frozen_y_ = y; frozen_z_ = z;
            return 0;
        }
        return now_ms - frozen_since_ms_;
    }
    void   frozen_reset() { frozen_since_ms_ = 0; }

    // ---- converge-to-fight (combat-join) stuck clock ----
    // How long this bot has CONTINUOUSLY satisfied the "stranded out of the
    // tank's active fight" condition: the tank is in combat with a real victim,
    // this bot is NOT in combat, and it sits beyond its own attack range of the
    // tank. Crucially this clock is INDEPENDENT of the strand clocks above,
    // which the cohesion early-return zeroes for any member inside 40y — so it
    // is the only signal that survives the 37-40y dead-band where a lagging DPS
    // can neither rejoin (>40y gate) nor strand-relocate (>40y gate) yet still
    // cannot reach the tank's victim on its own. That dead-band let the tank
    // SOLO a shielded Defias Envoker for 30+ minutes while three idle DPS stood
    // ~40y back (Deadmines Helix approach, 2026-06-27). A short dwell avoids
    // yanking a ranged DPS that is merely between casts (briefly out of combat
    // but in range / actively repositioning); a genuinely stranded bot stays
    // out of combat for the whole window and trips it.
    uint32 combat_join_stuck_ms(bool stuck_now, uint32 now_ms)
    {
        if (!stuck_now) { combat_join_since_ms_ = 0; return 0; }
        if (combat_join_since_ms_ == 0)
        { combat_join_since_ms_ = now_ms ? now_ms : 1u; return 0; }
        return now_ms - combat_join_since_ms_;
    }
    void   combat_join_reset() { combat_join_since_ms_ = 0; }
    // Last emitted move_to destination, if one was issued within the emit
    // window. The snapshot builder uses this as the StuckTracker's goal when
    // no quest goal exists, so wander/travel/vendor/portal/dock/corpse
    // wedges feed the recovery ladder (they previously did not). The 1.5s
    // window means a wedged bot — which re-emits the same move_to roughly
    // every lockout interval — keeps the goal fresh, while a bot that has
    // moved on naturally lets it lapse.
    bool last_move_to_goal(float& x, float& y, float& z, uint32 now_ms) const
    {
        if (!last_move_to_valid_ || now_ms >= last_move_to_until_) return false;
        x = last_move_to_x_; y = last_move_to_y_; z = last_move_to_z_;
        return true;
    }
    // Stuck-by-terrain tracker. Updated by the snapshot builder each tick:
    // when the bot has a movement goal (POI / quest_turnin / wander target),
    // the builder records (target, last_distance, ticks_no_progress). The
    // idle:unstick rule reads this and escalates recovery in tiers:
    //   Tier 0 (< 5 s no progress): no action, bot may yet make progress
    //   Tier 1 (5-15 s):           JumpIntent — often unsticks minor wedges
    //   Tier 2 (15-40 s):          NearTeleportToIntent with random offset
    //   Tier 3 (40-60 s):          clear current_objective, force wander
    //   Tier 4 (> 60 s):           hearth home (or release-corpse fallback)
    // Reset whenever distance drops by >= 5 y (real progress) or the goal
    // changes (different target). Stays within BotAI for thread-safety:
    // builder reads/writes from the world thread; AI worker reads to drive
    // the rule. Eventually-consistent — a stale read just delays escalation
    // by one tick.
    struct StuckTracker
    {
        bool     active = false;
        uint32   target_map = 0;
        float    target_x = 0.f, target_y = 0.f, target_z = 0.f;
        float    last_distance = 0.f;
        uint32   no_progress_ticks = 0;
        uint32   first_no_progress_ms = 0;
        uint32   last_recovery_ms = 0;
        uint8    recovery_tier = 0;
        // Goal provenance. true  = a quest POI / turn-in goal (Tier 3 drops it
        // via blacklist_quest). false = a plain movement goal (the bot's last
        // emitted move_to target — wander/travel/vendor/portal/dock/corpse);
        // for those Tier 3 suppresses the offending rule instead of touching
        // any quest. Lets the walk-first ladder serve non-quest wedges too.
        bool     goal_is_quest = false;
    };
public:
    StuckTracker const& stuck_tracker() const { return stuck_; }
    StuckTracker&       mutable_stuck_tracker() { return stuck_; }
    void                reset_stuck_tracker() { stuck_ = StuckTracker{}; }

    // ---- #1B WedgeWatchdog support (world-thread only) ----
    // Thin, decoupled "is this bot stuck right now, and for how long" query
    // for the runtime wedge watchdog (Diagnostics/WedgeWatchdog). Reads only
    // cheap BotAI-local fields written world-thread by the snapshot builder
    // (StuckTracker + the goal-agnostic oscillation leash). Returns true when
    // EITHER wedge signal has persisted for at least `threshold_ms`:
    //   1. StuckTracker no-progress: the bot has an unreached goal and has
    //      failed to close distance for first_no_progress_ms..now. (Goal-level
    //      wedge — quest POI / turn-in / movement target it can't reach.)
    //   2. Oscillation leash: the bot keeps wanting to travel but never
    //      escapes its anchor radius (survives goal flips — the Uraimus /
    //      Falin run-back-and-forth signature StuckTracker alone misses
    //      because each goal flip resets its counter).
    // `wedge_since_ms(now)` returns the earliest wedge-onset timestamp across
    // the two signals (0 when not wedged) so the watchdog can compute episode
    // duration without re-deriving it. Both are const; the watchdog never
    // mutates BotAI.
    uint32 wedge_since_ms(uint32 now_ms) const
    {
        uint32 onset = 0;
        // StuckTracker signal: active goal + accumulated no-progress ticks.
        // no_progress_ticks is a ~5/sec count; >=50 ticks ≈ 10s of no
        // progress confirms the wedge before we even look at the clock.
        if (stuck_.active && stuck_.no_progress_ticks >= 50 &&
            stuck_.first_no_progress_ms != 0)
            onset = stuck_.first_no_progress_ms;
        // Oscillation-leash signal: anchored + ticking without escape. Onset
        // is the anchor timestamp. ~50 ticks (10s) confirms thrash, matching
        // the StuckTracker gate above.
        if (osc_anchor_valid_ && osc_stuck_ticks_ >= 50 && osc_anchor_ms_ != 0)
        {
            if (onset == 0 || osc_anchor_ms_ < onset)
                onset = osc_anchor_ms_;
        }
        if (onset == 0 || onset > now_ms) return 0;
        return onset;
    }
    bool is_wedged(uint32 threshold_ms, uint32 now_ms) const
    {
        const uint32 onset = wedge_since_ms(now_ms);
        return onset != 0 && (now_ms - onset) >= threshold_ms;
    }
    // Coarse objective provenance for the wedge report. The human-readable
    // objective text (quest id / hub) lives in the snapshot; this returns the
    // BotAI-local provenance the watchdog appends to it: which goal kind the
    // StuckTracker is currently anchored on. Cheap + const — no allocation
    // beyond the returned literal-backed string.
    std::string wedge_objective_string() const
    {
        // Kept free of fmt/<sstream> so this widely-included header stays
        // light; integer-truncate the target coords with std::to_string.
        if (stuck_.active)
        {
            std::string out = stuck_.goal_is_quest ? "quest_goal@(" : "move_goal@(";
            out += std::to_string(static_cast<long>(stuck_.target_x));
            out += ',';
            out += std::to_string(static_cast<long>(stuck_.target_y));
            out += ')';
            return out;
        }
        if (osc_anchor_valid_)
            return "oscillation";
        return "(none)";
    }

    // ---- R7: sticky Chromie-time leveling-zone relocation target ----
    // When a bot out-levels its zone (no local quest work) the snapshot
    // builder picks a level-appropriate quest hub — often cross-continent —
    // via QuestHubDatabase::SelectLevelingHub and parks the choice HERE so it
    // stays stable across ticks (and, via the 0010 DB columns, across
    // restarts). hub_id==0 = unset. The choice is re-picked only with
    // hysteresis: when the bot's level leaves [bracket_lo, bracket_hi] OR the
    // stored hub's continent is no longer band-appropriate (a band transition,
    // e.g. 59→60 Classic→Dragon Isles). loaded_ guards the one-shot DB read at
    // login so the builder doesn't treat an un-hydrated target as "never
    // picked" and clobber a persisted choice.
    struct LevelingZoneTarget
    {
        uint32 hub_id     = 0;
        uint32 map_id     = 0;
        uint8  bracket_lo = 0;
        uint8  bracket_hi = 0;
        uint64 chosen_at  = 0;   // unix seconds (0 = unset); diagnostics only
    };
    LevelingZoneTarget const& leveling_target() const { return leveling_target_; }
    void set_leveling_target(LevelingZoneTarget const& t)
    { leveling_target_ = t; leveling_target_loaded_ = true; }
    bool leveling_target_loaded() const { return leveling_target_loaded_; }
    void mark_leveling_target_loaded() { leveling_target_loaded_ = true; }

    // R7 island-escape: cached "does reaching this SAME-MAP leveling hub need a
    // non-walk bridge (areatrigger teleport / ship / portal)?" decision, keyed
    // by the hub goal so the builder runs the FindRoute probe only once per goal
    // change (not per tick). POD (uint64+bool) written + read ONLY on the world
    // thread (the snapshot builder) — never touched by the AI worker, so unlike
    // the travel_plan_ vector it needs no cross-thread synchronization. The AI
    // worker sees the result purely through the snapshot's objective_is_relocation
    // flag the builder sets from it.
    uint64 reloc_bridge_goal_key() const { return reloc_bridge_goal_; }
    bool   reloc_bridge_has()      const { return reloc_bridge_has_; }
    uint32 reloc_bridge_at_ms()    const { return reloc_bridge_at_ms_; }
    void   set_reloc_bridge_decision(uint64 goal_key, bool has, uint32 now_ms)
    { reloc_bridge_goal_ = goal_key; reloc_bridge_has_ = has; reloc_bridge_at_ms_ = now_ms; reloc_bridge_legs_.clear(); }

    // ---- CombatLoop FIX C: sticky scan-MISS POI-fallback spawn ----
    // When the builder resolves a quest-target spawn as the objective POI
    // fallback (no quest_poi blob matched), it uses the NEAREST same-map spawn
    // of the target entry. At near-equidistant spawns that selection could flip
    // build-to-build, oscillating the bot's destination. This caches the last
    // selected spawn position keyed by (quest_id, obj_id, entry); the builder
    // keeps the cached spawn unless a different one is closer by a margin > the
    // arrive radius (40y). World-thread only (written + read by the snapshot
    // builder, like reloc_bridge_*), so no synchronization is needed.
    struct PoiSpawnSticky { uint32 quest_id = 0; uint32 obj_id = 0; uint32 entry = 0;
                            uint32 map_id = 0; float x = 0.f; float y = 0.f; float z = 0.f;
                            bool valid = false; };
    PoiSpawnSticky const& poi_spawn_sticky() const { return poi_spawn_sticky_; }
    void set_poi_spawn_sticky(PoiSpawnSticky const& s) { poi_spawn_sticky_ = s; }

    // ---- L2/L3: cached cross-map / cross-region TravelPlan ----
    // The full multi-modal route (UnifiedTravelGraph::FindRoute: walk/taxi/ship/
    // portal/teleport/elevator legs) is materialized ONCE per goal and cached
    // here, instead of being recomputed and discarded every snapshot tick (the
    // old travel_plan rule replanned from greedy anchors every tick → fragile,
    // non-deterministic multi-hop, and let the legacy cascade pick a WRONG
    // anchor — e.g. a Night Elf routed to the Exodar boat). State_Idle executes
    // it leg-by-leg, advancing current leg as each leg's destination is reached;
    // the legacy greedy cascade remains the fallback when no plan exists. Slim
    // POD legs keep this header free of the Travel includes.
    struct PlanLeg
    {
        uint8  kind   = 0;        // V2::Travel::EdgeKind: 1 Walk 2 Taxi 3 Portal
                                  //   4 Ship 5 Hearth 6 Teleport 7 Elevator
        uint32 to_map = 0;
        float  to_x = 0.f, to_y = 0.f, to_z = 0.f;     // leg destination (node)
        float  from_x = 0.f, from_y = 0.f, from_z = 0.f; // leg source node world pos
        uint32 payload = 0;       // taxipath id / transport-template id / portal GO
        uint32 to_taxi_node = 0;  // Taxi legs: destination TaxiNodesEntry id
    };
    bool   has_travel_plan() const
    { return !travel_plan_.empty() && travel_plan_leg_ < travel_plan_.size(); }
    uint64 travel_plan_goal_key() const { return travel_plan_goal_; }
    std::vector<PlanLeg> const& travel_plan() const { return travel_plan_; }
    size_t travel_plan_leg_index() const { return travel_plan_leg_; }
    PlanLeg const* current_travel_leg() const
    { return has_travel_plan() ? &travel_plan_[travel_plan_leg_] : nullptr; }
    void   set_travel_plan(std::vector<PlanLeg> legs, uint64 goal_key)
    { travel_plan_ = std::move(legs); travel_plan_leg_ = 0; travel_plan_goal_ = goal_key; }
    void   advance_travel_leg() { if (travel_plan_leg_ < travel_plan_.size()) ++travel_plan_leg_; }
    void   clear_travel_plan() { travel_plan_.clear(); travel_plan_leg_ = 0; travel_plan_goal_ = 0; }
    // Leg-skip rate-limit (harbor fix): when anchor-wedged on an unreachable
    // WALK leg, the bot advances to the next leg instead of replanning the same
    // bad route. The anchor-wedge cooldown returns "wedged" every tick for 30s,
    // so this timestamp gates the skip to once per few seconds — letting the bot
    // start moving toward the skipped-to leg before another skip is considered.
    // AI-worker-only (like travel_plan_): no cross-thread synchronization.
    uint32 last_leg_skip_ms() const { return last_leg_skip_ms_; }
    void   set_last_leg_skip_ms(uint32 ms) { last_leg_skip_ms_ = ms; }
    // One-shot escalation: the next driveTravelPlanTo for ANY goal routes
    // through graph A* (RouteRequest::skip_trivial). Armed by the executor
    // when a single-direct-walk plan wedges; cleared when a plan plants.
    bool   travel_force_graph() const { return travel_force_graph_; }
    void   set_travel_force_graph(bool v) { travel_force_graph_ = v; }

    // The reloc-bridge probe's VALIDATED route legs (source attaches checked
    // against the live navmesh). World-thread only, like the reloc_bridge
    // decision POD: written by the probe, read by the builder when copying
    // into the snapshot's bridge_route. The AI worker consumes them via the
    // snapshot — it must NOT recompute the route itself (its FindRoute can't
    // validate attaches off the world thread, so it would get the
    // unexecutable euclidean route a wedged bot can't follow).
    std::vector<PlanLeg> const& reloc_bridge_legs() const { return reloc_bridge_legs_; }
    void set_reloc_bridge_legs(std::vector<PlanLeg> legs)
    { reloc_bridge_legs_ = std::move(legs); }

    // Zero-yield combat watchdog accessors (see victim_watch_* fields).
    ObjectGuid victim_watch_guid() const     { return victim_watch_guid_; }
    int32      victim_watch_hp() const       { return victim_watch_hp_; }
    uint32     victim_watch_since_ms() const { return victim_watch_since_ms_; }
    void set_victim_watch(ObjectGuid g, int32 hp, uint32 now_ms)
    { victim_watch_guid_ = g; victim_watch_hp_ = hp; victim_watch_since_ms_ = now_ms; }

    // ---- Unkillable-target leash (flag-independent immortal-mob catch) ----
    // The no_xp/pacified disengage relies on creature flags, which some Training
    // Dummies carry incorrectly (entry 44820: template unit_flags has PACIFIED
    // but neither the live unit nor the difficulty-merged template expose it, so
    // every flag check reads false). The victim_watch HP timer is separately
    // defeated by a dummy whose HP oscillates (each landed hit resets it). This
    // leash is purely behavioural: track time on the SAME victim (reset only on
    // victim change) plus the victim's HP when the fight began; if after the
    // window the victim has lost no meaningful HP, it is immortal / evade-
    // leashing and can never be killed -> disengage with a long engage shield so
    // the pickers don't re-acquire it. See State_InCombat.
    ObjectGuid combat_stuck_victim() const     { return combat_stuck_victim_; }
    int32      combat_stuck_hp() const         { return combat_stuck_hp_; }
    uint32     combat_stuck_since_ms() const   { return combat_stuck_since_ms_; }
    void set_combat_stuck(ObjectGuid g, int32 hp, uint32 now_ms)
    { combat_stuck_victim_ = g; combat_stuck_hp_ = hp; combat_stuck_since_ms_ = now_ms; }

    // ---- Oscillation leash (goal-agnostic wedge detector) ----
    // StuckTracker resets its no-progress counter every time the goal changes.
    // A bot that thrashes between two unreachable goals (e.g. a quest objective
    // it can't path to and a turn-in giver) keeps switching goals, so the
    // counter never accumulates and recovery never fires — it just "runs back
    // and forth" forever (Falin/Varothel in Dolanaar; Uraimus above the den).
    // This leash is INDEPENDENT of the goal: it anchors on the bot's position
    // and counts ticks the bot fails to escape a radius while it still WANTS to
    // travel elsewhere. Survives goal switches because it's not in StuckTracker.
    void note_position_leash(float x, float y, uint32 now_ms, bool wants_to_travel)
    {
        constexpr float kEscapeR2 = 45.0f * 45.0f;   // must exceed the observed
                                                     // ~37y oscillation amplitude
        if (!wants_to_travel)
        {
            osc_anchor_valid_ = false;               // at-goal / in-combat / idle:
            osc_stuck_ticks_  = 0;                   // not a travel wedge
            return;
        }
        const float dx = x - osc_anchor_x_, dy = y - osc_anchor_y_;
        if (!osc_anchor_valid_ || (dx*dx + dy*dy) > kEscapeR2)
        {
            osc_anchor_x_ = x; osc_anchor_y_ = y;
            osc_anchor_ms_ = now_ms; osc_stuck_ticks_ = 0;
            osc_anchor_valid_ = true;
        }
        else
        {
            ++osc_stuck_ticks_;                       // moving, but not escaping
        }
    }
    uint32 osc_stuck_ticks() const { return osc_anchor_valid_ ? osc_stuck_ticks_ : 0; }
    void   reset_position_leash() { osc_anchor_valid_ = false; osc_stuck_ticks_ = 0; }

    // ---- Food-buy backoff ----
    // Real players almost never BUY food — they eat what drops while questing
    // and grinding. Proactive buying was firing on every vendor visit, piling
    // up 9 stacks (180 units) that filled the bags, which in turn tripped the
    // bag-full sell-vendor need and made the bot oscillate to/from vendors in
    // capitals (Uraimus in Darnassus). Allow at most one food top-up per 30 min
    // so a genuinely starved bot can still restock, but it can never pile up.
    bool food_buy_off_cooldown(uint32 now_ms) const
    { return last_food_buy_ms_ == 0 || (now_ms - last_food_buy_ms_) >= 1800000u; }
    void note_food_buy(uint32 now_ms) { last_food_buy_ms_ = now_ms; }

    // ---- #4B-1 Part 2: per-item AH sell-price memory ----
    //
    // Real sellers learn from the market: a listing that expires UNSOLD next
    // time gets cheaper; one that sells fast can hold or creep up. The static
    // quality bands in AuctionRules give a one-shot price with no feedback, so
    // a bot that over-prices a slow item re-posts it at the same too-high ask
    // forever (it expires, mails back, re-lists identically — never clears).
    //
    // This memo, keyed by item_entry, accumulates the unsold-relist streak and
    // the last-listed timestamp. The sell rule calls note_listed() when it
    // posts and, on the NEXT post of the same entry, calls
    // observe_prior_outcome() with what it sees in snapshot.auctions_owned:
    //   - an owned listing of this entry that expired with no bidder => UNSOLD
    //     (bump the streak; price_adjust_pct() returns a discount).
    //   - no owned listing of this entry but we DID list it recently => it
    //     either sold or mailed back; treated as "cleared" (decay the streak;
    //     price_adjust_pct() returns a small premium when the streak is 0).
    // The adjustment is a percentage applied ABOVE the hard quality floor in
    // AuctionRules, so the floor still clamps the discount — the loop can never
    // dump an item below its band floor regardless of how many times it
    // expires. Single-writer (AI worker thread), same model as the other
    // per-bot retry caches below.
    struct SellPriceMemo
    {
        uint32 last_listed_ms        = 0;
        uint16 relisted_unsold_count = 0;   // consecutive expire-unsold streak
    };
    // Record that we just (re)listed `entry`.
    void note_item_listed(uint32 entry, uint32 now_ms)
    {
        SellPriceMemo& m = sell_price_memo_[entry];
        m.last_listed_ms = now_ms;
    }
    // Fold a previous-listing outcome into the streak. `expired_unsold` is true
    // when an owned listing of this entry expired with no bidder; false means
    // it cleared (sold or no longer present). No-op if we never listed it.
    void observe_item_sale_outcome(uint32 entry, bool expired_unsold)
    {
        auto it = sell_price_memo_.find(entry);
        if (it == sell_price_memo_.end()) return;
        if (expired_unsold)
        {
            if (it->second.relisted_unsold_count < 12)
                ++it->second.relisted_unsold_count;
        }
        else
        {
            it->second.relisted_unsold_count = 0;
        }
    }
    // Signed percent to apply to the computed buyout BEFORE the floor clamp.
    // Negative = discount (item keeps expiring), positive = premium (cleared
    // fast, headroom to ask more). Each unsold relist shaves 8% (capped at
    // -48%); a clean clear lets the next ask sit 5% above band. `reseller_bias`
    // makes Resellers undercut a touch harder and Hoarders hold a higher floor.
    int32 sell_price_adjust_pct(uint32 entry, int32 reseller_bias) const
    {
        auto it = sell_price_memo_.find(entry);
        const uint16 streak = (it == sell_price_memo_.end()) ? 0 : it->second.relisted_unsold_count;
        int32 pct;
        if (streak == 0)
            pct = 5;                                   // fresh / just cleared — slight premium
        else
            pct = -std::min<int32>(48, int32(streak) * 8);   // each miss: -8%, floor -48%
        return pct + reseller_bias;
    }
    bool item_listed_recently(uint32 entry, uint32 now_ms, uint32 within_ms) const
    {
        auto it = sell_price_memo_.find(entry);
        if (it == sell_price_memo_.end() || it->second.last_listed_ms == 0) return false;
        return (now_ms - it->second.last_listed_ms) < within_ms;
    }

    // Follow-progress tracker. ingroup:follow_recall emits MotionMaster
    // MoveFollow, which never reports "leader unreachable" — a follower
    // separated from its leader by a gap / closed door re-emits follow
    // forever (grouped bots are also exempt from GlobalStuckRescue). This
    // detects "distance to the leader isn't dropping" so the rule can fall
    // back from follow to an explicit, navmesh-validated move_to toward the
    // leader (which then engages the walk-first unstick ladder). No teleport.
    // Single-writer (AI worker) — same threading model as the wedge maps.
    struct FollowProgress { uint64 anchor = 0; float last_dist = 0.f; uint32 stale_ticks = 0; uint32 last_repath_ms = 0; };
    // Throttle for the validated follow-repath move_to. The follow rule fires
    // every tick; without this a follower whose leader is unreachable would
    // emit a full pathfind (and a [move_blocked] log line) 5×/sec forever
    // (observed: follow_recall_repath was 75% of all move_blocked). Returns
    // true at most once per `interval_ms`; between, the caller uses the cheap
    // MotionMaster MoveFollow instead.
    bool follow_repath_due(uint32 now_ms, uint32 interval_ms)
    {
        if (now_ms - follow_progress_.last_repath_ms < interval_ms) return false;
        follow_progress_.last_repath_ms = now_ms;
        return true;
    }
    // Returns the running count of consecutive emits with no meaningful
    // progress (≥2y closer) toward `anchor`. Resets on anchor change or
    // real progress.
    uint32 note_follow_dist(uint64 anchor, float dist)
    {
        if (follow_progress_.anchor != anchor)
        {
            follow_progress_.anchor      = anchor;
            follow_progress_.last_dist   = dist;
            follow_progress_.stale_ticks = 0;
            return 0;
        }
        if (dist + 2.0f < follow_progress_.last_dist)   // got ≥2y closer
        {
            follow_progress_.last_dist   = dist;
            follow_progress_.stale_ticks = 0;
            return 0;
        }
        if (dist < follow_progress_.last_dist)           // tiny progress — track best
            follow_progress_.last_dist = dist;
        return ++follow_progress_.stale_ticks;
    }
    void reset_follow_progress() { follow_progress_ = FollowProgress{}; }

    // ---- Detour lease ----
    // Purposeful idle moves (loot a corpse, skin, walk to an in-dungeon
    // quest giver) grant a short lease; ingroup:follow_recall yields while
    // it's active so the two don't ping-pong the bot every other tick
    // (2026-06-11 Deadmines: follow_recall <-> move_to_corpse alternated
    // for minutes, 7 corpses pending, zero looted). The lease is
    // self-expiring and the recall rule bounds it further by range and
    // group-combat state — a fight or a real separation still recalls.
    void grant_detour(uint32 now_ms, uint32 dur_ms = 8000)
    { detour_until_ms_ = now_ms + dur_ms; }
    bool detour_active(uint32 now_ms) const
    { return detour_until_ms_ != 0 && now_ms < detour_until_ms_; }
private:
    uint32 detour_until_ms_ = 0;
    CachedGraveDist cached_grave_dist_{};
    std::unordered_map<uint32, uint32> cast_emit_until_ms_;
    // Per-item-entry equip retry cooldown — see kEquipTryLockoutMs above.
    std::unordered_map<uint32, uint32> equip_try_until_ms_;
    // Per-hub blacklist for unproductive arrivals — see kHubTriedLockoutMs.
    std::unordered_map<uint32, uint32> tried_hub_until_ms_;
    // A4 per-corpse loot first-seen timestamp — see loot_corpse_overdue.
    std::unordered_map<uint64, uint32> loot_first_seen_;
    // Per-corpse WALK first-attempt timestamp — see loot_corpse_walk_overdue.
    std::unordered_map<uint64, uint32> loot_walk_first_ms_;
    // Per-target start_attack lockout — see kStartAttackLockoutMs.
    std::unordered_map<uint64, uint32> start_attack_until_ms_;
    // critical-repair futility backoff — see crit_repair_note_adjacent() above.
    uint32 crit_repair_adjacent_since_ms_ = 0;  // first tick adjacent this episode (0 = not at vendor)
    uint32 crit_repair_backoff_until_ms_  = 0;  // suppress critical_repair until this time (too poor)
    // bag-recovery futility backoff — see bag_recovery_note_and_check_futile() above.
    uint32 bag_recovery_backoff_until_ms_ = 0;  // suppress bags_full_recover until this time
    uint32 capital_run_futile_until_ms_   = 0;  // suppress capital_bag_run after an unreachable-city give-up
    uint32 bag_recovery_first_ms_         = 0;  // first tick of the current no-progress window (0 = none)
    uint8  bag_recovery_free_seen_        = 255; // most free slots seen this window (progress detector)
    // hub-offer close-approach futility — see hub_offer_note_and_check_futile() above.
    uint32 hub_offer_since_ms_            = 0;  // first tick close-approaching unaccepted in-scan offers (0 = none)
    uint32 hub_offer_backoff_until_ms_    = 0;  // suppress walk_to_known_hub offer close-approach until this time
    uint8  hub_offer_min_count_           = 0;  // fewest in-scan offers seen this window (accept-progress detector)
    // chunked-walk position-stall tracker — see walk_stall_note() above.
    uint32 walk_stall_since_ms_           = 0;  // start of the current no-progress window (0 = no active episode)
    uint32 walk_stall_last_ms_            = 0;  // last walk_stall_note tick (for >10s episode-gap re-base)
    float  walk_stall_x_                  = 0.f;
    float  walk_stall_y_                  = 0.f;
    uint8  walk_stall_strikes_            = 0;  // consecutive no-progress windows (drives escape deflection)
    uint8  crit_repair_best_dura_pct_     = 0;  // best lowest-durability seen at the vendor this episode
    uint32 crit_repair_episode_since_ms_  = 0;  // first tick of THIS critical_repair episode (any sub-state)
    uint8  crit_repair_episode_best_dura_ = 0;  // best lowest-durability seen this episode (walk-futility)
    // Per-(vendor, subclass) restock retry cooldown — see kVendorBuyTryLockoutMs.
    std::unordered_map<uint64, uint32> vendor_buy_until_ms_;
    // Per-GO chest loot retry cooldown — see kChestLootTryLockoutMs.
    std::unordered_map<uint64, uint32> chest_loot_until_ms_;
    // Generic action retry cooldown — see ActionKind / kActionRetryLockoutMs.
    std::unordered_map<uint64, uint32> action_retry_until_ms_;
    // #4B-1 Part 2: per-item AH sell-price memory — see SellPriceMemo above.
    // Keyed by item_entry. Bounded naturally by the bot's distinct sellable
    // inventory; the AhPostItem 5-min retry lockout limits churn so this map
    // stays small (a few dozen entries at most over a bot's lifetime).
    std::unordered_map<uint32, SellPriceMemo> sell_price_memo_;
    // Pet-summon backoff. When MaintainPet keeps casting Call Pet / Raise
    // Dead but the bot stays petless (no tamed pet in stable, etc.), the
    // rule re-emits every tick and dispatch never reaches movement /
    // quest rules below it. Extends the per-spell cast dedup with a
    // per-bot 5-min lockout once a summon attempt fails to actually
    // produce a pet. Reset when has_pet flips true.
    uint32 pet_summon_backoff_until_ms_ = 0;
public:
    void note_pet_summon_attempt(uint32 now_ms)
    { pet_summon_backoff_until_ms_ = now_ms + 30u * 1000u; }   // 30s (was 5min — too long for dungeon context)
    bool pet_summon_in_backoff(uint32 now_ms) const
    { return now_ms < pet_summon_backoff_until_ms_; }
    void clear_pet_summon_backoff()
    { pet_summon_backoff_until_ms_ = 0; }

    // Last pet_guid we observed. Tick() compares against the live
    // snapshot and emits a PetSetReactStateIntent{1} (defensive) on
    // any empty→non-empty or guid-change transition — keeps every
    // newly-summoned pet in defensive instead of the server default
    // (aggressive for hunter pets, defensive for warlock pets, but
    // we normalise to defensive across the board because aggressive
    // mode actively hinders bot progress: pets pull mobs the group
    // wasn't ready for, break CC, chase respawns out of LoS).
    ObjectGuid last_known_pet_guid() const { return last_known_pet_guid_; }
    void       set_last_known_pet_guid(ObjectGuid g) { last_known_pet_guid_ = g; }
private:
    ObjectGuid last_known_pet_guid_;
public:

    // Swim-stuck timeout. Tracks when the bot first started swimming (or
    // 0 if not swimming). After kSwimStuckTimeoutMs the swim_stuck rule
    // tries to walk the bot to the nearest quest hub on its current map
    // (bots can't actually swim — they jump/walk along the surface, so
    // any prolonged water contact is "wrong location"). Falls back to
    // hearthstone when no hub fits (open ocean, sub-zone deserts of
    // submerged terrain). Threshold raised from 90s to 120s per owner
    // directive 2026-05-16 — short water crossings (river/lake fords)
    // shouldn't trip the rule.
    static constexpr uint32 kSwimStuckTimeoutMs = 120u * 1000u;
    uint32 swimming_since_ms() const { return swimming_since_ms_; }
    void update_swim_state(bool currently_swimming, uint32 now_ms)
    {
        if (currently_swimming)
        {
            if (swimming_since_ms_ == 0) swimming_since_ms_ = now_ms;
        }
        else
        {
            swimming_since_ms_ = 0;
        }
    }
    bool swim_stuck(uint32 now_ms) const
    {
        return swimming_since_ms_ != 0
            && (now_ms - swimming_since_ms_) > kSwimStuckTimeoutMs;
    }

    // Per-target start_attack lockout. When idle:quest_batch:kill (or any
    // rule that emits StartAttackIntent) picks a target the server keeps
    // refusing — Player::Attack returns false because the unit is in a
    // different faction state, phased out of the bot's view, immune,
    // mounted in a vehicle, friendly under PvP rules, etc — the rule
    // re-fires every snapshot tick. The intent path returns ServerRefused
    // every time and the bot's victim never gets set, so the loop has no
    // self-correction.
    //
    // Wedge example from a freeze dump: bot 87300 (L76 Paladin) emitted
    // StartAttack 32 times in ~7 seconds against the same target, all
    // ServerRefused. With ~100 such bots this saturates the intent
    // executor and the world-thread Update tick blows past 60 s.
    //
    // Cache (target_guid_low → until_ms) for 30 seconds. Inside the
    // lockout window the emitter drops the new StartAttack silently.
    // Outside the window, one retry is allowed (in case the underlying
    // condition cleared).
    static constexpr uint32 kStartAttackLockoutMs = 30u * 1000u;
    void note_start_attack_refused(uint64 target_low, uint32 now_ms)
    {
        if (target_low == 0) return;
        start_attack_until_ms_[target_low] = now_ms + kStartAttackLockoutMs;
    }
    bool start_attack_recently_refused(uint64 target_low, uint32 now_ms) const
    {
        if (target_low == 0) return false;
        auto const it = start_attack_until_ms_.find(target_low);
        if (it == start_attack_until_ms_.end()) return false;
        return now_ms < it->second;
    }
    void prune_start_attack_cache(uint32 now_ms)
    {
        for (auto it = start_attack_until_ms_.begin(); it != start_attack_until_ms_.end();)
        {
            if (now_ms >= it->second) it = start_attack_until_ms_.erase(it);
            else                       ++it;
        }
    }

    // Per-bot activity-mode "decision". The bot picks a mode every 30-60
    // minutes — the same way a real player decides "I'll quest tonight"
    // vs "I'll level professions tonight". Used by State_Idle to bias
    // rule priority: Questing → quest accept/turnin/hub-travel rules
    // are normal-priority; Professioning → those rules are suppressed
    // so the bot can park near nodes / trainers / crafting stations and
    // actually skill up. The mode persists until activity_mode_until_ms_
    // expires, at which point dispatch_primary rolls a new one.
    //
    // Probability mix on roll: 65% Questing, 30% Professioning, 5%
    // Wandering (free-roam fallback). Wandering is rare on purpose —
    // it's the "neither questing nor professioning" idle state, used
    // mostly for atmospheric variety.
    enum class ActivityMode : uint8 { Questing = 0, Professioning = 1, Wandering = 2 };
    ActivityMode activity_mode() const { return activity_mode_; }
    bool in_profession_mode(uint32 now_ms) const
    { return now_ms < activity_mode_until_ms_ && activity_mode_ == ActivityMode::Professioning; }
    bool activity_mode_expired(uint32 now_ms) const
    { return now_ms >= activity_mode_until_ms_; }
    // `level` is the bot's current level. Realm policy: bots below
    // level 10 don't run professions (LearnProfessions in the setup
    // pipeline is gated at L>=10 too). And Wandering wastes the
    // precious starter-zone questing window. Force L<10 bots into
    // Questing for a full hour so the quest pipeline (accept →
    // pursue → turn-in) actually gets a chance to run. Without this
    // gate, 30% of fresh L1 alts roll Professioning and idle silently
    // for 30–60 min — the exact symptom that landed fresh alts stuck
    // at L1 wandering in starter zones.
    void roll_activity_mode(uint32 now_ms, BotRng& rng, uint8 level = 80)
    {
        if (level < 10)
        {
            activity_mode_         = ActivityMode::Questing;
            activity_mode_until_ms_ = now_ms + 60u * 60u * 1000u; // 1h
            return;
        }
        const uint32 r = uint32(rng.next() % 100ULL);
        if (r < 65)      activity_mode_ = ActivityMode::Questing;
        else if (r < 95) activity_mode_ = ActivityMode::Professioning;
        else             activity_mode_ = ActivityMode::Wandering;
        // 30-60 min duration.
        const uint32 dur_ms = 30u * 60u * 1000u + uint32(rng.next() % (30ULL * 60ULL * 1000ULL));
        activity_mode_until_ms_ = now_ms + dur_ms;
    }
private:
    ActivityMode activity_mode_           = ActivityMode::Questing;
    uint32       activity_mode_until_ms_  = 0;
    StuckTracker stuck_{};
    // R7 sticky leveling-zone relocation target (see LevelingZoneTarget +
    // accessors above). leveling_target_loaded_ guards the login DB hydration.
    LevelingZoneTarget leveling_target_{};
    bool               leveling_target_loaded_ = false;
    // R7 island-escape bridge-decision cache (world-thread-only; see accessors).
    uint64             reloc_bridge_goal_ = 0;
    bool               reloc_bridge_has_  = false;
    uint32             reloc_bridge_at_ms_ = 0;   // probe time, for negative-verdict expiry
    std::vector<PlanLeg> reloc_bridge_legs_{};   // world-thread only (see accessor)
    // CombatLoop FIX C scan-MISS POI-fallback hysteresis (world-thread only).
    PoiSpawnSticky     poi_spawn_sticky_{};
    // Zero-yield combat watchdog (combat:disengage_no_progress): tracks the
    // bot's current victim and the lowest HP observed on it. If the same
    // victim's HP fails to drop for the watchdog window, the fight yields
    // nothing (evade-wedged mob, immune phase outside instances, unkillable
    // prop) and the bot disengages instead of swinging forever.
    ObjectGuid         victim_watch_guid_{};
    int32              victim_watch_hp_ = 0;
    uint32             victim_watch_since_ms_ = 0;
    // Unkillable-target leash — see set_combat_stuck() / State_InCombat.
    ObjectGuid         combat_stuck_victim_{};
    int32              combat_stuck_hp_ = 0;
    uint32             combat_stuck_since_ms_ = 0;
public:
    // In-instance entrance capture (audit B36): the bot's first observed
    // position after entering an instance, keyed by instance id so a new
    // run re-captures. World-thread only (written + read by the snapshot
    // builder; workers consume via the snapshot's inside_entrance_*).
    uint32 inside_entrance_instance() const { return inside_entrance_instance_; }
    float  inside_entrance_x() const { return inside_entrance_x_; }
    float  inside_entrance_y() const { return inside_entrance_y_; }
    float  inside_entrance_z() const { return inside_entrance_z_; }
    void   set_inside_entrance(uint32 instance_id, float x, float y, float z)
    { inside_entrance_instance_ = instance_id; inside_entrance_x_ = x;
      inside_entrance_y_ = y; inside_entrance_z_ = z; }
private:
    uint32             inside_entrance_instance_ = 0;
    float              inside_entrance_x_ = 0.f;
    float              inside_entrance_y_ = 0.f;
    float              inside_entrance_z_ = 0.f;
    // Multi-slot objective blacklist: (quest_id, obj_id) -> blacklisted-until
    // ms. Replaces the old single-slot blacklisted_until_ms inside
    // ObjectiveTrack, which could only remember ONE blacklisted objective —
    // blacklisting B erased A, so a bot with 2+ stuck objectives livelocked
    // ping-ponging between them (audit C03/C18). Bounded: pruned on insert.
    static uint64 BlacklistKey(uint32 quest_id, uint32 obj_id)
    { return (uint64(quest_id) << 32) | uint64(obj_id); }
    void blacklist_put(uint32 quest_id, uint32 obj_id, uint32 until_ms)
    {
        std::lock_guard<std::mutex> lk(quest_mem_mtx_);
        // Prune expired entries opportunistically; hard-cap the map so a
        // pathological bot can't grow it unbounded (~32 live entries is far
        // beyond any real quest log).
        if (objective_blacklist_.size() >= 32)
        {
            for (auto it = objective_blacklist_.begin(); it != objective_blacklist_.end();)
            {
                if (it->second <= until_ms) it = objective_blacklist_.erase(it);
                else ++it;
            }
            if (objective_blacklist_.size() >= 32)
                objective_blacklist_.clear();   // degenerate fallback
        }
        objective_blacklist_[BlacklistKey(quest_id, obj_id)] = until_ms;
    }
    std::unordered_map<uint64, uint32> objective_blacklist_{};
    // L2/L3 cached travel plan (see PlanLeg + accessors above).
    std::vector<PlanLeg> travel_plan_{};
    size_t               travel_plan_leg_  = 0;
    uint64               travel_plan_goal_ = 0;
    uint32               last_leg_skip_ms_ = 0;   // leg-skip rate-limit (see accessor)
    // Oscillation leash backing (see note_position_leash). Goal-agnostic, so it
    // is NOT cleared when the StuckTracker resets on a goal change.
    float  osc_anchor_x_ = 0.f, osc_anchor_y_ = 0.f;
    uint32 osc_anchor_ms_ = 0;
    uint32 osc_stuck_ticks_ = 0;
    bool   osc_anchor_valid_ = false;
    uint32 last_food_buy_ms_ = 0;
    FollowProgress follow_progress_{};
    // Last move_to dedup state — see kMoveToEmitLockoutMs above.
    float  last_move_to_x_       = 0.f;
    float  last_move_to_y_       = 0.f;
    float  last_move_to_z_       = 0.f;
    uint32 last_move_to_until_   = 0;
    bool   last_move_to_valid_   = false;
    // Dungeon regroup convergence sample: last dist² to the tank + the time it
    // was taken. See kRegroupSampleMs / note_regroup_sample above.
    float  last_regroup_dist2_   = 0.f;
    uint32 last_regroup_ms_      = 0;
    // Stranded-follower stuck clock: best (closest) 3D distance to the tank and
    // the time that best stopped improving. See regroup_stuck_ms above.
    uint32 regroup_stuck_since_ms_ = 0;
    float  regroup_stuck_best_     = 0.f;
    // Own-position frozen clock: the bot's last sampled pose + when it last
    // moved >3y from it. See frozen_ms above (rally-independent strand timer).
    uint32 frozen_since_ms_ = 0;
    float  frozen_x_ = 0.f, frozen_y_ = 0.f, frozen_z_ = 0.f;
    // Converge-to-fight clock: when the "stuck out of the tank's active fight"
    // condition first became continuously true. See combat_join_stuck_ms above.
    uint32 combat_join_since_ms_ = 0;
    // Path-blocked counter. Incremented every time API::move_to returns
    // Result::Locked (Detour rejected the path: NoPath / FarFromPolyEnd /
    // FarFromPolyStart). Mixed into the wander rule's angle hash so a
    // blocked emit immediately picks a different bearing instead of waiting
    // for the 5s angle bucket to roll over — the bot tries another direction
    // on the very next tick. Public getter + setter keep BotIntentExecutor
    // free of friend boilerplate.
    uint32 path_blocked_count_   = 0;
    uint32 last_path_blocked_ms_ = 0;
    // Post-rescue grace window: while in_post_rescue_grace() returns
    // true, travel/wander rules that would walk the bot a long distance
    // should fall through. Lets the freshly-teleported bot settle on
    // the safe plaza before re-planning a far-away goal.
    uint32 post_rescue_grace_until_ = 0;
    static constexpr uint32 kPostRescueGraceMs = 60u * 1000u;
    // Charter participant grace: BotGuildMgr::TeleportFounderAndSigners
    // pulls 5 bots (1 founder + 4 signers) onto a capital petitioner
    // plaza. Without this flag, idle:travel_to_hub immediately walks
    // them back to their original level-bracket hub (observed
    // 2026-05-18: bots running back-and-forth in Orgrimmar) and the
    // charter FSM never completes. Set to (now + 20min) on teleport;
    // travel/wander rules consult `in_charter_grace()` and skip.
    uint32 charter_grace_until_ = 0;
    static constexpr uint32 kCharterGraceMs = 20u * 60u * 1000u;

    // Per-rule wedge tracker. A rule that calls `check_anchor_wedge` on
    // each fire stores a baseline path_blocked_count + timestamp on its
    // first fire; if the count grows by `threshold` (default 3) on
    // subsequent calls without enough time having passed, we conclude
    // the rule's target anchor is unreachable, suppress the rule for
    // `cooldown_ms` (default 30s), and return true so the caller falls
    // through to the next rule. Tracked separately from
    // `set_last_rule_fired` so distinct concurrent rules don't share a
    // baseline (e.g. walk_to_known_dock + walk_to_known_portal can
    // each have their own wedge state).
    //
    // Fixed-size: rule names are stable string_view, ~10 distinct
    // walk-anchor rules across the codebase, so a 4-slot LRU is plenty.
    struct RuleWedge
    {
        std::string_view rule_name;
        uint32 baseline_blocks  = 0;
        uint32 first_fire_ms    = 0;
        uint32 suppress_until   = 0;
    };
    // Slot count tuned for the actual set of wedge-guarded rules. At 4
    // we observed idle:wander's slot getting evicted by travel_to_hub /
    // walk_to_known_dock / walk_to_known_portal in busy areas — so its
    // baseline kept resetting and threshold was never crossed. 8 slots
    // covers every wedge-emitting rule with room to spare.
    static constexpr size_t kRuleWedgeSlots = 8;
    std::array<RuleWedge, kRuleWedgeSlots> rule_wedges_{};
public:
    void   note_path_blocked(uint32 now_ms)
    {
        ++path_blocked_count_;
        last_path_blocked_ms_ = now_ms;
    }
    // Called when a move_to actually succeeds (API returned Result::Ok and a
    // real path/spline was issued). Resets the block tally so it reflects
    // CONSECUTIVE blocks ("currently wedged") rather than a monotonic
    // lifetime count — a bot that wandered fine for hours used to read
    // blocks=48000 forever, which made both the diagnostic and the wander
    // angle salt meaningless. NOTE: GlobalStuckRescue computes a uint32
    // delta against a stored baseline; it MUST clamp for the case where
    // this reset drops `cur` below that baseline (see PlayerbotV2.cpp),
    // otherwise the subtraction underflows and triggers a false rescue.
    void   note_move_succeeded()
    {
        path_blocked_count_ = 0;
    }
    uint32 path_blocked_count() const { return path_blocked_count_; }
    uint32 last_path_blocked_ms() const { return last_path_blocked_ms_; }

    // Called by GlobalStuckRescue after a teleport-to-safety. Clears the
    // per-bot planning state so post-teleport rules re-pick destinations
    // from the new position instead of carrying stale anchors / cooldowns
    // / wedge slots forward. Without this, a bot rescued from Forbidden
    // Reach to Stormwind Keep keeps emitting the SAME bad destinations
    // because the wedge slot for idle:wander still remembers "this is
    // wedged" and the anchor logic still chases the prior POI.
    //
    // Also sets `post_rescue_grace_until_` so cross-map / long-distance
    // travel rules can self-suppress for a short window, letting the
    // bot settle locally before chasing a new far-away goal.
    void reset_after_rescue(uint32 now_ms)
    {
        path_blocked_count_   = 0;
        last_path_blocked_ms_ = 0;
        last_rule_fired_      = nullptr;
        for (RuleWedge& w : rule_wedges_) w = RuleWedge{};
        post_rescue_grace_until_ = now_ms + kPostRescueGraceMs;
        stuck_                = StuckTracker{};
        objective_track_      = ObjectiveTrack{0, 0, 0, 0, 0, 0, 0};
        lateral_side_         = 0;
        lateral_side_ms_      = 0;
        last_move_to_valid_   = false;
        activity_mode_until_ms_ = 0;
        reset_regroup_tracking();
    }
    // Travel/wander rules can call this and fall through when true,
    // so they don't immediately walk a freshly-rescued bot back into
    // the same trap they just escaped.
    bool in_post_rescue_grace(uint32 now_ms) const
    {
        return post_rescue_grace_until_ != 0 && post_rescue_grace_until_ > now_ms;
    }
    // Set when BotGuildMgr::TeleportFounderAndSigners pulls this bot
    // (founder or signer) onto the petitioner plaza. Travel/wander
    // rules must consult in_charter_grace() and skip while it's true,
    // otherwise the bot walks back to its level-zone hub before the
    // charter FSM can complete signing + turn-in.
    void arm_charter_grace(uint32 now_ms)
    {
        charter_grace_until_ = now_ms + kCharterGraceMs;
    }
    bool in_charter_grace(uint32 now_ms) const
    {
        return charter_grace_until_ != 0 && charter_grace_until_ > now_ms;
    }

    // Returns true when `rule_name` should ABORT and fall through (either
    // currently in the suppress cooldown, or the per-rule block-growth
    // threshold was just exceeded — in which case we ALSO set the
    // cooldown). The caller is expected to call this BEFORE its emit;
    // when returning false, the caller proceeds normally (emit + return
    // true).
    //
    // Why a per-rule tracker rather than a snapshot-side anchor blacklist:
    // anchor blacklisting in the snapshot requires the Builder to know
    // which anchor failed, but path_blocked_count() is global per-bot
    // (path failures from any rule increment it). By tracking baselines
    // per rule name on the AI side, each rule independently sees "from
    // the moment I started firing, has the global block count grown by
    // K?" — a clean wedge detector that doesn't need anchor identity.
    //
    // Threshold tuning: 3 path failures in 10 seconds is the wedge mark.
    // Real navmesh fluke gives 0-1; persistent unreachable target gives
    // 5+ within a second. Cooldown of 30s lets other rules try; after
    // the cooldown the rule may attempt again (the anchor might
    // become reachable, e.g. if the bot was moved by an external rule).
    bool check_anchor_wedge(std::string_view rule_name,
                            uint32 path_block_count_snapshot,
                            uint32 now_ms,
                            uint32 threshold = 3,
                            uint32 max_window_ms = 10000,
                            uint32 cooldown_ms = 30000)
    {
        // Find or claim a slot. LRU eviction by `first_fire_ms` so a long-
        // suppressed wedge doesn't crowd out a fresh rule. Linear scan is
        // fine for 4 slots; trying to be clever here is pure overhead.
        RuleWedge* slot = nullptr;
        RuleWedge* oldest = &rule_wedges_[0];
        for (RuleWedge& w : rule_wedges_)
        {
            if (w.rule_name == rule_name) { slot = &w; break; }
            if (w.first_fire_ms < oldest->first_fire_ms) oldest = &w;
        }
        if (!slot)
        {
            slot = oldest;
            *slot = RuleWedge{};
            slot->rule_name = rule_name;
        }

        // Still in cooldown — caller must fall through.
        if (slot->suppress_until > now_ms) return true;

        // First fire (or post-cooldown re-fire). Capture baseline.
        if (slot->first_fire_ms == 0 || (now_ms - slot->first_fire_ms) > max_window_ms)
        {
            slot->baseline_blocks = path_block_count_snapshot;
            slot->first_fire_ms   = now_ms;
            slot->suppress_until  = 0;
            return false;
        }

        // Same wedge window. Has the block count grown enough to wedge?
        const uint32 grew = path_block_count_snapshot - slot->baseline_blocks;
        if (grew >= threshold)
        {
            slot->suppress_until = now_ms + cooldown_ms;
            return true;
        }
        return false;
    }
    bool quest_recently_shared(uint32 q) const
    {
        for (uint32 e : shared_ring_) if (e == q) return true;
        return false;
    }
    void note_shared_quest(uint32 q)
    {
        shared_ring_[shared_ring_head_] = q;
        shared_ring_head_ = (shared_ring_head_ + 1) % kSharedRingCap;
    }
private:
    // Rule-fire history ring buffer for /history whisper diagnostic. Holds
    // pointers to the static rule-name strings (owned by the rotation tables
    // / state code) — pointer comparison for dedup is safe.
    static constexpr size_t kRuleHistoryCap = 16;
    std::array<char const*, kRuleHistoryCap> rule_history_{};
    size_t         rule_history_head_ = 0;
    size_t         rule_history_size_ = 0;

    // DMM-P1b (State_Dead.cpp): Shaman Reincarnation self-rez fall-through
    // guard. Placed in this isolated end-of-class block to avoid merge
    // collision with concurrent BotAI.h edits. Accessors declared near the
    // other death-recovery accessors (reincarnation_attempted / *_ms above).
    bool           reincarnation_attempted_ = false;
    uint32         reincarnation_attempt_ms_ = 0;
    // DMM-P3a: corpse-run no-progress baseline distance (yards). -1 = unset.
    float          corpse_run_last_dist_ = -1.0f;
};

} // namespace Playerbot

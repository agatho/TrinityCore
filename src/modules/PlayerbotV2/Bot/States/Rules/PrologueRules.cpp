// PrologueRules - Refactor #3 pass 19. Migrates the state-machine
// prologue rules that used to live inline at the top of DispatchIdle:
//   - idle:on_transport_wait     (999) freeze while a ship/zeppelin is in motion
//   - idle:watchdog_escape       (995) rule-fire watchdog wedge-break
//   - combat opener              (993) APL tick when victim is set OOC
//   - idle:owner_following       (990) manual /follow
//   - idle:owner_holding         (985) manual /hold (fire returns false: lets
//                                       lower-pri rules still run)
//   - idle:owner_stay            (980) manual /stay
//   - idle:owner_engaging        (975) manual /engage
//   - idle:owner_disengage       (970) manual /disengage
//   - idle:owner_action          (965) manual one-shot action
// All register above the survival band (880-900) so they preempt
// hazards. Holding mode is the exception — it preserves the legacy
// "fall through" semantic via fire-returns-false.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/Dungeon/DungeonScript.h"   // DungeonAdvice (boss list)
#include "Services.h"                    // Services::Dungeons() — vanishing-boss re-engage
#include "Combat/ApRotation.h"
#include "Combat/ApRegistry.h"
#include "Log.h"   // [bg_orphan] escape diagnostics

#include <algorithm>
#include <cmath>

namespace Playerbot {

namespace {

// ---------- idle:on_transport_wait ----------
// Freeze the bot while a transport is carrying it. Two cases:
//   * Ships/zeppelins (type-15): the snapshot's transport_stopped flag
//     is accurate (Transport::IsStopped) — gate on !transport_stopped.
//   * Elevators (type-11): the snapshot builder's dynamic_cast<Transport>
//     fails and transport_stopped is always-true, but BotAI tracks
//     vertical-Z stability as a proxy. Freeze while we're moving
//     vertically (Z hasn't settled for ≥500ms). Without this freeze
//     the bot could be issued move_to commands by lower-priority
//     rules while the platform is mid-ascent and walk off the edge.
bool OnTransportGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!s.on_transport()) return false;
    if (!s.transport_stopped()) return true;                   // ship in motion
    if (ai.transport_z_stable_ms() < 500) return true;         // elevator rising/falling
    return false;
}

bool OnTransportFire(BotSnapshotView const&, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:on_transport_wait");
    return true;
}

// ---------- idle:watchdog_escape ----------
bool WatchdogGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.in_battleground()) return false;
    // Charter founders + signers are teleported onto the petitioner plaza
    // and rely on the FSM to advance phase 0→5 in place. The 50y random
    // escape move would yank them off the plaza (out of nearby_friends
    // range of the petitioner), and if it hits NoPath the path_blocks
    // counter trips GlobalStuckRescue, sending the bot to homebind (which
    // can be a TWW zone with sparse navmesh). The 20-min charter-grace
    // covers the full FSM budget; skip the watchdog while it's armed.
    const uint32 wdog_now_ms = s.published_at_ms();
    if (ai.in_charter_grace(wdog_now_ms)) return false;
    char const* last = ai.last_rule_fired();
    return last && ai.rule_watchdog_suppressed(last, wdog_now_ms);
}

bool WatchdogFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const&,
                  BotIntentEmitter& emit, uint32)
{
    const uint32 wdog_now_ms = s.published_at_ms();
    float bx, by, bz; s.position(bx, by, bz);
    const uint32 epoch = wdog_now_ms / 2000u;
    const uint64 mixed = (uint64(s.bot_id()) * 0x9E3779B97F4A7C15ULL)
                       ^ (uint64(epoch) * 0xBF58476D1CE4E5B9ULL);
    const float angle = (float(mixed & 0xFFFFu) / 65536.0f) * 6.2831853f;
    constexpr float kEscapeStep = 50.0f;
    emit.move_to(bx + std::cos(angle) * kEscapeStep,
                 by + std::sin(angle) * kEscapeStep,
                 bz, /*run*/ true);
    ai.set_last_rule_fired("idle:watchdog_escape");
    return true;
}

// ---------- idle:bg_orphan_escape ----------
// A battleground ended, the core's exit teleport missed this bot, and the
// Battleground object is gone — the bot is stranded on a dead BG/arena map
// (live 2026-06-11: 3 bots brawling forever on post-match map 566).
// combat:bg_orphan_disengage drops the fight; this rule then performs the
// exit the core owed the bot: hearthstone when ready, else a direct port
// to the homebind (the same destination TeleportToBGEntryPoint degrades
// to on error). 15s retry throttle via the action-retry table so a
// pending hearth cast / teleport isn't re-emitted every tick.
bool BgOrphanEscapeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    return s.is_bg_orphan() && s.is_alive() && !s.in_combat();
}

bool BgOrphanEscapeFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    if (ai.action_recently_tried(BotAI::ActionKind::BgOrphanEscape, 0, now_ms))
        return false;
    auto const& trv = s.raw().travel;
    if (trv.has_hearthstone && trv.hearthstone_cd_ms == 0)
        emit.hearth();
    else if (trv.homebind_map_id != 0 || trv.homebind_x != 0.f || trv.homebind_y != 0.f)
        emit.teleport_to(trv.homebind_map_id, trv.homebind_x, trv.homebind_y, trv.homebind_z);
    else
        return false;
    ai.note_action_retry(BotAI::ActionKind::BgOrphanEscape, 0, now_ms);
    TC_LOG_INFO("playerbot.v2",
        "[bg_orphan] bot={} escaping dead BG map {} (hearth_ready={})",
        s.bot_id(), s.map_id(),
        (trv.has_hearthstone && trv.hearthstone_cd_ms == 0) ? 1 : 0);
    ai.set_last_rule_fired("idle:bg_orphan_escape");
    return true;
}

// ---------- combat:opener ----------
bool OpenerGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    if (s.victim().IsEmpty() || s.is_casting() || s.in_combat()) return false;
    // Dungeon cohesion: a follower far from the tank must not burn ticks opening
    // on a target across a chokepoint it can't reach (the Gap-1 lip). The opener
    // (prio 993) otherwise preempts the DungeonDispatch regroup-cross (prio 720)
    // forever, stranding the follower while the tank fights the boss room unhealed
    // (3/5 wipe, Deadmines 2026-06-26). Yield so the cross runs; open after rejoin.
    if (ai.dungeon_active() && ai.effective_role(s) != Role::Tank && g.exists())
        if (GroupMemberSummary const* tk = g.tank())
            if (tk->online && tk->is_alive && tk->guid != s.raw().guid &&
                tk->map_id == s.map_id())
            {
                float bx, by, bz; s.position(bx, by, bz);
                const float dx = tk->x - bx, dy = tk->y - by;
                const float td2 = dx * dx + dy * dy;
                if (td2 > 40.0f * 40.0f)
                    return false;
                // Stuck out of the tank's ACTIVE fight: the tank is already
                // trading blows and we are not (in_combat was excluded above).
                // Burning opener ticks cast-spamming from the 12-40y dead-band
                // only ends in the 25s give-up (which 5-min-shields the kill
                // target), AND the opener (prio 993) monopolizes the tick so the
                // DungeonDispatch (720) converge/regroup that would WALK us into
                // range never runs — the tank then SOLOS a shielded mob it can't
                // finish for 30+ minutes (the Helix-approach Envoker deadlock,
                // 2026-06-27). Yield so converge-to-fight closes the gap; the
                // opener resumes the instant we're <12y and the cast lands.
                if (tk->in_combat && !tk->victim.IsEmpty() &&
                    td2 > 12.0f * 12.0f)
                    return false;
            }
    return true;
}

bool OpenerFire(BotSnapshotView const& s, BotAI& ai,
                GroupSnapshotView const& g,
                BotIntentEmitter& emit, uint32)
{
    // ---- Opener stall handling (2026-06-13) ----
    // The opener used to run the APL unconditionally against an out-of-
    // combat victim. With nothing closing the gap and nothing giving up,
    // a victim the bot couldn't reach produced an infinite
    // CastSpell|OutOfRange retry loop (~1.6s cadence, observed running
    // for hours fleet-wide): the rotation can't know its rules' ranges
    // here, State_InCombat's reposition/disengage never runs (the mob
    // never aggroes, so the state stays Idle), and can_autoact blocks
    // every idle movement rule while a victim is set.
    const uint32 now_ms = s.published_at_ms();
    ObjectGuid const vg = s.victim();
    // Fresh open when the victim changed OR the opener hasn't evaluated
    // recently (bot was in combat / casting / victimless in between) —
    // otherwise a re-open on the same GUID later would inherit a stale
    // since-timestamp and give up instantly.
    const bool fresh_open =
        (ai.opener_victim() != vg || now_ms - ai.opener_last_seen_ms() > 5000);
    if (fresh_open)
        ai.set_opener_victim(vg, now_ms);
    ai.set_opener_last_seen(now_ms);

    // Name the opener when it owns a dungeon tank's tick out of combat. This
    // rule (prio 993) sits ABOVE idle:dungeon_dispatch (720) and, for a tank
    // (which is exempt from the yield below), can claim every tick on a stale
    // victim selection with no MoveTo — presenting as a frozen tank still
    // "fighting" (InCombat=false, rotation names in RuleHist). If `since`
    // visibly RE-STAMPS while frozen, the 25s give-up can never fire because
    // the selection is flipping GUIDs. Same "make it say why" play as
    // [move_lock]/[emit_refused]. Throttled.
    if (ai.dungeon_active())
    {
        static uint32 s_opener_dbg_ms = 0;
        if (now_ms - s_opener_dbg_ms > 1500u)
        {
            s_opener_dbg_ms = now_ms;
            TC_LOG_INFO("playerbot.v2",
                "[opener_own] bot={} role={} in_combat={} fresh={} "
                "since={}ms oor={}",
                s.bot_id(), int(ai.effective_role(s)), s.in_combat() ? 1 : 0,
                fresh_open ? 1 : 0, now_ms - ai.opener_victim_since_ms(),
                ai.cast_oor_count());
        }
    }

    NearbyUnit const* vu = nullptr;
    for (auto const& u : s.raw().combat.nearby_enemies)
        if (u.guid == vg) { vu = &u; break; }
    // Record boss-ness WHILE the victim is visible so the lost-victim branch below
    // can tell a vanishing boss from despawned trash (the entry is gone once it
    // leaves the snapshot). Most scripted dungeon bosses (incl. Admiral Ripsnarl)
    // are NOT flagged Creature::IsDungeonBoss(), so fall back to the per-dungeon
    // advice.bosses entry list. Computed once per fresh victim (the advice copy is
    // non-trivial) and cached on the AI.
    if (vu && fresh_open)
    {
        bool is_boss = vu->is_dungeon_boss;
        if (!is_boss && s.is_in_dungeon())
        {
            DungeonAdvice const dav = Services::Dungeons().GetAdvice(s);
            for (uint32_t be : dav.bosses)
                if (be == vu->entry) { is_boss = true; break; }
        }
        ai.set_opener_victim_is_boss(is_boss);
    }
    if (!vu)
    {
        // Victim left the 40y snapshot sweep (despawned, evaded, kited
        // away, died to someone else). Holding the selection only keeps
        // can_autoact false — drop it and shield briefly.
        emit.stop_attack();
        // VANISHING-BOSS window (2026-06-29). A boss with a stealth/fog phase
        // (Admiral Ripsnarl) drops out of the snapshot for a few seconds, then
        // reappears. The default 30s engage-blacklist then keeps the tank from
        // re-acquiring him on reappear — it idles, drifts ~100y off the deck, and
        // the group fragments (the chronic Ripsnarl stalemate: 0 deaths, boss
        // alive, fight never finishes). When the lost victim was a BOSS, shield
        // only briefly (3s) so the bot re-grabs him the instant he's back. Trash
        // (despawned/evaded) keeps the full 30s so the unreachable-mob re-pick
        // loop the blacklist was added to prevent stays fixed.
        const uint32 reengage_ms = ai.opener_victim_is_boss() ? 3000u : 30000u;
        ai.note_engage(vg, now_ms, reengage_ms);
        ai.set_last_rule_fired("combat:opener_lost_victim");
        return true;
    }

    // Stalled open: still not in combat after this long means neither
    // casts nor approach steps worked (wall, elevation, unpathable mob).
    // Give up and shield the target for minutes so engage_nearby_mob /
    // assist rules don't re-pick it the moment an 8s shield lapses.
    constexpr uint32 kOpenerStallMs = 25000;
    if (now_ms - ai.opener_victim_since_ms() > kOpenerStallMs)
    {
        emit.stop_attack();
        ai.note_engage(vg, now_ms, 5u * 60u * 1000u);
        ai.set_last_rule_fired("combat:opener_give_up");
        TC_LOG_INFO("playerbot.v2",
            "[opener_give_up] bot={} victim={} unreachable for {}ms — shielded 5min",
            s.bot_id(), vg.ToString(), now_ms - ai.opener_victim_since_ms());
        return true;
    }

    // Server-verdict approach: the executor counts CastSpell results of
    // OutOfRange/LoS against our selection. Any since the last reset
    // means "can't reach from here" — step toward the victim (mirrors
    // combat:reposition_range), then reset so the APL gets a fresh try
    // from the new position on the next retry window.
    if (ai.cast_oor_count() > 0)
    {
        float bx, by, bz;
        s.position(bx, by, bz);
        const float dx = vu->x - bx, dy = vu->y - by, dz = vu->z - bz;
        const float dsq = dx * dx + dy * dy + dz * dz;
        if (dsq > 4.0f * 4.0f)
        {
            const float dist = std::sqrt(dsq);
            const float step = std::min(10.0f, dist);
            emit.move_to(bx + dx / dist * step,
                         by + dy / dist * step,
                         bz, /*run*/ true);
            ai.reset_cast_oor();
            ai.set_last_rule_fired("combat:opener_approach");
            return true;
        }
        // Already adjacent yet OutOfRange — usually elevation/LoS; let
        // the APL retry and the stall timer above bound the loop.
        ai.reset_cast_oor();
    }

    ApRotation const* rot = Combat::GetRotation(s.cls(), s.spec());
    if (!rot) return false;
    ApPredicateContext ctx{s, g, ai.aoe_preference()};
    char const* rule_name = nullptr;
    if (!rot->tick(ctx, emit, &rule_name)) return false;
    ai.set_last_rule_fired(rule_name ? rule_name : "combat:opener");
    return true;
}

// ---------- Manual-mode helpers ----------
bool ManualModeActive(BotSnapshotView const& s, BotAI& ai)
{
    return ai.is_manual_mode(s.published_at_ms());
}

// idle:follow_pull_threat
// Bot following a group leader (legacy party follow OR manual-mode
// owner-follow) plows through mob aggro because TC's
// FollowMovementGenerator pathfinds straight-line — same death mode
// as the solo quest-walk one fixed by path_threat. When bot is
// "catching up" to leader (>15y away on same map) and a hostile sits
// in the corridor, pull it first. Bot's start_attack chases the
// threat, preempting the follow generator; once combat ends the
// follow resumes.
bool FollowPullThreatGate(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g, uint32)
{
    if (s.in_combat() || s.is_casting() || !s.is_alive()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    if (!g.exists()) return false;
    ObjectGuid const leader_guid = g.leader();
    if (leader_guid.IsEmpty() || leader_guid == s.guid()) return false;
    auto const* members = g.members();
    if (!members) return false;
    GroupMemberSummary const* leader = nullptr;
    for (auto const& m : *members)
        if (m.guid == leader_guid) { leader = &m; break; }
    if (!leader) return false;
    if (!leader->online) return false;
    if (leader->map_id != s.map_id()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = leader->x - bx, dy = leader->y - by;
    const float dsq = dx*dx + dy*dy;
    // "Catching up" band: >15y means we're not in formation, <120y
    // means leader is still within reach of a corridor scan. Outside
    // these we either don't need to pull (in formation) or can't
    // path_threat usefully (leader out of nearby_enemies scan radius).
    constexpr float kMin = 15.0f, kMax = 120.0f;
    if (dsq < kMin * kMin || dsq > kMax * kMax) return false;
    return s.path_threat(leader->x, leader->y,
                          /*max_forward*/ 35.0f,
                          /*half_width*/  10.0f) != nullptr;
}

bool FollowPullThreatFire(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g,
                          BotIntentEmitter& emit, uint32)
{
    ObjectGuid const leader_guid = g.leader();
    auto const* members = g.members();
    GroupMemberSummary const* leader = nullptr;
    if (members)
        for (auto const& m : *members)
            if (m.guid == leader_guid) { leader = &m; break; }
    if (!leader) return false;
    NearbyUnit const* threat = s.path_threat(leader->x, leader->y,
                                              /*max_forward*/ 35.0f,
                                              /*half_width*/  10.0f);
    if (!threat) return false;
    if (!emit.start_attack(threat->guid)) return false;
    ai.set_last_rule_fired("idle:follow_pull_threat");
    return true;
}

// idle:owner_following
bool OwnerFollowGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    // In a battleground the bot must PLAY the BG, not follow its owner. This
    // rule is priority 990 — above idle:bg_dispatch (718) — so without this
    // gate a selfbot left in manual Follow preempts the BG layer every tick
    // and, because State_InGroup's follow only yields once bg_active is set,
    // chases the owner/anchor toward an unreachable point (the closed prep
    // gate, or the owner who isn't in this instance) — observed as the bot
    // running through the walls into the void at the map edge. The owner can
    // still direct it mid-BG via engage/action; passive Follow yields to
    // objective play. Mirrors the BG exclusion on idle:follow_pull_threat.
    if (s.in_battleground()) return false;
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Follow;
}
bool OwnerFollowFire(BotSnapshotView const&, BotAI& ai,
                     GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_following");
    return true;
}

// idle:owner_holding — fire returns FALSE so subsequent rules still get
// to run (mirrors legacy `break;`-out-of-switch behavior). The diagnostic
// tag is still set so /history reflects the manual mode.
bool OwnerHoldGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Hold;
}
bool OwnerHoldFire(BotSnapshotView const&, BotAI& ai,
                   GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_holding");
    return false;   // intentional: let lower-priority rules continue
}

// idle:owner_stay
bool OwnerStayGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Stay;
}
bool OwnerStayFire(BotSnapshotView const&, BotAI& ai,
                   GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_stay");
    return true;
}

// idle:owner_engaging
bool OwnerEngageGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Engage;
}
bool OwnerEngageFire(BotSnapshotView const&, BotAI& ai,
                     GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_engaging");
    return true;
}

// idle:owner_disengage
bool OwnerDisengageGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Disengage;
}
bool OwnerDisengageFire(BotSnapshotView const&, BotAI& ai,
                        GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_disengage");
    return true;
}

// idle:owner_action
bool OwnerActionGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Action;
}
bool OwnerActionFire(BotSnapshotView const&, BotAI& ai,
                     GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_action");
    ai.exit_manual();
    return true;
}

} // anonymous namespace

void RegisterPrologueRules(IdleRuleRegistry& r)
{
    auto add = [&r](char const* name, int prio,
                    bool (*gate)(BotSnapshotView const&, BotAI&,
                                 GroupSnapshotView const&, uint32),
                    bool (*fire)(BotSnapshotView const&, BotAI&,
                                 GroupSnapshotView const&,
                                 BotIntentEmitter&, uint32))
    {
        IdleRule rule;
        rule.name     = name;
        rule.priority = prio;
        rule.gate     = gate;
        rule.fire     = fire;
        r.register_rule(std::move(rule));
    };
    add("idle:on_transport_wait", 999, &OnTransportGate,    &OnTransportFire);
    // 998: a bot stranded on a dead BG map must leave before any other
    // idle decision — every other rule operates on a world it isn't in.
    add("idle:bg_orphan_escape",  998, &BgOrphanEscapeGate, &BgOrphanEscapeFire);
    add("idle:watchdog_escape",   995, &WatchdogGate,       &WatchdogFire);
    add("combat:opener",          993, &OpenerGate,         &OpenerFire);
    // 991: fires BEFORE owner_following (990) so a corridor threat
    // preempts the no-op follow tag and start_attack interrupts the
    // engine-level FollowMovementGenerator.
    add("idle:follow_pull_threat",991, &FollowPullThreatGate, &FollowPullThreatFire);
    add("idle:owner_following",   990, &OwnerFollowGate,    &OwnerFollowFire);
    add("idle:owner_holding",     985, &OwnerHoldGate,      &OwnerHoldFire);
    add("idle:owner_stay",        980, &OwnerStayGate,      &OwnerStayFire);
    add("idle:owner_engaging",    975, &OwnerEngageGate,    &OwnerEngageFire);
    add("idle:owner_disengage",   970, &OwnerDisengageGate, &OwnerDisengageFire);
    add("idle:owner_action",      965, &OwnerActionGate,    &OwnerActionFire);
}

} // namespace Playerbot

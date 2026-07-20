// CoreFlowRules - Refactor #3 pass 13. Migrates a small family of urgent
// "always preempt" rules that benefit from the two-stage dispatch model:
//   - idle:swim_stuck_walk_to_hub / idle:swim_stuck_hearth (priority 870)
//     A bot stuck swimming > 90s walks to the nearest quest hub on map or
//     hearths out. Lower priority than survival hazards (which can also
//     kill a bot), higher than every utility / questing rule.
//   - idle:group_convert_to_raid (priority 860) — leader auto-converts to
//     raid when inside a raid map.
//   - Acceptance family: group_invite_accept (855), guild_invite_accept (853),
//     lfg_proposal_accept (850), bg_port_accept (847). All preempt the inline
//     cascade so the bot reacts the moment the invite/proposal/port arrives.
// All these register at priority >= 700 so they fire from the high-priority
// top-of-tick dispatch in `State_Idle::DispatchIdle`.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Services.h"
#include "Bot/BotRegistry.h"
#include "Group/GroupSnapshot.h"
#include "Travel/QuestHubDatabase.h"
#include "GameTime.h"
#include "DB2Stores.h"
#include "Map.h"

namespace Playerbot {

namespace {

// ---------- idle:swim_stuck ----------
bool SwimStuckGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    const uint32 swim_now_ms = s.published_at_ms();
    ai.update_swim_state(s.is_swimming(), swim_now_ms);
    if (!ai.swim_stuck(swim_now_ms)) return false;
    return !s.in_battleground() && !s.is_in_dungeon();
}

bool SwimStuckFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    const uint32 swim_now_ms = s.published_at_ms();
    ::Playerbot::V2::Travel::QuestHub const* hub =
        Services::Hubs().GetNearestQuestHub(s.raw());
    if (hub && hub->mapId == s.map_id())
    {
        emit.move_to(hub->location.GetPositionX(),
                     hub->location.GetPositionY(),
                     hub->location.GetPositionZ(),
                     /*run*/ true);
        ai.set_last_rule_fired("idle:swim_stuck_walk_to_hub");
        ai.update_swim_state(false, swim_now_ms);
        return true;
    }
    emit.hearth();
    ai.set_last_rule_fired("idle:swim_stuck_hearth");
    ai.update_swim_state(false, swim_now_ms);
    return true;
}

// ---------- idle:water_escape ----------
// Open-world water-escape FOCUS (FIX #12). When the bot is in swim-water the
// builder resolves the nearest DRY navmesh footing (s.water_escape_*). This rule
// drives straight to it at priority 865 — above ALL travel/quest/wander movement
// (<700) — so while the bot is in the water NOTHING else moves it. Without this
// the bot oscillates forever: the move_to water-exit recovery pulls it to shore,
// then travel/wander immediately walk it back into the water (Tindle, Stormwind
// harbor, 2026-06-19). Sits just BELOW idle:swim_stuck (870): if the bot can't
// reach dry footing and stays swim-stuck long enough, swim_stuck's hub/hearth
// escalation takes over (its gate still runs every tick and updates the swim
// timer). Once the bot is on land the builder clears water_escape_valid and the
// normal rules (now-correct travel) resume.
bool WaterEscapeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    return s.water_escape_valid()
        && s.is_alive() && !s.in_combat() && !s.is_casting()
        && !s.on_transport()
        && !s.in_battleground() && !s.is_in_dungeon();
}

bool WaterEscapeFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter& emit, uint32)
{
    emit.move_to(s.water_escape_x(), s.water_escape_y(), s.water_escape_z(),
                 /*run*/ true);
    ai.set_last_rule_fired("idle:water_escape");
    return true;
}

// ---------- idle:group_convert_to_raid ----------
bool GroupConvertGate(BotSnapshotView const& s, BotAI&,
                     GroupSnapshotView const& g, uint32)
{
    return g.exists() && g.leader() == s.guid() && g.is_party() &&
           s.raw().instance_ctx.is_in_raid;
}

bool GroupConvertFire(BotSnapshotView const&, BotAI& ai,
                      GroupSnapshotView const&,
                      BotIntentEmitter& emit, uint32)
{
    emit.group_convert_to_raid();
    ai.set_last_rule_fired("idle:group_convert_to_raid");
    return true;
}

// ---------- idle:group_invite_accept ----------
// Edge-trigger gate: snapshot keeps `has_group_invite` true for a few ticks
// after the bot fires GroupAcceptIntent (world thread processes the accept
// async). The `invite_acked` BotAI flag dedups the emit within that window
// AND auto-clears when the invite goes away. The gate has a side effect
// (clearing the flag) — that's a one-off state-machine cleanup, runs once
// per invite cycle.
bool GroupInviteAcceptGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (!s.has_group_invite())
    {
        if (ai.invite_acked()) ai.set_invite_acked(false);
        if (ai.pending_group_invite_accept_at_ms() != 0)
            ai.set_pending_group_invite_accept_at_ms(0);
        return false;
    }
    if (ai.invite_acked()) return false;
    // Hesitation: 500–2500ms per-bot before accepting. Without this the
    // accept packet arrives <100ms after the invite, in lockstep across
    // an entire bot squad — easiest possible anti-bot tell.
    const uint32 now_ms = s.published_at_ms();
    uint32 accept_at = ai.pending_group_invite_accept_at_ms();
    if (accept_at == 0)
    {
        const uint32 jitter = 500u + (uint32(s.bot_id()) * 2654435761u) % 2000u;
        ai.set_pending_group_invite_accept_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= accept_at;
}

bool GroupInviteAcceptFire(BotSnapshotView const&, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    emit.group_accept();
    ai.set_invite_acked(true);
    ai.set_last_rule_fired("idle:group_invite_accept");
    return true;
}

// ---------- idle:guild_invite_accept ----------
bool GuildInviteAcceptGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (!s.has_guild_invite())
    {
        if (ai.guild_invite_acked()) ai.set_guild_invite_acked(false);
        return false;
    }
    return !ai.guild_invite_acked();
}

bool GuildInviteAcceptFire(BotSnapshotView const&, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    emit.guild_accept_invite();
    ai.set_guild_invite_acked(true);
    ai.set_last_rule_fired("idle:guild_invite_accept");
    return true;
}

// ---------- idle:lfg_proposal_accept ----------
bool LfgProposalAcceptGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (!s.has_lfg_proposal())
    {
        if (ai.lfg_proposal_acked_id() != 0) ai.set_lfg_proposal_acked_id(0);
        if (ai.pending_lfg_proposal_accept_at_ms() != 0)
            ai.set_pending_lfg_proposal_accept_at_ms(0);
        return false;
    }
    const uint32 pid = s.lfg_proposal_id();
    if (pid == 0 || ai.lfg_proposal_acked_id() == pid) return false;
    // Hesitation: real players see the LFG popup and pause to read the
    // role/dungeon line — 800–3000ms accept-at jitter. Without it the
    // entire queued group accepts inside 100ms, which is impossible for
    // a human-played party of 5.
    const uint32 now_ms = s.published_at_ms();
    uint32 accept_at = ai.pending_lfg_proposal_accept_at_ms();
    if (accept_at == 0)
    {
        const uint32 jitter = 800u + (uint32(s.bot_id()) * 2654435761u) % 2200u;
        ai.set_pending_lfg_proposal_accept_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= accept_at;
}

bool LfgProposalAcceptFire(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    const uint32 pid = s.lfg_proposal_id();
    emit.lfg_proposal_respond(pid, /*accept*/ true);
    ai.set_lfg_proposal_acked_id(pid);
    ai.set_last_rule_fired("idle:lfg_proposal_accept");
    return true;
}

// ---------- idle:bg_port_accept ----------
bool BgPortAcceptGate(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const&, uint32)
{
    const uint32 port_now_ms = GameTime::GetGameTimeMS();
    for (auto const& q : s.raw().bg.queues)
    {
        if (q.invited_to_instance == 0) continue;
        if (ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                     uint64(q.bg_type_id), port_now_ms))
            continue;
        return true;
    }
    return false;
}

bool BgPortAcceptFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const&,
                      BotIntentEmitter& emit, uint32)
{
    const uint32 port_now_ms = GameTime::GetGameTimeMS();
    for (auto const& q : s.raw().bg.queues)
    {
        if (q.invited_to_instance == 0) continue;
        if (ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                     uint64(q.bg_type_id), port_now_ms))
            continue;
        emit.bg_port(q.bg_type_id, /*accept*/ true);
        ai.note_action_retry(BotAI::ActionKind::BgPort,
                             uint64(q.bg_type_id), port_now_ms);
        ai.set_last_rule_fired("idle:bg_port_accept");
        return true;
    }
    return false;
}

// ---------- idle:bg_jit_staging ----------
// Owner directive 2026-06-22: a JIT bot (spawned ONLY to fill a BG queue) must do
// nothing but its purpose. While it holds a JIT purpose and is not yet inside the
// instance, confine it: ensure it's queued for its BG, then stand STAGED and wait
// for the invite — no questing, roaming, grinding, or vendor trips (those lower
// rules are suppressed because this consumes the tick). That confinement is also
// what makes matches FORM: staged bots stay queued + ready so both factions reach
// MinPlayers and the matchmaker can invite (JIT bots used to wander off / get into
// combat and never assemble, so invites never fired). An invited bot is handled by
// idle:bg_port_accept (847, just above this). A hard staging timeout clears the
// purpose so a bot whose match never forms reverts and the TotalTarget=0 LRU
// retires the surplus instead of it staging forever.
static constexpr uint32 kJitStageTimeoutMs = 12u * 60u * 1000u;

bool BgJitStagingGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ai.is_jit_purpose() && !s.in_battleground();
}

bool BgJitStagingFire(BotSnapshotView const& /*s*/, BotAI& ai, GroupSnapshotView const&,
                      BotIntentEmitter& /*emit*/, uint32)
{
    const uint32 now_ms = GameTime::GetGameTimeMS();
    // Time out a match that never formed: clear the purpose and yield so the
    // population manager's LRU (TotalTarget=0) retires the surplus bot.
    if (ai.jit_purpose_set_ms() != 0 &&
        (now_ms - ai.jit_purpose_set_ms()) > kJitStageTimeoutMs)
    {
        ai.clear_jit_purpose();
        return false;
    }
    // Stand staged: whether currently queued (waiting for the invite) or PARKED
    // between purposes (post-match, available for reuse), the bot does nothing but
    // wait — consuming the tick suppresses every lower behavior rule (no questing,
    // roaming, grinding, or vendor trips). It does NOT auto-re-queue: a parked JIT
    // bot stays idle and AVAILABLE so the filler's online-first pass can REUSE it
    // for the next purpose (avoiding the costly create+setup); if nothing reuses it,
    // its 10-min kick-protect lease lapses and the TotalTarget=0 LRU logs it out
    // (owner directive: no dead bots idling around, but reuse to save creation
    // load). The initial queue comes from the setup pipeline; reuse re-queues via
    // the filler.
    ai.set_last_rule_fired("idle:bg_jit_staging_wait");
    return true;
}

// ---------- idle:rejoin_group_instance ----------
// After server restart / DC the bot lands at homebind while the group's
// still inside their dungeon. Auto-teleport to a groupmate inside the
// instance so the human player doesn't have to abandon and re-form.
// Anchor selection prefers a non-bot human first; falls back to any
// bot already inside. Per-target-map 30s cooldown via the MailDrain
// ActionKind namespace + a high-bit-flagged key.
bool RejoinInstanceGate(BotSnapshotView const& s, BotAI&,
                        GroupSnapshotView const& g, uint32)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    return g.exists();
}

bool RejoinInstanceFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const& g,
                        BotIntentEmitter& emit, uint32)
{
    auto const* mems = g.members();
    if (!mems) return false;

    GroupMemberSummary const* anchor = nullptr;
    BotRegistry& reg_rejoin = Services::Registry();
    for (auto const& m : *mems)
    {
        if (!m.online) continue;
        if (m.guid == s.raw().guid) continue;
        if (reg_rejoin.has(m.guid.GetCounter())) continue;
        if (m.map_id == 0 || m.map_id == s.map_id()) continue;
        if (MapEntry const* me = sMapStore.LookupEntry(m.map_id))
            if (me->IsDungeon() || me->IsRaid())
            { anchor = &m; break; }
    }
    if (!anchor)
    {
        for (auto const& m : *mems)
        {
            if (!m.online) continue;
            if (m.guid == s.raw().guid) continue;
            if (m.map_id == 0 || m.map_id == s.map_id()) continue;
            if (MapEntry const* me = sMapStore.LookupEntry(m.map_id))
                if (me->IsDungeon() || me->IsRaid())
                { anchor = &m; break; }
        }
    }
    if (!anchor) return false;

    const uint32 now_ms = GameTime::GetGameTimeMS();
    const uint64 rejoin_key = (uint64(0xA2) << 56) | uint64(anchor->map_id);
    if (ai.action_recently_tried(BotAI::ActionKind::MailDrain, rejoin_key, now_ms))
        return false;
    emit.emit(TeleportToIntent{
        anchor->map_id, anchor->x, anchor->y, anchor->z, 0.f});
    ai.note_action_retry(BotAI::ActionKind::MailDrain, rejoin_key, now_ms);
    ai.set_last_rule_fired("idle:rejoin_group_instance");
    return true;
}

// ---------- idle:dungeon_loop_requeue ----------
bool DungeonLoopRequeueGate(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const& g, uint32)
{
    if (!ai.dungeon_loop_mode()) return false;
    if (ai.last_lfg_dungeon_id() == 0) return false;
    if (ai.dungeon_run_mode() != BotAI::DungeonRunMode::Off) return false;
    if (s.is_in_instance() || s.in_combat() || s.is_casting()) return false;
    if (!s.is_alive()) return false;
    // TC's WorldSession::HandleLfgJoinOpcode rejects LFG-join from non-
    // leaders — group queues as a whole, only leader drives it. Skip the
    // emit when grouped and non-leader so we don't spam an intent the
    // server will reject (5 bots × every 10s = 30 rejected joins/min per
    // 5-bot dungeon party).
    if (g.exists() && g.leader() != s.raw().guid) return false;
    const uint32 now_ms = s.published_at_ms();
    const uint32 last = ai.last_loop_requeue_ms();
    return last == 0 || (now_ms - last) >= 10000u;
}

bool DungeonLoopRequeueFire(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const&,
                            BotIntentEmitter& emit, uint32)
{
    Role role = ai.effective_role(s);
    if (role == Role::Unknown) role = s.my_role();
    if (role == Role::Unknown) role = Role::Dps;
    emit.lfg_queue(ai.last_lfg_dungeon_id(), role);
    ai.note_loop_requeue(s.published_at_ms());
    ai.set_last_rule_fired("idle:dungeon_loop_requeue");
    return true;
}

// ---------- idle:invite_nearby_player ----------
// Outgoing invite: solo bot >= L25 in a questing area (no rest bonus =
// not in a city/inn) invites a same-faction same-bracket nearby player
// to a group. Per-target 5-min cooldown via InviteOther absorbs declines.
bool InviteNearbyPlayerGate(BotSnapshotView const& s, BotAI&,
                            GroupSnapshotView const&, uint32)
{
    if (s.in_group() || s.has_group_invite()) return false;
    if (s.level() < 25) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().identity.rest_bonus_xp != 0) return false;
    return true;
}

bool InviteNearbyPlayerFire(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const&,
                            BotIntentEmitter& emit, uint32)
{
    const uint32 inv_now_ms = GameTime::GetGameTimeMS();
    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (u.guid.IsCreature()) continue;
        if (u.entry != 0) continue;
        if (u.guid == s.raw().guid) continue;
        if (u.is_in_group) continue;
        if (!u.victim.IsEmpty()) continue;
        const int dlvl = int(u.level) - int(s.level());
        if (dlvl > 5 || dlvl < -5) continue;
        float bx, by, bz; s.position(bx, by, bz);
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        constexpr float kInviteRangeSq = 30.0f * 30.0f;
        if (dx*dx + dy*dy + dz*dz > kInviteRangeSq) continue;
        const uint64 inv_key = u.guid.GetCounter();
        if (ai.action_recently_tried(BotAI::ActionKind::InviteOther,
                                     inv_key, inv_now_ms))
            continue;
        emit.invite_to_group(u.guid);
        ai.note_action_retry(BotAI::ActionKind::InviteOther,
                             inv_key, inv_now_ms);
        ai.set_last_rule_fired("idle:invite_nearby_player");
        return true;
    }
    return false;
}

} // anonymous namespace

void RegisterCoreFlowRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:swim_stuck";
        rule.priority = 870;
        rule.gate     = &SwimStuckGate;
        rule.fire     = &SwimStuckFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:water_escape";
        rule.priority = 865;   // below swim_stuck (870), above all travel/wander
        rule.gate     = &WaterEscapeGate;
        rule.fire     = &WaterEscapeFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:group_convert_to_raid";
        rule.priority = 860;
        rule.gate     = &GroupConvertGate;
        rule.fire     = &GroupConvertFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:group_invite_accept";
        rule.priority = 855;
        rule.gate     = &GroupInviteAcceptGate;
        rule.fire     = &GroupInviteAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_invite_accept";
        rule.priority = 853;
        rule.gate     = &GuildInviteAcceptGate;
        rule.fire     = &GuildInviteAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:lfg_proposal_accept";
        rule.priority = 850;
        rule.gate     = &LfgProposalAcceptGate;
        rule.fire     = &LfgProposalAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:bg_port_accept";
        rule.priority = 847;
        rule.gate     = &BgPortAcceptGate;
        rule.fire     = &BgPortAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        // 846: just below bg_port_accept (so an invited JIT bot ports in first),
        // above the entire quest/wander/grind/vendor band — this is the JIT
        // confinement (owner directive: JIT bots only do their purpose).
        IdleRule rule;
        rule.name     = "idle:bg_jit_staging";
        rule.priority = 846;
        rule.gate     = &BgJitStagingGate;
        rule.fire     = &BgJitStagingFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:rejoin_group_instance";
        rule.priority = 840;
        rule.gate     = &RejoinInstanceGate;
        rule.fire     = &RejoinInstanceFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:dungeon_loop_requeue";
        rule.priority = 836;
        rule.gate     = &DungeonLoopRequeueGate;
        rule.fire     = &DungeonLoopRequeueFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:invite_nearby_player";
        rule.priority = 830;
        rule.gate     = &InviteNearbyPlayerGate;
        rule.fire     = &InviteNearbyPlayerFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

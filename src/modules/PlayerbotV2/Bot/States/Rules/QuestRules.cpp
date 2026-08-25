// QuestRules - Refactor #3 pass 5. Migrates the quest-turnin, world-quest
// accept, world-quest turnin, and quest-abandon-overlevel idle rules out
// of the State_Idle linear cascade.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/States/MaintainHelpers.h"   // States::ChunkedWalkToward (far-goal chunked walk)
#include "Bot/QuestDoable.h"
#include "Fleet/JunkQuestResolver.h"
#include "World/WorldMetadata.h"
#include "Services.h"
#include "Travel/QuestHubDatabase.h"
#include "ObjectAccessor.h"
#include "UnitDefines.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace Playerbot {

namespace {

constexpr float kInteractSq = 5.0f * 5.0f;
// Accept is EMITTED only inside this tighter radius (3.5y), not the full 5y
// interact band. 2026-06-17: the server's live CanInteractWithQuestGiver gate
// (Player.cpp) is checked at intent-dispatch AFTER the bot may have drifted;
// emitting at the 5y snapshot edge raced the live gate and produced an endless
// out_of_range retry loop (the executor treats QuestAccept out_of_range as
// transient, no backoff). 3.5y leaves ~1.5-2y of drift headroom so the live
// check passes. kInteractSq is still used to pick the nearest giver / gate the
// approach legs. Invariant: kAcceptEmitSq must stay ABOVE the close-approach
// stop deadband (kQuestNoAct=3² in State_Idle.cpp) so accept fires while the
// redirect is already silent — no 5y triple-tie oscillation.
//
// 2026-06-19: raised 3.5y -> 4.5y. The close-approach often cannot park a bot
// inside 3.5y (sub-2y moves get dropped by the mover, or the giver sits behind
// terrain): live Velruun reached 3.9y from giver 1993 and stuck there forever —
// in the 3.5-5y DEAD BAND quest_accept neither emitted (>3.5y) nor recorded a
// near target for the approach (that only happened >5y), so it drifted 3.9-5.3y
// and never accepted. Quest accept is INSTANT (no cast), so 4.5y is safe — it
// still leaves 0.5y below the live 5y CanInteractWithQuestGiver gate, and the
// executor retries a transient out_of_range, so no infinite loop.
constexpr float kAcceptEmitSq = 4.5f * 4.5f;

// ---------- idle:wq_accept ----------
bool WqAcceptGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    for (auto const& wq : s.raw().quest_discovery.available_world_quests)
        if (wq.type == 0) return true;   // any offer to consider
    return false;
}

bool WqAcceptFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& wq : s.raw().quest_discovery.available_world_quests)
    {
        if (wq.type != 0) continue;
        const float dx = wq.x - bx, dy = wq.y - by, dz = wq.z - bz;
        if (dx*dx + dy*dy + dz*dz > kInteractSq) continue;
        emit.accept_quest(wq.giver, wq.quest_id);
        ai.set_last_rule_fired("idle:wq_accept");
        return true;
    }
    return false;
}

// ---------- idle:quest_turnin ----------
bool QuestTurninGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (s.raw().quest_discovery.quest_turnins.empty())
    {
        if (ai.pending_quest_turnin_at_ms() != 0) ai.set_pending_quest_turnin_at_ms(0);
        return false;
    }
    // Reward-choice hesitation: 1–4s. Bots used to slam Complete the same
    // tick the snapshot saw the turnin offer — a human looks at the
    // reward grid for a moment before clicking.
    const uint32 now_ms = s.published_at_ms();
    uint32 ready_at = ai.pending_quest_turnin_at_ms();
    if (ready_at == 0)
    {
        const uint32 jitter = 1000u + (uint32(s.bot_id()) * 2654435761u) % 3000u;
        ai.set_pending_quest_turnin_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= ready_at;
}

bool QuestTurninFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    const uint32 now_ms = s.published_at_ms();
    // Nearest in-scan ender that is OUT of interact range — used by the
    // close-approach leg below (2026-06-17). 271 COMPLETE quests were measured
    // stuck awaiting turn-in because the bot reached the ender's scan range but
    // the actual walk-to-ender lived in the starved autoact@50 cascade. Now that
    // this rule sits at 702 (above maintenance) it drives the final approach
    // itself so the quest actually gets turned in.
    float near_gx = 0.f, near_gy = 0.f, near_gz = 0.f;
    float near_dsq = std::numeric_limits<float>::max();
    bool  have_near = false;
    for (auto const& tin : s.raw().quest_discovery.quest_turnins)
    {
        // Skip quests whose reward turn-in is in failure back-off. The
        // executor records Result::Locked (CanRewardQuest failed) into a
        // per-quest escalating-TTL cache; re-emitting complete_quest here
        // would just reproduce the same rejection. Prevents the 120k-retry
        // loop seen on permanently-unrewardable quests (e.g. 26712).
        if (ai.quest_reward_failed_recent(tin.quest_id, now_ms))
            continue;
        float gx = 0.f, gy = 0.f, gz = 0.f;
        bool  found_pos = false;
        for (auto const& u : s.raw().combat.nearby_friends)
            if (u.guid == tin.giver) { gx = u.x; gy = u.y; gz = u.z; found_pos = true; break; }
        if (!found_pos)
            for (auto const& o : s.raw().world_objects.nearby_objects)
                if (o.guid == tin.giver) { gx = o.x; gy = o.y; gz = o.z; found_pos = true; break; }
        if (!found_pos) continue;
        const float dx = gx - bx, dy = gy - by, dz = gz - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq > kInteractSq)
        {
            if (dsq < near_dsq) { near_dsq = dsq; near_gx = gx; near_gy = gy; near_gz = gz; have_near = true; }
            continue;
        }
        // Face the questgiver before the click. Real humans don't
        // sidestep-click while looking sideways — they turn to the NPC.
        emit.face_target(tin.giver);
        // 0xFF = auto-pick best reward.
        emit.complete_quest(tin.giver, tin.quest_id, /*reward_choice*/ 0xFF);
        ai.set_last_rule_fired("idle:quest_turnin");
        return true;
    }
    // No ender in interact range, but one IS in scan — walk the last yards to it
    // (close-approach), with a wedge guard so an unreachable ender yields the
    // tick instead of looping.
    if (have_near)
    {
        if (ai.check_anchor_wedge("idle:walk_to_quest_ender", s.path_blocked_count(), now_ms))
            return false;
        // Walk to the ender's ACTUAL position — PathGenerator snaps the destination to
        // the nearest reachable navmesh poly (adjacent to the NPC, inside the 5y interact
        // band) and routes around local geometry. The old "stop 2.5y short" target
        // computed a point along the bot->ender line at the ender's z that was frequently
        // OFF-MESH (ender on a step/ledge/raised dais → the short point lands mid-air →
        // snaps to the wrong poly below → move_to path-fails → wedge). Harmless while
        // turnin sat starved at 430, this surfaced as the dominant wedge cluster once the
        // rule fired fleet-wide at 702. The path is short (<40y, ender is in scan) so it
        // is cheap. 2026-06-17.
        emit.move_to(near_gx, near_gy, near_gz, /*run*/ true);
        ai.set_last_rule_fired("idle:walk_to_quest_ender");
        return true;
    }
    return false;
}

// ---------- idle:wq_turnin ----------
bool WqTurninGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    for (auto const& wq : s.raw().quest_discovery.available_world_quests)
        if (wq.type == 1) return true;
    return false;
}

bool WqTurninFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& wq : s.raw().quest_discovery.available_world_quests)
    {
        if (wq.type != 1) continue;
        const float dx = wq.x - bx, dy = wq.y - by, dz = wq.z - bz;
        if (dx*dx + dy*dy + dz*dz > kInteractSq) continue;
        emit.complete_quest(wq.giver, wq.quest_id, /*reward_choice*/ 0xFF);
        ai.set_last_rule_fired("idle:wq_turnin");
        return true;
    }
    return false;
}

// ---------- idle:quest_accept ----------
bool QuestAcceptGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (ai.in_profession_mode(s.published_at_ms())) return false;
    // Full bags: accepting a quest that grants a starting item fails server-side
    // with InvFull, and re-emitting it every tick PINS the bot on the doomed
    // accept — starving the vendor / sell / buy-bag rules that would free space
    // (live: Velruun, full backpack, QuestAccept|InvFull looped forever). Yield
    // so the bot frees space the player way first; accept resumes once a slot
    // opens. bag_free_slots()==0 is the clean gate (deferring a no-item quest a
    // few ticks until space frees is harmless).
    if (s.bag_free_slots() == 0) return false;
    if (s.raw().quest_discovery.quest_offers.empty())
    {
        if (ai.pending_quest_accept_at_ms() != 0) ai.set_pending_quest_accept_at_ms(0);
        return false;
    }
    // Read-the-quest hesitation: 1.5–6s. Without this every bot that
    // walks up to a quest hub accepts every quest the same tick the
    // snapshot picks them up — 5 quests in <250ms across a hub. Real
    // players scroll through the description before clicking Accept.
    const uint32 now_ms = s.published_at_ms();
    uint32 ready_at = ai.pending_quest_accept_at_ms();
    if (ready_at == 0)
    {
        const uint32 jitter = 1500u + (uint32(s.bot_id()) * 2654435761u) % 4500u;
        ai.set_pending_quest_accept_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= ready_at;
}

bool QuestAcceptFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    // Nearest out-of-interact-range giver — used by the in-dungeon
    // approach leg below when no offer is acceptable from where we stand.
    float near_gx = 0.f, near_gy = 0.f, near_gz = 0.f;
    float near_dsq = std::numeric_limits<float>::max();
    for (auto const& off : s.raw().quest_discovery.quest_offers)
    {
        float gx = 0.f, gy = 0.f, gz = 0.f;
        bool  found_pos = false;
        for (auto const& u : s.raw().combat.nearby_friends)
            if (u.guid == off.giver) { gx = u.x; gy = u.y; gz = u.z; found_pos = true; break; }
        if (!found_pos)
            for (auto const& o : s.raw().world_objects.nearby_objects)
                if (o.guid == off.giver) { gx = o.x; gy = o.y; gz = o.z; found_pos = true; break; }
        if (!found_pos) continue;
        const float dx = gx - bx, dy = gy - by, dz = gz - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq > kInteractSq)
        {
            if (dsq < near_dsq) { near_dsq = dsq; near_gx = gx; near_gy = gy; near_gz = gz; }
            continue;
        }
        // Within the 5y interact band but outside the 3.5y accept-emit margin:
        // do NOT emit yet — let the close-approach (wander_to_quest_hub, now
        // delivering to ~2.5y) finish closing in, so the live interact gate has
        // drift headroom and we don't start the out_of_range retry loop.
        if (dsq > kAcceptEmitSq)
            continue;
        emit.face_target(off.giver);
        emit.accept_quest(off.giver, off.quest_id);
        // Hold follow-recall off while we work through a multi-quest hub —
        // grouped bots otherwise get yanked away between accepts.
        ai.grant_detour(s.published_at_ms());
        if (ai.hearth_zone() != s.zone_id())
        {
            constexpr float kInnBindRangeSq = 80.0f * 80.0f;
            if (auto const* inn = s.nearest_npc_with_flag(UNIT_NPC_FLAG_INNKEEPER))
            {
                const float ix = inn->x - bx, iy = inn->y - by;
                if (ix*ix + iy*iy <= kInnBindRangeSq)
                {
                    if (ix*ix + iy*iy <= kInteractSq)
                    {
                        emit.bind_homebind(inn->guid);
                        ai.set_last_homebind_innkeeper(inn->guid);
                    }
                    ai.set_hearth_zone(s.zone_id());
                }
            }
        }
        ai.set_last_rule_fired("idle:quest_accept");
        return true;
    }
    // Close-approach leg: walk the last few-to-50y to the nearest acceptable
    // offer giver, then the in-range accept above fires. This now runs in the
    // OPEN WORLD too, not just dungeons. The open-world approach was supposed to
    // be owned by walk_to_known_hub's in-scan-giver close-approach, but that
    // rule does not fire for a quest-having bot whose giver is in scan (live:
    // Velruun, offers from givers 28-37y away, 0 close-approach attempts over
    // minutes — it loops wander_to_service forever, 0 XP). quest_accept (this
    // rule, priority 701, gate confirmed passing) owns the whole flow instead:
    // detect offer -> walk to giver -> accept. No !is_moving gate (a bot that
    // lower rules keep wandering must still be driven to the giver; the move_to
    // executor dedups same-destination spam), out of combat only, detour lease
    // holds follow_recall off mid-walk. Bounded to 60y so it's a close-approach,
    // not a cross-zone trek (far givers stay with the travel/hub cascade).
    if (near_dsq < 60.0f * 60.0f && !s.in_combat())
    {
        const float dist = std::sqrt(near_dsq);
        const float scale = (dist - 2.5f) / dist;   // stop 2.5y — inside the 3.5y accept-emit margin
        emit.move_to(bx + (near_gx - bx) * scale,
                     by + (near_gy - by) * scale,
                     near_gz, /*run*/ true);
        ai.grant_detour(s.published_at_ms());
        ai.set_last_rule_fired(s.is_in_dungeon() ? "idle:dungeon_quest_approach"
                                                 : "idle:quest_accept_approach");
        return true;
    }
    return false;
}

// ---------- idle:quest_auto_accept ----------
// AUTO_ACCEPT chain-heads (QUEST_FLAGS_AUTO_ACCEPT 0x80000, e.g. 25152 "Your
// Place In The World") are auto-granted by a real client the instant it queries
// the giver. Bots have no client, and the normal idle:quest_accept path imposes
// a 1.5-6s "read the quest" hesitation (QuestAcceptGate) — long enough that a
// fresh starter bot fires idle:travel_to_hub and walks out of the giver's scan
// range first, dropping the offer. The chain-head then never lands and EVERY
// follow-up stays prev_quest-gated (observed live: Gorthak, L1 Orc, 2.5y from
// Kaltunk, never accepted 25152, wandered into Durotar). This rule grabs such
// offers with NO hesitation and, when just out of range, walks the last few
// yards to the giver instead of letting the bot relocate off the starter zone.
bool QuestAutoAcceptGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    for (auto const& off : s.raw().quest_discovery.quest_offers)
        if (off.auto_accept) return true;
    return false;
}

bool QuestAutoAcceptFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    float bx, by, bz; s.position(bx, by, bz);
    float near_gx = 0.f, near_gy = 0.f, near_gz = 0.f;
    float near_dsq = std::numeric_limits<float>::max();
    bool  have_near = false;
    for (auto const& off : s.raw().quest_discovery.quest_offers)
    {
        if (!off.auto_accept) continue;
        float gx = 0.f, gy = 0.f, gz = 0.f;
        bool  found_pos = false;
        for (auto const& u : s.raw().combat.nearby_friends)
            if (u.guid == off.giver) { gx = u.x; gy = u.y; gz = u.z; found_pos = true; break; }
        if (!found_pos)
            for (auto const& o : s.raw().world_objects.nearby_objects)
                if (o.guid == off.giver) { gx = o.x; gy = o.y; gz = o.z; found_pos = true; break; }
        if (!found_pos) continue;
        const float dx = gx - bx, dy = gy - by, dz = gz - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq <= kInteractSq)
        {
            // In interact range — accept the chain-head NOW (no hesitation).
            emit.face_target(off.giver);
            emit.accept_quest(off.giver, off.quest_id);
            ai.grant_detour(s.published_at_ms());
            ai.set_last_rule_fired("idle:quest_auto_accept");
            return true;
        }
        if (dsq < near_dsq) { near_dsq = dsq; near_gx = gx; near_gy = gy; near_gz = gz; have_near = true; }
    }
    // Approach leg: the auto-accept giver is known and near but out of interact
    // range. Walk the last stretch (out of combat, not already moving) so the
    // bot lands the chain-head instead of firing idle:travel_to_hub and
    // relocating off the starter zone. Bounded radius so it never chases a far
    // giver across the map (that's the wander/hub rules' job).
    constexpr float kAutoAcceptApproachSq = 60.0f * 60.0f;
    if (have_near && near_dsq < kAutoAcceptApproachSq && !s.in_combat() && !s.is_moving())
    {
        const float dist = std::sqrt(near_dsq);
        const float scale = (dist - 2.5f) / dist;   // stop ~2.5y out, inside interact range
        emit.move_to(bx + (near_gx - bx) * scale,
                     by + (near_gy - by) * scale,
                     near_gz, /*run*/ true);
        ai.grant_detour(s.published_at_ms());
        ai.set_last_rule_fired("idle:quest_auto_accept_approach");
        return true;
    }
    return false;
}

// ---------- idle:quest_abandon_unachievable ----------
bool QuestAbandonUnachievableGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    // Never abandon quests while in a BG / dungeon (BG audit S2). This rule
    // is priority 720 — ABOVE idle:bg_dispatch (718) — so without this gate a
    // bot that entered a BG still holding an un-abandonable quest (e.g. junk
    // 55660) fires this every tick, preempts bg_dispatch, and never plays the
    // objective. The quest is still there to clean up the moment it leaves.
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    for (auto const& q : s.raw().quest_log.quests)
    {
        // Structurally-unturnable junk (zero-obj + no ender, or blacklist —
        // e.g. 55660 "Time Trials") must be abandoned at ANY state, INCLUDING
        // complete (state==1): it can never be turned in and otherwise parks
        // the bot forever (current_quest_id=0 → [picker_none]). This was THE
        // dominant leveling-stall cause: 13,641 bots held it complete.
        if (q.unturnable) return true;
        if (q.unachievable && q.state != 1) return true;
    }
    return false;
}

bool QuestAbandonUnachievableFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    for (auto const& q : s.raw().quest_log.quests)
    {
        const bool junk         = q.unturnable;                      // abandon at any state
        const bool unachievable = q.unachievable && q.state != 1;    // incomplete + can't advance
        if (!junk && !unachievable) continue;
        ai.blacklist_quest(q.quest_id, s.raw().published_at_ms);
        emit.abandon_quest(q.quest_id);
        ai.set_last_rule_fired("idle:quest_abandon_unachievable");
        return true;
    }
    return false;
}

// ---------- idle:resolve_junk_quests ----------
// Auto-granted FEATURE quests (Quest::IsAutoPush() — 55660 "Time Trials" etc.)
// cannot be abandoned durably: TC core Player::PushQuests() re-adds them at
// every login. They must be force-COMPLETED to REWARDED instead (see
// JunkQuestResolver). Also resolves profession-spec choice quests. Runs ABOVE
// the abandon rule (priority 730 > 720) so an auto-push quest that is also
// flagged unturnable is COMPLETED (sticks) rather than abandoned (re-pushed).
bool ResolveJunkQuestsGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    // Defer junk-quest resolution out of BG / dungeon (BG audit S2): at
    // priority 730 this sits above idle:bg_dispatch (718) and would preempt
    // objective play every tick for a bot that entered with a resolvable junk
    // quest. Resolved the moment the bot is back in the open world.
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    for (auto const& q : s.raw().quest_log.quests)
        if (V2::Fleet::JunkQuestResolver::IsResolvableJunk(q.quest_id))
            return true;
    return false;
}

bool ResolveJunkQuestsFire(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    emit.resolve_junk_quests();
    ai.set_last_rule_fired("idle:resolve_junk_quests");
    return true;
}

// ---------- idle:quest_abandon_overlevel ----------
bool QuestAbandonOverlevelGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    // P1 (2026-06-16): MAX_QUEST_LOG_SIZE is 35 on the 12.0 client, not 25 —
    // the old >=24 fired this over-level shedding 11 slots too early. Trigger
    // only when one slot from full so a bot sheds an out-leveled quest only at
    // genuine capacity pressure.
    return s.raw().quest_log.quests.size() >= 34;
}

bool QuestAbandonOverlevelFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    const uint16 bot_level = s.level();
    for (auto const& q : s.raw().quest_log.quests)
    {
        if (q.state == 1) continue;
        if (q.flags & (0x02 | 0x04 | 0x08 | 0x10)) continue;  // daily/weekly/dungeon/raid
        if (q.level == 0) continue;
        if (q.level + 10 >= bot_level) continue;
        ai.blacklist_quest(q.quest_id, s.raw().published_at_ms);
        emit.abandon_quest(q.quest_id);
        ai.set_last_rule_fired("idle:quest_abandon_overlevel");
        return true;
    }
    return false;
}

// ---------- idle:walk_to_known_hub ----------
//
// Bots with zero quests in log AND no quest givers in snapshot scan
// range have nothing to chase, fall through to wander, then drift
// aimlessly. When the operator has annotated Hub points on the map,
// walk toward the closest one — getting the bot into a populated
// area where the snapshot's quest_offers will start surfacing.
bool WalkToKnownHubGate(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&, uint32 /*now_ms*/)
{
    (void)ai;
    // NOTE: mounted bots may STILL seek a hub — mounted travel is the faster,
    // correct path and emit.move_to is mount-agnostic. The old is_mounted
    // exclusion froze questless bots that had auto-mounted (live: Norethius
    // Mounted=true, ItemLevel 5, frozen 4+ min). 2026-06-17.
    if (s.in_combat() || s.is_casting())
        return false;
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    if (ai.in_profession_mode(s.published_at_ms())) return false;
    // (Bag-full handling lives in idle:bags_full_recover@710, which preempts.)
    // FIX 6 (group/owner exclusion): never pull a grouped bot toward a hub —
    // covers owner-following bots (owner's group) and all-bot groups alike.
    // Solo autonomous bots (ungrouped) still relocate.
    if (s.in_group()) return false;
    // Now that this rule sits at 700 (above the OOC maintenance band) it must
    // yield to a genuinely hurt bot so it heals before trekking. OOC heal is
    // benign to defer for a healthy-ish bot, but a <50% bot should not walk
    // across open terrain unhealed. (Accept/turn-in are instant so they need no
    // such deferral.) 2026-06-17.
    {
        auto const& v = s.raw().vitals;
        if (v.max_hp > 0 && uint64(v.hp) * 100u < uint64(v.max_hp) * 50u)
            return false;
    }
    // In-scan acceptable offer giver? quest_offers are builder-filtered to
    // genuinely takeable offers (CanTakeQuest + SatisfyQuestLog). The close-
    // approach in Fire below walks the bot to the giver so quest_accept (one
    // priority above) can grab it. This MUST run even for a bot that already
    // has quests: otherwise a quest-having bot sitting near an unaccepted offer
    // DEADLOCKS — pursue_quest_goal yields to the offer (it's "accept's job")
    // but nothing ever walks the bot into accept range, so it loops
    // wander_to_service forever (live: Velruun L11, 7 quests + 2 offers, 0 XP
    // for the whole session). Run the close-approach regardless of quest_log;
    // the far-hub questless-seek below stays questless-only.
    bool wk_has_inscan_offer = false;
    for (auto const& off : s.raw().quest_discovery.quest_offers)
    {
        for (auto const& u : s.raw().combat.nearby_friends)
            if (u.guid == off.giver) { wk_has_inscan_offer = true; break; }
        if (wk_has_inscan_offer) break;
        for (auto const& o : s.raw().world_objects.nearby_objects)
            if (o.guid == off.giver) { wk_has_inscan_offer = true; break; }
        if (wk_has_inscan_offer) break;
    }
    if (wk_has_inscan_offer)
    {
        // Futility back-off (Durnan wedge, 2026-06-21): if the close-approach has
        // been firing without ANY offer getting accepted for the window, the bot
        // can't reach the 3.5y accept-emit margin (a micro navmesh wedge at the
        // giver) and would loop HERE forever, permanently starving the lower
        // travel band (idle:far_same_map_travel@697 / pursue_quest_goal@698) — so
        // a bot with a far goal never travels (observed: Durnan L15, 4 in-scan
        // offers, position+XP frozen). Yield for a cooldown so the travel cascade
        // runs (and may reposition the bot out of the wedge), then retry fresh.
        // Normal close-approaches accept within seconds and never reach the
        // window, so the Velruun deadlock fix above is unaffected.
        const uint32 wk_now_ms = s.published_at_ms();
        if (wk_now_ms >= ai.hub_offer_backoff_until())
        {
            constexpr uint32 kHubOfferFutileMs  = 45000u;   // 45s w/o an accept = wedged
            constexpr uint32 kHubOfferBackoffMs = 60000u;   // yield 60s, then retry fresh
            if (ai.hub_offer_note_and_check_futile(
                    uint8(s.raw().quest_discovery.quest_offers.size()),
                    wk_now_ms, kHubOfferFutileMs))
            {
                ai.set_hub_offer_backoff(wk_now_ms + kHubOfferBackoffMs);
                ai.hub_offer_reset_window();   // fresh window after the back-off
                // fall through to yield (let far_travel / pursue run this tick)
            }
            else
                return true;   // close-approach to a reachable offer giver
        }
        // else: in futility back-off — fall through to yield so lower rules run.
    }
    else
    {
        ai.hub_offer_reset();   // no in-scan offer — clear the futility window
    }

    // No reachable offer to grab. Already have at least one quest? Let the
    // existing quest-walk rules pull us toward the objective POI / ender — the
    // far-hub seek below is only for the genuine "nothing to do" (questless) case.
    if (!s.raw().quest_log.quests.empty()) return false;
    // NOTE (2026-06-17): the old "no offer/turn-in in scan -> return false" gate
    // was REMOVED. It deferred near-giver bots to wander_to_quest_hub, which is
    // buried in autoact@50 and starved, so the ~69 bots measured within 60y of a
    // giver never closed in. This rule now (with the in-scan-giver target
    // preference in Fire) drives BOTH far bots (to the static hub anchor) AND
    // near bots (to the live in-scan giver) into accept range, where
    // quest_accept takes over. quest_log.empty() above keeps it questless-only.
    using ::Playerbot::V2::World::WorldMetadataKind;
    if (s.any_metadata_within(uint32(WorldMetadataKind::Hub), 1500.0f))
        return true;
    // REVIVE (2026-06-17): the operator-annotated Hub metadata table is EMPTY
    // fleet-wide (0 rows), so the clause above was always false and this rule —
    // the purpose-built questless-seek — was DEAD despite its priority, leaving
    // questless bots to grind/freeze in place forever. The real source of "a hub
    // exists for me" is QuestHubDatabase (1621 hubs, level+faction filtered).
    // GetNearestQuestHub takes a shared_lock + O(N) scan — forbidden on the
    // snapshot-Build thread but fine in this OFF-thread idle gate (same pattern
    // VendorRules already ships), bounded by the rule's 5s min_interval. It is
    // not map-filtered, but the Fire enforces same-map + HubHasDoableQuest, so a
    // cross-map-only candidate (e.g. on a hubless instanced map) passes the gate
    // then harmlessly falls through Fire (returns false) to the R7/autoact
    // cross-map relocation cascade.
    return Services::Initialized() && Services::Hubs().IsInitialized()
        && Services::Hubs().GetNearestQuestHub(s.raw()) != nullptr;
}

bool WalkToKnownHubFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32 /*now_ms*/)
{
    using ::Playerbot::V2::World::WorldMetadataStore;
    using ::Playerbot::V2::World::WorldMetadataKind;

    float bx, by, bz; s.position(bx, by, bz);
    float best_dsq = std::numeric_limits<float>::infinity();
    float bx_t = 0.f, by_t = 0.f, bz_t = 0.f;
    bool  have_target = false;
    uint32 picked_hub_id = 0;          // set when the target is a DB hub (for note_hub_tried)
    const uint32 wk_now_ms = s.published_at_ms();

    // PREFER A LIVE IN-SCAN GIVER (2026-06-17). If a quest offer's giver is in
    // the 40y snapshot scan, walk to its ACTUAL coord — this is the near-giver
    // close-approach that the starved wander_to_quest_hub@50 used to (fail to)
    // do. Targeting the live giver (not the static cluster anchor) lands the bot
    // on the RIGHT doable giver even in sprawling clusters; quest_accept (above
    // this rule) finishes the accept once inside 3.5y.
    for (auto const& off : s.raw().quest_discovery.quest_offers)
    {
        float gx = 0.f, gy = 0.f, gz = 0.f; bool found = false;
        for (auto const& u : s.raw().combat.nearby_friends)
            if (u.guid == off.giver) { gx = u.x; gy = u.y; gz = u.z; found = true; break; }
        if (!found)
            for (auto const& o : s.raw().world_objects.nearby_objects)
                if (o.guid == off.giver) { gx = o.x; gy = o.y; gz = o.z; found = true; break; }
        if (!found) continue;
        const float dx = gx - bx, dy = gy - by, dz = gz - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq < best_dsq) { best_dsq = dsq; bx_t = gx; by_t = gy; bz_t = gz; have_target = true; }
    }

    // FIX 5b: else prefer the nearest SAME-MAP QuestHubDatabase hub that STILL
    // has a doable quest for this bot (the shared HubHasDoableQuest predicate, so
    // this rule agrees with the builder + travel_to_hub about which hubs are
    // exhausted). Only fall back to the raw operator-annotated Hub metadata
    // points when no doable DB hub qualifies. The same-map guard is preserved on
    // BOTH paths — we never emit move_to toward a cross-map point.
    if (!have_target && Services::Initialized() && Services::Hubs().IsInitialized())
    {
        if (Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
        {
            ::Playerbot::V2::Travel::QuestHub const* picked = nullptr;
            float picked_dsq = std::numeric_limits<float>::infinity();
            // Collect same-map appropriate hubs nearest-first, then take the
            // first with a doable quest (early-exit predicate keeps this cheap).
            std::vector<std::pair<float, ::Playerbot::V2::Travel::QuestHub const*>> near;
            Services::Hubs().ForEach([&](::Playerbot::V2::Travel::QuestHub const& h)
            {
                if (h.mapId != s.map_id()) return;          // same continent only
                if (!h.IsAppropriateFor(s.raw())) return;   // level band + faction
                const float dx = h.location.GetPositionX() - bx;
                const float dy = h.location.GetPositionY() - by;
                near.emplace_back(dx*dx + dy*dy, &h);
            });
            std::sort(near.begin(), near.end(),
                      [](auto const& a, auto const& b) { return a.first < b.first; });
            for (auto const& dh : near)
            {
                // Skip hubs this bot recently tried unproductively (mesh-wedged
                // or arrived-but-exhausted) so it advances to the next candidate
                // instead of re-targeting the same unreachable/empty hub.
                if (ai.hub_recently_tried(dh.second->hubId, wk_now_ms)) continue;
                if (::Playerbot::HubHasDoableQuest(self, *dh.second))
                { picked = dh.second; picked_dsq = dh.first; break; }
            }

            if (picked)
            {
                best_dsq = picked_dsq;
                bx_t = picked->location.GetPositionX();
                by_t = picked->location.GetPositionY();
                bz_t = picked->location.GetPositionZ();
                picked_hub_id = picked->hubId;
                have_target = true;
            }
        }
    }

    if (!have_target)
    {
        auto rows = WorldMetadataStore::Instance().RecordsForMapAndKind(
            s.map_id(), WorldMetadataKind::Hub);
        if (rows.empty()) return false;
        for (auto const& r : rows)
        {
            const float dx = r.x - bx, dy = r.y - by;
            const float dsq = dx*dx + dy*dy;
            if (dsq < best_dsq) { best_dsq = dsq; bx_t = r.x; by_t = r.y; bz_t = r.z; }
        }
    }
    // Walk ALL the way to ~3y of the target (a live in-scan giver, or the static
    // hub anchor) — this rule now owns the full close approach (wander_to_quest_hub@50
    // stays starved so we cannot rely on it). quest_accept (above this rule) fires
    // once the bot is inside 3.5y, so in practice accept takes the tick before we
    // reach this yield. 2026-06-17 (was 60y, then 35y handoff to the starved
    // wander — both left the bot short of accept range).
    if (best_dsq <= 3.0f * 3.0f)
    {
        // Arrived. If the target was a static hub anchor and STILL no giver is in
        // scan (sprawling cluster — the doable giver sits elsewhere, or the hub
        // is exhausted), mark it tried so next tick advances to a different hub
        // instead of parking here. (A live-giver target inside 3.5y is handled by
        // quest_accept before we get here.)
        if (picked_hub_id != 0 && s.raw().quest_discovery.quest_offers.empty())
            ai.note_hub_tried(picked_hub_id, wk_now_ms);
        return false;
    }
    // Wedge guard (2026-06-17): a bot whose pathing to the hub keeps failing
    // (mesh gap / unreachable centroid) must give up on THAT hub and yield, not
    // re-aim it forever. Mark the DB hub tried so the next tick's picker advances
    // to a different candidate; surrender this tick so lower-priority rules can
    // untangle. Mirrors idle:travel_to_hub (State_Idle.cpp:7071-7076).
    if (ai.check_anchor_wedge("idle:walk_to_known_hub",
                              s.path_blocked_count(), wk_now_ms))
    {
        if (picked_hub_id != 0) ai.note_hub_tried(picked_hub_id, wk_now_ms);
        return false;
    }
    const float kStep =
        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                                                                       35.0f;
    const float dist = std::sqrt(best_dsq);
    const float scale = std::min(kStep, dist) / dist;
    const float tx = bx + (bx_t - bx) * scale;   // near waypoint, threat sweep only
    const float ty = by + (by_t - by) * scale;
    // Threat look-ahead: at hub-walk scale (25-50y steps) the corridor
    // is wider; sweep 12y half-width and engage a pullable hostile
    // instead of running into it.
    if (NearbyUnit const* threat = s.path_threat(
            tx, ty,
            /*max_forward*/ std::min(kStep, 35.0f),
            /*half_width*/  12.0f))
    {
        if (emit.start_attack(threat->guid))
        {
            ai.set_last_rule_fired("idle:walk_known_hub_pull_threat");
            return true;
        }
    }
    // Full-path to the hub anchor — same 2026-06-17 conversion as pursue_quest_goal /
    // walk_to_quest_ender: PathGenerator routes around geometry (bounded by PathBudget
    // + 1.5s move_to dedup) and move_to's far-goal handling escapes local minima. The
    // old greedy 35y hop aimed straight at the centroid and trapped bots on terrain.
    emit.move_to(bx_t, by_t, bz_t, /*run*/ true);
    ai.set_last_rule_fired("idle:walk_to_known_hub");
    return true;
}

// ---------- idle:pursue_quest_goal ----------
// P2 (2026-06-16, keystone): the quest-objective APPROACH walk lived only in the
// catch-all autoact dispatch at priority 50 — BELOW the opportunistic maintenance
// band (idle:equip_upgrade 600, vendor_visit 500, gather 440, loot 222). So a bot
// with a far quest objective fired equip/loot/gather/wander IN PLACE and never
// walked to its goal (live: GoalUnreachable wedges dominated by idle:equip_upgrade;
// 293/371 online bots questless; the stranded goals were intra-city ~525y, i.e.
// walkable — not a taxi gap). This rule lifts the objective approach ABOVE the
// opportunistic band so questing is the primary drive. It only closes DISTANCE;
// the in-range kill/turn-in/talk + cross-map travel logic still runs in autoact
// once the bot arrives. Nearby accept/turn-in keep their own rules — this gate
// yields whenever an offer/turn-in is already in scan range.
bool QuestPursueGoalGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    // A bot actively running a dungeon must NOT chase an outside-the-instance
    // quest objective — its goal POI is almost always in the open world, so
    // pursuing it walks the bot off the dungeon route (and into dead-end
    // geometry where it then NOPATHs and wedges; observed live in Deadmines:
    // the tank wandered to a stuck poly and reverted to idle:pursue_quest_goal).
    // The dungeon dispatch (priority 720) owns movement while the run is active;
    // any in-instance kill credit is earned through the pulls it drives.
    if (ai.dungeon_active()) return false;
    // Belt-and-braces for the LFG run-mode arming delay. dungeon_active() (run
    // mode) is the IDEAL gate — it carves out a bot escorting a human quester in
    // a dungeon — but it arms a tick or more AFTER the teleport into a finder-
    // formed instance (the group lacks GROUP_FLAG_LFG, and last_lfg_dungeon_id /
    // lfg_in_dungeon can lag the port). During that window DungeonDispatch (720)
    // is skipped and this rule (698) drove a quest-archetype bot off the route
    // chasing an outside POI before the run took over — observed 2026-06-26:
    // Dungmage walked 80y toward q27785 and then stranded on the cohesion gate,
    // wedging the whole run at the entrance. So ALSO yield when the bot is
    // PHYSICALLY inside an instance and the group is all-bots on this map: a
    // pure-bot dungeon group never pursues an outside quest. The human-escort
    // carve-out is preserved — when a human groupmate is on our map, fall through
    // (the POI map_id == map_id check below still confines pursuit to in-instance
    // objectives the human is sharing).
    if (s.is_in_dungeon())
    {
        bool human_here = false;
        if (g.exists())
            if (auto const* mems = g.members())
                for (auto const& m : *mems)
                    if (!m.is_bot && m.online && m.map_id == s.map_id())
                    { human_here = true; break; }
        if (!human_here) return false;
    }
    if (ai.in_profession_mode(s.published_at_ms())) return false;
    // Critically-broken gear (durability ~0): 0-durability items contribute 0
    // stats, so the bot does almost no damage and DIES pulling/closing on quest
    // mobs — the Morthan death-spiral (broke + 0% gear -> quests into death ->
    // loses 10% dur per death -> stays broken). Yield so idle:critical_repair@735
    // owns the tick: repair (or sell-the-hoard-to-fund-repair) FIRST, then quest.
    // Threshold 5%: above this the gear still functions enough to quest-and-earn,
    // so a merely-low-durability bot keeps questing (the existing behavior).
    if (s.lowest_equipped_durability_pct() <= 5) return false;
    // (Bag-full handling lives in idle:bags_full_recover@710, which preempts this
    // rule and drives a vendor trip; when it yields—nothing to sell—the bot
    // SHOULD keep questing here to gain combat XP from kills, so no bag gate.)
    // Now at 698 (above the OOC maintenance band) — yield to a genuinely hurt
    // bot so it heals before trekking to a far goal. 2026-06-17.
    {
        auto const& v = s.raw().vitals;
        if (v.max_hp > 0 && uint64(v.hp) * 100u < uint64(v.max_hp) * 50u)
            return false;
    }
    // Yield to a nearby offer ONLY when its giver is actually in scan — i.e. the
    // close-approach (walk_to_known_hub) + quest_accept can walk to it and grab it,
    // after which this resumes. An offer whose giver is NOT in scan can't be
    // approached or accepted from here, so yielding to it strands the bot in place
    // (the old `!quest_offers.empty()` blanket-yield deadlocked bots that had a
    // current goal AND a distant unaccepted offer). When no offer is reachable,
    // pursue the current objective instead.
    for (auto const& off : s.raw().quest_discovery.quest_offers)
    {
        bool giver_inscan = false;
        for (auto const& u : s.raw().combat.nearby_friends)
            if (u.guid == off.giver) { giver_inscan = true; break; }
        if (!giver_inscan)
            for (auto const& o : s.raw().world_objects.nearby_objects)
                if (o.guid == off.giver) { giver_inscan = true; break; }
        if (giver_inscan) return false;   // accept/close-approach owns this
    }
    if (!s.raw().quest_discovery.quest_turnins.empty()) return false;  // turn-in owns this
    auto const& poi = s.current_objective_poi();
    if (!poi.valid || poi.map_id != s.map_id()) return false;          // cross-map = autoact travel cascade
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = poi.x - bx, dy = poi.y - by;
    const float d2 = dx * dx + dy * dy;
    // Own the ROUTABLE band (80-2500y) with FULL-PATH routing in Fire — PathGenerator
    // detours around geometry, bounded by the PathBudget per-tick cap + the 1.5s move_to
    // dedup. Beyond 2500y the goal is cross-continent-scale → defer to the flight/taxi
    // cascade. This SUPERSEDES the earlier 300y greedy cap: greedy 35y-hop pursuit headed
    // straight at the goal and trapped in local minima (the 200y+ wedge cluster); full-path
    // routing fixes that the same way it fixed walk_to_quest_ender (62→14 wedges). 2026-06-17.
    // Beyond 2500y a SAME-MAP goal is handled by idle:far_same_map_travel@697
    // (FarTravelRules.cpp) — proactive flight/taxi + chunk-walk — which used to be
    // starved at autoact@50; CROSS-MAP goals still defer to the AutoactDispatch
    // portal/ship relocation cascade.
    return d2 > (80.0f * 80.0f) && d2 <= (2500.0f * 2500.0f);
}

bool QuestPursueGoalFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    auto const& poi = s.current_objective_poi();
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = poi.x - bx, dy = poi.y - by;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return false;
    // Lethal blocker: if a mob FAR above the bot's level sits in the immediate path
    // (aggro / one-shot range), do NOT keep walking into it. A goal whose corridor
    // crosses high-level content (e.g. a phased war-campaign zone overlapping a
    // low-level questing area) is unreachable for this bot — yield so the NoProgress
    // watchdog blacklists it and the picker chooses a survivable quest, instead of
    // marching into a one-shot every tick (live: Morthan L9 walking his classic
    // Tirisfal Darkhound quest straight through L50 Blighted Soldiers). +12 only
    // trips on genuinely lethal mobs, so normal tough-mob questing is unaffected.
    {
        const int myLvl = int(s.level());
        for (auto const& e : s.raw().combat.nearby_enemies)
        {
            if (!e.guid.IsCreature() || e.hp <= 0) continue;
            if (int(e.level) < myLvl + 12) continue;          // not lethal
            const float ex = e.x - bx, ey = e.y - by;
            if (ex * ex + ey * ey <= 40.0f * 40.0f)           // within aggro/one-shot range
            {
                ai.set_last_rule_fired("idle:pursue_quest_goal_lethal_block");
                return false;   // abandon this tick; watchdog blacklists the goal
            }
        }
    }
    const float kStep =
        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                                                                       35.0f;
    const float scale = std::min(kStep, dist) / dist;
    const float tx = bx + dx * scale, ty = by + dy * scale;   // near waypoint, threat sweep only
    // Threat look-ahead, same as the other approach walks.
    if (NearbyUnit const* threat = s.path_threat(tx, ty, std::min(kStep, 35.0f), 10.0f))
    {
        if (emit.start_attack(threat->guid))
        {
            ai.set_last_rule_fired("idle:pursue_quest_pull_threat");
            return true;
        }
    }
    // FAR goals (> 250y): a single full-path move_to(goal) hits the per-tick
    // PathBudget cap and PathGenerator returns an INCOMPLETE path (~245y), so the
    // bot wedges ~400y+ short and never closes — GoalUnreachable (live: Morthan on
    // the Tirisfal Forsaken starters Q24990/24993/6323, objectives 678-881y out;
    // cur_to_dst climbing 678->881 as the partial path led it nowhere). Route
    // through ChunkedWalkToward: bounded ~35y chunks ALWAYS path completely and
    // steadily close distance, with bearing-deflection on stalls and a 6-strike
    // give-up that yields to the wedge-watchdog. Same engine that cleared Durnan's
    // far travel; systemic for the far-goal wedge cluster. 2026-06-22.
    constexpr float kFarChunkThreshold = 250.0f;
    if (dist > kFarChunkThreshold)
        return States::ChunkedWalkToward(s, ai, emit, poi.x, poi.y, poi.z,
                                         "idle:pursue_quest_goal", now_ms);

    // Near/mid (<= 250y): wedge-guard, then full-path. PathGenerator returns a
    // COMPLETE route at this range and navmesh-routes the whole way around obstacles
    // (canals/walls/doors) — the 2026-06-17 fix that took walk_to_quest_ender 62->14
    // wedges. The anchor-wedge guard yields here (walled-off pocket / mesh gap) so the
    // lower cascade / watchdog recovers instead of looping into a wall.
    if (ai.check_anchor_wedge("idle:pursue_quest_goal", s.path_blocked_count(), now_ms))
        return false;
    emit.move_to(poi.x, poi.y, poi.z, /*run*/ true);
    ai.set_last_rule_fired("idle:pursue_quest_goal");
    return true;
}

} // anonymous namespace

void RegisterQuestRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:walk_to_known_hub";
        // P3 (2026-06-17): raised 615 -> 700, ABOVE the entire OOC MAINTENANCE
        // band (auto_equip 690, talents 692-696, ooc_heal 686, ooc_food 671,
        // self_buff 682, soulstone 680, conjure 672, loot 660, ...). 615 was
        // STILL starved: a questless bot almost always has SOMETHING in 660-696
        // to do (eat/buff/conjure/loot), so the seek rule never won the tick and
        // walk_to_known_hub fired ~2x/fleet/27min — the fleet sat 66% questless,
        // completions ~0 (MEASURED). The whole quest funnel was below maintenance.
        // 700 sits in the only free window (696 < x < 703 assist) so it beats
        // routine maintenance but still YIELDS to assist/pet/combat/survival
        // (>=703). Seeking a quest is OOC + a questless bot has no maintenance
        // urgency; the gate also yields when HP<50% so a hurt bot heals first.
        rule.priority = 700;  // RE-RAISED Stage 2 (2026-06-17) - safe under PathBudget per-tick cap
        rule.gate     = &WalkToKnownHubGate;
        rule.fire     = &WalkToKnownHubFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    // quest_turnin highest funnel priority (in-range COMPLETE quest = a finished
    // quest + reward + often a ding — the literal completion metric). 2026-06-17:
    // raised 430 -> 702, ABOVE the OOC maintenance band, so a bot standing at the
    // turn-in NPC hands the quest in (one instant action) BEFORE wandering off to
    // eat/buff/loot. This was a dominant completion leak: bots with a complete
    // quest sat doing maintenance instead of turning it in.
    {
        IdleRule rule;
        rule.name     = "idle:quest_turnin";
        rule.priority = 702;  // RE-RAISED Stage 2
        rule.gate     = &QuestTurninGate;
        rule.fire     = &QuestTurninFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:wq_turnin";
        rule.priority = 425;
        rule.gate     = &WqTurninGate;
        rule.fire     = &WqTurninFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:quest_accept";
        // 2026-06-17: raised 423 -> 701, ABOVE the OOC maintenance band. A bot
        // standing at a quest giver must ACCEPT (one instant action) before it
        // wanders off to eat/buff/conjure. At 423 (below maintenance 660-696) the
        // ~69 bots measured AT/NEAR a giver never accepted — they maintained
        // instead. Below quest_turnin (702) so finishing beats starting.
        rule.priority = 701;  // RE-RAISED Stage 2
        rule.gate     = &QuestAcceptGate;
        rule.fire     = &QuestAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:quest_auto_accept";
        // 703: above idle:quest_accept (701) and idle:quest_turnin (702), in the
        // top-of-tick band so it beats idle:travel_to_hub / wander. No hesitation
        // gate — AUTO_ACCEPT chain-heads must land the instant the giver is in
        // range, before the bot relocates off the starter zone. The gate is a
        // cheap presence check; the fire returns false (falls through to lower
        // rules) when the only auto-accept giver is out of range and unreachable
        // this tick, so it never starves the rest of the registry.
        rule.priority = 703;
        rule.gate     = &QuestAutoAcceptGate;
        rule.fire     = &QuestAutoAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:wq_accept";
        rule.priority = 420;
        rule.gate     = &WqAcceptGate;
        rule.fire     = &WqAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:quest_abandon_unachievable";
        // HIGH band (>=700) so it runs in the TOP-of-tick registry pass, BEFORE
        // the legacy inline movement/travel rules. At its old priority 415 it
        // sat in the BOTTOM-of-tick pass (<700), so any bot with a stale move
        // goal fired an inline walk/travel rule first (and path-failed), and the
        // abandon was NEVER reached — 13,482 bots held junk quest 55660 with 0
        // abandons fleet-wide. Dropping a structurally-unturnable quest is cheap
        // maintenance with no movement; doing it first unblocks the picker
        // immediately. Below survival/transport holds (900+), above everything
        // that moves the bot.
        rule.priority = 720;
        rule.gate     = &QuestAbandonUnachievableGate;
        rule.fire     = &QuestAbandonUnachievableFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:resolve_junk_quests";
        // Above the abandon rule (720): auto-push feature quests must be
        // force-completed (sticks), not abandoned (re-pushed at next login).
        rule.priority = 730;
        rule.gate     = &ResolveJunkQuestsGate;
        rule.fire     = &ResolveJunkQuestsFire;
        // Throttle: clearing is one quest per fire; a few seconds between
        // attempts is plenty and keeps the gate's per-quest cache lookups cheap.
        rule.min_interval_ms = 2000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:quest_abandon_overlevel";
        rule.priority = 410;
        rule.gate     = &QuestAbandonOverlevelGate;
        rule.fire     = &QuestAbandonOverlevelFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:pursue_quest_goal";
        // P3 keystone (2026-06-17): walk to the current objective POI (a far KILL/
        // ITEM target OR the turn-in ender of a COMPLETE quest). Raised 620 -> 698,
        // ABOVE the OOC maintenance band (660-696). At 620 it was STARVED: a
        // quested bot almost always had eat/buff/conjure/loot work in 660-696, so
        // it pursued nothing and 271 COMPLETE quests sat un-turned-in (MEASURED via
        // .playerbot quests). 698 sits just under accept/walk (700-701) and turnin
        // (702) and below assist/combat/survival (>=703); the gate yields when HP
        // is low so a hurt bot heals first (added below). Questing now beats
        // incidental maintenance for bots that HAVE a goal.
        rule.priority = 698;  // RE-RAISED Stage 2
        rule.gate     = &QuestPursueGoalGate;
        rule.fire     = &QuestPursueGoalFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

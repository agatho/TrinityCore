// State_InGroup - Cross-cutting layer that runs alongside the primary state
// whenever the bot is grouped. Today: keep within follow distance of the
// leader when idle. Future: ready-checks, raid mark targeting, role hand-off.

#include "StateBase.h"
#include "MaintainHelpers.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Group/GroupSnapshot.h"
#include "Bot/Formation.h"
#include "Bot/Dungeon/DungeonScript.h"
#include "Bot/BotRegistry.h"
#include "Services.h"
#include "Travel/UnifiedTravelGraph.h"
#include "SharedDefines.h"
#include <cmath>
#include <limits>

namespace Playerbot::States {

namespace {

float DistanceXY(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

// Same melee-spec table as State_InCombat::IsMeleeSpec. Duplicated locally
// rather than adding a cross-state header — both pieces are small and the
// definitions need to stay in sync per spec.
bool IsMeleeClass(uint8 cls, uint32 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_ROGUE:
        case CLASS_DEMON_HUNTER:
        case CLASS_MONK:
            return true;
        case CLASS_HUNTER:
            return spec == 255;          // Survival
        case CLASS_DRUID:
            return spec == 103 || spec == 104; // Feral / Guardian
        case CLASS_SHAMAN:
            return spec == 263;          // Enhancement
        default:
            return false;
    }
}

} // anonymous

void DispatchInGroup(BotAI& ai,
                     BotSnapshotView snapshot,
                     GroupSnapshotView group,
                     BotIntentEmitter& emit)
{
    if (!group.exists()) return;

    // Auto-accept any pending player-summon dialog (warlock ritual, meeting
    // stone, BG / LFG summon). Runs before the alive check so dead bots being
    // summoned to a graveyard / instance entrance still accept (the API
    // forwards to Player::SummonIfPossible which doesn't require alive).
    // Edge-trigger isn't necessary: snapshot.has_summon_pending flips false
    // server-side as soon as SummonIfPossible runs, so the next snapshot
    // won't re-trigger.
    // Auto-respond to LFG role-check ping. Server is waiting for every
    // member to declare a role before pushing the queue. Bot's spec maps
    // 1:1 to a role (Tank/Healer/Dps); add LEADER bit when bot is the
    // group leader so the role-check accepts. Without this the whole
    // group blocks indefinitely if any bot member doesn't respond.
    if (snapshot.lfg_role_check_pending())
    {
        uint8 lfg_role = 0;
        switch (ai.effective_role(snapshot))
        {
            case Role::Tank:   lfg_role = /*TANK*/   2; break;
            case Role::Healer: lfg_role = /*HEALER*/ 4; break;
            case Role::Dps:    lfg_role = /*DAMAGE*/ 8; break;
            default:           lfg_role = /*DAMAGE*/ 8; break;
        }
        if (group.leader() == snapshot.raw().guid)
            lfg_role |= /*LEADER*/ 1;
        emit.lfg_role_check(lfg_role);
        ai.set_last_rule_fired("ingroup:lfg_role_check");
    }
    // Proactive role-set for manually-formed dungeon groups (e.g.
    // `;all run` builds a party with bots; no LFG round-trip happens
    // so `lfg_role_check_pending` is always false and roles never get
    // assigned). When dungeon_active and the bot hasn't published its
    // role to the group, push it once via the same API (the API now
    // falls back to Group::SetLfgRoles direct when no LFG check is
    // pending). One-shot: `lfg_role_published_` BotAI flag dedups
    // across ticks. Resets when the bot's effective role changes
    // (spec swap) or the group changes (re-form).
    else if (ai.dungeon_active() && snapshot.lfg_published_role() == 0)
    {
        uint8 lfg_role = 0;
        switch (ai.effective_role(snapshot))
        {
            case Role::Tank:   lfg_role = /*TANK*/   2; break;
            case Role::Healer: lfg_role = /*HEALER*/ 4; break;
            case Role::Dps:    lfg_role = /*DAMAGE*/ 8; break;
            default:           lfg_role = /*DAMAGE*/ 8; break;
        }
        if (group.leader() == snapshot.raw().guid)
            lfg_role |= /*LEADER*/ 1;
        emit.lfg_role_check(lfg_role);
        ai.set_last_rule_fired("ingroup:set_role_no_lfg");
    }

    // Auto-leave on vote-kick. When the player initiates an LFG
    // vote-to-kick on someone in the group, the bot bows out
    // voluntarily so the kick succeeds and LFG backfills the slot
    // with a fresh bot. TC's LFG vote-kick API doesn't expose WHO
    // the target is, so we accept the false-positive risk: if the
    // vote targeted a human / different bot, this bot leaves anyway
    // and the player just gets a refreshed group. Player intent of
    // "kick a bot" still resolves correctly.
    // Per-vote-kick lockout via BgPort ActionKind (re-used since
    // vote-kicks and BG ports never overlap in the same group).
    if (snapshot.lfg_vote_kick_active())
    {
        const uint32 vk_now = GameTime::GetGameTimeMS();
        const uint64 vk_key = uint64(group.leader().GetCounter()) ^ 0xDEADBEEFu;
        if (!ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                      vk_key, vk_now))
        {
            emit.group_leave();
            ai.note_action_retry(BotAI::ActionKind::BgPort, vk_key, vk_now);
            ai.set_last_rule_fired("ingroup:vote_kick_auto_leave");
            return;
        }
    }

    // Auto-promote-human-to-leader. Bots end up as group leader when
    // LFG promotes the first-spawned bot, when the human gets kicked
    // and reformed, or when bots form a group among themselves and a
    // human later joins. The human should drive — they manage pulls.
    // When THIS bot is leader AND any non-bot human is in the group,
    // hand over leadership. Picks the first non-bot human in the
    // member list (stable enough; could be refined to prefer owner).
    if (group.leader() == snapshot.raw().guid)
    {
        BotRegistry& reg_promote = Services::Registry();
        ObjectGuid promote_target;
        if (auto const* mems_promote = group.members())
        {
            for (auto const& m : *mems_promote)
            {
                if (!m.online || m.guid == snapshot.raw().guid) continue;
                if (reg_promote.has(m.guid.GetCounter())) continue;  // is a bot
                promote_target = m.guid;
                break;
            }
        }
        if (!promote_target.IsEmpty())
        {
            // Edge-trigger: without this dedup the rule re-emits
            // CMSG_SET_PARTY_LEADER every snapshot tick (~3-5 Hz) for the
            // window between intent emit and the leader hand-off taking
            // effect. Server treats subsequent emits as no-ops but each
            // still pays packet-queue + lock cost. ActionKind::BgPort
            // slot is reused (mutually exclusive with vote-kick/promote).
            const uint32 promote_now_ms = GameTime::GetGameTimeMS();
            const uint64 promote_key =
                uint64(promote_target.GetCounter()) ^ 0xCAFEBABEu;
            if (!ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                           promote_key, promote_now_ms))
            {
                emit.group_promote_to_leader(promote_target);
                ai.note_action_retry(BotAI::ActionKind::BgPort,
                                     promote_key, promote_now_ms);
                ai.set_last_rule_fired("ingroup:promote_human_leader");
            }
        }
    }

    // Auto-share newly accepted quests with the party. Each shared quest
    // is recorded in BotAI's shared_ring_ (16-slot circular buffer). Walk
    // the log in order; share the first incomplete quest not already in
    // the ring. Old single-id tracking would alternate sharing two quests
    // forever — the ring fixes that. Skip repeatable/dungeon/raid quests
    // (already universal or party-context-specific).
    // Suppress entirely when the bot is in a BG raid OR a dungeon
    // instance: prep-phase quest-share spam clutters the popup queue for
    // the real player; bots can't progress open-world quests inside
    // either type of instance anyway. The BG raid is recreated per
    // match. Dungeon groups are also typically formed via LFG and the
    // user just wants to run the dungeon — share spam is pure noise.
    if (group.exists() && snapshot.is_alive() &&
        !snapshot.in_battleground() &&
        !snapshot.is_in_dungeon())
    {
        for (auto const& q : snapshot.raw().quest_log.quests)
        {
            if (q.state == 1) continue;
            if (q.flags & (0x01 | 0x08 | 0x10)) continue;
            if (ai.quest_recently_shared(q.quest_id)) continue;
            emit.share_quest(q.quest_id);
            ai.note_shared_quest(q.quest_id);
            ai.set_last_rule_fired("ingroup:share_quest");
            break;
        }
    }

    if (snapshot.has_quest_share())
    {
        // Auto-accept group-shared quests. Server pre-validated eligibility
        // (CanTakeQuest, CanAddQuest, level / class / faction gates) before
        // the share popup ever fires; the API call here just commits the
        // accept and clears m_playerSharingQuest. ServerRefused on the rare
        // race where state changed between share and our accept (log filled
        // up between ticks, etc.) — the next snapshot will drop the share
        // and we won't re-emit.
        //
        // Edge-trigger: m_playerSharingQuest is cleared by the server one
        // tick AFTER the accept lands, so without a dedup we re-emit
        // accept_shared_quest every tick for the round-trip window.
        // Mirror the promote_human_leader pattern using the BgPort
        // ActionKind slot (mutually exclusive with vote-kick/promote).
        const uint32 sq_now_ms = GameTime::GetGameTimeMS();
        constexpr uint64 sq_key = uint64(0xC0DECAFE'1u);
        if (!ai.action_recently_tried(BotAI::ActionKind::BgPort, sq_key, sq_now_ms))
        {
            emit.accept_shared_quest();
            ai.note_action_retry(BotAI::ActionKind::BgPort, sq_key, sq_now_ms);
            ai.set_last_rule_fired("ingroup:share_quest_accept");
        }
    }

    if (snapshot.has_summon_pending())
    {
        // Decline summons while the bot is in a battleground / dungeon
        // instance — accepting would yank the bot out and forfeit its slot.
        // The owner can re-summon after the bot leaves the instance. Use
        // map-based detection: snapshot.in_battleground() covers PvP, and
        // any encounter-active state (active_encounter_npc_id != 0) means
        // we're mid-fight in a scripted dungeon and shouldn't port out.
        if (snapshot.in_battleground() ||
            snapshot.raw().dungeon_exec.active_encounter_npc_id != 0)
        {
            emit.summon_decline();
            ai.set_last_rule_fired("ingroup:summon_decline");
        }
        else
        {
            emit.summon_accept();
            ai.set_last_rule_fired("ingroup:summon_accept");
        }
        // Don't return — let the rest of the InGroup dispatch run too. The
        // teleport happens on the world thread; this tick's other emits stay
        // valid (target the bot's current position) and naturally lapse on
        // arrival when the next snapshot inverts the relevant ranges.
    }

    if (!snapshot.is_alive()) return;

    // Auto-respond ready when the leader starts a ready check. Bots that don't
    // answer block the rest of the group from pulling. Edge-triggered: emit
    // exactly once per active window (cleared when the snapshot reports the
    // check no longer active). Server-side response is idempotent but the
    // intent still costs queue slots, so dedup is worth it.
    if (group.ready_check_active())
    {
        if (!ai.ready_check_acked())
        {
            emit.emit(GroupReadyResponseIntent{true});
            ai.set_ready_check_acked(true);
            ai.set_last_rule_fired("ingroup:ready_check_ack");
        }
    }
    else if (ai.ready_check_acked())
    {
        ai.set_ready_check_acked(false);
    }

    // Auto-skull: when this bot is the group leader (or assistant in raids),
    // and the group is engaged, mark the lowest-HP attacking enemy with skull
    // (icon 7). Picks the lowest-HP target from the union of "things attacking
    // me" and "things attacking my groupmates", because that's the kill order
    // a competent leader would call. Skip if a live skull is already set —
    // re-marking mid-fight thrashes assist targets. Trigger only on the
    // leader's tick; the published skull propagates to all bot members
    // through the next group snapshot.
    if (snapshot.in_combat() && !group.leader().IsEmpty() &&
        group.leader() == snapshot.raw().guid)
    {
        const ObjectGuid current_skull = group.skull_target();
        bool skull_alive = false;
        if (!current_skull.IsEmpty())
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == current_skull && u.hp > 0) { skull_alive = true; break; }
        if (!skull_alive)
        {
            // Skull-pick preference order:
            //   1. attackers (units in combat with us) whose entry is in
            //      the per-dungeon script's high_priority_kill_entries —
            //      these are the "kill first" adds the script knows about
            //      (e.g. Cult Adherent on Deathwhisper, Bone Spike on
            //      Marrowgar).
            //   2. lowest-HP among generic attackers (no priority entry hit).
            //   3. lowest-HP among nearby_enemies (out-of-combat trash hint).
            DungeonAdvice const advice = Services::Initialized()
                ? Services::Dungeons().GetAdvice(snapshot)
                : DungeonAdvice{};
            ObjectGuid pick;
            int32 best_hp = std::numeric_limits<int32>::max();
            // Priority pass.
            if (!advice.high_priority_kill_entries.empty())
            {
                for (auto const& u : snapshot.raw().combat.attackers)
                {
                    if (u.hp <= 0) continue;
                    bool prio = false;
                    for (uint32_t e : advice.high_priority_kill_entries)
                        if (e == u.entry) { prio = true; break; }
                    if (!prio) continue;
                    if (u.hp < best_hp) { best_hp = u.hp; pick = u.guid; }
                }
            }
            auto is_env_ignored = [&](uint32_t entry) -> bool
            {
                for (uint32_t ie : advice.ignore_entries)
                    if (ie == entry) return true;
                return false;
            };
            // Fallback: lowest-HP attacker.
            if (pick.IsEmpty())
            {
                for (auto const& u : snapshot.raw().combat.attackers)
                {
                    if (u.hp <= 0) continue;
                    if (is_env_ignored(u.entry)) continue;
                    if (u.hp < best_hp) { best_hp = u.hp; pick = u.guid; }
                }
            }
            if (pick.IsEmpty())
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                {
                    if (u.hp <= 0) continue;
                    if (is_env_ignored(u.entry)) continue;
                    if (u.hp < best_hp) { best_hp = u.hp; pick = u.guid; }
                }
            if (!pick.IsEmpty())
            {
                // Retry pacing: when the executor refuses the set (Locked —
                // bad group state / invalid target), an unthrottled emit
                // re-fired the SAME pick every tick (~6/sec, observed on the
                // 2026-06-11 BG orphans). One attempt per pick per 10s.
                const uint32 icon_now = GameTime::GetGameTimeMS();
                if (!ai.action_recently_tried(BotAI::ActionKind::RaidIcon,
                                              pick.GetCounter(), icon_now))
                {
                    emit.set_raid_target_icon(/*SKULL=*/7, pick);
                    ai.note_action_retry(BotAI::ActionKind::RaidIcon,
                                         pick.GetCounter(), icon_now);
                    ai.set_last_rule_fired("ingroup:auto_skull");
                }
            }
        }
    }

    // Don't override combat-state actions — the primary InCombat dispatch is
    // already running the APL.
    if (ai.state() == BotState::InCombat) return;

    const ObjectGuid leader = group.leader();
    if (leader.IsEmpty() || leader == snapshot.raw().guid) return;

    auto const* members = group.members();
    if (!members) return;

    GroupMemberSummary const* leader_member = nullptr;
    for (auto const& m : *members)
        if (m.guid == leader && m.online) { leader_member = &m; break; }
    if (!leader_member) return;

    if (leader_member->map_id != snapshot.map_id()) return;

    // Bot's current world position — needed for both anchor sanity checks
    // and the follow-recall distance test further below. Computed once and
    // reused so the anchor-on-tank fallback can compare distances.
    float bx, by, bz;
    snapshot.position(bx, by, bz);

    // Off-mesh crossing commitment — honored FIRST, before any anchor/assist/follow.
    // A follower mid-jump over the Gap-1 void that gets routed to the InGroup layer
    // (assist/follow alongside the primary state) must COMPLETE the hop, not chase a
    // target across the gap — chasing replaces the off-mesh spline and strands it on
    // the off-mesh poly (NoPath thereafter; observed live 2026-06-26). Shared with
    // the idle/InCombat paths so the crossing is uninterruptible in every state.
    if (ai.dungeon_active() &&
        DungeonHonorCross(snapshot, ai, emit, snapshot.published_at_ms()))
        return;

    // Anchor selection:
    //   * Healer (always): stick to the tank. Being out of range to heal
    //     is the most common cause of group wipes from bot positioning.
    //   * DPS in dungeon-run mode: also stick to the tank. The tank drives
    //     the pull cadence and the pull location; DPS following the human
    //     leader instead would leave them out of melee on the wrong mob
    //     when the leader wanders. Outside dungeon-run mode DPS stays on
    //     the leader so /follow during travel still works.
    //   * Otherwise: anchor on the group leader.
    GroupMemberSummary const* anchor = leader_member;
    Role const my_role = ai.effective_role(snapshot);
    bool const anchor_on_tank =
        my_role == Role::Healer ||
        (my_role == Role::Dps && ai.dungeon_active());
    if (anchor_on_tank)
    {
        if (auto const* t = group.tank())
        {
            if (t->online && t->map_id == snapshot.map_id() && t->guid != snapshot.raw().guid)
            {
                // Sanity gate: only commit to anchoring on the tank when the
                // tank is plausibly reachable. Same-map alone isn't enough —
                // a tank that zoned ahead through a portal we haven't crossed
                // yet, or one that ran far ahead through a door, leaves the
                // bot pathfinding through walls because the navmesh can't
                // reach the tank's tile. Heuristic: tank within 60yd OR not
                // far behind the leader. If both fail, anchor on the leader
                // instead — leader-anchor is the safe default that always
                // produces sane follow paths. Observed 2026-05-15 after
                // `;all run`: 3 bots wall-ran trying to reach a tank that
                // had already pulled into the next room.
                const float tank_dist = DistanceXY(bx, by, t->x, t->y);
                const float lead_dist = DistanceXY(bx, by,
                                                    leader_member->x,
                                                    leader_member->y);
                if (tank_dist <= 60.0f || tank_dist <= lead_dist + 20.0f)
                    anchor = t;
            }
        }
    }

    // Stranded-member recovery runs BEFORE the per-state rejoin: a follower that
    // respawned at the far entrance graveyard (or wedged on a disconnected poly)
    // cannot traverse back on foot and would otherwise oscillate the ingroup rejoin
    // below forever while the group holds for it (the sole-rezzer-healer harbor
    // stall, 2026-06-27). Relocate it onto the group so it can rejoin / rez.
    if (ai.dungeon_active() &&
        DungeonRecoverStrandedFollower(snapshot, ai, group, emit,
                                       snapshot.published_at_ms()))
        return;

    // Converge-to-fight: a DPS lagging in the 37-40y dead-band while the tank
    // is soloing a mob it cannot finish is walked into the fight (off-mesh
    // aware) so the assist below engages the tank's victim. Runs after strand
    // recovery and before the >40y rejoin (which only fires past the dead-band).
    if (ai.dungeon_active() &&
        DungeonConvergeToFight(snapshot, ai, group, emit,
                               snapshot.published_at_ms()))
        return;

    // ── Dungeon cohesion: rejoin a far tank BEFORE fighting ──────────────
    // A follower stranded far from the tank — the Gap-1 mouth case, where the
    // only visible enemies are ACROSS the offmesh bridge — must CROSS to the tank
    // first. Assisting a target it can't reach pins it at the chokepoint while
    // the tank fights the boss-room trash UNHEALED and the group wipes (observed
    // live 2026-06-26 Deadmines: healer + 2 DPS stuck at the mouth running
    // assist/opener across the bridge, tank died, 3/5 dead). Cross now; the assist
    // and follow below resume the instant the bot has rejoined the tank. Tank-
    // distance (not anchor) so it fires even when the >60y anchor sanity gate
    // above fell back to the leader. The tank's heal-leash holds it for us.
    if (ai.dungeon_active() && my_role != Role::Tank)
    {
        if (auto const* tk = group.tank())
        {
            if (tk->online && tk->is_alive && tk->guid != snapshot.raw().guid &&
                tk->map_id == snapshot.map_id() &&
                DistanceXY(bx, by, tk->x, tk->y) > 40.0f)
            {
                if (!snapshot.victim().IsEmpty())
                    emit.stop_attack();
                // Emit the SAME off-mesh-aware far-vertex goal the idle
                // regroup-cross (and in-combat rule 0b) emits — NOT a plain
                // move_to(tank). When a follower oscillates between Idle
                // (regroup → far vertex of the Gap-1 bridge) and InGroup
                // (this rejoin), two DIFFERENT goal-keys reach the spline:
                // move_to dedups on the goal, so each state-switch restarts
                // the spline at the bridge mouth and the follower never
                // completes the off-mesh hop — the observed Gap-1 flip-flop
                // (healer + hunter pinned at -214.7,-490.7 alternating
                // ingroup:dungeon_rejoin_tank / idle regroup-cross, never
                // crossing). DungeonStepTowardTank returns the bridge's SOLID
                // far vertex when the step crosses the off-mesh, so this
                // rejoin and the idle cross now share one stable goal-key and
                // the commit-crossing completes. Falls back to a plain
                // move_to(tank) only when no NORMAL path step resolves
                // (same-stratum follow, no bridge to commit to).
                {
                    float rx, ry, rz; bool roff = false;
                    if (DungeonStepTowardTank(snapshot.raw().guid,
                                              tk->x, tk->y, tk->z, 45.0f,
                                              rx, ry, rz, roff))
                    {
                        // When the step crosses an off-mesh bridge, COMMIT: store
                        // the far vertex so DungeonHonorCross (run first in every
                        // state) keeps re-asserting it and the jump completes even
                        // if combat would otherwise interrupt it mid-span.
                        if (roff)
                            ai.set_dungeon_cross(rx, ry, rz,
                                                 snapshot.published_at_ms() + 12000);
                        emit.move_to(rx, ry, rz, /*run=*/true);
                    }
                    // Direct path to the tank is gap-broken (NoPath). Before the
                    // doomed beeline, try to rejoin ALONG the crumb-route — the
                    // tank reached its spot by route-following down a descent the
                    // straight line NoPaths (RFC Adarogg pit); the follower must
                    // descend the SAME navigable crumbs, not beeline across the
                    // gap and strand the whole group on the cohesion gate.
                    else if (DungeonRouteStepTowardPos(snapshot,
                                                       tk->x, tk->y, tk->z, 45.0f,
                                                       rx, ry, rz))
                    {
                        emit.move_to(rx, ry, rz, /*run=*/true);
                        ai.set_last_rule_fired("ingroup:dungeon_rejoin_tank_route");
                        return;
                    }
                    else
                        emit.move_to(tk->x, tk->y, tk->z, /*run=*/true);
                }
                ai.set_last_rule_fired("ingroup:dungeon_rejoin_tank");
                return;
            }
        }
    }

    // Combat assist: if a group member is in combat with a target and we
    // don't have one ourselves, engage their target. Skull-marked target
    // beats both — DPS coordination convention is "kill skull first."
    // Tanks pick their own targets via threat; they ignore assist but DO
    // honor skull (so they can pull/taunt the focus mob).
    //
    // Aggression personality gates whether the bot proactively engages:
    //   - Passive: never assist on its own. Owner-pinned focus_target still
    //     wins (focus is an explicit owner directive, not assist), and the
    //     bot still defends itself via the InCombat dispatch when attacked.
    //     Useful for "follow but don't fight" RP / escort scenarios.
    //   - Defensive: assist only when the group is taking attrition damage
    //     (any same-map member <50% HP). Skips skull-mark assist and the
    //     leader-victim pile-on when the group looks healthy — Defensive
    //     bots conserve resources and let the tank manage trash. Owner
    //     focus + dropping below the wound threshold still engages.
    //   - Normal / Aggressive: full assist chain (focus → skull → leader
    //     victim → any in-combat member's victim). Identical for now;
    //     Aggressive is reserved for future opportunistic-pulling rules.
    const Aggression agg = ai.personality().aggression;
    const bool has_focus_pin = !ai.focus_target().IsEmpty();
    bool group_is_wounded = false;
    if (agg == Aggression::Defensive)
    {
        for (auto const& m : *members)
        {
            if (!m.online || m.map_id != snapshot.map_id() || m.hp <= 0) continue;
            if (m.max_hp > 0 && (int64(m.hp) * 100) < (int64(m.max_hp) * 50))
            { group_is_wounded = true; break; }
        }
    }
    // Combat-in-progress override: if ANY same-map group member is
    // currently in combat, every non-Passive bot assists. This is a
    // symptom-driven signal — we *see* combat happening — and is more
    // reliable than the dungeon-active flag (which can be inconsistent
    // across the squad when `;all run` doesn't land on every bot, or
    // when the squad is fighting world-content without the dungeon
    // mode set). Real-player intent: "the group is fighting, I help."
    // Defensive personality stops being "wait for HP to drop below 50%"
    // and becomes "join the fight already in progress." Passive stays
    // opted out (owner explicitly told it to never assist).
    bool any_member_in_combat = false;
    for (auto const& m : *members)
    {
        if (!m.online || m.map_id != snapshot.map_id()) continue;
        if (m.guid == snapshot.raw().guid) continue;     // skip self
        if (m.in_combat) { any_member_in_combat = true; break; }
    }
    const bool engage_allowed =
        (agg != Aggression::Passive)
            ? (agg != Aggression::Defensive || group_is_wounded ||
               has_focus_pin || any_member_in_combat)
            : has_focus_pin;
    if (engage_allowed)
    {
    if (snapshot.victim().IsEmpty())
    {
        ObjectGuid assist_target;
        // Owner-pinned focus target wins over everything. /focus <name>
        // sets ai.focus_target() so the owner can micromanage which add the
        // bot prioritizes (kicks, dispels, spike damage). Cleared via
        // /focus clear or when the focus dies (drops out of nearby_enemies).
        if (ObjectGuid focus = ai.focus_target(); !focus.IsEmpty())
        {
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == focus && u.hp > 0) { assist_target = focus; break; }
            if (assist_target.IsEmpty())
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == focus && u.hp > 0) { assist_target = focus; break; }
        }
        // Skull mark wins for everyone (after focus). Only use it if it's
        // actually nearby — out-of-vision skulls are stale. Defensive bots
        // skip skull pile-ons (the tank can manage their own kill order)
        // unless the group is already wounded enough to need extra DPS.
        //
        // CRITICAL: non-tanks must wait for the tank (or another group
        // member) to be engaged with the skull before opening on it.
        // Skull is "kill priority", NOT "fire at will" — a DPS bot
        // opening on skull before tank engages is the #1 overpull
        // source. Tanks always honor skull (they're the ones who
        // should be pulling it). Observed 2026-05-15: Defensive
        // warlock attacked skull immediately on mark, before tank
        // had any threat. Causes loss-of-aggro wipes.
        if (assist_target.IsEmpty() &&
            (agg != Aggression::Defensive || group_is_wounded))
        {
            if (ObjectGuid skull = group.skull_target(); !skull.IsEmpty())
            {
                const bool is_tank = ai.effective_role(snapshot) == Role::Tank;
                bool skull_engaged_by_member = false;
                if (!is_tank)
                {
                    for (auto const& m : *members)
                    {
                        if (!m.online || m.map_id != snapshot.map_id()) continue;
                        if (m.guid == snapshot.raw().guid) continue;
                        if (m.in_combat && m.victim == skull)
                        { skull_engaged_by_member = true; break; }
                    }
                }
                if (is_tank || skull_engaged_by_member)
                {
                    // Skip dead skull — corpses linger in nearby_enemies and
                    // we'd start_attack a body until raid leader re-marks.
                    for (auto const& u : snapshot.raw().combat.nearby_enemies)
                        if (u.guid == skull && u.hp > 0) { assist_target = skull; break; }
                    if (assist_target.IsEmpty())
                        for (auto const& u : snapshot.raw().combat.attackers)
                            if (u.guid == skull && u.hp > 0) { assist_target = skull; break; }
                }
            }
        }
        // Non-tanks fall back to leader's / any in-combat member's victim.
        // The copied guid MUST resolve against our own hostile lists
        // (nearby_enemies / attackers — both IsValidAttackTarget-gated at
        // build time) before we engage it. member.victim is published
        // verbatim from Unit::GetVictim(), and a member wedged on an
        // INVALID victim (2026-06-11 Deadmines: healer stuck swinging at
        // a groupmate's hunter pet) would otherwise propagate that victim
        // to the whole party — bots visibly "fighting each other" while
        // every cast rejects BAD_TARGETS and the run stalls.
        if (assist_target.IsEmpty() && ai.effective_role(snapshot) != Role::Tank)
        {
            auto resolves_hostile = [&](ObjectGuid v) -> bool
            {
                if (v.IsEmpty()) return false;
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                    if (u.guid == v && u.hp > 0) return true;
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == v && u.hp > 0) return true;
                // Creature-typed victims are trusted even when outside our
                // own capped 16-entry/40y scan (big pulls evict, range/LoS
                // hide) — refusing them left bots standing idle mid-fight
                // (user re-report 2026-06-11 Stockades). The friendly-fire
                // wedge class this guard exists for involved Player/Pet
                // guids only, and start_attack's IsValidAttackTarget gate
                // refuses anything that slips through at engage time.
                return v.IsCreature();
            };
            if (resolves_hostile(leader_member->victim))
                assist_target = leader_member->victim;
            if (assist_target.IsEmpty())
                for (auto const& m : *members)
                    if (m.online && m.in_combat && resolves_hostile(m.victim))
                    { assist_target = m.victim; break; }
        }
        // Defend-member engage (ALL roles, tanks very much included): a mob
        // whose victim is a group member is a pull in progress even when NO
        // member's own victim pointer is set — ranged/spell pulls never call
        // Unit::Attack on the puller, so a hunter (e.g. the user pulling
        // manually with Auto Shot) has GetVictim()==null and the whole
        // member-victim chain above sees nothing while the mob runs the
        // puller down (user report 2026-06-11 Stockades: "even if I start
        // pulling mobs the whole group doesn't react"). The enemy scan
        // publishes each mob's victim; engage the nearest visible mob
        // that's attacking one of ours.
        if (assist_target.IsEmpty())
        {
            float sx, sy, sz; snapshot.position(sx, sy, sz);
            float best_dsq = std::numeric_limits<float>::max();
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
            {
                if (u.hp <= 0 || u.victim.IsEmpty() || !u.in_los) continue;
                bool attacks_member = (u.victim == snapshot.raw().guid);
                if (!attacks_member)
                    for (auto const& m : *members)
                        if (m.guid == u.victim) { attacks_member = true; break; }
                if (!attacks_member) continue;
                const float ddx = u.x - sx, ddy = u.y - sy, ddz = u.z - sz;
                const float dsq = ddx*ddx + ddy*ddy + ddz*ddz;
                if (dsq < best_dsq) { best_dsq = dsq; assist_target = u.guid; }
            }
        }
        if (!assist_target.IsEmpty() && assist_target != snapshot.victim())
        {
            emit.start_attack(assist_target);
            ai.set_last_rule_fired("ingroup:assist");
        }
    }
    else
    {
        // Bot already has a victim. Honor focus pivots first, then skull
        // pivots. Focus is owner-pinned and outranks the raid leader's
        // skull mark; both override the bot's pre-engaged target. Defensive
        // bots still pivot off focus (owner directive) but skip skull
        // pivots when the group is healthy — same conserve-resource logic
        // as the open-engage path.
        if (ObjectGuid focus = ai.focus_target();
            !focus.IsEmpty() && focus != snapshot.victim())
        {
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == focus && u.hp > 0)
                {
                    emit.start_attack(focus);
                    ai.set_last_rule_fired("ingroup:focus_pivot");
                    return;
                }
        }
        if (agg != Aggression::Defensive || group_is_wounded)
        {
            if (ObjectGuid skull = group.skull_target();
                !skull.IsEmpty() && skull != snapshot.victim())
            {
                // Same tank-or-member-engaged gate as the open-engage path.
                // Pivoting off the current target onto skull before anyone
                // has aggro on it is just as bad as opening on it cold.
                const bool is_tank = ai.effective_role(snapshot) == Role::Tank;
                bool skull_engaged_by_member = false;
                if (!is_tank)
                {
                    for (auto const& m : *members)
                    {
                        if (!m.online || m.map_id != snapshot.map_id()) continue;
                        if (m.guid == snapshot.raw().guid) continue;
                        if (m.in_combat && m.victim == skull)
                        { skull_engaged_by_member = true; break; }
                    }
                }
                if (is_tank || skull_engaged_by_member)
                {
                    for (auto const& u : snapshot.raw().combat.nearby_enemies)
                        if (u.guid == skull && u.hp > 0)
                        {
                            emit.start_attack(skull);
                            ai.set_last_rule_fired("ingroup:skull_pivot");
                            break;
                        }
                }
            }
        }
    }
    }

    // bx/by/bz already computed above at anchor-selection time.
    (void)bz;
    const float dist = DistanceXY(bx, by, anchor->x, anchor->y);

    // Anchor has moved past the slack distance — issue a follow. The follow
    // movement generator is sticky, so re-issuing every tick is wasteful;
    // we only emit when the gap exceeds the bot's preferred follow distance.
    // Healers/casters trail further so they don't eat melee cleaves; melee /
    // tanks stack tight to be in their threat range. Owner can override the
    // role-based default via /follow_distance N — pinned value wins (lets
    // the owner micro-position around encounter mechanics).
    const bool ranged = snapshot.my_role() == Role::Healer
                     || (snapshot.my_role() == Role::Dps && !IsMeleeClass(snapshot.cls(), snapshot.spec()));
    float kFollowDistance = ranged ? 12.0f : 5.0f;
    float kRecallSlack    = ranged ? 18.0f : 8.0f;
    if (ai.follow_distance() > 0.f)
    {
        kFollowDistance = ai.follow_distance();
        // Recall slack scales proportionally — pin 8yd → ~12yd recall, etc.
        kRecallSlack = kFollowDistance * 1.5f;
    }
    // In a battleground, the BG-objective rules in State_Idle drive
    // movement (node capture, flag carry, role patrol). Following the BG
    // raid leader here would stomp those move_to emits and pile all bots
    // onto whichever player the BG queue auto-assigned as leader, which
    // is the bug behind "bots stay idle at the BG start area". Skip the
    // follow when bg_active so the BG rules win. Combat assist / skull
    // pivots above still fire, and the InCombat dispatch still drives
    // attack rotations once a target is acquired.
    const bool in_active_bg = snapshot.in_battleground() && ai.bg_active();
    // Dungeon-run movement ownership: while dungeon-run mode is Active, the
    // dungeon dispatch in State_Idle owns ALL squad movement — the tank via
    // idle:dungeon_tank_advance/boss_nav/tank_pull_next, and every non-tank
    // via idle:dungeon_regroup_follow_tank (a navmesh-validated move_to that
    // advances on partial paths and CROSSES off-mesh bridges) plus the
    // idle:dungeon_hold cohesion floor. State_InGroup's generic follow must
    // stand down for the WHOLE squad, not just the tank.
    //
    // For the TANK: follow-recall would yank it back to the LFG group leader
    // (often a DPS), fighting tank_advance — the observed
    // `follow_recall <-> tank_waypoint` oscillation that stalls the tank.
    //
    // For NON-TANKS: dispatch_layers (this code) runs AFTER dispatch_primary
    // (State_Idle), so a follow-recall here re-emits MoveFollow every tick and
    // OVERRIDES the regroup move_to that State_Idle just issued. MoveFollow has
    // no partial-path fallback and PINS against an off-mesh bridge — observed
    // live 2026-06-25 at the Deadmines foundry Gap-1 bridge: tank + 1 dps
    // crossed, healer + 2 dps stuck 55y back on ingroup:follow_recall while
    // their dungeon_regroup move_to (which one bot used to cross successfully)
    // was clobbered the very next tick. Generic follow also anchors on the LFG
    // leader rather than the tank, which is wrong for a dungeon. Below the
    // 22y regroup threshold the regroup is silent and idle:dungeon_hold simply
    // holds position — that IS the dungeon cohesion target, so no follow is
    // needed there either. (The dungeon dispatch always re-anchors on g.tank().)
    const bool dungeon_run_owns_movement = ai.dungeon_active();
    // Detour lease: a purposeful idle move (loot/skin a corpse, walk to an
    // in-dungeon quest giver) granted itself a few seconds of slack — honor
    // it instead of yanking the bot back mid-errand (the follow_recall <->
    // move_to_corpse ping-pong starved looting for whole runs). Bounded:
    // a fight in the group or a real separation (>45y) overrides the lease.
    constexpr float kDetourMaxRange = 45.0f;
    const bool detour_leased = ai.detour_active(snapshot.published_at_ms()) &&
                               !any_member_in_combat && !snapshot.in_combat() &&
                               dist < kDetourMaxRange;
    if (!in_active_bg && !dungeon_run_owns_movement && !detour_leased &&
        dist > kRecallSlack && !snapshot.is_rooted())
    {
        // Follow uses MotionMaster MoveFollow, which silently re-emits forever
        // when the leader is unreachable (across a gap / closed door) — and
        // grouped bots are exempt from GlobalStuckRescue, so they have no other
        // recovery. Track whether the gap is actually closing; if it stalls for
        // ~5s, fall back from follow to an explicit, navmesh-validated move_to
        // toward the leader. A failed move_to (Locked) feeds path_blocked +
        // last_move_to, which engages the walk-first unstick ladder (walk-escape
        // toward the nearest known-good node). No teleport.
        constexpr uint32 kFollowStaleTicks  = 25;    // ~5s at 5 Hz — gap not closing
        constexpr uint32 kFollowEscapeTicks = 100;   // ~20s — leader unreachable
        // M-P2b: note_follow_dist counts a tick "stale" whenever the gap
        // doesn't shrink by ≥2y. But when the leader is MOVING and the
        // follower is chasing at a matched pace, the gap holds roughly
        // constant even though MoveFollow is working perfectly — the
        // generator is tracking a moving target. The old code let stale
        // climb in that case and then yanked the bot off MoveFollow into
        // an explicit move_to (and eventually the escape-node path),
        // interrupting a follow that was actually fine.
        //
        // The bot being IN MOTION is the direct symptom of "following is
        // working": MoveFollow is producing movement toward the (moving)
        // leader. So while the bot is moving, treat the follow as healthy —
        // reset the stale counter and stay on the cheap MoveFollow path.
        // The stale escalation only fires when the bot is NOT moving (truly
        // wedged: MoveFollow re-emitting forever against an unreachable
        // leader, bot pinned in place). The proper, position-exact fix
        // (reset on leader-position-changed-since-last-tick) needs a
        // per-bot last-anchor-XY field on BotAI, which this file cannot add.
        //   // M-P2b: precise leader-moved reset needs last_anchor_x/y on BotAI.
        const bool bot_in_motion = snapshot.raw().movement.is_moving;
        uint32 stale;
        if (bot_in_motion)
        {
            // Following is actively progressing — don't let a constant gap
            // accrue false stale ticks. Re-seed the tracker at the current
            // gap so a genuine stall (motion stops, gap stops closing) still
            // escalates from a clean baseline.
            ai.reset_follow_progress();
            stale = ai.note_follow_dist(anchor->guid.GetCounter(), dist);
        }
        else
        {
            stale = ai.note_follow_dist(anchor->guid.GetCounter(), dist);
        }
        // Throttle the validated repath to ~every 2.5s; between attempts use the
        // cheap MotionMaster MoveFollow so we don't pathfind + log every tick.
        if (!bot_in_motion && stale >= kFollowStaleTicks &&
            ai.follow_repath_due(snapshot.published_at_ms(), 2500))
        {
            float tx = anchor->x, ty = anchor->y, tz = anchor->z;
            if (stale >= kFollowEscapeTicks)
            {
                // Stalled for ~20s with the gap not closing → the leader is
                // genuinely unreachable (across a gap / closed door). Aim the
                // move_to at the nearest known-good navmesh node instead of
                // hammering the unreachable leader, giving the bot a chance to
                // route onto valid mesh. No teleport. Falls back to the leader
                // position when no node is near.
                if (auto const* node = Services::TravelGraph().FindNearestNodeOnMap(
                        snapshot.map_id(), bx, by, 800.f))
                {
                    const float ndx = node->x - bx, ndy = node->y - by;
                    // M-P2a: the nearest node to the BOT can sit on the far
                    // side of the bot from the leader — routing onto it would
                    // walk the follower AWAY from the group (observed: a dock
                    // bot recalled to a pier node behind it, increasing the
                    // gap every escape cycle). Only accept the node when it
                    // makes positive progress toward the anchor: its XY
                    // distance to the anchor must be strictly less than the
                    // bot's current gap. Otherwise keep aiming the move_to at
                    // the anchor itself (the navmesh-validated move_to + the
                    // walk-first unstick ladder still handle a truly blocked
                    // leader without sending the bot backward).
                    const float node_to_anchor =
                        DistanceXY(node->x, node->y, anchor->x, anchor->y);
                    if (ndx * ndx + ndy * ndy > 25.f && node_to_anchor < dist)
                    {
                        tx = node->x; ty = node->y; tz = node->z;
                    }
                }
            }
            emit.move_to(tx, ty, tz, /*run*/ true);
            ai.set_last_rule_fired("ingroup:follow_recall_repath");
        }
        else
        {
            // M-P1a: follow into the bot's FORMATION SLOT, not generically
            // behind the anchor. Without the offset the whole squad piles onto
            // a single point behind the leader (clumping = AoE bait + obvious
            // bot tell). ComputeFormationOffset yields the per-slot
            // distance+angle for the owner-configured formation; Free/slot-0
            // collapses to a plain follow so unconfigured squads are unchanged.
            FormationOffset const off = ComputeFormationOffset(
                ai.effective_formation_type(snapshot.in_combat()),
                ai.formation_slot(),
                kFollowDistance);
            emit.follow(anchor->guid, off.distance, off.angle_radians);
            ai.set_last_rule_fired("ingroup:follow_recall");
        }
    }
    else
    {
        // Within slack (or following suppressed) — not separated, clear the
        // stale-progress counter so a future real separation starts fresh.
        ai.reset_follow_progress();
    }

    // Auto-mount when the leader is far ahead and we're outdoors — closes
    // the gap quickly. We only mount if the bot isn't already mounted, isn't
    // in combat (handled by InCombat short-circuit above), isn't swimming /
    // flying, and the leader is mounted (a strong signal we should be too).
    constexpr float kMountThreshold = 30.0f;
    // Same in_active_bg gate applies to leader-driven mount/dismount —
    // BG maps generally disallow mounts and the InCombat dispatch already
    // dismounts on attack. Letting these fire would spam intents the
    // spell engine rejects.
    const bool want_mount = !in_active_bg &&
                            dist > kMountThreshold &&
                            leader_member->is_mounted &&
                            !snapshot.is_mounted() &&
                            !snapshot.is_indoors() &&
                            !snapshot.raw().movement.is_swimming &&
                            !snapshot.raw().movement.is_flying;
    // M-P2e: dismount jitter shares the per-bot mount-timer field. Mount and
    // dismount are mutually exclusive in a single tick (want_mount requires
    // !is_mounted(); the dismount block below requires is_mounted()), so the
    // field is never contended. Computed here so the mount-else clear below
    // knows not to wipe a pending dismount deadline.
    const bool want_dismount = !in_active_bg && snapshot.is_mounted() &&
                               !leader_member->is_mounted &&
                               dist <= kMountThreshold;
    if (want_mount)
    {
        // Edge-triggered: a mount cast takes ~1.5s; without dedup we'd spam
        // MountIntent every tick during the cast and the API would just
        // reject all but the first. Cleared once the bot is mounted or the
        // condition lapses (e.g. leader dismounted, gap closed).
        if (!ai.mount_pending())
        {
            // Reaction-time hesitation: 300–1200ms per bot. Without this
            // a 5-bot raid all mounts on the same frame as the leader,
            // which is impossible for human-played followers.
            const uint32 now_ms = snapshot.published_at_ms();
            uint32 ready_at = ai.pending_follow_mount_at_ms();
            if (ready_at == 0)
            {
                const uint32 jitter =
                    300u + (uint32(snapshot.bot_id()) * 2654435761u) % 900u;
                ai.set_pending_follow_mount_at_ms(now_ms + jitter);
            }
            else if (now_ms >= ready_at)
            {
                emit.mount_appropriate();
                ai.set_mount_pending(true);
                ai.set_last_rule_fired("ingroup:mount");
            }
        }
    }
    else
    {
        if (ai.mount_pending()) ai.set_mount_pending(false);
        // Don't wipe the shared timer when a dismount is pending — the
        // dismount block (M-P2e) is using it for its own stagger this tick.
        if (!want_dismount && ai.pending_follow_mount_at_ms() != 0)
            ai.set_pending_follow_mount_at_ms(0);
    }

    // Auto-dismount when the leader dismounts and we're close enough that
    // running speed catches up. Without this the bot stays mounted
    // indefinitely after the auto-mount fired earlier — mounted bots can't
    // cast or interact, so leader's "we're at the quest hub" signal needs
    // the bots to be ready too. dist gate avoids dismounting mid-travel
    // when the leader briefly dismounts to interact with something while
    // we're still 50yd back.
    if (want_dismount)
    {
        // M-P2e: reaction-time stagger, mirroring the mount path. Without it
        // the whole squad dismounts on the exact frame the leader does —
        // impossible for human-played followers and an obvious bot tell.
        // Reuse the per-bot mount-timer field (mount/dismount never overlap
        // in a tick; the mount-else above is taught not to wipe it while a
        // dismount is pending). Deterministic 300–1200ms per-bot offset keyed
        // on bot_id so the same bot is consistent run-to-run but the squad
        // spreads out. The action-retry dedup below still suppresses
        // in-flight duplicate packets once the stagger elapses.
        const uint32 dm_now_ms = GameTime::GetGameTimeMS();
        const uint32 sched_now_ms = snapshot.published_at_ms();
        uint32 dm_ready_at = ai.pending_follow_mount_at_ms();
        if (dm_ready_at == 0)
        {
            const uint32 jitter =
                300u + (uint32(snapshot.bot_id()) * 2654435761u) % 900u;
            ai.set_pending_follow_mount_at_ms(sched_now_ms + jitter);
        }
        else if (sched_now_ms >= dm_ready_at)
        {
            // Edge-trigger: dismount is a server-validated packet; the
            // `is_mounted()` snapshot field flips false the same tick the
            // SPELL_AURA_MOUNTED aura is removed but BEFORE the dismount
            // round-trip completes if invoked twice. Without dedup, a fresh
            // dismount intent in flight gets duplicated. Same pattern as
            // mount above.
            constexpr uint64 dm_key = uint64(0xD150FF1'1u);
            if (!ai.action_recently_tried(BotAI::ActionKind::BgPort, dm_key, dm_now_ms))
            {
                emit.dismount();
                ai.note_action_retry(BotAI::ActionKind::BgPort, dm_key, dm_now_ms);
                ai.set_last_rule_fired("ingroup:dismount");
            }
            // Stagger satisfied — release the shared timer so the next
            // mount/dismount cycle re-rolls a fresh offset.
            ai.set_pending_follow_mount_at_ms(0);
        }
    }
}

} // namespace Playerbot::States

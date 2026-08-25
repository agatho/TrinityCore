// AmbientRules - Refactor #3 pass 11/12. Migrates top-level ambient/PvP rules
// out of the State_Idle tail:
//   - idle:assist_friend_pvp  : grouped bot's friend is being attacked by a
//                                hostile player -> start_attack on the attacker.
//   - idle:pvp_help_callout   : bot in low-HP open-world PvP attack -> /say
//                                "help! pvp <zone>" with a 5-min lockout.
//   - idle:yell_lfg           : solo level-60+ bot in a rested area (city/inn)
//                                /yells a role-flavored LFG line every ~45 min.
//   - idle:ambient_emote      : Roleplay-personality bot in a rested area emits
//                                a flavor /wave / /cheer / /salute targeted at
//                                a random groupmate (or untargeted solo).
//                                Added in pass 12 after IdleRule signature was
//                                extended to thread GroupSnapshotView.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "DB2Stores.h"
#include "World.h"
#include "UnitDefines.h"   // UNIT_NPC_FLAG_FLIGHTMASTER / _QUESTGIVER

#include <string>

namespace Playerbot {

namespace {

// ---------- idle:assist_friend_pvp ----------
bool AssistFriendPvpGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (!s.is_alive()) return false;
    if (!s.in_group()) return false;
    if (s.in_combat()) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    if (s.is_casting()) return false;
    return true;
}

bool AssistFriendPvpFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float self_xa = 0.f, self_ya = 0.f, self_za = 0.f;
    s.position(self_xa, self_ya, self_za);
    ObjectGuid best_attacker;
    float      best_d2 = 1e9f;
    for (auto const& e : s.nearby_enemies())
    {
        if (!e.is_player) continue;
        if (e.hp <= 0) continue;
        if (e.victim.IsEmpty()) continue;
        if (e.victim == s.raw().guid) continue;
        bool victim_is_friend = false;
        for (auto const& f : s.nearby_friends())
        {
            if (!f.is_player) continue;
            if (f.guid != e.victim) continue;
            if (f.hp <= 0) continue;
            victim_is_friend = true;
            break;
        }
        if (!victim_is_friend) continue;
        const float dx = e.x - self_xa;
        const float dy = e.y - self_ya;
        const float dz = e.z - self_za;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2)
        {
            best_d2 = d2;
            best_attacker = e.guid;
        }
    }
    if (best_attacker.IsEmpty() || best_d2 >= 40.0f * 40.0f) return false;
    if (!emit.start_attack(best_attacker)) return false;
    ai.set_last_rule_fired("idle:assist_friend_pvp");
    return true;
}

// ---------- idle:assist_group_pve ----------
// PvE mirror of assist_friend_pvp: a group member is in combat with
// a creature but the bot is OOC. Real players see "my groupmate is
// being attacked" and open up. Without this, a hunter who pulls a
// mob solo while two other hunters stand 20y away gets no support.
//
// Open-world only for v1. BG scripts and dungeon scripts own their
// own engagement choreography (skull mark, tank pull, etc.) — a
// generic assist would risk pulling extra mobs the script doesn't
// want pulled. Re-evaluate dungeon scope only with evidence that
// the existing tank/heal/engage chain misses cases.
bool AssistGroupPveGate(BotSnapshotView const& s, BotAI&,
                        GroupSnapshotView const& g, uint32)
{
    if (!s.is_alive()) return false;
    if (!g.exists()) return false;
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    return true;
}

bool AssistGroupPveFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const& g,
                        BotIntentEmitter& emit, uint32)
{
    auto const* members = g.members();
    if (!members || members->empty()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    ObjectGuid best_attacker;
    float      best_d2 = 40.0f * 40.0f;     // engage range cap
    for (auto const& e : s.nearby_enemies())
    {
        if (e.is_player)        continue;   // PvE flavour only
        if (e.hp <= 0)          continue;
        if (e.victim.IsEmpty()) continue;
        if (e.victim == s.raw().guid) continue;  // already on us → in_combat
        // e.victim must be a live group member on our map (excluding self).
        bool victim_is_groupmate = false;
        for (auto const& m : *members)
        {
            if (m.guid == s.raw().guid) continue;
            if (m.guid != e.victim)     continue;
            if (m.hp <= 0)              continue;
            if (m.map_id != s.map_id()) continue;
            victim_is_groupmate = true;
            break;
        }
        if (!victim_is_groupmate) continue;
        const float dx = e.x - bx, dy = e.y - by, dz = e.z - bz;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2) { best_d2 = d2; best_attacker = e.guid; }
    }
    if (best_attacker.IsEmpty()) return false;
    if (!emit.start_attack(best_attacker)) return false;
    ai.set_last_rule_fired("idle:assist_group_pve");
    return true;
}

// ---------- idle:pvp_help_callout ----------
bool PvpHelpCalloutGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (!s.is_alive()) return false;
    if (!s.in_group()) return false;
    if (s.in_battleground()) return false;
    if (!s.under_player_attack()) return false;
    if (s.hp_pct() >= 40) return false;
    const uint32 pw_now_ms = s.published_at_ms();
    const uint64 pw_key = (uint64(7) << 32) | uint64(0xDEADu);
    return !ai.action_recently_tried(BotAI::ActionKind::BgCallout, pw_key, pw_now_ms);
}

bool PvpHelpCalloutFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    uint32 const area_id = s.area_id();
    std::string area_name;
    if (auto const* ae = sAreaTableStore.LookupEntry(area_id))
        area_name = ae->AreaName[sWorld->GetDefaultDbcLocale()];
    if (area_name.empty())
        area_name = std::to_string(area_id);
    const std::string msg = "help! pvp " + area_name;
    if (!emit.say(msg)) return false;
    const uint32 pw_now_ms = s.published_at_ms();
    const uint64 pw_key = (uint64(7) << 32) | uint64(0xDEADu);
    ai.note_action_retry(BotAI::ActionKind::BgCallout, pw_key, pw_now_ms);
    ai.set_last_rule_fired("idle:pvp_help_callout");
    return true;
}

// ---------- idle:yell_lfg ----------
bool YellLfgGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (s.in_group()) return false;
    if (s.level() < 60) return false;
    if (s.raw().identity.rest_bonus_xp <= 0) return false;
    if (ai.personality().verbosity == Verbosity::Silent) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;

    constexpr uint32 kYellThrottleMs = 45u * 60u * 1000u;
    const uint32 yell_now_ms = s.published_at_ms();
    if (ai.last_yell_lfg_ms() == 0)
    {
        const uint32 stagger = (s.bot_id() * 9173u) % kYellThrottleMs;
        const uint32 base = yell_now_ms > kYellThrottleMs
                            ? yell_now_ms - kYellThrottleMs
                            : 0u;
        ai.set_last_yell_lfg_ms(base + stagger);
    }
    const uint32 last_yell = ai.last_yell_lfg_ms();
    return yell_now_ms != 0 && yell_now_ms >= last_yell &&
           (yell_now_ms - last_yell) >= kYellThrottleMs;
}

bool YellLfgFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    const uint32 yell_now_ms = s.published_at_ms();
    constexpr char const* kTankYells[] = {
        "Tank LFG, msg me!",
        "Looking for heroic group, can tank.",
        "Tank for hire, dungeons or M+.",
    };
    constexpr char const* kHealerYells[] = {
        "Healer LFG.",
        "Looking for group to heal, dungeon or raid.",
        "Healer available, msg me for runs.",
    };
    constexpr char const* kDpsYells[] = {
        "DPS LFG.",
        "Looking for any group.",
        "Hi! Anyone running content tonight?",
        "lfg, can do most stuff.",
    };
    constexpr uint32 kTankSize = sizeof(kTankYells) / sizeof(kTankYells[0]);
    constexpr uint32 kHealerSize = sizeof(kHealerYells) / sizeof(kHealerYells[0]);
    constexpr uint32 kDpsSize = sizeof(kDpsYells) / sizeof(kDpsYells[0]);
    const Role role = ai.effective_role(s);
    char const* phrase = nullptr;
    const uint32 sel_seed = (s.bot_id() ^ (yell_now_ms / (45u * 60u * 1000u)));
    if (role == Role::Tank)
        phrase = kTankYells[sel_seed % kTankSize];
    else if (role == Role::Healer)
        phrase = kHealerYells[sel_seed % kHealerSize];
    else
        phrase = kDpsYells[sel_seed % kDpsSize];
    emit.yell_world(phrase);
    ai.set_last_yell_lfg_ms(yell_now_ms);
    ai.set_last_rule_fired("idle:yell_lfg");
    return true;
}

// ---------- idle:ambient_sit ----------
// Side-effect "sit when resting" — SetStandStateIntent is idempotent
// server-side so re-emitting on an already-sitting bot is a no-op. Fire
// returns FALSE so subsequent registry rules still run (the legacy code
// did this unconditionally as a side effect before any return).
//
// Requires sustained idle: 10 seconds of standing still without combat
// or casting. Without this gate the bot sat on EVERY tiny pause during
// movement — between path segments, after each enemy kill before
// re-engaging, while a snapshot tick computed the next destination,
// etc. Looked frankly broken from the observer's perspective.
// Verified 2026-05-20: user reported "all bots sit on every tiny stop
// during movement". 10s threshold matches the standard MMO "out of
// combat → rest" feel without flooding sit animations.
bool AmbientSitGate(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&, uint32)
{
    // Common safety gates first — these hold regardless of WHERE the bot
    // is allowed to sit.
    if (s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    // Don't sit while anything hostile is in pull range. Even in a
    // hub guards / event mobs / world-PvP attackers can be 40y away.
    if (s.enemies_within(40.0f) > 0) return false;

    const uint32 now_ms = s.published_at_ms();
    if (s.is_moving())
    {
        ai.set_last_moving_ms(now_ms);
        return false;
    }
    // is_moving == false. last_moving_ms == 0 means "never moved since
    // boot" — those bots just spawned, give them a beat before sitting.
    const uint32 last_move = ai.last_moving_ms();
    if (last_move == 0)
    {
        ai.set_last_moving_ms(now_ms);
        return false;
    }
    const uint32 idle_ms = now_ms - last_move;

    // I-P3g: broadened sit locations. The original gate was hub-only
    // (operator-annotated city/village footprint, kind 3/4) — but real
    // players also rest while parked at a flight master, waiting on a
    // quest giver, or just standing still in a quiet safe spot after a
    // while. Three tiers, each with conservative thresholds:
    //
    //   1. City / village footprint  — 30s standstill (settled in a hub).
    //   2. Parked at a flight master or quest giver anywhere — 30s. These
    //      are natural "I'm waiting here" spots (queuing a taxi, reading
    //      quest text), so the same hub threshold reads naturally.
    //   3. Anywhere else that is safe — 90s standstill. Much longer so a
    //      bot pausing mid-route to recompute a path / scan objectives
    //      doesn't flop down by the roadside; only a genuinely settled
    //      bot reaches 90s of continuous standstill.
    constexpr uint32 kHubIdleMs    = 30u * 1000u;
    constexpr uint32 kAnywhereIdleMs = 90u * 1000u;

    if (s.in_city() || s.in_village())
        return idle_ms >= kHubIdleMs;

    if (s.nearest_npc_with_flag(uint32(UNIT_NPC_FLAG_FLIGHTMASTER) |
                                uint32(UNIT_NPC_FLAG_QUESTGIVER)) != nullptr)
        return idle_ms >= kHubIdleMs;

    return idle_ms >= kAnywhereIdleMs;
}

bool AmbientSitFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    // Edge-paced: one SIT per 60s settled window. The original per-tick
    // re-emit ("idempotent server-side") still executed ~6 intents/sec per
    // settled bot — the TOP intent in /diag fleet-wide (Uraimus, Dolanaar
    // inn, 2026-06-11) — and Unit::SetStandState re-sends the stand-state
    // self-packet on every call.
    const uint32 sit_now_ms = s.published_at_ms();
    if (!ai.action_recently_tried(BotAI::ActionKind::AmbientSit, 0, sit_now_ms))
    {
        emit.emit(SetStandStateIntent{1});
        ai.note_action_retry(BotAI::ActionKind::AmbientSit, 0, sit_now_ms);
    }
    return false;   // intentional: side effect only, don't claim the tick
}

// ---------- idle:look_at_npc ----------
// Real players don't stand staring at their pet's tail — they
// idle-rotate their camera and "look at" nearby quest-givers /
// vendors / mailboxes. Without this, bots are eerily still in a hub,
// always facing whatever they last targeted. The rule picks a
// random nearby_friend with NPC flags (vendor/quest/banker/etc.)
// every ~30–60s (jittered per-bot) and emits face_target — pure
// cosmetic, no behaviour impact. Skipped while moving/in combat/
// casting/mounted.
//
// I-P3d: cadence is tracked via ai.last_look_around_ms() (added to
// BotAI mirroring last_ambient_emote_ms_). A per-bot jitter spreads
// the fire window across 30–60s so a hub full of bots doesn't all
// snap their heads at the same NPC on the same tick. Personality
// verbosity gate same as emote.
bool LookAroundGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (!s.is_alive()) return false;
    // Need at least one NPC-flagged friendly in range.
    bool any = false;
    for (auto const& f : s.nearby_friends())
        if (f.npc_flags != 0) { any = true; break; }
    if (!any) return false;

    // I-P3d: jittered 30–60s cadence. Base 30s + a stable per-bot 0–30s
    // offset (deterministic from bot_id so the same bot keeps a steady
    // personal rhythm rather than re-rolling every tick).
    const uint32 now_ms = s.published_at_ms();
    const uint32 jitter = (s.bot_id() * 2654435761u) % 30000u;     // 0–30s
    const uint32 interval_ms = 30000u + jitter;                    // 30–60s
    const uint32 last_ms = ai.last_look_around_ms();
    return now_ms == 0 || last_ms == 0 || (now_ms - last_ms) >= interval_ms;
}

bool LookAroundFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // I-P3c: pick ONE candidate via an up-front index, then select it
    // in a second pass. The old `% candidate_count` reservoir was buggy:
    // it picks index i only when (seed % (i+1)) == 0, which for any seed
    // is overwhelmingly biased toward the first candidate (i=0 always
    // matches since seed % 1 == 0) — so the bot almost always faced the
    // nearest NPC. Count first, derive a single random index, select it.
    uint32 candidate_count = 0;
    for (auto const& f : s.nearby_friends())
        if (f.npc_flags != 0) ++candidate_count;
    if (candidate_count == 0) return false;
    const uint32 idx = (now_ms ^ (s.bot_id() * 2654435761u)) % candidate_count;
    uint32 seen = 0;
    ObjectGuid pick;
    for (auto const& f : s.nearby_friends())
    {
        if (f.npc_flags == 0) continue;
        if (seen == idx) { pick = f.guid; break; }
        ++seen;
    }
    if (pick.IsEmpty()) return false;
    if (!emit.face_target(pick)) return false;
    ai.set_last_look_around_ms(now_ms);
    ai.set_last_rule_fired("idle:look_at_npc");
    return true;
}

// ---------- idle:ambient_emote ----------
bool AmbientEmoteGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    // Pre-fix: Roleplay-only. Result: Normal/Chatty bots in capital
    // cities sat in dead silence indefinitely, looking like a mannequin
    // display rather than a living city. Now: Normal+ (so Silent/Terse
    // still stay quiet). Cadence varies by personality so Chatty bots
    // emote more often than Normal.
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (s.raw().identity.rest_bonus_xp <= 0) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request) return false;
    const uint32 kThrottleMs =
        ai.personality().verbosity == Verbosity::Roleplay ? 2u * 60u * 1000u :
        ai.personality().verbosity == Verbosity::Chatty   ? 3u * 60u * 1000u :
                                                            5u * 60u * 1000u;
    const uint32 now_ms = s.published_at_ms();
    const uint32 last_ms = ai.last_ambient_emote_ms();
    return now_ms == 0 || (now_ms - last_ms) >= kThrottleMs;
}

bool AmbientEmoteFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const& g,
                      BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // Pool expanded 2026-05-21: 3=wave, 4=bow, 34=cheer, 39=flex, 66=hug,
    // 113=love, 25=point ("window shopping"), 16=talk (chat to NPCs/
    // bystanders), 21=shy. Mixed-personality pool — humans browse
    // vendors, point at things, chat at NPCs, not just wave/hug.
    constexpr uint32 kEmotePool[] = {
        3, 4, 66, 34, 39, 113, 25, 16, 21,
    };
    constexpr uint32 kPoolSize = sizeof(kEmotePool) / sizeof(uint32);
    const uint32 sel = ((now_ms / 1000u) + s.bot_id()) % kPoolSize;
    ObjectGuid target;
    if (g.exists())
    {
        if (auto const* members = g.members())
        {
            // I-P3c: same reservoir bug as look_at_npc — `% (seen+1)`
            // biases hard toward the first eligible member. Count the
            // eligible (online, same-map, non-self) members, derive one
            // random index up-front, then select that index in a second
            // pass so the emote target is uniformly random.
            uint32 eligible = 0;
            for (auto const& m : *members)
            {
                if (m.guid == s.raw().guid) continue;
                if (!m.online || m.map_id != s.map_id()) continue;
                ++eligible;
            }
            if (eligible > 0)
            {
                const uint32 idx =
                    (now_ms ^ (s.bot_id() * 2654435761u)) % eligible;
                uint32 seen = 0;
                for (auto const& m : *members)
                {
                    if (m.guid == s.raw().guid) continue;
                    if (!m.online || m.map_id != s.map_id()) continue;
                    if (seen == idx) { target = m.guid; break; }
                    ++seen;
                }
            }
        }
    }
    emit.emit(PerformEmoteIntent{kEmotePool[sel], target});
    ai.set_last_ambient_emote_ms(now_ms);
    ai.set_last_rule_fired("idle:ambient_emote");
    return true;
}

// ---------- idle:ambient_state_emote ----------
// I-P3e: the existing ambient_emote pool is all ONESHOT emotes (wave,
// bow, cheer...) — a quick animation that immediately resets. Real
// players who park in a hub for a while strike SUSTAINED state emotes:
// they dance on the mailbox, sit at the inn, lie down ("sleep") in a
// quiet corner. Without a sustained emote the city looks like everyone
// is perpetually fidgeting but never settling.
//
// This rule emits a single low-frequency state emote when the bot has
// been settled in a hub for a good while. State emotes persist
// server-side until the unit moves (WoW clears EMOTE_STATE on any
// movement), and the gate already bails on movement/combat, so we never
// re-emit while moving — the animation clears itself the instant the
// bot walks. Conservative cadence (~8 min) so a tavern isn't a rave.
bool AmbientStateEmoteGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (!s.is_alive()) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request) return false;
    // Hub-only — sustained emotes look out of place mid-world.
    if (!s.in_city() && !s.in_village()) return false;
    // Don't strike a pose with anything hostile around.
    if (s.enemies_within(40.0f) > 0) return false;
    // Require a sustained standstill (reuses the ambient_sit move tracker).
    const uint32 now_ms = s.published_at_ms();
    const uint32 last_move = ai.last_moving_ms();
    if (last_move == 0 || (now_ms - last_move) < 30u * 1000u) return false;

    constexpr uint32 kThrottleMs = 8u * 60u * 1000u;   // ~8 min
    const uint32 last_ms = ai.last_ambient_state_emote_ms();
    return now_ms == 0 || last_ms == 0 || (now_ms - last_ms) >= kThrottleMs;
}

bool AmbientStateEmoteFire(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // Sustained "state" emote ids (persist until movement), verified
    // against SharedDefines.h Emote enum:
    //   10 = EMOTE_STATE_DANCE, 12 = EMOTE_STATE_SLEEP, 13 = EMOTE_STATE_SIT.
    // DANCE reads most "alive"; weight the pool toward it.
    constexpr uint32 kStatePool[] = { 10 /*dance*/, 10 /*dance*/, 12 /*sleep*/ };
    constexpr uint32 kPoolSize = sizeof(kStatePool) / sizeof(uint32);
    const uint32 sel = (now_ms ^ (s.bot_id() * 2654435761u)) % kPoolSize;
    emit.emit(PerformEmoteIntent{kStatePool[sel], ObjectGuid::Empty});
    ai.set_last_ambient_state_emote_ms(now_ms);
    ai.set_last_rule_fired("idle:ambient_state_emote");
    return true;
}

// ---------- idle:react_to_passerby ----------
// I-P3h: bots never acknowledged real players walking past — they'd
// stand frozen next to a human who waved at them, which immediately
// breaks the illusion. This rule occasionally faces and /waves at a
// nearby NON-grouped player.
//
// Bot-on-bot spam guard: there is currently no is_bot flag on
// NearbyUnit, so we cannot positively exclude other bots. We therefore
// (a) only react to is_player units that are NOT in our group, and
// (b) rate-limit HARD — the rule's coarse 30s min_interval_ms (one
// greet attempt per bot per 30s at most) plus a per-target 10-min
// lockout (ActionKind::GreetPlayer). Even if two bots greet each other,
// the per-target lockout caps it to one wave per pair per 10 min, which
// reads as natural rather than spammy.
// I-P3h: needs a NearbyUnit.is_bot flag (owned by BotSnapshotBuilder)
// to fully suppress bot-on-bot greets — until then the hard rate-limit
// keeps it from looking robotic.
bool ReactToPasserbyGate(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (!s.is_alive()) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request) return false;
    // Cheap pre-check: at least one eligible (real, non-grouped, alive)
    // player nearby. The actual rate-limiting is the rule's coarse 30s
    // min_interval_ms plus the per-target 10-min GreetPlayer lockout in
    // the fire — so we don't need a separate per-bot timestamp here.
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player) continue;
        if (u.is_in_group) continue;     // skip grouped players (likely in a party)
        if (u.hp <= 0) continue;
        return true;
    }
    return false;
}

bool ReactToPasserbyFire(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const& g,
                         BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // Build a quick set of our own group member guids so we never greet
    // someone actually in our party (is_in_group is a coarse "in ANY
    // group" flag; this is the precise "in OUR group" exclusion).
    auto in_our_group = [&](ObjectGuid gid) -> bool
    {
        if (!g.exists()) return false;
        if (auto const* members = g.members())
            for (auto const& m : *members)
                if (m.guid == gid) return true;
        return false;
    };

    float bx, by, bz; s.position(bx, by, bz);
    ObjectGuid best;
    float best_d2 = 30.0f * 30.0f;       // only greet players within 30y
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player) continue;
        if (u.is_in_group) continue;
        if (u.hp <= 0) continue;
        if (u.guid == s.raw().guid) continue;
        if (in_our_group(u.guid)) continue;
        // Per-target hard lockout (10 min, ActionKind::GreetPlayer).
        if (ai.action_recently_tried(BotAI::ActionKind::GreetPlayer,
                                     u.guid.GetCounter(), now_ms))
            continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2) { best_d2 = d2; best = u.guid; }
    }
    if (best.IsEmpty()) return false;
    // Face them, then wave (3 = EMOTE_ONESHOT_WAVE aimed at the target).
    emit.face_target(best);
    if (!emit.perform_emote(3, best)) return false;
    ai.note_action_retry(BotAI::ActionKind::GreetPlayer,
                         best.GetCounter(), now_ms);
    ai.set_last_rule_fired("idle:react_to_passerby");
    return true;
}

} // anonymous namespace

void RegisterAmbientRules(IdleRuleRegistry& r)
{
    {
        // 706: just above assist_friend_pvp (705). PvE assist isn't
        // a higher emergency than PvP defense, but the cheaper gate
        // (no per-enemy player check) firing first avoids redundant
        // work when both want to run. Both still in survival band.
        IdleRule rule;
        rule.name     = "idle:assist_group_pve";
        rule.priority = 706;
        rule.gate     = &AssistGroupPveGate;
        rule.fire     = &AssistGroupPveFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:assist_friend_pvp";
        // Survival-tier priority (705). Was 215 — below mail/vendor/
        // gossip — which meant a bot whose groupmate was being killed
        // by an enemy player would run to the vendor / type "ding!" /
        // mail-thank first. World-PvP defense is an emergency, not
        // flavor. Sits just below the 880-900 survival hazards and
        // BG/dungeon dispatch (720/718) but above all utility/social.
        rule.priority = 705;
        rule.gate     = &AssistFriendPvpGate;
        rule.fire     = &AssistFriendPvpFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:pvp_help_callout";
        rule.priority = 703;        // paired emergency with assist_friend
        rule.gate     = &PvpHelpCalloutGate;
        rule.fire     = &PvpHelpCalloutFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:yell_lfg";
        rule.priority = 100;
        rule.gate     = &YellLfgGate;
        rule.fire     = &YellLfgFire;
        // Yell-LFG has an internal 45min throttle; coarse 30s skip
        // saves the per-tick gate eval (rest_bonus_xp + in_group +
        // verbosity reads) for 149/150 ticks.
        rule.min_interval_ms = 30000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ambient_emote";
        rule.priority = 80;   // ambient flavor, lowest priority
        rule.gate     = &AmbientEmoteGate;
        rule.fire     = &AmbientEmoteFire;
        // Internal 5min throttle on emote cadence; 10s coarse skip.
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
    {
        // Look-around — face a random nearby NPC every 30s. Pure
        // cosmetic; pure side-effect (returns true but no movement).
        // Priority 70 (below ambient_emote 80) so emote wins when
        // both are ready. Throttle 30s.
        IdleRule rule;
        rule.name     = "idle:look_at_npc";
        rule.priority = 70;
        rule.gate     = &LookAroundGate;
        rule.fire     = &LookAroundFire;
        rule.min_interval_ms = 30000;
        r.register_rule(std::move(rule));
    }
    {
        // Priority 90 — above ambient_emote (80) so the sit emit fires
        // BEFORE the emote (matches legacy inline order). Fire returns
        // false so subsequent rules still run.
        IdleRule rule;
        rule.name     = "idle:ambient_sit";
        rule.priority = 90;
        rule.gate     = &AmbientSitGate;
        rule.fire     = &AmbientSitFire;
        r.register_rule(std::move(rule));
    }
    {
        // I-P3h: greet a nearby non-grouped real player. Priority 75 sits
        // between look_at_npc (70) and ambient_emote (80): reacting to a
        // passer-by matters more than idly facing an NPC but less than the
        // self-directed emote cadence. Coarse 30s skip; the heavy lifting
        // is the per-target 10-min GreetPlayer lockout inside the fire.
        IdleRule rule;
        rule.name     = "idle:react_to_passerby";
        rule.priority = 75;
        rule.gate     = &ReactToPasserbyGate;
        rule.fire     = &ReactToPasserbyFire;
        rule.min_interval_ms = 30000;
        r.register_rule(std::move(rule));
    }
    {
        // I-P3e: sustained state emote (dance/sleep) when settled in a hub.
        // Lowest priority (65) — pure ambient flavor, below look_at_npc.
        // Internal ~8min throttle; coarse 60s skip.
        IdleRule rule;
        rule.name     = "idle:ambient_state_emote";
        rule.priority = 65;
        rule.gate     = &AmbientStateEmoteGate;
        rule.fire     = &AmbientStateEmoteFire;
        rule.min_interval_ms = 60000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

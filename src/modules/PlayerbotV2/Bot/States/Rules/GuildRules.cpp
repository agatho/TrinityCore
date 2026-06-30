// GuildRules - Refactor #3 pass 9. Migrates the GUILD_PLAN.md A-E rule
// family out of the State_Idle linear cascade. Covers:
//   - idle:guild_charter_drive (founder FSM, phases 0..5/0xFF)
//   - idle:guild_charter_sign  (signer branch — folded under one outer
//                                gate alongside founder, registered as
//                                a separate rule for /whyidle clarity).
//   - idle:guild_recruit_nearby
//   - idle:guild_event:announce
//   - idle:guild_event:tavern_*    (walk / dance / chat / idle)
//   - idle:guild_event:staging_*   (walk / chat / idle)
//   - idle:guild_chat:ding / login_greet / smalltalk / tavern_hangout
//                            / rival_banter / quest_brag / lfm_pulse
//   - idle:guild_recruit_channel
//   - idle:guild_chat_babble
//
// All rules preserve the inline behavior verbatim (cooldowns, RNG gates,
// faction lookups). The set_last_rule_fired tag is the legacy one for
// /diag and /history compatibility.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Services.h"
#include "Fleet/BotGuildMgr.h"
#include "Fleet/BotGuildEvent.h"
#include "Bot/BotArchetype.h"
#include "Config.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include "UnitDefines.h"

#include <fmt/format.h>

namespace Playerbot {

namespace {

// ---------- Faction helper ----------
inline ::Playerbot::V2::BotGuildMgr::Faction FactionOf(BotSnapshotView const& s)
{
    return (Player::TeamForRace(s.race()) == ALLIANCE)
        ? ::Playerbot::V2::BotGuildMgr::FACTION_ALLIANCE
        : ::Playerbot::V2::BotGuildMgr::FACTION_HORDE;
}

// ---------- Shared "guildless + quiet" outer gate ----------
bool GuildlessQuietGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const& g, uint32)
{
    if (s.raw().guild.id != 0) return false;
    if (s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    return s.raw().guid.GetCounter() != 0;
}

// ---------- idle:guild_charter_drive (founder FSM) ----------
bool CharterDriveGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildlessQuietGate(s, ai, g, now_ms)) return false;
    const uint64 me_low = s.raw().guid.GetCounter();
    const auto kFaction = FactionOf(s);
    return Services::Guilds().ActiveFounderLow(kFaction) == me_low;
}

bool CharterDriveFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const auto kFaction = FactionOf(s);
    const uint32 now_ms = GameTime::GetGameTimeMS();
    uint8 phase = ai.guild_charter_phase();

    // Abort FSM if we exceeded the 30-min budget.
    if (phase != 0 && phase != 0xFF && phase != 0xFE)
    {
        if (now_ms - ai.guild_charter_started_ms()
            > ::Playerbot::BotAI::kGuildCharterBudgetMs)
        {
            TC_LOG_WARN("playerbot.v2",
                "[CharterDrive] abort reason=budget_30min faction={} phase={} "
                "bot_id={} elapsed_ms={}",
                static_cast<uint32>(kFaction), uint32(phase), s.bot_id(),
                now_ms - ai.guild_charter_started_ms());
            Services::Guilds().OnCharterAborted(kFaction);
            ai.clear_guild_charter();
            phase = 0;
        }
    }

    // Phase 0 -> 1: stamp start + find petitioner.
    if (phase == 0)
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_PETITIONER))
        {
            TC_LOG_WARN("playerbot.v2",
                "[CharterDrive] phase 0->1 faction={} bot_id={} "
                "petitioner_low={} entry={} npc_pos=({:.1f},{:.1f},{:.1f})",
                static_cast<uint32>(kFaction), s.bot_id(),
                npc->guid.GetCounter(), npc->entry, npc->x, npc->y, npc->z);
            ai.set_guild_charter(/*phase*/ 1,
                Services::Guilds().ActiveFounderName(kFaction),
                npc->guid.GetCounter(),
                now_ms);
            phase = 1;
        }
        else
        {
            float bxd, byd, bzd; s.position(bxd, byd, bzd);
            // Throttle to once / 5s per bot — without throttle this fires
            // every tick while phase 0 is stuck and floods the log.
            static thread_local uint32 s_last_log = 0;
            if (now_ms - s_last_log > 5000)
            {
                s_last_log = now_ms;
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] phase 0 STUCK faction={} bot_id={} "
                    "bot_pos=({:.1f},{:.1f},{:.1f}) nearby_friends.size={} "
                    "(no UNIT_NPC_FLAG_PETITIONER carrier in scan)",
                    static_cast<uint32>(kFaction), s.bot_id(),
                    bxd, byd, bzd,
                    uint32(s.raw().combat.nearby_friends.size()));
            }
        }
    }

    if (phase >= 1 && phase <= 5)
    {
        NearbyUnit const* pet_npc = nullptr;
        const uint64 pinned = ai.guild_charter_petitioner_low();
        for (auto const& u : s.raw().combat.nearby_friends)
        {
            if (u.guid.GetCounter() != pinned) continue;
            if ((u.npc_flags & UNIT_NPC_FLAG_PETITIONER) == 0) continue;
            pet_npc = &u;
            break;
        }
        if (!pet_npc)
            pet_npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_PETITIONER);

        if (!pet_npc)
        {
            // Phase 2/5 require the petitioner Creature* in scan to
            // interact with (buy / turn-in). Abort if we drifted
            // outside the 40y nearby_friends radius — catches genuine
            // petitioner-despawn vs. transient scan-window miss.
            if (phase == 2 || phase == 5)
            {
                float bxd, byd, bzd; s.position(bxd, byd, bzd);
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] abort reason=no_petitioner_in_scan "
                    "faction={} phase={} bot_id={} bot_pos=({:.1f},{:.1f},{:.1f}) "
                    "pinned_low={} nearby_friends.size={}",
                    static_cast<uint32>(kFaction), uint32(phase), s.bot_id(),
                    bxd, byd, bzd, pinned,
                    uint32(s.raw().combat.nearby_friends.size()));
                Services::Guilds().OnCharterAborted(kFaction);
                ai.clear_guild_charter();
                return false;
            }
            // Phase 1/4: walk-to-petitioner — wait silently until we
            // drift back into 40y scan (legacy behavior).
            // Phase 3: wait_signatures doesn't need the petitioner;
            // signers come to the founder, server-side accumulation.
            // Fall through with pet_npc null; phase 3 block below
            // handles it without dereferencing pet_npc.
            if (phase == 1 || phase == 4)
                return false;
            // phase 3 falls through. Bail on the dist_sq math below.
        }

        float dist_sq = 0.0f;
        if (pet_npc)
        {
            float bx2, by2, bz2; s.position(bx2, by2, bz2);
            const float ddx = pet_npc->x - bx2,
                        ddy = pet_npc->y - by2,
                        ddz = pet_npc->z - bz2;
            dist_sq = ddx*ddx + ddy*ddy + ddz*ddz;
        }
        constexpr float kInteract = 5.0f;

        if ((phase == 1 || phase == 4) && pet_npc)
        {
            if (dist_sq <= kInteract * kInteract)
            {
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] phase {}->{} (in-range, dist={:.1f}y) "
                    "faction={} bot_id={}",
                    uint32(phase), uint32(phase == 1 ? 2 : 5),
                    std::sqrt(dist_sq),
                    static_cast<uint32>(kFaction), s.bot_id());
                ai.advance_guild_charter_phase(phase == 1 ? 2 : 5);
            }
            else
            {
                emit.move_to(pet_npc->x, pet_npc->y, pet_npc->z, /*run*/ true);
                ai.set_last_rule_fired(phase == 1
                    ? "idle:guild_charter_drive:walk_petitioner_out"
                    : "idle:guild_charter_drive:walk_petitioner_back");
                return true;
            }
        }

        if (phase == 2)
        {
            const std::string name = ai.guild_charter_name();
            if (name.empty())
            {
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] abort reason=phase2_empty_name "
                    "faction={} bot_id={}",
                    static_cast<uint32>(kFaction), s.bot_id());
                Services::Guilds().OnCharterAborted(kFaction);
                ai.clear_guild_charter();
                return false;
            }
            // Pre-flight bag-space check. BotBuyGuildCharter creates a
            // new charter Item in the bot's inventory; if no slot is
            // free, the buy returns BagFull, the FSM advances to phase 3
            // anyway, and we wait 20 min for phase3 timeout before re-
            // electing — wasting the founder slot. Abort here so the
            // manager can elect a different founder this tick.
            if (s.bag_free_slots() == 0)
            {
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] abort reason=phase2_bag_full "
                    "faction={} bot_id={} (re-election needed)",
                    static_cast<uint32>(kFaction), s.bot_id());
                Services::Guilds().OnCharterAborted(kFaction);
                ai.clear_guild_charter();
                return false;
            }
            TC_LOG_WARN("playerbot.v2",
                "[CharterDrive] phase 2 EMIT buy_guild_charter "
                "faction={} bot_id={} pet_guid_low={} name='{}'",
                static_cast<uint32>(kFaction), s.bot_id(),
                pet_npc->guid.GetCounter(), name);
            emit.buy_guild_charter(pet_npc->guid, name);
            ai.advance_guild_charter_phase(3);
            ai.set_last_rule_fired("idle:guild_charter_drive:buy");
            return true;
        }

        if (phase == 3)
        {
            const uint32 sigs = Services::Guilds()
                .ActiveFounderSignatureCount(kFaction);
            const uint32 required = sConfigMgr->GetIntDefault(
                "MinPetitionSigns", 4);
            if (sigs >= required)
            {
                ai.advance_guild_charter_phase(4);
                ai.set_last_rule_fired("idle:guild_charter_drive:advance_to_submit");
                return true;
            }
            constexpr uint32 kPhase3MaxMs = 20u * 60u * 1000u;
            if (now_ms - ai.guild_charter_started_ms() > kPhase3MaxMs)
            {
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] abort reason=phase3_signature_timeout "
                    "faction={} bot_id={} sigs={}/{} elapsed_ms={}",
                    static_cast<uint32>(kFaction), s.bot_id(), sigs, required,
                    now_ms - ai.guild_charter_started_ms());
                Services::Guilds().OnCharterAborted(kFaction);
                ai.clear_guild_charter();
                ai.set_last_rule_fired("idle:guild_charter_drive:phase3_timeout");
                return true;
            }
            ai.set_last_rule_fired("idle:guild_charter_drive:wait_signatures");
            return true;
        }

        if (phase == 5)
        {
            const uint64 petition_low = ai.guild_charter_petition_low();
            if (petition_low == 0)
            {
                TC_LOG_WARN("playerbot.v2",
                    "[CharterDrive] abort reason=phase5_no_petition_id "
                    "faction={} bot_id={}",
                    static_cast<uint32>(kFaction), s.bot_id());
                Services::Guilds().OnCharterAborted(kFaction);
                ai.clear_guild_charter();
                return false;
            }
            emit.turnin_guild_charter(pet_npc->guid, petition_low);
            ai.set_last_rule_fired("idle:guild_charter_drive:turnin");
            return true;
        }
    }
    return false;
}

// ---------- idle:guild_charter_sign (signer branch) ----------
bool CharterSignGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildlessQuietGate(s, ai, g, now_ms)) return false;
    const uint64 me_low = s.raw().guid.GetCounter();
    const auto kFaction = FactionOf(s);
    const uint64 founder_low = Services::Guilds().ActiveFounderLow(kFaction);
    return founder_low != 0 && founder_low != me_low;
}

bool CharterSignFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const auto kFaction = FactionOf(s);
    const uint64 founder_low = Services::Guilds().ActiveFounderLow(kFaction);
    const uint64 petition_low =
        Services::Guilds().ActiveFounderPetitionLow(kFaction);
    if (petition_low == 0) return false;

    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (u.guid.GetCounter() != founder_low) continue;
        float bx3, by3, bz3; s.position(bx3, by3, bz3);
        const float fdx = u.x - bx3, fdy = u.y - by3, fdz = u.z - bz3;
        if (fdx*fdx + fdy*fdy + fdz*fdz > 30.0f * 30.0f) return false;

        const uint32 sign_now_ms = GameTime::GetGameTimeMS();
        if (ai.action_recently_tried(BotAI::ActionKind::InviteOther,
                                     petition_low, sign_now_ms))
            return false;
        emit.sign_guild_charter(petition_low);
        ai.note_action_retry(BotAI::ActionKind::InviteOther,
                             petition_low, sign_now_ms);
        ai.set_last_rule_fired("idle:guild_charter_sign");
        return true;
    }
    return false;
}

// ---------- idle:guild_recruit_nearby ----------
bool RecruitNearbyGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const& g, uint32)
{
    if (s.raw().guild.id == 0) return false;
    // #4C: snapshot-resolved flag (builder ran IsBotManaged world-thread)
    // replaces the prior per-tick BotGuildMgr lookup on the worker thread.
    if (!s.guild_is_bot_managed()) return false;
    if (s.guild_member_count() >= Services::Guilds().MaxMembersPerGuild()) return false;
    if (s.guild_rank_id() > 1) return false;
    if (s.in_combat() || s.is_casting() || s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    return true;
}

// Archetype Social-weight bias for the recruit-nearby cadence (#4C item 2).
// Social-dominant officers (SocialGuildie etc.) recruit eagerly on every
// eligible encounter; non-social archetypes still recruit but at a reduced
// frequency so the guild fills organically without every officer pestering
// every passer-by. Gated in the FIRE path so the bias only spends an RNG
// roll when an eligible guildless target is actually about to be invited.
inline bool RecruitArchetypeBiasPasses(BotSnapshotView const& s, BotAI& ai)
{
    if (s.archetype_dominant_activity() == ::Playerbot::ArchetypeActivity::Social)
        return true;
    // ~1-in-3 for non-social officers — keeps recruiting alive across the
    // fleet while concentrating the bulk of invites on social archetypes.
    return ai.rng().int_range(0, 3) == 0;
}

bool RecruitNearbyFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const uint32 rec_now_ms = GameTime::GetGameTimeMS();
    const uint64 rec_self_key = uint64(0xDEAD'BEEF'0000'0000ULL)
                              | s.raw().guid.GetCounter();
    if (ai.action_recently_tried(BotAI::ActionKind::InviteOther,
                                  rec_self_key, rec_now_ms))
        return false;

    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (!u.is_player) continue;
        if (u.guild_id != 0) continue;
        if (u.level < 10) continue;
        const uint64 tgt_low = u.guid.GetCounter();
        if (tgt_low == s.raw().guid.GetCounter()) continue;

        if (ai.action_recently_tried(BotAI::ActionKind::InviteOther,
                                      tgt_low, rec_now_ms))
            continue;

        float bx4, by4, bz4; s.position(bx4, by4, bz4);
        const float rdx = u.x - bx4, rdy = u.y - by4, rdz = u.z - bz4;
        if (rdx*rdx + rdy*rdy + rdz*rdz > 30.0f * 30.0f) continue;

        // Archetype Social-weight bias: a non-social officer skips this
        // encounter most of the time (still stamps the target cooldown so
        // we don't immediately re-roll the same bot next tick).
        if (!RecruitArchetypeBiasPasses(s, ai))
        {
            ai.note_action_retry(BotAI::ActionKind::InviteOther,
                                 tgt_low, rec_now_ms);
            continue;
        }

        emit.recruit_to_guild(tgt_low);
        ai.note_action_retry(BotAI::ActionKind::InviteOther,
                             tgt_low, rec_now_ms);
        ai.note_action_retry(BotAI::ActionKind::InviteOther,
                             rec_self_key, rec_now_ms);
        ai.set_last_rule_fired("idle:guild_recruit_nearby");
        return true;
    }
    return false;
}

// ---------- Shared guild-chat outer gate ----------
bool GuildChatQuietGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const& g, uint32)
{
    if (s.guild_id() == 0) return false;
    // #4C: snapshot-resolved flag replaces the per-tick worker-thread lookup.
    if (!s.guild_is_bot_managed()) return false;
    if (!s.is_alive()) return false;
    if (s.in_combat() || s.is_casting()) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    return true;
}

// ---------- idle:guild_chat:ding ----------
bool ChatDingGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    return s.level() > ai.guild_chat_last_ding_level();
}

bool ChatDingFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    if (ai.guild_chat_last_ding_level() == 0)
    {
        // First-time stamp; don't announce.
        ai.set_guild_chat_last_ding_level(s.level());
        return false;
    }
    static constexpr char const* kDingVariants[] = {
        "Ding {}!", "{} achieved", "hit {} :)", "Level {}!", "ding {}",
    };
    const int32_t n_dings = static_cast<int32_t>(sizeof(kDingVariants) / sizeof(*kDingVariants));
    const size_t idx = static_cast<size_t>(ai.rng().int_range(0, n_dings));
    emit.guild_chat(fmt::format(fmt::runtime(kDingVariants[idx]), s.level()));
    // Party-chat ding is handled separately in BotAI::tick (the
    // pending_ding_level_ stagger path) — don't double-emit here.
    ai.set_guild_chat_last_ding_level(s.level());
    ai.set_last_rule_fired("idle:guild_chat:ding");
    return true;
}

// ---------- idle:guild_chat:login_greet ----------
bool ChatLoginGreetGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    return ai.guild_chat_login_greeted_ms() == 0 &&
           s.guild_online_member_count() >= 2;
}

bool ChatLoginGreetFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    static constexpr char const* kGreetVariants[] = {
        "hi all", "hey :)", "o/", "hello!", "evening", "yo",
        "hi guild", "hey hey", "o/ everyone", "back online",
    };
    const int32_t n_greets = static_cast<int32_t>(sizeof(kGreetVariants) / sizeof(*kGreetVariants));
    const size_t idx = static_cast<size_t>(ai.rng().int_range(0, n_greets));
    emit.guild_chat(kGreetVariants[idx]);
    ai.note_guild_chat_login_greet(GameTime::GetGameTimeMS());
    (void)s;
    ai.set_last_rule_fired("idle:guild_chat:login_greet");
    return true;
}

// ---------- idle:guild_chat:smalltalk ----------
bool ChatSmalltalkGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    // SC-P3c: require at least one ONLINE human guildmate. Smalltalk is
    // self-initiated ambient chatter; with no human in the guild it's bots
    // babbling to each other, which is pointless traffic and reads as a tell.
    // Reactive announces (ding/login_greet/quest_brag) stay unconditional.
    if (s.guild_online_human_member_count() < 1) return false;
    if (s.guild_online_member_count() < 3) return false;
    const uint32 gc_now_ms = GameTime::GetGameTimeMS();
    if (ai.guild_chat_last_smalltalk_ms() != 0 &&
        gc_now_ms - ai.guild_chat_last_smalltalk_ms() <= 30u * 60u * 1000u)
        return false;
    return ai.rng().int_range(0, 100) == 0;
}

bool ChatSmalltalkFire(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    static constexpr char const* kSmalltalkVariants[] = {
        "anyone need help with anything?", "what's good tonight folks",
        "soloing this is rough", "anyone running dungeons?",
        "pulled too much again, classic", "where is everyone leveling?",
        "the new patch feels good", "anyone got mats to spare?",
        "drinking tea, brb", "good run earlier, ty",
        "checking AH brb", "this zone is gorgeous",
        "lf groupmates for tonight", "back from a break, hi all",
        "rolling around questing", "anyone seen [redacted-name] online?",
        "love this server", "phase is acting weird, anyone else?",
        "great patch notes today", "stretch break",
        "tea then back to grinding", "anyone need a port?",
        "this mount drop is brutal", "looking for a leveling buddy",
        "kicking back, easy night", "anyone want to chain quests?",
        "this rep grind...", "soloing dungeons for chests",
        "wow my repair bill", "good music tonight, share recs",
    };
    const int32_t n_st = static_cast<int32_t>(sizeof(kSmalltalkVariants) / sizeof(*kSmalltalkVariants));
    const size_t idx = static_cast<size_t>(ai.rng().int_range(0, n_st));
    emit.guild_chat(kSmalltalkVariants[idx]);
    ai.note_guild_chat_smalltalk(GameTime::GetGameTimeMS());
    ai.set_last_rule_fired("idle:guild_chat:smalltalk");
    return true;
}

// ---------- idle:guild_chat:tavern_hangout ----------
bool ChatTavernHangoutGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    // SC-P3c: the tavern-hangout pitch is broadcast to /g, so it needs a
    // human audience to be worth sending. (The cluster check below only
    // confirms 2+ guildmates are physically nearby — those can all be bots.)
    if (s.guild_online_human_member_count() < 1) return false;
    const uint32 gc_now_ms = GameTime::GetGameTimeMS();
    if (ai.guild_chat_last_hangout_ms() != 0 &&
        gc_now_ms - ai.guild_chat_last_hangout_ms() <= 60u * 60u * 1000u)
        return false;
    uint32 cluster = 0;
    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (!u.is_player) continue;
        if (u.guild_id != s.guild_id()) continue;
        if (u.guid == s.raw().guid) continue;
        ++cluster;
        if (cluster >= 2) break;
    }
    return cluster >= 2 && ai.rng().int_range(0, 50) == 0;
}

bool ChatTavernHangoutFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const auto kHFaction = FactionOf(s);
    char const* venue = (kHFaction == ::Playerbot::V2::BotGuildMgr::FACTION_ALLIANCE)
        ? "Pig & Whistle" : "Wyvern's Tail";
    emit.guild_chat(fmt::format("anyone want to hit the tavern? {}", venue));
    ai.note_guild_chat_hangout(GameTime::GetGameTimeMS());
    ai.set_last_rule_fired("idle:guild_chat:tavern_hangout");
    return true;
}

// ---------- idle:guild_chat:rival_banter ----------
bool ChatRivalBanterGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    if (s.guild_rival_id() == 0) return false;
    const uint32 gc_now_ms = GameTime::GetGameTimeMS();
    if (ai.guild_chat_last_smalltalk_ms() != 0 &&
        gc_now_ms - ai.guild_chat_last_smalltalk_ms() <= 30u * 60u * 1000u)
        return false;
    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (!u.is_player) continue;
        if (u.guild_id != s.guild_rival_id()) continue;
        return ai.rng().int_range(0, 30) == 0;
    }
    return false;
}

bool ChatRivalBanterFire(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    static constexpr char const* kRivalLines[] = {
        "rival guild spotted", "show them how it's done",
        "they think they're tough", "second place again",
        "watch the rivals here", "let's outperform them",
        "rival in sight, on guard", "amateurs",
    };
    const int32_t n_rv = sizeof(kRivalLines) / sizeof(*kRivalLines);
    const size_t ri = static_cast<size_t>(ai.rng().int_range(0, n_rv));
    emit.guild_chat(kRivalLines[ri]);
    ai.note_guild_chat_smalltalk(GameTime::GetGameTimeMS());
    ai.set_last_rule_fired("idle:guild_chat:rival_banter");
    return true;
}

// ---------- idle:guild_chat:quest_brag ----------
bool ChatQuestBragGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    const uint16 quest_count_now = s.completed_quest_count();
    if (ai.guild_chat_last_seen_quest_count() == 0xFFFF)
    {
        ai.set_guild_chat_last_seen_quest_count(quest_count_now);
        return false;
    }
    if (quest_count_now <= ai.guild_chat_last_seen_quest_count()) return false;
    const uint32 gc_now_ms = GameTime::GetGameTimeMS();
    if (ai.guild_chat_last_brag_ms() != 0 &&
        gc_now_ms - ai.guild_chat_last_brag_ms() <= 10u * 60u * 1000u)
    {
        // Update tracker even if we skip — don't re-evaluate same transition.
        ai.set_guild_chat_last_seen_quest_count(quest_count_now);
        return false;
    }
    if (ai.rng().int_range(0, 10) != 0)
    {
        ai.set_guild_chat_last_seen_quest_count(quest_count_now);
        return false;
    }
    return true;
}

bool ChatQuestBragFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    ai.set_guild_chat_last_seen_quest_count(s.completed_quest_count());
    static constexpr char const* kBragVariants[] = {
        "another one down", "just finished a quest",
        "feeling productive tonight", "quest log getting lighter",
        "another tick on the list", "ding XP",
        "easy gold", "quest hub clear",
        "love a good quest chain", "rolling through quests",
    };
    const int32_t n_br = sizeof(kBragVariants) / sizeof(*kBragVariants);
    const size_t bi = static_cast<size_t>(ai.rng().int_range(0, n_br));
    emit.guild_chat(kBragVariants[bi]);
    ai.note_guild_chat_brag(GameTime::GetGameTimeMS());
    ai.set_last_rule_fired("idle:guild_chat:quest_brag");
    return true;
}

// ---------- idle:guild_chat:lfm_pulse ----------
bool ChatLfmPulseGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    const uint32 gc_now_ms = GameTime::GetGameTimeMS();
    if (!s.lfg_in_queue())
    {
        // Clear tracker on exit so next queue cycle measures from scratch.
        if (ai.guild_chat_lfg_queue_entered_ms() != 0)
            ai.set_guild_chat_lfg_queue_entered_ms(0);
        return false;
    }
    if (ai.guild_chat_lfg_queue_entered_ms() == 0)
        ai.set_guild_chat_lfg_queue_entered_ms(gc_now_ms);
    const uint32 queued_for_ms = gc_now_ms - ai.guild_chat_lfg_queue_entered_ms();
    const bool over_threshold = queued_for_ms > 2u * 60u * 1000u;
    const bool cooldown_ok =
        ai.guild_chat_last_lfm_ms() == 0 ||
        gc_now_ms - ai.guild_chat_last_lfm_ms() > 60u * 1000u;
    return over_threshold && cooldown_ok;
}

bool ChatLfmPulseFire(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    emit.guild_chat("LFM for a dungeon, anyone?");
    ai.note_guild_chat_lfm(GameTime::GetGameTimeMS());
    ai.set_last_rule_fired("idle:guild_chat:lfm_pulse");
    return true;
}

// ---------- idle:guild_event:announce ----------
bool EventAnnounceGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    return s.guild_has_pending_callout() && s.guild_rank_id() <= 1;
}

bool EventAnnounceFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    ::Playerbot::V2::GuildEventKind k = ::Playerbot::V2::GuildEventKind::None;
    uint16_t mins_until = 0;
    Services::Guilds().ConsumePendingCallout(s.guild_id(), k, mins_until);
    if (k == ::Playerbot::V2::GuildEventKind::None) return false;
    char const* event_label = "Event";
    switch (k)
    {
        case ::Playerbot::V2::GuildEventKind::TavernParty:  event_label = "Tavern party"; break;
        case ::Playerbot::V2::GuildEventKind::RaidNight:    event_label = "Raid night"; break;
        case ::Playerbot::V2::GuildEventKind::DungeonNight: event_label = "Dungeon night"; break;
        case ::Playerbot::V2::GuildEventKind::BgNight:      event_label = "BG night"; break;
        default: break;
    }
    emit.guild_chat(fmt::format("{} starts in {} min!", event_label, mins_until));
    ai.set_last_rule_fired("idle:guild_event:announce");
    return true;
}

// ---------- idle:guild_event:tavern (walk / dance / chat / idle) ----------
bool EventTavernGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    if (s.guild_active_event_kind() !=
        static_cast<uint8>(::Playerbot::V2::GuildEventKind::TavernParty))
        return false;
    const auto kEvFaction = FactionOf(s);
    auto const& spot = ::Playerbot::V2::kFactionTavern[static_cast<int>(kEvFaction)];
    return s.map_id() == spot.map_id;
}

bool EventTavernFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const auto kEvFaction = FactionOf(s);
    auto const& spot = ::Playerbot::V2::kFactionTavern[static_cast<int>(kEvFaction)];
    float bx5, by5, bz5; s.position(bx5, by5, bz5);
    const float tdx = spot.x - bx5, tdy = spot.y - by5, tdz = spot.z - bz5;
    const float dist_sq = tdx*tdx + tdy*tdy + tdz*tdz;
    constexpr float kPartyRadius = 12.0f;
    if (dist_sq > kPartyRadius * kPartyRadius)
    {
        emit.move_to(spot.x, spot.y, spot.z, /*run=*/true);
        ai.set_last_rule_fired("idle:guild_event:tavern_walk");
        return true;
    }
    static constexpr uint32 kPartyEmotes[] = {
        10, 4, 3, 5, 71,
    };
    const int32_t n_em = sizeof(kPartyEmotes) / sizeof(*kPartyEmotes);
    if (ai.rng().int_range(0, 6) == 0)
    {
        const size_t ei = static_cast<size_t>(ai.rng().int_range(0, n_em));
        emit.emit(PerformEmoteIntent{kPartyEmotes[ei], ObjectGuid::Empty});
        ai.set_last_rule_fired("idle:guild_event:tavern_dance");
        return true;
    }
    if (ai.rng().int_range(0, 20) == 0)
    {
        static constexpr char const* kPartyChat[] = {
            "this is fun", "cheers everyone", "/dance",
            "best night of the week", "love these party nights",
            "more bots should hit the tavern", "great turnout",
            "tavern night incoming", "rolling tipsy", "/toast",
        };
        const int32_t n_pc = sizeof(kPartyChat) / sizeof(*kPartyChat);
        const size_t pi = static_cast<size_t>(ai.rng().int_range(0, n_pc));
        emit.guild_chat(kPartyChat[pi]);
        ai.set_last_rule_fired("idle:guild_event:tavern_chat");
        return true;
    }
    ai.set_last_rule_fired("idle:guild_event:tavern_idle");
    return true;
}

// ---------- idle:guild_event:staging (walk / chat / idle) ----------
bool EventStagingGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    const uint8 active_kind = s.guild_active_event_kind();
    const bool is_staging =
        active_kind == static_cast<uint8>(::Playerbot::V2::GuildEventKind::RaidNight) ||
        active_kind == static_cast<uint8>(::Playerbot::V2::GuildEventKind::DungeonNight) ||
        active_kind == static_cast<uint8>(::Playerbot::V2::GuildEventKind::BgNight);
    if (!is_staging) return false;
    const auto kEvFaction2 = FactionOf(s);
    auto const& staging = ::Playerbot::V2::kFactionStaging[static_cast<int>(kEvFaction2)];
    return s.map_id() == staging.map_id;
}

bool EventStagingFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const uint8 active_kind = s.guild_active_event_kind();
    const auto kEvFaction2 = FactionOf(s);
    auto const& staging = ::Playerbot::V2::kFactionStaging[static_cast<int>(kEvFaction2)];
    float bx6, by6, bz6; s.position(bx6, by6, bz6);
    const float sdx = staging.x - bx6, sdy = staging.y - by6, sdz = staging.z - bz6;
    const float dist_sq = sdx*sdx + sdy*sdy + sdz*sdz;
    constexpr float kStagingRadius = 20.0f;
    if (dist_sq > kStagingRadius * kStagingRadius)
    {
        emit.move_to(staging.x, staging.y, staging.z, /*run=*/true);
        ai.set_last_rule_fired("idle:guild_event:staging_walk");
        return true;
    }
    if (ai.rng().int_range(0, 30) == 0)
    {
        char const* line = "anyone joining?";
        if (active_kind == static_cast<uint8>(::Playerbot::V2::GuildEventKind::RaidNight))
        {
            static constexpr char const* kRaidLines[] = {
                "raid night, who's in?", "rolling for raid spots",
                "/g raid in 10", "looking for one more dps",
                "loot reservations open",
            };
            line = kRaidLines[ai.rng().int_range(0, 5)];
        }
        else if (active_kind == static_cast<uint8>(::Playerbot::V2::GuildEventKind::DungeonNight))
        {
            static constexpr char const* kDungeonLines[] = {
                "dungeon night, who's down?", "lf2m for keys",
                "let's chain some 5-mans", "got a healer?",
                "rolling tank dual spec",
            };
            line = kDungeonLines[ai.rng().int_range(0, 5)];
        }
        else
        {
            static constexpr char const* kBgLines[] = {
                "BG night! /join wsg", "premade rolling",
                "honor cap incoming", "anyone seen the queue times?",
                "rated arena later maybe",
            };
            line = kBgLines[ai.rng().int_range(0, 5)];
        }
        emit.guild_chat(line);
        ai.set_last_rule_fired("idle:guild_event:staging_chat");
        return true;
    }
    ai.set_last_rule_fired("idle:guild_event:staging_idle");
    return true;
}

// ---------- idle:guild_recruit_channel ----------
bool RecruitChannelGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GuildChatQuietGate(s, ai, g, now_ms)) return false;
    if (s.guild_rank_id() > 1) return false;
    if (s.guild_member_count() >= Services::Guilds().MaxMembersPerGuild()) return false;
    if (!Services::Guilds().RecruitmentChannelEnabled()) return false;
    return ai.rng().int_range(0, 20) == 0;
}

bool RecruitChannelFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    const uint32 chan_now_ms = GameTime::GetGameTimeMS();
    if (!Services::Guilds().TryClaimRecruitChannelPost(s.guild_id(), chan_now_ms))
        return false;
    emit.guild_recruit_channel_post();
    ai.set_last_rule_fired("idle:guild_recruit_channel");
    return true;
}

// ---------- idle:guild_chat_babble ----------
bool ChatBabbleGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    if (!s.in_guild()) return false;
    // SC-P3c: babble is the lowest-value self-initiated ambient guild
    // chatter. Suppress it entirely when no human guildmate is online —
    // a bot-only guild filling /g with "Hey everyone!" every 30 min is
    // pure noise (and a tell). Reactive announces stay unconditional.
    if (s.guild_online_human_member_count() < 1) return false;
    if (ai.personality().verbosity == Verbosity::Silent) return false;
    if (ai.personality().verbosity == Verbosity::Terse) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;

    constexpr uint32 kBabbleThrottleMs = 30u * 60u * 1000u;
    const uint32 now_ms = s.published_at_ms();
    if (ai.last_guild_babble_ms() == 0)
    {
        const uint32 stagger = (s.bot_id() * 9173u) % kBabbleThrottleMs;
        const uint32 base = now_ms > kBabbleThrottleMs
                            ? now_ms - kBabbleThrottleMs
                            : 0u;
        ai.set_last_guild_babble_ms(base + stagger);
    }
    const uint32 last_babble = ai.last_guild_babble_ms();
    return now_ms != 0 && now_ms >= last_babble &&
           (now_ms - last_babble) >= kBabbleThrottleMs;
}

bool ChatBabbleFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    constexpr uint32 kBabbleThrottleMs = 30u * 60u * 1000u;
    const uint32 now_ms = s.published_at_ms();
    constexpr char const* kPhrases[] = {
        "Hey everyone!",
        "anyone need a hand with anything?",
        "Got some good loot today.",
        "Anyone running dungeons later?",
        "Crafting reagents up in the bank if anyone needs them.",
        "Who's online tonight?",
        "lfg if anyone wants to group",
        "How's everyone doing?",
        "Just dinged again — slow grind.",
        "Hope to see folks at the next raid night.",
    };
    constexpr uint32 kPoolSize = sizeof(kPhrases) / sizeof(kPhrases[0]);
    const uint32 bucket = now_ms / kBabbleThrottleMs;
    const uint32 sel = (s.bot_id() ^ bucket) % kPoolSize;
    emit.emit(ChatIntent{GuildChatIntent{std::string(kPhrases[sel])}});
    ai.set_last_guild_babble_ms(now_ms);
    ai.set_last_rule_fired("idle:guild_chat_babble");
    return true;
}

} // anonymous namespace

void RegisterGuildRules(IdleRuleRegistry& r)
{
    // All guild rules are social/utility — internal stamps already
    // throttle the actual fire cadence to minute+ scale (CharterDrive
    // FSM steps every few seconds; chat banter and event rules use
    // RNG-rolled cooldowns; recruit rules walk nearby_friends once per
    // few seconds at most). Adding coarse min_interval_ms here saves
    // the per-tick gate eval (which still scans bot state + RNG-rolls)
    // for the 30-150 ticks/min where the internal stamp would have
    // returned false anyway. Per IdleRule perf audit 2026-05-21.

    // ----- Charter family (highest priority within guild rules) -----
    {
        IdleRule rule;
        rule.name     = "idle:guild_charter_drive";
        rule.priority = 348;
        rule.gate     = &CharterDriveGate;
        rule.fire     = &CharterDriveFire;
        // Charter FSM steps every few seconds; 2s throttle is below
        // the phase-transition cadence so progress isn't delayed.
        rule.min_interval_ms = 2000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_charter_sign";
        rule.priority = 346;
        rule.gate     = &CharterSignGate;
        rule.fire     = &CharterSignFire;
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_recruit_nearby";
        rule.priority = 344;
        rule.gate     = &RecruitNearbyGate;
        rule.fire     = &RecruitNearbyFire;
        // Walks nearby_friends each gate; 5s throttle saves the scan.
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    // ----- Chat suite -----
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:ding";
        rule.priority = 340;
        rule.gate     = &ChatDingGate;
        rule.fire     = &ChatDingFire;
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:login_greet";
        rule.priority = 338;
        rule.gate     = &ChatLoginGreetGate;
        rule.fire     = &ChatLoginGreetFire;
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:smalltalk";
        rule.priority = 336;
        rule.gate     = &ChatSmalltalkGate;
        rule.fire     = &ChatSmalltalkFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:tavern_hangout";
        rule.priority = 334;
        rule.gate     = &ChatTavernHangoutGate;
        rule.fire     = &ChatTavernHangoutFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:rival_banter";
        rule.priority = 332;
        rule.gate     = &ChatRivalBanterGate;
        rule.fire     = &ChatRivalBanterFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:quest_brag";
        rule.priority = 330;
        rule.gate     = &ChatQuestBragGate;
        rule.fire     = &ChatQuestBragFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat:lfm_pulse";
        rule.priority = 328;
        rule.gate     = &ChatLfmPulseGate;
        rule.fire     = &ChatLfmPulseFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    // ----- Event family -----
    {
        IdleRule rule;
        rule.name     = "idle:guild_event:announce";
        rule.priority = 326;
        rule.gate     = &EventAnnounceGate;
        rule.fire     = &EventAnnounceFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_event:tavern";
        rule.priority = 324;
        rule.gate     = &EventTavernGate;
        rule.fire     = &EventTavernFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_event:staging";
        rule.priority = 322;
        rule.gate     = &EventStagingGate;
        rule.fire     = &EventStagingFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    // ----- Recruit channel -----
    {
        IdleRule rule;
        rule.name     = "idle:guild_recruit_channel";
        rule.priority = 320;
        rule.gate     = &RecruitChannelGate;
        rule.fire     = &RecruitChannelFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    // ----- Babble (low ambient priority) -----
    {
        IdleRule rule;
        rule.name     = "idle:guild_chat_babble";
        rule.priority = 250;
        rule.gate     = &ChatBabbleGate;
        rule.fire     = &ChatBabbleFire;
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

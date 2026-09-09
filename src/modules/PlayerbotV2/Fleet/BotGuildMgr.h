// BotGuildMgr - Bot-managed guild fleet (6 guilds per faction, 75 cap).
//
// PlayerbotV2's bots historically had no guild affiliation: the random
// character distributor sometimes inherited a `guild_id` from a deleted
// player, but no bot ever **created** a guild, **invited** another bot,
// or **chatted** as a guildmate. The result was a fleet that felt like
// strangers passing through each capital — no social cohesion, no
// background guild-chat activity for players to overhear and react to.
//
// This manager fills that gap. **Phase A (this file) is scaffolding
// only** — interface + target table; the charter-and-signatures
// formation flow lands in Phase A.2.
//
// **Design intent (see docs/GUILD_PLAN.md):**
//   - Target 6 bot-managed guilds per faction (12 total).
//   - 75 members per guild — small enough to feel like a real social
//     unit; matches retail "active small guild" feel.
//   - Realistic formation: a founder bot **buys a Guild Charter from
//     the petitioner NPC**, walks around capital seeking 4 signatures
//     from other bots (TC `CONFIG_MIN_PETITION_SIGNS` default), then
//     turns the charter back in. Same flow a real player follows.
//   - Organic recruitment: officers + GMs of guilds with open slots
//     invite eligible guildless bots they encounter.
//   - Operator-owned guilds (GM is a non-bot account) are untouched.
//
// **bot_guild_meta** table (sql/playerbot/0006_guild_meta.sql, Phase A.2):
//   guild_id (PK), faction, theme, founder_low, rival_low, created_at,
//   last_event_at, member_cap (default 75)
//
// World-thread only. The auto-recruitment trigger fires from the bot's
// idle path which already runs on the world thread.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Player;

namespace Playerbot::V2 {

class BotGuildNamePool;
enum class GuildEventKind : uint8_t;

// Wire-in hook for the BotGuildNamePool pointer used by the manager's
// founder-election / abort paths. Called from Services::Init after the
// name pool is constructed. Kept as a free function rather than a
// member setter to avoid the chicken/egg include cycle through
// Services.h.
void BotGuildMgr_SetNamePool(BotGuildNamePool* p);

class BotGuildMgr
{
public:
    // Faction-indexed slots. We intentionally don't depend on TC's
    // TEAM_ALLIANCE/TEAM_HORDE enum values; the translator
    // PlayerFaction(Player*) below maps them.
    enum Faction : uint8
    {
        FACTION_ALLIANCE = 0,
        FACTION_HORDE    = 1,
        FACTION_COUNT    = 2,
    };

    // Target guild population per faction (config-overridable via
    // PlayerbotV2.Guilds.TargetCountPerFaction). 6 small guilds per
    // faction = realistic capital diversity without dead-air problems
    // of single-digit-member guilds; matches retail server feel.
    static constexpr uint8 kDefaultTargetGuildsPerFaction = 6;

    // Per-guild member cap (config-overridable via
    // PlayerbotV2.Guilds.MaxMembersPerGuild). 75 is small enough that
    // every member sees most chat traffic and recognizes raid regulars,
    // while large enough to fill an organic 25-man raid roster.
    static constexpr uint16 kDefaultMaxMembersPerGuild = 75;

    // Charter founding constraints. The actual `Petitions.dbc` row
    // sets the gold cost; we just gate which bots are eligible to
    // attempt founding (no point spawning a charter FSM on an L5).
    static constexpr uint8 kCharterMinFounderLevel = 25;

    // Recruitment cooldowns. Per-recruiter to avoid invite spam from a
    // chatty officer; per-target to avoid pestering the same guildless
    // bot every encounter.
    static constexpr uint32 kRecruiterCooldownMs = 5u * 60u * 1000u;
    static constexpr uint32 kRecruitTargetCooldownMs = 24u * 60u * 60u * 1000u;

    // Probability a bot accepts a cold invite from a player. Bots
    // already-in-a-bot-guild auto-decline; guildless bots accept at
    // this rate to mirror "yeah sure" player behavior.
    static constexpr float kPlayerInviteAcceptProbability = 0.6f;

    BotGuildMgr();
    ~BotGuildMgr();

    // Phase E: pull config knobs from Services::Config() into the
    // manager's runtime state. Called once at Services::Init right
    // after LoadFromDb, and again on hot-reload. Safe to call from
    // the world thread only.
    void ApplyConfig();

    // Phase E master switch. When false:
    //   - EnsureFounderElected returns immediately (no new guilds).
    //   - RunRankHygiene returns immediately (existing ranks frozen).
    //   - Event scheduler ticks but emits no active event.
    //   - QueueRecruitInvite returns false.
    //   - Trade-channel recruit rule is gated off via recruit_chan_enabled_.
    // Existing bot_guild_meta rows + Guild objects stay; the manager
    // simply stops *acting*.
    bool Enabled() const;
    bool EventsEnabled() const;
    bool RecruitmentChannelEnabled() const;

    // Per-guild metadata cached at load + populated as new guilds form.
    struct GuildMeta
    {
        uint64       guild_id     = 0;
        Faction      faction      = FACTION_ALLIANCE;
        std::string  theme;          // "adventurers", "crafters", ...
        uint64       founder_low   = 0;
        uint64       rival_low     = 0;
        uint16       member_cap   = kDefaultMaxMembersPerGuild;
    };

    // Load bot_guild_meta JOIN guild into the in-memory cache. Call
    // once at Services::Init. Idempotent on re-call (clears + reloads).
    void LoadFromDb();

    // True when guild_id is registered in bot_guild_meta — i.e., a bot
    // founded it. Used by snapshot builder to set
    // `snap->guild_is_bot_managed`. O(1) lookup.
    bool IsBotManaged(uint64 guild_id) const;

    // Open-slot count for the faction = `target - active_bot_guilds`.
    // When > 0, the manager elects a founder per
    // PlayerbotV2.Guilds.TargetCountPerFaction. Returns 0 when the
    // faction is at target or above (we never over-provision).
    uint8 OpenSlotCount(Faction f) const;

    // Live counts (excludes operator-owned guilds).
    uint8 CountActiveGuilds(Faction f) const;
    uint16 TargetGuildsPerFaction() const { return target_per_faction_; }
    uint16 MaxMembersPerGuild() const { return max_members_per_guild_; }

    // Phase A.2 entry points — currently stubbed:
    //
    // EnsureFounderElected(): when OpenSlotCount(f) > 0 AND no founder
    // FSM is currently active for that faction, picks an eligible bot
    // and seeds a BotGuildCharterFSM. One founder at a time per
    // faction; the charter flow takes 10-20 in-game min so there's no
    // need to parallelize.
    bool EnsureFounderElected(Faction f);

    // Charter FSM kickstart helper. Called from EnsureFounderElected
    // right after the founder slot is stamped: teleports the founder
    // onto the petitioner plaza in their faction capital, then pulls
    // up to 4 guildless online same-faction bots into an 8y ring around
    // them so the FSM's signer branch fires immediately on the next
    // tick. Without this co-location, founders sit in their leveling
    // zone (no nearby petitioner NPC) and the FSM aborts via the
    // 30-min budget every cycle.
    void TeleportFounderAndSigners(uint64 founder_low, Faction f);

    // QueueRecruitInvite(): called from idle:guild_recruit_nearby when
    // an officer bot is near a guildless bot AND the officer's guild
    // has open slots AND cooldowns allow. Returns true when an invite
    // was queued.
    bool QueueRecruitInvite(Player* recruiter, Player* target);

    // Lookup helpers for snapshot / chat rules. Returns BY VALUE: MetaForGuild
    // is called from the Phase 4 parallel snapshot-build workers concurrently
    // with OnCharterSucceeded inserting into meta_by_guild_id_; a raw pointer
    // into the map could dangle if a concurrent insert rehashed after the lock
    // released. std::nullopt when the guild is not bot-managed.
    std::optional<GuildMeta> MetaForGuild(uint64 guild_id) const;
    std::vector<uint64> ActiveGuildIds(Faction f) const;

    // ---- Charter founder bookkeeping (Phase A.2) ----
    //
    // At most one founder per faction is active at any time — the
    // charter FSM takes 10-20 in-game min to complete, and parallel
    // founders would race for the same signers and pool names.
    //
    // ActiveFounderLow(faction) returns the guid_low of the founder
    // bot the manager elected (0 = no active founder). The idle rule
    // matches against the bot's own guid_low to decide whether THIS
    // bot should drive the charter FSM this tick.
    //
    // active_founder_name_ is the reserved name from BotGuildNamePool;
    // the FSM passes this to BotBuyGuildCharter at phase 2.
    uint64 ActiveFounderLow(Faction f) const;
    std::string ActiveFounderName(Faction f) const;

    // Charter item guid_low for the active founder. Populated at FSM
    // phase 2 (buy) so the signer rule (other guildless bots of same
    // faction) can find the petition by id without scanning the
    // founder's bag from a different worker thread. 0 = founder hasn't
    // bought yet (or no active founder).
    uint64 ActiveFounderPetitionLow(Faction f) const;
    void   SetActiveFounderPetitionLow(Faction f, uint64 petition_low);

    // Called by the FSM on success (guild submitted) — releases the
    // founder slot, inserts the bot_guild_meta row, refreshes caches.
    void OnCharterSucceeded(Faction f, uint64 founder_low, uint64 guild_id,
                            std::string const& name);

    // Called by the FSM on abort (timeout, charter lost, etc.) —
    // releases the founder slot + name reservation. The next
    // EnsureFounderElected tick will elect a fresh founder.
    void OnCharterAborted(Faction f);

    // Periodic tick (Phase A.2 wires this from Services::Tick at 60s
    // cadence). Walks each faction, ensures founder elected if
    // OpenSlotCount > 0, sweeps stale reservations, and once per
    // real-day runs the rank-ladder hygiene pass.
    void Tick(uint32 now_ms);

    // ---- Phase B: recruitment + rank ladder ----
    //
    // Member metadata for hygiene. Returns 0 when the bot isn't yet
    // tracked in `bot_guild_member_meta` (e.g. real player guild).
    // Days since `joined_at`.
    uint32 GetMemberDaysInGuild(uint64 guild_id, uint64 char_guid_low) const;

    // Called by the recruit-rule executor handler after a successful
    // Guild::AddMember. Inserts (guild_id, char_guid_low) row into
    // bot_guild_member_meta. Idempotent via INSERT IGNORE.
    void OnBotJoinedGuild(uint64 guild_id, uint64 char_guid_low);

    // Called by the rank-promotion path after Guild::ChangeMemberRank
    // succeeds. Updates `last_promoted_at`. No-op when unknown.
    void OnBotPromoted(uint64 guild_id, uint64 char_guid_low);

    // ---- Phase D: events ----
    // The scheduler stamps the currently-active server-wide event kind
    // here once per minute. Snapshot builder copies it into per-bot
    // snapshots so idle rules can react. uint8 cast of GuildEventKind.
    uint8 ActiveEventKind() const;
    void  SetActiveEventKind(GuildEventKind kind);

    // Pre-announce queue: scheduler calls this once per event when
    // we're in the [start - pre_announce_min, start) window. The
    // manager picks one online officer per bot-managed guild and
    // increments a "pending callout" counter the idle rule consumes.
    // `minutes_until` is included in the callout text.
    void  QueueEventPreAnnounce(GuildEventKind kind, uint16 minutes_until);

    // Read + consume one pending callout for `guild_id`. Returns
    // (kind, minutes_until); kind==None when no callout pending.
    // Once consumed the slot resets to None — only one officer per
    // guild emits the callout per pre-announce window.
    void  ConsumePendingCallout(uint64 guild_id, GuildEventKind& out_kind, uint16& out_minutes_until);
    bool  HasPendingCallout(uint64 guild_id) const;

    // Phase A.2 polish: real-time signature count for the active
    // founder's petition. Reads sPetitionMgr->GetPetition(...) on the
    // world thread. Returns 0 when no active petition (founder hasn't
    // bought charter yet, or already turned in). Used by the idle FSM
    // to advance from phase 3 (seek signatures) to phase 4 (walk back)
    // once required signatures (CONFIG_MIN_PETITION_SIGNS, default 4)
    // are collected — replaces the prior fixed 60s wait.
    uint32 ActiveFounderSignatureCount(Faction f) const;

    // Phase E.1: per-guild trade-channel recruit-post cooldown.
    // Returns true when `guild_id`'s last recruit post was > 15 min
    // ago (or never). On Ok, stamps the new post time. Per-guild
    // (not per-bot) so only one officer per guild posts per cooldown
    // window — players see clean recruitment scroll, not spam.
    bool  TryClaimRecruitChannelPost(uint64 guild_id, uint32 now_ms);

    // Daily rank-ladder hygiene. For each bot-managed guild:
    //   * Initiate (rank 4) → Member (3) after 7 days.
    //   * Member  (3) → Veteran (2) after 30 days.
    //   * Veteran (2) → Officer (1) if guild has < target officers AND
    //     bot is the longest-tenured non-officer.
    // Officer count target: floor(member_count / 15), min 2, max 5.
    // GM (rank 0) is never demoted by hygiene; charter founder stays
    // GM until they /gquit. Rotation on departure lives in Phase B.2.
    void RunRankHygiene();

private:
    mutable std::mutex                                  mtx_;
    uint16                                              target_per_faction_   = kDefaultTargetGuildsPerFaction;
    uint16                                              max_members_per_guild_ = kDefaultMaxMembersPerGuild;
    std::array<std::vector<uint64>, FACTION_COUNT>      active_by_faction_{};
    std::unordered_map<uint64, GuildMeta>               meta_by_guild_id_;

    // Per-faction active founder. 0 = none. Mutated by
    // EnsureFounderElected, OnCharterSucceeded, OnCharterAborted.
    std::array<uint64, FACTION_COUNT>                   active_founder_low_{};
    std::array<std::string, FACTION_COUNT>              active_founder_name_{};
    std::array<uint32, FACTION_COUNT>                   active_founder_at_ms_{};
    std::array<uint64, FACTION_COUNT>                   active_founder_petition_low_{};

    // Last manager tick (for 60s rate-limit on EnsureFounderElected).
    uint32                                              last_tick_ms_ = 0;
    // Last rank-hygiene tick (Phase B). Runs once per real-day.
    uint32                                              last_rank_hygiene_ms_ = 0;
    // Phase E config snapshot (refreshed by ApplyConfig()).
    bool                                                enabled_              = true;
    bool                                                events_enabled_       = true;
    bool                                                recruit_chan_enabled_ = true;

    // Phase D: currently-active event kind (uint8 cast of
    // GuildEventKind). 0 = None. Set by BotGuildEventScheduler.
    uint8                                               active_event_kind_ = 0;
    // Per-guild pending callout: when scheduler enters pre-announce
    // window, every bot-managed guild gets one entry (kind +
    // minutes_until). First idle officer of that guild to fire the
    // callout rule consumes the entry; subsequent officers see it
    // empty and skip.
    struct PendingCallout { uint8 kind = 0; uint16 minutes_until = 0; };
    std::unordered_map<uint64, PendingCallout>          pending_callouts_;
    // Phase E.1: per-guild last recruit-channel post time (ms). For
    // the 15-min cooldown. Bounded by number of bot guilds (≤12).
    std::unordered_map<uint64, uint32>                  last_recruit_post_ms_;

    // #4C: per-recruiter + per-target last-invite timestamps (ms) for
    // QueueRecruitInvite's cooldown gates (kRecruiterCooldownMs /
    // kRecruitTargetCooldownMs). The per-target map is swept of entries
    // older than the target cooldown opportunistically on insert so it
    // stays bounded by the live guildless population.
    std::unordered_map<uint64, uint32>                  last_recruiter_invite_ms_;
    std::unordered_map<uint64, uint32>                  last_target_invite_ms_;

    // Owned scheduler — ticked from BotGuildMgr::Tick. Defined as a
    // unique_ptr to keep BotGuildEventScheduler an opaque type from
    // this header's POV (forward decl above).
    std::unique_ptr<class BotGuildEventScheduler>       event_scheduler_;
};

} // namespace Playerbot::V2

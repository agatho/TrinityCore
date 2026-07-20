// BotGuildMgr - see header for design overview.
//
// Phase A scaffolding: this file deliberately keeps the implementation
// surface tiny — LoadFromDb / IsBotManaged / OpenSlotCount /
// CountActiveGuilds are real; the founder-elect, charter FSM, and
// invite-queue paths are stubbed until Phase A.2 lands the full
// PetitionsHandler-driven flow.
//
// The intent of landing the manager interface early (even mostly-stub)
// is so the snapshot builder + Services init order have the symbol
// they'll need, and follow-up PRs (Phase A.2, B, C, D, E) are surgical
// edits rather than cross-cutting interface churn.

#include "BotGuildMgr.h"

#include "../Bot/BotAI.h"
#include "../Bot/BotRegistry.h"
#include "BotGroupBuilder.h"
#include "BotGuildEvent.h"
#include "BotGuildEventScheduler.h"
#include "BotGuildNamePool.h"
#include "DatabaseEnv.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "PetitionMgr.h"
#include "Player.h"
#include "PlayerbotMovement.h"  // SafeTeleport for founder + signer placement
#include "SharedDefines.h"
#include "World.h"
#include "../Services.h"
#include "../Util/ConfigReader.h"
#include "../Bot/BotIntent.h"
#include "../Threading/IntentQueue.h"
#include <algorithm>

namespace {

// Per-process singleton for BotGuildNamePool. Services::Init owns the
// underlying object; we hand the manager a pointer so Phase A.2's
// founder-election + abort paths can reserve/release without circular
// includes through Services.h. Set by SetNamePool() at Init time.
::Playerbot::V2::BotGuildNamePool* g_name_pool = nullptr;

// Petitioner spawn coords. Picked to land within 10y of an actual
// UNIT_NPC_FLAG_PETITIONER (0x40000) carrier on a dense navmesh-valid
// plaza, so the founder lands on-mesh and the FSM's nearby_friends scan
// picks up the petitioner immediately. Coords verified via world.creature
// JOIN creature_template WHERE npcflag & 262144.
//   Alliance: Aldwin Laughlin (entry 4974) at Stormwind Trade District
//             (-8889.8, 607.3, 95.3).
//   Horde:    Urtrun Clanbringer (entry 3370) at Orgrimmar Valley of
//             Strength (1565.2, -4318.8, 23.3).
struct PetitionerSpot { uint32 map_id; float x, y, z; };
constexpr PetitionerSpot kPetitionerAlliance  = { /*EK*/        0, -8889.0f,   607.0f,   95.3f };
constexpr PetitionerSpot kPetitionerHorde     = { /*Kalimdor*/  1,  1565.0f, -4319.0f,   23.3f };

} // anonymous

namespace Playerbot::V2 {

// Wire-in hook exposed via header in a future PR; for Phase A.2 the
// Services init code calls this directly after constructing the pool.
void BotGuildMgr_SetNamePool(BotGuildNamePool* p) { g_name_pool = p; }

// Out-of-line ctor/dtor — the scheduler is a unique_ptr to a
// forward-declared type in the header; defining the destructor here
// where the full type is visible avoids "use of incomplete type" at
// every call site that constructs/destroys a BotGuildMgr.
BotGuildMgr::BotGuildMgr() = default;
BotGuildMgr::~BotGuildMgr() = default;

void BotGuildMgr::ApplyConfig()
{
    // Read once at Init + on reload — Phase E knobs. Master disable
    // short-circuits subsequent EnsureFounderElected / RunRankHygiene
    // calls via the `enabled_` flag check.
    if (!Services::Initialized()) return;
    ConfigReader const& cfg = Services::Config();
    std::lock_guard<std::mutex> g(mtx_);
    enabled_              = cfg.guilds_enabled();
    target_per_faction_   = cfg.guilds_target_count_per_faction();
    max_members_per_guild_= cfg.guilds_max_members_per_guild();
    events_enabled_       = cfg.guilds_events_enabled();
    recruit_chan_enabled_ = cfg.guilds_recruitment_channel_enabled();
}

bool BotGuildMgr::Enabled() const
{
    std::lock_guard<std::mutex> g(mtx_);
    return enabled_;
}

bool BotGuildMgr::EventsEnabled() const
{
    std::lock_guard<std::mutex> g(mtx_);
    return events_enabled_;
}

bool BotGuildMgr::RecruitmentChannelEnabled() const
{
    std::lock_guard<std::mutex> g(mtx_);
    return recruit_chan_enabled_;
}

void BotGuildMgr::LoadFromDb()
{
    std::lock_guard<std::mutex> g(mtx_);
    for (auto& v : active_by_faction_) v.clear();
    meta_by_guild_id_.clear();

    // Phase A.2 migration `sql/playerbot/0006_guild_meta.sql` will land
    // the `bot_guild_meta` table. Until then, the query below returns
    // an empty result and the manager simply reports "0 bot guilds
    // active, 6 slots open per faction" — which is correct for the
    // pre-migration state.
    QueryResult result = CharacterDatabase.Query(
        "SELECT m.guild_id, m.faction, m.theme, m.founder_low, m.rival_low, m.member_cap "
        "FROM bot_guild_meta m "
        "JOIN guild g ON g.guildid = m.guild_id");

    if (!result)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGuildMgr] No bot_guild_meta rows loaded (table missing or empty). "
            "Phase A.2 will start founding bot guilds once the charter FSM lands.");
        return;
    }

    do
    {
        Field* f = result->Fetch();
        GuildMeta meta;
        meta.guild_id    = f[0].GetUInt64();
        const uint8 fac  = f[1].GetUInt8();
        meta.faction     = (fac < FACTION_COUNT) ? static_cast<Faction>(fac) : FACTION_ALLIANCE;
        meta.theme       = f[2].GetString();
        meta.founder_low = f[3].GetUInt64();
        meta.rival_low   = f[4].GetUInt64();
        meta.member_cap  = f[5].GetUInt16();

        meta_by_guild_id_[meta.guild_id] = meta;
        active_by_faction_[meta.faction].push_back(meta.guild_id);
    } while (result->NextRow());

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildMgr] Loaded {} bot-managed guilds ({} Alliance, {} Horde)",
        meta_by_guild_id_.size(),
        active_by_faction_[FACTION_ALLIANCE].size(),
        active_by_faction_[FACTION_HORDE].size());
}

bool BotGuildMgr::IsBotManaged(uint64 guild_id) const
{
    if (guild_id == 0) return false;
    std::lock_guard<std::mutex> g(mtx_);
    return meta_by_guild_id_.find(guild_id) != meta_by_guild_id_.end();
}

uint8 BotGuildMgr::OpenSlotCount(Faction f) const
{
    if (f >= FACTION_COUNT) return 0;
    std::lock_guard<std::mutex> g(mtx_);
    const size_t active = active_by_faction_[f].size();
    if (active >= target_per_faction_) return 0;
    return static_cast<uint8>(target_per_faction_ - active);
}

uint8 BotGuildMgr::CountActiveGuilds(Faction f) const
{
    if (f >= FACTION_COUNT) return 0;
    std::lock_guard<std::mutex> g(mtx_);
    return static_cast<uint8>(active_by_faction_[f].size());
}

bool BotGuildMgr::EnsureFounderElected(Faction f)
{
    if (f >= FACTION_COUNT) return false;

    // 0. Master switch (Phase E). Existing guilds stay; we just stop
    // electing new founders.
    if (!Enabled()) return false;

    // 1. Skip if no open slots for this faction.
    if (OpenSlotCount(f) == 0) return false;

    // 2. One founder per faction at a time. The charter FSM takes
    //    10-20 in-game min to complete; parallel founders would race
    //    for the same signers + capital NPC.
    {
        std::lock_guard<std::mutex> g(mtx_);
        if (active_founder_low_[f] != 0) return false;
    }

    if (!g_name_pool)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGuildMgr] EnsureFounderElected({}): name pool not wired yet",
            static_cast<uint32>(f));
        return false;
    }

    // 3. Query candidates. Faction filter is done in C++ via
    //    Player::TeamForRace since CharacterDatabase doesn't carry a
    //    pre-computed team column. We over-fetch (LIMIT 50) and
    //    filter; with ~6 active guilds × ~75 members per faction the
    //    eligible pool is plenty large to find one match quickly.
    //
    //    JOIN playerbot_v2_character ensures we only consider
    //    V2-managed bots (not real players who happen to fit).
    //    `setup_pipeline_state = 0xFF` requires the bot's setup
    //    pipeline is fully complete — gear, talents, mount, capital
    //    placement all done — so the founder is ready to act.
    const uint32 charter_cost = sWorld->getIntConfig(CONFIG_CHARTER_COST_GUILD);
    const uint32 min_money    = charter_cost * 12 / 10;       // 1.2× reserve
    const uint8  min_level    = kCharterMinFounderLevel;

    // TC's `characters` table has no `guildid` column — guild membership
    // lives in `guild_member.guid -> guildid`. LEFT JOIN + IS NULL is the
    // standard "characters not in any guild" filter. (Prior version queried
    // `c.guildid = 0` which fails with ERROR 1054 "Unknown column".)
    QueryResult result = CharacterDatabase.PQuery(
        "SELECT c.guid, c.race, c.money "
        "FROM characters c "
        "JOIN playerbot_v2_character pv ON c.guid = pv.character_guid_low "
        "LEFT JOIN guild_member gm ON gm.guid = c.guid "
        "WHERE c.level >= {} AND gm.guid IS NULL AND c.money >= {} "
        "AND pv.setup_pipeline_state = 255 "
        "AND c.online = 1 "                  // must be in-world so TeleportFounderAndSigners can act
        "ORDER BY RAND() LIMIT 50",
        min_level, min_money);

    if (!result)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGuildMgr] No eligible founder candidates for faction={} "
            "(level>={} money>={} setup_complete)",
            static_cast<uint32>(f), min_level, min_money);
        return false;
    }

    uint64 chosen_low = 0;
    do
    {
        Field* row = result->Fetch();
        const uint64 guid_low = row[0].GetUInt64();
        const uint8  race     = row[1].GetUInt8();
        const Team   team     = Player::TeamForRace(race);
        const Faction fac     = (team == ALLIANCE) ? FACTION_ALLIANCE : FACTION_HORDE;
        if (fac != f) continue;
        chosen_low = guid_low;
        break;
    } while (result->NextRow());

    if (chosen_low == 0)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGuildMgr] No faction={} founder among 50 candidates "
            "(unbalanced race distribution?)",
            static_cast<uint32>(f));
        return false;
    }

    // 4. Reserve a name from the pool.
    const std::string name = g_name_pool->PickAndReserve(
        static_cast<BotGuildNamePool::Faction>(f), chosen_low);
    if (name.empty()) return false;     // pool exhausted, retry next tick.

    // 5. Stamp the active founder slot. The idle:guild_charter_drive
    //    rule on the chosen bot's tick will pick this up and start
    //    walking to a petitioner NPC.
    const uint32 now_ms = getMSTime();
    {
        std::lock_guard<std::mutex> g(mtx_);
        active_founder_low_[f]    = chosen_low;
        active_founder_name_[f]   = name;
        active_founder_at_ms_[f]  = now_ms;
    }

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildMgr] Elected founder guid_low={} for faction={} "
        "with reserved name '{}'",
        chosen_low, static_cast<uint32>(f), name);

    // 6. Charter FSM kickstart: teleport the founder + up to 4 signer
    //    candidates onto the same plaza near a guild master NPC.
    //    Without this, founders are wherever BotSetupPipeline placed
    //    them (a leveling zone matching their level bracket), and the
    //    FSM's phase-0 nearby-petitioner scan (40y) finds nothing —
    //    every previously-elected founder hit the 30-min budget abort.
    //    Co-locating 5 bots on the plaza also satisfies the signer
    //    branch (CharterSignFire requires founder within 30y of signer).
    TeleportFounderAndSigners(chosen_low, f);

    return true;
}

void BotGuildMgr::TeleportFounderAndSigners(uint64 founder_low, Faction f)
{
    if (f >= FACTION_COUNT) return;
    auto const spot = (f == FACTION_ALLIANCE) ? kPetitionerAlliance : kPetitionerHorde;

    // Teleport founder first so signers land NEXT to the founder's
    // freshly-set Player position.
    ObjectGuid founder_guid = ObjectGuid::Create<HighGuid::Player>(founder_low);
    Player* founder = ObjectAccessor::FindConnectedPlayer(founder_guid);
    if (!founder)
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotGuildMgr] Founder guid_low={} not in-world; charter FSM "
            "will attempt phase-0 in current location (likely abort)",
            founder_low);
        return;
    }
    Playerbot::BotMovement::SafeTeleport(founder,
        WorldLocation(spot.map_id, Position(spot.x, spot.y, spot.z, 0.f)),
        /*options*/ 0);
    // Arm the founder's charter-grace so idle:travel_to_hub /
    // idle:wander don't yank them back to their level zone before the
    // FSM completes. 20-min grace covers the full 30-min FSM budget
    // minus initial walk + signature collection.
    if (BotAI* fai = Services::Registry().ai(founder_low))
        fai->arm_charter_grace(getMSTime());

    // Pull up to 4 signer candidates: guildless V2 bots, same faction,
    // setup complete, currently online. Pre-fetch 20 to over-sample
    // before C++-side faction filtering (race→team isn't a DB column).
    QueryResult sigs = CharacterDatabase.PQuery(
        "SELECT c.guid, c.race FROM characters c "
        "JOIN playerbot_v2_character pv ON c.guid = pv.character_guid_low "
        "LEFT JOIN guild_member gm ON gm.guid = c.guid "
        "WHERE gm.guid IS NULL "
        "AND pv.setup_pipeline_state = 255 "
        "AND c.guid != {} AND c.online = 1 "
        "ORDER BY RAND() LIMIT 20", founder_low);

    if (!sigs)
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotGuildMgr] No online signer candidates for faction={} — "
            "founder will need 4 organic signers before phase-3 timeout",
            static_cast<uint32>(f));
        return;
    }

    uint32 pulled = 0;
    uint32 angle_idx = 0;
    constexpr uint32 kMaxSignersToPull = 4;
    constexpr float  kRingRadius = 8.0f;
    constexpr float  k2Pi        = 6.2831853f;
    do
    {
        if (pulled >= kMaxSignersToPull) break;
        Field* row = sigs->Fetch();
        const uint64 sig_low = row[0].GetUInt64();
        const uint8  race    = row[1].GetUInt8();
        const Team   team    = Player::TeamForRace(race);
        const Faction fac    = (team == ALLIANCE) ? FACTION_ALLIANCE : FACTION_HORDE;
        if (fac != f) continue;

        ObjectGuid sig_guid = ObjectGuid::Create<HighGuid::Player>(sig_low);
        Player* sig = ObjectAccessor::FindConnectedPlayer(sig_guid);
        if (!sig) continue;

        // Spread the signers around the founder in a small ring so they
        // all see each other AND the founder in their 40y nearby_friends
        // snapshot. Skip if the same Player is the founder itself
        // (shouldn't happen due to WHERE guid != ..., but defensive).
        if (sig == founder) continue;

        const float angle = (float(angle_idx) * k2Pi) / float(kMaxSignersToPull);
        const float dx = std::cos(angle) * kRingRadius;
        const float dy = std::sin(angle) * kRingRadius;
        Playerbot::BotMovement::SafeTeleport(sig,
            WorldLocation(spot.map_id, Position(spot.x + dx, spot.y + dy, spot.z, 0.f)),
            /*options*/ 0);
        // Same charter-grace as the founder so signers stay on the
        // plaza until they've signed (CharterSignFire fires once they
        // see the founder in nearby_friends).
        if (BotAI* sai = Services::Registry().ai(sig_low))
            sai->arm_charter_grace(getMSTime());
        ++angle_idx;
        ++pulled;
    } while (sigs->NextRow());

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildMgr] Teleported founder + {} signer candidate(s) "
        "to faction={} petitioner plaza (map={} {:.1f},{:.1f},{:.1f})",
        pulled, static_cast<uint32>(f),
        spot.map_id, spot.x, spot.y, spot.z);
}

uint64 BotGuildMgr::ActiveFounderLow(Faction f) const
{
    if (f >= FACTION_COUNT) return 0;
    std::lock_guard<std::mutex> g(mtx_);
    return active_founder_low_[f];
}

std::string BotGuildMgr::ActiveFounderName(Faction f) const
{
    if (f >= FACTION_COUNT) return {};
    std::lock_guard<std::mutex> g(mtx_);
    return active_founder_name_[f];
}

void BotGuildMgr::OnCharterSucceeded(Faction f, uint64 founder_low,
                                     uint64 guild_id, std::string const& name)
{
    if (f >= FACTION_COUNT || guild_id == 0) return;

    // Phase E.2: pick a rival_guild_id from the existing bot-managed
    // guilds of the same faction. The newest-founded guild's rival is
    // the longest-active prior guild (asymmetric on purpose — the
    // older guild already has a rival or NULL; symmetry is the wrong
    // model for "this new guild's natural rival"). NULL when this is
    // the first guild of its faction.
    uint64 rival_low = 0;
    {
        std::lock_guard<std::mutex> g(mtx_);
        if (!active_by_faction_[f].empty())
            rival_low = active_by_faction_[f].front();
    }

    // Insert into bot_guild_meta + refresh cache + clear founder slot
    // + release name from pool (the row will move from
    // bot_guild_name_reserved to the implicit "in use" set tracked by
    // sGuildMgr->GetGuildByName once the guild row is created — Release
    // here just drops the reservation row).
    if (rival_low != 0)
    {
        CharacterDatabase.DirectPExecute(
            "INSERT INTO bot_guild_meta (guild_id, faction, founder_low, rival_low, member_cap, theme) "
            "VALUES ({}, {}, {}, {}, {}, 'adventurers')",
            guild_id, static_cast<uint32>(f), founder_low, rival_low, max_members_per_guild_);
    }
    else
    {
        CharacterDatabase.DirectPExecute(
            "INSERT INTO bot_guild_meta (guild_id, faction, founder_low, member_cap, theme) "
            "VALUES ({}, {}, {}, {}, 'adventurers')",
            guild_id, static_cast<uint32>(f), founder_low, max_members_per_guild_);
    }

    {
        std::lock_guard<std::mutex> g(mtx_);
        GuildMeta meta;
        meta.guild_id    = guild_id;
        meta.faction     = f;
        meta.theme       = "adventurers";
        meta.founder_low = founder_low;
        meta.rival_low   = rival_low;
        meta.member_cap  = max_members_per_guild_;
        meta_by_guild_id_[guild_id] = meta;
        active_by_faction_[f].push_back(guild_id);
        active_founder_low_[f]    = 0;
        active_founder_name_[f].clear();
        active_founder_at_ms_[f]  = 0;
    }

    if (g_name_pool) g_name_pool->Release(name);

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildMgr] Charter SUCCEEDED — '{}' (guild_id={}) founded for faction={} "
        "by bot guid_low={}",
        name, guild_id, static_cast<uint32>(f), founder_low);
}

void BotGuildMgr::OnCharterAborted(Faction f)
{
    if (f >= FACTION_COUNT) return;
    std::string released_name;
    uint64 released_founder = 0;
    {
        std::lock_guard<std::mutex> g(mtx_);
        released_name = active_founder_name_[f];
        released_founder = active_founder_low_[f];
        active_founder_low_[f]    = 0;
        active_founder_name_[f].clear();
        active_founder_at_ms_[f]  = 0;
    }
    if (g_name_pool && !released_name.empty())
        g_name_pool->Release(released_name);

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildMgr] Charter ABORTED for faction={} bot guid_low={} name='{}'",
        static_cast<uint32>(f), released_founder, released_name);
}

void BotGuildMgr::Tick(uint32 now_ms)
{
    // 60s rate limit — the manager doesn't need finer granularity
    // (charter FSM runs on bot ticks; this is just slot bookkeeping).
    if (last_tick_ms_ != 0 && (now_ms - last_tick_ms_) < 60u * 1000u)
        return;
    last_tick_ms_ = now_ms;

    // Abort founders whose FSM exceeded the 30-min budget — the bot
    // may have logged out or hit an unrecoverable wedge. Phase A.2 of
    // the FSM checks budget itself; this is the manager-side safety.
    constexpr uint32 kBudgetMs = 30u * 60u * 1000u;
    for (uint8 fi = 0; fi < FACTION_COUNT; ++fi)
    {
        uint32 at_ms = 0;
        {
            std::lock_guard<std::mutex> g(mtx_);
            at_ms = active_founder_at_ms_[fi];
        }
        if (at_ms != 0 && (now_ms - at_ms) > kBudgetMs)
            OnCharterAborted(static_cast<Faction>(fi));
    }

    // Sweep stale name reservations (60-min cutoff = 2× FSM budget).
    if (g_name_pool) g_name_pool->SweepStale(60u * 60u);

    // Elect founders for any faction with open slots and none active.
    for (uint8 fi = 0; fi < FACTION_COUNT; ++fi)
        EnsureFounderElected(static_cast<Faction>(fi));

    // Phase B: rank ladder hygiene once per real-day.
    constexpr uint32 kRankHygieneIntervalMs = 24u * 60u * 60u * 1000u;
    if (last_rank_hygiene_ms_ == 0
        || (now_ms - last_rank_hygiene_ms_) >= kRankHygieneIntervalMs)
    {
        RunRankHygiene();
        last_rank_hygiene_ms_ = now_ms;
    }

    // BotCoordinationBus async drain. Heavy handlers (group formation,
    // mass invites) queued by sync publish paths execute here on the
    // world thread. 60s drain cadence matches the bus's design — bus
    // events are tactical coordination, not real-time game state.
    if (Services::Initialized())
    {
        Services::Coordination().DrainAsync();
        // D.4: drive pending-finish records (queue starts + entrance
        // walks after a group has had time to form). Runs each Tick
        // cycle so the 10s grace window finishes within ~1 tick of
        // becoming eligible.
        Services::GroupBuilder().DrainPending(now_ms);
    }

    // Phase D: event scheduler tick. Lazy-construct so tests that
    // never touch events don't pay for it. Skipped when Phase E
    // events_enabled toggle is off (operator can keep guilds + chat
    // active but skip the timed-events traffic).
    if (EventsEnabled())
    {
        if (!event_scheduler_)
            event_scheduler_ = std::make_unique<BotGuildEventScheduler>();
        event_scheduler_->Tick(*this, now_ms);
    }
    else
    {
        // Force-clear any leftover active event so snapshot rules see
        // None — prevents tavern-walk traffic if operator flips events
        // off mid-day.
        SetActiveEventKind(GuildEventKind::None);
    }
}

uint8 BotGuildMgr::ActiveEventKind() const
{
    std::lock_guard<std::mutex> g(mtx_);
    return active_event_kind_;
}

void BotGuildMgr::SetActiveEventKind(GuildEventKind kind)
{
    std::lock_guard<std::mutex> g(mtx_);
    active_event_kind_ = static_cast<uint8>(kind);
}

void BotGuildMgr::QueueEventPreAnnounce(GuildEventKind kind, uint16 minutes_until)
{
    // Stamp one pending callout per bot-managed guild. The first idle
    // officer of each guild that fires the callout rule consumes it.
    std::lock_guard<std::mutex> g(mtx_);
    const uint8 kind_byte = static_cast<uint8>(kind);
    for (uint64 const& gid : active_by_faction_[FACTION_ALLIANCE])
        pending_callouts_[gid] = PendingCallout{kind_byte, minutes_until};
    for (uint64 const& gid : active_by_faction_[FACTION_HORDE])
        pending_callouts_[gid] = PendingCallout{kind_byte, minutes_until};
}

void BotGuildMgr::ConsumePendingCallout(uint64 guild_id, GuildEventKind& out_kind, uint16& out_minutes_until)
{
    out_kind = GuildEventKind::None;
    out_minutes_until = 0;
    if (guild_id == 0) return;
    std::lock_guard<std::mutex> g(mtx_);
    auto it = pending_callouts_.find(guild_id);
    if (it == pending_callouts_.end()) return;
    out_kind = static_cast<GuildEventKind>(it->second.kind);
    out_minutes_until = it->second.minutes_until;
    pending_callouts_.erase(it);
}

bool BotGuildMgr::HasPendingCallout(uint64 guild_id) const
{
    if (guild_id == 0) return false;
    std::lock_guard<std::mutex> g(mtx_);
    return pending_callouts_.find(guild_id) != pending_callouts_.end();
}

uint32 BotGuildMgr::ActiveFounderSignatureCount(Faction f) const
{
    if (f >= FACTION_COUNT) return 0;
    const uint64 petition_low = ActiveFounderPetitionLow(f);
    if (petition_low == 0) return 0;
    ObjectGuid petition_guid = ObjectGuid::Create<HighGuid::Item>(petition_low);
    Petition* p = sPetitionMgr->GetPetition(petition_guid);
    if (!p) return 0;
    return static_cast<uint32>(p->Signatures.size());
}

bool BotGuildMgr::TryClaimRecruitChannelPost(uint64 guild_id, uint32 now_ms)
{
    if (guild_id == 0) return false;
    constexpr uint32 kRecruitPostCooldownMs = 15u * 60u * 1000u;
    std::lock_guard<std::mutex> g(mtx_);
    auto it = last_recruit_post_ms_.find(guild_id);
    if (it != last_recruit_post_ms_.end() &&
        (now_ms - it->second) < kRecruitPostCooldownMs)
        return false;
    last_recruit_post_ms_[guild_id] = now_ms;
    return true;
}

uint32 BotGuildMgr::GetMemberDaysInGuild(uint64 guild_id, uint64 char_guid_low) const
{
    if (guild_id == 0 || char_guid_low == 0) return 0;
    QueryResult result = CharacterDatabase.PQuery(
        "SELECT TIMESTAMPDIFF(DAY, joined_at, NOW()) "
        "FROM bot_guild_member_meta "
        "WHERE guild_id = {} AND char_guid_low = {}",
        guild_id, char_guid_low);
    if (!result) return 0;
    Field* f = result->Fetch();
    int32 d = f[0].GetInt32();
    return d < 0 ? 0 : static_cast<uint32>(d);
}

void BotGuildMgr::OnBotJoinedGuild(uint64 guild_id, uint64 char_guid_low)
{
    if (guild_id == 0 || char_guid_low == 0) return;
    if (!IsBotManaged(guild_id)) return;     // skip player guilds
    CharacterDatabase.DirectPExecute(
        "INSERT IGNORE INTO bot_guild_member_meta (guild_id, char_guid_low) "
        "VALUES ({}, {})",
        guild_id, char_guid_low);
}

void BotGuildMgr::OnBotPromoted(uint64 guild_id, uint64 char_guid_low)
{
    if (guild_id == 0 || char_guid_low == 0) return;
    CharacterDatabase.DirectPExecute(
        "UPDATE bot_guild_member_meta SET last_promoted_at = NOW() "
        "WHERE guild_id = {} AND char_guid_low = {}",
        guild_id, char_guid_low);
}

void BotGuildMgr::RunRankHygiene()
{
    // Per-bot-guild walk. We iterate the active_by_faction_ snapshot
    // (small — ≤12 guilds total) outside the lock to avoid holding
    // the mutex across DB calls per the project's threading rules.
    std::vector<uint64> guilds_to_process;
    {
        std::lock_guard<std::mutex> g(mtx_);
        for (auto const& v : active_by_faction_)
            for (uint64 gid : v) guilds_to_process.push_back(gid);
    }

    for (uint64 gid : guilds_to_process)
    {
        Guild* g = sGuildMgr->GetGuildById(gid);
        if (!g) continue;

        // Count current officers (rank 1). Officer target scales with
        // membership: 1 per 15 members, clamped [2, 5].
        uint32 member_count = g->GetMembersCount();
        uint32 officer_target = std::clamp<uint32>(member_count / 15u, 2u, 5u);
        uint32 officer_count = 0;
        // Collect candidates for veteran-officer promotion (sorted later
        // by tenure DESC).
        struct OfficerCandidate { ObjectGuid guid; uint64 low; };
        std::vector<OfficerCandidate> veterans_for_officer;

        for (auto const& [guid, member] : g->GetMembers())
        {
            const GuildRankId rank = member.GetRankId();
            const uint8 rid = static_cast<uint8>(rank);
            const uint64 low = guid.GetCounter();

            if (rid == 1) ++officer_count;

            // Skip GM — never demoted by hygiene.
            if (rank == GuildRankId::GuildMaster) continue;

            const uint32 days = GetMemberDaysInGuild(gid, low);
            // Rank promotions: walk Initiate→Member→Veteran. Officer
            // promotion handled separately below (limited slots).
            GuildRankId target = rank;
            if (rid == 4 /* Initiate */ && days >= 7)
                target = static_cast<GuildRankId>(3);
            else if (rid == 3 /* Member */ && days >= 30)
                target = static_cast<GuildRankId>(2);

            if (target != rank)
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                if (g->ChangeMemberRank(trans, guid, target))
                {
                    CharacterDatabase.CommitTransaction(trans);
                    OnBotPromoted(gid, low);
                    TC_LOG_INFO("playerbot.v2",
                        "[BotGuildMgr] Promoted guid_low={} in guild_id={} from rank={} → {} (days={})",
                        low, gid, rid, static_cast<uint32>(target), days);
                }
                else
                {
                    CharacterDatabase.CommitTransaction(trans);
                }
            }
            else if (rid == 2 /* Veteran */)
            {
                veterans_for_officer.push_back({guid, low});
            }
        }

        // Promote veterans to officer until we hit officer_target.
        if (officer_count < officer_target && !veterans_for_officer.empty())
        {
            // Sort by tenure DESC.
            std::sort(veterans_for_officer.begin(), veterans_for_officer.end(),
                [&](OfficerCandidate const& a, OfficerCandidate const& b) {
                    return GetMemberDaysInGuild(gid, a.low)
                         > GetMemberDaysInGuild(gid, b.low);
                });
            const uint32 promote_n = officer_target - officer_count;
            for (uint32 i = 0; i < promote_n && i < veterans_for_officer.size(); ++i)
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                if (g->ChangeMemberRank(trans, veterans_for_officer[i].guid,
                                        static_cast<GuildRankId>(1)))
                {
                    CharacterDatabase.CommitTransaction(trans);
                    OnBotPromoted(gid, veterans_for_officer[i].low);
                    TC_LOG_INFO("playerbot.v2",
                        "[BotGuildMgr] Promoted veteran guid_low={} in guild_id={} → Officer (target={}/{})",
                        veterans_for_officer[i].low, gid,
                        officer_count + i + 1, officer_target);
                }
                else
                {
                    CharacterDatabase.CommitTransaction(trans);
                }
            }
        }
    }
}

bool BotGuildMgr::QueueRecruitInvite(Player* recruiter, Player* target)
{
    // #4C: real implementation of the Phase B outline. Validates the
    // recruiter/target pair, applies the per-recruiter + per-target
    // cooldown gates, and queues the recruit through the same
    // GuildOp::RecruitTarget intent the executor handler consumes (which
    // calls BotRecruitToGuild). Returns true only when an invite was
    // actually queued.
    //
    // 1. Null + master-switch.
    if (!recruiter || !target) return false;
    if (!Enabled()) return false;

    // 2. Recruiter must be in a bot-managed guild with an officer+ rank.
    const uint64 guild_id = recruiter->GetGuildId();
    if (guild_id == 0) return false;
    if (!IsBotManaged(guild_id)) return false;

    Guild* g = sGuildMgr->GetGuildById(guild_id);
    if (!g) return false;
    // Officer+ only (GM=0 / Officer=1). recruiter->GetGuildId() == guild_id was
    // confirmed above, so GetGuildRank() is this guild's rank. Use the public
    // Player accessor — Guild::GetMember is private in this core version.
    if (recruiter->GetGuildRank() > 1) return false;   // GM(0)/Officer(1) only

    // 3. Guild must have an open slot.
    if (g->GetMembersCount() >= max_members_per_guild_) return false;

    // 4. Target must be guildless + same faction as the recruiter.
    if (target->GetGuildId() != 0) return false;
    if (Player::TeamForRace(target->GetRace()) !=
        Player::TeamForRace(recruiter->GetRace()))
        return false;

    // 5. Cooldown gates (per-recruiter + per-target).
    const uint32 now_ms       = getMSTime();
    const uint64 recruiter_low = recruiter->GetGUID().GetCounter();
    const uint64 target_low    = target->GetGUID().GetCounter();
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto rit = last_recruiter_invite_ms_.find(recruiter_low);
        if (rit != last_recruiter_invite_ms_.end() &&
            (now_ms - rit->second) < kRecruiterCooldownMs)
            return false;
        auto tit = last_target_invite_ms_.find(target_low);
        if (tit != last_target_invite_ms_.end() &&
            (now_ms - tit->second) < kRecruitTargetCooldownMs)
            return false;
    }

    // 6. Queue the recruit through the recruiter's intent queue. The
    //    GuildOp::RecruitTarget executor (BotRecruitToGuild) performs the
    //    actual Guild::AddMember on the world thread.
    Intent it{};
    it.bot_id = recruiter_low;
    it.body   = GuildIntent{GuildOp::RecruitTarget{target_low}};
    Services::Intents(recruiter_low).push(std::move(it));

    // 7. Stamp cooldowns + opportunistically sweep stale target entries so
    //    the per-target map stays bounded by the live guildless population.
    {
        std::lock_guard<std::mutex> lock(mtx_);
        last_recruiter_invite_ms_[recruiter_low] = now_ms;
        last_target_invite_ms_[target_low]       = now_ms;
        if (last_target_invite_ms_.size() > 4096)
        {
            for (auto sweep = last_target_invite_ms_.begin();
                 sweep != last_target_invite_ms_.end(); )
            {
                if ((now_ms - sweep->second) >= kRecruitTargetCooldownMs)
                    sweep = last_target_invite_ms_.erase(sweep);
                else
                    ++sweep;
            }
        }
    }

    TC_LOG_INFO("playerbot.v2",
        "[BotGuildMgr] QueueRecruitInvite: recruiter_low={} -> target_low={} guild_id={}",
        recruiter_low, target_low, guild_id);
    return true;
}

std::optional<BotGuildMgr::GuildMeta> BotGuildMgr::MetaForGuild(uint64 guild_id) const
{
    if (guild_id == 0) return std::nullopt;
    std::lock_guard<std::mutex> g(mtx_);
    auto it = meta_by_guild_id_.find(guild_id);
    if (it == meta_by_guild_id_.end()) return std::nullopt;
    return it->second;   // copy out under the lock — no dangling pointer to the map
}

std::vector<uint64> BotGuildMgr::ActiveGuildIds(Faction f) const
{
    if (f >= FACTION_COUNT) return {};
    std::lock_guard<std::mutex> g(mtx_);
    return active_by_faction_[f];
}

uint64 BotGuildMgr::ActiveFounderPetitionLow(Faction f) const
{
    if (f >= FACTION_COUNT) return 0;
    std::lock_guard<std::mutex> g(mtx_);
    return active_founder_petition_low_[f];
}

void BotGuildMgr::SetActiveFounderPetitionLow(Faction f, uint64 petition_low)
{
    if (f >= FACTION_COUNT) return;
    std::lock_guard<std::mutex> g(mtx_);
    active_founder_petition_low_[f] = petition_low;
}

} // namespace Playerbot::V2

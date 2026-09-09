// BotGroupBuilder - see header.

#include "BotGroupBuilder.h"

#include "../Services.h"
#include "../Threading/IntentQueue.h"
#include "../Bot/BotIntent.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/BotAI.h"
#include "../Bot/BotArchetype.h"
#include "BotGuildMgr.h"

#include "DB2Stores.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Log.h"

#include <algorithm>

namespace Playerbot::V2 {

namespace {

// Pull a bot's faction-team (Alliance/Horde) from its Player* via TC's
// race table. Returns 0 = Alliance, 1 = Horde, or 0xFF when unknown
// (caller should skip).
uint8 BotFactionIndex(Player const* p)
{
    if (!p) return 0xFF;
    return (Player::TeamForRace(p->GetRace()) == ALLIANCE) ? 0u : 1u;
}

// Compose the faction-mask bit for index 0=Alliance / 1=Horde.
uint32 FactionMaskBit(uint8 idx)
{
    return idx <= 1 ? (1u << idx) : 0u;
}

// Role-affinity slots in BotArchetype::role_affinity.
enum RoleSlot : uint8 { kRoleTank = 0, kRoleHealer = 1, kRoleDps = 2 };

// Pull a bot's role affinity from its BotAI archetype (world-thread read of
// the authoritative BotArchetype — PickCandidates runs on the world thread
// via the bus DrainAsync). Returns {0,0,1} (pure DPS) when the bot has no
// registered AI, so an unmanaged candidate counts as DPS-only and never
// pre-empts a tank/healer slot.
std::array<float, 3> RoleAffinityOf(uint64 bot_low)
{
    if (Services::Initialized())
        if (::Playerbot::BotAI* ai = Services::Registry().ai(bot_low))
            return ai->archetype().role_affinity;
    return std::array<float, 3>{ 0.f, 0.f, 1.f };
}

} // anonymous

void BotGroupBuilder::RegisterSubscriptions(BotCoordinationBus& bus)
{
    // Heavy work (registry walk + invite cascade) → async dispatch.
    // The bus drains on world-thread Tick so we still execute on the
    // world thread, just decoupled from the publisher's tick.
    bus.Subscribe(CoordSignal::GuildEventForming,
        [this](CoordEvent const& ev) { OnGuildEventForming(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::LfgTankNeeded,
        [this](CoordEvent const& ev) { OnLfgRoleNeeded(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::LfgHealerNeeded,
        [this](CoordEvent const& ev) { OnLfgRoleNeeded(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::LfgDpsNeeded,
        [this](CoordEvent const& ev) { OnLfgRoleNeeded(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::BgTeamForming,
        [this](CoordEvent const& ev) { OnBgTeamForming(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::ArenaTeamForming,
        [this](CoordEvent const& ev) { OnArenaTeamForming(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::OwnerSquadAssemble,
        [this](CoordEvent const& ev) { OnOwnerSquadAssemble(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::WorldBossSpotted,
        [this](CoordEvent const& ev) { OnWorldBossSpotted(ev); },
        CoordDispatch::Async);
    bus.Subscribe(CoordSignal::MPlusKeyForming,
        [this](CoordEvent const& ev) { OnMPlusKeyForming(ev); },
        CoordDispatch::Async);
}

std::vector<uint64> BotGroupBuilder::PickCandidates(GroupRequest const& req)
{
    // Walk all in-world online bots via the registry. Filter by:
    //   - Online + alive + not in a group
    //   - Faction mask
    //   - Level bracket
    //   - Optional guild_id match
    // Sort by leader-affinity then by activity (longest in-world first)
    // so the same recurring set tends to anchor groups.
    std::vector<uint64> picked;
    picked.reserve(req.required);

    if (!Services::Initialized()) return picked;
    BotRegistry& reg = Services::Registry();

    // Pass 1: leader candidate if explicitly requested.
    if (req.leader_low != 0)
    {
        if (Player* lp = ObjectAccessor::FindConnectedPlayer(
                ObjectGuid::Create<HighGuid::Player>(req.leader_low)))
        {
            if (lp->IsInWorld() && lp->IsAlive() && !lp->GetGroup() &&
                !lp->IsInCombat() && !lp->IsDeserter() &&
                !lp->InBattleground() && !lp->InBattlegroundQueue(/*ignoreArena*/ false))
                picked.push_back(req.leader_low);
        }
    }

    // Pass 2: walk the registry of registered bots, collect all eligible
    // candidates into a pool. We DON'T cap here — role balancing (below)
    // needs the full eligible set so it can prefer tank/healer-affinity
    // bots for the requested role slots instead of taking whatever the
    // registry iteration order yields first (which was all-DPS in practice).
    std::vector<uint64> pool;
    pool.reserve(req.required * 4);
    reg.for_each([&](BotId id, BotRegistryEntry const& /*entry*/) {
        if (id == req.leader_low) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || p->GetGroup()) return;

        // Availability filters: never pull a bot that's mid-fight, sitting
        // under a Deserter debuff, or already parked in a BG/arena queue
        // into a fresh group. These were previously enforced only at the
        // final api.bg_queue gate; doing it here too keeps the formed group
        // from including members that the leader's queue would then reject
        // (which, for arenas, fails the whole group's CanJoinBattlegroundQueue).
        if (p->IsInCombat() || p->IsDeserter()) return;
        if (p->InBattleground() || p->InBattlegroundQueue(/*ignoreArena*/ false)) return;

        // Faction filter.
        if (req.faction_mask != 0)
        {
            const uint8 fi = BotFactionIndex(p);
            if ((req.faction_mask & FactionMaskBit(fi)) == 0) return;
        }
        // Level filter.
        const uint8 lvl = p->GetLevel();
        if (req.level_min != 0 && lvl < req.level_min) return;
        if (req.level_max != 0 && lvl > req.level_max) return;
        // Guild filter.
        if (req.guild_id != 0 && p->GetGuildId() != req.guild_id) return;

        pool.push_back(id);
    });

    // Role balancing (#4C item 5). When the request specifies role wants
    // (tank/healer/dps), fill the tank + healer slots FIRST from the bots
    // with the highest role_affinity for that role, so a formed group isn't
    // all-DPS. Remaining slots take whatever is left (DPS-leaning bots and
    // any overflow). When no role mix is requested, fall back to plain
    // registry order (preserves the prior behavior for owner-squad / world-
    // boss / role-agnostic requests).
    const bool role_balanced =
        req.want_tank != 0 || req.want_healer != 0 || req.want_dps != 0;
    if (!role_balanced)
    {
        for (uint64 id : pool)
        {
            if (picked.size() >= req.required) break;
            picked.push_back(id);
        }
        return picked;
    }

    // Track which pool entries have already been picked so the role passes
    // don't double-assign the same bot.
    std::vector<bool> taken(pool.size(), false);

    auto fill_role = [&](uint8 slot, uint8 want) {
        if (want == 0) return;
        // Order remaining pool members by descending affinity for `slot`.
        std::vector<size_t> order;
        order.reserve(pool.size());
        for (size_t i = 0; i < pool.size(); ++i)
            if (!taken[i]) order.push_back(i);
        std::stable_sort(order.begin(), order.end(),
            [&](size_t a, size_t b) {
                return RoleAffinityOf(pool[a])[slot] > RoleAffinityOf(pool[b])[slot];
            });
        uint8 assigned = 0;
        for (size_t idx : order)
        {
            if (assigned >= want) break;
            if (picked.size() >= req.required) break;
            // For tank/healer, require a real lean (affinity > DPS) so we
            // don't burn the slot on a pure-DPS bot when a hybrid exists;
            // if none qualifies the slot is left to the DPS/backfill pass.
            if (slot != kRoleDps)
            {
                auto const aff = RoleAffinityOf(pool[idx]);
                if (aff[slot] <= aff[kRoleDps] || aff[slot] <= 0.f)
                    break;   // order is sorted desc, so nothing better remains
            }
            picked.push_back(pool[idx]);
            taken[idx] = true;
            ++assigned;
        }
    };

    // Tank + healer first (the scarce roles), then DPS, then backfill any
    // remaining required slots from whatever eligible bots are left.
    fill_role(kRoleTank,   req.want_tank);
    fill_role(kRoleHealer, req.want_healer);
    fill_role(kRoleDps,    req.want_dps);
    for (size_t i = 0; i < pool.size() && picked.size() < req.required; ++i)
        if (!taken[i]) { picked.push_back(pool[i]); taken[i] = true; }

    return picked;
}

uint32 BotGroupBuilder::BuildGroup(GroupRequest const& req)
{
    std::vector<uint64> cands = PickCandidates(req);
    if (cands.size() < 2)        // can't form a group of one
        return 0;

    // Cap to required size.
    if (cands.size() > req.required) cands.resize(req.required);

    // Form the group DIRECTLY (server-side Group::Create + AddMember) rather
    // than invite->accept (2026-06-23). Bot invitees rarely run the accept rule
    // in time — busy leveling/questing bots left every premade stuck at size 1,
    // so DrainPending aged them out and NOTHING queued (broke arena skirmishes
    // AND guild BG/dungeon-night premades alike: idle:group_invite_accept fired
    // 0 times live). Direct membership is the exact commit the core performs on
    // invite-accept (GroupHandler.cpp: new Group -> Create(leader) ->
    // sGroupMgr->AddGroup -> AddMember) and needs no invitee consent. Runs on
    // the world thread (bus DrainAsync), so Group/ObjectAccessor ops are safe.
    const uint64 leader_low = cands.front();
    Player* leader = ObjectAccessor::FindConnectedPlayer(
        ObjectGuid::Create<HighGuid::Player>(leader_low));
    if (!leader || leader->GetGroup())
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGroupBuilder] BuildGroup abort: leader_low={} offline-or-grouped",
            leader_low);
        return 0;
    }
    Group* g = new Group();
    if (!g->Create(leader)) { delete g; return 0; }
    sGroupMgr->AddGroup(g);
    // Party caps at 5; a larger premade (raid night) must be a raid group.
    if (req.required > 5)
        g->ConvertToRaid();
    uint32 added = 0;
    for (size_t i = 1; i < cands.size(); ++i)
    {
        Player* m = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(cands[i]));
        if (!m || m->GetGroup() || m->GetTeam() != leader->GetTeam()) continue;
        if (g->IsFull()) break;
        if (g->AddMember(m)) ++added;
    }

    TC_LOG_INFO("playerbot.v2",
        "[BotGroupBuilder] Formed group: leader_low={} added={} size={} src=signal {} content_id={}",
        leader_low, added, g->GetMembersCount(), static_cast<uint32>(req.source), req.content_id);

    // D.4: stamp a pending-finish record so the next DrainPending pass
    // (10s later) fires the content-specific queue / walk.
    if (added > 0)
    {
        PendingFinish pf{};
        pf.leader_low = leader_low;
        pf.source     = req.source;
        pf.content_id = req.content_id;
        pf.level_min  = req.level_min;
        pf.level_max  = req.level_max;
        pf.arena_type = req.arena_type;
        pf.stamped_ms = GameTime::GetGameTimeMS();
        pending_finish_.push_back(pf);
    }
    else
    {
        // No member could join — don't leave a dangling 1-man group.
        g->Disband();
    }

    return added;
}

namespace {
// D.4 level-bracket → LFG dungeon_id mapping. Picks one classic
// dungeon entry per bracket — bots queue for that dungeon on
// DungeonNight. The IDs reference LFGDungeons.db2 entries; values
// chosen so a level-bracketed bot can actually queue (TC's LFG mgr
// rejects bots outside the bracket).
//   bracket           dungeon         lfg_dungeon_id
//   L15-L24           Ragefire Chasm  1
//   L17-L26           Wailing Caverns 2
//   L29-L38           Scarlet Mon.    8
//   L60+              random heroic   N (broad range)
uint32 PickDungeonForLevel(uint8 lvl)
{
    if (lvl >= 60) return 285;   // BfA Freehold heroic ~ filler; LFG mgr accepts cap-tier
    if (lvl >= 50) return 12;    // ZG-tier
    if (lvl >= 30) return 8;     // Scarlet Monastery (Library)
    if (lvl >= 20) return 2;     // Wailing Caverns
    return 1;                    // Ragefire Chasm (L15-L24)
}
} // anonymous

void BotGroupBuilder::DrainPending(uint32 now_ms)
{
    if (pending_finish_.empty()) return;

    // Iterate forward, replace with completed-mark indices, then erase.
    std::vector<size_t> to_erase;
    for (size_t i = 0; i < pending_finish_.size(); ++i)
    {
        PendingFinish& pf = pending_finish_[i];
        const uint32 age = now_ms - pf.stamped_ms;

        // Drop stale records (group never formed, leader disconnected,
        // etc.) without firing.
        if (age > kPendingFinishMaxAgeMs)
        {
            TC_LOG_INFO("playerbot.v2",
                "[BotGroupBuilder] DrainPending: leader_low={} aged out (age_ms={})",
                pf.leader_low, age);
            to_erase.push_back(i);
            continue;
        }

        // Need at least kPendingFinishGraceMs for invitees to accept.
        if (age < kPendingFinishGraceMs) continue;

        Player* leader = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(pf.leader_low));
        if (!leader || !leader->IsInWorld())
        {
            TC_LOG_INFO("playerbot.v2",
                "[BotGroupBuilder] DrainPending: leader_low={} not in world; dropping",
                pf.leader_low);
            to_erase.push_back(i);
            continue;
        }
        Group* g = leader->GetGroup();
        if (!g || g->GetMembersCount() < 2)
        {
            // Group didn't actually form (invites rejected / timed out).
            // Leave the record alive until kPendingFinishMaxAgeMs in case
            // the accept just hasn't propagated yet.
            continue;
        }

        // Fire content-specific finish based on source signal.
        if (pf.source == CoordSignal::GuildEventForming)
        {
            // content_id is the GuildEventKind (uint8 cast):
            //   2 = RaidNight, 3 = DungeonNight, 4 = BgNight.
            switch (pf.content_id)
            {
                case 3: // DungeonNight
                {
                    Intent it{};
                    it.bot_id = pf.leader_low;
                    LfgQueueIntent lfg{};
                    lfg.dungeon_or_bg_id = PickDungeonForLevel(pf.level_min);
                    lfg.role             = Role::Dps;  // leader queues as DPS by default
                    it.body = QueueIntent{lfg};
                    Services::Intents(pf.leader_low).push(std::move(it));
                    TC_LOG_INFO("playerbot.v2",
                        "[BotGroupBuilder] DrainPending: DungeonNight queue dungeon_id={} leader_low={}",
                        lfg.dungeon_or_bg_id, pf.leader_low);
                    break;
                }
                case 4: // BgNight — pick WSG (bg_type_id=2) as default
                {
                    Intent it{};
                    it.bot_id = pf.leader_low;
                    BgQueueIntent bg{};
                    bg.battlemaster = ObjectGuid::Empty;  // queue-from-anywhere
                    bg.bg_type_id   = 2;                  // WSG (BattlegroundTypeId)
                    it.body = QueueIntent{bg};
                    Services::Intents(pf.leader_low).push(std::move(it));
                    TC_LOG_INFO("playerbot.v2",
                        "[BotGroupBuilder] DrainPending: BgNight queue (WSG) leader_low={}",
                        pf.leader_low);
                    break;
                }
                case 2: // RaidNight — leader walks to a raid entrance
                {
                    // Pick a faction-appropriate iconic raid: ICC (mapId
                    // 631) for Alliance/Horde alike. sMapStore exposes
                    // the entrance via GetEntrancePos.
                    if (MapEntry const* me = sMapStore.LookupEntry(631))
                    {
                        int32 ent_map = 0; float ex = 0, ey = 0;
                        if (me->GetEntrancePos(ent_map, ex, ey) && ent_map >= 0)
                        {
                            float ez = leader->GetPositionZ();
                            // Update Z via Map::GetHeight is preferable
                            // but for D.4 the snap-to-ground in the bot's
                            // path planner handles the Z fix.
                            Intent it{};
                            it.bot_id = pf.leader_low;
                            MoveToIntent mv{};
                            mv.x = ex; mv.y = ey; mv.z = ez; mv.run = true;
                            it.body = mv;
                            Services::Intents(pf.leader_low).push(std::move(it));
                            TC_LOG_INFO("playerbot.v2",
                                "[BotGroupBuilder] DrainPending: RaidNight walk-to-ICC-entrance "
                                "leader_low={} ent_map={} pos=({:.1f},{:.1f})",
                                pf.leader_low, ent_map, ex, ey);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
        else if (pf.source == CoordSignal::BgTeamForming)
        {
            // Leader queues the formed premade for the specific BG that
            // triggered the formation (content_id = bg_type_id).
            Intent it{};
            it.bot_id = pf.leader_low;
            BgQueueIntent bg{};
            bg.battlemaster = ObjectGuid::Empty;
            bg.bg_type_id   = static_cast<uint16>(pf.content_id);
            it.body = QueueIntent{bg};
            Services::Intents(pf.leader_low).push(std::move(it));
            TC_LOG_INFO("playerbot.v2",
                "[BotGroupBuilder] DrainPending: BgTeamForming queue bg_type={} leader_low={}",
                pf.content_id, pf.leader_low);
        }
        else if (pf.source == CoordSignal::ArenaTeamForming)
        {
            // Leader queues the formed arena group for the skirmish.
            // content_id = arena BattlemasterList id; arena_type = team
            // size. api.bg_queue routes this through the Arena queue id and
            // rejects (Locked) if the group hasn't reached arena_type size,
            // so an undersized formation simply ages out without queuing.
            Intent it{};
            it.bot_id = pf.leader_low;
            BgQueueIntent bg{};
            bg.battlemaster = ObjectGuid::Empty;
            bg.bg_type_id   = static_cast<uint16>(pf.content_id);
            bg.arena_type   = pf.arena_type;
            it.body = QueueIntent{bg};
            Services::Intents(pf.leader_low).push(std::move(it));
            TC_LOG_INFO("playerbot.v2",
                "[BotGroupBuilder] DrainPending: ArenaTeamForming queue arena_bml={} arena_type={} leader_low={}",
                pf.content_id, uint32(pf.arena_type), pf.leader_low);
        }
        else if (pf.source == CoordSignal::LfgTankNeeded ||
                 pf.source == CoordSignal::LfgHealerNeeded ||
                 pf.source == CoordSignal::LfgDpsNeeded)
        {
            // Role-fill signals: leader queues the (small) formed group
            // for the requesting player's dungeon.
            Intent it{};
            it.bot_id = pf.leader_low;
            LfgQueueIntent lfg{};
            lfg.dungeon_or_bg_id = pf.content_id;
            lfg.role             = (pf.source == CoordSignal::LfgTankNeeded)   ? Role::Tank
                                 : (pf.source == CoordSignal::LfgHealerNeeded) ? Role::Healer
                                                                                : Role::Dps;
            it.body = QueueIntent{lfg};
            Services::Intents(pf.leader_low).push(std::move(it));
            TC_LOG_INFO("playerbot.v2",
                "[BotGroupBuilder] DrainPending: Lfg fill queue dungeon_id={} leader_low={}",
                pf.content_id, pf.leader_low);
        }
        // OwnerSquadAssemble / WorldBossSpotted / MPlusKeyForming:
        // group formation alone is sufficient finish for these — no
        // queue start needed (owner directs from there; world boss is
        // walked to manually; M+ needs a key insert which is its own
        // intent shape). Mark complete.

        to_erase.push_back(i);
    }

    if (!to_erase.empty())
    {
        // Erase back-to-front to keep indices stable.
        for (auto rit = to_erase.rbegin(); rit != to_erase.rend(); ++rit)
            pending_finish_.erase(pending_finish_.begin() + *rit);
    }
}

void BotGroupBuilder::OnGuildEventForming(CoordEvent const& ev)
{
    GroupRequest req{};
    req.source       = ev.kind;
    req.guild_id     = ev.origin_low;        // event publishes guild_id as origin
    req.content_id   = ev.content_id;        // event_kind as uint8
    req.level_min    = ev.level_min;
    req.level_max    = ev.level_max;
    req.faction_mask = ev.faction_mask;
    // Event-kind heuristic for required size:
    //   1 (TavernParty)  - no group needed, skip
    //   2 (RaidNight)    - 10
    //   3 (DungeonNight) - 5
    //   4 (BgNight)      - 5 (premade group; queue handles up-fill)
    switch (ev.content_id)
    {
        case 2:  req.required = 10; req.want_tank = 2; req.want_healer = 2; req.want_dps = 6; break;
        case 3:  req.required = 5;  req.want_tank = 1; req.want_healer = 1; req.want_dps = 3; break;
        case 4:  req.required = 5;  break;
        default: return;
    }
    BuildGroup(req);
}

void BotGroupBuilder::OnLfgRoleNeeded(CoordEvent const& ev)
{
    // LFG single-slot fill. Required = 1, role inferred from signal.
    GroupRequest req{};
    req.source       = ev.kind;
    req.required     = 1;
    req.faction_mask = ev.faction_mask;
    req.level_min    = ev.level_min;
    req.level_max    = ev.level_max;
    req.content_id   = ev.content_id;
    switch (ev.kind)
    {
        case CoordSignal::LfgTankNeeded:   req.want_tank = 1; break;
        case CoordSignal::LfgHealerNeeded: req.want_healer = 1; break;
        case CoordSignal::LfgDpsNeeded:    req.want_dps = 1; break;
        default: return;
    }
    BuildGroup(req);
}

void BotGroupBuilder::OnBgTeamForming(CoordEvent const& ev)
{
    // BG queue fill: 5-bot premade for cohesion; the BG itself will
    // be up-filled by the queue server-side. content_id = bg_type_id.
    GroupRequest req{};
    req.source       = ev.kind;
    req.required     = 5;
    req.faction_mask = ev.faction_mask;
    req.level_min    = ev.level_min;
    req.level_max    = ev.level_max;
    req.content_id   = ev.content_id;
    BuildGroup(req);
    // Post-form: queue the group for BG via the leader's intent queue.
    // Deferred to D.4 — needs BgQueueIntent integration with group
    // (existing intent queues solo only). Today the formed group
    // simply exists; players see them clustered + advertising via
    // the guild_event:staging_chat rule.
}

void BotGroupBuilder::OnArenaTeamForming(CoordEvent const& ev)
{
    // Arena skirmish team formation. content_id = arena BattlemasterList
    // id (4=Nagrand / 6=AllArenas / 8=Ruins); arena_type = team size
    // (2/3/5). Form EXACTLY arena_type bots into a group; the leader
    // queues the group for the arena skirmish in DrainPending (~10s later,
    // once the invitees have accepted). One CoordEvent is published per
    // faction by SeedArenaMatches, so each call here builds one side.
    if (ev.arena_type != 2 && ev.arena_type != 3 && ev.arena_type != 5)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotGroupBuilder] OnArenaTeamForming: bad arena_type={} content_id={}; skipping",
            uint32(ev.arena_type), ev.content_id);
        return;
    }
    GroupRequest req{};
    req.source       = ev.kind;
    req.required     = ev.arena_type;     // 2v2 → 2 bots, etc.
    req.faction_mask = ev.faction_mask;
    req.level_min    = ev.level_min;
    req.level_max    = ev.level_max;
    req.content_id   = ev.content_id;     // arena BattlemasterList id
    req.arena_type   = ev.arena_type;
    BuildGroup(req);
}

void BotGroupBuilder::OnOwnerSquadAssemble(CoordEvent const& ev)
{
    // Owner squad assemble. origin_low is the owner player guid_low.
    // Pick all owned bots within the owner's range; faction_mask is
    // the owner's faction; no level filter.
    GroupRequest req{};
    req.source       = ev.kind;
    req.required     = 4;        // a player + 4 bots = full party
    req.faction_mask = ev.faction_mask;
    BuildGroup(req);
}

void BotGroupBuilder::OnWorldBossSpotted(CoordEvent const& ev)
{
    // World-boss response: grab nearby max-level bots. Publisher is
    // stubbed (no world-boss detection exists yet); this handler is
    // wired so the day the publisher lands, response is automatic.
    GroupRequest req{};
    req.source       = ev.kind;
    req.required     = 20;       // raid-sized group for a world boss
    req.faction_mask = ev.faction_mask;
    req.level_min    = 60;       // max-level only — placeholder
    req.content_id   = ev.content_id;
    BuildGroup(req);
}

void BotGroupBuilder::OnMPlusKeyForming(CoordEvent const& ev)
{
    // M+ keystone: 5-man with strict role mix. Publisher is stubbed
    // (M+ scheduling not implemented); handler ready.
    GroupRequest req{};
    req.source       = ev.kind;
    req.required     = 5;
    req.want_tank    = 1;
    req.want_healer  = 1;
    req.want_dps     = 3;
    req.faction_mask = ev.faction_mask;
    req.level_min    = 60;
    req.content_id   = ev.content_id;
    BuildGroup(req);
}

} // namespace Playerbot::V2

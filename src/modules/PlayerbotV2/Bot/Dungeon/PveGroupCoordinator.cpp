// PveGroupCoordinator implementation. See header for the design contract.

#include "PveGroupCoordinator.h"
#include "DungeonScript.h"
#include "../BotRegistry.h"
#include "../BotSnapshotView.h"
#include "../ClassTables.h"
#include "../../Group/GroupSnapshot.h"
#include "../../Fleet/BotIdentityRegistry.h"
#include "../../Services.h"
#include "../../Threading/SnapshotPublisher.h"

#include "Config.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"

#include <algorithm>
#include <sstream>

namespace Playerbot {

namespace {

// Plan cadence. PvE duty assignments (tank roles, interrupt ranks, healer
// focus) are stable for whole pulls; the only fast-moving output is the
// synchronized kill target, and a 500ms refresh after an add dies is well
// inside human reaction time.
constexpr uint32 kPlanIntervalMs = 500;

} // namespace

void PveGroupCoordinator::Update(uint32 now_ms)
{
    if (now_ms - last_plan_ms_ < kPlanIntervalMs)
        return;
    last_plan_ms_ = now_ms;

    if (!sConfigMgr->GetBoolDefault("Playerbot.Pve.Coordinator.Enable", true))
    {
        if (!orders_.empty()) orders_.clear();
        last_dump_ = "coordinator disabled (Playerbot.Pve.Coordinator.Enable=0)";
        return;
    }

    // -- Bucket every grouped, instanced bot by (group, map) -----------------
    // The map is part of the key: a group can momentarily span TWO dungeon
    // maps (laggards still zoning out of the previous instance). One plan
    // per (group, map) keeps every duty holder physically able to act —
    // the review found a single per-group plan could hand main-tank /
    // soaker / rank-0 duties to members in a different instance.
    struct Bucket
    {
        uint64 group_low = 0;
        uint32 map_id = 0;
        std::vector<std::pair<uint64, Player*>> bots;
    };
    std::unordered_map<uint64, Bucket> buckets;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
                        ObjectGuid::Create<HighGuid::Player>(id));
        if (!p) return;
        Group const* grp = p->GetGroup();
        if (!grp) return;
        // Dungeons AND raids (Map::IsDungeon covers both); the open world
        // keeps the legacy behavior — coordination there is the BG
        // coordinator's job (battlegrounds) or unnecessary (quest mobs).
        Map* m = p->GetMap();
        if (!m || !m->IsDungeon()) return;
        const uint64 glow = grp->GetGUID().GetCounter();
        const uint64 key  = (glow << 16) ^ uint64(m->GetId());
        Bucket& b = buckets[key];
        b.group_low = glow;
        b.map_id    = m->GetId();
        b.bots.emplace_back(uint64(id), p);
    });

    next_orders_.clear();
    std::ostringstream dump;

    for (auto& [bkey, b] : buckets)
    {
        // Shared group snapshot: one per group per tick, covers EVERY
        // member including humans (roles, positions, vitals).
        std::shared_ptr<GroupSnapshot const> gs =
            Services::Snapshots().latest_group(b.bots.front().first);
        if (!gs || gs->members.empty())
            continue;   // group data cold — legacy runs

        // Reference bot snapshot: freshest member view; provides
        // nearby_enemies for the synchronized kill target and the view
        // GetAdvice reads. Same world-thread-computed-advice pattern as
        // the BG coordinator — never read a BotAI's advice cache here.
        std::shared_ptr<BotSnapshot const> ref;
        for (auto const& [blow, p] : b.bots)
        {
            auto s = Services::Snapshots().latest(blow);
            if (s && (!ref || s->version > ref->version))
                ref = s;
        }
        if (!ref)
            continue;
        DungeonAdvice const advice =
            Services::Dungeons().GetAdvice(BotSnapshotView(*ref));

        // -- Census ----------------------------------------------------------
        // Humans first, then bots, each tier ordered by guid: a human tank
        // claims main-tank duty, a human healer claims the tank-heal slot,
        // and duty assignment stays deterministic across plans.
        //
        // Two presence semantics from the review:
        //  * MAP SCOPE: only members physically on the bucket's instance
        //    map hold duties. A tank corpse-running outside the portal
        //    must not keep main-tank (which gated the inside tank out of
        //    every pull until they zoned back in).
        //  * TRUE DEATH STATE: m.is_alive (Unit::IsAlive), NOT hp > 0 —
        //    a released ghost has hp == 1 and would otherwise keep its
        //    duty for the whole graveyard run (ghost rank-0 kicker =
        //    nobody interrupts; ghost soakers = circles unsoaked).
        std::vector<Member> mem;
        mem.reserve(gs->members.size());
        auto& lifecycle = Services::Lifecycle();
        for (auto const& m : gs->members)
        {
            if (!m.online) continue;
            if (m.map_id != b.map_id) continue;
            Member e;
            e.guid_low = m.guid.GetCounter();
            e.is_bot   = lifecycle.is_bot(e.guid_low);
            e.alive    = m.is_alive;
            e.cls      = m.cls;
            e.spec     = uint16(m.spec);
            e.tank     = m.role == Role::Tank   || IsTankSpec(m.cls, e.spec);
            e.healer   = m.role == Role::Healer || IsHealerSpec(m.cls, e.spec);
            e.interrupter = !e.healer && ClassInterrupt(m.cls, m.spec) != 0;
            mem.push_back(e);
        }
        std::sort(mem.begin(), mem.end(), [](Member const& a, Member const& c)
        {
            if (a.is_bot != c.is_bot) return !a.is_bot;   // humans first
            return a.guid_low < c.guid_low;
        });
        if (mem.empty())
            continue;

        // -- Duty assignment ---------------------------------------------------
        // Start every BOT member with an active, otherwise-empty order:
        // `active` alone already suppresses the legacy herd paths that
        // need an explicit assignment to act (soak stays governed by the
        // `soaker` field below).
        std::unordered_map<uint64, PveOrder> plan;
        for (auto const& e : mem)
            if (e.is_bot)
            {
                PveOrder o;
                o.active = true;
                plan[e.guid_low] = o;
            }
        auto bot_order = [&](uint64 g) -> PveOrder*
        {
            auto it = plan.find(g);
            return it == plan.end() ? nullptr : &it->second;
        };

        // Tanks: first (human-preferred) = main. The OFF-tank duty — a
        // dedicated second tank that shadows the main and taunts on swap
        // triggers — is a RAID concept ONLY. A classic 5-man runs
        // 1 tank / 1 healer / 3 DPS: a second tank-spec member there is
        // just a DPS, NOT a bodyguard — they keep tank_duty=0 and run
        // legacy behavior (the pull gates still keep them from starting
        // pulls, because they aren't the designated main tank).
        // Deliberately NOT keyed on advice.tank_swap_on_spells: the
        // MPlusAffix script merges Necrotic (209858) into EVERY dungeon's
        // advice unconditionally, so list-presence is "always" — and a
        // party tank-swap emergency is already handled by the legacy
        // taunt-swap rule without a standing off-tank designation.
        const bool wants_off_tank = gs->is_raid;
        uint64 main_tank = 0, off_tank = 0;
        for (auto const& e : mem)
        {
            if (!e.tank || !e.alive) continue;
            if (!main_tank) { main_tank = e.guid_low; continue; }
            if (!off_tank && wants_off_tank)
            { off_tank = e.guid_low; continue; }
        }
        for (auto const& e : mem)
        {
            if (!e.tank) continue;
            if (PveOrder* o = bot_order(e.guid_low))
            {
                if (e.guid_low == main_tank)     o->tank_duty = 1;
                else if (e.guid_low == off_tank) o->tank_duty = 2;
            }
        }

        // Healers: the first healer is the tank healer (focus = main
        // tank); every further healer raid-triages. A human first healer
        // simply leaves all bot healers on triage.
        bool tank_heal_taken = false;
        for (auto const& e : mem)
        {
            if (!e.healer || !e.alive) continue;
            const bool take_focus = !tank_heal_taken && main_tank != 0;
            tank_heal_taken = tank_heal_taken || take_focus;
            if (PveOrder* o = bot_order(e.guid_low))
                if (take_focus)
                    o->heal_focus =
                        ObjectGuid::Create<HighGuid::Player>(main_tank);
        }

        // Interrupt rotation: capable non-healer bots ranked 0..N-1 in
        // census order (rank 0 kicks on sight, rank 1 backs up, 2+ hold
        // for the NEXT cast — preserving kicks instead of dumping every
        // cooldown on one cast). Human kickers can't be scheduled, so
        // they are simply not part of the rotation.
        uint8 irank = 0;
        for (auto const& e : mem)
        {
            if (!e.interrupter || !e.alive || !e.is_bot) continue;
            if (PveOrder* o = bot_order(e.guid_low))
                o->interrupt_rank = irank < 0xFF ? irank++ : 0xFF;
        }

        // Soak duty: assigned UNCONDITIONALLY every plan (the fields cost
        // nothing and the consumer only acts when the live advice lists
        // soak mechanics — assigning here regardless removes any plan-
        // time/consume-time advice disagreement window). Two designated
        // DPS soakers (more bodies add nothing and feed the AoE);
        // everyone else explicitly stays out — the legacy rule walked the
        // ENTIRE group into the circle. Tanks and healers never soak.
        // Living human DPS occupy soaker slots first (census order puts
        // them ahead): a human already standing in for the mechanic means
        // fewer bots need drafting.
        {
            int soakers = 0;
            for (auto const& e : mem)
            {
                const bool dps = !e.tank && !e.healer;
                if (!e.is_bot)
                {
                    if (dps && e.alive && soakers < 2) ++soakers;
                    continue;
                }
                PveOrder* o = bot_order(e.guid_low);
                if (!o) continue;
                if (dps && e.alive && soakers < 2) { o->soaker = 1; ++soakers; }
                else                               { o->soaker = 2; }
            }
        }

        // Spread slots: stable per-member bearing indices so spread
        // mechanics fan the group out instead of every bot stepping away
        // from its (mutually nearest) neighbour in lockstep.
        uint8 slot = 0;
        for (auto const& e : mem)
            if (PveOrder* o = bot_order(e.guid_low))
                o->spread_slot = slot < 0xFF ? slot++ : 0xFF;
            else
                ++slot;   // humans consume a bearing too

        // Synchronized kill target: ONE live priority add, every bot DPS
        // burns it together. Scan the script's priority list in order
        // (front = top priority) against the reference snapshot's live
        // enemies. Sticky: keep the previous focus while it is still a
        // live candidate so the focus doesn't flap between two adds of
        // the same entry.
        ObjectGuid kill_focus;
        if (!advice.high_priority_kill_entries.empty())
        {
            // Stickiness is PER-GROUP coordinator state, not a read-back
            // from a member's previous order (members leave groups; a
            // single cold-data tick would silently drop the focus and
            // re-roll it next plan).
            ObjectGuid const prev_focus = [&]() -> ObjectGuid
            {
                auto it = last_kill_focus_.find(bkey);
                return it != last_kill_focus_.end() ? it->second
                                                    : ObjectGuid::Empty;
            }();
            // Candidate guards (review): skip adds CC-locked by a group
            // member (burning them breaks the CC — same rule the per-bot
            // target pickers follow) and adds that are not engaged with
            // anyone (ordering DPS onto an unpulled pack is a coordinated
            // BAD pull; the tank rules own first contact).
            auto cc_by_group = [&](NearbyUnit const& u) -> bool
            {
                if (!u.is_cc_locked || u.cc_caster.IsEmpty()) return false;
                const uint64 caster_low = u.cc_caster.GetCounter();
                for (auto const& e : mem)
                    if (e.guid_low == caster_low) return true;
                return false;
            };
            for (uint32 want : advice.high_priority_kill_entries)
            {
                for (auto const& u : ref->combat.nearby_enemies)
                {
                    if (u.hp <= 0 || u.entry != want) continue;
                    if (cc_by_group(u)) continue;
                    if (u.victim.IsEmpty()) continue;   // unpulled — leave it
                    if (kill_focus.IsEmpty()) kill_focus = u.guid;
                    if (u.guid == prev_focus) { kill_focus = prev_focus; break; }
                }
                if (!kill_focus.IsEmpty())
                    break;   // highest-priority entry with a live mob wins
            }
        }
        if (kill_focus.IsEmpty())
            last_kill_focus_.erase(bkey);
        else
        {
            last_kill_focus_[bkey] = kill_focus;
            for (auto const& e : mem)
            {
                if (e.tank || e.healer) continue;   // tanks hold aggro, healers heal
                if (PveOrder* o = bot_order(e.guid_low))
                    o->kill_focus = kill_focus;
            }
        }

        // Publish the main tank's guid to every ordered bot — the
        // off-tank's between-pull follow consumer and future assist
        // logic key off it.
        if (main_tank != 0)
        {
            ObjectGuid const mt_guid =
                ObjectGuid::Create<HighGuid::Player>(main_tank);
            for (auto& [pg_low, po] : plan)
                po.main_tank = mt_guid;
        }

        // -- Publish + diagnostics --------------------------------------------
        int n_bots = 0, n_soak = 0, n_kick = 0;
        uint64 sig = 1469598103934665603ull;
        auto mix = [&sig](uint64 v) { sig ^= v; sig *= 1099511628211ull; };
        for (auto& [bg_low, o] : plan)
        {
            next_orders_[bg_low] = o;
            ++n_bots;
            if (o.soaker == 1) ++n_soak;
            if (o.interrupt_rank != 0xFF) ++n_kick;
            mix(bg_low);
            mix(uint64(o.tank_duty) | (uint64(o.interrupt_rank) << 8) |
                (uint64(o.soaker) << 16));
            mix(o.heal_focus.GetCounter());
            mix(o.kill_focus.GetCounter());
        }
        auto sig_it = plan_sig_.find(bkey);
        if (sig_it == plan_sig_.end() || sig_it->second != sig)
        {
            plan_sig_[bkey] = sig;
            TC_LOG_INFO("playerbot.v2",
                "[pvecoord] group={} map={} plan changed: members={} bots={} "
                "main_tank={} off_tank={} kickers={} soakers={} kill_focus={}",
                b.group_low, b.map_id, uint32(mem.size()), n_bots, main_tank,
                off_tank, n_kick, n_soak, kill_focus.GetCounter());
        }

        dump << "group=" << b.group_low << " map=" << b.map_id
             << " members=" << mem.size()
             << " bots=" << n_bots
             << " main_tank=" << main_tank << " off_tank=" << off_tank
             << " kickers=" << n_kick << " soakers=" << n_soak
             << " kill_focus=" << kill_focus.GetCounter() << "\n";
    }

    orders_ = std::move(next_orders_);
    next_orders_.clear();
    for (auto it = plan_sig_.begin(); it != plan_sig_.end();)
        it = buckets.count(it->first) ? std::next(it) : plan_sig_.erase(it);
    for (auto it = last_kill_focus_.begin(); it != last_kill_focus_.end();)
        it = buckets.count(it->first) ? std::next(it)
                                      : last_kill_focus_.erase(it);
    last_dump_ = dump.str();
    if (last_dump_.empty())
        last_dump_ = "no coordinated dungeon/raid groups";
}

std::string PveGroupCoordinator::DebugDump() const
{
    return last_dump_.empty() ? std::string("coordinator has not planned yet")
                              : last_dump_;
}

} // namespace Playerbot

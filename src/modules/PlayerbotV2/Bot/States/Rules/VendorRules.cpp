// VendorRules - Pilot subsystem for Refactor #3.
//
// Hosts the vendor_visit FSM previously inline in State_Idle::OnTick.
// Also hosts idle:walk_to_known_vendor: when the bot's bag is full or
// durability is low AND no vendor is in snapshot scan range, walks
// toward the closest WorldMetadataKind::Vendor annotation within 600y.
// Closes the gap where the existing idle:vendor_visit fsm only fires
// at 5y interact range, and idle:travel_to_vendor (in State_Idle)
// only fires at 80y scan range.
// Registered with the IdleRuleRegistry via Services::IdleRules() at init.
// One rule per FSM (not per phase) — the FSM walks repair → sell → bag →
// food → bandage → reagent on successive ticks while the bot is parked
// at a single vendor. set_last_rule_fired carries the per-phase tag so
// /diag and observability remain backward-compatible.
//
// Behavior identical to the legacy inline block at State_Idle.cpp ≈4690.
// See ITEM_VENDOR_SYSTEM_PLAN.md §"Level 0" / Phase 2 for the rationale.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Bot/States/MaintainHelpers.h"   // ChunkedWalkToward (shared chunked walk)
#include "Bot/BagSizeTable.h"
#include "Bot/World/CapitalsTable.h"   // CapitalForRace — capital run routing
#include "Bot/RecipeDifficulty.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitDefines.h"   // UNIT_NPC_FLAG_VENDOR / _REPAIR

#include "World/WorldMetadata.h"
#include "Services.h"
#include "Travel/RepairVendorIndex.h"
#include "Travel/QuestHubDatabase.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Playerbot {

namespace States {

// Shared chunked walk toward a same-map point with a position-stall escape
// (ledge/corner). See MaintainHelpers.h for the rationale. Used by the capital /
// repair / vendor / far-travel walks instead of a single long emit.move_to().
bool ChunkedWalkToward(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                       float tx, float ty, float tz, char const* tag, uint32 now_ms)
{
    float bx, by, bz; s.position(bx, by, bz);
    // Arrival is judged against the REAL goal (before any blackspot deflection).
    {
        const float adx = tx - bx, ady = ty - by;
        if (adx*adx + ady*ady <= 9.0f * 9.0f) { ai.walk_stall_reset(); return false; }
    }
    // Death-blackspot avoidance (owner idea 2026-06-22): if a known death zone lies
    // across the straight path to the goal, steer toward a waypoint that skirts its
    // edge instead of walking back into it. Same destination — just another way; the
    // chunk loop below then advances along the deflected bearing.
    {
        float ex, ey;
        if (ai.deflect_for_blackspot(bx, by, tx, ty, now_ms, ex, ey)) { tx = ex; ty = ey; }
    }
    const float dx = tx - bx, dy = ty - by;
    const float dsq = dx*dx + dy*dy;

    const uint8 strikes = ai.walk_stall_note(bx, by, now_ms);
    // Give up this episode after ~6 no-progress windows (~24s) despite escalating
    // escapes — yield so stuck-recovery / the watchdog / lower rules can act
    // instead of grinding the same wall/ledge forever.
    if (strikes >= 6) { ai.walk_stall_reset(); return false; }

    const float dist = std::sqrt(dsq);
    const float kStep =
        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f : 35.0f;

    // Near + not stalled + not blocked: emit the FULL waypoint so PathGenerator
    // navmesh-routes the last leg (curving around canals/walls/through doors)
    // rather than a straight chunk that can climb a ledge.
    if (dist <= 60.0f && strikes == 0 && s.path_blocked_count() == 0)
    {
        emit.move_to(tx, ty, tz, /*run*/ true);
        ai.set_last_rule_fired(tag);
        return true;
    }

    // Bearing deflection. On a position stall, escalate the angle with the strike
    // count (alternating sign), up to ~150deg (near-reverse) to back OFF a ledge/
    // corner; otherwise the mild path_blocked deflection.
    float ang = 0.f;
    if (strikes >= 1)
        ang = (strikes % 2 == 1 ? 1.0f : -1.0f) * std::min(0.7f * float(strikes), 2.6f);
    else if (s.path_blocked_count() > 0)
        ang = (s.path_blocked_count() % 2 == 1 ? 0.7f : -0.7f);

    float fdx = dx, fdy = dy;
    if (ang != 0.f)
    {
        const float c = std::cos(ang), sn = std::sin(ang);
        fdx = dx * c - dy * sn;
        fdy = dx * sn + dy * c;
    }
    const float scale = kStep / dist;
    emit.move_to(bx + fdx * scale, by + fdy * scale, bz, /*run*/ true);
    ai.set_last_rule_fired(tag);
    return true;
}

} // namespace States

namespace {

bool VendorVisitGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32 now_ms)
{
    // No vendors live inside an instance — a repair/sell trip can only route the
    // bot out of the dungeon/BG. Suppress so the dungeon dispatch keeps the bot
    // on the run (it vendors on exit). Matches bags_full_recover's dungeon gate.
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    // Defer to quest actions — questgivers may share interact range with
    // vendors; quest pickups have higher value than a repair tick.
    if (!s.raw().quest_discovery.quest_turnins.empty() || !s.raw().quest_discovery.quest_offers.empty())
        return false;

    const uint8 vv_mask = s.vendor_visit_phases_pending();

    // Quest-first (2026-06-16): yield to a reachable quest action — UNLESS the
    // bot is progress-blocked (low durability bit 0 or bag full bit 1), in
    // which case it must still vendor or it can't keep questing.
    {
        constexpr uint8 kVvDuraLow = 1u << 0;
        constexpr uint8 kVvBagFull = 1u << 1;
        if ((vv_mask & (kVvDuraLow | kVvBagFull)) == 0 && s.has_actionable_quest())
            return false;
    }
    const int   vv_bits = std::popcount(vv_mask);
    const bool  vv_has_reagent_need =
        !s.raw().spellbook.known_recipes.empty() && s.gold() >= 10000;

    // A single PROGRESS-BLOCKING need is enough to justify a repair/sell tick
    // when the bot is already parked at a vendor: low durability (0x01),
    // repair-soon (0x20), or bag-full (0x02). The old `vv_bits >= 2` test
    // silently refused durability-only repairs — a 0%-durability bot standing
    // at a repair NPC would walk away unrepaired. The FSM is only reachable
    // with a vendor already in interact range (below), so this never causes a
    // dedicated trip on its own.
    constexpr uint8 kVvBlocking = 0x01 | 0x02 | 0x20;
    if (!((vv_mask & kVvBlocking) != 0 || vv_bits >= 2 ||
          (vv_mask != 0 && vv_has_reagent_need)))
        return false;
    if (ai.vendor_visit_in_lockout(now_ms))
        return false;

    // Need a vendor in the snapshot to consider firing — fire() does the
    // interact-range check and the walked-out-of-range cleanup.
    NearbyUnit const* vv_npc =
        s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR);
    if (!vv_npc) vv_npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR);
    if (!vv_npc)
    {
        // No vendor in range — drop any pending hesitation so a fresh
        // approach to a different vendor gets a clean window.
        if (ai.pending_vendor_visit_at_ms() != 0)
            ai.set_pending_vendor_visit_at_ms(0);
        return false;
    }
    // Done-lockout: the FSM finished a full pass at this vendor moments
    // ago — don't re-arm yet. BotAI has carried this lockout since the
    // FSM was built, but the gate never consulted it, so a bot parked at
    // a vendor (resting at an innkeeper) re-ran the whole FSM the moment
    // each phase throttle expired (observed: Uraimus cycling
    // bag→bandage→done forever at the Dolanaar inn, 2026-06-11).
    if (ai.vendor_visit_in_lockout(now_ms))
        return false;

    // Open-the-window hesitation: 1000–3500ms before the FSM fires its
    // first phase. Mid-FSM phases are NOT gated (phase 1 already fired
    // means the bot opened the window already; subsequent phases pace
    // naturally on the action_recently_tried throttle). Only the first
    // visit to a vendor hesitates.
    if (ai.vendor_visit_phase() == 0 || ai.vendor_visit_phase() == 0xFF)
    {
        uint32 ready_at = ai.pending_vendor_visit_at_ms();
        if (ready_at == 0)
        {
            const uint32 jitter = 1000u + (uint32(s.bot_id()) * 2654435761u) % 2500u;
            ai.set_pending_vendor_visit_at_ms(now_ms + jitter);
            return false;
        }
        if (now_ms < ready_at) return false;
    }
    return true;
}

bool VendorVisitFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    const Role effective_role = ai.effective_role(s);

    NearbyUnit const* vv_npc =
        s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR);
    if (!vv_npc) vv_npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR);
    if (!vv_npc) return false;

    float bx, by, bz; s.position(bx, by, bz);
    const float vdx = vv_npc->x - bx, vdy = vv_npc->y - by, vdz = vv_npc->z - bz;
    constexpr float kInteract = 5.0f;
    if ((vdx*vdx + vdy*vdy + vdz*vdz) > kInteract * kInteract)
    {
        // Walked out of interact range mid-visit — clear so a fresh
        // approach (to any vendor) re-starts the FSM. Fire returns
        // false so lower-priority rules can run this tick.
        if (ai.vendor_visit_phase() != 0 && ai.vendor_visit_phase() != 0xFF)
            ai.vendor_visit_clear();
        return false;
    }

    const uint8  vv_mask    = s.vendor_visit_phases_pending();
    const uint64 vv_npc_low = vv_npc->guid.GetCounter();
    // A real player does NOT restock consumables/reagents while bags are full —
    // you sell (and bank/upgrade bags) first, then buy. Buying food/bandages/
    // reagents into ≤2 free slots just re-fills the bags the bot is trying to
    // clear, creating a sell→rebuy→"bags full"→sell loop that parks the bot at
    // the vendor forever (observed: Grokmar, tiny 6-slot bags, never leaving to
    // quest). Suppress the additive buy phases while genuinely full; selling and
    // the bag-UPGRADE buy (which adds capacity) still run.
    const bool vv_bag_pressured = s.raw().bags.bag_free_slots <= 2;

    // FSM ownership: if a different NPC than the one we started this visit
    // at, reset the phase counter.
    if (ai.vendor_visit_phase() != 0 &&
        ai.vendor_visit_npc_low() != vv_npc_low)
        ai.set_vendor_visit_phase(0, 0, now_ms);

    // ---- Phase 1: repair ----
    // Fires on the CRITICAL bit (0x01, <30%) OR the proactive-soon bit (0x20,
    // <35%) — once the bot is parked at a repair NPC there's no reason to wait
    // for gear to hit the critical band; a human repairs the moment they're at
    // the vendor. (The dedicated-trip gating that keeps <35% from ABANDONING
    // quests lives in the routing rules, not here at the interact ring.)
    constexpr uint8 kVvRepairBits = 0x01 | 0x20;
    if ((vv_mask & kVvRepairBits) &&
        (vv_npc->npc_flags & UNIT_NPC_FLAG_REPAIR) != 0 &&
        !ai.action_recently_tried(BotAI::ActionKind::Repair, vv_npc_low, now_ms))
    {
        emit.emit(VendorIntent{RepairAllIntent{vv_npc->guid, /*from_guild_bank*/ false}});
        ai.note_action_retry(BotAI::ActionKind::Repair, vv_npc_low, now_ms);
        ai.set_vendor_visit_phase(1, vv_npc_low, now_ms);
        ai.set_last_rule_fired("idle:vendor_visit:repair");
        return true;
    }

    // ---- Phase 2: sell trash ----
    if ((vv_mask & 0x02) &&
        !ai.action_recently_tried(BotAI::ActionKind::SellTrash, vv_npc_low, now_ms))
    {
        emit.emit(VendorIntent{VendorSellTrashIntent{vv_npc->guid}});
        ai.note_action_retry(BotAI::ActionKind::SellTrash, vv_npc_low, now_ms);
        ai.set_vendor_visit_phase(2, vv_npc_low, now_ms);
        ai.set_last_rule_fired("idle:vendor_visit:sell");
        return true;
    }

    // ---- Phase 3: bag upgrade ----
    if (vv_mask & 0x04)
    {
        // 30 min per NPC (was 60s): most vendors don't SELL bags, so the
        // buy fails, the need-bit stays set, and the old 60s window
        // re-attempted the doomed purchase every minute for as long as
        // the bot stood there. After a SUCCESSFUL buy the need-bit
        // clears via the snapshot anyway, so the long window only
        // throttles failures.
        constexpr uint32 kBagBuyCdMs = 30u * 60u * 1000u;
        const bool same_npc_recent =
            ai.last_bag_buy_npc() == vv_npc->guid &&
            (now_ms - ai.last_bag_buy_ms()) < kBagBuyCdMs;
        if (!same_npc_recent)
        {
            uint32 best_entry = 0;
            const int32 gold = s.gold();
            const uint8 cur_smallest = s.smallest_bag_capacity();
            const bool has_empty = s.has_empty_bag_slot();
            for (auto it = kBagSizeTable.rbegin(); it != kBagSizeTable.rend(); ++it)
            {
                if (uint64(gold) < uint64(it->approx_price) * 6 / 5) continue;
                if (!has_empty && it->capacity <= cur_smallest) continue;
                best_entry = it->item_entry;
                break;
            }
            if (best_entry != 0)
            {
                emit.emit(VendorIntent{VendorBuyByEntryIntent{vv_npc->guid, best_entry, /*count*/ 1}});
                ai.note_bag_buy_try(vv_npc->guid, now_ms);
                ai.set_vendor_visit_phase(3, vv_npc_low, now_ms);
                ai.set_last_rule_fired("idle:vendor_visit:bag");
                return true;
            }
        }
    }

    // ---- Phase 4: food/drink ----
    // Gated on the 30-min food-buy cooldown: real players loot food, they don't
    // re-stock every vendor visit. Without this the bot piled up 9 stacks that
    // filled its bags and drove a perpetual sell-vendor oscillation.
    if ((vv_mask & 0x08) && !vv_bag_pressured && ai.food_buy_off_cooldown(now_ms) &&
        !ai.vendor_buy_recently_tried(vv_npc_low, /*FOOD_DRINK*/ 5, now_ms))
    {
        emit.vendor_buy_category(vv_npc->guid, /*CONSUMABLE*/ 0, /*FOOD_DRINK*/ 5, 20);
        ai.note_vendor_buy_try(vv_npc_low, 5, now_ms);
        ai.note_food_buy(now_ms);
        ai.set_vendor_visit_phase(4, vv_npc_low, now_ms);
        ai.set_last_rule_fired("idle:vendor_visit:food");
        return true;
    }

    // ---- Phase 5: bandage ----
    if ((vv_mask & 0x10) && !vv_bag_pressured && effective_role != Role::Healer &&
        !ai.vendor_buy_recently_tried(vv_npc_low, /*BANDAGE*/ 7, now_ms))
    {
        emit.vendor_buy_category(vv_npc->guid, 0, 7, 10);
        ai.note_vendor_buy_try(vv_npc_low, 7, now_ms);
        ai.set_vendor_visit_phase(5, vv_npc_low, now_ms);
        ai.set_last_rule_fired("idle:vendor_visit:bandage");
        return true;
    }

    // ---- Phase 6: profession reagents ----
    const bool vv_has_reagent_need =
        !s.raw().spellbook.known_recipes.empty() && s.gold() >= 10000;
    if (vv_has_reagent_need && !vv_bag_pressured)
    {
        for (uint32 spell_id : s.raw().spellbook.known_recipes)
        {
            RecipeMeta const* meta = FindRecipeMeta(spell_id);
            if (!meta) continue;
            if (s.is_skill_capped(meta->skill_line_id)) continue;
            const RecipeColor c = ResolveRecipeColor(spell_id,
                s.skill_value(meta->skill_line_id));
            if (c == RecipeColor::Gray || c == RecipeColor::Unknown) continue;
            SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
            if (!si) continue;
            for (size_t i = 0; i < si->Reagent.size(); ++i)
            {
                const int32 entry = si->Reagent[i];
                const int16 need  = si->ReagentCount[i];
                if (entry <= 0 || need <= 0) continue;
                if (s.item_count(uint32(entry)) >= uint32(need)) continue;
                if (ai.action_recently_tried(BotAI::ActionKind::ReagentBuy,
                                              uint64(entry), now_ms))
                    continue;
                emit.vendor_buy_by_entry(vv_npc->guid, uint32(entry), 5);
                ai.note_action_retry(BotAI::ActionKind::ReagentBuy,
                                     uint64(entry), now_ms);
                ai.set_vendor_visit_phase(6, vv_npc_low, now_ms);
                ai.set_last_rule_fired("idle:vendor_visit:reagent");
                return true;
            }
        }
    }

    // All phases either cleared or on cooldown — mark done and apply
    // lockout so the FSM doesn't re-arm next tick while the bot is still
    // standing at the same NPC. Return false so lower-priority rules can
    // still run this tick (matches the legacy "no return — fall through"
    // semantics).
    if (ai.vendor_visit_phase() != 0 && ai.vendor_visit_phase() != 0xFF)
    {
        ai.set_vendor_visit_phase(0xFF, vv_npc_low, now_ms);
        ai.set_last_rule_fired("idle:vendor_visit:done");
    }
    return false;
}

// ---------- idle:walk_to_known_vendor ----------
//
// Bot with bag-full / low-durability gates and no vendor in snapshot
// 80y scan → fall back to operator-curated Vendor annotation within
// 600y. Once within 80y the existing idle:travel_to_vendor + then
// idle:vendor_visit take over the close approach.
bool WalkToKnownVendorGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32 now_ms)
{
    if (s.in_combat() || s.is_casting() || s.raw().movement.is_mounted)
        return false;
    // Never walk off the dungeon route toward a vendor. An outside vendor is
    // unreachable from inside an instance, and during the LFG run-mode arming
    // delay (dungeon_active() arms a tick or more after the teleport into a
    // finder-formed group) this rule — gated only on bag-full / low-durability,
    // NOT run mode — would pull a member off the group while DungeonDispatch is
    // not yet driving (observed 2026-06-26: Dunghealer ran walk_to_known_vendor
    // at the Deadmines entrance and lagged the cohesion gate). Repair inside a
    // dungeon is handled by the run, not a vendor walk. Mirrors WalkToKnownHub /
    // FarTravel / Gathering, which already hard-gate on is_in_dungeon.
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    const uint8 vv_phases = s.vendor_visit_phases_pending();
    constexpr uint8 kVvBagFull = 1u << 1;
    constexpr uint8 kVvDuraLow = 1u << 0;
    // Quest-first (2026-06-16): yield to a reachable quest action — UNLESS the
    // bot is progress-blocked (bag full or low durability), which overrides.
    if ((vv_phases & (kVvBagFull | kVvDuraLow)) == 0 && s.has_actionable_quest())
        return false;
    if ((vv_phases & (kVvBagFull | kVvDuraLow)) == 0) return false;
    // Don't walk across the zone toward a vendor for full bags we can't clear: if
    // the only need is bag-full and recovery is in its futility back-off, yield to
    // questing (mirrors BagsFullRecoverGate so the two don't oscillate). Durability
    // still overrides — a broken bot must reach a repair vendor regardless.
    if ((vv_phases & kVvDuraLow) == 0 && (vv_phases & kVvBagFull) &&
        now_ms < ai.bag_recovery_backoff_until())
        return false;
    // Already in snapshot scan range — let idle:travel_to_vendor / vendor_visit handle.
    if (s.nearest_npc_with_flag(UNIT_NPC_FLAG_REPAIR) != nullptr) return false;
    if (s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR) != nullptr) return false;
    using ::Playerbot::V2::World::WorldMetadataKind;
    // Operator-curated Vendor metadata within 600y is the first-choice target.
    if (s.any_metadata_within(uint32(WorldMetadataKind::Vendor), 600.0f))
        return true;
    // Fallback: a real same-map repair-vendor SPAWN (spawn-derived, faction-
    // appropriate), or — cross-map / none — the nearest quest hub (hubs have
    // vendors). Either gives the fire() a destination, so this gate must also
    // pass when one is available. Pure lock-free reads (NOT the Build path).
    if (Services::RepairVendors().GetNearestRepairVendor(s.raw()).has_value())
        return true;
    if (Services::Hubs().GetNearestQuestHub(s.raw()) != nullptr)
        return true;
    return false;
}

bool WalkToKnownVendorFire(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32 now_ms)
{
    using ::Playerbot::V2::World::WorldMetadataStore;
    using ::Playerbot::V2::World::WorldMetadataKind;
    float bx, by, bz; s.position(bx, by, bz);
    float best_dsq = std::numeric_limits<float>::infinity();
    float bx_t = 0.f, by_t = 0.f, bz_t = 0.f;
    bool  have_target = false;

    // First choice: the operator-curated Vendor annotations (manually placed,
    // known-reachable). Pick the nearest on this map.
    auto rows = WorldMetadataStore::Instance().RecordsForMapAndKind(
        s.map_id(), WorldMetadataKind::Vendor);
    for (auto const& r : rows)
    {
        const float dx = r.x - bx, dy = r.y - by;
        const float dsq = dx*dx + dy*dy;
        if (dsq < best_dsq) { best_dsq = dsq; bx_t = r.x; by_t = r.y; bz_t = r.z; have_target = true; }
    }

    // Fallback: no operator metadata yielded a target (or none on this map) —
    // use the spawn-derived nearest same-map repair vendor, and if there is no
    // same-map repair spawn, the nearest quest hub (hubs have vendors). This is
    // what unblocks questless wilderness bots with no curated annotation. Both
    // are pure lock-free index reads (off the Build hot path).
    if (!have_target)
    {
        if (auto hit = Services::RepairVendors().GetNearestRepairVendor(s.raw()))
        {
            bx_t = hit->x; by_t = hit->y; bz_t = hit->z; have_target = true;
            const float dx = bx_t - bx, dy = by_t - by;
            best_dsq = dx*dx + dy*dy;
        }
        else if (auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw()))
        {
            // GetNearestQuestHub returns +inf-distance hubs only when no same-map
            // hub exists; guard on same-map so we never step-walk toward a hub on
            // another continent (the cross-map composer owns those journeys).
            if (hub->mapId == s.map_id())
            {
                bx_t = hub->location.GetPositionX();
                by_t = hub->location.GetPositionY();
                bz_t = hub->location.GetPositionZ();
                have_target = true;
                const float dx = bx_t - bx, dy = by_t - by;
                best_dsq = dx*dx + dy*dy;
            }
        }
    }

    if (!have_target) return false;
    if (best_dsq <= 80.0f * 80.0f) return false;   // hand off to in-scan rule
    const float kStep =
        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                                                                       35.0f;
    const float dist = std::sqrt(best_dsq);
    const float scale = std::min(kStep, dist) / dist;
    const float tx = bx + (bx_t - bx) * scale;   // near waypoint, threat sweep only
    const float ty = by + (by_t - by) * scale;
    if (NearbyUnit const* threat = s.path_threat(
            tx, ty,
            /*max_forward*/ std::min(kStep, 35.0f),
            /*half_width*/  10.0f))
    {
        if (emit.start_attack(threat->guid))
        {
            ai.set_last_rule_fired("idle:walk_vendor_pull_threat");
            return true;
        }
    }
    // Chunked walk with ledge/corner escape to the vendor (the prior single
    // move_to wedged on terrain — observed: Tindle, 91s wedge to Kharanos repair).
    // ChunkedWalkToward steps, escalates a bearing-deflection on a position stall,
    // and yields when genuinely stuck instead of grinding the wall.
    return States::ChunkedWalkToward(s, ai, emit, bx_t, by_t, bz_t,
                                     "idle:walk_to_known_vendor", now_ms);
}

// ---------- idle:critical_repair ----------
// A bot whose gear is critically broken (<=20% durability) does almost no
// weapon/spell damage and is effectively progress-blocked. The routine
// idle:vendor_visit (priority 500) is starved by quest pursuit (698-730), so a
// broken bot chases un-killable quest mobs forever and never repairs (observed:
// Bramwell, 135s combat-locked on quest 26389 at 0% durability). This dedicated
// rule sits ABOVE the quest funnel so a critically-broken, OUT-OF-COMBAT bot
// heads to a repair vendor instead of resuming a quest it cannot complete. It
// fires ONLY at <=20% durability with a reachable repair target, so healthy
// questers never see it. (The combat-side combat:flee_to_repair branch breaks
// the InCombat lock so this idle rule can run; together they close the
// soft-lock.) Routine <30%/<35% repairs stay at idle:vendor_visit (500).
bool CriticalRepairGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (s.lowest_equipped_durability_pct() > 20)
    {
        ai.crit_repair_reset_episode();   // gear healthy again — fresh episode next time
        return false;
    }
    // Futility backoff: the bot was at a vendor making no durability progress
    // (too poor to repair the rest even after selling trash) and yielded so the
    // quest funnel could run and earn money. Stay suppressed until the cooldown
    // expires; the bot quests at partial (functional) durability meanwhile, then
    // returns to finish repairs once it can afford them.
    if (now_ms < ai.crit_repair_backoff_until()) return false;
    // Need a reachable repair target: an in-scan repair NPC, a known repair
    // vendor spawn, or a same-map quest hub (hubs carry repair vendors).
    if (s.nearest_npc_with_flag(UNIT_NPC_FLAG_REPAIR)) return true;
    if (Services::RepairVendors().GetNearestRepairVendor(s.raw()).has_value()) return true;
    if (auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw()))
        if (hub->mapId == s.map_id()) return true;
    return false;
}

bool CriticalRepairFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32 now_ms)
{
    // No repair NPCs exist inside dungeons; walking toward an imaginary one wedges
    // the bot in place for the entire run (observed: Dungrogue cycling
    // idle:critical_repair_walk inside Deadmines harbor while healer fights alone).
    if (s.is_in_dungeon()) return false;

    // Episode-level walk futility. The adjacent timer below only fires once the
    // bot REACHES the vendor; a bot that can't reach one at all (vendor across a
    // navmesh gap, in another zone, or path-blocked) walks idle:critical_repair_walk
    // FOREVER — and because maintenance rules are classified Travel, the wedge-
    // watchdog never rescues it (observed: Zorinus/Arigs/Zorurdi stuck 14-21min).
    // If the bot has spent >4min in critical_repair with NO lowest-durability gain,
    // it can't make progress — back off so the quest funnel runs (the bot earns /
    // travels elsewhere and re-attempts repair after the cooldown). Generous cap so
    // a legitimate long walk to a reachable vendor (repairs -> durability gain ->
    // episode resets) is never cut short.
    // AFFORDABILITY gate (2026-06-21): the futility back-off exists to let a bot
    // that is TOO POOR to raise its lowest-durability item go earn money. A SOLVENT
    // bot is NOT futile — it just hasn't reached the vendor yet (it keeps dying /
    // getting pulled away en route). Backing a solvent bot off hands control to
    // pursue_quest_goal (698), which walks it back into the lethal kill objective ->
    // death -> rez -> repeat (observed: Morthan L9, 20% dura but 82g and a reachable
    // surface vendor 134y away, XP frozen for hours). Only the genuinely-poor case
    // may declare the repair episode futile.
    const uint32 crit_repair_cost = s.estimated_repair_cost();
    const bool   crit_can_afford  = (crit_repair_cost == 0) || (uint64(s.gold()) >= uint64(crit_repair_cost));
    constexpr uint32 kRepairEpisodeMaxMs = 240u * 1000u;
    constexpr uint32 kRepairWalkBackoffMs = 5u * 60u * 1000u;
    if (crit_can_afford)
        ai.crit_repair_reset_episode();   // solvent: never declare the walk futile
    else if (ai.crit_repair_note_active(s.lowest_equipped_durability_pct(), now_ms, kRepairEpisodeMaxMs))
    {
        // Too poor to repair AND can't reach progress: back off so the ESCALATION
        // runs — with the pursue_quest_goal broken-gear gate (QuestRules) stopping
        // a quest trip, this yields to bags_full_recover@710 -> capital_bag_run@705,
        // which takes a broke+full-bagged bot to its NEAREST capital to bank the
        // hoard + sell (earn money + free bags) so it can then afford the repair.
        // (Broke Morthan: 0% gear, ~20s, near Undercity -> capital run recovers it.)
        ai.set_crit_repair_backoff(now_ms + kRepairWalkBackoffMs);
        ai.crit_repair_reset_episode();
        return false;
    }

    NearbyUnit const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_REPAIR);
    if (npc)
    {
        float bx, by, bz; s.position(bx, by, bz);
        const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        // Always attempt the repair when plausibly in range. The server gates it on
        // Player::GetNPCIfCanInteractWith (range AND line-of-sight), so a RepairAll
        // fired through a wall returns InvalidTarget and silently no-ops.
        constexpr float kInteract = 5.0f;
        if (dsq <= kInteract * kInteract)
        {
            const uint64 npc_low = npc->guid.GetCounter();
            if (!ai.action_recently_tried(BotAI::ActionKind::Repair, npc_low, now_ms))
            {
                emit.face_target(npc->guid);
                emit.emit(VendorIntent{RepairAllIntent{npc->guid, /*from_guild_bank*/ false}});
                ai.note_action_retry(BotAI::ActionKind::Repair, npc_low, now_ms);
            }
        }
        // Keep PATHING to the vendor's EXACT position until truly adjacent (~2y).
        // move_to runs PathGenerator, which routes AROUND walls / through the door to
        // a navmesh-reachable spot next to the NPC. This is critical because
        // nearest_npc_with_flag picks the closest vendor by straight-line distance,
        // which can sit just through a wall with NO line-of-sight — and the server's
        // interact check needs LoS. The old "stop ~3y short" straight-line approach
        // parked the bot AT the wall, where every RepairAll failed the LoS check and
        // the bot looped idle:critical_repair forever with 0% gear (observed: Bramwell
        // frozen 3y from Godric Rothgar inside the Northshire Abbey). Pathing to the
        // vendor's real position walks the bot to the door, inside, up to the NPC,
        // where it gains LoS and the RepairAll above then succeeds.
        constexpr float kAdjacent = 2.0f;
        if (dsq > kAdjacent * kAdjacent)
        {
            // Position-stall escape: the single move_to to the NPC wedged 91s
            // (observed: Morthan, critical_repair_approach) — the bot corners /
            // climbs a ledge and never closes the last yards. On a position stall,
            // deflect the approach bearing (escalating angle) to break free, then
            // re-aim at the NPC; otherwise head straight to its exact position so
            // PathGenerator routes through the door for LoS.
            float mtx = npc->x, mty = npc->y;
            const uint8 strikes = ai.walk_stall_note(bx, by, now_ms);
            if (strikes >= 1)
            {
                const float adx = npc->x - bx, ady = npc->y - by;
                const float adist = std::sqrt(adx*adx + ady*ady);
                if (adist > 0.1f)
                {
                    const float ang = (strikes % 2 == 1 ? 1.0f : -1.0f) *
                                      std::min(0.7f * float(strikes), 2.6f);
                    const float c = std::cos(ang), sn = std::sin(ang);
                    const float step = std::min(15.0f, adist) / adist;
                    mtx = bx + (adx * c - ady * sn) * step;
                    mty = by + (adx * sn + ady * c) * step;
                }
            }
            emit.move_to(mtx, mty, npc->z, /*run*/ true);
            ai.set_last_rule_fired("idle:critical_repair_approach");
            return true;
        }
        ai.walk_stall_reset();   // adjacent to the vendor -> reset the approach stall tracker

        ai.set_last_rule_fired("idle:critical_repair");
        const uint64 npc_low = npc->guid.GetCounter();

        // Fund the repair from vendor trash. Armorer/repair NPCs almost always
        // also buy (Godric Rothgar = REPAIR|VENDOR), and a fresh bot is usually
        // too poor to repair its whole kit at once. Liquidating greys/unwanted
        // trade-goods first stretches the purse. sell_trash no-ops if the NPC
        // doesn't buy or there's nothing to sell; the SellTrash lockout keeps it
        // to once per visit.
        if (!ai.action_recently_tried(BotAI::ActionKind::SellTrash, npc_low, now_ms))
        {
            emit.emit(VendorIntent{VendorSellTrashIntent{npc->guid}});
            ai.note_action_retry(BotAI::ActionKind::SellTrash, npc_low, now_ms);
        }

        // Futility backoff. The repair above is affordable-partial (PlayerbotAPI
        // repairs cheapest-first up to the bot's money). If the bot is too poor to
        // raise its LOWEST durability any further, it would otherwise pin here at
        // <=20% forever (the gate never closes) and never quest to earn the money
        // it needs. So if it sits adjacent making no durability progress for a
        // while, back off: yield to the quest funnel and retry repairs after a
        // cooldown. At <=20% the repaired items still function, so questing at
        // partial durability is strictly better than an infinite vendor loop.
        constexpr uint32 kRepairFutilityMs = 25u * 1000u;
        constexpr uint32 kRepairBackoffMs  = 5u * 60u * 1000u;
        // With the repair-reserve top-up (API::repair_all grants a broke bot the
        // shortfall), a bot that REACHES the vendor always fully repairs — so this
        // adjacent-futility back-off effectively only trips when the bot can't
        // actually interact (LoS/range), in which case yielding is correct.
        if (ai.crit_repair_note_adjacent(s.lowest_equipped_durability_pct(), now_ms, kRepairFutilityMs))
        {
            ai.set_crit_repair_backoff(now_ms + kRepairBackoffMs);
            ai.crit_repair_reset_episode();
            return false;   // can't make repair progress here: yield so other rules run
        }
        return true;
    }
    // No repair NPC in snapshot scan — walk all the way to the nearest known
    // repair vendor (or same-map hub). Unlike walk_to_known_vendor this does NOT
    // hand off at 80y: when critically broken, close the FULL distance until the
    // vendor enters scan range (then the approach branch above takes over). This
    // covers the dead-zone where a vendor sits beyond NPC scan range but inside
    // 80y — where the routine routing stalled and the bot idled near the vendor
    // unrepaired (observed: Bramwell parked ~83y from the Northshire repair NPC,
    // looping idle:look_at_npc at 0% durability).
    float tx = 0.f, ty = 0.f, tz = 0.f;
    bool have = false;
    if (auto hit = Services::RepairVendors().GetNearestRepairVendor(s.raw()))
    { tx = hit->x; ty = hit->y; tz = hit->z; have = true; }
    else if (auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw()))
        if (hub->mapId == s.map_id())
        {
            tx = hub->location.GetPositionX();
            ty = hub->location.GetPositionY();
            tz = hub->location.GetPositionZ();
            have = true;
        }
    if (!have) return false;
    // Chunked walk with ledge/corner escape to the repair vendor (the prior single
    // move_to wedged on terrain it couldn't cross — observed: Morthan,
    // critical_repair_walk Travel-wedge 91s heading to the Brill vendor; and the
    // repair path often crosses hostile/elevated ground). ChunkedWalkToward steps,
    // escalates a bearing-deflection on a position stall, and yields when genuinely
    // stuck so the bot stops grinding the wall.
    return States::ChunkedWalkToward(s, ai, emit, tx, ty, tz,
                                     "idle:critical_repair_walk", now_ms);
}

// ---------- idle:bags_full_recover ----------
// Full bags are a HARD progress-blocker — the bot can't loot a kill, accept an
// item-granting quest, or receive a quest reward. A player-like bot must clear
// its bags BEFORE questing, exactly like a human ("I'm full, go sell / buy a
// bag"). The existing vendor-travel logic was buried in the autoact catch-all
// (priority 50), far below equip_upgrade (600), innkeeper_rebind (240) and the
// quest funnel — so a full-bagged bot did everything EXCEPT go sell (live:
// Durnan, full backpack, frozen oscillating innkeeper_rebind/equip_upgrade).
// This rule lifts vendor recovery ABOVE the quest funnel (gated on genuinely-
// full bags so healthy questers never reach it; below critical_repair 735 so a
// broken-gear bot still repairs first). It reuses the existing vendor fires:
// at a vendor it sells trash + buys a bag (VendorVisitFire); otherwise it walks
// to the nearest in-scan vendor, or routes to a known vendor / hub town
// (WalkToKnownVendorFire). If there is genuinely nothing to sell and no bag to
// buy, VendorVisitFire returns false and this YIELDS — so the bot resumes
// questing and gains combat XP from kills rather than parking at the vendor.
bool BagsFullRecoverGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    // Reset the futility window only on a GENUINE recovery — bags clearly cleared
    // (>=10 free), not the transient spikes a churning bot makes at a vendor
    // (sell a few greys -> free briefly 6-8 -> buy food/bandage -> full again).
    // A low threshold let those spikes reset the window every few seconds, so the
    // bot never escalated to the capital run. 10 sits well above any sell burst
    // for a small-bagged bot but is easily reached by a real bank/big-bag clear.
    if (s.raw().bags.bag_free_slots >= 10)
        ai.bag_recovery_reset();
    constexpr uint8 kVvBagFull = 1u << 1;
    if ((s.vendor_visit_phases_pending() & kVvBagFull) == 0)
        return false;   // not genuinely full (<=2) — no dedicated local vendor trip
    if (s.in_combat() || s.is_casting() || s.raw().movement.is_mounted) return false;
    if (s.in_group() || s.is_in_dungeon() || s.in_battleground()) return false;
    // Futility back-off: a real player doesn't freeze on full bags. If a dedicated
    // recovery trip hasn't freed a single slot within the window, the bags are full
    // of unsellable items (quest items / kept gear / reagents) — selling can't help.
    // Yield to questing (turn-ins free quest-item slots; the bank/AH capital run
    // handles reagents/bags) and retry recovery after the back-off.
    if (now_ms < ai.bag_recovery_backoff_until()) return false;
    if (ai.bag_recovery_note_and_check_futile(s.raw().bags.bag_free_slots, now_ms))
    {
        // Set the back-off and YIELD. Do NOT call bag_recovery_reset() here — it
        // clears bag_recovery_backoff_until_ms_, which would instantly wipe the
        // back-off we just set (and capital_bag_run keys on it). The back-off
        // gate above short-circuits future ticks; when it expires the still-old
        // futility window re-triggers and re-arms. reset() runs only when bags
        // genuinely clear (>=10 free), ending the futile episode.
        constexpr uint32 kBagBackoffMs = 4u * 60u * 1000u;
        ai.set_bag_recovery_backoff(now_ms + kBagBackoffMs);
        return false;
    }
    return true;
}

bool BagsFullRecoverFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                         BotIntentEmitter& emit, uint32 now_ms)
{
    NearbyUnit const* v = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR);
    if (!v) v = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR);
    if (v)
    {
        float bx, by, bz; s.position(bx, by, bz);
        const float dx = v->x - bx, dy = v->y - by, dz = v->z - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq <= 5.0f * 5.0f)
            return VendorVisitFire(s, ai, g, emit, now_ms);   // sell trash + buy bag (yields if nothing)
        if (dsq <= 80.0f * 80.0f)
        {
            // Walk the last yards to the in-scan vendor (wedge-guarded so an
            // unreachable one yields to the far-vendor / hub path below).
            if (ai.check_anchor_wedge("idle:bags_full_recover", s.path_blocked_count(), now_ms))
                return WalkToKnownVendorFire(s, ai, g, emit, now_ms);
            const float d = std::sqrt(dsq);
            const float sc = (d - 4.0f) / d;
            emit.move_to(bx + dx * sc, by + dy * sc, v->z, /*run*/ true);
            ai.set_last_rule_fired("idle:bags_full_recover");
            return true;
        }
    }
    // No vendor in scan — walk to the nearest known vendor / hub town (vendors live there).
    return WalkToKnownVendorFire(s, ai, g, emit, now_ms);
}

// ---------- idle:capital_bag_run ----------
// What a real player does when bags are full of items a local vendor can't take
// (reagents, quest items, kept gear) and selling nearby won't free space: make a
// trip to a CAPITAL to bank the reagents/special items (and, at the AH there, buy
// a bigger bag). Capitals co-locate banker + auctioneer + vendors, so this rule
// only does the ROUTING — once the bot arrives, the existing passive rules
// (idle:bank_deposit, idle:vendor_visit, the AH rules) run the transactions and
// free the bags, which clears the full bit and ends this rule.
//
// It engages only AFTER local recovery has proven futile (the bag_recovery
// back-off is active), so the bot first tries to sell at a nearby vendor, then
// escalates to the city trip — and only when the bot's faction capital is on the
// CURRENT map (MVP: same-map routing; cross-map capital runs fall through to the
// anti-freeze, i.e. keep questing, until cross-map routing is added here).
bool CapitalBagRunGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (s.in_combat() || s.is_casting() || s.raw().movement.is_mounted) return false;
    if (s.in_group() || s.is_in_dungeon() || s.in_battleground()) return false;
    // Unreachable-city back-off: a prior in-city walk gave up (navmesh gap at the
    // banker/AH cluster). Yield to questing for the cooldown so the bot levels off
    // kills instead of re-looping the unreachable walk above the quest funnel.
    if (ai.capital_run_in_futility(now_ms)) return false;
    // Engage only once local recovery proved futile: BagsFullRecoverGate sets the
    // back-off after ~2 min with bags never comfortably clearing, and
    // bag_recovery_reset() clears it only when bags reach >6 free. So a non-zero
    // value means "still stuck with near-full bags" — robust to the 2<->3 churn
    // (we deliberately do NOT re-check the strict full bit here, which toggles).
    if (ai.bag_recovery_backoff_until() == 0) return false;
    using ::Playerbot::V2::World::NearestCapital;
    float bx, by, bz; s.position(bx, by, bz);
    // NEAREST faction capital (not race-home): a dwarf full-bagged in Stormwind
    // banks in Stormwind, it does NOT trek to Ironforge (that far walk is exactly
    // what wedged the old race-home capital run).
    auto const* cap = NearestCapital(s.map_id(), bx, by, s.raw().identity.race);
    if (!cap) return false;   // no same-map faction capital (cross-map run = flight, follow-up)
    const float dx = cap->x - bx, dy = cap->y - by;
    const float dsq = dx*dx + dy*dy;
    // Fire ONLY when already in/at the capital city: a SHORT, navigable in-city
    // walk to the banker/AH/vendor cluster. NOT a long wilderness trek — that was
    // the 2026-06-21 disable reason (the ~1000y cross-zone walk wedged at Durotar).
    // The "travel TO a far capital to bank" case needs flight routing (separate
    // work) and is excluded here. 800y ~ a capital's city footprint from center.
    constexpr float kInCityMaxSq = 800.0f * 800.0f;
    return dsq > (60.0f * 60.0f) && dsq <= kInCityMaxSq;
}

bool CapitalBagRunFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                       BotIntentEmitter& emit, uint32 now_ms)
{
    using ::Playerbot::V2::World::NearestCapital;
    float bx, by, bz; s.position(bx, by, bz);
    auto const* cap = NearestCapital(s.map_id(), bx, by, s.raw().identity.race);
    if (!cap) return false;
    // Chunked walk with ledge/corner escape to the banker/AH/vendor cluster. The
    // prior single move_to climbed a Stormwind ledge (z=100) and froze ~370y short
    // of the bank; ChunkedWalkToward steps + escalates a bearing-deflection on a
    // position stall to break off the ledge, and yields when genuinely stuck.
    // When it gives up (returns false — the cluster is genuinely unreachable, e.g.
    // a navmesh gap at the destination), arm the futility back-off so the gate
    // yields to questing for the cooldown instead of restarting the walk every tick
    // (live 2026-06-22: Durnan pinned on the same 55y/6.5y-short Stormwind path).
    if (!States::ChunkedWalkToward(s, ai, emit, cap->x, cap->y, cap->z,
                                   "idle:capital_bag_run", now_ms))
    {
        ai.capital_run_note_futile(now_ms);
        return false;
    }
    return true;
}

} // anonymous namespace

void RegisterVendorRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:critical_repair";
        // 735: ABOVE the quest funnel (quest rules 698-730) so a critically-
        // broken (<=20% durability) out-of-combat bot heads to a repair vendor
        // instead of resuming a quest it does ~no damage on. Below the survival/
        // heal band (>=830) so a genuinely hurt bot still heals first. The narrow
        // trigger (critical durability + reachable vendor) means healthy questers
        // never reach it. Pairs with combat:flee_to_repair (breaks the InCombat
        // lock). Routine <30%/<35% repairs stay at idle:vendor_visit (500).
        rule.priority = 735;
        rule.gate     = &CriticalRepairGate;
        rule.fire     = &CriticalRepairFire;
        // NO min_interval throttle: this rule must fire EVERY tick so the bot
        // walks continuously to the vendor. With a throttle, the skipped ticks
        // fell through to lower-priority quest-pursuit, which walked the bot
        // back toward its quest goal — the two oscillated and the bot never
        // reached the vendor (live: Bramwell stuck at 0% durability ~110y from
        // the Northshire repair NPC, never closing the gap).
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:bags_full_recover";
        // 710: ABOVE the quest funnel (698-702) so a genuinely-full-bag bot
        // clears its bags before questing (full bags block loot/accept/turn-in);
        // below critical_repair (735) so broken gear is still fixed first. Narrow
        // trigger (bag-full bit) keeps healthy questers out. Yields when there's
        // nothing to sell / no bag to buy, so it never parks a bot at a vendor.
        rule.priority = 710;
        rule.gate     = &BagsFullRecoverGate;
        rule.fire     = &BagsFullRecoverFire;
        // No min_interval: must drive the walk continuously to the vendor (same
        // reasoning as critical_repair — a throttle lets lower rules pull it back).
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:capital_bag_run";
        // 705: below bags_full_recover (710) so the LOCAL vendor run tries first;
        // above the quest funnel (698-702) so the escalation pre-empts questing.
        // RE-ENABLED 2026-06-21 with two regression fixes vs. the original (which
        // wedged bots on ~1000y wilderness walks to their race-home capital):
        //   (1) NEAREST faction capital, not race-home (a dwarf in SW banks in SW);
        //   (2) gate fires ONLY when already in/at the capital city (<=800y) — a
        //       short navigable in-city walk to the banker/AH cluster, never the
        //       long wilderness trek that caused the original pin-at-Durotar bug.
        // Escalation-gated: only fires once the local bag_recovery back-off is
        // active (the nearby vendor run proved unable to free space — e.g. bags
        // full of bankable reagents / kept gear a vendor won't take, the hoarder
        // case). Once at the cluster the passive bank/AH/vendor rules run the
        // transactions and free the bags. The cross-map / far "travel TO a capital
        // to bank" case still needs flight routing (separate follow-up).
        rule.priority = 705;
        rule.gate     = &CapitalBagRunGate;
        rule.fire     = &CapitalBagRunFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:vendor_visit";
        // High priority: vendor visits should out-rank most utility rules
        // because the bot is already parked at the vendor. The legacy
        // cascade put this above auto-equip + most movement rules. 500
        // matches the example in the handover doc.
        rule.priority = 500;
        rule.gate     = &VendorVisitGate;
        rule.fire     = &VendorVisitFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:walk_to_known_vendor";
        // Priority lower than vendor_visit (500) but higher than wander —
        // a bag-full bot should head toward the known vendor before
        // wandering off to do unrelated work.
        rule.priority = 485;
        rule.gate     = &WalkToKnownVendorGate;
        rule.fire     = &WalkToKnownVendorFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot

#include "BotPopulationManager.h"
#include "BotQueueFiller.h"
#include "BotSetupPipeline.h"
#include "BotAccountMgr.h"
#include "BotCharacterFactory.h"
#include "BotComposition.h"
#include "BotIdentityRegistry.h"
#include "BotNamePool.h"
#include "../Services.h"
#include "BotCoordinationBus.h"   // ArenaTeamForming publish (autonomous arena seed)
#include "../Session/BotSessionMgr.h"
#include "../Util/ConfigReader.h"
#include "../Bot/BotIntent.h"
#include "../Bot/BotAI.h"        // BotAI::ActionKind for per-bot rebalance lockout
#include "../Bot/BotRegistry.h"  // walking online bot AIs
#include "../Bot/ClassTables.h"  // IsTankSpec / TankSpecForClass etc.
#include "../Threading/IntentQueue.h"
#include "../Diagnostics/PerfCounters.h"
#include "../Bot/Gear/BotGearGenerator.h"   // weaponless re-gear hygiene (audit B15)
#include "BotItemScorer.h"   // EffectiveItemLevelForLevel — under-gear detection
#include "Battleground.h"
#include "Item.h"   // re-gear hygiene: StoreNewItem/EquipItem need the full type
#include "Bag.h"   // FreeOneJunkBagSlot: GetBagByPos/GetBagSize need the full type
#include "ObjectMgr.h"   // re-gear hygiene: sObjectMgr->GetItemTemplate for ilvl guard
#include "BattlegroundMgr.h"
#include "DungeonFinding/LFGMgr.h"   // LFG-state guard on overflow/hygiene kicks
#include "OwnerRegistry.h"           // altbot (owner-bound) kick exemption
#include "WorldSession.h"            // account-id lookup for the altbot test
#include "Config.h"   // sConfigMgr — Playerbot.Bg.AutoSeed.Matches knob
#include "Player.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "RaceMask.h"
#include <algorithm>
#include <cmath>

namespace Playerbot::V2::Fleet {

namespace {

// 60s reconcile. Earlier experiment dropped this to 5s for faster fill;
// that drove kSpawnRatePerTick=5 to ~60 ops/sec which generated more
// async DB writes than CharacterDatabase.WorkerThreads could drain. Queue
// saturated to 888k pending statements, process consumed ~20 GB RAM
// holding the formatted SQL strings, world thread froze waiting on map
// workers fighting MySQL contention. 60s is the right cadence; if a
// faster fill is needed, raise kSpawnRatePerTick instead so the work
// happens in larger batches per cycle (more writes amortize over fewer
// reconcile ticks).
constexpr uint32 kTickIntervalMs      = 60'000;     // 60s reconcile
constexpr uint32 kHygieneIntervalMs   = 3'600'000;  // 1h hygiene
// Rebalance pass: every 5 min walk online bots, count tank/healer/dps per
// (faction, bracket), and proactively switch hybrid DPS bots when the
// bracket's tank/healer count is below the 1:1:3 target ratio. 5 min is a
// good cadence — fast enough to react before a peak-hour /lfg burst, slow
// enough that idle hybrids aren't churning their talent state on noise.
constexpr uint32 kRebalanceIntervalMs = 5u * 60u * 1000u;
// Per-bot rebalance lockout is enforced via the shared
// BotAI::ActionKind::DualSpec retry table (action_retry_lockout_for
// returns 30 min for that kind). Both rebalance and idle:dual_spec_switch
// stamp the same key, so a bot recently switched by either path is
// excluded from the next pass — they coordinate without an extra
// constant here.
// Cap rebalance switches per cycle: 1 per (faction, bracket) per pass. A
// fleet of 2000 has ~10 brackets × 2 factions = 20 switches every 5min,
// which is well under the talent-apply throttle and prevents a mass-respec
// spike if the fleet just logged in.
constexpr uint32 kRebalanceSwitchesPerBracket = 1;
// Per-Reconcile-cycle spawn budget. Was 5 with 60s reconcile = 5/min,
// then bumped indirectly via 5s reconcile (rolled back). Now 25 per
// 60s = 25/min steady-state spawn rate. Each spawn triggers an async
// Player::SaveToDB transaction (~30-50 prepared statements). 25/min
// produces ~1k async writes/min - well within 12 worker threads'
// drain capacity. Bursts during wipefleet still come from the
// 10-cycle ForceReconcile loop = ~250 spawns immediately.
constexpr uint32 kSpawnRatePerTick    = 25;
// Login rate per Reconcile cycle (60s). Each login adds a Player to a Map,
// triggering relocation notifies + lazy navmesh tile loading on a Map worker
// thread. At 20/cycle the bursts saturated one worker (DK bots cluster in
// Acherus map 609 - all hit the same worker thread), pushing tick time past
// the 60s freeze-detector limit on cold-grid loads. 5/cycle gives the map
// updater room to absorb each batch before the next arrives. Population
// fills slower (300/hour) but stays responsive.
// Was 5 per 60s. Bumped to 25 to match the new spawn budget so logging
// in existing offline bots can keep pace with creation. Map worker
// pressure was the original concern, but navmesh prewarm at module
// init + MapUpdate.Threads=16 in worldserver.conf give us headroom
// for higher login concurrency.
constexpr uint32 kLoginRatePerTick    = 25;
constexpr uint32 kJitMaxAgeDays       = 7;
constexpr float  kHardCapMultiplier   = 1.5f;       // total bots ≤ target * 1.5

// Distribution starts at L10. Below L10 there's nothing to participate in
// (no BG/dungeon/raid queues open until L10 anyway), and L1-9 bots clutter
// starter zones without producing any queue value. Bots that level past 10
// via gameplay come from the natural questing pipeline, not distribution.
constexpr uint8 kBuckets[][2] = {
    {10, 19}, {20, 29}, {30, 39},
    {40, 49}, {50, 59}, {60, 69}, {70, 79}, {80, 80},
};

// #5 session-rhythm: cheap deterministic 64->32 bit mix (splitmix64
// finalizer) for per-bot stagger/jitter without a global RNG. Same input ->
// same output, so a given bot's jitter is stable within a session and
// varies between bots — no Math.random, no shared state, no lock.
inline uint32 MixHash64to32(uint64 x)
{
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return uint32(x);
}

// True when the bot is a real player's ALT rather than population-shaper
// inventory: either explicitly owner-bound (`.playerbot summon` / adopt),
// or living on a NON-pool account (a character the operator/player marked
// and logged in manually, e.g. Somi). Altbots are never kicked by the
// overflow/hygiene/excess passes and don't count toward TotalTarget —
// they ride on top of the ambient population.
bool IsPlayerOwnedAlt(BotId id, Player const* p)
{
    if (Services::Owners().GetOwner(id).account_id != 0)
        return true;
    if (p)
        if (WorldSession const* sess = p->GetSession())
            return !Services::Accounts().is_pool_account(sess->GetAccountId());
    return false;
}

// Returns weight per bucket for the configured shape. Sum of weights = 1.0.
std::vector<float> ShapeWeights(PopulationShape shape, size_t n_buckets, float max_heavy_factor)
{
    std::vector<float> w(n_buckets, 0.0f);
    if (n_buckets == 0) return w;

    switch (shape)
    {
        case PopulationShape::MaxHeavy: {
            // last bucket gets max_heavy_factor; remaining N-1 share the rest
            // weighted by descending pyramid (more at low end).
            w[n_buckets - 1] = max_heavy_factor;
            float remain = 1.0f - max_heavy_factor;
            float pyr_sum = 0.0f;
            for (size_t i = 0; i < n_buckets - 1; ++i)
                pyr_sum += float(n_buckets - 1 - i);
            for (size_t i = 0; i < n_buckets - 1; ++i)
                w[i] = remain * float(n_buckets - 1 - i) / pyr_sum;
            break;
        }
        case PopulationShape::Pyramid: {
            float sum = 0.0f;
            for (size_t i = 0; i < n_buckets; ++i)
                sum += float(n_buckets - i);
            for (size_t i = 0; i < n_buckets; ++i)
                w[i] = float(n_buckets - i) / sum;
            break;
        }
        case PopulationShape::Bell: {
            float mid = float(n_buckets) / 2.0f;
            float sum = 0.0f;
            std::vector<float> tmp(n_buckets);
            for (size_t i = 0; i < n_buckets; ++i)
            {
                float x = (float(i) - mid) / (mid * 0.6f);
                tmp[i] = std::exp(-0.5f * x * x);
                sum += tmp[i];
            }
            for (size_t i = 0; i < n_buckets; ++i)
                w[i] = tmp[i] / sum;
            break;
        }
        case PopulationShape::Flat:
        default:
            for (size_t i = 0; i < n_buckets; ++i)
                w[i] = 1.0f / float(n_buckets);
            break;
    }
    return w;
}

PopulationShape ParseShape(std::string const& s)
{
    if (s == "Pyramid")  return PopulationShape::Pyramid;
    if (s == "Bell")     return PopulationShape::Bell;
    if (s == "Flat")     return PopulationShape::Flat;
    return PopulationShape::MaxHeavy;
}

// The 14 BfA+ allied races (incl. Dracthyr). Each is created in a special /
// phased recruitment scenario, and its race-restricted starter chain grants
// racial abilities the bot would otherwise never learn. StarterQuestAutocomplete
// already gathers these via each quest's AllowableRaces mask + a starter-level
// cap (StarterQuestAutocomplete.cpp:58-87), so no per-quest list is needed here.
bool IsAlliedRace(uint32 race)
{
    switch (race)
    {
        // Alliance
        case RACE_VOID_ELF: case RACE_LIGHTFORGED_DRAENEI: case RACE_DARK_IRON_DWARF:
        case RACE_KUL_TIRAN: case RACE_MECHAGNOME: case RACE_EARTHEN_DWARF_ALLIANCE:
        case RACE_DRACTHYR_ALLIANCE:
        // Horde
        case RACE_NIGHTBORNE: case RACE_HIGHMOUNTAIN_TAUREN: case RACE_MAGHAR_ORC:
        case RACE_ZANDALARI_TROLL: case RACE_VULPERA: case RACE_EARTHEN_DWARF_HORDE:
        case RACE_DRACTHYR_HORDE:
            return true;
    }
    return false;
}

// Bots that must run the starter-only rescue (complete the starter quest chain
// + relocate to capital, kept at start level) even with no distribution bucket,
// because they're created in ISOLATED / phased starter zones whose scripted
// intro grants essential class/racial abilities the bot AI can't earn by normal
// questing:
//   - Death Knight  → Acherus (map 609)
//   - Demon Hunter  → Mardum  (map 1481)
//   - Dracthyr/Evoker → Forbidden Reach (map 2081)
//   - all 14 allied races → their recruitment scenarios (heritage/racials)
// Until per-race start-level behavior is scripted, a start-level bot here that
// never runs through distribution sits incomplete (missing spells/racials) and
// often stuck in a no-/sparse-navmesh zone.
//
// NOTE: classic base races (Human/Orc/Dwarf/…) and Worgen/Goblin/Pandaren are
// intentionally NOT here — they start on the natural L1-9 questing path; owner
// scoped this to DK/DH/Dracthyr + the 14 allied races (2026-05-29).
bool BotNeedsStarterOnly(Player const* p)
{
    return p && (p->GetClass() == CLASS_DEATH_KNIGHT
              || p->GetClass() == CLASS_DEMON_HUNTER
              || IsAlliedRace(p->GetRace()));
}

bool RaceIsAlliance(uint32 race)
{
    switch (race)
    {
        case RACE_HUMAN: case RACE_DWARF: case RACE_GNOME: case RACE_NIGHTELF:
        case RACE_DRAENEI: case RACE_WORGEN: case RACE_PANDAREN_ALLIANCE:
        case RACE_VOID_ELF: case RACE_LIGHTFORGED_DRAENEI: case RACE_DARK_IRON_DWARF:
        case RACE_KUL_TIRAN: case RACE_MECHAGNOME: case RACE_DRACTHYR_ALLIANCE:
        case RACE_EARTHEN_DWARF_ALLIANCE:
            return true;
    }
    return false;
}

} // anonymous

BotPopulationManager::BotPopulationManager()
    : pipeline_(new BotSetupPipeline())
{
}

BotPopulationManager::~BotPopulationManager()
{
    delete pipeline_;
}

void BotPopulationManager::OnWorldTick(uint32 now_ms)
{
    // Granular sub-phase instrumentation. Aggregated across a 60s window
    // and emitted once per window so we can identify which pop sub-phase
    // is the killer at scale (observed 2026-05-18: pop=14321ms/tick at
    // 3160 bots — needed to break that down). Each phase's enter→exit
    // delta accumulates into the per-window total; the window emit logs
    // ALL phase totals so the dominant one is obvious.
    const uint32 wt_start = getMSTime();
    const uint32 t_drain_start = wt_start;

    // Drain deferred LoginBot calls. Each entry was queued by SpawnNew on
    // the previous tick (or earlier this tick); by now the async DB worker
    // has committed the new character row, so the LoginQueryHolder's
    // SELECT will succeed. Without this defer, mass-spawn under load
    // produced "Player::LoadFromDB ... not found in table characters"
    // because LoginBot ran ahead of the async commit.
    //
    // Pass B: cap drain at kLoginDrainPerTick. Supply is ~0.83 logins/sec
    // (Reconcile queues at most 50 per 60s cycle), so even at 1Hz
    // degraded tick the 3/tick cap stays 3.6× above demand. Spreading
    // the burst across multiple ticks removes the ~1.5s stall that a
    // single tick used to absorb when draining 50 entries at once.
    constexpr size_t kLoginDrainPerTick = 3;
    if (!deferred_logins_.empty())
    {
        const size_t drain_count = std::min(deferred_logins_.size(), kLoginDrainPerTick);
        for (size_t i = 0; i < drain_count; ++i)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(deferred_logins_[i]);
            Services::SessionMgr().LoginBot(g);
        }
        deferred_logins_.erase(deferred_logins_.begin(),
                               deferred_logins_.begin() + drain_count);
    }
    perf_window_drain_ms_ += getMSTimeDiff(t_drain_start, getMSTime());

    // Drive pipelines every tick — cheap, O(active bots) read.
    const uint32 t_pipelines_start = getMSTime();
    DriveSetupPipelines();
    perf_window_pipelines_ms_ += getMSTimeDiff(t_pipelines_start, getMSTime());

    // Gear backfill every 5 min — re-gears under-geared online bots (separate
    // from the 1h hygiene so it runs after the fleet logs in, not once at boot).
    RunGearBackfill(now_ms);

    // Hygiene every 1h — cleans up stale JIT bots and enforces hard cap.
    const uint32 t_hygiene_start = getMSTime();
    RunHygiene(now_ms);
    // Corpse backlog drain: 10-min cadence independent of the 1h hygiene
    // (the failed-JIT backlog is ~6K rows; at hygiene cadence it would
    // take half a day to clear while every cycle keeps colliding with
    // its dead names).
    if (!last_corpse_sweep_ms_ || (now_ms - last_corpse_sweep_ms_) >= 10u * 60u * 1000u)
    {
        last_corpse_sweep_ms_ = now_ms;
        SweepFailedJitCorpses();
    }
    perf_window_hygiene_ms_ += getMSTimeDiff(t_hygiene_start, getMSTime());

    // Fleet rebalance every 5min — proactive spec rotation per bracket.
    const uint32 t_rebalance_start = getMSTime();
    RunRebalance(now_ms);
    perf_window_rebalance_ms_ += getMSTimeDiff(t_rebalance_start, getMSTime());

    // BG prep-phase top-up — every 5s, top up active BGs in WAIT_JOIN
    // until each team is at max_per_team. Lets the 2-minute prep window
    // be used to grow the population from initial fill to capacity.
    const uint32 t_bgtopup_start = getMSTime();
    TopUpActiveBGs(now_ms);
    SeedBgMatches(now_ms);
    SeedArenaMatches(now_ms);
    perf_window_bgtopup_ms_ += getMSTimeDiff(t_bgtopup_start, getMSTime());

    // Aggregate emit. 60s window matches FleetLog cadence so the two
    // can be cross-referenced in the same log scroll.
    constexpr uint32 kPerfEmitIntervalMs = 60u * 1000u;
    if (perf_window_start_ms_ == 0) perf_window_start_ms_ = wt_start;
    if (getMSTimeDiff(perf_window_start_ms_, wt_start) >= kPerfEmitIntervalMs)
    {
        TC_LOG_INFO("playerbot.v2",
            "[PopPerf] window={}ms drain={}ms pipelines={}ms hygiene={}ms "
            "rebalance={}ms bgtopup={}ms reconcile={}ms (compute={}ms populate={}ms "
            "reconcile_side={}ms login_existing={}ms spawn_new={}ms logout_lru={}ms)",
            getMSTimeDiff(perf_window_start_ms_, wt_start),
            perf_window_drain_ms_, perf_window_pipelines_ms_, perf_window_hygiene_ms_,
            perf_window_rebalance_ms_, perf_window_bgtopup_ms_, perf_window_reconcile_ms_,
            perf_window_compute_targets_ms_, perf_window_populate_actual_ms_,
            perf_window_reconcile_side_ms_, perf_window_login_existing_ms_,
            perf_window_spawn_new_ms_, perf_window_logout_lru_ms_);
        perf_window_start_ms_       = wt_start;
        perf_window_drain_ms_       = 0;
        perf_window_pipelines_ms_   = 0;
        perf_window_hygiene_ms_     = 0;
        perf_window_rebalance_ms_   = 0;
        perf_window_bgtopup_ms_     = 0;
        perf_window_reconcile_ms_   = 0;
        perf_window_compute_targets_ms_ = 0;
        perf_window_populate_actual_ms_ = 0;
        perf_window_reconcile_side_ms_  = 0;
        perf_window_login_existing_ms_  = 0;
        perf_window_spawn_new_ms_       = 0;
        perf_window_logout_lru_ms_      = 0;
    }

    // Reconcile only every kTickIntervalMs ms.
    if (last_tick_ms_ && (now_ms - last_tick_ms_) < kTickIntervalMs)
        return;
    last_tick_ms_ = now_ms;

    // Split each cycle's budget per faction based on HordePct. Without
    // this, Reconcile's bucket loop drained the entire pool into Alliance
    // before Horde got serviced (8.3× imbalance observed 2026-05-15).
    // Round-half-up keeps the totals at kSpawnRate/kLoginRate when HordePct
    // is non-integer-friendly (e.g. 50% → 12+13).
    {
        const uint8 horde_pct = Services::Config().population_horde_pct();
        spawn_budget_horde_ = (kSpawnRatePerTick * horde_pct + 50u) / 100u;
        spawn_budget_alli_  = kSpawnRatePerTick - spawn_budget_horde_;
        login_budget_horde_ = (kLoginRatePerTick * horde_pct + 50u) / 100u;
        login_budget_alli_  = kLoginRatePerTick - login_budget_horde_;
    }

    Reconcile();
}

void BotPopulationManager::SweepFailedJitCorpses()
{
    // Failed-JIT corpse sweep. The pre-wave-16d spawn_jit login race
    // ("Player::LoadFromDB failed") left thousands of JIT-tagged
    // characters that NEVER entered the world (totaltime=0, online=0)
    // -- dead rows that exhaust the per-race name pools ('name already
    // in use' on every new JIT batch) and bloat the lifecycle. They are
    // pure bug artifacts: never played, never seen by anyone.
    // Bulk-reclaim them (names released to the pool, both rows dropped)
    // at 500/pass.
    auto corpse_res = CharacterDatabase.PQuery(
        "SELECT pv.character_guid_low, c.account, c.name "
        "FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE pv.jit_for_queue IS NOT NULL AND pv.jit_for_queue != '' "
        "AND c.totaltime = 0 AND c.online = 0 "
        "AND pv.last_seen_at < (NOW() - INTERVAL 1 HOUR) LIMIT 500");
    if (!corpse_res)
        return;
    std::string in_list;
    uint32 swept = 0;
    do {
        Field* f = corpse_res->Fetch();
        const uint64 guid    = f[0].GetUInt64();
        const uint32 account = f[1].GetUInt32();
        BotNamePool::Release(f[2].GetString());
        Services::Lifecycle().unmark_as_bot(guid);
        Services::Accounts().note_character_removed(account);
        if (!in_list.empty()) in_list += ',';
        in_list += std::to_string(guid);
        ++swept;
    } while (corpse_res->NextRow());
    if (swept)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM playerbot_v2_character WHERE character_guid_low IN ({})", in_list);
        CharacterDatabase.PExecute(
            "DELETE FROM characters WHERE guid IN ({})", in_list);
        TC_LOG_INFO("playerbot.v2",
            "[Hygiene] swept {} failed-JIT corpse characters (never entered world)", swept);
    }
}

// Shield-tank weapon correction (task #8, 2026-06-28). A Protection Warrior (73)
// or Protection Paladin (66) that was generated/looted onto a TWO-HANDED weapon
// runs SHIELD-LESS: a 2H locks out the offhand, so the tank loses block, ~half
// its armor, and its entire Shield Slam / Shield Block kit (it casts those with
// no shield → they fail). The normal backfill CANNOT fix this — a 1H has LOWER
// ilvl than the 2H so the per-slot upgrade guard refuses the swap, and the
// snapshot auto-equip can't put a shield in the offhand while a 2H holds both
// hands (EQUIP_ERR_2HANDED_EQUIPPED). Observed live 2026-06-28: the Deadmines
// prot-warrior tank ran a 2H + empty offhand and died 38× in the harbor. Force
// the correct 1H+shield: equip the generated 1H mainhand FIRST (replaces the 2H,
// freeing the offhand), then the shield. Cheap pre-checks short-circuit every
// non-mis-equipped bot before the generator call. Returns true if it changed a
// slot. World-thread only (mutates the Player) — called from RunGearBackfill.
static bool EnsureShieldTankWeapon(Player* p)
{
    if (!p) return false;
    uint32 const spec = uint32(AsUnderlyingType(p->GetPrimarySpecialization()));
    if (spec != 73 && spec != 66) return false;       // Prot Warrior / Prot Paladin
    Item* mh = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* oh = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    // Already correct: a shield in the offhand (impossible alongside a 2H).
    if (oh && oh->GetTemplate() &&
        oh->GetTemplate()->GetInventoryType() == INVTYPE_SHIELD)
        return false;
    bool const mh_is_2h = mh && mh->GetTemplate() &&
        mh->GetTemplate()->GetInventoryType() == INVTYPE_2HWEAPON;

    Gear::GearGenerationContext ctx;
    ctx.level  = p->GetLevel();
    ctx.cls    = p->GetClass();
    ctx.spec   = uint16(spec);
    ctx.bot_id = p->GetGUID().GetCounter();
    uint32 oneh_entry = 0, shield_entry = 0;
    for (auto const& gen : Gear::GenerateGearFor(ctx))   // generator now skips 2H mainhand for these specs
    {
        if (gen.slot == EQUIPMENT_SLOT_MAINHAND) oneh_entry = gen.item_entry;
        else if (gen.slot == EQUIPMENT_SLOT_OFFHAND) shield_entry = gen.item_entry;
    }
    auto force_equip = [&](uint32 entry, uint8 slot) -> bool
    {
        if (!entry) return false;
        Item* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (cur && cur->GetEntry() == entry) return false;
        ItemPosCountVec dest;
        if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, 1) != EQUIP_ERR_OK)
            return false;
        Item* it = p->StoreNewItem(dest, entry, true);
        if (!it) return false;
        uint16 eq_dest = 0;
        if (p->CanEquipItem(slot, eq_dest, it, /*swap*/ true) != EQUIP_ERR_OK)
            return false;   // generated piece refused (leave it in the bag)
        const uint16 src = (uint16(it->GetBagSlot()) << 8) | it->GetSlot();
        const uint16 dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | slot;
        p->SwapItem(src, dst);   // SAFE unequip + re-store-displaced
        return true;
    };
    bool changed = false;
    if (mh_is_2h) changed |= force_equip(oneh_entry, EQUIPMENT_SLOT_MAINHAND);
    changed |= force_equip(shield_entry, EQUIPMENT_SLOT_OFFHAND);
    if (changed) p->AutoUnequipOffhandIfNeed();
    return changed;
}

// Free ONE bag slot by destroying the least-valuable grey (POOR) vendor-trash
// item, so a strict gear UPGRADE can be staged when the bags are full. This is
// the common case for a bot that has looted a full dungeon — the live blocker
// for the Deadmines healer re-gear: every generated upgrade hit CanStoreNewItem
// == bags_full and the healer stayed ilvl48 (under-healing the harbor). Only
// POOR-quality items are ever destroyed (vendor trash a player drops on sight),
// never quest items, never a non-empty container, never UNCOMMON+ loot. Returns
// true if a slot was freed. Distribution bots only — altbots are guarded out by
// the caller (they never re-gear-swap an occupied slot).
static bool FreeOneJunkBagSlot(Player* p)
{
    if (!p) return false;
    Item* victim = nullptr;
    uint32 victim_val = 0xFFFFFFFFu;
    auto consider = [&](Item* it)
    {
        if (!it) return;
        ItemTemplate const* t = it->GetTemplate();
        if (!t) return;
        if (t->GetQuality() != ITEM_QUALITY_POOR) return;   // grey trash only
        if (it->IsNotEmptyBag()) return;                    // never a full container
        if (t->HasFlag(ITEM_FLAG_HAS_LOOT)) return;         // never a lootable (locked) item
        const uint32 val = t->GetSellPrice() * std::max<uint32>(1u, it->GetCount());
        if (val < victim_val) { victim_val = val; victim = it; }
    };
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        consider(p->GetItemByPos(INVENTORY_SLOT_BAG_0, i));
    for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END; ++b)
        if (Bag* bag = p->GetBagByPos(b))
            for (uint32 s = 0; s < bag->GetBagSize(); ++s)
                consider(p->GetItemByPos(b, uint8(s)));
    if (!victim) return false;
    p->DestroyItem(victim->GetBagSlot(), victim->GetSlot(), true);
    return true;
}

void BotPopulationManager::RunGearBackfill(uint32 now_ms)
{
    // 5-min cadence (vs the 1h hygiene). Re-gears under-geared / weaponless
    // ONLINE bots. Two historical roots are now fixed at the source — the equip
    // swap destroyed generated upgrades (EquipItem on an occupied slot, see
    // Player.cpp:11758), and the generator ranked by static base ilvl not the
    // level-scaled effective ilvl bots actually wear (Varethon L16 stuck at
    // effective ItemLevel 5). But GenerateGear is a one-shot setup bit that
    // never re-runs, so the EXISTING fleet (geared under the broken code, or
    // out-leveled past setup) needs a backfill. ONLINE-only, capped 25/pass,
    // OFF the snapshot-build thread. Uses Player::SwapItem (the SAFE unequip +
    // re-store-displaced path, NOT the destructive RemoveItem+EquipItem) and
    // strict guards so it never dupes, churns, or downgrades.
    constexpr uint32 kGearBackfillIntervalMs = 5u * 60u * 1000u;
    if (last_gear_backfill_ms_ && (now_ms - last_gear_backfill_ms_) < kGearBackfillIntervalMs)
        return;
    last_gear_backfill_ms_ = now_ms;

    uint32 regear_count = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (regear_count >= 25) return;     // bound per pass
        Player* p = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(id));
        // L>=5 (was L>=10): an under-geared bot BELOW 10 is exactly the death-
        // spiral victim that most needs help — it can't out-level into the old
        // L>=10 gate because it keeps dying to quest mobs in ilvl-~1 starter gear
        // (observed 2026-06-21: Morthan, L9 warlock, effective ilvl 1, looping
        // die->repair->die on a 5-mob kill quest it can't survive). The old gate
        // only existed to mirror the professions gate (LearnProfessions needs
        // L>=10); GEARING has no such dependency. The `under_geared` test below
        // (weaponless || avg_eff < level) is the real safety — a properly-geared
        // low bot is never touched — so this only rescues genuinely-stuck bots.
        // L1-4 stay excluded: they're in the immediate starter window where
        // ilvl-1 gear IS level-appropriate and quest greens arrive within minutes.
        if (!p || !p->IsInWorld() || p->GetLevel() < 5) return;

        // Shield-tank weapon correction runs for EVERY online shield-tank each
        // pass (cheap spec/slot pre-check inside), INDEPENDENT of the under-gear
        // gate below: a prot tank on a 2H is "geared" by average ilvl yet
        // critically mis-equipped (no shield). Does not consume the regear cap.
        if (EnsureShieldTankWeapon(p))
            TC_LOG_INFO("playerbot.v2",
                "[GearBackfill] shield-tank weapon fix: {} -> 1H+shield", p->GetName());

        const uint8 lvl = p->GetLevel();
        const bool weaponless =
            !p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

        // Altbots (owner-bound) keep the SAFE legacy behavior: fill an empty
        // mainhand only, never touch armor. A real player's alt may run
        // intentional transmog / twink gear (owner decision 2026-06-10 keeps
        // them combat-functional, but we must not overwrite owner choices).
        const bool is_altbot = Services::Owners().GetOwner(id).account_id != 0;

        // Average equipped EFFECTIVE ilvl over filled slots — the value the
        // bot actually wears (GetItemLevel(p) applies 12.0 scaling curves).
        uint32 eff_sum = 0, eff_cnt = 0;
        for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
            if (Item const* eq = p->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
                { eff_sum += eq->GetItemLevel(p); ++eff_cnt; }
        const uint32 avg_eff = eff_cnt ? eff_sum / eff_cnt : 0;

        // Under-geared if missing a weapon OR wearing effective ilvl below the
        // character level. Level-appropriate gear has effective ilvl >= level
        // (an L40 green is ~ilvl 40-50), while starter gear sits at ~ilvl 1-5
        // regardless of level — so avg_eff < lvl cleanly separates the ilvl-5/15
        // victims from properly-geared bots WITHOUT the scale mismatch of the
        // linear TargetIlvlForLevel ramp (which is ~120 at L16, far above what
        // any L16 item wears at, so it would flag everyone forever). Cheap: no
        // generator call needed to decide.
        // DUNGEON-READINESS bar (2026-06-28): the open-world `avg_eff < lvl`
        // gate deems a bot wearing level-appropriate GREENS "geared" — fine for
        // questing, but too weak for scaled 5-man content. Live in Deadmines: a
        // Holy Priest healer at avg_eff 48 (L30) could NOT out-heal the harbor
        // Defias caster burst, yet the SAME generator gives the tank ilvl ~75 at
        // L30 — so the healer/rogue (ilvl 36-48) sat 25+ ilvl below their own
        // achievable gear and the squad death-grinded. Inside a dungeon/raid we
        // therefore re-gear toward the generator's potential whenever the worn
        // avg is well under ~2x level (the low-level generator output sits near
        // 2.3-2.5x level). The STRICT per-slot guards below (entry-equality +
        // effective-ilvl-upgrade + quality) still apply, so this can only swap in
        // pieces that are genuinely better and NEVER downgrades a good item — a
        // well-geared dungeon bot (tank ilvl75 / hunter ilvl74 here) is left
        // untouched; only the genuinely under-geared support gets pulled up.
        const bool in_instance_run =
            p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsRaid());
        // Dungeon-readiness bar (2026-06-29): the old lvl*2 bar (60 at L30) sat
        // BELOW the generator's own achievable ceiling (~2.3-2.5x level, i.e.
        // 69-75 at L30 — the tank/hunter hit 75), so a support bot at avg_eff 67
        // was deemed "geared" yet sat ~8 ilvl under its OWN best gear and never
        // got pulled up. Measured live in Deadmines: that 8-25 ilvl support
        // deficit is the throughput wall on the scaled ship deck (Ripsnarl 213k +
        // Cookie 186k). Raise the bar to ~2.5x level so support is re-geared
        // toward the generator's potential. The strict per-slot guards below
        // (entry-equality + strict effective-upgrade + quality) mean this only
        // swaps in genuinely better pieces and NEVER downgrades — a bot already
        // at its class/slot ceiling sees swapped=0 and is parked on the
        // failed-backfill cooldown, so the higher bar costs at most one no-op
        // GenerateGearFor per cooldown window. Re-gearing also replaces broken
        // (0%-durability) pieces with fresh 100%-dur gear — the orphaned-DPS
        // durability decay seen on the deck.
        const uint32 dungeon_bar = (uint32(lvl) * 5u) / 2u;
        const bool dungeon_undergear = in_instance_run && (avg_eff < dungeon_bar);
        const bool under_geared =
            weaponless || (avg_eff < uint32(lvl)) || dungeon_undergear;
        if (under_geared)
            TC_LOG_INFO("playerbot.v2",
                "[GearBackfill] candidate {} L{} weaponless={} avg_eff_ilvl={} "
                "(threshold <L{}, dungeon_bar={} hit={})",
                p->GetName(), uint32(lvl), weaponless, avg_eff, uint32(lvl),
                in_instance_run ? dungeon_bar : 0u, dungeon_undergear);
        if (!under_geared) return;

        // Skip bots parked on the failed-backfill cooldown (could not be
        // re-geared last time — bags full / equip refused) so they do not
        // re-consume the processed-cap and starve re-gearable bots downstream.
        {
            auto it = gear_backfill_skip_until_.find(id);
            if (it != gear_backfill_skip_until_.end() && it->second > now_ms)
                return;
        }

        // Cap counts PROCESSED bots (not just successes): this bounds the
        // world-tick cost of GenerateGearFor (which now does DB2 effective-ilvl
        // lookups per candidate) regardless of how many bots turn out unfixable
        // (sparse pool). At 25/pass every 5 min that is 300 bots/hr — the
        // historical backlog drains overnight without spiking the tick.
        ++regear_count;

        // For altbots we only ever fill an empty weapon slot (legacy-safe).
        const bool armor_ok = !is_altbot;

        Gear::GearGenerationContext ctx;
        ctx.level  = lvl;
        ctx.cls    = p->GetClass();
        ctx.spec   = uint16(AsUnderlyingType(p->GetPrimarySpecialization()));
        ctx.bot_id = id;
        uint32 swapped = 0;
        for (auto const& g : Gear::GenerateGearFor(ctx))
        {
            const bool is_weapon = (g.slot == EQUIPMENT_SLOT_MAINHAND ||
                                    g.slot == EQUIPMENT_SLOT_OFFHAND);
            if (!is_weapon && !armor_ok)
                continue;   // altbot: weapon-only

            Item const* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, g.slot);
            // Altbots NEVER have an occupied slot replaced (legacy invariant:
            // owner alts only ever got an EMPTY weapon filled, never a swap).
            if (is_altbot && cur)
                continue;
            // Entry-equality guard: never re-store the item already worn
            // (the generator is deterministic — without this the pass would
            // re-buy the same piece forever -> unbounded bag growth).
            if (cur && cur->GetEntry() == g.item_entry)
                continue;
            ItemTemplate const* gtpl = sObjectMgr->GetItemTemplate(g.item_entry);
            int32 gen_eff = gtpl ? ::Playerbot::Gear::EffectiveItemLevelForLevel(gtpl, lvl) : -1;
            int32 cur_eff = cur ? int32(cur->GetItemLevel(p)) : -1;
            int32 equip_err = -1;
            // Strict effective-ilvl-upgrade guard: only replace an occupied
            // slot when the generated piece actually wears BETTER. Protects
            // already-good gear (incl. owner-curated alt gear) from a
            // downgrade toward the coarse linear target ramp.
            if (cur)
            {
                // Quality guard (operator-reported downgrade): a generated
                // COMMON/white must NEVER replace an equipped UNCOMMON+ (quest
                // green / looted blue). The effective-ilvl guard below is not
                // enough — 12.0 scaling can make a generated white's effective
                // ilvl edge above a low-level green's, swapping out real loot the
                // bot earned and leaving it WORSE. Quality is the stable "real
                // gear" signal; never downgrade across it.
                if (gtpl && cur->GetTemplate() &&
                    cur->GetTemplate()->GetQuality() >= gtpl->GetQuality())
                    continue;   // never downgrade across quality (real loot > generated)
                if (gen_eff <= cur_eff)
                    continue;   // generated piece is not a strict effective-ilvl upgrade
            }
            ItemPosCountVec dest;
            bool staged = (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK);
            if (!staged && !is_altbot && FreeOneJunkBagSlot(p))
            {
                // Bags full — first try the least-destructive route: drop ONE grey
                // vendor-trash item (a player clears junk to equip an upgrade) and
                // retry staging. Only POOR-quality is ever destroyed.
                dest.clear();
                staged = (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK);
            }
            if (!staged)
            {
                // DIRECT-EQUIP fallback when bags are full and there is no grey
                // trash to drop — the live Deadmines blocker (healer/rogue, every
                // bag slot full of looted gear, staging impossible, stuck ilvl48).
                // EquipNewItem places the piece straight into the equip slot and
                // needs NO bag space. For an OCCUPIED slot the quality + effective-
                // ilvl guards above already proved the worn piece (`cur`) strictly
                // worse, so destroy it in place first (exactly what a player does
                // swapping an upgrade with a full pack); for an empty slot just
                // equip. Validate equippability (proficiency/level/unique) BEFORE
                // destroying anything so a refusal never leaves the slot naked.
                if (!is_altbot)
                {
                    uint16 eq_dest_direct;
                    InventoryResult dmsg =
                        p->CanEquipNewItem(g.slot, eq_dest_direct, g.item_entry, /*swap*/ cur != nullptr);
                    if (dmsg == EQUIP_ERR_OK)
                    {
                        if (cur)
                            p->DestroyItem(INVENTORY_SLOT_BAG_0, g.slot, true);
                        if (p->EquipNewItem(eq_dest_direct, g.item_entry, ItemContext::NONE, true))
                        {
                            p->AutoUnequipOffhandIfNeed();
                            ++swapped;
                            continue;
                        }
                    }
                }
                continue;   // bags full and direct-equip not possible — skip slot
            }
            Item* it = p->StoreNewItem(dest, g.item_entry, true);
            if (!it) continue;
            uint16 eq_dest;
            equip_err = int32(p->CanEquipItem(g.slot, eq_dest, it, /*swap*/ true));
            if (equip_err == EQUIP_ERR_OK)
            {
                // SwapItem does the proper unequip + re-store-displaced dance
                // (the destructive EquipItem path is why generated gear used
                // to vanish). Displaced item lands back in the bag.
                const uint16 src = (uint16(it->GetBagSlot()) << 8) | it->GetSlot();
                const uint16 dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | g.slot;
                p->SwapItem(src, dst);
                p->AutoUnequipOffhandIfNeed();
                ++swapped;
            }
            // CanEquipItem refused (proficiency/level/unique): leave the one
            // stored item in bags for State_Idle auto_equip's throttled
            // retry. Do NOT loop or re-store.
        }
        if (swapped > 0)
        {
            gear_backfill_skip_until_.erase(id);   // made progress — keep eligible
            TC_LOG_INFO("playerbot.v2",
                "[GearBackfill] re-geared under-geared bot {} (class {} L{}): {} slot(s), "
                "avg_eff_ilvl {} (was below level){}",
                p->GetName(), uint32(p->GetClass()), uint32(lvl), swapped,
                avg_eff, is_altbot ? " [altbot weapon-only]" : "");
        }
        else
        {
            // Under-geared but nothing could be re-geared this pass (bags full /
            // every slot's CanEquipItem refused). Park 30 min so it stops
            // hogging the processed-cap; its normal vendor/equip behavior may
            // free bags or equip the bagged items before the next attempt.
            TC_LOG_INFO("playerbot.v2",
                "[GearBackfill] {} L{} under-geared but 0 swaps this pass "
                "(bags full / every slot's CanEquipItem refused: proficiency/level/unique) "
                "- parked 30min",
                p->GetName(), uint32(lvl));
            gear_backfill_skip_until_[id] = now_ms + 30u * 60u * 1000u;
        }
    });
}

void BotPopulationManager::RunHygiene(uint32 now_ms)
{
    if (last_hygiene_ms_ && (now_ms - last_hygiene_ms_) < kHygieneIntervalMs)
        return;
    last_hygiene_ms_ = now_ms;

    // 0) Under-gear re-gear moved to RunGearBackfill() (own 5-min cadence, driven
    //    from OnWorldTick) so it runs SOON after the fleet logs in and drains the
    //    historical backlog fast — the 1h hygiene pass fires once at boot before
    //    bots are online, which would waste it.

    // 0b) Failed-JIT corpse sweep — extracted to SweepFailedJitCorpses()
    //     and ALSO driven on a 10-min cadence from OnWorldTick so the
    //     multi-thousand backlog drains in hours, not days.
    SweepFailedJitCorpses();

    // 1) Delete JIT bots not seen in N days. Pull (guid, account) so we
    //    can also notify BotAccountMgr to free the pool slot — without
    //    that, the pool count drifts and new spawns hit "account full"
    //    sooner than the hard cap should allow.
    auto del_res = CharacterDatabase.PQuery(
        "SELECT pv.character_guid_low, c.account FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE pv.jit_for_queue IS NOT NULL "
        "AND pv.last_seen_at IS NOT NULL "
        "AND pv.last_seen_at < (NOW() - INTERVAL {} DAY) LIMIT 50",
        uint32(kJitMaxAgeDays));
    if (del_res)
    {
        uint32 deleted = 0;
        do {
            Field* f = del_res->Fetch();
            uint64 guid    = f[0].GetUInt64();
            uint32 account = f[1].GetUInt32();
            // Logout if online (may already be offline).
            Services::SessionMgr().LogoutBot(ObjectGuid::Create<HighGuid::Player>(guid));
            // Release the bot's name back to the pool BEFORE deleting the
            // characters row. BotNamePool::Release uses the name string;
            // we look it up first via the characters row.
            if (auto name_res = CharacterDatabase.PQuery(
                    "SELECT name FROM characters WHERE guid={}", guid))
            {
                BotNamePool::Release(name_res->Fetch()[0].GetString());
            }
            // Drop V2 row + character row + cache entry.
            CharacterDatabase.PExecute(
                "DELETE FROM playerbot_v2_character WHERE character_guid_low={}", guid);
            CharacterDatabase.PExecute(
                "DELETE FROM characters WHERE guid={}", guid);
            // Free the pool-account slot so BotAccountMgr knows there's
            // capacity to spawn a fresh bot on this account next time.
            Services::Accounts().note_character_removed(account);
            ++deleted;
        } while (del_res->NextRow());
        if (deleted)
            TC_LOG_INFO("playerbot.v2", "[Population] hygiene: deleted {} stale JIT bots (>{}d unused)",
                        deleted, kJitMaxAgeDays);
    }

    // 2) Hard cap: if total online bots exceed target * multiplier, mass-logout
    //    excess to prevent runaway.
    //
    // LP-P1b: this is NOT last-activity ordered — the live registry has no
    // per-bot last-activity timestamp, and for_each visits entries in hash
    // order, not recency order. The earlier "oldest LRU first" wording was
    // misleading; we kick the first `to_kick` ELIGIBLE bots the registry
    // yields. (A true LRU would require either a last-activity field on
    // BotRegistryEntry or a DB sort on last_seen_at; neither exists on this
    // path today. // LP-P1b: needs a last-activity signal on BotRegistryEntry
    // for genuine LRU ordering.)
    //
    // Eligibility mirrors LogoutLRUMany's guard set (skip grouped + in
    // dungeon) and ADDS raid + battleground/arena guards: kicking a bot that
    // is grouped, in a dungeon/raid, or in a BG/arena would yank it out of
    // active content and break its group's run. Without these guards the
    // hygiene kick logged out bots mid-instance.
    auto& cfg = Services::Config();
    uint32 target = cfg.population_total_target();
    if (!target) return;
    uint32 hard_cap = uint32(float(target) * kHardCapMultiplier);
    // Pass I: count + kick from live registry instead of lifecycle.
    // Live registry size IS the online count — no per-id
    // FindConnectedPlayer needed for the tally. At 11K marked vs 3K
    // in-world this saved ~8K hashmap lookups per hygiene cycle.
    // Altbots, BG/LFG-queued bots, and kick-protected JIT fills are
    // excluded from the tally (deliberate overshoot above TotalTarget;
    // see Reconcile) and from the kick loop below.
    const uint32 hyg_tally_now = getMSTime();
    uint32 online = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player const* p = ObjectAccessor::FindConnectedPlayer(g);
        if (p && p->InBattlegroundQueue()) return;
        if (p && sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
        if (IsKickProtected(id, hyg_tally_now)) return;
        if (IsPlayerOwnedAlt(id, p)) return;
        if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — off the shaper's books
        ++online;
    });
    if (online > hard_cap)
    {
        uint32 to_kick = online - hard_cap;
        TC_LOG_WARN("playerbot.v2", "[Population] hygiene: hard-cap exceeded ({}>{}); kicking up to {} eligible",
                    online, hard_cap, to_kick);
        std::vector<ObjectGuid> to_logout;
        to_logout.reserve(to_kick);
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            if (to_logout.size() >= to_kick) return;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            // Resolve the Player and SKIP candidates engaged in content —
            // mirrors LogoutLRUMany's guards (group + dungeon) and adds the
            // raid + BG/arena guards. See the LP-P1b note above.
            Player* p = ObjectAccessor::FindConnectedPlayer(g);
            if (!p) return;
            if (p->GetGroup()) return;
            // Spare BG- and LFG-queued bots (JIT match-fill overshoot) —
            // same rationale as the Reconcile overflow kick.
            if (p->InBattlegroundQueue()) return;
            if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
            if (IsKickProtected(id, getMSTime())) return;
            // Altbots are a real player's squad members, not population-
            // shaper inventory — never kick them.
            if (IsPlayerOwnedAlt(id, p)) return;
            if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out
            if (Map* m = p->GetMap())
            {
                if (m->IsDungeon()) return;            // covers dungeon + raid (IsRaid implies IsDungeon)
                if (m->IsBattlegroundOrArena()) return;
            }
            to_logout.push_back(g);
        });
        for (ObjectGuid g : to_logout)
            Services::SessionMgr().LogoutBot(g);
    }
}

void BotPopulationManager::ForceReconcile()
{
    // Split each cycle's budget per faction based on HordePct. Without
    // this, Reconcile's bucket loop drained the entire pool into Alliance
    // before Horde got serviced (8.3× imbalance observed 2026-05-15).
    // Round-half-up keeps the totals at kSpawnRate/kLoginRate when HordePct
    // is non-integer-friendly (e.g. 50% → 12+13).
    {
        const uint8 horde_pct = Services::Config().population_horde_pct();
        spawn_budget_horde_ = (kSpawnRatePerTick * horde_pct + 50u) / 100u;
        spawn_budget_alli_  = kSpawnRatePerTick - spawn_budget_horde_;
        login_budget_horde_ = (kLoginRatePerTick * horde_pct + 50u) / 100u;
        login_budget_alli_  = kLoginRatePerTick - login_budget_horde_;
    }
    Reconcile();
}

std::vector<PopulationBucket> BotPopulationManager::ComputeTargets() const
{
    auto& cfg = Services::Config();
    uint32 total = cfg.population_total_target();
    if (!total) return {};

    PopulationShape shape = ParseShape(cfg.population_shape());
    float max_heavy_factor = cfg.population_max_heavy_factor();
    uint8 horde_pct = cfg.population_horde_pct();

    constexpr size_t N = sizeof(kBuckets) / sizeof(kBuckets[0]);
    auto weights = ShapeWeights(shape, N, max_heavy_factor);

    // B-2: largest-remainder apportionment. The previous per-bucket
    // round-half-up (`weights[i] * total + 0.5`) let the bucket targets
    // sum ABOVE TotalTarget (up to +N/2 bots across N buckets) — and the
    // fleet then held that overshoot forever because every bucket
    // individually sat at its (inflated) target. Floor every bucket,
    // then hand the remaining units to the buckets with the largest
    // fractional parts; targets now sum to exactly `total`.
    std::vector<uint32> bucket_totals(N, 0);
    std::vector<std::pair<float, size_t>> fractions;
    fractions.reserve(N);
    uint32 assigned = 0;
    for (size_t i = 0; i < N; ++i)
    {
        float raw = weights[i] * float(total);
        bucket_totals[i] = uint32(raw);
        assigned += bucket_totals[i];
        fractions.emplace_back(raw - float(bucket_totals[i]), i);
    }
    std::sort(fractions.begin(), fractions.end(),
              [](auto const& a, auto const& b) { return a.first > b.first; });
    for (size_t k = 0; assigned < total && k < fractions.size(); ++k, ++assigned)
        ++bucket_totals[fractions[k].second];

    std::vector<PopulationBucket> out;
    out.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        PopulationBucket b{};
        b.level_lo = kBuckets[i][0];
        b.level_hi = kBuckets[i][1];
        b.horde_target    = bucket_totals[i] * horde_pct / 100u;
        b.alliance_target = bucket_totals[i] - b.horde_target;
        out.push_back(b);
    }
    return out;
}

void BotPopulationManager::PopulateActual(std::vector<PopulationBucket>& buckets) const
{
    // Pass I: walk live registry (in-world bots), not lifecycle (all
    // marked, ~11K historical). The lifecycle entries that aren't
    // in-world contribute nothing to bucket counts anyway — they'd be
    // filtered by FindConnectedPlayer == nullptr. Saves N-live vs
    // N-marked iteration cost (10K+ at scale).
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) return;
        // Altbots are a player's squad, not population-shaper inventory.
        // Counting them here made reconcile_side see the bucket as over-
        // target and log out POOL bots to compensate — summoning 5 alts
        // silently evicted 5 ambient-world bots. Alts ride on top of
        // TotalTarget.
        if (IsPlayerOwnedAlt(id, p)) return;
        uint8 lvl = p->GetLevel();
        bool alliance = p->GetTeam() == ALLIANCE;
        for (auto& b : buckets)
        {
            if (lvl < b.level_lo || lvl > b.level_hi) continue;
            if (alliance) ++b.alliance_actual;
            else          ++b.horde_actual;
            break;
        }
    });
}

PopulationSnapshot BotPopulationManager::Snapshot() const
{
    PopulationSnapshot snap;
    snap.buckets = ComputeTargets();
    PopulateActual(snap.buckets);
    for (auto const& b : snap.buckets)
    {
        snap.total_target += b.alliance_target + b.horde_target;
        snap.total_actual += b.alliance_actual + b.horde_actual;
    }
    return snap;
}

std::vector<BotPopulationManager::OfflineCandidate>
BotPopulationManager::FindOfflineCandidatesIn(uint8 lo, uint8 hi, bool alliance, uint32 cap) const
{
    // Query characters table for V2-marked bots with level ∈ [lo, hi]
    // matching the faction. We use playerbot_v2_character JOINed against
    // characters for level / race.
    std::vector<OfflineCandidate> out;
    auto res = CharacterDatabase.PQuery(
        "SELECT pv.character_guid_low, c.level, c.race FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE c.level BETWEEN {} AND {} "
        "AND c.online = 0 "
        "AND pv.distribution_level > 0 "
        "ORDER BY pv.last_seen_at ASC LIMIT {}",
        uint32(lo), uint32(hi), cap * 2);  // 2x cap, faction filter happens client-side
    if (!res) return out;

    do {
        Field* f = res->Fetch();
        OfflineCandidate c{};
        c.char_guid = f[0].GetUInt64();
        c.level = f[1].GetUInt8();
        c.alliance = RaceIsAlliance(f[2].GetUInt8());
        if (c.alliance != alliance) continue;
        out.push_back(c);
        if (out.size() >= cap) break;
    } while (res->NextRow());
    return out;
}

bool BotPopulationManager::LoginExisting(uint64 char_guid_low, bool alliance)
{
    uint32& budget = alliance ? login_budget_alli_ : login_budget_horde_;
    if (!budget) return false;
    ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(char_guid_low);
    auto r = Services::SessionMgr().LoginBot(g);
    if (r.ok)
    {
        --budget;
        TC_LOG_DEBUG("playerbot.v2", "[Population] login existing {} ok", g.ToString());
        return true;
    }
    TC_LOG_DEBUG("playerbot.v2", "[Population] login existing {} failed: {}", g.ToString(), r.reason);
    return false;
}

uint64 BotPopulationManager::SpawnNew(uint8 level, bool alliance)
{
    uint32& budget = alliance ? spawn_budget_alli_ : spawn_budget_horde_;
    if (!budget) return 0;

    // Pass J: spawn cap by marked-bot count. Once the DB holds ≥1.10×
    // target characters, we already have a pool large enough to satisfy
    // any in-world target via LoginExisting — spawning more just inflates
    // the lifecycle registry forever (each new character = permanent DB
    // row + permanent setup-pipeline work + permanent reload-on-boot).
    // 10% overhead per the user's brief: leaves room for distribution
    // shape changes (HordePct, MaxHeavyFactor) without immediate spawning.
    // When the cap trips, the cycle still runs LoginExisting which brings
    // offline chars back in-world; spawn only resumes after target grows
    // or characters are explicitly deleted.
    auto& cfg = Services::Config();
    const uint32 total_target = cfg.population_total_target();
    if (total_target > 0)
    {
        const uint32 spawn_cap = uint32(float(total_target) * 1.10f);
        const uint32 marked    = uint32(Services::Lifecycle().size());
        if (marked >= spawn_cap)
        {
            // Quiet log: spam once per cycle would dominate the [Population]
            // log line below. Just no-op.
            return 0;
        }
    }

    // LP-P1a: roll the (race, class, gender) tuple from BotComposition's
    // WEIGHTED census tables, constrained only by faction. The previous
    // version pre-selected a race via `budget % races.size()` and passed it
    // as a race_hint — that forced a near-uniform race distribution (each
    // faction's 14 races picked round-robin off the budget counter),
    // completely bypassing the per-race census weights in
    // BotComposition.cpp. Result: the world had ~equal numbers of every
    // race instead of the Blizzlike skew (e.g. far more Humans/Blood Elves
    // than Mechagnomes/Vulpera).
    //
    // BotComposition::Roll has no faction parameter — when called with no
    // hints it rolls faction 50/50 internally then applies PickRace(faction)
    // (the weighted table). So we constrain faction the only way the public
    // API allows: re-roll the weighted pick and reject any whose race is on
    // the wrong faction, using the existing RaceIsAlliance() helper. This
    // preserves the intra-faction census weighting (unlike a race_hint) while
    // still guaranteeing a faction-correct character. Each Roll already
    // succeeds ~50% on faction match, so the expected attempt count is ~2;
    // the cap of 8 makes the worst case bounded without a hard failure mode.
    BotComposition::Pick pick{};
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        BotComposition::Pick candidate = BotComposition::Roll();
        if (!candidate.race)
            continue;
        if (RaceIsAlliance(candidate.race) == alliance)
        {
            pick = candidate;
            break;
        }
    }
    if (!pick.race)
    {
        TC_LOG_DEBUG("playerbot.v2", "[Population] Roll produced no faction-matching pick (faction={})",
                     alliance ? "A" : "H");
        return 0;
    }

    auto created = BotCharacterFactory::Create(/*ownerSession=*/nullptr,
        pick.name, pick.race, pick.cls, pick.gender);
    if (!created.ok)
    {
        TC_LOG_DEBUG("playerbot.v2", "[Population] spawn failed: {}", created.reason);
        return 0;
    }

    --budget;

    uint64 guid_low = created.guid.GetCounter();

    // Mark distribution_level so the setup pipeline knows what to set.
    CharacterDatabase.PExecute(
        "UPDATE playerbot_v2_character SET distribution_level={} WHERE character_guid_low={}",
        uint32(level), guid_low);

    // Defer LoginBot one world tick. SaveToDB(true) inside Create commits
    // asynchronously - calling LoginBot now would race the async worker
    // and the LoginQueryHolder would see "not found in table characters".
    // OnWorldTick drains deferred_logins_ at the start of every tick; by
    // then the INSERT has landed and LoadFromDB succeeds.
    deferred_logins_.push_back(guid_low);
    return guid_low;
}

uint32 BotPopulationManager::LogoutLRUMany(uint8 lo, uint8 hi, bool alliance, uint32 count)
{
    // Pass G + I: batch logout, scanning live registry only. Originally
    // LogoutLRU walked the full lifecycle (~10K-11K marked) and returned
    // the FIRST match — Reconcile called it N times for N excess bots,
    // doing N × 10K = 2M iterations per cycle. Pass G batched the scan
    // to find up to `count` victims in ONE pass; Pass I switched the
    // scan target from lifecycle to live registry (in-world only), so
    // we iterate the actual logout-eligible pool (200-3000 typical)
    // instead of every bot ever spawned.
    if (count == 0) return 0;
    std::vector<ObjectGuid> victims;
    victims.reserve(count);
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (victims.size() >= count) return;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) return;
        if (p->GetLevel() < lo || p->GetLevel() > hi) return;
        if ((p->GetTeam() == ALLIANCE) != alliance) return;
        if (p->GetGroup()) return;
        if (p->GetMap() && p->GetMap()->IsDungeon()) return;
        if (p->GetMap() && p->GetMap()->IsBattlegroundOrArena()) return;
        // Spare BG/LFG-queued bots (JIT match-fill overshoot) — same
        // rationale as the Reconcile overflow kick and hygiene pass.
        if (p->InBattlegroundQueue()) return;
        if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
        if (IsKickProtected(id, getMSTime())) return;
        // Spare altbots: a real player's alt is under that player's
        // control, not the population shaper's — kicking it yanks a
        // character out of the owner's squad. Owners log them out.
        if (IsPlayerOwnedAlt(id, p)) return;
        if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out
        victims.push_back(g);
    });
    for (ObjectGuid v : victims)
        Services::SessionMgr().LogoutBot(v);
    return uint32(victims.size());
}

bool BotPopulationManager::LogoutLRU(uint8 lo, uint8 hi, bool alliance)
{
    // Single-victim wrapper kept for any direct callers; new code should
    // prefer LogoutLRUMany for overshoot loops.
    return LogoutLRUMany(lo, hi, alliance, 1) > 0;
}

void BotPopulationManager::SessionRhythmLogout(uint32 now_ms)
{
    ConfigReader const& cfg = Services::Config();
    if (!cfg.session_rhythm_enabled()) return;
    const float mult = cfg.session_rhythm_multiplier();
    if (mult <= 0.0f) return;                 // <=0 disables the layer
    const uint32 cap = cfg.session_rhythm_max_logouts_per_cycle();
    if (cap == 0) return;

    // 24h ceiling so an extreme archetype*multiplier can't overflow the uint32
    // ms budget (uint16 minutes * 60000 * mult). Real sessions are << 24h.
    constexpr double kBudgetCeilMs = 86'400'000.0;

    auto& reg = Services::Registry();
    std::vector<ObjectGuid> expired;
    expired.reserve(cap);
    std::unordered_set<uint64> seen;          // online set, for stale-entry prune
    seen.reserve(session_start_ms_.size() + 64);

    reg.for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) return;
        seen.insert(uint64(id));

        // Eligibility — never yank a bot out of an active social/content
        // commitment; a human finishes the dungeon/BG/group before logging
        // off. Mirrors the LogoutLRUMany guard set exactly.
        if (p->GetGroup()) return;
        if (p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())) return;
        if (p->InBattlegroundQueue()) return;
        if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
        if (IsKickProtected(id, now_ms)) return;
        if (IsPlayerOwnedAlt(id, p)) return;  // owner's alt — not ours to log off
        if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out

        // Per-bot session budget from the (immutable-after-login) archetype.
        BotAI* ai = reg.ai(id);
        const uint16 base_min = ai ? ai->archetype().target_session_minutes : 90;

        auto it = session_start_ms_.find(uint64(id));
        if (it == session_start_ms_.end())
        {
            // First sighting: stagger the synthetic session start UNIFORMLY
            // back across one session window so the already-online boot cohort
            // expires spread out, not all on the first cycle after this layer
            // goes live.
            const uint32 budget_ms = uint32(std::min(
                double(base_min) * 60000.0 * double(mult), kBudgetCeilMs));
            const uint32 stagger = budget_ms ? (MixHash64to32(uint64(id)) % budget_ms) : 0;
            session_start_ms_[uint64(id)] = now_ms - stagger;
            return;
        }

        if (expired.size() >= cap) return;    // budget spent; leftovers next cycle
        const uint32 login_ms = it->second;
        // Deterministic +/-10% jitter per (bot, session) so a cohort that all
        // logged in together still expires staggered. Salt with the login
        // stamp so the SAME bot draws fresh jitter on its next session.
        const uint32 jitter_pct = 90u + (MixHash64to32(uint64(id) ^ uint64(login_ms)) % 21u);
        const uint32 budget_ms = uint32(std::min(
            double(base_min) * 60000.0 * double(mult) * double(jitter_pct) / 100.0, kBudgetCeilMs));
        if (budget_ms == 0) return;
        if (uint32(now_ms - login_ms) >= budget_ms)
        {
            expired.push_back(g);
            session_start_ms_.erase(it);      // fresh budget on next login
        }
    });

    for (ObjectGuid g : expired)
        Services::SessionMgr().LogoutBot(g);

    // Opportunistic prune: drop stamps for bots no longer online (logged out
    // by overflow/hygiene/owner) so the map can't grow past the live roster.
    if (session_start_ms_.size() > seen.size() + 256)
        for (auto i = session_start_ms_.begin(); i != session_start_ms_.end();)
            i = seen.count(i->first) ? std::next(i) : session_start_ms_.erase(i);

    if (!expired.empty())
        TC_LOG_INFO("playerbot.v2",
            "[Population] session-rhythm: {} bots ended their play session "
            "(cap={}, mult={:.2f}) — roster will rotate as reconcile refills",
            uint32(expired.size()), cap, mult);
}

void BotPopulationManager::ProtectFromKick(uint64 guid_low, uint32 duration_ms)
{
    const uint32 now = getMSTime();
    kick_protect_until_ms_[guid_low] = now + duration_ms;
    // Opportunistic expiry sweep — the map only ever holds the last few
    // JIT batches, but without pruning it grows for the process lifetime.
    if (kick_protect_until_ms_.size() > 256)
        for (auto it = kick_protect_until_ms_.begin(); it != kick_protect_until_ms_.end();)
            it = (int32(it->second - now) < 0) ? kick_protect_until_ms_.erase(it) : std::next(it);
}

bool BotPopulationManager::IsKickProtected(uint64 guid_low, uint32 now_ms) const
{
    auto it = kick_protect_until_ms_.find(guid_low);
    return it != kick_protect_until_ms_.end() && int32(it->second - now_ms) > 0;
}

bool BotPopulationManager::IsOperatorPersistentBot(uint64 guid_low) const
{
    // See the declaration in the header for the dist_level == 0 invariant.
    // Same world thread as DriveSetupPipelines (the cache's writer), so no
    // lock is needed.
    auto it = pipeline_row_cache_.find(guid_low);
    return it != pipeline_row_cache_.end() && it->second.dist_level == 0;
}

void BotPopulationManager::Reconcile()
{
    const uint32 rec_start = getMSTime();
    // #5 session-rhythm: expire bots whose play session is over FIRST, so the
    // per-bucket fill below logs in replacements within this SAME cycle —
    // online headcount holds at target while WHO is online rotates (the
    // population reads like a living server, not a frozen roster). The post-
    // logout census is what ComputeTargets/PopulateActual then reconcile to.
    SessionRhythmLogout(rec_start);
    const uint32 ct_start = getMSTime();
    auto buckets = ComputeTargets();
    perf_window_compute_targets_ms_ += getMSTimeDiff(ct_start, getMSTime());
    if (buckets.empty()) { perf_window_reconcile_ms_ += getMSTimeDiff(rec_start, getMSTime()); return; }
    const uint32 pa_start = getMSTime();
    PopulateActual(buckets);
    perf_window_populate_actual_ms_ += getMSTimeDiff(pa_start, getMSTime());

    // Per-cycle diagnostic: without this the Population cycle was
    // entirely silent except for hygiene cleanups. Owner asked
    // 2026-05-15 "is population running?" — answer was a DB diff,
    // not a log. Summarize totals per cycle so the log answers
    // the question directly. Spawns/logins are reported by the
    // per-action paths; this is the planning view.
    uint32 sum_alli_actual = 0, sum_alli_target = 0;
    uint32 sum_horde_actual = 0, sum_horde_target = 0;
    for (auto const& b : buckets)
    {
        sum_alli_actual  += b.alliance_actual;
        sum_alli_target  += b.alliance_target;
        sum_horde_actual += b.horde_actual;
        sum_horde_target += b.horde_target;
    }
    TC_LOG_INFO("playerbot.v2",
        "[Population] reconcile buckets={} alli={}/{} horde={}/{} "
        "spawn_budget=A{}/H{} login_budget=A{}/H{}",
        uint32(buckets.size()),
        sum_alli_actual, sum_alli_target,
        sum_horde_actual, sum_horde_target,
        spawn_budget_alli_, spawn_budget_horde_,
        login_budget_alli_, login_budget_horde_);

    // Per-bucket reconcile, BUT each side draws only from its own faction's
    // budget — so Alliance can no longer starve Horde (or vice versa) within
    // a cycle. See the budget reset at OnWorldTick for the split formula.
    auto reconcile_side = [&](PopulationBucket& b, bool alliance) {
        uint32 const actual = alliance ? b.alliance_actual : b.horde_actual;
        uint32 const target = alliance ? b.alliance_target : b.horde_target;
        uint32& login_budget = alliance ? login_budget_alli_ : login_budget_horde_;
        uint32& spawn_budget = alliance ? spawn_budget_alli_ : spawn_budget_horde_;
        if (actual < target)
        {
            uint32 needed = target - actual;
            const uint32 le_start = getMSTime();
            auto offline = FindOfflineCandidatesIn(b.level_lo, b.level_hi, alliance,
                                                   std::min(needed, login_budget));
            for (auto const& c : offline)
            {
                if (!login_budget) break;
                if (LoginExisting(c.char_guid, alliance)) { if (needed) --needed; }
            }
            perf_window_login_existing_ms_ += getMSTimeDiff(le_start, getMSTime());
            const uint32 sn_start = getMSTime();
            while (needed > 0 && spawn_budget > 0)
            {
                uint8 lvl = (b.level_lo + b.level_hi) / 2;
                if (SpawnNew(lvl, alliance)) --needed;
                else break;
            }
            perf_window_spawn_new_ms_ += getMSTimeDiff(sn_start, getMSTime());
        }
        else if (actual > target)
        {
            uint32 excess = actual - target;
            const uint32 ll_start = getMSTime();
            // Single registry scan finds up to `excess` victims and logs
            // them all out — replaces the prior N × full-scan loop.
            LogoutLRUMany(b.level_lo, b.level_hi, alliance, excess);
            perf_window_logout_lru_ms_ += getMSTimeDiff(ll_start, getMSTime());
        }
    };

    const uint32 rs_start = getMSTime();
    for (auto& b : buckets)
    {
        reconcile_side(b, /*alliance=*/true);
        reconcile_side(b, /*alliance=*/false);
    }
    perf_window_reconcile_side_ms_ += getMSTimeDiff(rs_start, getMSTime());

    // B-10: global TotalTarget enforcement at Reconcile cadence. The
    // per-bucket loop above only sees bots whose level falls inside a
    // bucket range — anything outside every bucket was invisible here,
    // and the only global check was hygiene (hourly, 1.5× hard cap), so
    // a boot-time login overshoot persisted for up to an hour and a
    // ≤1.5× steady-state overshoot persisted forever. Kick the overflow
    // now, with the same content-engagement guards as the hygiene pass
    // (never yank a bot out of a group, dungeon/raid, or BG/arena).
    {
        const uint32 total_target = Services::Config().population_total_target();
        // Count only population-shaper inventory: altbots ride on top of
        // TotalTarget (see PopulateActual), and BG/LFG-queued + kick-
        // protected JIT match-fill bots are deliberate overshoot — if any
        // of them inflated `online`, the kick budget would land on
        // innocent POOL bots instead.
        const uint32 tally_now = getMSTime();
        uint32 online = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player const* p = ObjectAccessor::FindConnectedPlayer(g);
            if (p && p->InBattlegroundQueue()) return;
            if (p && sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
            if (IsKickProtected(id, tally_now)) return;
            if (IsPlayerOwnedAlt(id, p)) return;
            if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — off the shaper's books
            ++online;
        });
        if (total_target > 0 && online > total_target)
        {
            const uint32 to_kick = online - total_target;
            std::vector<ObjectGuid> overflow;
            overflow.reserve(to_kick);
            Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
            {
                if (overflow.size() >= to_kick) return;
                ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
                Player* p = ObjectAccessor::FindConnectedPlayer(g);
                if (!p) return;
                if (p->GetGroup()) return;
                // Queued bots are intentional overshoot: BotQueueFiller
                // JIT-spawns them above TotalTarget to form BG matches and
                // LFG groups, and until the port/proposal they have no
                // group and sit on a normal map — kicking them here would
                // strangle autonomous match seeding. Hygiene reclaims them
                // after the run.
                if (p->InBattlegroundQueue()) return;
                if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
                // JIT match-fill bots between login and queue-join hold a
                // kick-protection lease (see ProtectFromKick).
                if (IsKickProtected(id, getMSTime())) return;
                // Altbots are a real player's squad members, not
                // population-shaper inventory — never kick them.
                if (IsPlayerOwnedAlt(id, p)) return;
                if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out
                if (Map* m = p->GetMap())
                {
                    if (m->IsDungeon()) return;
                    if (m->IsBattlegroundOrArena()) return;
                }
                overflow.push_back(g);
            });
            if (!overflow.empty())
            {
                TC_LOG_INFO("playerbot.v2",
                    "[Population] reconcile: online {} > target {}; logging out {} overflow bot(s)",
                    online, total_target, uint32(overflow.size()));
                for (ObjectGuid g : overflow)
                    Services::SessionMgr().LogoutBot(g);
            }
        }
    }
    perf_window_reconcile_ms_ += getMSTimeDiff(rec_start, getMSTime());
}

void BotPopulationManager::RegisterPrioritySetup(uint64 bot_id)
{
    if (bot_id == 0) return;
    std::lock_guard<std::mutex> lk(priority_mtx_);
    priority_setup_ids_.insert(bot_id);
}

void BotPopulationManager::UnregisterPrioritySetup(uint64 bot_id)
{
    if (bot_id == 0) return;
    std::lock_guard<std::mutex> lk(priority_mtx_);
    priority_setup_ids_.erase(bot_id);
}

void BotPopulationManager::DriveSetupPipelines()
{
    // 1-second throttle: this used to fire every world frame (50Hz),
    // and on a cold cache that meant 10 sync `CharacterDatabase.PQuery`
    // reads × 50Hz = 500 sync DB roundtrips/sec on the WORLD thread.
    // Pipelines aren't latency-critical (they run during multi-tick
    // setup teleport sequences); 1-second cadence is plenty and
    // keeps the world thread off the DB worker. Perf audit
    // a2155f31bccccc961 ranked this as catastrophic-on-cold-cache.
    constexpr uint32 kDrivePipelinesIntervalMs = 1000u;
    const uint32 now = getMSTime();
    if (last_drive_pipelines_ms_ &&
        (now - last_drive_pipelines_ms_) < kDrivePipelinesIntervalMs)
        return;
    last_drive_pipelines_ms_ = now;

    // Priority pass (2026-05-21): bots registered via RegisterPrioritySetup
    // get drained FIRST and without the per-tick budget cap. Existed to fix
    // `.playerbot smoketest` under 1000-bot load: the normal Pass I cap of
    // 10/tick starved freshly-spawned smoketest bots indefinitely because
    // they sit at the tail of the registry's hash order. Operates on a
    // copy of the set so the world thread doesn't hold the mutex across
    // the per-bot work (pipeline RunFor calls into DB, teleport, talents,
    // etc.).
    std::vector<uint64> priority_snapshot;
    {
        std::lock_guard<std::mutex> lk(priority_mtx_);
        priority_snapshot.reserve(priority_setup_ids_.size());
        for (uint64 id : priority_setup_ids_) priority_snapshot.push_back(id);
    }
    for (uint64 id : priority_snapshot)
    {
        if (setup_done_cache_.contains(id)) continue;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) continue;  // not in-world yet — re-check next tick
        // Cold-resolve the row state for this bot — same logic as the
        // general pass but without the cap. Cheap because priority set is
        // small (smoketest count, typically <50).
        uint8 pri_state = 0;
        uint8 pri_dist  = 0;
        std::string pri_tag;
        auto pc = pipeline_row_cache_.find(id);
        if (pc != pipeline_row_cache_.end())
        {
            pri_state = pc->second.state;
            pri_dist  = pc->second.dist_level;
            pri_tag   = pc->second.jit_tag;
        }
        else
        {
            auto pres = CharacterDatabase.PQuery(
                "SELECT setup_pipeline_state, distribution_level, IFNULL(jit_for_queue, '') "
                "FROM playerbot_v2_character WHERE character_guid_low={}", id);
            if (!pres || !pres->GetRowCount()) continue;
            Field* pf = pres->Fetch();
            pri_state = pf[0].GetUInt8();
            pri_dist  = pf[1].GetUInt8();
            pri_tag   = pf[2].GetString();
            pipeline_row_cache_.emplace(id,
                PipelineRow{ pri_state, pri_dist, pri_tag });
        }
        bool pri_done = (pri_state == 0xFF);
        bool pri_complete = false;
        if (!pri_done && pri_dist > 0)
            pri_complete = pipeline_->RunFor(p, pri_dist);
        else if (pri_done)
            pri_complete = true;
        if (pri_complete)
        {
            setup_done_cache_.insert(id);
            // Auto-unregister — the caller (smoketest) doesn't have to
            // clean up after itself when AllDone is reached. Removal
            // here makes the priority set self-pruning.
            std::lock_guard<std::mutex> lk(priority_mtx_);
            priority_setup_ids_.erase(id);
        }
    }

    // Pass I: walk Services::Registry() (live in-world bots) instead of
    // Services::Lifecycle() (all marked, ~11K historical accumulation).
    // The lifecycle registry never decrements on logout — each spawn
    // adds an entry that persists forever — so at scale it scanned 11K
    // ids per second to find ~10 in-world non-cached bots. Live registry
    // is bounded by the actual in-world bot count (200-3000 typical) and
    // its entries are by definition online (no FindConnectedPlayer race).
    int driven = 0;
    std::vector<uint64> live_ids;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        live_ids.push_back(uint64(id));
    });
    for (auto id : live_ids)
    {
        if (driven >= 10) break;  // throttle per tick
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) continue;
        // In-memory short-circuit: if RunFor returned true earlier for this
        // bot, skip the DB roundtrip entirely. Without this, the async
        // PersistState write hasn't committed by the time we re-read state
        // here, so we'd re-enter the pipeline and re-fire DoPlaceAndTravel's
        // teleport every tick - the cause of the recent map-worker freezes.
        if (setup_done_cache_.contains(uint64(id))) continue;

        // Pass H: in-memory cache of the pipeline row. With many bots in
        // setup state at scale, the outer SELECT was firing ~10/sec on
        // the world thread — 50-150 ms/sec just on this readback. The
        // cache stores (state, dist_level, jit_tag) keyed by guid;
        // populated lazily on first observation and refreshed only when
        // RunFor reports a state change. RunFor itself also reads/writes
        // state internally — that's a separate hot path tracked under
        // pipelines and not eliminated by this cache.
        uint8 state = 0;
        uint8 dist = 0;
        std::string jit_tag;
        auto cached_it = pipeline_row_cache_.find(uint64(id));
        if (cached_it != pipeline_row_cache_.end())
        {
            state   = cached_it->second.state;
            dist    = cached_it->second.dist_level;
            jit_tag = cached_it->second.jit_tag;
        }
        else
        {
            // Cold load — one sync SELECT, then cache forever (state
            // mutations come back through PersistState which we also
            // mirror to the cache below).
            auto res = CharacterDatabase.PQuery(
                "SELECT setup_pipeline_state, distribution_level, IFNULL(jit_for_queue, '') "
                "FROM playerbot_v2_character WHERE character_guid_low={}", uint64(id));
            if (!res || !res->GetRowCount()) continue;
            Field* f = res->Fetch();
            state   = f[0].GetUInt8();
            dist    = f[1].GetUInt8();
            jit_tag = f[2].GetString();
            pipeline_row_cache_.emplace(uint64(id),
                PipelineRow{ state, dist, jit_tag });
        }
        bool already_done = (state == 0xFF);

        // Run pipeline. Returns true when complete (whether just-completed or already-done).
        bool complete = false;
        if (!already_done && dist > 0)
        {
            complete = pipeline_->RunFor(p, dist);
            if (complete) ++driven;
        }
        else if (!already_done && dist == 0 && BotNeedsStarterOnly(p))
        {
            // Start-level Dracthyr/DK with no distribution bucket: run the
            // starter quest chain + relocate to capital (kept at start level).
            // Count every call toward the throttle — each does a quest reward
            // (DB write), and there can be hundreds of these bots.
            complete = pipeline_->RunStarterOnly(p);
            ++driven;
        }
        else if (already_done)
        {
            complete = true;
        }

        // Cache once we know the pipeline is finished. Covers both "we just
        // completed it" and "DB already says done" branches.
        if (complete) setup_done_cache_.insert(uint64(id));

        // Phase D follow-up: if this bot was JIT-spawned for a queue and the
        // setup pipeline is now complete, push the matching queue intent and
        // clear the JIT tag so we fire once and only once.
        // Tag format set by BotQueueFiller: "<KIND>:<INSTANCE_ID>:<ROLE>"
        //   e.g. "BG:128:DPS", "LFG:271:HEAL"
        if (complete && !jit_tag.empty())
        {
            // Parse "<KIND>:<id>:<role>"
            auto first_colon = jit_tag.find(':');
            auto last_colon  = jit_tag.rfind(':');
            if (first_colon != std::string::npos && last_colon != first_colon)
            {
                std::string kind = jit_tag.substr(0, first_colon);
                uint32 instance_id = uint32(std::strtoul(
                    jit_tag.substr(first_colon + 1, last_colon - first_colon - 1).c_str(),
                    nullptr, 10));
                std::string role_str = jit_tag.substr(last_colon + 1);
                Role role = (role_str == "TANK") ? Role::Tank
                          : (role_str == "HEAL") ? Role::Healer
                          : Role::Dps;

                // Spec alignment: pick the right spec id for (class, role)
                // and ActivateTalentGroup so the bot enters the queue with
                // the matching role's talents. Without this, a JIT-tagged
                // "Tank Warrior" might be queued as Arms (default first
                // spec), failing role validation in dungeon finder.
                if (role != Role::Dps)
                {
                    uint32 target_spec = 0;
                    // Canonical role→spec mapping in Bot/ClassTables.cpp.
                    // Single source for all role-switching call sites.
                    uint8 cls = p->GetClass();
                    target_spec = (role == Role::Tank)
                                  ? TankSpecForClass(cls)
                                  : HealerSpecForClass(cls);
                    if (target_spec != 0 && uint32(AsUnderlyingType(p->GetPrimarySpecialization())) != target_spec)
                    {
                        if (auto const* spec_entry = sChrSpecializationStore.LookupEntry(target_spec))
                            p->ActivateTalentGroup(spec_entry);
                    }
                }

                Intent it{};
                it.bot_id = id;
                if (kind == "BG")
                    it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(instance_id)}};
                else
                    it.body = QueueIntent{LfgQueueIntent{instance_id, role}};
                Services::Intents(id).push(std::move(it));

                // Owner directive 2026-06-22: a JIT BG bot must do ONLY its purpose.
                // Flag it so idle:bg_jit_staging confines it to queue->wait->port
                // (no questing/roaming) — which also keeps it queued+ready so the
                // match can actually form. LFG/dungeon JIT uses the existing LFG
                // proposal flow; BG is the one that needed confinement.
                if (kind == "BG")
                    if (BotAI* jit_ai = Services::Registry().ai(id))
                        jit_ai->set_bg_jit_purpose(uint16(instance_id), getMSTime());

                // Telemetry: JIT path took ~setup-pipeline-time + 1-2 ticks.
                // We don't have the original Filler::Fill timestamp on hand,
                // so approximate via a fixed 2000ms estimate. A future
                // refinement can persist the start_ms in the jit_for_queue
                // tag itself. Good enough for "verify it's <10s in practice".
                Services::Perf().record_queue_fill_completion(Ms{2000});

                TC_LOG_INFO("playerbot.v2",
                    "[Population] JIT setup complete for {} L{}; pushed {} queue intent (instance={}, role={})",
                    p->GetName(), uint32(p->GetLevel()), kind, instance_id, role_str);
            }
            CharacterDatabase.PExecute(
                "UPDATE playerbot_v2_character SET jit_for_queue=NULL, last_seen_at=NOW() "
                "WHERE character_guid_low={}", uint64(id));
            // Mirror the JIT clear into the cache so we don't re-fire the
            // queue intent on the next pipeline tick.
            auto cit = pipeline_row_cache_.find(uint64(id));
            if (cit != pipeline_row_cache_.end()) cit->second.jit_tag.clear();
        }
        // After a successful completion, lock the cache's state to AllDone
        // so subsequent ticks see the same state without re-SELECT. setup_done_cache_
        // also gates the per-call work above; this just keeps both caches consistent.
        if (complete)
        {
            auto cit = pipeline_row_cache_.find(uint64(id));
            if (cit != pipeline_row_cache_.end()) cit->second.state = 0xFF;
        }
    }
}

uint32 BotPopulationManager::ms_until_next_reconcile(uint32 now_ms) const
{
    if (last_tick_ms_ == 0) return 0;
    const uint32 next = last_tick_ms_ + kTickIntervalMs;
    return now_ms >= next ? 0u : (next - now_ms);
}

uint32 BotPopulationManager::ms_until_next_hygiene(uint32 now_ms) const
{
    if (last_hygiene_ms_ == 0) return 0;
    const uint32 next = last_hygiene_ms_ + kHygieneIntervalMs;
    return now_ms >= next ? 0u : (next - now_ms);
}

uint32 BotPopulationManager::ms_until_next_rebalance(uint32 now_ms) const
{
    if (last_rebalance_ms_ == 0) return 0;
    const uint32 next = last_rebalance_ms_ + kRebalanceIntervalMs;
    return now_ms >= next ? 0u : (next - now_ms);
}

// Rebalance pass — proactive spec rotation.
//
// Idea: BotQueueFiller already does reactive role coverage (online bots
// match needed role; if not enough, switch hybrid DPS in place; if still
// not enough, JIT-spawn). The reactive path's worst case is JIT-spawning
// 5-15s of pipeline work per missing slot — that latency is what players
// feel when /lfg pops slowly.
//
// This cron pre-empties that path: every 5 min, walk online bots grouped
// by (faction, bracket), count tanks/healers/DPS, and if a bracket has
// fewer than 1 tank or 1 healer per 5 bots, switch one hybrid DPS bot in
// that bracket into the under-rep role. The next /lfg request in that
// bracket finds the right spec already active and skips the conversion
// step (and avoids a JIT spawn entirely if the fleet was already large
// enough but mis-specced).
//
// Constraints:
//   - Per-bracket cap of 1 switch per cycle prevents mass-respec spikes.
//   - Per-bot 60min lockout via ActionKind::DualSpec (already used by
//     idle:dual_spec_switch) prevents flip-flop respec.
//   - Skips bots that are in combat / dungeon / raid / group — switching
//     a bot mid-content would break their group's plan.
//   - Skips bots whose owner has /setrole-pinned a different role —
//     respect explicit owner intent over rebalance heuristics.
void BotPopulationManager::SeedBgMatches(uint32 now_ms)
{
    // Autonomous BG seeding (audit B24). The only BG entry point used to be
    // a REAL player's CMSG queue packet — with zero human players, the whole
    // BG subsystem was dormant all night (0 [QueueFill] lines, wins=0).
    // Every kSeedIntervalMs, if no bot BG is currently active, queue both
    // factions of a rotating bg_type so TC's matchmaker forms a bot-vs-bot
    // match organically (Fill handles both factions + JIT spawn shortfall).
    const int32 matches_cap = sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.Matches", 1);
    if (matches_cap <= 0) return;
    constexpr uint32 kSeedIntervalMs = 3u * 60u * 1000u;
    if (last_bg_seed_ms_ && (now_ms - last_bg_seed_ms_) < kSeedIntervalMs) return;
    last_bg_seed_ms_ = now_ms;

    // Count active bot-relevant BG instances (any status) — same enumeration
    // TopUpActiveBGs uses. Seed only when below the cap.
    int32 active = 0;
    {
        std::unordered_set<uint32> seen_maps;
        for (BattlemasterListXMapEntry const* xmap : sBattlemasterListXMapStore)
        {
            if (!xmap || xmap->MapID <= 0) continue;
            if (!seen_maps.insert(uint32(xmap->MapID)).second) continue;
            for (Battleground* bg : sBattlegroundMgr->GetBGFreeSlotQueueStore(uint32(xmap->MapID)))
                if (bg) ++active;
        }
    }
    if (active > 0)
        bg_seed_match_seen_ = true;   // current type produced a match — rotate next cycle
    if (active >= matches_cap) return;

    // Saturation guard: when plenty of bots are already BG-queued, the
    // matchmaker is only waiting on in-flight JIT setups — re-seeding
    // would JIT-spawn ANOTHER full deficit on top (the filler can't see
    // setups that haven't queued yet). 72 ≈ both sides of a 36/side fill.
    {
        uint32 already_queued = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            if (Player const* p = ObjectAccessor::FindConnectedPlayer(g))
                if (p->InBattlegroundQueue()) ++already_queued;
        });
        if (already_queued >= 72u)
        {
            TC_LOG_INFO("playerbot.v2",
                "[BgSeed] skipped: {} bots already queued (waiting on matchmaker/setups)",
                already_queued);
            return;
        }
    }

    // Rotate through evergreen, bracket-friendly BGs (validated
    // battleground_template ids): WSG, AB, EotS, Twin Peaks, BfG, Kotmogu.
    // Rotation = every normal BG with a live battleground_template row AND
    // a verified data layer.
    //
    // CRITICAL MAP-ID FIX (2026-06-22): WSG and AB exist under TWO
    // BattlemasterList ids each — a LEGACY one bound to the retired map and
    // a MODERN one bound to the live map that actually carries the flag/node
    // GameObjects + the registered BattlegroundScript:
    //   WSG: BML 2  "Warsong Gulch - Classic" -> map 489  (0 GOs, NO script)
    //        BML 1014 "Warsong Gulch"          -> map 2106 (10 GOs, script)
    //   AB : BML 3  "Arathi Basin"             -> map 529  (0 GOs, NO script)
    //        BML 1018 "AB New"                 -> map 2107 (34 GOs, script)
    // (BG map = BattlemasterListXMap[template.ID]; entry teleport lands on
    //  the template's WorldSafeLocs start-loc map. For 489/529 BOTH are the
    //  dead legacy map, so bots reached a live match on an EMPTY field — no
    //  flag GO ever spawned there, so score_delta stayed 0 forever despite
    //  [bgcoord] assigning carriers.) The modern ids land bots on 2106/2107
    //  where the flags exist; BattlegroundScriptMgr::AliasToBaseBg already
    //  maps 1014->WSG and 1018->AB advice. EotS 7 (566), Twin Peaks 108
    //  (726), BfG 120 (761) and Kotmogu 699 (998) are already consistent
    //  (script map == BML map) so they stay on their canonical ids.
    // Silvershard 708 + Seething Shore 894 join once their cart/azerite
    // mechanics are verified live; Deepwind 754 has NO template row on this
    // core (cannot be created).
    static constexpr uint16 kSeedBgTypes[] = { 1014, 1018, 7, 108, 120, 699 };
    // STICKY rotation: advance to the next type only when the previous
    // seed actually produced a match (active>0 observed since). Rotating
    // every cycle regardless (the old behavior) DILUTED the queues — each
    // 3-min cycle pushed a different bg_type while the prior type's JIT
    // setups were still completing, so no single (type, faction, bracket)
    // queue ever reached MinPlayers and the fleet ballooned with stranded
    // JIT bots (observed: 196 in-world, 51 queued across 4 types, 0
    // matches). Re-seeding the SAME type also self-heals: Fill counts the
    // already-queued bots as coverage and JIT-spawns only the remainder.
    if (bg_seed_match_seen_)
    {
        ++bg_seed_rotation_;
        bg_seed_match_seen_ = false;
    }
    // Test harness: pin the seed to ONE BattlemasterList id so a specific BG
    // can be driven and verified in isolation ("one after the other"). 0 (the
    // default) keeps the normal sticky rotation. Read live (sConfigMgr) so it
    // can be changed via `.reload config` without a restart. Useful BMLs:
    // WSG 1014, AB 1018, EotS 7, Twin Peaks 108, BfG 120, Kotmogu 699.
    const int32 forced_bg_type =
        sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.ForceType", 0);
    const uint16 bg_type = forced_bg_type > 0
        ? uint16(forced_bg_type)
        : kSeedBgTypes[bg_seed_rotation_ % std::size(kSeedBgTypes)];

    BotQueueFiller::FillRequest req{};
    req.kind              = BotQueueFiller::QueueKind::Bg;
    req.bracket           = 7;          // max-level bracket: most complete kits
    req.faction           = ALLIANCE;   // Fill runs both factions for Bg kind
    req.instance_id       = bg_type;
    req.requesting_player = nullptr;
    // Pin level + bracket + per-team CAP exactly like the player-triggered path
    // (OnPlayerJoinedBgQueue). Without these the autonomous seed defaulted to
    // target_level 80±15 with NO cap, so NeedsFor(Bg)=36/side overcommitted ~72
    // fresh bots per cycle — far more than the setup pipeline can level+gear before
    // the next cycle. The pool then filled with half-setup sub-65 bots that the
    // level filter rejects and that never converge into a ready, queued L80 roster,
    // so the matchmaker could never assemble a side (live: WSG stuck PREP 0/0,
    // never IN_PROGRESS). Cap to the BG's MaxPlayersPerTeam so each side spawns just
    // enough; reuse of already-built L80 bots then dominates over fresh creation.
    req.target_level_override = 80;
    if (BattlegroundTemplate const* tmpl =
            sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bg_type)))
    {
        if (uint16 const max_per_team = tmpl->GetMaxPlayersPerTeam())
            req.max_total_bots = uint8(std::min<uint16>(max_per_team, 255));
        if (!tmpl->MapIDs.empty())
        {
            if (PVPDifficultyEntry const* diff =
                    DB2Manager::GetBattlegroundBracketById(tmpl->MapIDs.front(),
                                                           BattlegroundBracketId(7)))
            {
                req.bracket_min_level = uint8(std::clamp<int32>(diff->MinLevel, 1, 80));
                req.bracket_max_level = uint8(std::clamp<int32>(diff->MaxLevel, 1, 80));
            }
        }
    }
    BotQueueFiller filler;
    filler.Fill(req);
    TC_LOG_INFO("playerbot.v2",
        "[BgSeed] seeded bg_type={} bracket=7 (active={} cap={})",
        uint32(bg_type), active, matches_cap);
}

void BotPopulationManager::SeedArenaMatches(uint32 now_ms)
{
    // Autonomous arena skirmish seeding. Unlike BGs (which the matchmaker
    // assembles from independent solo queuers via BotQueueFiller), an arena
    // queue entry is a GROUP keyed by team size — so the only way to drive
    // bot-vs-bot skirmishes with zero humans is to FORM two arena_type-sized
    // groups (one per faction) and have each leader queue its group. We do
    // that by publishing one ArenaTeamForming CoordEvent per faction;
    // BotGroupBuilder (bus async consumer) picks the bots, invites, and
    // (after the group forms) the leader emits an arena BgQueueIntent.
    //
    // Gated OFF by default so it never disturbs normal BG/arena play.
    // `Playerbot.Bg.AutoSeed.Arena` = desired number of concurrent seeded
    // skirmishes (0 = disabled). One skirmish needs both factions' groups.
    const int32 arena_cap = sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.Arena", 0);
    if (arena_cap <= 0) return;

    constexpr uint32 kArenaSeedIntervalMs = 3u * 60u * 1000u;
    if (last_arena_seed_ms_ && (now_ms - last_arena_seed_ms_) < kArenaSeedIntervalMs) return;
    last_arena_seed_ms_ = now_ms;

    if (!Services::Initialized()) return;

    // Skirmish is 3v3-ONLY in this core (BattlegroundMgr::IsValidQueueId →
    // ArenaSkirmish requires teamSize == 3v3). 2v2/5v5 are rated-only and need
    // a persistent ArenaTeam bots don't have, so skirmish == 3v3.
    constexpr uint8 kArenaType = 3;

    // Default arena BattlemasterList: All Arenas (6) — the canonical skirmish
    // template the live HandleBattlemasterJoinArena uses (it always queues
    // BATTLEGROUND_AA regardless of which specific arena map the match lands
    // on). Honor Playerbot.Bg.AutoSeed.ForceType if it names an arena BML so
    // the same override that pins BG seeds can pin a specific arena
    // (4=Nagrand, 5=Blade's Edge, 6=AllArenas, 8=Ruins, 10=Sewers, 11=RoV).
    uint16 arena_bml = 6;   // BATTLEGROUND_AA
    const int32 forced = sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.ForceType", 0);
    if (forced > 0)
    {
        if (BattlegroundTemplate const* ftmpl =
                sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(uint16(forced))))
        {
            if (ftmpl->IsArena())
                arena_bml = uint16(forced);
        }
    }

    // Validate the chosen template is a real arena before publishing.
    BattlegroundTemplate const* tmpl =
        sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(arena_bml));
    if (!tmpl || !tmpl->IsArena())
    {
        TC_LOG_INFO("playerbot.v2",
            "[ArenaSeed] skipped: arena_bml={} has no arena template", uint32(arena_bml));
        return;
    }

    // Concurrency / saturation guard. Count bots ALREADY sitting in an arena
    // queue (InBattlegroundQueue counts arena queues by default). One seeded
    // skirmish parks 2*kArenaType bots in queue; don't pile beyond the cap.
    uint32 arena_queued = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (Player const* p = ObjectAccessor::FindConnectedPlayer(g))
            if (p->InBattlegroundQueue(/*ignoreArena*/ false) && !p->InBattleground())
                ++arena_queued;
    });
    const uint32 needed_per_skirmish = uint32(kArenaType) * 2u;
    if (arena_queued >= uint32(arena_cap) * needed_per_skirmish)
    {
        TC_LOG_INFO("playerbot.v2",
            "[ArenaSeed] skipped: {} bots already in BG/arena queue (cap={} skirmishes)",
            arena_queued, arena_cap);
        return;
    }

    // Eligible-idle-bot counter for a faction + level band. Mirrors
    // BotGroupBuilder's picker eligibility (online, in-world, alive, ungrouped,
    // in level band, not in combat, not deserter, not already queued) so we
    // don't fire a request the builder will then fail to satisfy.
    auto count_eligible = [&](uint8 faction_bit, uint8 lo, uint8 hi) -> uint32
    {
        uint32 n = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            Player* p = ObjectAccessor::FindConnectedPlayer(
                ObjectGuid::Create<HighGuid::Player>(id));
            if (!p || !p->IsInWorld() || !p->IsAlive() || p->GetGroup()) return;
            if (p->IsInCombat() || p->IsDeserter()) return;
            if (p->InBattleground() || p->InBattlegroundQueue(/*ignoreArena*/ false)) return;
            const uint8 lvl = p->GetLevel();
            if (lvl < lo || lvl > hi) return;
            // Race-based team to match BotGroupBuilder::PickCandidates'
            // faction filter (it uses Player::TeamForRace, not GetTeam()).
            const bool is_alliance =
                (Player::TeamForRace(p->GetRace()) == ALLIANCE);
            const uint8 bit = is_alliance ? 0x1 : 0x2;
            if (bit != faction_bit) return;
            ++n;
        });
        return n;
    };

    // ADAPTIVE bracket (2026-06-23): the fixed max-level (80) band is empty in a
    // mostly-leveling fleet, so scan TC's 10-level PvP bands high->low and seed
    // the first where BOTH factions have >=kArenaType idle bots. Bands align to
    // TC bracket boundaries so a formed group shares one matchmaking bracket.
    struct Band { uint8 lo, hi; };
    static constexpr Band kBands[] = {
        {80,80},{70,79},{60,69},{50,59},{40,49},{30,39},{20,29},{10,19}};
    uint8 bracket_min = 0, bracket_max = 0;
    uint32 alli_ok = 0, horde_ok = 0;
    for (Band const& b : kBands)
    {
        const uint32 a = count_eligible(0x1, b.lo, b.hi);
        const uint32 h = count_eligible(0x2, b.lo, b.hi);
        if (a >= kArenaType && h >= kArenaType)
        { bracket_min = b.lo; bracket_max = b.hi; alli_ok = a; horde_ok = h; break; }
    }
    if (bracket_min == 0)
    {
        TC_LOG_INFO("playerbot.v2",
            "[ArenaSeed] skipped: no level band has >={}/side idle bots (arena_bml={})",
            uint32(kArenaType), uint32(arena_bml));
        return;
    }

    // Publish one ArenaTeamForming per faction. BotGroupBuilder forms a
    // kArenaType-sized group for each and the leader queues it for the arena
    // skirmish via the Arena queue id. The two groups meet in the matchmaker.
    auto publish_side = [&](uint8 faction_bit, char const* label)
    {
        CoordEvent ev{};
        ev.kind         = CoordSignal::ArenaTeamForming;
        ev.origin_low   = 0;            // system-initiated
        ev.content_id   = arena_bml;    // arena BattlemasterList id
        ev.level_min    = bracket_min;
        ev.level_max    = bracket_max;
        ev.faction_mask = faction_bit;  // bit0=Alliance, bit1=Horde
        ev.arena_type   = kArenaType;
        ev.content_name = label;
        Services::Coordination().Publish(ev);
    };
    publish_side(0x1, "Arena skirmish (Alliance)");
    publish_side(0x2, "Arena skirmish (Horde)");

    TC_LOG_INFO("playerbot.v2",
        "[ArenaSeed] seeded {}v{} arena_bml={} bracket={}-{} (eligible alli={} horde={} queued={} cap={})",
        uint32(kArenaType), uint32(kArenaType), uint32(arena_bml),
        uint32(bracket_min), uint32(bracket_max), alli_ok, horde_ok, arena_queued, arena_cap);
}

// True if a BG contains at least one REAL human player (non-pool account).
// Bots ride on pool accounts; a human is anyone whose session account is not a
// pool account. Used to ensure the top-up only SUPPORTS human battlegrounds and
// never sustains a bot-only match (which is pure server load with no human
// benefit — owner directive 2026-06-17).
static bool BgHasHumanPlayer(Battleground const* bg)
{
    if (!bg) return false;
    for (auto const& kv : bg->GetPlayers())
    {
        Player const* p = ObjectAccessor::FindPlayer(kv.first);
        if (!p) continue;
        if (WorldSession const* sess = p->GetSession())
            if (!Services::Accounts().is_pool_account(sess->GetAccountId()))
                return true;
    }
    return false;
}

void BotPopulationManager::TopUpActiveBGs(uint32 now_ms)
{
    // Two cadences:
    //   - 5s for STATUS_WAIT_JOIN (prep phase) — fast fill before gates open.
    //   - 30s for STATUS_IN_PROGRESS — replace players that left mid-match
    //     without flooding the queue every 5s.
    // Each timer gates independently so we still hit the fast prep cadence
    // even if a running BG is also live.
    constexpr uint32 kPrepTopUpIntervalMs    = 5000;
    constexpr uint32 kRunningTopUpIntervalMs = 30000;

    bool const prep_due = !last_bg_topup_ms_ ||
        (now_ms - last_bg_topup_ms_) >= kPrepTopUpIntervalMs;
    bool const running_due = !last_bg_running_topup_ms_ ||
        (now_ms - last_bg_running_topup_ms_) >= kRunningTopUpIntervalMs;

    if (!prep_due && !running_due) return;
    if (prep_due)    last_bg_topup_ms_         = now_ms;
    if (running_due) last_bg_running_topup_ms_ = now_ms;

    // Walk BattlemasterListXMap (BattlemasterListID → MapID) → per-map
    // free-slot queue. The free-slot queue contains BG instances that
    // still have capacity (TC's existing FreeSlotQueue mechanism). For
    // each BG below max population in prep OR running, re-issue
    // BotQueueFiller::Fill. Newly-queued bots are auto-pulled into the
    // BG by AddToBGFreeSlotQueue when the next queue cycle runs.
    // Dedup by map_id so we don't process the same map's free-slot store
    // multiple times when several BattlemasterLists share a map.
    std::unordered_set<uint32> seen_maps;
    for (BattlemasterListXMapEntry const* xmap : sBattlemasterListXMapStore)
    {
        if (!xmap) continue;
        if (xmap->MapID <= 0) continue;
        uint32 map_id = uint32(xmap->MapID);
        if (!seen_maps.insert(map_id).second) continue;

        auto& free_slot_store = sBattlegroundMgr->GetBGFreeSlotQueueStore(map_id);
        for (Battleground* bg : free_slot_store)
        {
            if (!bg) continue;
            auto const status = bg->GetStatus();
            bool const is_prep    = (status == STATUS_WAIT_JOIN);
            bool const is_running = (status == STATUS_IN_PROGRESS);
            if (!is_prep && !is_running) continue;
            // Only SUSTAIN a RUNNING battleground that has a human in it. A
            // bot-only running BG (autonomous-seeded, or a human-BG the human
            // left) must NOT be perpetually refilled — skipping its top-up lets
            // it bleed out and end at its time limit instead of running forever
            // as pure server load (owner directive 2026-06-17). PREP BGs are
            // left ungated so a forming HUMAN BG still fills before its gates
            // open (the human is invited but may not have entered m_Players yet).
            if (is_running && !BgHasHumanPlayer(bg)) continue;
            // Respect each phase's own cadence:
            // - Prep BGs only on prep_due ticks (every 5s)
            // - Running BGs only on running_due ticks (every 30s)
            if (is_prep    && !prep_due)    continue;
            if (is_running && !running_due) continue;
            uint32 const max_per_team = bg->GetMaxPlayersPerTeam();
            uint32 const alli  = bg->GetPlayersCountByTeam(ALLIANCE);
            uint32 const horde = bg->GetPlayersCountByTeam(HORDE);
            if (alli >= max_per_team && horde >= max_per_team) continue;

            uint8 const bracket = uint8(bg->GetBracketId());
            // PVPDifficulty bracket ID is NOT level/10. For AV bracket 0
            // corresponds to L51-60, etc. Derive an authoritative target
            // level from the bracket's MaxLevel so the filler matches the
            // right bot-level pool. Falls back to bot-level / 10 * 10 + 5
            // midpoint if the PVPDifficulty lookup fails (very unlikely).
            uint8 target_level_for_bracket = 0;
            uint8 bracket_min = 0, bracket_max = 0;
            if (PVPDifficultyEntry const* diff =
                    DB2Manager::GetBattlegroundBracketById(bg->GetMapId(),
                                                          BattlegroundBracketId(bracket)))
            {
                target_level_for_bracket = uint8(std::min<uint16>(diff->MaxLevel, 80));
                bracket_min = uint8(std::clamp<int32>(diff->MinLevel, 1, 80));
                bracket_max = uint8(std::clamp<int32>(diff->MaxLevel, 1, 80));
            }
            BotQueueFiller filler;

            // online_only=true for PREP (5s cadence — pre-queued bots
            // already in-flight will arrive in time, no need to spawn
            // fresh ones). online_only=false for RUNNING (30s cadence,
            // BG is actively bleeding players; allow JIT-spawn so we
            // backfill from offline pool when the online pool is
            // exhausted). The 30s gate naturally rate-limits the JIT
            // storm to one batch per BG per cycle.
            //
            // max_total_bots = EXACTLY the deficit per faction. Without
            // this, Fill queued 36 candidates per side per cycle and
            // the team count overshot max_per_team — observed WSG with
            // 13 alliance vs 7 horde in a 10v10 (user-strict requirement
            // is "10v10 exactly, no overpopulation"). With the cap, we
            // inject exactly (max_per_team - current_count), the BG
            // never overshoots, and once full we naturally skip.
            bool const allow_jit = is_running;
            // GetFreeSlotsForTeam respects TC's balance gate (the BG won't
            // accept new alliance bots when alliance is over cap or
            // alliance.invited > horde.invited). Without this check we'd
            // keep pushing bots into TC's queue that can never get pulled
            // in — wasted queue churn and confusing diagnostic output.
            // Observed 2026-05-13 07:40: BG running at alli=13/10
            // horde=7/10 with filler still queueing 3 alli tanks/cycle
            // because alli<max_per_team computed off m_PlayersCount which
            // had drifted from the actual cap-enforced view.
            uint32 const alli_free_tc  = bg->GetFreeSlotsForTeam(ALLIANCE);
            uint32 const horde_free_tc = bg->GetFreeSlotsForTeam(HORDE);
            if (alli < max_per_team && alli_free_tc > 0)
            {
                BotQueueFiller::FillRequest req{};
                req.kind = BotQueueFiller::QueueKind::Bg;
                req.bracket = bracket;
                req.faction = ALLIANCE;
                req.instance_id = uint32(bg->GetTypeID());
                req.requesting_player = nullptr;
                req.online_only = !allow_jit;
                req.max_total_bots = uint8(std::min(uint32(max_per_team - alli),
                                                     alli_free_tc));
                if (target_level_for_bracket > 0)
                    req.target_level_override = target_level_for_bracket;
                if (bracket_min > 0 && bracket_max > 0)
                {
                    req.bracket_min_level = bracket_min;
                    req.bracket_max_level = bracket_max;
                }
                filler.Fill(req);
            }
            if (horde < max_per_team && horde_free_tc > 0)
            {
                BotQueueFiller::FillRequest req{};
                req.kind = BotQueueFiller::QueueKind::Bg;
                req.bracket = bracket;
                req.faction = HORDE;
                req.instance_id = uint32(bg->GetTypeID());
                req.requesting_player = nullptr;
                req.online_only = !allow_jit;
                req.max_total_bots = uint8(std::min(uint32(max_per_team - horde),
                                                     horde_free_tc));
                if (target_level_for_bracket > 0)
                    req.target_level_override = target_level_for_bracket;
                if (bracket_min > 0 && bracket_max > 0)
                {
                    req.bracket_min_level = bracket_min;
                    req.bracket_max_level = bracket_max;
                }
                filler.Fill(req);
            }
            // Diagnostic: surface the team-balance state alongside the
            // top-up activity so an imbalanced BG is visible in the log.
            TC_LOG_INFO("playerbot.v2",
                "[BgTopUp] free_slots alli={} horde={} (m_PlayersCount alli={} horde={}, "
                "invited alli={} horde={})",
                alli_free_tc, horde_free_tc, alli, horde,
                bg->GetInvitedCount(ALLIANCE), bg->GetInvitedCount(HORDE));

            TC_LOG_INFO("playerbot.v2",
                "[BgTopUp] phase={} bg_type={} bracket={} alli={}/{} horde={}/{} delay={}ms",
                is_running ? "RUNNING" : "PREP",
                uint32(bg->GetTypeID()), uint32(bracket),
                alli, max_per_team, horde, max_per_team,
                bg->GetStartDelayTime());
        }
    }
}

void BotPopulationManager::RunRebalance(uint32 now_ms)
{
    if (last_rebalance_ms_ && (now_ms - last_rebalance_ms_) < kRebalanceIntervalMs)
        return;
    last_rebalance_ms_ = now_ms;

    if (!Services::Initialized()) return;

    // Group online bots by (faction_alliance, bracket=level/10).
    struct BracketCounts
    {
        std::vector<uint64> tank_ids;   // already in tank spec
        std::vector<uint64> heal_ids;   // already in heal spec
        std::vector<uint64> dps_ids;    // candidates for switch (in DPS spec)
    };
    std::unordered_map<uint32 /*bracket_key*/, BracketCounts> by_bracket;

    auto& id_reg = Services::Lifecycle();   // walk online ids
    auto& ai_reg = Services::Registry();    // ai(id) for ActionKind lockout
    auto ids = id_reg.snapshot_ids();
    for (auto id : ids)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot) continue;
        // Eligibility filters mirror BotQueueFiller's pass-2 rules so
        // rebalance and reactive switching pull from the same eligible set.
        if (bot->IsInCombat()) continue;
        if (bot->GetGroup()) continue;
        if (bot->GetMap() && bot->GetMap()->IsDungeon()) continue;
        // NOTE on altbots: deliberately NOT excluded here. Owner decision
        // 2026-06-10: while an alt serves as a bot the bot system manages
        // its spec (talents/respec) so it stays combat-functional; the
        // player can respec when they log it in personally. Alts ARE
        // excluded from kicks and queue-drafting (those remove the
        // character from the owner's control — different category).

        const uint8 cls = bot->GetClass();
        const uint8 lvl = bot->GetLevel();
        if (lvl < 10) continue;  // pre-spec characters
        const uint32 bracket = lvl / 10u;
        const bool alliance = bot->GetTeam() == ALLIANCE;
        // Pack (faction, bracket) into one key. Bracket is 1..8 so 4 bits suffice.
        const uint32 key = (alliance ? 0x10000u : 0u) | bracket;

        const uint16 spec = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));
        auto& bc = by_bracket[key];
        if (IsTankSpec(cls, spec))        bc.tank_ids.push_back(id);
        else if (IsHealerSpec(cls, spec)) bc.heal_ids.push_back(id);
        else                              bc.dps_ids.push_back(id);
    }

    // Target ratios: 1T:1H:3D per group of 5. Equivalently, want
    // tank_count >= total/5 and heal_count >= total/5 in each bracket.
    // If the bracket is too small for meaningful role coverage (< 5
    // bots), skip — JIT will fill any /lfg there and rebalance churn
    // would be more disruptive than helpful.
    uint32 switched_total = 0;
    for (auto& [key, bc] : by_bracket)
    {
        const size_t total = bc.tank_ids.size() + bc.heal_ids.size() + bc.dps_ids.size();
        if (total < 5) continue;
        const size_t target_each = (total + 4) / 5;  // ceil(total/5)

        auto try_switch_one = [&](Role role) -> bool
        {
            // #4A: prefer the candidate whose ARCHETYPE most wants this role.
            // Rather than respec the first eligible DPS bot (arbitrary), score
            // every eligible candidate by its archetype role_affinity for the
            // target role and switch the highest-affinity one. A HardcoreRaider
            // (high tank/heal affinity) is drafted before a GathererFlipper
            // (pure-DPS affinity), so the fleet's tanks/healers are the bots
            // that "want" to play those roles — fewer reluctant respecs and a
            // more believable role distribution. Eligibility (combat / group /
            // dungeon / lockout / class-can-fill) is unchanged.
            const uint8 role_idx = (role == Role::Tank) ? 0u : 1u;  // role_affinity[Tank,Healer,Dps]

            auto best_it    = bc.dps_ids.end();
            float best_aff  = -1.0f;
            uint32 best_spec = 0;
            for (auto it = bc.dps_ids.begin(); it != bc.dps_ids.end(); ++it)
            {
                BotAI* ai = ai_reg.ai(*it);
                if (!ai) continue;
                const uint64 bot_id = *it;
                ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(bot_id);
                Player* bot = ObjectAccessor::FindConnectedPlayer(g);
                if (!bot) continue;
                // Owner-pinned role check is handled by idle:dual_spec_switch
                // on the bot's own next tick — that rule reads role_override_
                // and switches to the right spec independently. Rebalance
                // doesn't need to read role_override directly; if the owner
                // pinned Tank but rebalance switches to Healer, idle's rule
                // will flip back on the bot's next idle observation, paying
                // the per-bot lockout cost. Net effect: rebalance produces a
                // small amount of churn for owner-pinned bots, capped at one
                // flip per 60min via the lockout. Acceptable.
                // Per-bot lockout via DualSpec ActionKind. Key on
                // `bot_id` so each bot has its own lockout — keying on
                // class would lock out every Warrior for 30min after
                // one Warrior is switched, which is the opposite of
                // what we want for a fleet-wide rebalance.
                if (ai->action_recently_tried(BotAI::ActionKind::DualSpec,
                                              bot_id, now_ms))
                    continue;
                const uint8 cls = bot->GetClass();
                const uint32 target_spec = (role == Role::Tank)
                    ? TankSpecForClass(cls)
                    : HealerSpecForClass(cls);
                if (target_spec == 0) continue;  // class can't fill role
                auto const* spec_entry = sChrSpecializationStore.LookupEntry(target_spec);
                if (!spec_entry) continue;
                const float aff = ai->archetype().role_affinity[role_idx];
                if (aff > best_aff)
                {
                    best_aff  = aff;
                    best_it   = it;
                    best_spec = target_spec;
                }
            }

            if (best_it == bc.dps_ids.end())
                return false;   // no eligible candidate this bracket/role

            const uint64 bot_id = *best_it;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(bot_id);
            Player* bot = ObjectAccessor::FindConnectedPlayer(g);
            BotAI* ai = ai_reg.ai(bot_id);
            if (!bot || !ai) return false;   // raced away between scan and apply
            const uint8 cls = bot->GetClass();
            auto const* spec_entry = sChrSpecializationStore.LookupEntry(best_spec);
            if (!spec_entry) return false;
            bot->ActivateTalentGroup(spec_entry);
            ai->note_action_retry(BotAI::ActionKind::DualSpec, bot_id, now_ms);
            bc.dps_ids.erase(best_it);   // remove from candidate pool
            if (role == Role::Tank) bc.tank_ids.push_back(bot_id);
            else                    bc.heal_ids.push_back(bot_id);
            ++switched_total;
            TC_LOG_INFO("playerbot.v2",
                "[Rebalance] bracket=0x{:x} switched bot {} ({}) to spec {} ({}) affinity={:.2f}",
                key, bot_id, uint32(cls), best_spec,
                role == Role::Tank ? "tank" : "healer", best_aff);
            return true;
        };

        uint32 switches_in_bracket = 0;
        if (bc.tank_ids.size() < target_each &&
            switches_in_bracket < kRebalanceSwitchesPerBracket)
        {
            if (try_switch_one(Role::Tank)) ++switches_in_bracket;
        }
        if (bc.heal_ids.size() < target_each &&
            switches_in_bracket < kRebalanceSwitchesPerBracket)
        {
            if (try_switch_one(Role::Healer)) ++switches_in_bracket;
        }
    }

    if (switched_total > 0)
    {
        TC_LOG_INFO("playerbot.v2",
            "[Rebalance] cycle complete: {} switches across {} brackets",
            switched_total, by_bracket.size());
    }
}

uint32 BotPopulationManager::ForceRebalance()
{
    // Reset the throttle so RunRebalance fires this call. RunRebalance
    // sets last_rebalance_ms_ on entry; we need to walk it backwards
    // first. We can't easily measure "switches" without restructuring
    // the function — log line counts ('[Rebalance] cycle complete: N')
    // give the operator the answer. This helper exists mostly to
    // expose the bypass to /rebalance whisper.
    last_rebalance_ms_ = 0;
    RunRebalance(getMSTime());
    return 0;
}

std::vector<BotPopulationManager::BracketReport> BotPopulationManager::BracketCoverage() const
{
    std::unordered_map<uint32, BracketReport> by_bracket;
    if (!Services::Initialized()) return {};

    auto& id_reg = Services::Lifecycle();
    auto ids = id_reg.snapshot_ids();
    for (auto id : ids)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot) continue;
        const uint8 lvl = bot->GetLevel();
        if (lvl < 10) continue;
        const uint8 cls = bot->GetClass();
        const uint16 spec = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));
        const bool alliance = bot->GetTeam() == ALLIANCE;
        const uint32 key = (alliance ? 0x10000u : 0u) | uint32(lvl / 10u);
        auto& br = by_bracket[key];
        br.bracket_key = key;
        if (IsTankSpec(cls, spec))        ++br.tanks;
        else if (IsHealerSpec(cls, spec)) ++br.healers;
        else                              ++br.dps;
    }
    std::vector<BracketReport> out;
    out.reserve(by_bracket.size());
    for (auto& [k, v] : by_bracket) out.push_back(v);
    // Sort by bracket key so the whisper output reads naturally
    // (alliance / horde grouped via high bit, then bracket low->high).
    std::sort(out.begin(), out.end(),
              [](BracketReport const& a, BracketReport const& b) {
                  return a.bracket_key < b.bracket_key;
              });
    return out;
}

} // namespace Playerbot::V2::Fleet

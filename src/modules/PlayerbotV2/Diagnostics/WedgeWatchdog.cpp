#include "WedgeWatchdog.h"
#include "PerfCounters.h"
#include "Bot/BotAI.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotSnapshot.h"
#include "../Services.h"
#include "../Util/ConfigReader.h"   // #7 self-remediation reads wedge_remediation_* (Services::Config() returns it by ref)
#include "../Threading/SnapshotPublisher.h"

#include "CharacterCache.h"
#include "ObjectGuid.h"
#include "Log.h"
#include "DatabaseEnv.h"   // #7 ledger: CharacterDatabase upsert
#include "fmt/format.h"    // #7 ledger: format the upsert (mirrors fleet_vitals)

#include <cmath>
#include <cstring>
#include <string_view>

namespace Playerbot::Diagnostics {

// Tripwire: PerfCounters' wedge bucket array MUST match the enum cardinality,
// and GoalUnreachable (the last enum value, index 6) must be the highest
// index. If a category is added, bump both kWedgeCategoryCount constants.
static_assert(kWedgeCategoryCount == PerfCounters::kWedgeCategoryCount,
              "WedgeCategory count out of sync with PerfCounters bucket array");
static_assert(static_cast<size_t>(WedgeCategory::NoProgress) ==
              kWedgeCategoryCount - 1,
              "WedgeCategory enum order changed; update kWedgeCategoryCount");

char const* WedgeCategoryName(WedgeCategory c)
{
    switch (c)
    {
        case WedgeCategory::None:            return "None";
        case WedgeCategory::Navmesh:         return "Navmesh";
        case WedgeCategory::OffMesh:         return "OffMesh";
        case WedgeCategory::Travel:          return "Travel";
        case WedgeCategory::CombatLoop:      return "CombatLoop";
        case WedgeCategory::PickerNone:      return "PickerNone";
        case WedgeCategory::GoalUnreachable: return "GoalUnreachable";
        case WedgeCategory::NoProgress:      return "NoProgress";
    }
    return "?";
}

namespace {

// Cheap "does rule name start with prefix" — last_rule_fired() points at a
// stable static string (rotation/idle tables), so a plain strncmp is fine.
bool RulePrefix(char const* rule, std::string_view prefix)
{
    if (!rule) return false;
    return std::strncmp(rule, prefix.data(), prefix.size()) == 0;
}

// Resolve a bot's character name from the cache (world-thread safe). Returns
// "?" when not cached.
std::string ResolveName(BotId id)
{
    ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
    if (CharacterCacheEntry const* ce = sCharacterCache->GetCharacterCacheByGuid(g))
        return ce->Name;
    return "?";
}

} // namespace

// ---------------------------------------------------------------------------
// Classifier.
//
// Disambiguation strategy (uses only cheaply-available world-thread signals):
//
//   last_rule_fired() prefix is the primary discriminator —
//     "combat:*"                      -> CombatLoop  (stuck mid-fight)
//     "idle:walk_to_*" / "*travel*"   -> Travel      (wedged on a hub/anchor leg)
//     "idle:wander" / "idle:unstick"  -> still a movement wedge; fall to the
//                                        navmesh/off-mesh split below
//     "idle:picker_none" / null rule  -> PickerNone  (picker emitted nothing)
//
//   For movement wedges, GoalUnreachable wins when the StuckTracker is anchored
//   on a real QUEST goal (goal_is_quest) — the bot has a concrete objective it
//   can't reach. Otherwise we split Navmesh vs OffMesh:
//
//   OffMesh vs Navmesh heuristic (no per-bot FarFromPoly outcome is cheaply
//   available at this layer — the fine-grained PathOutcome is recorded globally
//   in PerfCounters, not per bot). We treat the wedge as OffMesh when the
//   bot's situation matches the classic off-mesh / seam signature:
//     * the snapshot flags objective_needs_bridge (builder's FindRoute proved
//       the goal needs a non-walk bridge edge — elevator / AT-teleport / intra-
//       map ship; i.e. the naive walk dest is off the reachable mesh), OR
//     * the bot is on a different "floor": indoors flag set while the goal is a
//       far open-world point (the Org zeppelin-deck / den-ledge signature), OR
//     * path_blocked_count has climbed high AND the bot is barely moving (the
//       FarFromPolyStart "drifted off mesh" pattern — recovered only by a
//       NearTeleport snap, which off-mesh wedges keep failing).
//   Everything else (path refused repeatedly on a connected mesh) is Navmesh.
//
// Duration: prefer StuckTracker.first_no_progress_ms; if only the oscillation
// leash tripped, BotAI::wedge_since_ms already folds in osc_anchor_ms_. We take
// the watchdog-agreed onset straight from wedge_since_ms so the digest and the
// log line never disagree.
// ---------------------------------------------------------------------------
WedgeInfo ClassifyWedge(BotAI& ai, BotId id, uint32 stationary_for_ms,
                        uint32 threshold_ms, uint32 combat_threshold_ms, uint32 now_ms)
{
    WedgeInfo wi;
    wi.bot = id;

    // Primary gate: the bot must be PHYSICALLY stationary for >= threshold
    // (stationary_for_ms is measured by the watchdog's position anchor) AND its
    // own stuck signal must agree it isn't progressing (wedge_since_ms != 0).
    // The displacement gate kills the false positives where a bot travelling a
    // winding multi-leg route shows little straight-line goal progress yet moves
    // hundreds of yards; the wedge_since_ms requirement keeps genuinely idle /
    // parked bots (nothing to do, not stuck) out of the report.
    if (stationary_for_ms < threshold_ms)
        return wi;   // still moving (or not parked long enough) — not a wedge
    if (ai.wedge_since_ms(now_ms) == 0)
        return wi;   // physically still but no active goal-progress failure

    wi.since_ms    = now_ms - stationary_for_ms;
    wi.duration_ms = stationary_for_ms;
    wi.objective   = ai.wedge_objective_string();
    char const* lr = ai.last_rule_fired();
    wi.last_rule   = lr ? lr : "(null)";

    // Position / zone + the snapshot-side bridge/objective signals come from
    // the latest published snapshot (eventually-consistent read is fine — the
    // watchdog only needs an approximate location for the operator).
    bool needs_bridge = false;
    bool indoors      = false;
    bool in_combat    = false;
    uint32 snap_path_blocks = 0;
    if (Services::Initialized())
    {
        if (auto snap = Services::Snapshots().latest(id))
        {
            wi.map     = snap->position.map_id;
            wi.x       = snap->position.x;
            wi.y       = snap->position.y;
            wi.z       = snap->position.z;
            wi.zone_id = snap->area.zone_id;
            needs_bridge = snap->quest_log.objective_needs_bridge ||
                           snap->quest_log.objective_is_relocation;
            indoors    = snap->area.is_indoors;
            in_combat  = snap->vitals.in_combat;
            snap_path_blocks = snap->path_telemetry.count;
            // Prefer the live quest id over the bare provenance string when a
            // real quest goal is being chased — gives the operator the exact
            // quest to investigate.
            if (snap->quest_log.current_quest_id != 0)
                wi.objective = "quest=" + std::to_string(snap->quest_log.current_quest_id) +
                               " " + wi.objective;
            else if (snap->quest_log.objective_is_relocation)
                wi.objective = "relocation " + wi.objective;

            // Capture the picker's CURRENT objective identity for self-remediation
            // (see WedgeInfo). current_objective is the actionable row that owns
            // current_objective_poi — its (quest_id, id) is the exact key the
            // snapshot builder checks via objective_blacklisted(q.quest_id, o.id),
            // so blacklisting it actually prevents re-selection. Captured here from
            // the wedge-defining snapshot to avoid the latest()-refetch skew that
            // let current_quest_id flip to a different log quest between classify
            // and remediation. A TURN-IN/BREADCRUMB goal has no actionable row
            // (current_objective.quest_id==0) -> fall back to the current quest with
            // the (quest,0) sentinel the breadcrumb picker honors.
            auto const& co = snap->quest_log.current_objective;
            if (co.quest_id != 0) { wi.wedge_quest_id = co.quest_id; wi.wedge_obj_id = co.id; }
            else { wi.wedge_quest_id = snap->quest_log.current_quest_id; wi.wedge_obj_id = 0; }
        }
    }
    // path_blocked_count() is the authoritative live counter; fall back to the
    // snapshot copy if the AI hasn't been re-read this tick.
    const uint32 path_blocks = ai.path_blocked_count() > 0
                                 ? ai.path_blocked_count()
                                 : snap_path_blocks;

    // --- Primary discriminator: the last rule that fired. ---
    if (in_combat || RulePrefix(lr, "combat:"))
    {
        // Combat legitimately holds position (melee swing / cast / spawn-cluster
        // grind), so a 90s movement gate over-flags normal fights. Require the
        // longer combat gate: a bot stuck fighting one spot this long is a real
        // combat wedge (unkillable dummy / mis-attacked talk-objective / the
        // opener out-of-range loop). Below it, not yet reportable.
        if (stationary_for_ms < combat_threshold_ms)
            return wi;   // cat stays None
        wi.cat = WedgeCategory::CombatLoop;
        return wi;
    }
    // Using a quest item on a target legitimately HOLDS POSITION while the on-use
    // spell casts (cast-time spells like Q26118's sledgehammer). The bot stands at
    // the target on purpose — treating that stillness as a wedge would blacklist the
    // quest and yank the bot away mid-cast (which is exactly what kept Durnan from
    // ever completing the arrest). Never a wedge.
    if (RulePrefix(lr, "idle:quest_use_item"))
        return wi;   // cat stays None
    if (RulePrefix(lr, "idle:walk_to_") || RulePrefix(lr, "idle:travel") ||
        RulePrefix(lr, "idle:fly_to") || RulePrefix(lr, "idle:xport") ||
        RulePrefix(lr, "idle:board") || RulePrefix(lr, "idle:use_areatrigger") ||
        RulePrefix(lr, "idle:walk_to_hub") || RulePrefix(lr, "idle:travel_to_hub"))
    {
        wi.cat = WedgeCategory::Travel;
        return wi;
    }
    // --- Maintenance detour (repair / vendor / sell / restock). ---
    // The bot still carries its quest goal in the StuckTracker while it walks off
    // to a vendor, so without this guard a slow or LoS-blocked maintenance trip
    // would fall through to the GoalUnreachable branch below and the remediation
    // pass would ABANDON a perfectly completable quest (the bot was never stuck on
    // the quest — it chose to repair first). Classify these as a movement wedge so
    // the operator still sees a stuck-at-vendor report, but the quest is never
    // remediated away. A genuinely unreachable vendor surfaces as Travel/Navmesh,
    // not as a false GoalUnreachable.
    if (RulePrefix(lr, "idle:critical_repair") || RulePrefix(lr, "idle:vendor") ||
        RulePrefix(lr, "idle:walk_to_known_vendor") || RulePrefix(lr, "idle:repair") ||
        RulePrefix(lr, "idle:sell") || RulePrefix(lr, "idle:restock"))
    {
        wi.cat = WedgeCategory::Travel;
        return wi;
    }
    if (lr == nullptr || RulePrefix(lr, "idle:picker_none") ||
        RulePrefix(lr, "idle:none"))
    {
        wi.cat = WedgeCategory::PickerNone;
        return wi;
    }

    // --- Movement wedge (wander / unstick / quest_path / corpse / vendor). ---
    // A concrete quest goal it can't reach is the clearest signal.
    BotAI::StuckTracker const& st = ai.stuck_tracker();
    if (st.active && st.goal_is_quest)
    {
        wi.cat = WedgeCategory::GoalUnreachable;
        return wi;
    }

    // Off-mesh vs navmesh split (heuristic documented above).
    const bool off_mesh_signature =
        needs_bridge ||
        (indoors && st.active &&
         st.target_map == wi.map &&
         std::hypot(st.target_x - wi.x, st.target_y - wi.y) > 60.0f) ||
        (path_blocks >= 20);
    wi.cat = off_mesh_signature ? WedgeCategory::OffMesh : WedgeCategory::Navmesh;
    return wi;
}

void WedgeWatchdog::Tick(uint32 now_ms)
{
    if (!Services::Initialized())
        return;

    m_active.clear();

    // Track which bots are currently wedged so we can prune dedup entries for
    // bots that recovered (episode ended) — without this the map leaks an entry
    // per bot that was ever wedged.
    std::vector<BotId> still_wedged;
    still_wedged.reserve(m_reportedOnset.size());

    // Bots seen this tick — used to prune the position-anchor map so it can't
    // grow unbounded as bots log out / cycle.
    std::vector<BotId> seen;
    seen.reserve(m_posAnchor.size());

    // #7 self-remediation knobs, read once per tick (hot-reloadable, cheap).
    const bool   remediate_enabled = Services::Config().wedge_remediation_enabled();
    const uint32 remediate_ms      = Services::Config().wedge_remediation_ms();

    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        BotAI& ai = *e.ai;
        seen.push_back(id);

        // Displacement gate: maintain a position anchor; while the bot stays
        // within m_minDisplacement of it (same map), stationary_for grows; once
        // it moves past the radius (or changes map) the anchor — and the clock —
        // reset. This is the authoritative "is the bot physically stuck" signal.
        uint32 stationary_for = 0;
        // No-progress detector working set, filled from the snapshot below. A
        // candidate is a bot that keeps ACTING yet hasn't advanced its composite
        // progress fingerprint within the (archetype-scaled) window — the
        // movement-independent complement to the displacement gate.
        bool      np_candidate = false;
        WedgeInfo np_wi;   // pre-filled NoProgress info, applied only if no other wedge wins
        if (auto snap = Services::Snapshots().latest(id))
        {
            PosAnchor& a = m_posAnchor[id];
            const uint32 cmap = snap->position.map_id;
            const float  cx = snap->position.x, cy = snap->position.y;
            if (!a.valid || a.map != cmap ||
                std::hypot(cx - a.x, cy - a.y) > m_minDisplacement)
            {
                a.valid = true; a.map = cmap; a.x = cx; a.y = cy; a.ms = now_ms;
            }
            stationary_for = now_ms - a.ms;

            // ---- No-progress (composite-metric) tracking ----
            // Advance = ANY of: ding/level-xp, quests turned in, gold, total
            // skill points (gathering/fishing/crafting skill-ups), or bag-item
            // count (a capped-skill gatherer hoarding mats). Travel = net move
            // past m_noProgressRadius (a long walk toward a goal is progress, not
            // a stall). Either resets the anchor + its clock, so any legitimate
            // productive behavior keeps a bot off this list; only a bot truly
            // looping in place with nothing to show for it ages past the window.
            ProgressAnchor& pg = m_progress[id];
            uint32 skill_sum = 0;
            for (auto const& sk : snap->progression.skills) skill_sum += sk.value;
            const uint32 cur_level  = snap->identity.level;
            const uint32 cur_xp     = snap->identity.xp;
            const uint32 cur_quests = snap->quest_log.completed_quest_count;
            const int32  cur_gold   = snap->inventory.gold;
            const uint32 cur_bags   = static_cast<uint32>(snap->inventory.bag_items.size());
            const bool advanced =
                cur_level > pg.level ||
                (cur_level == pg.level && cur_xp > pg.xp) ||
                cur_quests > pg.quests_done ||
                cur_gold   > pg.gold ||
                skill_sum  > pg.skill_sum ||
                cur_bags   > pg.bag_items;
            const bool traveled = pg.valid &&
                (pg.map != cmap || std::hypot(cx - pg.x, cy - pg.y) > m_noProgressRadius);
            if (!pg.valid || advanced || traveled)
            {
                pg.valid = true; pg.map = cmap; pg.x = cx; pg.y = cy;
                pg.level = cur_level; pg.xp = cur_xp; pg.quests_done = cur_quests;
                pg.gold = cur_gold; pg.skill_sum = skill_sum; pg.bag_items = cur_bags;
                pg.ms = now_ms;
            }
            const uint32 np_stuck_for = now_ms - pg.ms;
            const uint32 np_window =
                (snap->archetype.dominant_activity == 3 /*ArchetypeActivity::Profession*/)
                    ? m_noProgressMs * kProfessionWindowMul : m_noProgressMs;
            // Exemptions: coordinated content (group/instance/BG), active combat
            // (owned by the combat layer + CombatLoop), and an in-flight cast
            // (a fishing/herb/craft channel that hasn't yet booked its gain).
            const bool np_exempt =
                !snap->group.group_guid.IsEmpty() ||
                snap->instance_ctx.is_in_instance ||
                snap->bg.in_battleground ||
                snap->vitals.in_combat ||
                snap->cast.is_casting;
            np_candidate = m_noProgressEnabled && !np_exempt && np_stuck_for >= np_window;
            if (np_candidate)
            {
                np_wi.bot         = id;
                np_wi.since_ms    = pg.ms;
                np_wi.duration_ms = np_stuck_for;
                np_wi.map = cmap; np_wi.x = cx; np_wi.y = cy; np_wi.z = snap->position.z;
                np_wi.zone_id = snap->area.zone_id;
                char const* lr = ai.last_rule_fired();
                np_wi.last_rule = lr ? lr : "(null)";
                // Objective the picker is routing to, for blacklist remediation
                // (same key the snapshot builder checks via objective_blacklisted).
                auto const& co = snap->quest_log.current_objective;
                if (co.quest_id != 0) { np_wi.wedge_quest_id = co.quest_id; np_wi.wedge_obj_id = co.id; }
                else { np_wi.wedge_quest_id = snap->quest_log.current_quest_id; np_wi.wedge_obj_id = 0; }
                np_wi.objective = np_wi.wedge_quest_id != 0
                    ? ("quest=" + std::to_string(np_wi.wedge_quest_id))
                    : std::string("(no-objective)");
                // An unresolvable POI on the current objective (mobile/escort NPC
                // with no static spawn) means the bot can NEVER path to it — the
                // root of a no-progress wander loop the displacement watchdog can't
                // see. Captured here to narrowly authorise NoProgress remediation.
                np_wi.objective_poi_invalid =
                    np_wi.wedge_quest_id != 0 && !snap->quest_log.current_objective_poi.valid;
            }
        }
        // else: no published snapshot -> no position data -> stationary_for stays
        // 0 so ClassifyWedge's gate returns None (can't confirm a wedge blind).

        WedgeInfo wi = ClassifyWedge(ai, id, stationary_for, m_thresholdMs,
                                     m_combatThresholdMs, now_ms);
        if (wi.cat == WedgeCategory::None)
        {
            // Not displacement-wedged. Fall back to the no-progress backstop: a
            // bot that keeps acting but never advances (innkeeper<->service
            // oscillation, picker_none churn, a death-cycle) is flagged here so
            // the SAME remediation (blacklist objective -> repick / relocate)
            // runs. If it isn't a no-progress candidate either, it's genuinely
            // fine — leave it out so dedup prunes.
            if (!np_candidate)
                return;
            wi = np_wi;
            wi.cat = WedgeCategory::NoProgress;
        }

        still_wedged.push_back(id);
        wi.bot_name = ResolveName(id);
        m_active.push_back(wi);

        // ---- Self-remediation: TIME-BOUNDED temp-blacklist of an UNREACHABLE
        //      objective (a human "skip this for now, come back later"). ----
        // The watchdog's PRIMARY job is DETECT + TRACK (the [wedge] log line and
        // the persistent stuck-objective ledger below, both ALWAYS-on regardless
        // of this flag — debuggability is never sacrificed). Remediation here is
        // NOT quest abandonment: a bot stuck past RemediationMs has the objective
        // the PICKER is routing to blacklisted for a TIME-BOUNDED 5-min block —
        // the quest resurfaces and is re-tested. The picker then repicks a
        // reachable objective and, meanwhile, the bot grinds nearby mobs for XP
        // (BotSnapshotBuilder treats a blacklisted objective as starvation). That
        // is exactly how a player handles a quest mob they cannot path to: do
        // other quests, retry later. No teleport-rescue, no relocation.
        //
        // SCOPE (2026-06-20): only GENUINELY-UNREACHABLE wedges remediate —
        // GoalUnreachable (a concrete quest goal the pathfinder cannot reach) and
        // CombatLoop (an unwinnable combat lock). NoProgress (the XP-rate stall
        // backstop) is DETECT+TRACK ONLY and NEVER blacklists a quest OVER A MERE
        // XP PLATEAU: a plateau can be legitimate non-XP work (crafting/gathering/
        // travel) and abandoning a quest over it is the band-aid the design avoids.
        // The ONE exception: a NoProgress bot whose current objective has an
        // UNRESOLVABLE POI (objective_poi_invalid) is provably unable to path to
        // that objective at all (mobile/escort NPC, no static spawn) — it wanders
        // forever and never displacement-wedges, so GoalUnreachable never fires.
        // That specific case IS a genuinely-unreachable objective and gets the same
        // temp-blacklist → repick/grind. The flag (RemediationEnabled, default TRUE)
        // governs the reachability temp-skip; set 0 for pure report-only.
        const bool remediable_cat =
            wi.cat == WedgeCategory::GoalUnreachable ||
            wi.cat == WedgeCategory::CombatLoop ||
            (wi.cat == WedgeCategory::NoProgress && wi.objective_poi_invalid);
        if (remediate_enabled && remediable_cat &&
            wi.duration_ms >= remediate_ms)
        {
            uint32 rq = wi.wedge_quest_id, ro = wi.wedge_obj_id;
            if (rq != 0 && !ai.objective_blacklisted(rq, ro, now_ms))
            {
                ai.blacklist_objective_now(rq, ro, now_ms);
                ++m_remediationTotal;
                TC_LOG_INFO("playerbot.v2",
                    "[wedge_remediate] bot={} cat={} dur={}s -> temp-blacklisted objective "
                    "quest={} obj={} 5min (picker repicks; quest resurfaces)",
                    wi.bot_name.empty() ? std::to_string(id) : wi.bot_name,
                    WedgeCategoryName(wi.cat), wi.duration_ms / 1000u,
                    rq, ro);
            }
        }

        // Dedup: report once per EPISODE. The episode identity is its onset
        // timestamp (since_ms); a bot that un-wedges then re-wedges gets a new
        // onset and is reported again.
        auto it = m_reportedOnset.find(id);
        const bool already_reported = (it != m_reportedOnset.end() &&
                                       it->second == wi.since_ms);
        if (already_reported)
            return;

        m_reportedOnset[id] = wi.since_ms;

        const size_t cat_idx = static_cast<size_t>(wi.cat);
        Services::Perf().record_wedge(cat_idx);
        if (cat_idx < m_categoryTotals.size())
            ++m_categoryTotals[cat_idx];

        TC_LOG_INFO("playerbot.v2",
            "[wedge] bot={} cat={} map={} zone={} pos=({:.1f},{:.1f},{:.1f}) "
            "dur={}s obj={} rule={}",
            wi.bot_name.empty() ? std::to_string(id) : wi.bot_name,
            WedgeCategoryName(wi.cat), wi.map, wi.zone_id,
            wi.x, wi.y, wi.z, wi.duration_ms / 1000u,
            wi.objective, wi.last_rule);

        // ---- Persistent stuck-objective TRACKING (always-on; the worklist). ----
        // Records the problematic (quest_id, obj_id) into the per-(quest,obj)
        // ledger that FlushStuckLedger UPSERTs into playerbot_v2_stuck_objective
        // ~1/min, so the fleet self-documents which content strands bots — the
        // debugging worklist the operator queries to find + fix root-cause quest
        // bugs. Independent of RemediationEnabled: tracking is the watchdog's job
        // whether or not auto-blacklist is on. Once per EPISODE (we're past the
        // dedup return), so hit_count counts distinct stuck episodes, not 7s
        // ticks. wedge_quest_id==0 (no actionable objective — picker_none /
        // oscillation / death-cycle with no resolvable goal) has no quest key to
        // aggregate; the [wedge] log line above is its record.
        if (wi.wedge_quest_id != 0)
        {
            const uint64 lkey = (uint64(wi.wedge_quest_id) << 32) | uint64(wi.wedge_obj_id);
            StuckAgg& sa = m_stuckLedger[lkey];
            ++sa.pending_delta; ++sa.total;
            sa.category = WedgeCategoryName(wi.cat);
            sa.map = wi.map; sa.zone = wi.zone_id; sa.x = wi.x; sa.y = wi.y;
            sa.bot = wi.bot_name;
        }
    });

    // Rebuild the dedup map from the set of bots still wedged THIS tick so a
    // recovered bot's entry is dropped (its next wedge re-reports) and the map
    // stays bounded. Rebuilt unconditionally rather than size-compared: equal
    // sizes can still hide a membership change (one bot recovered as another
    // newly wedged in the same tick), which a size check would miss.
    {
        std::unordered_map<BotId, uint32> kept;
        kept.reserve(still_wedged.size());
        for (BotId id : still_wedged)
        {
            auto it = m_reportedOnset.find(id);
            if (it != m_reportedOnset.end())
                kept.emplace(id, it->second);
        }
        m_reportedOnset.swap(kept);
    }

    // Prune position anchors for bots not seen this tick (logged out / cycled),
    // mirroring the dedup-map prune so neither map grows unbounded.
    if (m_posAnchor.size() > seen.size())
    {
        std::unordered_map<BotId, PosAnchor> keptAnchor;
        keptAnchor.reserve(seen.size());
        for (BotId id : seen)
        {
            auto it = m_posAnchor.find(id);
            if (it != m_posAnchor.end())
                keptAnchor.emplace(id, it->second);
        }
        m_posAnchor.swap(keptAnchor);
    }

    // Prune the no-progress anchors the same way (bots logged out / cycled).
    if (m_progress.size() > seen.size())
    {
        std::unordered_map<BotId, ProgressAnchor> keptProgress;
        keptProgress.reserve(seen.size());
        for (BotId id : seen)
        {
            auto it = m_progress.find(id);
            if (it != m_progress.end())
                keptProgress.emplace(id, it->second);
        }
        m_progress.swap(keptProgress);
    }

    // #7 follow-up: persist the accumulated stuck-objective aggregates (~1/min).
    FlushStuckLedger(now_ms);
}

// UPSERT every dirty stuck-objective aggregate into playerbot_v2_stuck_objective
// on a slow cadence. hit_count = hit_count + VALUES(hit_count) accumulates the
// per-interval delta both within a session and across restarts, so the table is
// a durable fleet-wide tally of which content strands bots. World-thread only;
// CharacterDatabase.Execute is async (non-blocking). quest_id/obj_id/map/zone are
// integers and x/y floats; the only string columns (category, bot) are sanitized
// to a safe character set + capped to the column width so the formatted INSERT
// can never be broken or injected by a stray character.
void WedgeWatchdog::FlushStuckLedger(uint32 now_ms)
{
    if (m_lastLedgerFlushMs != 0 && (now_ms - m_lastLedgerFlushMs) < kLedgerFlushMs)
        return;
    m_lastLedgerFlushMs = now_ms;

    auto sanitize = [](std::string const& in, size_t cap, bool digits) {
        std::string out;
        for (char c : in)
        {
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (digits && c >= '0' && c <= '9');
            if (ok) { out.push_back(c); if (out.size() >= cap) break; }
        }
        return out;
    };

    for (auto& [key, sa] : m_stuckLedger)
    {
        if (sa.pending_delta == 0)
            continue;
        const uint32 quest_id = static_cast<uint32>(key >> 32);
        const uint32 obj_id   = static_cast<uint32>(key & 0xFFFFFFFFu);
        const std::string cat = sanitize(sa.category, 24, false);
        const std::string bot = sanitize(sa.bot, 48, true);
        CharacterDatabase.Execute(fmt::format(
            "INSERT INTO playerbot_v2_stuck_objective "
            "(quest_id,obj_id,category,hit_count,first_seen,last_seen,"
            " sample_map,sample_zone,sample_x,sample_y,sample_bot) "
            "VALUES ({},{},'{}',{},NOW(),NOW(),{},{},{:.1f},{:.1f},'{}') "
            "ON DUPLICATE KEY UPDATE hit_count=hit_count+VALUES(hit_count), "
            "last_seen=NOW(), category=VALUES(category), sample_map=VALUES(sample_map), "
            "sample_zone=VALUES(sample_zone), sample_x=VALUES(sample_x), "
            "sample_y=VALUES(sample_y), sample_bot=VALUES(sample_bot)",
            quest_id, obj_id, cat, sa.pending_delta, sa.map, sa.zone,
            sa.x, sa.y, bot).c_str());
        sa.pending_delta = 0;
    }
}

} // namespace Playerbot::Diagnostics

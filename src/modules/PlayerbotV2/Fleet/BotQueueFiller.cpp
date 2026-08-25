#include "BotQueueFiller.h"
#include "BotAccountMgr.h"          // altbot (non-pool account) draft exemption
#include "BotPopulationManager.h"    // ProtectFromKick lease for JIT match-fill spawns
#include "BotCharacterFactory.h"
#include "BotComposition.h"
#include "BotIdentityRegistry.h"
#include "OwnerRegistry.h"          // altbot (owner-bound) draft exemption
#include "../Services.h"
#include "WorldSession.h"           // account-id lookup for the altbot test
#include "../Session/BotSessionMgr.h"
#include "../Bot/BotIntent.h"
#include "../Bot/BotIntentEmitter.h"
#include "../Bot/BotAI.h"           // ActionKind::DualSpec lockout
#include "../Bot/BotRegistry.h"     // Services::Registry().ai(id)
#include "../Bot/ClassTables.h"  // IsTankSpec / TankSpecForClass etc.
#include "../Threading/IntentQueue.h"
#include "../Diagnostics/PerfCounters.h"
#include "DB2Stores.h"
#include "Player.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "DungeonFinding/LFGMgr.h"
#include "Group.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "RaceMask.h"
#include "Timer.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <span>

namespace Playerbot::V2::Fleet {

namespace {

struct RoleNeeds { uint32 tanks; uint32 healers; uint32 dps; };

RoleNeeds NeedsFor(BotQueueFiller::QueueKind kind)
{
    // BG needs intentionally overcommit: pipeline attrition (JIT bots
    // that don't finish setup before queue resolves) drops the
    // effective population by 30-60%. Queueing 2x ensures the BG
    // hits min-players quickly AND keeps the FreeSlotQueue topped up
    // so the BG can auto-pull replacements during prep phase / mid-
    // match. Bots that don't make it into the first instance fall
    // back to the next queue resolution; harmless.
    switch (kind)
    {
        case BotQueueFiller::QueueKind::Dungeon5: return {1, 1, 3};
        case BotQueueFiller::QueueKind::Raid10:   return {2, 3, 5};
        case BotQueueFiller::QueueKind::Raid20:   return {2, 4, 14};
        // BG fill is ROLE-AGNOSTIC: a battleground has no tank/healer slot
        // requirement (any N players form a side), so requiring tanks/healers here
        // only HARMS roster completion — JIT tank/heal CREATION fails on name-pool
        // exhaustion for the few tank/heal-capable race+class combos (DPS creation
        // succeeds), so a side stalled at ~6/10 in PREP waiting for tanks/heals that
        // never spawn and the match never started (live: WSG stuck PREP 6v6,
        // needs=3T/1H/0D). Fill purely as DPS so creation always succeeds and the
        // side reaches MinPlayers. (Healers/tanks among ONLINE bots are still
        // queued by the online-first pass — they just aren't a hard requirement.)
        case BotQueueFiller::QueueKind::Bg:
        default:                                  return {0, 0, 36};  // ~36 per side, role-agnostic
    }
}

// Role/spec predicates and class→spec lookups live in Bot/ClassTables.cpp
// so BotQueueFiller and idle:dual_spec_switch share one source of truth.
// Adding a class or shifting a canonical spec requires editing ClassTables
// only. (Originally inlined here; deduped 2026-05-10 when the idle rule
// landed.)

// Bracket -> level midpoint
uint8 BracketMidpoint(uint8 bracket)
{
    // bracket is the bracket "tag" used by BG queues: 0 for 10-19, 1 for 20-29...
    // For our purposes treat bracket as approximate floor of the player level / 10.
    // Convert by midpoint: bracket=2 -> level 25, bracket=7 -> level 75, etc.
    if (bracket >= 7) return 80;
    return uint8(bracket * 10 + 15);
}

} // anonymous

void BotQueueFiller::Fill(FillRequest const& req)
{
    auto needs = NeedsFor(req.kind);
    // Per-role override (LFG top-up). When ANY override is non-zero, the
    // caller is in override-mode — the entire (T, H, D) triple is treated
    // as authoritative deficit, including zero values. This lets the
    // LFG-refill cron say "need 0 tanks, 1 healer, 2 dps" without the
    // tank default of 1 sneaking back in. A request with all overrides
    // zero falls through to NeedsFor(kind) defaults.
    const bool override_mode =
        (req.needs_tank_override |
         req.needs_healer_override |
         req.needs_dps_override) != 0;
    if (override_mode)
    {
        needs.tanks   = req.needs_tank_override;
        needs.healers = req.needs_healer_override;
        needs.dps     = req.needs_dps_override;
    }
    // Apply explicit cap (running-BG top-up). When max_total_bots is set,
    // we want EXACTLY that many bots queued total across roles. Trim the
    // role quotas proportionally so the early-break in the bot loop fires
    // at the right cumulative count. Tanks/healers preserved up to the
    // cap; remainder goes to DPS.
    if (req.max_total_bots > 0)
    {
        const uint32 cap = req.max_total_bots;
        uint32 remaining = cap;
        const uint32 t_cap = std::min<uint32>(needs.tanks, remaining); remaining -= t_cap;
        const uint32 h_cap = std::min<uint32>(needs.healers, remaining); remaining -= h_cap;
        const uint32 d_cap = std::min<uint32>(needs.dps, remaining);
        needs = { t_cap, h_cap, d_cap };
    }
    uint32 total_needed = needs.tanks + needs.healers + needs.dps;
    if (!total_needed) return;

    Services::Perf().record_queue_fill_request();
    // Latency tracking: stamp the start time on the bot's DB row when JIT-
    // spawned (encoded in jit_for_queue tag isn't enough — we'd lose ms
    // resolution). Use process-time-ms via GameTime::Now via OS clock.
    // For online-bot fill we count immediate completion since they queue
    // synchronously in this call.
    uint32 const start_ms_for_completion = getMSTime();

    uint8 const target_level = req.target_level_override
                                   ? req.target_level_override
                                   : BracketMidpoint(req.bracket);

    // Compute the specific queue-type id for this Fill so we can
    // distinguish "bot already in OUR queue" (harmless, TC handles
    // idempotently) from "bot in some OTHER BG queue" (problematic
    // for backfill — the bot will be claimed by whichever BG pops
    // first and we won't get them).
    BattlegroundQueueTypeId const this_qid =
        (req.kind == QueueKind::Bg)
            ? BattlegroundMgr::BGQueueTypeId(
                  uint16(req.instance_id),
                  BattlegroundQueueIdType::Battleground,
                  /*rated*/ false, /*teamSize*/ 0)
            : BATTLEGROUND_QUEUE_NONE;

    // Faction handling. For BG queues, BOTH factions need bots so a match
    // can form (queue waits for N alliance + N horde). For LFG/raid queues,
    // only the player's faction needs filling (group is same-faction).
    bool const player_is_alliance = (req.faction == ALLIANCE);
    bool const is_bg = (req.kind == QueueKind::Bg);

    TC_LOG_INFO("playerbot.v2", "[QueueFill] kind={} bracket={} faction={} instance={} needs={}T/{}H/{}D{}",
                uint32(req.kind), uint32(req.bracket), uint32(req.faction),
                req.instance_id, needs.tanks, needs.healers, needs.dps,
                is_bg ? " (both factions)" : "");

  // Run the fill flow once per faction we need to populate.
  // For BG: 2 passes (player faction + enemy faction).
  // For LFG: 1 pass (player faction only).
  for (int faction_pass = 0; faction_pass < (is_bg ? 2 : 1); ++faction_pass)
  {
    bool const want_alliance = (faction_pass == 0) ? player_is_alliance : !player_is_alliance;

    // Walk online bots, queue matching candidates by role.
    auto& reg = Services::Lifecycle();
    auto ids = reg.snapshot_ids();
    uint32 queued_t = 0, queued_h = 0, queued_d = 0;

    // Rejection histogram for diagnostics — without this we can't tell whether
    // the pool was small (online count low) or filtered (most bots in WSG queue).
    // Critical for diagnosing AB-stuck-after-WSG style asymmetries.
    uint32 rej_faction = 0, rej_level = 0, rej_grouped = 0, rej_combat = 0,
           rej_dungeon = 0, rej_no_qslot = 0, rej_role = 0, rej_deserter = 0;
    uint32 online_seen = 0, multi_queued = 0;

    // Level window: ±15 for BG (matches level-scale anyway and BG bracket
    // bounds are enforced strictly via PVPDifficulty below). LFG: ±3.
    // Owner directive 2026-05-15 — even with modern level-scaling, a L22
    // player matched with L14 dungeon-mates "reads broken" in chat / UI.
    // Tight window forces JIT-spawn of player-level bots when the online
    // pool is far off, which is sub-second on the modern pipeline and
    // produces a level-coherent group. target_level comes from
    // req.target_level_override which the PlayerbotV2 LFG path sets to
    // the player's actual level (see PlayerbotV2.cpp ~line 932).
    int const level_window = is_bg ? 15 : 3;
    for (auto id : ids)
    {
        if (queued_t >= needs.tanks && queued_h >= needs.healers && queued_d >= needs.dps)
            break;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot) continue;
        ++online_seen;
        if ((bot->GetTeam() == ALLIANCE) != want_alliance)    { ++rej_faction; continue; }
        // BG-side hard bracket bounds (when provided). The BG's
        // PVPDifficulty data declares which levels each bracket accepts;
        // any bot outside that exact range fails the bracket lookup in
        // API::bg_queue and gets `no_bracket`. Filter strictly here so
        // the queue intents we emit can actually land in TC's queue.
        // Falls back to the coarse ±level_window check when bracket
        // bounds aren't supplied (LFG path, legacy callers).
        if (is_bg && req.bracket_min_level > 0 && req.bracket_max_level > 0)
        {
            uint8 const lvl = bot->GetLevel();
            if (lvl < req.bracket_min_level || lvl > req.bracket_max_level)
            { ++rej_level; continue; }
        }
        else if (std::abs(int(bot->GetLevel()) - int(target_level)) > level_window)
        { ++rej_level; continue; }
        if (!bot->IsAlive())                                  { ++rej_dungeon; continue; }   // dead → api.lfg_queue rejects with "dead"
        if (bot->GetGroup())                                  { ++rej_grouped; continue; }
        if (bot->IsInCombat())                                { ++rej_combat; continue; }
        if (bot->GetMap() && bot->GetMap()->IsDungeon())      { ++rej_dungeon; continue; }
        // Never draft a real player's alt into a BG/LFG queue — porting
        // their character into a random match is the owner's call, not
        // the queue filler's. Alt = owner-bound, or living on a non-pool
        // (real player) account.
        if (Services::Owners().GetOwner(id).account_id != 0)  { ++rej_grouped; continue; }
        if (WorldSession const* bsess = bot->GetSession())
            if (!Services::Accounts().is_pool_account(bsess->GetAccountId()))
                                                              { ++rej_grouped; continue; }
        // Pre-filter Deserter (BG side): API::bg_queue rejects deserters with
        // Locked. For LFG, the equivalent gate is HasAura(LFG_SPELL_DUNGEON_
        // DESERTER) — silently rejected inside LFGMgr::JoinLfg with no error
        // bubbling back to us. Either way the intent is wasted; skip up-front.
        if (is_bg && bot->IsDeserter())                       { ++rej_deserter; continue; }
        if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_DESERTER*/ 71041))
                                                              { ++rej_deserter; continue; }
        if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_COOLDOWN*/ 71328))
                                                              { ++rej_deserter; continue; }
        // BG-queue handling. TC's API::bg_queue is idempotent for the
        // same bg_type_id (returns Ok without re-adding). The real cost
        // is bots in queues for DIFFERENT bg_type_ids: they have 3 queue
        // slots total, and whichever BG pops first claims them, dropping
        // the others. For specific-BG top-up (max_total_bots > 0), skip
        // bots that are in some OTHER queue — bots already in OUR target
        // queue are fine to "queue" again (TC dedups) and tracking the
        // counter just surfaces pool saturation.
        const bool in_our_queue = is_bg &&
            this_qid != BATTLEGROUND_QUEUE_NONE &&
            bot->InBattlegroundQueueForBattlegroundQueueType(this_qid);
        const bool in_any_queue = bot->InBattlegroundQueue();
        if (in_any_queue && !in_our_queue)
        {
            ++multi_queued;
            if (req.max_total_bots > 0) { ++rej_no_qslot; continue; }
            // For LFG we MUST reject — LFGMgr::JoinLfg rejects any player
            // in BattlegroundQueue with LFG_JOIN_CANT_USE_DUNGEONS, silently
            // (no Result propagation back to us). Without this filter the
            // intent is pushed and the bot never actually enters the LFG
            // queue → observed "filler queued 3D, only 1D in LFG queue".
            if (!is_bg)              { ++rej_no_qslot; continue; }
        }
        if (!bot->HasFreeBattlegroundQueueId() && !in_our_queue)
        {
            ++rej_no_qslot;
            continue;
        }
        // LFG state pre-check: bot already queued/proposed/in-dungeon would
        // hit API::lfg_queue's state guard and return Locked. Filter here so
        // the histogram surfaces the count (otherwise these bots look "OK"
        // in the queue log but never actually join the dungeon queue).
        if (!is_bg)
        {
            lfg::LfgState const lst = sLFGMgr->GetState(bot->GetGUID());
            if (lst == lfg::LFG_STATE_QUEUED || lst == lfg::LFG_STATE_PROPOSAL ||
                lst == lfg::LFG_STATE_DUNGEON || lst == lfg::LFG_STATE_FINISHED_DUNGEON)
            {
                ++rej_no_qslot;
                continue;
            }
        }

        uint8 cls = bot->GetClass();
        uint16 spec = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));

        // Inline-respec priority for hybrid classes. Without this,
        // pass 1 queues a Shadow Priest / Balance Druid / Enhancement Shaman
        // as DPS, then pass 2 wants to convert them to Healer but the bot is
        // already LFG-queued → JoinLfg's "already queued" guard rejects the
        // second call. Net: bot is Holy-spec but registered as DPS in TC's
        // LFG → matcher never picks them as a healer. Fix: prefer scarce
        // roles (T > H > D) for hybrid classes in pass 1 itself, doing the
        // respec inline. Lockout still gates churn.
        bool const has_tank_path   = TankSpecForClass(cls) != 0;
        bool const has_healer_path = HealerSpecForClass(cls) != 0;
        BotAI* respec_ai_p1 = Services::Registry().ai(id);
        uint32 const lockout_now_ms = getMSTime();
        auto inline_respec = [&](uint32 target_spec) -> bool
        {
            if (target_spec == 0 || spec == target_spec) return false;
            if (respec_ai_p1 && respec_ai_p1->action_recently_tried(
                    BotAI::ActionKind::QueueFillRespec, id, lockout_now_ms))
                return false;   // lockout active — fall through; pass 2 won't help either
            if (auto const* se = sChrSpecializationStore.LookupEntry(target_spec))
            {
                bot->ActivateTalentGroup(se);
                if (respec_ai_p1)
                    respec_ai_p1->note_action_retry(
                        BotAI::ActionKind::QueueFillRespec, id, lockout_now_ms);
                return true;
            }
            return false;
        };

        auto queue_role = [&](Role r)
        {
            Intent it{};
            it.bot_id = id;
            if (req.kind == QueueKind::Bg)
                it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(req.instance_id)}};
            else
                it.body = QueueIntent{LfgQueueIntent{req.instance_id, r}};
            Services::Intents(id).push(std::move(it));
            // REUSE of an online JIT bot (owner directive: reuse over re-create to
            // save the costly create+setup). Re-confine it to THIS BG and reset its
            // unused-timer: refresh the JIT purpose (new bg_type so it stages for the
            // right match) and extend the 10-min kick-protect lease so it isn't
            // logged out mid-match. An unused parked JIT bot's lease still lapses
            // -> LRU retires it (no idle bots lingering).
            if (req.kind == QueueKind::Bg)
            {
                if (BotAI* reuse_ai = Services::Registry().ai(id))
                    reuse_ai->set_bg_jit_purpose(uint16(req.instance_id), getMSTime());
                Services::Population().ProtectFromKick(uint64(id), 10u * 60u * 1000u);
            }
        };

        // Priority pick: scarce roles first (Tank > Healer > DPS). For each
        // role in priority order, if the slot is still open and the bot CAN
        // fill it (either natively or via inline respec), queue them as
        // that role. Critically: a tank-spec bot whose tank slot is already
        // full but who's healer-capable (e.g. Guardian Druid → Resto Druid)
        // gets respec'd to healer if that slot is open. Previous logic's
        // `!IsTankSpec && !IsHealerSpec` guard prevented this, wasting
        // hybrid capacity. Result of earlier log: `already_role=2, no_target
        // _class=8, healer_conv=0` even though both already-role bots could
        // have been respec'd into the open healer slot.
        auto try_queue_role = [&](Role r) -> bool
        {
            // Capacity check.
            if (r == Role::Tank   && queued_t >= needs.tanks)   return false;
            if (r == Role::Healer && queued_h >= needs.healers) return false;
            if (r == Role::Dps    && queued_d >= needs.dps)     return false;
            // Spec match? If yes, no respec needed.
            bool spec_matches = false;
            switch (r)
            {
                case Role::Tank:   spec_matches = IsTankSpec(cls, spec);   break;
                case Role::Healer: spec_matches = IsHealerSpec(cls, spec); break;
                case Role::Dps:    spec_matches = !IsTankSpec(cls, spec)
                                              && !IsHealerSpec(cls, spec); break;
                default: return false;
            }
            if (!spec_matches)
            {
                // Need to respec. Class must have a target spec for this role.
                uint32 target = 0;
                if      (r == Role::Tank)   target = TankSpecForClass(cls);
                else if (r == Role::Healer) target = HealerSpecForClass(cls);
                if (target == 0) return false;       // class can't fill role
                if (!inline_respec(target)) return false;
                // After respec, update local spec to avoid re-matching by old spec.
                spec = uint16(target);
            }
            queue_role(r);
            if      (r == Role::Tank)   ++queued_t;
            else if (r == Role::Healer) ++queued_h;
            else                        ++queued_d;
            return true;
        };

        if (try_queue_role(Role::Tank))   continue;
        if (try_queue_role(Role::Healer)) continue;
        if (try_queue_role(Role::Dps))    continue;
        // No role bucket has space the bot can fill — pass 2 still runs.
        ++rej_role;
    }

    TC_LOG_INFO("playerbot.v2",
        "[QueueFill] pool faction={} online_seen={} queued={}T/{}H/{}D "
        "(multi_queued={}) rejected: faction={} level={} grouped={} "
        "combat={} dungeon={} no_qslot={} role_full={} deserter={}",
        want_alliance ? "ALLIANCE" : "HORDE", online_seen,
        queued_t, queued_h, queued_d, multi_queued,
        rej_faction, rej_level, rej_grouped, rej_combat,
        rej_dungeon, rej_no_qslot, rej_role, rej_deserter);

    // Second pass: convert tank-/healer-CAPABLE classes that are currently in
    // the wrong spec. The 4885-bot pool tends to drift toward DPS specs
    // (State_Idle's apply_context_talents picks the leveling/raid build, which
    // for hybrid classes defaults to a DPS spec). Without conversion, a /lfg
    // request for 1T+1H falls almost entirely through to the JIT-spawn path,
    // which takes 5-15s of pipeline work per slot. Activating an existing
    // online bot's tank/healer spec inline is sub-100ms and uses bots the
    // owner already paid to spawn.
    //
    // We only re-spec when role quotas are still unmet after the first pass.
    // Re-spec then queue with the matching role; State_Idle's apply_context
    // _talents detects the spec change in-dungeon and lays in the right
    // talents on the next tick.
    // Pass-2 diagnostics: count candidates evaluated, rejection reasons,
    // respec attempts, and outcome. Without this we can't tell whether
    // pass-2 has zero hybrid candidates, a respec-lockout problem, a
    // missing HealerSpecForClass mapping, or an ActivateTalentGroup
    // failure. Emitted as a single line at end of pass-2 so the log
    // stays readable.
    uint32 p2_seen = 0, p2_filtered_out = 0;
    uint32 p2_already_role_spec = 0, p2_lockout = 0;
    uint32 p2_no_target_class = 0, p2_activate_failed = 0;
    uint32 p2_tank_converted = 0, p2_healer_converted = 0;
    if (queued_t < needs.tanks || queued_h < needs.healers)
    {
        for (auto id : ids)
        {
            if (queued_t >= needs.tanks && queued_h >= needs.healers)
                break;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* bot = ObjectAccessor::FindConnectedPlayer(g);
            if (!bot) continue;
            if ((bot->GetTeam() == ALLIANCE) != want_alliance) continue;
            if (is_bg && req.bracket_min_level > 0 && req.bracket_max_level > 0)
            {
                uint8 const lvl2 = bot->GetLevel();
                if (lvl2 < req.bracket_min_level || lvl2 > req.bracket_max_level)
                    continue;
            }
            else if (std::abs(int(bot->GetLevel()) - int(target_level)) > level_window)
                continue;
            ++p2_seen;
            if (!bot->IsAlive())                          { ++p2_filtered_out; continue; }
            if (bot->GetGroup())                          { ++p2_filtered_out; continue; }
            if (bot->IsInCombat())                        { ++p2_filtered_out; continue; }
            if (bot->GetMap() && bot->GetMap()->IsDungeon()) { ++p2_filtered_out; continue; }
            if (is_bg && bot->IsDeserter())               { ++p2_filtered_out; continue; }
            if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_DESERTER*/ 71041))
                                                          { ++p2_filtered_out; continue; }
            if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_COOLDOWN*/ 71328))
                                                          { ++p2_filtered_out; continue; }
            // Same other-queue exclusion as pass 1.
            const bool in_our_q2 = is_bg &&
                this_qid != BATTLEGROUND_QUEUE_NONE &&
                bot->InBattlegroundQueueForBattlegroundQueueType(this_qid);
            if (req.max_total_bots > 0 &&
                bot->InBattlegroundQueue() && !in_our_q2)
                                                          { ++p2_filtered_out; continue; }
            // For LFG, bot in any BG queue is rejected by LFGMgr::JoinLfg.
            if (!is_bg && bot->InBattlegroundQueue() && !in_our_q2)
                                                          { ++p2_filtered_out; continue; }
            if (!bot->HasFreeBattlegroundQueueId() && !in_our_q2)
                                                          { ++p2_filtered_out; continue; }
            if (!is_bg)
            {
                lfg::LfgState const lst2 = sLFGMgr->GetState(bot->GetGUID());
                if (lst2 == lfg::LFG_STATE_QUEUED || lst2 == lfg::LFG_STATE_PROPOSAL ||
                    lst2 == lfg::LFG_STATE_DUNGEON || lst2 == lfg::LFG_STATE_FINISHED_DUNGEON)
                                                          { ++p2_filtered_out; continue; }
            }

            uint8 const cls  = bot->GetClass();
            uint16 const cur_spec = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));

            // Skip bots already counted in pass 1 — IsTankSpec / IsHealerSpec
            // would have routed them to the right bucket above.
            if (IsTankSpec(cls, cur_spec) || IsHealerSpec(cls, cur_spec))
            {
                ++p2_already_role_spec;
                continue;
            }

            // Per-bot respec lockout (mirrors RunRebalance's DualSpec lockout).
            // ActivateTalentGroup calls UpdateTraitConfig which re-learns all
            // passive talent spells via Player::AddSpell. Some passive auras
            // (e.g. Monk Coalescence 450529) hit a TC core assertion when
            // re-applied while already active — preventing rapid-fire respec
            // on the same bot avoids the assertion crash.
            uint32 const now_ms_for_lockout = getMSTime();
            BotId const bot_id = id;
            BotAI* respec_ai = Services::Registry().ai(bot_id);
            bool can_respec = !respec_ai ||
                !respec_ai->action_recently_tried(BotAI::ActionKind::QueueFillRespec,
                                                  bot_id, now_ms_for_lockout);
            if (!can_respec) { ++p2_lockout; continue; }

            auto activate = [&](uint32 target_spec) -> bool {
                if (target_spec == 0 || cur_spec == target_spec) return false;
                if (auto const* spec_entry = sChrSpecializationStore.LookupEntry(target_spec))
                {
                    bot->ActivateTalentGroup(spec_entry);
                    if (respec_ai)
                        respec_ai->note_action_retry(BotAI::ActionKind::QueueFillRespec,
                                                    bot_id, now_ms_for_lockout);
                    return true;
                }
                return false;
            };

            if (queued_t < needs.tanks)
            {
                uint32 const target_spec = TankSpecForClass(cls);
                if (target_spec == 0) { ++p2_no_target_class; }
                else if (activate(target_spec))
                {
                    Intent it{};
                    it.bot_id = id;
                    if (req.kind == QueueKind::Bg)
                        it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(req.instance_id)}};
                    else
                        it.body = QueueIntent{LfgQueueIntent{req.instance_id, Role::Tank}};
                    Services::Intents(id).push(std::move(it));
                    ++queued_t;
                    ++p2_tank_converted;
                    continue;
                }
                else { ++p2_activate_failed; }
            }
            if (queued_h < needs.healers)
            {
                uint32 const target_spec = HealerSpecForClass(cls);
                if (target_spec == 0) { ++p2_no_target_class; }
                else if (activate(target_spec))
                {
                    Intent it{};
                    it.bot_id = id;
                    if (req.kind == QueueKind::Bg)
                        it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(req.instance_id)}};
                    else
                        it.body = QueueIntent{LfgQueueIntent{req.instance_id, Role::Healer}};
                    Services::Intents(id).push(std::move(it));
                    ++queued_h;
                    ++p2_healer_converted;
                }
                else { ++p2_activate_failed; }
            }
        }
        TC_LOG_INFO("playerbot.v2",
            "[QueueFill] pass2 respec faction={} seen={} filtered={} already_role={} "
            "lockout={} no_target_class={} activate_failed={} → tank_conv={} healer_conv={}",
            want_alliance ? "ALLIANCE" : "HORDE",
            p2_seen, p2_filtered_out, p2_already_role_spec, p2_lockout,
            p2_no_target_class, p2_activate_failed,
            p2_tank_converted, p2_healer_converted);
    }

    // Shortfall — JIT-spawn the rest. Each new bot gets full setup pipeline
    // at the bracket midpoint. The pipeline runs across a few ticks so the
    // bot may queue 1-2s after this call.
    // Class candidates per role. JIT picks one of these classes so the bot
    // can fill the right slot — even if exact spec depends on Player::Create
    // defaults, the class-side proficiency is right (Warrior can tank, Priest
    // can heal). Spec gets aligned later by State_Idle's apply_context_talents.
    static constexpr uint8 kTankClasses[]   = { 1, 2, 6, 10, 11, 12 };  // Warrior/Pally/DK/Monk/Druid/DH
    static constexpr uint8 kHealerClasses[] = { 2, 5, 7, 10, 11, 13 };  // Pally/Priest/Sham/Monk/Druid/Evoker
    auto pick_role_class = [](Role r, uint64 seed) -> uint8 {
        if (r == Role::Tank)
            return kTankClasses[seed % (sizeof(kTankClasses)/sizeof(kTankClasses[0]))];
        if (r == Role::Healer)
            return kHealerClasses[seed % (sizeof(kHealerClasses)/sizeof(kHealerClasses[0]))];
        return 0;  // 0 = no hint (random class)
    };

    auto spawn_jit = [&](Role role, uint32 count, char const* role_name) {
        // Faction-correct race candidates. Hinting a race to BotComposition::Roll
        // guarantees the resulting bot is on the right faction without retries.
        static constexpr uint8 kAllianceRaces[] = {
            RACE_HUMAN, RACE_DWARF, RACE_GNOME, RACE_NIGHTELF, RACE_DRAENEI,
            RACE_WORGEN, RACE_PANDAREN_ALLIANCE, RACE_VOID_ELF,
            RACE_LIGHTFORGED_DRAENEI, RACE_KUL_TIRAN, RACE_DARK_IRON_DWARF,
            RACE_MECHAGNOME, RACE_DRACTHYR_ALLIANCE, RACE_EARTHEN_DWARF_ALLIANCE,
        };
        static constexpr uint8 kHordeRaces[] = {
            RACE_ORC, RACE_TROLL, RACE_TAUREN, RACE_UNDEAD_PLAYER, RACE_BLOODELF,
            RACE_GOBLIN, RACE_PANDAREN_HORDE, RACE_NIGHTBORNE,
            RACE_HIGHMOUNTAIN_TAUREN, RACE_ZANDALARI_TROLL, RACE_VULPERA,
            RACE_MAGHAR_ORC, RACE_DRACTHYR_HORDE, RACE_EARTHEN_DWARF_HORDE,
        };
        auto const races = want_alliance ? std::span<uint8 const>(kAllianceRaces)
                                         : std::span<uint8 const>(kHordeRaces);

        uint32 roll_fail = 0, create_fail = 0, success = 0, reused = 0;
        std::string last_create_reason;

        // REUSE pass before creating anything: the DB accumulates offline
        // marked L80 bots from every previous JIT cycle (each seed used to
        // mint ~70 brand-new characters; nothing ever logged the old ones
        // back in -- observed: thousands of offline pool L80s while every
        // cycle paid full character-creation + setup-pipeline cost and the
        // name pools drained). Re-tag + defer-login existing pool-account
        // characters of the right faction/class/level; the same
        // jit_for_queue pipeline hook queues them once in-world. Only the
        // remainder falls through to creation.
        {
            std::string class_in;
            auto append_classes = [&](uint8 const* arr, size_t n)
            {
                for (size_t ci = 0; ci < n; ++ci)
                {
                    if (!class_in.empty()) class_in += ',';
                    class_in += std::to_string(uint32(arr[ci]));
                }
            };
            if (role == Role::Tank)
                append_classes(kTankClasses, sizeof(kTankClasses)/sizeof(kTankClasses[0]));
            else if (role == Role::Healer)
                append_classes(kHealerClasses, sizeof(kHealerClasses)/sizeof(kHealerClasses[0]));
            std::string race_in;
            for (uint8 rc : races)
            {
                if (!race_in.empty()) race_in += ',';
                race_in += std::to_string(uint32(rc));
            }
            const uint8 lvl_lo = (is_bg && req.bracket_min_level > 0)
                ? req.bracket_min_level : uint8(std::max(1, int(target_level) - 3));
            const uint8 lvl_hi = (is_bg && req.bracket_max_level > 0)
                ? req.bracket_max_level : uint8(std::min(80, int(target_level) + 3));
            // totaltime>0 excludes the failed-login corpse pool (hygiene
            // sweeps those); the pool-account join excludes player alts.
            auto reuse_res = CharacterDatabase.PQuery(
                "SELECT c.guid FROM characters c "
                "JOIN playerbot_v2_character pv ON pv.character_guid_low = c.guid "
                "JOIN playerbot_v2_account pa ON pa.account_id = c.account "
                "WHERE c.online = 0 AND c.totaltime > 0 "
                "AND c.level BETWEEN {} AND {} "
                "AND c.race IN ({}) {} "
                "LIMIT {}",
                uint32(lvl_lo), uint32(lvl_hi), race_in,
                class_in.empty() ? std::string()
                                 : fmt::format("AND c.class IN ({})", class_in),
                count);
            if (!reuse_res)
                TC_LOG_INFO("playerbot.v2",
                    "[QueueFill] reuse query EMPTY role={} lvl={}..{} classes='{}' races_n={} limit={}",
                    role_name, uint32(lvl_lo), uint32(lvl_hi),
                    class_in.empty() ? "any" : class_in, uint32(races.size()), count);
            if (reuse_res)
            {
                do {
                    const uint64 reuse_guid = reuse_res->Fetch()[0].GetUInt64();
                    char const* kind_str = (req.kind == QueueKind::Bg) ? "BG" : "LFG";
                    CharacterDatabase.PExecute(
                        "UPDATE playerbot_v2_character SET distribution_level={}, "
                        "jit_for_queue='{}:{}:{}', last_seen_at=NOW() "
                        "WHERE character_guid_low={}",
                        uint32(target_level), kind_str, req.instance_id, role_name,
                        reuse_guid);
                    Services::Population().DeferLogin(reuse_guid);
                    Services::Population().ProtectFromKick(reuse_guid, 10u * 60u * 1000u);
                    ++reused;
                } while (reuse_res->NextRow());
            }
        }
        count = (reused >= count) ? 0u : count - reused;
        success += reused;

        for (uint32 i = 0; i < count; ++i)
        {
            uint8 cls_hint  = pick_role_class(role, uint64(i + std::time(nullptr)));
            uint8 race_hint = races[uint64(i + std::time(nullptr)) % races.size()];
            BotComposition::Pick pick = BotComposition::Roll(race_hint, cls_hint);
            if (!pick.race) { ++roll_fail; continue; }
            auto created = BotCharacterFactory::Create(/*ownerSession=*/nullptr,
                pick.name, pick.race, pick.cls, pick.gender);
            // Name-collision retry: rolled names collide against the 40K+
            // existing characters DB at meaningful rates, and a single-shot
            // create made the whole JIT spawn batch fail with 'name already
            // in use' (observed: 36/36 fails on the first autonomous BG
            // seed). Re-roll a fresh pick a few times before surrendering
            // the slot.
            for (int retry = 0;
                 !created.ok && created.reason == "name already in use" && retry < 8;
                 ++retry)
            {
                // Vary the RACE per retry — name pools are per race/gender,
                // so re-rolling the same race draws from the same (possibly
                // exhausted) pool and collides again (observed: tank/heal
                // passes failing all retries while DPS succeeded).
                const uint8 retry_race =
                    races[uint64(i + retry + 1 + std::time(nullptr)) % races.size()];
                pick = BotComposition::Roll(retry_race, cls_hint);
                if (!pick.race) break;
                created = BotCharacterFactory::Create(/*ownerSession=*/nullptr,
                    pick.name, pick.race, pick.cls, pick.gender);
            }
            if (!created.ok)
            {
                ++create_fail;
                last_create_reason = created.reason;
                continue;
            }
            ++success;
            // Mark distribution_level + JIT-tag with instance + role so the
            // setup pipeline post-completion hook (BotPopulationManager::
            // DriveSetupPipelines) can fire the matching queue intent once
            // the bot is fully geared/talented/placed in-world.
            // Tag format: "<KIND>:<INSTANCE_ID>:<ROLE>" e.g. "BG:128:DPS",
            // "LFG:271:HEAL". Parsed in the population manager.
            uint64 guid_low = created.guid.GetCounter();
            char const* kind_str = (req.kind == QueueKind::Bg) ? "BG" : "LFG";
            char tag_buf[48];
            std::snprintf(tag_buf, sizeof(tag_buf), "%s:%u:%s",
                          kind_str, req.instance_id, role_name);
            CharacterDatabase.PExecute(
                "UPDATE playerbot_v2_character SET distribution_level={}, jit_for_queue='{}', "
                "last_seen_at=NOW() WHERE character_guid_low={}",
                uint32(target_level), tag_buf, guid_low);
            // DEFERRED login (BG audit follow-up): Create's SaveToDB is
            // async -- a same-tick LoginBot loses the race and the session
            // is kicked with "Player::LoadFromDB failed" (observed: 542
            // JIT chars created in 30 min, 1 in world). SpawnNew solved
            // this with deferred_logins_; route through the same queue.
            Services::Population().DeferLogin(guid_low);
            // Kick-protection lease: between this login and the setup
            // pipeline's queue intent, the bot is overshoot the Reconcile
            // overflow kick would otherwise log straight back out
            // (observed live: online_seen pinned at TotalTarget while
            // every spawn_jit batch "succeeded"). 10 min covers the
            // longest setup pipeline; once queued/ported, the BG-queue
            // and BG-map guards take over.
            Services::Population().ProtectFromKick(guid_low, 10u * 60u * 1000u);
            Services::Perf().record_queue_fill_jit_spawned();
            (void)role;
        }
        // Per-spawn_jit-call summary. Without this, a queue request that
        // hits 5/5 silent Create failures looked indistinguishable from
        // a successful spawn — owner observed 2026-05-15: queued Ragefire,
        // QueueFill logged "JIT-spawning rest" but no JIT setup-complete
        // ever fired, no rows tagged jit_for_queue. Could be roll fail,
        // could be name collision, could be pool starvation — we need
        // to know which.
        if (success != count + reused || roll_fail || create_fail || reused)
            TC_LOG_INFO("playerbot.v2",
                "[QueueFill] spawn_jit role={} requested={} success={} (reused={}) roll_fail={} create_fail={}{}",
                role_name, count + reused, success, reused, roll_fail, create_fail,
                create_fail ? fmt::format(" last_reason='{}'", last_create_reason) : std::string());
    };

    // Skip JIT spawn entirely when caller asked for online-only fill
    // (BG top-up tick uses this to avoid spamming new JIT bots that
    // wouldn't complete setup before the BG's 90s invite window).
    if (!req.online_only)
    {
        if (queued_t < needs.tanks)   spawn_jit(Role::Tank,   needs.tanks - queued_t,   "TANK");
        if (queued_h < needs.healers) spawn_jit(Role::Healer, needs.healers - queued_h, "HEAL");
        if (queued_d < needs.dps)     spawn_jit(Role::Dps,    needs.dps - queued_d,     "DPS");
    }

    TC_LOG_INFO("playerbot.v2", "[QueueFill] faction={} queued online: {}T {}H {}D; JIT-spawning rest",
                want_alliance ? "ALLIANCE" : "HORDE", queued_t, queued_h, queued_d);
  }  // end faction_pass loop

    // The "online" portion completed synchronously — stamp completion latency
    // (typically <1ms) so we have data even for queues that didn't need JIT.
    // JIT completions get stamped separately in BotPopulationManager when
    // each newly-spawned bot finishes its setup pipeline and its queue
    // intent fires.
    uint32 const elapsed_ms = getMSTime() - start_ms_for_completion;
    Services::Perf().record_queue_fill_completion(Ms{elapsed_ms});
}

void BotQueueFiller::OnMatchEnd(Player* /*bot*/)
{
    // Retention window applied at hygiene cron — JIT bots whose
    // last_seen_at expired get logged out + deleted. Nothing to do per match.
}

} // namespace Playerbot::V2::Fleet

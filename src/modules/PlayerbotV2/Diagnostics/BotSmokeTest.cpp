#include "BotSmokeTest.h"

#include "PerfCounters.h"
#include "../Bot/BotRegistry.h"
#include "../Fleet/BotAccountMgr.h"
#include "../Fleet/BotCharacterFactory.h"
#include "../Fleet/BotComposition.h"
#include "../Fleet/BotIdentityRegistry.h"
#include "../Fleet/BotPopulationManager.h"
#include "../Fleet/BotSetupPipeline.h"
#include "../Session/BotSessionMgr.h"
#include "../Services.h"

#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "fmt/format.h"

#include <atomic>
#include <chrono>
#include <map>
#include <string_view>
#include <thread>

namespace Playerbot::V2::Diagnostics {

namespace {

std::atomic<bool> g_running{false};

// Comma-join helper — fmt::join needs fmt/ranges.h which the rest of V2 does
// not include, so format the list ourselves to keep the dependency footprint
// identical to BotInspector.cpp.
std::string CommaJoin(std::vector<std::string> const& v)
{
    std::string s;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i) s += ", ";
        s += v[i];
    }
    return s;
}

// Snapshot of the last completed run. Read by GM command after the worker
// thread finished. Mutex bridges the world-thread reader and worker writer.
std::mutex            g_result_mtx;
SmokeTestResult       g_last_result;

// Read setup_pipeline_state for a bot via the SAME query shape as
// BotInspector's ReadPipelineDbRow. Returns 0 (== no row) if the lookup
// fails. Safe to call from the worker thread because CharacterDatabase
// is the trinity DatabaseWorkerPool which is thread-safe.
uint8 ReadPipelineState(BotId id)
{
    auto res = CharacterDatabase.PQuery(
        "SELECT setup_pipeline_state FROM playerbot_v2_character "
        "WHERE character_guid_low={}",
        uint64(id));
    if (!res || !res->GetRowCount()) return 0;
    return res->Fetch()[0].GetUInt8();
}

// Walk the pipeline failure ring for a bot and count entries. Used by the
// "no_failures" assertion. Safe from any thread (ring uses an internal mutex).
size_t CountPipelineFailures(BotId id)
{
    size_t n = 0;
    Fleet::PipelineFailureRing::Instance().ForEach(uint64(id),
        [&](size_t /*i*/, Fleet::PipelineFailureEntry const& /*e*/) { ++n; });
    return n;
}

// Walk the per-bot intent history ring and count. Safe from any thread.
size_t CountIntentHistory(BotId id)
{
    if (!Playerbot::Services::Initialized()) return 0;
    size_t n = 0;
    Playerbot::Services::Registry().for_each_intent_history(id,
        [&](size_t /*i*/, Playerbot::IntentHistoryEntry const& /*e*/) { ++n; });
    return n;
}

// Builds the worker-thread test body. Captures `bot_ids` + thresholds; runs
// the wait/assert phase. Writes `g_last_result` and clears `g_running` on
// exit. Logs the final report to TC_LOG_INFO so CI can grep Server.log.
void RunWorker(std::vector<BotId> bot_ids, LaunchOptions opts,
               uint64 baseline_intents_dropped)
{
    using clock = std::chrono::steady_clock;
    auto const start = clock::now();
    auto const deadline = start + std::chrono::milliseconds(opts.timeout_ms);

    {
        std::lock_guard lk(g_result_mtx);
        g_last_result.phase = SmokeTestResult::Phase::Waiting;
    }

    // Phase 2: Wait until every bot's setup_pipeline_state == AllDone, OR
    // timeout. Polling cadence is 1s — fast enough to keep the test under a
    // minute on a happy path, slow enough that we don't hammer the DB.
    //
    // 2026-05-21: ALSO track in-world arrival per bot. Under heavy load the
    // async login chain can take many seconds; bots never enter
    // Services::Registry() within the timeout, so DriveSetupPipelines never
    // ticks them and their state stays 0x00. Pre-fix the failure looked like
    // a pipeline regression; with this distinguishing instrumentation, the
    // assertion phase can call out "login-stuck" vs "pipeline-stuck"
    // separately.
    std::vector<uint8> final_states(bot_ids.size(), 0);
    std::vector<bool>  in_world(bot_ids.size(), false);
    bool all_done = false;
    while (clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        size_t done = 0;
        for (size_t i = 0; i < bot_ids.size(); ++i)
        {
            if (final_states[i] == Fleet::SetupBit::AllDone) { ++done; continue; }
            const uint8 s = ReadPipelineState(bot_ids[i]);
            final_states[i] = s;
            if (s == Fleet::SetupBit::AllDone) ++done;
            // Latch in-world status — once observed, stays true even if the
            // bot logs out before we check again (we want to know whether the
            // bot EVER entered the world during the window).
            if (!in_world[i])
            {
                ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(bot_ids[i]);
                if (ObjectAccessor::FindConnectedPlayer(g) != nullptr)
                    in_world[i] = true;
            }
        }
        if (done == bot_ids.size()) { all_done = true; break; }
    }

    // Phase 3: assertions.
    SmokeTestResult result;
    result.requested_count = opts.count;
    result.spawned_count   = static_cast<uint32>(bot_ids.size());
    // Carry the roll_misses count from Launch through to the worker via
    // g_last_result (set in Launch); RunWorker doesn't see opts. Read
    // back under the lock.
    {
        std::lock_guard lk(g_result_mtx);
        result.roll_misses = g_last_result.roll_misses;
    }
    result.bot_ids         = bot_ids;
    // started_at preserved from Launch (which copied it into g_last_result).
    {
        std::lock_guard lk(g_result_mtx);
        result.started_at = g_last_result.started_at;
    }
    result.phase           = SmokeTestResult::Phase::Asserting;

    auto& a = result.assertions;

    // 3a-prologue) In-world arrival assertion. Distinguishes "bots never
    // entered Services::Registry()" (async login slow/stuck) from "bots
    // entered Registry but pipeline didn't tick them" (DriveSetupPipelines
    // budget starvation). Critical for triage at scale — 1000+ existing
    // bot population caused the smoketest's new bots to never reach world
    // within 60s, making the failure look like a pipeline regression when
    // it was really a session-throughput regression.
    size_t reached_world = 0;
    for (size_t i = 0; i < in_world.size(); ++i) if (in_world[i]) ++reached_world;
    {
        SmokeTestAssertion as;
        as.name = "bots_reached_world";
        if (reached_world == bot_ids.size())
        {
            as.passed = true;
            as.detail = fmt::format("{}/{} bots entered world within timeout",
                                    reached_world, bot_ids.size());
        }
        else
        {
            as.passed = false;
            std::vector<std::string> stuck_logins;
            for (size_t i = 0; i < bot_ids.size(); ++i)
                if (!in_world[i])
                    stuck_logins.push_back(fmt::format("bot {} (no FindConnectedPlayer)", bot_ids[i]));
            as.detail = fmt::format(
                "{}/{} bots entered world — {} stuck in async login ({}). "
                "Likely cause: high concurrent session count starving the "
                "login pipeline. Pipeline state for these bots will be 0x00.",
                reached_world, bot_ids.size(),
                stuck_logins.size(), CommaJoin(stuck_logins));
        }
        a.push_back(std::move(as));
    }

    // 3a) Every bot's pipeline_state == 0xFF.
    {
        SmokeTestAssertion as;
        as.name = "pipeline_complete";
        std::vector<std::string> stragglers_in_world;
        std::vector<std::string> stragglers_login_stuck;
        for (size_t i = 0; i < bot_ids.size(); ++i)
        {
            if (final_states[i] == Fleet::SetupBit::AllDone) continue;
            const auto entry = fmt::format("bot {} state=0x{:02X}",
                                            bot_ids[i], uint32(final_states[i]));
            if (in_world[i])
                stragglers_in_world.push_back(entry);
            else
                stragglers_login_stuck.push_back(entry);
        }
        const size_t total_stragglers =
            stragglers_in_world.size() + stragglers_login_stuck.size();
        if (total_stragglers == 0)
        {
            as.passed = true;
            as.detail = fmt::format("{}/{} bots reached AllDone",
                                    bot_ids.size(), bot_ids.size());
        }
        else
        {
            as.passed = false;
            // Separate the two failure modes in the detail string so the
            // operator can immediately see which root cause is biting.
            as.detail = fmt::format(
                "timeout {}ms — {} straggler(s): in_world+pipeline_stuck={} ({}); "
                "login_stuck={} ({})",
                opts.timeout_ms, total_stragglers,
                stragglers_in_world.size(),
                stragglers_in_world.empty() ? "none" : CommaJoin(stragglers_in_world),
                stragglers_login_stuck.size(),
                stragglers_login_stuck.empty() ? "none" : CommaJoin(stragglers_login_stuck));
        }
        a.push_back(std::move(as));
    }

    // 3b) PipelineFailureRing must be empty for every bot.
    {
        SmokeTestAssertion as;
        as.name = "no_pipeline_failures";
        std::vector<std::string> failed;
        for (BotId id : bot_ids)
        {
            const size_t n = CountPipelineFailures(id);
            if (n != 0)
                failed.push_back(fmt::format("bot {} ({} failure(s))", id, n));
        }
        if (failed.empty())
        {
            as.passed = true;
            as.detail = "no failures recorded";
        }
        else
        {
            as.passed = false;
            as.detail = fmt::format("{} bot(s) with pipeline failures: {}",
                                    failed.size(), CommaJoin(failed));
        }
        a.push_back(std::move(as));
    }

    // 3c) Each bot's intent history must have at least min_intents entries.
    //     A zero ring means the AI never fired a single intent in 60s — that's
    //     a regression in BotAI, the snapshot publisher, or the intent
    //     executor. Distinct from "bots that haven't spawned yet" because we
    //     already gated on AllDone above.
    {
        SmokeTestAssertion as;
        as.name = "intent_history_threshold";
        std::vector<std::string> too_quiet;
        for (BotId id : bot_ids)
        {
            const size_t n = CountIntentHistory(id);
            if (n < opts.min_intents)
                too_quiet.push_back(fmt::format("bot {} ({} intents)", id, n));
        }
        if (too_quiet.empty())
        {
            as.passed = true;
            as.detail = fmt::format("each bot >= {} intents", opts.min_intents);
        }
        else
        {
            as.passed = false;
            as.detail = fmt::format("{} bot(s) below threshold {}: {}",
                                    too_quiet.size(), opts.min_intents,
                                    CommaJoin(too_quiet));
        }
        a.push_back(std::move(as));
    }

    // 3d) PerfCounters intents_dropped_total must not have grown. A drop
    //     is "AI worker produced an intent but per-bot IntentQueue was full"
    //     — sustained drops mean the world-thread executor is starving and
    //     the bot is effectively desynced from its own AI.
    {
        SmokeTestAssertion as;
        as.name = "no_dropped_intents";
        const uint64 now_dropped = Playerbot::Services::Perf().snapshot().intents_dropped_total;
        const uint64 delta = (now_dropped >= baseline_intents_dropped)
                                 ? (now_dropped - baseline_intents_dropped) : 0u;
        if (delta == 0)
        {
            as.passed = true;
            as.detail = fmt::format("dropped_total stable at {}", now_dropped);
        }
        else
        {
            as.passed = false;
            as.detail = fmt::format("dropped_total grew {} → {} (+{})",
                                    baseline_intents_dropped, now_dropped, delta);
        }
        a.push_back(std::move(as));
    }

    // 3d.bg) (Wave C — only when opts.sample_bg) Walk each bot's last fired
    //        rule and count BG-flavored ones. If at least one bot is firing
    //        `idle:bg_*` rules during the test window, the BG dispatcher
    //        path is alive end-to-end. If zero, either the BG queue-filler
    //        never sent the bots to queue OR a regression broke the per-BG
    //        snapshot fields the rules read. Either way the operator wants
    //        to know.
    //
    //        Reads BotAI::last_rule_fired() via the registry's for_each
    //        helper — the const char* pointer is naturally atomic on
    //        x86-64 and the underlying storage is static, so a worker-
    //        thread read can't tear or dangle. We tolerate a one-tick
    //        stale read as harmless for this diagnostic.
    if (opts.sample_bg)
    {
        SmokeTestAssertion as;
        as.name = "bg_dispatcher_fires";
        size_t bg_firing = 0, dungeon_firing = 0, sampled = 0;
        // Wave D: per-rule histogram so a single smoketest run gives a
        // coverage view ("which BG idle rules ever fired"). The operator
        // can compare this against the full registered-rule list to see
        // which dispatcher paths are dark. Iterating across BGs is then
        // a matter of running this command with the queue-filler biased
        // toward different bg_type_ids — the histogram accumulates per-
        // run breadcrumbs across the whole test session via Server.log.
        std::map<std::string, size_t> rule_histo;
        std::vector<std::string> per_bot_rules;
        if (Playerbot::Services::Initialized())
        {
            Playerbot::Services::Registry().for_each(
                [&](BotId id, Playerbot::BotRegistryEntry const& e)
                {
                    if (!e.ai) return;
                    ++sampled;
                    char const* rule = e.ai->last_rule_fired();
                    if (!rule) return;
                    if (std::string_view(rule).starts_with("idle:bg_"))
                    {
                        ++bg_firing;
                        per_bot_rules.push_back(
                            fmt::format("bot {}: {}", id, rule));
                        rule_histo[rule]++;
                    }
                    else if (std::string_view(rule).starts_with("idle:dungeon_"))
                    {
                        ++dungeon_firing;
                    }
                });
        }
        if (bg_firing > 0)
        {
            as.passed = true;
            as.detail = fmt::format(
                "{} of {} bots firing idle:bg_* rules; {} firing dungeon",
                bg_firing, sampled, dungeon_firing);
        }
        else
        {
            as.passed = false;
            as.detail = fmt::format(
                "0 of {} bots fired any idle:bg_* rule "
                "(BG queue-filler off, no eligible BG, or dispatcher broken)",
                sampled);
        }
        a.push_back(std::move(as));

        // Surface the per-bot rule list as a triage breadcrumb so the
        // operator can see WHICH BG rules fired — useful for catching
        // "only the chase_carrier rule ever fires" regressions.
        if (!per_bot_rules.empty())
        {
            for (auto& s : per_bot_rules)
                result.failure_diags.push_back(std::move(s));
        }

        // Wave D: rule-coverage histogram pushed to Server.log so an
        // operator running `.playerbot smoketest bglive N` repeatedly
        // (or under different BG configurations) can grep:
        //
        //     grep "bg_rule_coverage" Server.log | sort -u
        //
        // to get the union of every BG rule fired across the test
        // matrix — i.e., the live-mode coverage report Wave D asked for.
        for (auto const& [rule_name, hits] : rule_histo)
        {
            TC_LOG_INFO("playerbot.v2.smoketest",
                "bg_rule_coverage rule={} hits={}",
                rule_name, hits);
        }
    }

    // 3e) CharacterDatabase async queue depth must be < 1000. A persistent
    //     backlog there is the canonical symptom of the setup-pipeline loop
    //     the diag surface was built to debug; catching it here means CI
    //     fails before the loop melts a real test environment.
    {
        SmokeTestAssertion as;
        as.name = "db_queue_bounded";
        const size_t qs = CharacterDatabase.QueueSize();
        if (qs < 1000u)
        {
            as.passed = true;
            as.detail = fmt::format("CharacterDatabase queue = {}", qs);
        }
        else
        {
            as.passed = false;
            as.detail = fmt::format("CharacterDatabase queue = {} (>= 1000)", qs);
        }
        a.push_back(std::move(as));
    }

    // Aggregate PASS/FAIL.
    bool overall = !bot_ids.empty();
    for (auto const& as : a) if (!as.passed) { overall = false; break; }
    if (!all_done) overall = false;

    // Failure triage: list the failing bot IDs + pipeline-failure breadcrumbs
    // so the user can then run `.playerbot diag <name>` from the world thread
    // for the full dump. We deliberately do NOT call DiagBot()/HealthReport()
    // here — those iterate Services state that is only race-free on the
    // world thread (Population::setup_done_cache_, snapshot publisher), and
    // calling them from the worker thread risks intermittent dirty reads
    // during the report itself. The CI-friendly summary below is enough to
    // decide PASS/FAIL; deep-dive uses the existing /diag command.
    if (!overall)
    {
        for (size_t i = 0; i < bot_ids.size(); ++i)
        {
            const BotId id = bot_ids[i];
            const bool stragg = final_states[i] != Fleet::SetupBit::AllDone;
            const size_t failure_count = CountPipelineFailures(id);
            const size_t intent_count  = CountIntentHistory(id);
            const bool quiet  = intent_count < opts.min_intents;
            if (!stragg && failure_count == 0 && !quiet) continue;
            std::string line = fmt::format(
                "bot {} state=0x{:02X} pipeline_failures={} intents={}",
                id, uint32(final_states[i]), failure_count, intent_count);
            // Append the most recent pipeline-failure step name(s) to the
            // summary line so the failing step is visible without /diag.
            if (failure_count != 0)
            {
                std::string steps;
                Fleet::PipelineFailureRing::Instance().ForEach(uint64(id),
                    [&](size_t /*i*/, Fleet::PipelineFailureEntry const& e)
                    {
                        if (!steps.empty()) steps += ",";
                        steps += e.step_name;
                    });
                line += fmt::format(" failed_steps=[{}]", steps);
            }
            result.failure_diags.push_back(std::move(line));
        }
    }

    // Thread-safe summary (atomics + DB queue + per-bot intent counts) in
    // place of HealthReport — gives the user the headline numbers without
    // touching world-thread-only state from the worker thread.
    {
        auto p = Playerbot::Services::Perf().snapshot();
        const size_t qs = CharacterDatabase.QueueSize();
        std::string s;
        s.reserve(512);
        s += fmt::format("==== Smoketest summary ====\n");
        s += fmt::format("Intents emitted/executed/failed/dropped : {}/{}/{}/{}\n",
                         p.intents_emitted_total, p.intents_executed_total,
                         p.intents_failed_total,  p.intents_dropped_total);
        s += fmt::format("CharacterDatabase async queue depth     : {}\n", qs);
        s += fmt::format("Snapshots published total                : {}\n",
                         p.snapshots_published_total);
        s += fmt::format("World updates total                      : {}\n",
                         p.world_updates_total);
        result.health_report_tail = std::move(s);
    }

    result.pass        = overall;
    result.completed   = true;
    result.phase       = SmokeTestResult::Phase::Done;
    result.finished_at = std::chrono::system_clock::now();

    // Belt-and-suspenders cleanup: unregister every bot we put on the
    // priority-setup list, regardless of pass/fail. DriveSetupPipelines
    // already auto-removes entries on AllDone, but failed runs (timeout,
    // login-stuck) leave stale ids in the set. Force-clear here so a
    // subsequent smoketest invocation gets a clean slate.
    for (BotId id : bot_ids)
        Playerbot::Services::Population().UnregisterPrioritySetup(uint64(id));

    {
        std::lock_guard lk(g_result_mtx);
        g_last_result = result;
    }

    // Log the verdict so CI can grep Server.log without a follow-up command.
    if (overall)
        TC_LOG_INFO("playerbot.v2.smoketest",
            "PASS: {}/{} bots completed setup, no failures, no dropped intents",
            result.spawned_count, result.spawned_count);
    else
        TC_LOG_ERROR("playerbot.v2.smoketest",
            "FAIL: {} bots — see assertions:\n{}",
            result.spawned_count, Render(result));

    g_running.store(false, std::memory_order_release);
}

} // anonymous

bool IsRunning()
{
    return g_running.load(std::memory_order_acquire);
}

SmokeTestResult Snapshot()
{
    std::lock_guard lk(g_result_mtx);
    return g_last_result;
}

bool Launch(uint32 owner_account_id, LaunchOptions const& opts_in)
{
    if (!Playerbot::Services::Initialized()) return false;

    // Single-flight gate. Two concurrent runs would race on the spawn budget
    // and produce nondeterministic intent counts.
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true,
                                           std::memory_order_acq_rel))
        return false;

    LaunchOptions opts = opts_in;
    if (opts.count == 0) opts.count = 1;
    if (opts.count > 50) opts.count = 50;
    if (opts.timeout_ms == 0) opts.timeout_ms = 60'000;

    // Capture baselines BEFORE we spawn so the dropped-intents delta is
    // attributable to this run, not to whatever was happening on the realm
    // before the test started.
    const uint64 baseline_dropped =
        Playerbot::Services::Perf().snapshot().intents_dropped_total;

    // Phase 1: spawn synchronously on the world thread (we are on the world
    // thread because Launch is called from the GM command handler). We need
    // the WorldSession only at submission time — BotCharacterFactory::Create
    // requires one to set up the new character's account binding. We pull a
    // fresh session for the requesting account; if none is online we abort,
    // because background spawn would leak account-side state on the next
    // operator login.
    WorldSession* sess = sWorld->FindSession(owner_account_id);
    if (!sess)
    {
        g_running.store(false, std::memory_order_release);
        return false;
    }

    std::vector<BotId> bot_ids;
    bot_ids.reserve(opts.count);
    uint32 created = 0, login_ok = 0;
    uint32 roll_misses = 0;       // diagnostic: how many roll/create failures
    // Per-slot retry: under heavy load BotComposition::Roll can return
    // race=0 transiently (e.g. name pool starvation, composition table
    // bracket starvation). The pre-2026-05-22 loop did `break` on race=0
    // — that aborted the ENTIRE spawn batch the moment a single roll
    // missed, producing the "4 of 10 bots not spawned" symptom observed
    // in smoketest results on a busy realm. Each slot now gets ONE
    // retry; further misses on the same slot drop the slot (rather
    // than spinning indefinitely if the pool is truly empty).
    for (uint32 i = 0; i < opts.count; ++i)
    {
        Playerbot::V2::BotComposition::Pick picked{};
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            picked = Playerbot::V2::BotComposition::Roll();
            if (picked.race != 0) break;
            ++roll_misses;
        }
        if (picked.race == 0)
            continue;  // both attempts missed — drop this slot, move on

        auto r = Playerbot::V2::BotCharacterFactory::Create(
            sess, picked.name, picked.race, picked.cls, picked.gender);
        if (!r.ok) continue;
        ++created;

        auto login = Playerbot::Services::SessionMgr().LoginBot(r.guid);
        // Only enroll successfully-logged-in bots in the wait list.
        // Previously failed logins (session-cap rejection, account-lock,
        // JIT factory fail) still got added — they never entered world,
        // never ran SetupPipeline, never reached AllDone, then the 60s
        // poll loop reported them as "stragglers" and the test FAILED
        // even when survivors completed perfectly. Gate on login.ok so
        // the smoketest measures actual pipeline behavior, not session
        // budget.
        if (!login.ok) continue;
        ++login_ok;
        const BotId id = r.guid.GetCounter();
        bot_ids.push_back(id);
        // Register on the priority-setup list so DriveSetupPipelines drains
        // this bot ahead of the regular 10-per-tick budget cap. Without this
        // the smoketest under 1000-bot load hits cap starvation: new bots
        // sit at the tail of the hash-ordered registry, never reached
        // within the timeout. Auto-removed by DriveSetupPipelines when
        // AllDone; cleanup pass at end of RunWorker handles aborted runs.
        Playerbot::Services::Population().RegisterPrioritySetup(uint64(id));
    }

    {
        std::lock_guard lk(g_result_mtx);
        g_last_result = SmokeTestResult{};
        g_last_result.phase           = SmokeTestResult::Phase::Spawning;
        g_last_result.requested_count = opts.count;
        g_last_result.spawned_count   = created;
        g_last_result.roll_misses     = roll_misses;
        g_last_result.bot_ids         = bot_ids;
        g_last_result.started_at      = std::chrono::system_clock::now();
    }

    if (bot_ids.empty())
    {
        // Nothing to wait for — record as failed test and bail.
        SmokeTestResult fail;
        fail.requested_count = opts.count;
        fail.completed       = true;
        fail.phase           = SmokeTestResult::Phase::Aborted;
        fail.pass            = false;
        fail.assertions.push_back({"spawn_succeeded", false,
            fmt::format("0/{} bots created (factory rejected every roll)",
                        opts.count)});
        fail.finished_at = std::chrono::system_clock::now();
        {
            std::lock_guard lk(g_result_mtx);
            g_last_result = fail;
        }
        TC_LOG_ERROR("playerbot.v2.smoketest",
            "FAIL: spawn produced 0 bots (requested {})", opts.count);
        g_running.store(false, std::memory_order_release);
        return true;   // launched, but already finished
    }

    TC_LOG_INFO("playerbot.v2.smoketest",
        "Smoketest started — requested={} created={} login_submitted={} "
        "roll_misses={} timeout_ms={}",
        opts.count, created, login_ok, roll_misses, opts.timeout_ms);

    // Phase 2 + 3 happen on a detached worker so the world thread keeps
    // ticking — without that, the async DB callbacks that finish login + the
    // setup-pipeline never fire, the bots never reach AllDone, and the test
    // would always time out.
    std::thread(RunWorker, std::move(bot_ids), opts, baseline_dropped).detach();
    return true;
}

std::string Render(SmokeTestResult const& r)
{
    std::string out;
    out.reserve(2048);

    if (r.phase == SmokeTestResult::Phase::NotStarted)
        return "Smoketest: no run yet.\n";

    if (!r.completed)
    {
        out += fmt::format("Smoketest: in progress (phase={}, spawned={}/{})\n",
            [&]() -> char const* {
                switch (r.phase) {
                    case SmokeTestResult::Phase::Spawning:  return "spawning";
                    case SmokeTestResult::Phase::Waiting:   return "waiting";
                    case SmokeTestResult::Phase::Asserting: return "asserting";
                    default: return "?";
                }
            }(), r.spawned_count, r.requested_count);
        return out;
    }

    out += fmt::format("==== Smoketest {} ====\n", r.pass ? "PASS" : "FAIL");
    out += fmt::format("Requested : {}\n", r.requested_count);
    out += fmt::format("Spawned   : {}{}\n", r.spawned_count,
        r.roll_misses > 0
            ? fmt::format(" ({} composition roll miss(es), retried)", r.roll_misses)
            : std::string());
    if (!r.bot_ids.empty())
    {
        std::string ids_line;
        for (size_t i = 0; i < r.bot_ids.size(); ++i)
        {
            if (i) ids_line += ",";
            ids_line += std::to_string(r.bot_ids[i]);
        }
        out += fmt::format("BotIds    : {}\n", ids_line);
    }
    out += "Assertions:\n";
    for (auto const& a : r.assertions)
        out += fmt::format("  [{}] {:<28} : {}\n",
                            a.passed ? "PASS" : "FAIL",
                            a.name, a.detail);

    if (!r.pass)
    {
        if (!r.failure_diags.empty())
        {
            out += "==== Failing-bot diagnostics ====\n";
            for (auto const& d : r.failure_diags)
            {
                out += d;
                out += "----\n";
            }
        }
    }

    if (!r.health_report_tail.empty())
        out += r.health_report_tail;

    return out;
}

} // namespace Playerbot::V2::Diagnostics

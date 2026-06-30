// BotSmokeTest - End-to-end regression harness driving the PlayerbotV2
// spawn → setup-pipeline → AI-tick path. Used by `.playerbot smoketest
// [count]` to catch regressions in fleet bring-up against a clean realm.
//
// Design constraints (see /diag and HealthReport documentation):
//   * Spawn submits async DB callbacks that complete on the world thread.
//     We therefore CANNOT block the world thread for the wait — the same
//     thread is responsible for draining the callbacks that finish login.
//   * The harness runs as a detached worker thread that sleep-polls the
//     V2 services + CharacterDatabase. The world thread keeps ticking,
//     callbacks fire, bots move through pipeline → AllDone, intents flow.
//   * Results are written to a thread-safe LastResult buffer + the server
//     log so a CI runner can scrape Server.log for PASS/FAIL.

#pragma once

#include "Bot/BotTypes.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace Playerbot::V2::Diagnostics {

struct SmokeTestAssertion
{
    std::string name;        // e.g. "pipeline_complete", "no_failures"
    bool        passed = false;
    std::string detail;      // failure detail (passing => empty)
};

struct SmokeTestResult
{
    enum class Phase : uint8 { NotStarted, Spawning, Waiting, Asserting, Done, Aborted };

    Phase                            phase = Phase::NotStarted;
    bool                             completed = false;
    bool                             pass = false;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point finished_at;
    uint32                           requested_count = 0;
    uint32                           spawned_count   = 0;
    // Diagnostic for the "spawn-loss" symptom (Requested=10, Spawned=6
    // observed under 1000-bot load): how many times the spawn loop hit
    // BotComposition::Roll() returning race=0. Each iteration of the
    // loop retries up to once before dropping the slot. Reported in
    // Launch's TC_LOG_INFO and in the result's Render() so the operator
    // can distinguish "composition table starved" from "pipeline stuck".
    uint32                           roll_misses     = 0;
    std::vector<BotId>               bot_ids;
    std::vector<SmokeTestAssertion>  assertions;
    std::string                      health_report_tail;
    // Per-failed-bot diag dumps (only populated on failure for triage).
    std::vector<std::string>         failure_diags;
};

// Launches the smoke test on a detached worker thread. Returns false if a
// test is already in flight (only one can run at a time so they don't race
// on shared spawn budgets / DB row counts). Safe to call from world thread.
//
// `count` clamps to 1..50 to match `.playerbot spawn`'s hard cap. The
// caller's WorldSession is needed because BotCharacterFactory::Create
// requires one; we capture only its account id so the worker thread does
// NOT touch the session pointer (lifetime not guaranteed across a 60s wait).
struct LaunchOptions
{
    uint32  count           = 5;
    // 2026-05-21: default bumped 60s → 180s. Under heavy concurrent
    // session load (1000+ existing bots), async login alone can take
    // 30-60s; the original 60s window left almost no budget for the
    // setup pipeline. 180s gives the failure mode time to actually be
    // "pipeline" not "login still queued". CLI users can override.
    uint32  timeout_ms      = 180'000;
    uint32  min_intents     = 5;        // per-bot lower bound for intent ring
    bool    cleanup_on_pass = false;    // logout + delete created bots when PASS
    // Wave C: when true, after the standard pipeline/intent assertions, the
    // worker also samples each bot's last fired rule and asserts that AT
    // LEAST ONE bot fired an `idle:bg_*` rule. Used by `.playerbot smoketest
    // bglive` to validate that the BG dispatcher path is alive end-to-end —
    // bot spawned → queue rules fired → bot ported → BG-role rule fired.
    //
    // Reads `BotAI::last_rule_fired()` from the worker thread. The pointer
    // refers to a static string literal (every set_last_rule_fired call
    // passes a constexpr char*), so the lifetime is safe; the pointer load
    // is naturally atomic on aligned x86-64. A torn read would just give a
    // slightly stale rule name, which doesn't change the assertion outcome.
    bool    sample_bg       = false;
};
bool Launch(uint32 owner_account_id, LaunchOptions const& opts);

// True when a test is currently running. Used by the GM command to refuse
// re-entry with a friendly message.
bool IsRunning();

// Returns a copy of the most recent finished result. Phase is NotStarted
// when no run has completed yet.
SmokeTestResult Snapshot();

// Returns a multi-line human-readable rendering of `r` — used by the GM
// command to print the final report to the requesting handler.
std::string Render(SmokeTestResult const& r);

} // namespace Playerbot::V2::Diagnostics

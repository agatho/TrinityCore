// BotSessionMgr - Owns headless WorldSessions for V2 bots.
//
// Per-tick driver: V2's Module::OnWorldUpdate calls Update(diff) which ticks
// each session's WorldSession::Update (so the async login callbacks fire) and
// reaps dead sessions.
//
// Bot sessions are NOT registered with sWorld->AddSession() — that path
// kicks any other session sharing the same account id, which would log out
// the GM that is spawning bots from their own characters. Instead we keep a
// parallel session list owned by this manager. That trades the world's
// queueing/limit logic (irrelevant for bots) for predictable lifecycles.

#pragma once

#include "ObjectGuid.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Playerbot::V2 {

class BotSession;

class BotSessionMgr
{
public:
    BotSessionMgr() = default;
    ~BotSessionMgr();

    BotSessionMgr(BotSessionMgr const&) = delete;
    BotSessionMgr& operator=(BotSessionMgr const&) = delete;

    // Begin headless login for a marked-as-bot character. Returns:
    //   ok = true and reason empty on submit;
    //   ok = false with reason populated otherwise (no such character / not
    //   marked / already logged in / DB lookup failed / holder init failed).
    struct LoginResult { bool ok = false; std::string reason; };
    LoginResult LoginBot(ObjectGuid playerGuid);

    // Trigger logout for an active bot session. Idempotent. Returns true if
    // a matching session was found and asked to logout.
    bool LogoutBot(ObjectGuid playerGuid);

    // Batch login: walk BotIdentityRegistry::snapshot_ids() and submit a
    // login for each marked bot not already in-world or in-flight. Returns
    // (attempted, succeeded). Safe to re-run; idempotent for already-logged-
    // in bots. Bounded: skips when active_count() >= cap to avoid DB flood.
    struct BatchResult { uint32 attempted = 0; uint32 succeeded = 0; uint32 skipped_already_in_world = 0; };
    BatchResult LoginAll(uint32 cap);

    // Batch logout: kick every active V2 bot session. Returns count kicked.
    uint32 LogoutAll();

    // Per-tick driver. Iterates sessions and calls WorldSession::Update so
    // the async login callbacks (and any other deferred work) fire.
    void Update(uint32 diff);

    // Number of currently-tracked sessions (alive or finishing logout).
    size_t active_count() const;

    // True when this character is driven by one of OUR headless BotSessions.
    // False for a real-client character running self-AI (`.playerbot self`):
    // those must NEVER receive manufactured client packets (teleport acks
    // etc.) — the real client performs the genuine handshake, and forging it
    // desyncs the client (stuck loading screen on LFG teleport, live
    // 2026-06-11).
    bool IsHeadless(ObjectGuid playerGuid) const;

private:
    mutable std::mutex                                     mtx_;
    std::unordered_map<ObjectGuid, std::shared_ptr<BotSession>> sessions_;
};

} // namespace Playerbot

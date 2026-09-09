// BotSession - Headless WorldSession for V2 bots.
//
// Inherits WorldSession with a null socket. The `is_bot` constructor flag
// enables the TRINITY_PLAYERBOT_V2 guards in the base (skip idle-disconnect,
// skip socket close in dtor).
//
// Lifecycle:
//   1. .playerbot login <name> calls BotSessionMgr::LoginBot.
//   2. Mgr looks up the character's account id, constructs a BotSession with
//      that account, calls BotSession::BeginLogin(playerGuid).
//   3. BeginLogin builds a LoginQueryHolder and submits via
//      AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(holder));
//      the callback calls WorldSession::HandlePlayerLogin(holder), which
//      builds the Player and brings them in-world via the standard core path.
//   4. Once Player is in-world, V2's hook OnPlayerLogin sees the registered
//      bot id (set by BotIdentityRegistry) and attaches BotAI.
//   5. BotSessionMgr::Update is driven from Module::OnWorldUpdate so the
//      session ProcessQueryCallbacks runs and deferred work progresses.

#pragma once

#include "WorldSession.h"
#include "ObjectGuid.h"

// V2's BotSession lives in `Playerbot::V2`.
namespace Playerbot::V2 {

class BotSession final : public WorldSession
{
public:
    BotSession(uint32 accountId, std::string name);
    ~BotSession() override = default;

    BotSession(BotSession const&) = delete;
    BotSession& operator=(BotSession const&) = delete;

    // Submit the async login query holder for the given character guid.
    // Returns false on duplicate-login or holder init failure.
    bool BeginLogin(ObjectGuid characterGuid);

    // Per-tick driver. Hides (non-virtual) WorldSession::Update because the
    // base method dereferences m_Socket[REALM] in the cleanup block — and a
    // bot has no socket. Static dispatch via shared_ptr<BotSession> in the
    // mgr picks ours instead. Returns false only when KickPlayer was called
    // (forceExit set) so BotSessionMgr can drop the session from its map.
    bool Update(uint32 diff, class PacketFilter& updater);

    // KickPlayer override (non-virtual; static-dispatched by mgr). Base
    // implementation iterates m_Socket and sets forceExit only if it found
    // an open socket — bots have no socket, so the base is a no-op. We
    // set forceExit directly so the next Update tick observes it, fires
    // LogoutPlayer, and returns false to be reaped.
    void KickPlayer(std::string_view reason);

    // Bots have no client — drop outbound packets. Base method is virtual.
    void SendPacket(WorldPacket const* packet, bool forced = false) override;
};

} // namespace Playerbot::V2

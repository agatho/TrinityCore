// PlayerbotControl addon — server-side wire endpoint.
//
// Contract: every CHAT_MSG_ADDON whisper from an account that owns at least
// one V2 bot is inspected. If the body starts with the PBC frame header
// "1|<seqHex>|<idx>|<tot>|MTYPE|..." we dispatch to the appropriate handler
// and emit replies back via a synthesized addon-whisper packet to the same
// session. Non-PBC traffic passes through untouched.
//
// Reassembly buffers live per-account with a 5s TTL. Outbound EVENT_PUSH
// frames carry a retry spool keyed by seq; ACK<seq> drops the entry, and
// a 10s no-ack triggers one retransmit.
//
// See ../Addon/PlayerbotControl/Comms.lua for the canonical wire format
// and ../Addon/PlayerbotControl/SERVER_INTEGRATION_STUBS.md for the
// per-message contract this file fulfils.

#pragma once

#include "Bot/BotTypes.h"
#include <string>

class Player;

namespace Playerbot::V2::AddonControl {

constexpr char const* kPrefix       = "PBC";
constexpr char const* kProtoVersion = "1";
constexpr char const* kFleetTarget  = "PBCFLEET";
constexpr size_t      kMaxChunkBytes  = 240;
constexpr size_t      kMaxFrameBytes  = 4096;
constexpr int         kReassemblyTtlSec = 5;

// Called from the PlayerScript::OnChat (5-arg whisper overload) hook for
// every addon whisper. Returns true when the message was a PBC frame and
// got dispatched (caller doesn't care — return is informational for tests).
// The hook fires for BOTH self-whispers (the common case — addon sends to
// the player's own name) AND whispers to a bot character; both work.
bool OnAddonWhisper(Player* sender, Player* receiver,
                    std::string const& prefix, std::string const& body);

// Tear-down hook — reaps the per-account reassembly state and retry spool
// when a session logs out. Called from PlayerbotV2 module on OnSessionLogout.
void OnSessionLogout(uint32 sessionAccountId);

} // namespace

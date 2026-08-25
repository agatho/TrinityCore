// PlayerbotHooks.h
// Hook points called from TrinityCore core code. Per v2/MODULE_LAYOUT.md §4,
// total hook insertion sites in core are budgeted at ≤30.
//
// When BUILD_PLAYERBOT_V2=ON, calls dispatch into Playerbot::V2::Module.
// When OFF, calls resolve to inline-empty bodies and are dead-code-eliminated.

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <string>

class Player;
class Unit;
class Aura;
class Group;

namespace Playerbot::Hooks {

#if TRINITY_PLAYERBOT_V2

// Real declarations — bodies in PlayerbotHooks.cpp dispatch to V2::Module.
void OnPlayerLogin(Player* p);
void OnPlayerLogout(Player* p);
void OnLevelUp(Player* p, uint8 new_level);
void OnDeath(Unit* victim, Unit* killer);
void OnResurrect(Player* p);
void OnSpecChanged(Player* p, uint8 new_spec);
void OnDamageDealt(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
void OnDamageTaken(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id);
void OnHealReceived(Unit* healer, Unit* target, int32 amount, uint32 spell_id);
void OnAuraApplied(Unit* target, Aura* aura);
void OnAuraRemoved(Unit* target, Aura* aura);
void OnGroupMemberJoined(Group* g, Player* p);
void OnGroupMemberLeft(Group* g, Player* p);
void OnWhisperReceived(Player* sender, Player* receiver, std::string const& msg);
// Squad-chat hook for owner squad control: party-chat messages from
// the bot's owner are routed through the address resolver so commands
// like `;tank pull` apply to addressed bots without forcing per-bot
// whispers. Non-prefix messages no-op.
void OnPartyChat(Player* sender, Group* group, std::string const& msg);

// Guild-chat hook (Phase C.3). Fired from HandleMessagechatOpcode for
// every CHAT_MSG_GUILD / CHAT_MSG_OFFICER line. V2 routes this through
// BotChatReactor::ReactGuild so one online officer of the sender's
// guild emits a contextual reply (gz / nice / :) ).
//
// Sender may be a bot — the reactor itself filters bot-originated
// chat to avoid echo storms.
void OnGuildChat(Player* sender, uint64 guild_id, std::string const& msg);

// SC-P1a: Say / Yell social hooks. Fired from HandleChatMessage right
// after the player's Say(...) / Yell(...) packet is broadcast. V2 routes
// these through BotChatReactor::ReactSay / ReactYell, which run a
// grid-bounded range query (CONFIG_LISTEN_RANGE_SAY / _YELL) and let at
// most one nearby bot answer (per-bot + per-area cooldown). Bots
// themselves are filtered by the reactor to avoid echo storms.
void OnSayChat(Player* sender, std::string const& msg);
void OnYellChat(Player* sender, std::string const& msg);

// SC-P2c: text-emote (/wave, /salute, /cheer) hook. Fired from
// HandleTextEmoteOpcode after the STextEmote packet is broadcast in
// CONFIG_LISTEN_RANGE_TEXTEMOTE. V2 routes to BotChatReactor::ReactEmote
// so a nearby targeted (or, if untargeted, one nearby) bot reciprocates
// with a human-paced PerformEmoteIntent. emote_id is the EmotesText.db2
// EmoteID resolved core-side; target is the emote's target guid (may be
// empty for untargeted emotes).
void OnTextEmote(Player* sender, uint32 emote_id, ObjectGuid target);

// SC-P2b: guild member-added hook. Fired from Guild::AddMember after the
// member is committed. V2 routes to BotChatReactor::ReactGuildJoin so one
// online bot guildmate emits a welcome line a few seconds later
// (per-guild throttle). joiner_name is the new member's character name
// (resolved core-side; the joining Player may be offline at add time).
void OnGuildMemberAdded(uint64 guild_id, ObjectGuid joiner_guid, std::string const& joiner_name);

// Queue auto-fill hooks (Phase D of WORLD_POPULATION_PLAN). Fired by core
// when a Player joins a BG / LFG / LFR queue, so V2's BotQueueFiller can
// invite online bots and JIT-spawn the rest. Bots themselves no-op these.
void OnPlayerJoinedBgQueue(Player* player, uint32 bg_type_id, uint8 bracket);
void OnPlayerJoinedLfg(Player* player, uint32 dungeon_id, uint8 role_mask);

// Fired synchronously from BattlegroundQueue::InviteGroupToBG (and its
// reminder event) the moment TC writes the BattlefieldStatusNeedConfirmation
// for a queued player. For V2 bots this is the only reliable signal that
// the 90s INVITE_ACCEPT_WAIT_TIME window has started — the snapshot-poll
// path can take 50ms-1s + a 5s per-bot dedup cooldown to react, which
// caused "always the same 5 bots port" when 10 needed to. The hook handler
// emits an immediate BgPortIntent (and stamps the dedup cooldown) so all
// invited bots port within the same world tick as the invite.
//
// IMPORTANT: handler MUST NOT mutate BG queue data structures synchronously
// (the queue is mid-iteration when this fires). Pushing into the per-bot
// IntentQueue is safe — drain runs after BattlegroundQueueUpdate.
void OnBGInvitationReceived(Player* player, uint32 bg_instance_id, uint32 bg_type_id);

// Fired from LFGMgr::AddProposal / UpdateProposal the instant TC sends an
// LFG group-formation proposal to a candidate. For V2 bots we auto-accept
// with a tiny per-bot stagger so all 5 (or 25) bots respond well inside the
// LFG_TIME_PROPOSAL window. Currently the snapshot-poll idle:lfg_proposal_
// accept rule races the timer at scale; this hook collapses the response
// latency from ~50ms-1s to one world tick.
void OnLfgProposalReceived(Player* player, uint32 proposal_id);

// Diagnostic: fired from HandleBattleFieldPortOpcode whenever a player's
// Enter-Battle click hits a silent-return path (not-in-queue, no
// group-info, not-invited, bg-instance-gone, freeze-debuff, etc.).
// Reason codes match the source-line gates 1:1 so future "click Enter
// no port" reports auto-record which gate fired. Inert no-op for bots.
//
// Reason codes:
//   1 = player not in queue            (HandleBattleFieldPortOpcode line 290)
//   2 = invalid queue slot             (line 298)
//   3 = no GroupQueueInfo              (line 309)
//   4 = invited flag not set on ginfo  (line 316)
//   5 = BG instance no longer exists   (line 336)
//   6 = no bracket entry               (line 350)
//   7 = freeze debuff                  (line 378)
//   8 = !IsInvitedForBattlegroundQueueType (line 381 — the "cheating?" gate
//                                            that usually fires when bots
//                                            already filled the BG and the
//                                            queue cleared the player's
//                                            invite flag)
void OnBGPortFailed(Player* player, uint8 reason_code, uint32 bg_instance_id);

// Path-outcome telemetry from PlayerbotAPI::move_to. Outcome values match
// PerfCounters::PathOutcome (Ok=0, NoPath=1, FarFromPolyStart=2,
// FarFromPolyEnd=3, Incomplete=4, Short=5). Routed through the hook layer
// to keep PlayerbotAPI.cpp (core game module) free of V2 includes.
void OnPathOutcome(uint8 outcome);

// (Additional hooks added here as their insertion sites land in core code per
//  MODULE_LAYOUT.md §4 — full list of ≤30 enumerated there.)

#else // !TRINITY_PLAYERBOT_V2

// Inline-empty fallbacks — zero cost when V2 is not built.
inline void OnPlayerLogin(Player*) {}
inline void OnPlayerLogout(Player*) {}
inline void OnLevelUp(Player*, uint8) {}
inline void OnDeath(Unit*, Unit*) {}
inline void OnResurrect(Player*) {}
inline void OnSpecChanged(Player*, uint8) {}
inline void OnDamageDealt(Unit*, Unit*, int32, uint32) {}
inline void OnDamageTaken(Unit*, Unit*, int32, uint32) {}
inline void OnHealReceived(Unit*, Unit*, int32, uint32) {}
inline void OnAuraApplied(Unit*, Aura*) {}
inline void OnAuraRemoved(Unit*, Aura*) {}
inline void OnGroupMemberJoined(Group*, Player*) {}
inline void OnGroupMemberLeft(Group*, Player*) {}
inline void OnWhisperReceived(Player*, Player*, std::string const&) {}
inline void OnPartyChat(Player*, Group*, std::string const&) {}
inline void OnGuildChat(Player*, uint64, std::string const&) {}
inline void OnSayChat(Player*, std::string const&) {}
inline void OnYellChat(Player*, std::string const&) {}
inline void OnTextEmote(Player*, uint32, ObjectGuid) {}
inline void OnGuildMemberAdded(uint64, ObjectGuid, std::string const&) {}
inline void OnPlayerJoinedBgQueue(Player*, uint32, uint8) {}
inline void OnPlayerJoinedLfg(Player*, uint32, uint8) {}
inline void OnBGInvitationReceived(Player*, uint32, uint32) {}
inline void OnLfgProposalReceived(Player*, uint32) {}
inline void OnBGPortFailed(Player*, uint8, uint32) {}
inline void OnPathOutcome(uint8) {}

#endif // TRINITY_PLAYERBOT_V2

} // namespace Playerbot::Hooks

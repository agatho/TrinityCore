// BotChatReactor - lightweight party/raid chat reaction layer.
//
// When a real player speaks in party/raid/instance chat, scan for common
// social keywords ("ty"/"thanks", "gz"/"grats", "lol", direct name address)
// and emit a contextual reply from at most one bot in the channel. This
// makes group chat read like a room of real players instead of silent
// auto-attackers.
//
// Scope notes:
//   - Pure reactive layer. Behavioral commands ("pull", "wait", "watch FC")
//     still flow through BotCommandParser via the ;-prefix path.
//   - Sender must be a real player; bot-originated chat is never reacted to
//     (avoids self-talk echo storms).
//   - At most one bot replies per message — picked stably by guid hash so
//     the same bot tends to be the "talker" of a group.
//   - Per-bot throttle prevents reply spam under heavy chat.
//   - Verbosity gates: Silent/Terse skip entirely; Normal+/Chatty/Roleplay
//     have progressively higher fire chance.

#pragma once

#include "ObjectGuid.h"

#include <cstdint>
#include <string>

class Player;
class Group;

namespace Playerbot {

class BotChatReactor
{
public:
    // Entry point. Called from PlayerbotV2::Module::OnPartyChat for every
    // party/raid/instance-chat line (after the ;-prefix command path).
    // No-op when sender is null / a bot / message is empty.
    static void React(Player* sender, Group* group, std::string const& msg);

    // Whisper variant. Called from OnWhisperReceived AFTER BotCommandParser
    // returned false (command not recognized) — treats the line as a
    // social cue and emits a whisper-back if the message matches a known
    // pattern (hi/ty/gz/etc.). Per-bot throttle shared with party react
    // path. No-op when sender is a bot or the bot isn't owned/grouped.
    static void ReactWhisper(Player* sender, Player* bot, std::string const& msg);

    // Phase C.3 guild-chat variant. Called from Module::OnGuildChat for
    // every CHAT_MSG_GUILD/CHAT_MSG_OFFICER line. Picks ONE online
    // officer of the sender's guild (oldest-by-guid hash, deterministic
    // so the same officer tends to be "the talker"), and emits a
    // contextual guild_chat reply 30-90s later via the bot's intent
    // queue. No-op when:
    //   - Sender is a bot (avoids echo storms).
    //   - Guild has no online officer bot.
    //   - Reactor's per-guild throttle (60s) is hot.
    static void ReactGuild(Player* sender, uint64 guild_id, std::string const& msg);

    // SC-P1a /say reactor. Called from Module::OnSayChat for every /say a
    // real player makes. Runs a grid-bounded range query anchored on the
    // sender (radius = CONFIG_LISTEN_RANGE_SAY) for V2 bots, classifies the
    // message per-bot, and lets at most ONE bot answer — picked stably by
    // the same guid-hash selection used by the party path, except addressed
    // bots (name mentioned) always win. Say replies are biased toward
    // greetings / direct-name mentions / location questions. Enforces a
    // per-bot cooldown AND a per-area cooldown so crowded hubs don't explode
    // at scale. No-op when sender is a bot / message is a command prefix.
    static void ReactSay(Player* sender, std::string const& msg);

    // SC-P1a /yell reactor. Same machinery as ReactSay but anchored at
    // CONFIG_LISTEN_RANGE_YELL and gated to a LOWER fire chance — players
    // answer yells far less often than says. Shares the per-bot/per-area
    // cooldown maps with ReactSay.
    static void ReactYell(Player* sender, std::string const& msg);

    // SC-P2c text-emote reactor. Called from Module::OnTextEmote. A nearby
    // bot reciprocates: if the emote was targeted at a specific bot, THAT
    // bot answers; otherwise one nearby bot is picked. Emits a human-paced
    // PerformEmoteIntent with a reciprocal animation. Per-bot cooldown,
    // range-gated by CONFIG_LISTEN_RANGE_TEXTEMOTE.
    static void ReactEmote(Player* sender, uint32 emote_id, ObjectGuid target);

    // SC-P2b guild-join welcome. Called from Module::OnGuildMemberAdded.
    // Picks one online bot guildmate (deterministic by guid hash) to emit a
    // welcome line a few seconds later through GuildChatIntent. Per-guild
    // throttle prevents a recruiting spree from spamming chat. No-op when
    // the joiner is itself a bot (auto-spawned recruits don't get welcomed).
    static void ReactGuildJoin(uint64 guild_id, ObjectGuid joiner_guid, std::string const& joiner_name);
};

} // namespace Playerbot

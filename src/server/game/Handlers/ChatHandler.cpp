/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSession.h"
#include "AccountMgr.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "Chat.h"
#include "ChatCaution.h"
#include "ChatPackets.h"
#include "Common.h"
#include "CreatureAI.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "LanguageMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "Util.h"
#include "World.h"
#include <algorithm>

enum class ChatWhisperTargetStatus : uint8
{
    CanWhisper      = 0,
    CanWhisperGuild = 1,
    Offline         = 2,
    WrongFaction    = 3
};

inline bool isNasty(uint8 c)
{
    if (c == '\t')
        return false;
    if (c <= '\037') // ASCII control block
        return true;
    return false;
}

inline bool ValidateMessage(Player const* player, std::string& msg)
{
    // cut at the first newline or carriage return
    std::string::size_type pos = msg.find_first_of("\n\r");
    if (pos == 0)
        return false;
    else if (pos != std::string::npos)
        msg.erase(pos);

    // abort on any sort of nasty character
    for (uint8 c : msg)
    {
        if (isNasty(c))
        {
            TC_LOG_ERROR("network", "Player {} {} sent a message containing invalid character {} - blocked", player->GetName(),
                player->GetGUID().ToString(), uint32(c));
            return false;
        }
    }

    // collapse multiple spaces into one
    if (sWorld->getBoolConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
    {
        auto end = std::unique(msg.begin(), msg.end(), [](char c1, char c2) { return (c1 == ' ') && (c2 == ' '); });
        msg.erase(end, msg.end());
    }

    return true;
}

void WorldSession::HandleChatMessageOpcode(WorldPackets::Chat::ChatMessage& chatMessage)
{
    ChatMsg type;

    switch (chatMessage.GetOpcode())
    {
        case CMSG_CHAT_MESSAGE_SAY:
            type = CHAT_MSG_SAY;
            break;
        case CMSG_CHAT_MESSAGE_YELL:
            type = CHAT_MSG_YELL;
            break;
        case CMSG_CHAT_MESSAGE_GUILD:
            type = CHAT_MSG_GUILD;
            break;
        case CMSG_CHAT_MESSAGE_OFFICER:
            type = CHAT_MSG_OFFICER;
            break;
        case CMSG_CHAT_MESSAGE_PARTY:
            type = CHAT_MSG_PARTY;
            break;
        case CMSG_CHAT_MESSAGE_RAID:
            type = CHAT_MSG_RAID;
            break;
        case CMSG_CHAT_MESSAGE_RAID_WARNING:
            type = CHAT_MSG_RAID_WARNING;
            break;
        case CMSG_CHAT_MESSAGE_INSTANCE_CHAT:
            type = CHAT_MSG_INSTANCE_CHAT;
            break;
        default:
            TC_LOG_ERROR("network", "HandleMessagechatOpcode : Unknown chat opcode ({})", chatMessage.GetOpcode());
            return;
    }

    HandleChatMessage(type, Language(chatMessage.Language), chatMessage.Text);
}

void WorldSession::HandleChatMessageWhisperOpcode(WorldPackets::Chat::ChatMessageWhisper& chatMessageWhisper)
{
    HandleChatMessage(CHAT_MSG_WHISPER, Language(chatMessageWhisper.Language), chatMessageWhisper.Text, chatMessageWhisper.Target, chatMessageWhisper.TargetGUID);
}

void WorldSession::HandleChatMessageChannelOpcode(WorldPackets::Chat::ChatMessageChannel& chatMessageChannel)
{
    HandleChatMessage(CHAT_MSG_CHANNEL, Language(chatMessageChannel.Language), chatMessageChannel.Text, chatMessageChannel.Target, chatMessageChannel.ChannelGUID);
}

void WorldSession::HandleChatMessageEmoteOpcode(WorldPackets::Chat::ChatMessageEmote& chatMessageEmote)
{
    HandleChatMessage(CHAT_MSG_EMOTE, LANG_UNIVERSAL, chatMessageEmote.Text);
}

ChatMessageResult WorldSession::HandleChatMessage(ChatMsg type, Language lang, std::string msg, std::string target /*= ""*/, Optional<ObjectGuid> targetGuid /*= {}*/)
{
    Player* sender = GetPlayer();

    // The realm's chat service is in maintenance. The client has already been told with
    // SMSG_CHAT_DOWN / SMSG_CHAT_IS_DOWN and prints CHAT_SERVER_DISCONNECTED_MESSAGE itself, so
    // there is nothing to answer here.
    if (!sWorld->getBoolConfig(CONFIG_CHAT_SERVICE_ENABLED))
        return ChatMessageResult::ChatServiceDown;

    if (lang == LANG_UNIVERSAL && type != CHAT_MSG_EMOTE && type != CHAT_MSG_GUILD && type != CHAT_MSG_OFFICER)
    {
        TC_LOG_ERROR("entities.player.cheat", "CMSG_MESSAGECHAT: Possible hacking-attempt: {} tried to send a message in universal language", GetPlayerInfo());
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return ChatMessageResult::DisallowedLanguage;
    }

    // prevent talking at unknown language (cheating)
    auto languageData = sLanguageMgr->GetLanguageDescById(lang);
    if (languageData.begin() == languageData.end())
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return ChatMessageResult::InvalidLanguage;
    }

    if (std::none_of(languageData.begin(), languageData.end(),
        [sender](std::pair<uint32 const, LanguageDesc> const& langDesc) { return langDesc.second.SkillId == 0 || sender->HasSkill(langDesc.second.SkillId); }))
    {
        // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
        if (!sender->HasAuraTypeWithMiscvalue(SPELL_AURA_COMPREHEND_LANGUAGE, lang))
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return ChatMessageResult::LanguageNotLearned;
        }
    }

    // send in universal language if player in .gm on mode (ignore spell effects)
    if (sender->IsGameMaster())
        lang = LANG_UNIVERSAL;
    else
    {
        // send in universal language in two side iteration allowed mode
        if (HasPermission(rbac::RBAC_PERM_TWO_SIDE_INTERACTION_CHAT))
            lang = LANG_UNIVERSAL;
        else
        {
            switch (type)
            {
                case CHAT_MSG_PARTY:
                case CHAT_MSG_RAID:
                case CHAT_MSG_RAID_WARNING:
                    // allow two side chat at group channel if two side group allowed
                    if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                        lang = LANG_UNIVERSAL;
                    break;
                case CHAT_MSG_GUILD:
                case CHAT_MSG_OFFICER:
                    // allow two side chat at guild channel if two side guild allowed
                    if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                        lang = LANG_UNIVERSAL;
                    break;
                default:
                    break;
            }
        }

        // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
        Unit::AuraEffectList const& ModLangAuras = sender->GetAuraEffectsByType(SPELL_AURA_MOD_LANGUAGE);
        if (!ModLangAuras.empty())
            lang = Language(ModLangAuras.front()->GetMiscValue());
    }

    // SMSG_CHAT_IGNORED_ACCOUNT_MUTED (0x4A0000) is the parental controls case and ONLY that:
    // its consumer hardwires GameError 946 = ERR_PARENTAL_CONTROLS_CHAT_MUTED. Checked before the
    // mutetime so the more specific reason wins.
    if (HasPermission(rbac::RBAC_PERM_CHAT_MUTED_PARENTAL_CONTROLS))
    {
        SendChatIgnoredAccountMuted();
        return ChatMessageResult::MutedByParentalControls;
    }

    if (!CanSpeak())
    {
        // Bestand B1: this used to go out as SMSG_PRINT_NOTIFICATION, which is the wrong UI surface.
        // The client covers an account mute with SMSG_CHAT_RESTRICTED reason 2 (ERR_USER_SQUELCHED);
        // the five values of ChatRestrictionType match the client's five branches position for
        // position (consumer 0x1E1C650 -> GameError 612/613/654/798/799).
        // The remaining mute time is not part of that message - retail does not send it either.
        SendChatRestricted(ERR_USER_SQUELCHED);
        return ChatMessageResult::Muted;
    }

    if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
    {
        if (!sender->UpdateSpeakTime(Player::ChatFloodThrottle::REGULAR))
        {
            // Bestand B1: SMSG_CHAT_RESTRICTED reason 1 (ERR_CHAT_THROTTLED). Previously the
            // flood filter only armed the mute for the NEXT message and said nothing about this one.
            SendChatRestricted(ERR_CHAT_THROTTLED);
            return ChatMessageResult::Muted;
        }
    }

    if (sender->HasAura(GM_SILENCE_AURA) && type != CHAT_MSG_WHISPER)
    {
        SendNotification(GetTrinityString(LANG_GM_SILENCE), sender->GetName().c_str());
        return ChatMessageResult::SilencedByGM;
    }

    if (msg.size() > 511)
        return ChatMessageResult::MessageTooLong;

    if (msg.empty())
        return ChatMessageResult::MessageEmpty;

    if (ChatHandler(this).ParseCommands(msg.c_str()))
        return ChatMessageResult::HandledCommand;

    // do message validity checks
    if (!ValidateMessage(GetPlayer(), msg))
        return ChatMessageResult::MessageHasInvalidCharacters;

    // validate hyperlinks
    if (!ValidateHyperlinksAndMaybeKick(msg))
        return ChatMessageResult::MalformedHyperlinks;

    // The cautionary mechanism (SMSG_CAUTIONARY_CHAT_MESSAGE / _CHANNEL_MESSAGE). The server - not
    // the client - decides that a message is questionable; the client only ever hands a
    // ConfirmNumber back. Reuses the same `chat_spam_record` patterns that are shipped to the client
    // with SMSG_EXPECTED_SPAM_RECORDS, so both directions of the mechanism are fed from one table.
    // Off by default: holding player messages is intrusive and is the realm operator's decision.
    if (!_chatCautionAccepted && sWorld->getBoolConfig(CONFIG_CHAT_CAUTIONARY_ENABLED)
        && (type == CHAT_MSG_WHISPER || type == CHAT_MSG_CHANNEL) && sObjectMgr->MatchesChatSpamPattern(msg))
    {
        if (type == CHAT_MSG_WHISPER)
        {
            if (SendCautionaryChatMessage(type, lang, msg, target, targetGuid.value_or(ObjectGuid::Empty)))
                return ChatMessageResult::CautionaryChatPending;
            // Too many pending messages already - fall through and deliver rather than lose it.
        }
        else
        {
            // Channel variant: informative only, the client auto-confirms (K4), so the message is
            // delivered in the same breath.
            SendCautionaryChannelMessage(msg);
        }
    }

    switch (type)
    {
        case CHAT_MSG_SAY:
        {
            // Prevent cheating
            if (!sender->IsAlive())
                return ChatMessageResult::PlayerDead;

            if (sender->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_SAY_LEVEL_REQ))
            {
                SendNotification(GetTrinityString(LANG_SAY_REQ), sWorld->getIntConfig(CONFIG_CHAT_SAY_LEVEL_REQ));
                return ChatMessageResult::LevelTooLow;
            }

            sender->Say(msg, lang);
            break;
        }
        case CHAT_MSG_EMOTE:
        {
            // Prevent cheating
            if (!sender->IsAlive())
                return ChatMessageResult::PlayerDead;

            if (sender->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_EMOTE_LEVEL_REQ))
            {
                SendNotification(GetTrinityString(LANG_SAY_REQ), sWorld->getIntConfig(CONFIG_CHAT_EMOTE_LEVEL_REQ));
                return ChatMessageResult::LevelTooLow;
            }

            sender->TextEmote(msg);
            break;
        }
        case CHAT_MSG_YELL:
        {
            // Prevent cheating
            if (!sender->IsAlive())
                return ChatMessageResult::PlayerDead;

            if (sender->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_YELL_LEVEL_REQ))
            {
                SendNotification(GetTrinityString(LANG_SAY_REQ), sWorld->getIntConfig(CONFIG_CHAT_YELL_LEVEL_REQ));
                return ChatMessageResult::LevelTooLow;
            }

            sender->Yell(msg, lang);
            break;
        }
        case CHAT_MSG_WHISPER:
        {
            /// @todo implement cross realm whispers (someday)
            Player* receiver = nullptr;
            if (targetGuid && !targetGuid->IsEmpty())
            {
                receiver = ObjectAccessor::FindConnectedPlayer(*targetGuid);
            }
            else
            {
                ExtendedPlayerName extName = ExtractExtendedPlayerName(target);

                if (!normalizePlayerName(extName.Name))
                {
                    SendChatPlayerNotfoundNotice(target);
                    break;
                }

                receiver = ObjectAccessor::FindConnectedPlayerByName(extName.Name);
            }
            if (!receiver || (lang != LANG_ADDON && !receiver->isAcceptWhispers() && receiver->GetSession()->HasPermission(rbac::RBAC_PERM_CAN_FILTER_WHISPERS) && !receiver->IsInWhisperWhiteList(sender->GetGUID())))
            {
                SendChatPlayerNotfoundNotice(target);
                return ChatMessageResult::NoWhisperTarget;
            }

            // Apply checks only if receiver is not already in whitelist and if receiver is not a GM with ".whisper on"
            if (!receiver->IsInWhisperWhiteList(sender->GetGUID()) && !receiver->IsGameMasterAcceptingWhispers())
            {
                if (!sender->IsGameMaster() && sender->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_WHISPER_LEVEL_REQ))
                {
                    SendNotification(GetTrinityString(LANG_WHISPER_REQ), sWorld->getIntConfig(CONFIG_CHAT_WHISPER_LEVEL_REQ));
                    return ChatMessageResult::LevelTooLow;
                }

                if (GetPlayer()->GetEffectiveTeam() != receiver->GetEffectiveTeam() && !HasPermission(rbac::RBAC_PERM_TWO_SIDE_INTERACTION_CHAT))
                {
                    SendChatPlayerNotfoundNotice(target);
                    return ChatMessageResult::WhisperTargetWrongFaction;
                }
            }

            if (GetPlayer()->HasAura(1852) && !receiver->IsGameMaster())
            {
                SendNotification(GetTrinityString(LANG_GM_SILENCE), GetPlayer()->GetName().c_str());
                return ChatMessageResult::SilencedByGM;
            }

            // If player is a Gamemaster and doesn't accept whisper, we auto-whitelist every player that the Gamemaster is talking to
            // We also do that if a player is under the required level for whispers.
            if (receiver->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_WHISPER_LEVEL_REQ) ||
                (HasPermission(rbac::RBAC_PERM_CAN_FILTER_WHISPERS) && !sender->isAcceptWhispers() && !sender->IsInWhisperWhiteList(receiver->GetGUID())))
                sender->AddWhisperWhiteList(receiver->GetGUID());

            GetPlayer()->Whisper(msg, lang, receiver);
            break;
        }
        case CHAT_MSG_PARTY:
        {
            // if player is in battleground, he cannot say to battleground members by /p
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = sender->GetGroup();
                if (!group || group->isBGGroup())
                {
                    SendChatNotInParty(type);   // CHAT_MSG_PARTY -> GameError 105 ERR_NOT_IN_GROUP
                    return ChatMessageResult::NotInGroup;
                }
            }

            type = group->IsLeader(sender->GetGUID()) ? CHAT_MSG_PARTY_LEADER : CHAT_MSG_PARTY;

            sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

            WorldPackets::Chat::Chat packet;
            packet.Initialize(ChatMsg(type), lang, sender, nullptr, msg);
            group->BroadcastPacket(packet.Write(), false, group->GetMemberGroup(GetPlayer()->GetGUID()));
            break;
        }
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        {
            bool officerOnly = type == CHAT_MSG_OFFICER;

            Guild* guild = GetPlayer()->GetGuildId() ? sGuildMgr->GetGuildById(GetPlayer()->GetGuildId()) : nullptr;
            if (!guild)
            {
                // SMSG_CHAT_NOT_IN_GUILD with GuildCommandError 9 -> GameError 143
                // ERR_GUILD_NOT_IN_A_GUILD. Both branches used to fail silently.
                SendChatNotInGuild(ERR_GUILD_PLAYER_NOT_IN_GUILD);
                return ChatMessageResult::NotInGuild;
            }

            if (!guild->HasChatRankRight(GetPlayer(), officerOnly))
            {
                // SMSG_CHAT_NOT_IN_GUILD with GuildCommandError 8 -> GameError 122
                // ERR_GUILD_PERMISSIONS
                SendChatNotInGuild(ERR_GUILD_PERMISSIONS);
                return ChatMessageResult::NoGuildChatPermission;
            }

            sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, guild);

            guild->BroadcastToGuild(this, officerOnly, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
            break;
        }
        case CHAT_MSG_RAID:
        {
            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isRaidGroup() || group->isBGGroup())
            {
                SendChatNotInParty(type);   // CHAT_MSG_RAID -> GameError 552 ERR_NOT_IN_RAID
                return ChatMessageResult::NotInGroup;
            }

            if (group->IsLeader(GetPlayer()->GetGUID()))
                type = CHAT_MSG_RAID_LEADER;

            sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

            WorldPackets::Chat::Chat packet;
            packet.Initialize(ChatMsg(type), lang, sender, nullptr, msg);
            group->BroadcastPacket(packet.Write(), false);
            break;
        }
        case CHAT_MSG_RAID_WARNING:
        {
            Group* group = GetPlayer()->GetGroup();
            if (!group)
            {
                SendChatNotInParty(type);   // CHAT_MSG_RAID_WARNING -> GameError 552 ERR_NOT_IN_RAID
                return ChatMessageResult::NotInGroup;
            }

            if (group->isRaidGroup())
            {
                if (!group->IsLeader(GetPlayer()->GetGUID()) && !group->IsAssistant(GetPlayer()->GetGUID()))
                    return ChatMessageResult::NotLeaderOrAssistant;
            }
            else if (!sWorld->getBoolConfig(CONFIG_CHAT_PARTY_RAID_WARNINGS))
                return ChatMessageResult::RaidWarningInPartyDisabled;

            sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

            WorldPackets::Chat::Chat packet;
            //in battleground, raid warning is sent only to players in battleground - code is ok
            packet.Initialize(CHAT_MSG_RAID_WARNING, lang, sender, nullptr, msg);
            group->BroadcastPacket(packet.Write(), false);
            break;
        }
        case CHAT_MSG_CHANNEL:
        {
            if (!HasPermission(rbac::RBAC_PERM_SKIP_CHECK_CHAT_CHANNEL_REQ))
            {
                if (sender->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_CHANNEL_LEVEL_REQ))
                {
                    SendNotification(GetTrinityString(LANG_CHANNEL_REQ), sWorld->getIntConfig(CONFIG_CHAT_CHANNEL_LEVEL_REQ));
                    return ChatMessageResult::LevelTooLow;
                }
            }

            Channel* chn = targetGuid
                ? ChannelMgr::GetChannelForPlayerByGuid(*targetGuid, sender)
                : ChannelMgr::GetChannelForPlayerByNamePart(target, sender);
            if (chn)
            {
                if (ChatChannelsEntry const* chatChannel = sChatChannelsStore.LookupEntry(chn->GetChannelId()))
                    if (chatChannel->GetFlags().HasFlag(ChatChannelFlags::ReadOnly))
                        return ChatMessageResult::ChannelIsReadOnly;

                sScriptMgr->OnPlayerChat(sender, type, lang, msg, chn);
                chn->Say(sender->GetGUID(), msg, lang);
            }
            break;
        }
        case CHAT_MSG_INSTANCE_CHAT:
        {
            Group* group = GetPlayer()->GetGroup();
            if (!group)
            {
                // Filtered out inside SendChatNotInParty: the client's consumer has no branch for
                // CHAT_MSG_INSTANCE_CHAT (62), so nothing is sent. Kept as a call so the reason is
                // in one place instead of duplicated here.
                SendChatNotInParty(type);
                return ChatMessageResult::NotInGroup;
            }

            if (group->IsLeader(GetPlayer()->GetGUID()))
                type = CHAT_MSG_INSTANCE_CHAT_LEADER;

            sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

            WorldPackets::Chat::Chat packet;
            packet.Initialize(ChatMsg(type), lang, sender, nullptr, msg);
            group->BroadcastPacket(packet.Write(), false);
            break;
        }
        default:
            TC_LOG_ERROR("network", "CHAT: unknown message type {}, lang: {}", type, lang);
            break;
    }

    return ChatMessageResult::Ok;
}

void WorldSession::HandleChatAddonMessageOpcode(WorldPackets::Chat::ChatAddonMessage& chatAddonMessage)
{
    HandleChatAddonMessage(chatAddonMessage.Params.Type, chatAddonMessage.Params.Prefix, chatAddonMessage.Params.Text, chatAddonMessage.Params.IsLogged);
}

void WorldSession::HandleChatAddonMessageTargetedOpcode(WorldPackets::Chat::ChatAddonMessageTargeted& chatAddonMessageTargeted)
{
    switch (chatAddonMessageTargeted.Params.Type)
    {
        case CHAT_MSG_WHISPER:
            HandleChatAddonMessage(chatAddonMessageTargeted.Params.Type, chatAddonMessageTargeted.Params.Prefix, chatAddonMessageTargeted.Params.Text,
                chatAddonMessageTargeted.Params.IsLogged, chatAddonMessageTargeted.PlayerName, chatAddonMessageTargeted.PlayerGUID);
            break;
        case CHAT_MSG_CHANNEL:
            HandleChatAddonMessage(chatAddonMessageTargeted.Params.Type, chatAddonMessageTargeted.Params.Prefix, chatAddonMessageTargeted.Params.Text,
                chatAddonMessageTargeted.Params.IsLogged, chatAddonMessageTargeted.ChannelName, chatAddonMessageTargeted.ChannelGUID);
            break;
        default:
            TC_LOG_ERROR("misc", "HandleChatAddonMessageTargetedOpcode: unknown addon message type {}", chatAddonMessageTargeted.Params.Type);
            break;
    }
}

void WorldSession::HandleChatAddonMessage(ChatMsg type, std::string prefix, std::string text, bool isLogged, std::string target /*= ""*/, Optional<ObjectGuid> targetGuid /*= {}*/)
{
    Player* sender = GetPlayer();

    if (prefix.empty() || prefix.length() > 16)
        return;

    // Disabled addon channel?
    if (!sWorld->getBoolConfig(CONFIG_ADDON_CHANNEL))
        return;

    if (!CanSpeak())
        return;

    // No SMSG_CHAT_RESTRICTED here on purpose: an addon message has no chat surface, and retail
    // answers addon throttling client side with Enum.SendAddonMessageResult.AddonMessageThrottle.
    if (!sender->UpdateSpeakTime(Player::ChatFloodThrottle::ADDON))
        return;

    if (prefix == AddonChannelCommandHandler::PREFIX && AddonChannelCommandHandler(this).ParseCommands(text.c_str()))
        return;

    if (text.length() > 255)
        return;

    switch (type)
    {
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        {
            if (sender->GetGuildId())
                if (Guild* guild = sGuildMgr->GetGuildById(sender->GetGuildId()))
                    guild->BroadcastAddonToGuild(this, type == CHAT_MSG_OFFICER, text, prefix, isLogged);
            break;
        }
        case CHAT_MSG_WHISPER:
        {
            /// @todo implement cross realm whispers (someday)
            Player* receiver = nullptr;
            if (targetGuid && !targetGuid->IsEmpty())
            {
                receiver = ObjectAccessor::FindConnectedPlayer(*targetGuid);
            }
            else
            {
                ExtendedPlayerName extName = ExtractExtendedPlayerName(target);

                if (!normalizePlayerName(extName.Name))
                    break;

                receiver = ObjectAccessor::FindConnectedPlayerByName(extName.Name);
            }
            if (!receiver)
                break;

            sender->WhisperAddon(text, prefix, isLogged, receiver);
            break;
        }
        // Messages sent to "RAID" while in a party will get delivered to "PARTY"
        case CHAT_MSG_PARTY:
        case CHAT_MSG_RAID:
        case CHAT_MSG_INSTANCE_CHAT:
        {
            Group* group = nullptr;
            int32 subGroup = -1;
            if (type != CHAT_MSG_INSTANCE_CHAT)
                group = sender->GetOriginalGroup();

            if (!group)
            {
                group = sender->GetGroup();
                if (!group)
                {
                    // Deliberately no SMSG_CHAT_NOT_IN_PARTY: that opcode renders a visible chat
                    // line, and an addon message is invisible traffic. Retail reports this case
                    // client side, as the synchronous return value
                    // Enum.SendAddonMessageResult.NotInGroup of C_ChatInfo.SendAddonMessage
                    // (ChatConstantsDocumentation.lua:144).
                    break;
                }

                if (type == CHAT_MSG_PARTY)
                    subGroup = sender->GetSubGroup();
            }

            WorldPackets::Chat::Chat packet;
            packet.Initialize(type, isLogged ? LANG_ADDON_LOGGED : LANG_ADDON, sender, nullptr, text, 0, "", DEFAULT_LOCALE, prefix);
            group->BroadcastAddonMessagePacket(packet.Write(), prefix, true, subGroup, sender->GetGUID());
            break;
        }
        case CHAT_MSG_CHANNEL:
        {
            Channel* chn = targetGuid
                ? ChannelMgr::GetChannelForPlayerByGuid(*targetGuid, sender)
                : ChannelMgr::GetChannelForPlayerByNamePart(target, sender);
            if (chn)
                chn->AddonSay(sender->GetGUID(), prefix, text.c_str(), isLogged);
            break;
        }
        default:
        {
            TC_LOG_ERROR("misc", "HandleAddonMessagechatOpcode: unknown addon message type {}", type);
            break;
        }
    }
}

void WorldSession::HandleChatMessageAFKOpcode(WorldPackets::Chat::ChatMessageAFK& chatMessageAFK)
{
    Player* sender = GetPlayer();

    if (sender->IsInCombat())
        return;

    if (chatMessageAFK.Text.length() > 511)
        return;

    // do message validity checks
    if (!ValidateMessage(sender, chatMessageAFK.Text))
        return;

    if (!ValidateHyperlinksAndMaybeKick(chatMessageAFK.Text))
        return;

    if (sender->HasAura(GM_SILENCE_AURA))
    {
        SendNotification(GetTrinityString(LANG_GM_SILENCE), sender->GetName().c_str());
        return;
    }

    if (sender->isAFK()) // Already AFK
    {
        if (chatMessageAFK.Text.empty())
            sender->ToggleAFK(); // Remove AFK
        else
            sender->autoReplyMsg = chatMessageAFK.Text; // Update message
    }
    else // New AFK mode
    {
        sender->autoReplyMsg = chatMessageAFK.Text.empty() ? GetTrinityString(LANG_PLAYER_AFK_DEFAULT) : chatMessageAFK.Text;

        if (sender->isDND())
            sender->ToggleDND();

        sender->ToggleAFK();
    }

    if (Guild* guild = sender->GetGuild())
        guild->SendEventAwayChanged(sender->GetGUID(), sender->isAFK(), sender->isDND());

    sScriptMgr->OnPlayerChat(sender, CHAT_MSG_AFK, LANG_UNIVERSAL, chatMessageAFK.Text);
}

void WorldSession::HandleChatMessageDNDOpcode(WorldPackets::Chat::ChatMessageDND& chatMessageDND)
{
    Player* sender = GetPlayer();

    if (sender->IsInCombat())
        return;

    if (chatMessageDND.Text.length() > 511)
        return;

    // do message validity checks
    if (!ValidateMessage(sender, chatMessageDND.Text))
        return;

    if (!ValidateHyperlinksAndMaybeKick(chatMessageDND.Text))
        return;

    if (sender->HasAura(GM_SILENCE_AURA))
    {
        SendNotification(GetTrinityString(LANG_GM_SILENCE), sender->GetName().c_str());
        return;
    }

    if (sender->isDND()) // Already DND
    {
        if (chatMessageDND.Text.empty())
            sender->ToggleDND(); // Remove DND
        else
            sender->autoReplyMsg = chatMessageDND.Text; // Update message
    }
    else // New DND mode
    {
        sender->autoReplyMsg = chatMessageDND.Text.empty() ? GetTrinityString(LANG_PLAYER_DND_DEFAULT) : chatMessageDND.Text;

        if (sender->isAFK())
            sender->ToggleAFK();

        sender->ToggleDND();
    }

    if (Guild* guild = sender->GetGuild())
        guild->SendEventAwayChanged(sender->GetGUID(), sender->isAFK(), sender->isDND());

    sScriptMgr->OnPlayerChat(sender, CHAT_MSG_DND, LANG_UNIVERSAL, chatMessageDND.Text);
}

void WorldSession::HandleEmoteOpcode(WorldPackets::Chat::EmoteClient& /* packet */)
{
    if (!GetPlayer()->IsAlive() || GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        return;

    sScriptMgr->OnPlayerClearEmote(GetPlayer());

    _player->SetEmoteState(EMOTE_ONESHOT_NONE);
}

void WorldSession::HandleTextEmoteOpcode(WorldPackets::Chat::CTextEmote& packet)
{
    if (!_player->IsAlive())
        return;

    if (!CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - GameTime::GetGameTime());
        SendNotification(GetTrinityString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    sScriptMgr->OnPlayerTextEmote(_player, packet.EmoteID, packet.SoundIndex, packet.Target);

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(packet.EmoteID);
    if (!em)
        return;

    Emote emote = static_cast<Emote>(em->EmoteID);

    switch (emote)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        case EMOTE_STATE_DANCE:
        case EMOTE_STATE_READ:
        case EMOTE_STATE_LEAN:
            _player->SetEmoteState(emote);
            break;
        default:
            // Only allow text-emotes for "dead" entities (feign death included)
            if (_player->HasUnitState(UNIT_STATE_DIED))
                break;
            _player->HandleEmoteCommand(emote, nullptr, { packet.SpellVisualKitIDs.data(), packet.SpellVisualKitIDs.data() + packet.SpellVisualKitIDs.size() }, packet.SequenceVariation);
            break;
    }

    WorldPackets::Chat::STextEmote textEmote;
    textEmote.SourceGUID = _player->GetGUID();
    textEmote.SourceAccountGUID = GetAccountGUID();
    textEmote.TargetGUID = packet.Target;
    textEmote.EmoteID = packet.EmoteID;
    textEmote.SoundIndex = packet.SoundIndex;
    _player->SendMessageToSetInRange(textEmote.Write(), sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE), true);

    Unit* unit = ObjectAccessor::GetUnit(*_player, packet.Target);

    _player->UpdateCriteria(CriteriaType::DoEmote, packet.EmoteID, 0, 0, unit);

    // Send scripted event call
    if (Creature* creature = Object::ToCreature(unit))
        creature->AI()->ReceiveEmote(_player, packet.EmoteID);

    if (emote != EMOTE_ONESHOT_NONE)
        _player->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::Anim);
}

void WorldSession::HandleChatIgnoredOpcode(WorldPackets::Chat::ChatReportIgnored& chatReportIgnored)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(chatReportIgnored.IgnoredGUID);
    if (!player || !player->GetSession())
        return;

    WorldPackets::Chat::Chat packet;
    packet.Initialize(CHAT_MSG_IGNORED, LANG_UNIVERSAL, _player, _player, GetPlayer()->GetName());
    player->SendDirectMessage(packet.Write());
}

void WorldSession::SendChatPlayerNotfoundNotice(std::string const& name)
{
    SendPacket(WorldPackets::Chat::ChatPlayerNotfound(name).Write());
}

// Bestand B2: this has no caller, and that is not fixed here because there is nothing honest to
// call it from. SMSG_CHAT_PLAYER_AMBIGUOUS (GameError 625, ERR_CHAT_PLAYER_AMBIGUOUS_S) is for a
// whisper target name that resolves to more than one character; character names are unique per
// realm and cross realm whispers are still a @todo in HandleChatMessage, so the situation cannot
// arise. All three name resolutions correctly use SendChatPlayerNotfoundNotice instead.
// The wire bug that this dead path was hiding (B4, the missing FlushBits) is fixed in
// ChatPackets.cpp.
void WorldSession::SendPlayerAmbiguousNotice(std::string const& name)
{
    SendPacket(WorldPackets::Chat::ChatPlayerAmbiguous(name).Write());
}

void WorldSession::SendChatRestricted(ChatRestrictionType restriction)
{
    WorldPackets::Chat::ChatRestricted packet;
    packet.Reason = restriction;
    SendPacket(packet.Write());
}

void WorldSession::HandleChatCanLocalWhisperTargetRequest(WorldPackets::Chat::CanLocalWhisperTargetRequest const& canLocalWhisperTargetRequest)
{
    ChatWhisperTargetStatus status = [&]
    {
        Player* sender = GetPlayer();
        Player* receiver = ObjectAccessor::FindConnectedPlayer(canLocalWhisperTargetRequest.WhisperTarget);
        if (!receiver || (!receiver->isAcceptWhispers() && receiver->GetSession()->HasPermission(rbac::RBAC_PERM_CAN_FILTER_WHISPERS) && !receiver->IsInWhisperWhiteList(sender->GetGUID())))
            return ChatWhisperTargetStatus::Offline;

        if (!receiver->IsInWhisperWhiteList(sender->GetGUID()) && !receiver->IsGameMasterAcceptingWhispers())
            if (GetPlayer()->GetEffectiveTeam() != receiver->GetEffectiveTeam() && !HasPermission(rbac::RBAC_PERM_TWO_SIDE_INTERACTION_CHAT))
                return ChatWhisperTargetStatus::WrongFaction;

        return ChatWhisperTargetStatus::CanWhisper;
    }();

    WorldPackets::Chat::CanLocalWhisperTargetResponse canLocalWhisperTargetResponse;
    canLocalWhisperTargetResponse.WhisperTarget = canLocalWhisperTargetRequest.WhisperTarget;
    canLocalWhisperTargetResponse.Status = status;
    SendPacket(canLocalWhisperTargetResponse.Write());
}

void WorldSession::HandleChatUpdateAADCStatus(WorldPackets::Chat::UpdateAADCStatus const& /*updateAADCStatus*/)
{
    // disabling chat not supported
    // send Sueccess and force chat disabled to false instead of sending that change failed
    // this makes client change the cvar back to false instead of just printing error message in console
    WorldPackets::Chat::UpdateAADCStatusResponse response;
    response.Success = true;
    response.ChatDisabled = false;
    SendPacket(response.Write());
}

// SMSG_CHAT_NOT_IN_PARTY (0x4A0006). Tells the client that the group channel it addressed does not
// exist for it right now.
//
// The value set is CLOSED. Consumer 0x1E1C6A0 maps 2/49 to GameError 105 (ERR_NOT_IN_GROUP) and
// 3/39/40 to GameError 552 (ERR_NOT_IN_RAID) and returns silently for everything else - no text, no
// error, no log. CHAT_MSG_INSTANCE_CHAT (0x3E = 62) is NOT in that set, which is why this function
// filters instead of forwarding whatever chat type came in: origin/feature/gap-closers, from which
// the packet class is taken over, calls it with the raw type at its INSTANCE_CHAT site and the
// client throws that packet away.
void WorldSession::SendChatNotInParty(ChatMsg type)
{
    switch (type)
    {
        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARNING:
            break;
        default:
            // UNVERIFIED: the retail client has no branch for any other chat type here, so retail
            // cannot be using this opcode for them. What it uses instead - if anything - is not
            // derivable offline; sending an out-of-set value would be silently discarded, which is
            // strictly worse than sending nothing.
            TC_LOG_DEBUG("network", "SendChatNotInParty: chat type {} has no branch in the client, not sending", uint32(type));
            return;
    }

    SendPacket(WorldPackets::Chat::ChatNotInParty(type).Write());
}

// SMSG_CHAT_NOT_IN_GUILD (0x4A0023). Consumer 0x1E1C6E0 knows exactly two values:
//   ERR_GUILD_PERMISSIONS (8)         -> GameError 122 ERR_GUILD_PERMISSIONS
//   ERR_GUILD_PLAYER_NOT_IN_GUILD (9) -> GameError 143 ERR_GUILD_NOT_IN_A_GUILD
// This is a GuildCommandError, NOT a ChatMsg - the structurally identical SMSG_CHAT_NOT_IN_PARTY
// invites exactly that mistake (AGENT_BRIEF_CHAT_2C_4A.md K6).
void WorldSession::SendChatNotInGuild(uint32 guildCommandError)
{
    switch (guildCommandError)
    {
        case ERR_GUILD_PERMISSIONS:
        case ERR_GUILD_PLAYER_NOT_IN_GUILD:
            break;
        default:
            TC_LOG_DEBUG("network", "SendChatNotInGuild: guild command error {} has no branch in the client, not sending", guildCommandError);
            return;
    }

    SendPacket(WorldPackets::Chat::ChatNotInGuild(guildCommandError).Write());
}

// SMSG_CHAT_IGNORED_ACCOUNT_MUTED (0x4A0000). 0 bytes, and the error id is hardwired in the client:
// consumer 0x1E2ED50 starts with MOV ECX, 0x3B2 (946) = ERR_PARENTAL_CONTROLS_CHAT_MUTED.
// Only for an age/parental restriction on the account. An ordinary mutetime or GM mute is
// SMSG_CHAT_RESTRICTED with ERR_USER_SQUELCHED - see SendChatRestricted below.
void WorldSession::SendChatIgnoredAccountMuted()
{
    SendPacket(WorldPackets::Chat::ChatIgnoredAccountMuted().Write());
}

// SMSG_CHAT_DOWN / SMSG_CHAT_IS_DOWN / SMSG_CHAT_RECONNECT (0x4A0014, 0x4A0015, 0x4A0016).
// All 0 bytes. Both DOWN opcodes share one consumer (0x20ABAB0) and one effect, so the client does
// not distinguish them; the names suggest _IS_DOWN is the state at login and _DOWN the event, which
// is how they are used here.
void WorldSession::SendChatServiceStatus(bool available, bool initial)
{
    if (available)
    {
        // The consumer walks the client's channel table and restores every channel's join state, so
        // a wave of CMSG_CHAT_JOIN_CHANNEL follows this packet.
        SendPacket(WorldPackets::Chat::ChatReconnect().Write());
    }
    else
        SendPacket(WorldPackets::Chat::ChatDown(initial ? SMSG_CHAT_IS_DOWN : SMSG_CHAT_DOWN).Write());
}

// SMSG_CHAT_REGIONAL_SERVICE_STATUS (0x4A001D). 1 byte, and Status == 0 means AVAILABLE.
// The client debounces against its cached value (0x4674E2C), so repeating the same value produces
// no CHAT_REGIONAL_STATUS_CHANGED event at all.
void WorldSession::SendChatRegionalServiceStatus(bool available)
{
    WorldPackets::Chat::ChatRegionalServiceStatus status;
    status.Status = available
        ? WorldPackets::Chat::ChatRegionalServiceStatus::STATUS_AVAILABLE
        : WorldPackets::Chat::ChatRegionalServiceStatus::STATUS_UNAVAILABLE;
    SendPacket(status.Write());
}

// SMSG_EXPECTED_SPAM_RECORDS (0x4A0005). The patterns the client should filter incoming chat
// against; a hit there is reported back with CMSG_CHAT_REPORT_FILTERED (0x2C0004), which closes the
// loop. Sent once at login - consumer 0x20B31A0 replaces the container contents, it does not merge.
void WorldSession::SendExpectedSpamRecords()
{
    std::vector<std::string> const& records = sObjectMgr->GetChatSpamRecords();
    if (records.empty())
        return;

    WorldPackets::Chat::ExpectedSpamRecords packet;
    packet.Records = &records;
    SendPacket(packet.Write());
}

// SMSG_CHAT_AUTO_RESPONDED (0x4A000E). The AFK/DND auto reply of a whisper target, rendered by the
// client as CHAT_MSG_AFK (23) or CHAT_MSG_DND (24) - consumer 0x1DDFC30 computes
// ChatMsg = 0x17 + (IsDND != 0).
void WorldSession::SendChatAutoResponded(bool isDND, std::string_view text)
{
    WorldPackets::Chat::ChatAutoResponded autoResponded;
    autoResponded.IsDND = isDND;
    // The client reads into a 1283 byte buffer with ReadBytes and appends its own terminator, so it
    // would overrun on anything longer - 11 bits would allow 2047.
    autoResponded.Text.assign(text.substr(0, WorldPackets::Chat::ChatAutoResponded::MaxTextLength));
    autoResponded.SenderVirtualRealmAddress = GetVirtualRealmAddress();  // UNVERIFIED: field is unread by the consumer
    SendPacket(autoResponded.Write());
}

// SMSG_CAUTIONARY_CHAT_MESSAGE (0x4A0008) - hold a whisper and ask the player to confirm or drop it.
// Returns false when nothing could be held (too many pending), in which case the caller should let
// the message through or drop it on its own terms.
bool WorldSession::SendCautionaryChatMessage(ChatMsg type, Language lang, std::string const& msg, std::string const& targetName, ObjectGuid targetGuid)
{
    ChatCautionMgr::PendingMessage pending;
    pending.Type = type;
    pending.Lang = lang;
    pending.Text = msg;
    pending.TargetName = targetName;
    pending.TargetGuid = targetGuid;

    uint32 confirmNumber = _chatCautionMgr->Hold(std::move(pending));
    if (!confirmNumber)
        return false;

    WorldPackets::Chat::CautionaryChatMessage cautionary;
    // The client finds the chat line to rewrite by comparing THIS string case insensitively against
    // the text of its own chat lines (consumer 0x2667D30 against field +246 of the chat line table
    // at 0x4674E30). It has to be the message text verbatim, otherwise nothing is found and the
    // player never sees the confirm/discard links.
    cautionary.Text.assign(std::string_view(msg).substr(0, WorldPackets::Chat::CautionaryChatMessage::MaxTextLength));
    cautionary.TargetName = targetName;             // UNVERIFIED: unread by the 69382 retail consumer
    cautionary.SenderGUID = GetPlayer()->GetGUID(); // UNVERIFIED: unread by the 69382 retail consumer
    cautionary.ConfirmNumber = confirmNumber;
    SendPacket(cautionary.Write());
    return true;
}

// SMSG_CAUTIONARY_CHANNEL_MESSAGE (0x4A0009) - the channel variant. Informative only: consumer
// 0x2667E30 answers CMSG_CHAT_SEND_CAUTIONARY_CHANNEL_MESSAGE unconditionally and there is no drop
// path, so nothing is held back here (AGENT_BRIEF_CHAT_2C_4A.md K4). A server that waited for a
// decision would just delay the message by one round trip.
void WorldSession::SendCautionaryChannelMessage(std::string const& msg)
{
    WorldPackets::Chat::CautionaryChannelMessage cautionary;
    cautionary.Text.assign(std::string_view(msg).substr(0, WorldPackets::Chat::CautionaryChannelMessage::MaxTextLength));
    // No pending entry exists for the channel variant, so any non zero token would do; keep 0 to
    // make it obvious in a sniff that nothing is being tracked.
    cautionary.ConfirmNumber = 0;
    SendPacket(cautionary.Write());
}

// CMSG_CHAT_SEND_CAUTIONARY_CHAT_MESSAGE (0x2C0009) - the player confirmed: deliver the held message.
void WorldSession::HandleChatSendCautionaryChatMessage(WorldPackets::Chat::CautionaryAction& packet)
{
    Optional<ChatCautionMgr::PendingMessage> pending = _chatCautionMgr->Take(packet.ConfirmNumber);
    if (!pending)
    {
        TC_LOG_DEBUG("network", "CMSG_CHAT_SEND_CAUTIONARY_CHAT_MESSAGE: {} confirmed unknown or expired confirmNumber {}",
            GetPlayerInfo(), packet.ConfirmNumber);
        return;
    }

    // Re-run the whole ladder: the group/guild/channel membership and the mute state may have
    // changed while the message was held. _chatCautionAccepted keeps the cautionary check from
    // holding the same message a second time.
    _chatCautionAccepted = true;
    HandleChatMessage(pending->Type, pending->Lang, pending->Text, pending->TargetName,
        pending->TargetGuid.IsEmpty() ? Optional<ObjectGuid>() : Optional<ObjectGuid>(pending->TargetGuid));
    _chatCautionAccepted = false;
}

// CMSG_CHAT_DROP_CAUTIONARY_CHAT_MESSAGE (0x2C000A) - the player discarded it.
// The client puts the original text back into its own input line (ItemRefHandlersShared.lua:247),
// so the server must not echo anything.
void WorldSession::HandleChatDropCautionaryChatMessage(WorldPackets::Chat::CautionaryAction& packet)
{
    if (!_chatCautionMgr->Drop(packet.ConfirmNumber))
        TC_LOG_DEBUG("network", "CMSG_CHAT_DROP_CAUTIONARY_CHAT_MESSAGE: {} dropped unknown or expired confirmNumber {}",
            GetPlayerInfo(), packet.ConfirmNumber);
}

// CMSG_CHAT_SEND_CAUTIONARY_CHANNEL_MESSAGE (0x2C000B) - arrives automatically and unconditionally
// (consumer 0x2667E30 sends it right after calling the display hook, without a branch and without a
// Lua binding). There is nothing held back for it, so there is nothing to release; the packet is
// read so the tail is consumed and logged for diagnostics.
void WorldSession::HandleChatSendCautionaryChannelMessage(WorldPackets::Chat::CautionaryAction& packet)
{
    TC_LOG_DEBUG("network", "CMSG_CHAT_SEND_CAUTIONARY_CHANNEL_MESSAGE: {} auto-acknowledged confirmNumber {}",
        GetPlayerInfo(), packet.ConfirmNumber);
}

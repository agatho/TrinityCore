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

#ifndef TRINITYCORE_CHAT_PACKETS_H
#define TRINITYCORE_CHAT_PACKETS_H

#include "Packet.h"
#include "Common.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "PacketUtilities.h"
#include "SharedDefines.h"
#include <string>
#include <vector>

class WorldObject;
enum class ChatWhisperTargetStatus : uint8;

namespace WorldPackets
{
    namespace Chat
    {
        // CMSG_CHAT_MESSAGE_GUILD
        // CMSG_CHAT_MESSAGE_OFFICER
        // CMSG_CHAT_MESSAGE_YELL
        // CMSG_CHAT_MESSAGE_SAY
        // CMSG_CHAT_MESSAGE_PARTY
        // CMSG_CHAT_MESSAGE_RAID
        // CMSG_CHAT_MESSAGE_RAID_WARNING
        // CMSG_CHAT_MESSAGE_INSTANCE_CHAT
        class ChatMessage final : public ClientPacket
        {
        public:
            explicit ChatMessage(WorldPacket&& packet) : ClientPacket(std::move(packet)) { }

            void Read() override;

            std::string Text;
            int32 Language = LANG_UNIVERSAL;
            bool IsSecure = true;
        };

        // CMSG_CHAT_MESSAGE_WHISPER
        // CMSG_MESSAGE_WHISPER
        class ChatMessageWhisper final : public ClientPacket
        {
        public:
            explicit ChatMessageWhisper(WorldPacket&& packet) : ClientPacket(std::move(packet)) { }

            void Read() override;

            int32 Language = LANG_UNIVERSAL;
            ObjectGuid TargetGUID;
            uint32 TargetVirtualRealmAddress = 0;
            std::string Target;
            std::string Text;
        };

        // CMSG_CHAT_MESSAGE_CHANNEL
        class ChatMessageChannel final : public ClientPacket
        {
        public:
            explicit ChatMessageChannel(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_MESSAGE_CHANNEL, std::move(packet)) { }

            void Read() override;

            int32 Language = LANG_UNIVERSAL;
            ObjectGuid ChannelGUID;
            std::string Text;
            std::string Target;
            Optional<bool> IsSecure;
        };

        struct ChatAddonMessageParams
        {
            std::string Prefix;
            std::string Text;
            ChatMsg Type = CHAT_MSG_PARTY;
            bool IsLogged = false;
        };

        // CMSG_CHAT_ADDON_MESSAGE
        class ChatAddonMessage final : public ClientPacket
        {
        public:
            explicit ChatAddonMessage(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_ADDON_MESSAGE, std::move(packet)) { }

            void Read() override;

            ChatAddonMessageParams Params;
        };

        // CMSG_CHAT_ADDON_MESSAGE_CHANNEL
        class ChatAddonMessageTargeted final : public ClientPacket
        {
        public:
            explicit ChatAddonMessageTargeted(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_ADDON_MESSAGE_TARGETED, std::move(packet)) { }

            void Read() override;

            ChatAddonMessageParams Params;
            std::string PlayerName;
            ObjectGuid PlayerGUID;
            uint32 PlayerVirtualRealmAddress = 0;
            std::string ChannelName;
            ObjectGuid ChannelGUID;
        };

        class ChatMessageDND final : public ClientPacket
        {
        public:
            explicit ChatMessageDND(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_MESSAGE_DND, std::move(packet)) { }

            void Read() override;

            std::string Text;
        };

        class ChatMessageAFK final : public ClientPacket
        {
        public:
            explicit ChatMessageAFK(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_MESSAGE_AFK, std::move(packet)) { }

            void Read() override;

            std::string Text;
        };

        class ChatMessageEmote final : public ClientPacket
        {
        public:
            explicit ChatMessageEmote(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_MESSAGE_EMOTE, std::move(packet)) { }

            void Read() override;

            std::string Text;
        };

        // SMSG_CHAT
        class TC_GAME_API Chat final : public ServerPacket
        {
        public:
            explicit Chat() : ServerPacket(SMSG_CHAT, 100) { }
            Chat(Chat const& chat);

            void Initialize(ChatMsg chatType, Language language, WorldObject const* sender, WorldObject const* receiver, std::string_view message, uint32 achievementId = 0,
                std::string_view channelName = "", LocaleConstant locale = DEFAULT_LOCALE, std::string_view addonPrefix = "");
            void SetSender(WorldObject const* sender, LocaleConstant locale);
            void SetReceiver(WorldObject const* receiver, LocaleConstant locale);

            WorldPacket const* Write() override;

            uint8 SlashCmd = 0;     ///< @see enum ChatMsg
            uint32 _Language = LANG_UNIVERSAL;
            ObjectGuid SenderGUID;
            ObjectGuid SenderGuildGUID;
            ObjectGuid SenderWowAccount;
            ObjectGuid TargetGUID;
            uint32 SenderVirtualAddress = 0;
            uint32 TargetVirtualAddress = 0;
            std::string SenderName;
            std::string TargetName;
            std::string Prefix;     ///< Addon Prefix
            std::string _Channel;   ///< Channel Name
            std::string ChatText;
            uint32 AchievementID = 0;
            uint32 _ChatFlags = 0;   ///< @see enum ChatFlags
            float DisplayTime = 0.0f;
            int32 SpellID = 0;
            Optional<uint32> BroadcastTextID;
            bool HideChatLog = false;
            bool FakeSenderName = false;
            Optional<ObjectGuid> ChannelGUID;
            Optional<uint32> EncounterEventID;
        };

        class Emote final : public ServerPacket
        {
        public:
            explicit Emote() : ServerPacket(SMSG_EMOTE, 18 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
            uint32 EmoteID = 0;
            std::vector<int32> SpellVisualKitIDs;
            int32 SequenceVariation = 0;
        };

        class CTextEmote final : public ClientPacket
        {
        public:
            explicit CTextEmote(WorldPacket&& packet) : ClientPacket(CMSG_SEND_TEXT_EMOTE, std::move(packet)) { }

            void Read() override;

            ObjectGuid Target;
            int32 EmoteID = 0;
            int32 SoundIndex = -1;
            Array<int32, 2> SpellVisualKitIDs;
            int32 SequenceVariation = 0;
        };

        class STextEmote final : public ServerPacket
        {
        public:
            explicit STextEmote() : ServerPacket(SMSG_TEXT_EMOTE, 3 * 18 + 2 * 4) { }

            WorldPacket const* Write() override;

            ObjectGuid SourceGUID;
            ObjectGuid SourceAccountGUID;
            ObjectGuid TargetGUID;
            int32 SoundIndex = -1;
            int32 EmoteID = 0;
        };

        class ClearBossEmotes final : public ServerPacket
        {
        public:
            explicit ClearBossEmotes() : ServerPacket(SMSG_CLEAR_BOSS_EMOTES, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class TC_GAME_API PrintNotification final : public ServerPacket
        {
        public:
            explicit PrintNotification(std::string const& notifyText) : ServerPacket(SMSG_PRINT_NOTIFICATION, 2 + notifyText.size()), NotifyText(notifyText) { }

            WorldPacket const* Write() override;

            std::string NotifyText;
        };

        class EmoteClient final : public ClientPacket
        {
        public:
            explicit EmoteClient(WorldPacket&& packet) : ClientPacket(CMSG_EMOTE, std::move(packet)) { }

            void Read() override { }
        };

        class ChatPlayerNotfound final : public ServerPacket
        {
        public:
            explicit ChatPlayerNotfound(std::string const& name) : ServerPacket(SMSG_CHAT_PLAYER_NOTFOUND, 2 + name.size()), Name(name) { }

            WorldPacket const* Write() override;

            std::string Name;
        };

        class ChatServerMessage final : public ServerPacket
        {
        public:
            explicit ChatServerMessage() : ServerPacket(SMSG_CHAT_SERVER_MESSAGE, 4 + 2) { }

            WorldPacket const* Write() override;

            int32 MessageID = 0;
            std::string_view StringParam;
        };

        class ChatRegisterAddonPrefixes final : public ClientPacket
        {
        public:
            enum
            {
                MAX_PREFIXES = 64
            };

            explicit ChatRegisterAddonPrefixes(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_REGISTER_ADDON_PREFIXES, std::move(packet)) { }

            void Read() override;

            Array<std::string, MAX_PREFIXES> Prefixes;
        };

        class ChatUnregisterAllAddonPrefixes final : public ClientPacket
        {
        public:
            explicit ChatUnregisterAllAddonPrefixes(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_UNREGISTER_ALL_ADDON_PREFIXES, std::move(packet)) { }

            void Read() override { }
        };

        class DefenseMessage final : public ServerPacket
        {
        public:
            explicit DefenseMessage() : ServerPacket(SMSG_DEFENSE_MESSAGE) { }

            WorldPacket const* Write() override;

            int32 ZoneID = 0;
            std::string MessageText;
        };

        class ChatReportIgnored final : public ClientPacket
        {
        public:
            explicit ChatReportIgnored(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_REPORT_IGNORED, std::move(packet)) { }

            void Read() override;

            ObjectGuid IgnoredGUID;
            uint8 Reason = 0;
        };

        class ChatPlayerAmbiguous final : public ServerPacket
        {
        public:
            explicit ChatPlayerAmbiguous(std::string const& name) : ServerPacket(SMSG_CHAT_PLAYER_AMBIGUOUS, 2 + name.length()), Name(name) { }

            WorldPacket const* Write() override;

            std::string Name;
        };

        class ChatRestricted final : public ServerPacket
        {
        public:
            explicit ChatRestricted() : ServerPacket(SMSG_CHAT_RESTRICTED, 4) { }

            WorldPacket const* Write() override;

            int32 Reason = 0;
        };

        class ChatNotInParty final : public ServerPacket
        {
        public:
            explicit ChatNotInParty(ChatMsg chatType) : ServerPacket(SMSG_CHAT_NOT_IN_PARTY, 4), ChatType(chatType) { }

            WorldPacket const* Write() override;

            ChatMsg ChatType;
        };

        class CanLocalWhisperTargetRequest final : public ClientPacket
        {
        public:
            explicit CanLocalWhisperTargetRequest(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_CAN_LOCAL_WHISPER_TARGET_REQUEST, std::move(packet)) { }

            void Read() override;

            ObjectGuid WhisperTarget;
        };

        class CanLocalWhisperTargetResponse final : public ServerPacket
        {
        public:
            explicit CanLocalWhisperTargetResponse() : ServerPacket(SMSG_CHAT_CAN_LOCAL_WHISPER_TARGET_RESPONSE, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid WhisperTarget;
            ChatWhisperTargetStatus Status = {};
        };

        class UpdateAADCStatus final : public ClientPacket
        {
        public:
            explicit UpdateAADCStatus(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_AADC_STATUS, std::move(packet)) { }

            void Read() override;

            bool ChatDisabled = false;
        };

        class UpdateAADCStatusResponse final : public ServerPacket
        {
        public:
            explicit UpdateAADCStatusResponse() : ServerPacket(SMSG_UPDATE_AADC_STATUS_RESPONSE, 1) { }

            WorldPacket const* Write() override;

            bool Success = false;
            bool ChatDisabled = false;
        };

        // SMSG_CHAT_IGNORED_ACCOUNT_MUTED (0x4A0000) - 0 bytes.
        // Consumer 0x1E2ED50 has signature void(void) and never touches the raw payload pointer the
        // dispatcher parks at [msg+0x20]; its first instruction is MOV ECX, 0x3B2 (946), so the shown
        // error is hardwired to GameError 946 = ERR_PARENTAL_CONTROLS_CHAT_MUTED.
        // NOT the message for an ordinary mutetime/GM mute - that is SMSG_CHAT_RESTRICTED reason 2
        // (ERR_USER_SQUELCHED). Build 12.1.0.69382, no reference packet.
        class ChatIgnoredAccountMuted final : public ServerPacket
        {
        public:
            explicit ChatIgnoredAccountMuted() : ServerPacket(SMSG_CHAT_IGNORED_ACCOUNT_MUTED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_EXPECTED_SPAM_RECORDS (0x4A0005) - the client side spam filter patterns.
        // Element reader 0x72F170 (NOT 0x6FF170, which is a POP RBP; RET tail): a byte aligned
        // uint32 count, then per JamClientSpamRecord (element size 512) two raw bytes forming a
        // 9 bit length MSB first ((A << 1) | (B >> 7)), then that many raw text bytes; the client
        // appends the null terminator itself. Each element restarts byte aligned, so the 7 spare
        // bits of the second byte are padding -> BitsSize<9> + FlushBits() per element.
        // Consumer 0x20B31A0 inserts every pattern into the global filter container 0x43BF420;
        // a hit there makes the client send CMSG_CHAT_REPORT_FILTERED (0x2C0004).
        // Build 12.1.0.69382, no reference packet.
        class ExpectedSpamRecords final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxRecordLength = 511;

            explicit ExpectedSpamRecords() : ServerPacket(SMSG_EXPECTED_SPAM_RECORDS, 4) { }

            WorldPacket const* Write() override;

            std::vector<std::string> const* Records = nullptr;
        };

        // SMSG_CHAT_NOT_IN_PARTY (0x4A0006) - { uint32 ChatMsg }, 4 bytes.
        // Reference packet: 1 packet of 4 bytes from build 68275 (the opcode was 0x470006 then),
        // payload 02 00 00 00 = CHAT_MSG_PARTY.
        // Consumer 0x1E1C6A0 accepts a closed set and silently returns for anything else:
        //   2 (PARTY), 49 (PARTY_LEADER)                  -> GameError 105 ERR_NOT_IN_GROUP
        //   3 (RAID), 39 (RAID_LEADER), 40 (RAID_WARNING) -> GameError 552 ERR_NOT_IN_RAID
        // Taken over from origin/feature/gap-closers (ChatPackets.h:342); wire and typing
        // re-verified against 69382.
        class ChatNotInParty final : public ServerPacket
        {
        public:
            explicit ChatNotInParty(ChatMsg chatType) : ServerPacket(SMSG_CHAT_NOT_IN_PARTY, 4), ChatType(chatType) { }

            WorldPacket const* Write() override;

            ChatMsg ChatType;
        };

        // SMSG_CAUTIONARY_CHAT_MESSAGE (0x4A0008) - the server withholds a whisper and asks the
        // player to confirm or drop it. Dispatcher case 4849672:
        //   bits9  = length of the first string  (incl. null terminator)
        //   bits11 = length of the second string (incl. null terminator)
        //   FlushBits (the 4 spare bits of the third byte are discarded)
        //   PackedGuid, uint32, uint32, first string data, second string data
        // 13..2587 bytes.
        // The consumer 0x2667D30 reads exactly two fields: the string object at msg+72 (that is the
        // bits11 one - string objects are 40 bytes and the message header is 32) and the uint32 at
        // msg+132. It compares that string case insensitively against field +246 of the 200 entry
        // chat line table at 0x4674E30 (stride 6280) and fires
        // CAUTIONARY_CHAT_MESSAGE(chatLineID = entry+6256, confirmNumber) on a hit.
        // Field +246 is the chat line TEXT, not a name: 0x21004B0 passes entry+246 with size 1280
        // into the JamCliSupportTicketChatLine collector, and C_ChatInfo.GetChatLineText (0xC26B90)
        // returns entry+246, or entry+3246 when the line is censored (flag at entry+6272).
        // => the bits11 string is the message text, the bits9 string is the other, shorter field.
        // This is the one point where AGENT_BRIEF_CHAT_2C_4A.md 7.2 is wrong: it labels bits9 as
        // Text and bits11 as TargetName, which is the wrong way round. Corroboration:
        // SMSG_CAUTIONARY_CHANNEL_MESSAGE, whose single string can only be the text, also carries
        // an 11 bit length.
        // Build 12.1.0.69382, no reference packet.
        class CautionaryChatMessage final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxTextLength = 2046;

            explicit CautionaryChatMessage() : ServerPacket(SMSG_CAUTIONARY_CHAT_MESSAGE, 3 + 18 + 8 + 32) { }

            WorldPacket const* Write() override;

            // UNVERIFIED: unread by the 69382 retail consumer. The 9 bit width (max 511) matches the
            // player name convention of this family; filled with the whisper target name.
            std::string TargetName;
            std::string Text;               ///< matched against the client's chat line text, see above
            ObjectGuid SenderGUID;          ///< UNVERIFIED: unread by the 69382 retail consumer
            uint32 Unused = 0;              ///< UNVERIFIED: unread by the 69382 retail consumer
            uint32 ConfirmNumber = 0;       ///< msg+132, echoed back by CMSG_CHAT_SEND/DROP_CAUTIONARY_CHAT_MESSAGE
        };

        // SMSG_CAUTIONARY_CHANNEL_MESSAGE (0x4A0009) - dispatcher case 4849673:
        //   bits11 = length of Text (incl. null terminator), FlushBits (5 spare bits discarded),
        //   uint32 ConfirmNumber (msg+72), Text data. 6..2053 bytes.
        // Consumer 0x2667E30 fires CAUTIONARY_CHANNEL_MESSAGE(confirmNumber) and then sends
        // CMSG_CHAT_SEND_CAUTIONARY_CHANNEL_MESSAGE unconditionally - there is no Lua binding and no
        // drop path, so the client always confirms. The server must not wait for a user decision.
        // Build 12.1.0.69382, no reference packet.
        class CautionaryChannelMessage final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxTextLength = 2046;

            explicit CautionaryChannelMessage() : ServerPacket(SMSG_CAUTIONARY_CHANNEL_MESSAGE, 2 + 4 + 32) { }

            WorldPacket const* Write() override;

            std::string Text;
            uint32 ConfirmNumber = 0;
        };

        // SMSG_CHAT_AUTO_RESPONDED (0x4A000E) - the AFK/DND auto reply of a whisper target.
        // Dispatcher case 4849678: bit1 IsDND, bits11 length of Text, FlushBits, uint32,
        // then Text as raw bytes (no null terminator on the wire, the client appends it).
        // Consumer 0x1DDFC30 computes ChatMsg = 0x17 + (IsDND != 0), so the bit selects
        // CHAT_MSG_AFK (23) vs CHAT_MSG_DND (24) and the string is rendered as that chat type.
        // The client buffer for Text is 1283 bytes (the uint32 lands at msg+1316, Text at msg+33),
        // so never send more than 1282 characters even though 11 bits would allow 2047.
        // Build 12.1.0.69382, no reference packet.
        class ChatAutoResponded final : public ServerPacket
        {
        public:
            static constexpr std::size_t MaxTextLength = 1282;

            explicit ChatAutoResponded() : ServerPacket(SMSG_CHAT_AUTO_RESPONDED, 2 + 4 + 32) { }

            WorldPacket const* Write() override;

            bool IsDND = false;
            std::string Text;
            uint32 SenderVirtualRealmAddress = 0;   ///< UNVERIFIED: read by the dispatcher into msg+1316, unused by the consumer
        };

        // SMSG_CHAT_DOWN (0x4A0014) and SMSG_CHAT_IS_DOWN (0x4A0015) - 0 bytes each.
        // Both hook slots (0x462E2F0, 0x462E2A8) are filled with the same function by registrar
        // 0x209FD40, and that consumer (0x20ABAB0) has signature void(void): it never reads the raw
        // payload pointer. It sets the global chat outage flag 0x47A8098 to 1 and fires
        // CHAT_SERVER_DISCONNECTED with the constant two byte payload {1, 1}
        // (Lua isInitialMessage = true). One packet class, two opcode registrations.
        // Build 12.1.0.69382, no reference packet - only a real Blizzard chat outage records this.
        class ChatDown final : public ServerPacket
        {
        public:
            explicit ChatDown(OpcodeServer opcode) : ServerPacket(opcode, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_CHAT_RECONNECT (0x4A0016) - 0 bytes.
        // Consumer 0x20ABB00, signature void(void): clears the outage flag 0x47A8098, walks the
        // client channel table (base 0x4C998E0, stride 608) restoring each channel's join state and
        // fires CHAT_SERVER_RECONNECTED. Expect a wave of CMSG_CHAT_JOIN_CHANNEL afterwards.
        // Build 12.1.0.69382, no reference packet.
        class ChatReconnect final : public ServerPacket
        {
        public:
            explicit ChatReconnect() : ServerPacket(SMSG_CHAT_RECONNECT, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_CHAT_REGIONAL_SERVICE_STATUS (0x4A001D) - { uint8 Status }, exactly 1 byte.
        // Consumer 0x20B2CD0 reads a single byte, debounces it against the cached value at
        // 0x4674E2C and only then fires CHAT_REGIONAL_STATUS_CHANGED with the Lua payload
        // isServiceAvailable = (Status == 0). Status 0 means AVAILABLE - inverted against the naive
        // reading, cross checked through C_ChatInfo.IsRegionalServiceAvailable (0xC2DF40, cmp/sete).
        // Repeating the same value produces no client event at all.
        // Build 12.1.0.69382, no reference packet.
        class ChatRegionalServiceStatus final : public ServerPacket
        {
        public:
            enum : uint8
            {
                STATUS_AVAILABLE   = 0,
                STATUS_UNAVAILABLE = 1
            };

            explicit ChatRegionalServiceStatus() : ServerPacket(SMSG_CHAT_REGIONAL_SERVICE_STATUS, 1) { }

            WorldPacket const* Write() override;

            uint8 Status = STATUS_AVAILABLE;
        };

        // SMSG_CHAT_NOT_IN_GUILD (0x4A0023) - { uint32 GuildCommandError }, 4 bytes.
        // Structurally identical to SMSG_CHAT_NOT_IN_PARTY but a DIFFERENT enum. Consumer
        // 0x1E1C6E0 has exactly two branches and silently returns for everything else:
        //   8 (ERR_GUILD_PERMISSIONS)          -> GameError 122 ERR_GUILD_PERMISSIONS
        //   9 (ERR_GUILD_PLAYER_NOT_IN_GUILD)  -> GameError 143 ERR_GUILD_NOT_IN_A_GUILD
        // Sending a ChatMsg here (CHAT_MSG_GUILD = 4) is discarded without a trace.
        // Build 12.1.0.69382, no reference packet.
        class ChatNotInGuild final : public ServerPacket
        {
        public:
            explicit ChatNotInGuild(uint32 guildCommandError) : ServerPacket(SMSG_CHAT_NOT_IN_GUILD, 4), GuildCommandError(guildCommandError) { }

            WorldPacket const* Write() override;

            uint32 GuildCommandError;
        };

        // SMSG_CHAT_LAIR_DIFFICULTY_MESSAGE (0x4A0024) - { uint32, uint16 }, exactly 6 bytes.
        // Dispatcher case 4849700 reads Read<uint32> into msg+32 and Read<uint16> into msg+36.
        // Consumer 0x20B3330 sign extends the word at msg+36 (MOVSX), looks it up in the Difficulty
        // store (handle 0x4BBDC10, identified through four independent xrefs) and prints the
        // GlobalString LAIRS_CAN_ENTER with the difficulty name, falling back to 0x3B46010 on a miss.
        // The uint32 has no reader in the 69382 retail client but must still be written because the
        // dispatcher consumes it.
        // NO SENDER ON PURPOSE: TrinityCore has no Lair content and Difficulty.db2 of 69382 has no
        // Lair row (offene Frage O1), so there is nothing that could legitimately trigger this.
        // The wire definition and the opcode registration are in place so that the sender is a one
        // liner once Lair content exists. Unit status for this opcode: blockiert.
        // Build 12.1.0.69382, no reference packet.
        class ChatLairDifficultyMessage final : public ServerPacket
        {
        public:
            explicit ChatLairDifficultyMessage() : ServerPacket(SMSG_CHAT_LAIR_DIFFICULTY_MESSAGE, 6) { }

            WorldPacket const* Write() override;

            uint32 Unused = 0;              ///< UNVERIFIED: no reader in the 69382 retail client (offene Frage O2)
            uint16 DifficultyID = 0;        ///< Difficulty::ID, looked up by the consumer
        };

        // CMSG_CHAT_SEND_CAUTIONARY_CHAT_MESSAGE (0x2C0009)
        // CMSG_CHAT_DROP_CAUTIONARY_CHAT_MESSAGE (0x2C000A)
        // CMSG_CHAT_SEND_CAUTIONARY_CHANNEL_MESSAGE (0x2C000B)
        // Serializers 0x747BD0 / 0x747C20 / 0x747C70 are identical apart from the opcode constant:
        // Write<uint32>(opcode) followed by a single Write<uint32> of the value at object+32.
        // { uint32 ConfirmNumber }, exactly 4 bytes. The client names the value itself in the usage
        // strings 0x3C709F0 and 0x3C70410 ("...(confirmNumber)").
        // Build 12.1.0.69382, no reference packet.
        class CautionaryAction final : public ClientPacket
        {
        public:
            explicit CautionaryAction(WorldPacket&& packet);

            void Read() override;

            uint32 ConfirmNumber = 0;
        };
    }
}

#endif // TRINITYCORE_CHAT_PACKETS_H

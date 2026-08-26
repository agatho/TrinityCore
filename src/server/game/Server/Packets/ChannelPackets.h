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

#ifndef TRINITYCORE_CHANNEL_PACKETS_H
#define TRINITYCORE_CHANNEL_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"

namespace WorldPackets
{
    namespace Channel
    {
        class ChannelListResponse final : public ServerPacket
        {
        public:
            struct ChannelPlayer
            {
                ChannelPlayer(ObjectGuid const& guid, uint32 virtualRealmAddress, uint8 flags) :
                    Guid(guid), VirtualRealmAddress(virtualRealmAddress), Flags(flags) { }

                ObjectGuid Guid; ///< Player Guid
                uint32 VirtualRealmAddress;
                uint8 Flags;     ///< @see enum ChannelMemberFlags
            };

            explicit ChannelListResponse() : ServerPacket(SMSG_CHANNEL_LIST) { }

            WorldPacket const* Write() override;

            std::vector<ChannelPlayer> Members;
            std::string _Channel; ///< Channel Name
            uint32 _ChannelFlags = 0; ///< @see enum ChannelFlags
            bool Display = false;
        };

        class TC_GAME_API ChannelNotify final : public ServerPacket
        {
        public:
            explicit ChannelNotify() : ServerPacket(SMSG_CHANNEL_NOTIFY, 80) { }

            WorldPacket const* Write() override;

            std::string Sender;
            ObjectGuid SenderGuid;
            ObjectGuid SenderAccountID;
            uint8 Type                = 0; ///< @see enum ChatNotify
            uint8 OldFlags            = 0; ///< @see enum ChannelMemberFlags
            uint8 NewFlags            = 0; ///< @see enum ChannelMemberFlags
            std::string _Channel;          ///< Channel Name
            uint32 SenderVirtualRealm = 0;
            ObjectGuid TargetGuid;
            uint32 TargetVirtualRealm = 0;
            int32 ChatChannelID       = 0;
        };

        class ChannelNotifyJoined final : public ServerPacket
        {
        public:
            explicit ChannelNotifyJoined() : ServerPacket(SMSG_CHANNEL_NOTIFY_JOINED, 50) { }

            WorldPacket const* Write() override;

            std::string ChannelWelcomeMsg;
            int32 ChatChannelID = 0;
            uint64 InstanceID    = 0;
            uint32 _ChannelFlags = 0; ///< @see enum ChannelFlags
            std::string _Channel;     ///< Channel Name
            ObjectGuid ChannelGUID;
            uint8 Unknown1107 = 0;
        };

        class ChannelNotifyLeft final : public ServerPacket
        {
        public:
                explicit ChannelNotifyLeft() : ServerPacket(SMSG_CHANNEL_NOTIFY_LEFT, 30) { }

            WorldPacket const* Write() override;

            std::string Channel;    ///< Channel Name
            int32 ChatChannelID = 0;
            bool Suspended = false; ///< User Leave - false, On Zone Change - true
        };

        class UserlistAdd final : public ServerPacket
        {
        public:
            explicit UserlistAdd() : ServerPacket(SMSG_USERLIST_ADD, 30) { }

            WorldPacket const* Write() override;

            ObjectGuid AddedUserGUID;
            uint32 _ChannelFlags = 0; ///< @see enum ChannelFlags
            uint8 UserFlags = 0; ///< @see enum ChannelMemberFlags
            int32 ChannelID = 0;
            std::string ChannelName;
        };

        class UserlistRemove final : public ServerPacket
        {
        public:
            explicit UserlistRemove() : ServerPacket(SMSG_USERLIST_REMOVE, 30) { }

            WorldPacket const* Write() override;

            ObjectGuid RemovedUserGUID;
            uint32 _ChannelFlags = 0; ///< @see enum ChannelFlags
            uint32 ChannelID = 0;
            std::string ChannelName;
        };

        class UserlistUpdate final : public ServerPacket
        {
        public:
            explicit UserlistUpdate() : ServerPacket(SMSG_USERLIST_UPDATE, 30) { }

            WorldPacket const* Write() override;

            ObjectGuid UpdatedUserGUID;
            uint32 _ChannelFlags = 0; ///< @see enum ChannelFlags
            uint8 UserFlags = 0; ///< @see enum ChannelMemberFlags
            int32 ChannelID = 0;
            std::string ChannelName;
        };

        // SMSG_CHANNEL_NOTIFY_NPE_JOINED_BATCH (0x4A0018) - { uint32, uint32, uint32 }, exactly
        // 12 bytes. Raw pointer class; the format is in consumer 0x20AB190, which reads three
        // dwords at +0, +4 and +8:
        //   +0 ChatChannelID - must be non zero AND must match field +0x130 (304) of an entry in the
        //      client's own channel table (base 0x4C998E0, stride 608), otherwise nothing happens
        //   +4, +8 - the two %d arguments of the GlobalString NPEV2_CHAT_BATCH_JOIN_MESSAGE
        //      (string at 0x3D79070, binary only - it is not in the 12.1 UI source tree), passed in
        //      that order (raw listing at 0x20AB211..0x20AB227)
        // The whole path is additionally gated on dword_7FF7852F3918 (RVA 0x4323918) being zero;
        // that gate is 1 in the retail image, so retail clients drop this silently.
        // Server side it belongs to the only Mentor ruleset channel, ChatChannels row 32
        // "Newcomer Chat".
        // Build 12.1.0.69382, no reference packet.
        class ChannelNotifyNPEJoinedBatch final : public ServerPacket
        {
        public:
            explicit ChannelNotifyNPEJoinedBatch() : ServerPacket(SMSG_CHANNEL_NOTIFY_NPE_JOINED_BATCH, 12) { }

            WorldPacket const* Write() override;

            uint32 ChatChannelID = 0;
            uint32 JoinedCount = 0;     ///< UNVERIFIED: first %d of NPEV2_CHAT_BATCH_JOIN_MESSAGE
            uint32 TotalCount = 0;      ///< UNVERIFIED: second %d of NPEV2_CHAT_BATCH_JOIN_MESSAGE
        };

        class ChannelCommand final : public ClientPacket
        {
        public:
            explicit ChannelCommand(WorldPacket&& packet);

            void Read() override;

            std::string ChannelName;
        };

        class ChannelPlayerCommand final : public ClientPacket
        {
        public:
            explicit ChannelPlayerCommand(WorldPacket&& packet);

            void Read() override;

            std::string ChannelName;
            std::string Name;
        };

        class ChannelPassword final : public ClientPacket
        {
        public:
            explicit ChannelPassword(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_CHANNEL_PASSWORD, std::move(packet)) { }

            void Read() override;

            std::string ChannelName;
            std::string Password;
        };

        class JoinChannel final : public ClientPacket
        {
        public:
            explicit JoinChannel(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_JOIN_CHANNEL, std::move(packet)) { }

            void Read() override;

            std::string Password;
            std::string ChannelName;
            bool CreateVoiceSession = false;
            int32 ChatChannelId         = 0;
            bool Internal           = false;
        };

        class LeaveChannel final : public ClientPacket
        {
        public:
            explicit LeaveChannel(WorldPacket&& packet) : ClientPacket(CMSG_CHAT_LEAVE_CHANNEL, std::move(packet)) { }

            void Read() override;

            int32 ZoneChannelID = 0;
            std::string ChannelName;
        };
    }
}

#endif // TRINITYCORE_CHANNEL_PACKETS_H

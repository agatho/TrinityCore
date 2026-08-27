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

#ifndef TRINITYCORE_AUTHENTICATION_PACKETS_H
#define TRINITYCORE_AUTHENTICATION_PACKETS_H

#include "Packet.h"
#include "Define.h"
#include "Optional.h"
#include "PacketUtilities.h"
#include <array>

struct CharacterTemplate;
struct RaceClassAvailability;

namespace WorldPackets
{
    namespace Auth
    {
        template <typename Derived>
        class EarlyProcessClientPacket : public ClientPacket
        {
            explicit EarlyProcessClientPacket(OpcodeClient opcode, WorldPacket&& packet) : ClientPacket(opcode, std::move(packet)) { }

        public:
            bool ReadNoThrow() try
            {
                static_cast<Derived*>(this)->Read();
                return true;
            }
            catch (ByteBufferException const& /*ex*/)
            {
                return false;
            }

            friend Derived;
        };

        class Ping final : public EarlyProcessClientPacket<Ping>
        {
        public:
            explicit Ping(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_PING, std::move(packet)) { }

            uint32 Serial = 0;
            uint32 Latency = 0;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class Pong final : public ServerPacket
        {
        public:
            explicit Pong(uint32 serial) : ServerPacket(SMSG_PONG, 4), Serial(serial) { }

            WorldPacket const* Write() override;

            uint32 Serial = 0;
        };

        class LogDisconnect final : public EarlyProcessClientPacket<LogDisconnect>
        {
        public:
            explicit LogDisconnect(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_LOG_DISCONNECT, std::move(packet)) { }

            uint32 Reason = 0;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class AuthChallenge final : public ServerPacket
        {
        public:
            explicit AuthChallenge() : ServerPacket(SMSG_AUTH_CHALLENGE, 16 + 4 * 8 + 1) { }

            WorldPacket const* Write() override;

            std::array<uint8, 32> Challenge = { };
            std::array<uint32, 8> DosChallenge = { };
            uint8 DosZeroBits = 0;
        };

        class AuthSession final : public EarlyProcessClientPacket<AuthSession>
        {
        public:
            static constexpr uint32 DigestLength = 24;

            explicit AuthSession(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_AUTH_SESSION, std::move(packet)) { }

            uint32 RegionID = 0;
            uint32 BattlegroupID = 0;
            uint32 RealmID = 0;
            std::array<uint8, 32> LocalChallenge = { };
            std::array<uint8, DigestLength> Digest = { };
            uint64 DosResponse = 0;
            std::string RealmJoinTicket;
            bool UseIPv6 = false;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        struct AuthWaitInfo
        {
            uint32 WaitCount = 0; ///< position of the account in the login queue
            uint32 WaitTime = 0; ///< Wait time in login queue in minutes, if sent queued and this value is 0 client displays "unknown time"
            uint8 AllowedFactionGroupForCharacterCreate = 0;
            bool HasFCM = false; ///< true if the account has a forced character migration pending. @todo implement
            bool CanCreateOnlyIfExisting = false; ///< Can create characters on realm only if player has other existing characters there
        };

        struct VirtualRealmNameInfo
        {
            VirtualRealmNameInfo() : IsLocal(false), IsInternalRealm(false) { }
            VirtualRealmNameInfo(bool isHomeRealm, bool isInternalRealm, std::string const& realmNameActual, std::string const& realmNameNormalized) :
                IsLocal(isHomeRealm), IsInternalRealm(isInternalRealm), RealmNameActual(realmNameActual), RealmNameNormalized(realmNameNormalized) { }

            bool IsLocal;                    ///< true if the realm is the same as the account's home realm
            bool IsInternalRealm;            ///< @todo research
            std::string RealmNameActual;     ///< the name of the realm
            std::string RealmNameNormalized; ///< the name of the realm without spaces
        };

        struct VirtualRealmInfo
        {
            VirtualRealmInfo() : RealmAddress(0) { }
            VirtualRealmInfo(uint32 realmAddress, bool isHomeRealm, bool isInternalRealm, std::string const& realmNameActual, std::string const& realmNameNormalized) :
                RealmAddress(realmAddress), RealmNameInfo(isHomeRealm, isInternalRealm, realmNameActual, realmNameNormalized) { }

            uint32 RealmAddress;             ///< the virtual address of this realm, constructed as RealmHandle::Region << 24 | RealmHandle::Battlegroup << 16 | RealmHandle::Index
            VirtualRealmNameInfo RealmNameInfo;
        };

        struct GameTime
        {
            uint32 BillingType = 0;
            uint32 MinutesRemaining = 0;
            uint32 RealBillingType = 0;
            bool IsInIGR = false;
            bool IsPaidForByIGR = false;
            bool IsCAISEnabled = false;
        };

        struct BaseBuildKey
        {
            std::array<uint8, 16> BuildKey = { };
            std::array<uint8, 16> ConfigKey = { };
        };

        struct AuthSuccessInfo
        {
            uint8 ActiveExpansionLevel = 0; ///< the current server expansion, the possible values are in @ref Expansions
            uint8 AccountExpansionLevel = 0; ///< the current expansion of this account, the possible values are in @ref Expansions
            uint32 TimeRested = 0; ///< affects the return value of the GetBillingTimeRested() client API call, it is the number of seconds you have left until the experience points and loot you receive from creatures and quests is reduced. It is only used in the Asia region in retail, it's not implemented in TC and will probably never be.

            uint32 VirtualRealmAddress = 0; ///< a special identifier made from the Index, BattleGroup and Region.
            uint32 TimeSecondsUntilPCKick = 0; ///< @todo research
            uint32 CurrencyID = 0; ///< this is probably used for the ingame shop. @todo implement
            Timestamp<> Time;

            GameTime GameTimeInfo;

            std::vector<VirtualRealmInfo> VirtualRealms;     ///< list of realms connected to this one (inclusive) @todo implement
            std::vector<CharacterTemplate const*> Templates; ///< list of pre-made character templates.

            std::vector<RaceClassAvailability> const* AvailableClasses = nullptr; ///< the minimum AccountExpansion required to select race/class combinations

            bool IsExpansionTrial = false;
            bool ForceCharacterTemplate = false; ///< forces the client to always use a character template when creating a new character. @see Templates. @todo implement
            Optional<uint16> NumPlayersHorde; ///< number of horde players in this realm. @todo implement
            Optional<uint16> NumPlayersAlliance; ///< number of alliance players in this realm. @todo implement
            Optional<Timestamp<>> ExpansionTrialExpiration; ///< expansion trial expiration unix timestamp
            Optional<BaseBuildKey> CurrentBuild;
        };

        class AuthResponse final : public ServerPacket
        {
        public:
            explicit AuthResponse() : ServerPacket(SMSG_AUTH_RESPONSE, 132) { }

            WorldPacket const* Write() override;

            Optional<AuthSuccessInfo> SuccessInfo; ///< contains the packet data in case that it has account information (It is never set when WaitInfo is set), otherwise its contents are undefined.
            Optional<AuthWaitInfo> WaitInfo; ///< contains the queue wait information in case the account is in the login queue.
            uint32 Result = 0; ///< the result of the authentication process, possible values are @ref BattlenetRpcErrorCode
        };

        class WaitQueueUpdate final : public ServerPacket
        {
        public:
            explicit WaitQueueUpdate() : ServerPacket(SMSG_WAIT_QUEUE_UPDATE, 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            AuthWaitInfo WaitInfo;
        };

        class WaitQueueFinish final : public ServerPacket
        {
        public:
            explicit WaitQueueFinish() : ServerPacket(SMSG_WAIT_QUEUE_FINISH, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        enum class ConnectToSerial : uint32
        {
            None            = 0,
            Realm           = 14,
            WorldAttempt1   = 17,
            WorldAttempt2   = 35,
            WorldAttempt3   = 53,
            WorldAttempt4   = 71,
            WorldAttempt5   = 89
        };

        class TC_GAME_API ConnectTo final : public ServerPacket
        {
        public:
            static bool InitializeEncryption();
            static void ShutdownEncryption();

            enum AddressType : uint8
            {
                None = 0,
                IPv4 = 1,
                IPv6 = 2,
                NamedSocket = 3 // not supported by windows client
            };

            struct SocketAddress
            {
                AddressType Type = None;
                union
                {
                    std::array<uint8, 4> V4;
                    std::array<uint8, 16> V6;
                    std::array<char, 128> Name;
                } Address = { };
            };

            struct BleepToken
            {
                std::string_view Token;
                std::string_view ProxyId;
                std::string_view Address;
                Duration<std::chrono::nanoseconds> TokenLifespan;
            };

            struct ConnectPayload
            {
                SocketAddress Address;
                uint16 Port = 0;
                BleepToken Token;
            };

            explicit ConnectTo() : ServerPacket(SMSG_CONNECT_TO, 4 + 4 + 1 + 8 + 4 + 4 + 1 + 16 + 2 + 1 + 3 + 1 + 8 + 0 + 0 + 0) { }

            WorldPacket const* Write() override;

            uint64 Key = 0;
            uint32 NativeRealmAddress = 0;
            uint32 Key3 = 0;
            ConnectToSerial Serial = ConnectToSerial::None;
            std::vector<ConnectPayload> Payload;
            uint8 Con = 0;
        };

        class AuthContinuedSession final : public EarlyProcessClientPacket<AuthContinuedSession>
        {
        public:
            static constexpr uint32 DigestLength = 24;

            explicit AuthContinuedSession(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_AUTH_CONTINUED_SESSION, std::move(packet)) { }

            uint64 DosResponse = 0;
            uint64 Key = 0;
            uint32 NativeRealmAddress = 0;
            uint32 Key3 = 0;
            std::array<uint8, 32> LocalChallenge = { };
            std::array<uint8, DigestLength> Digest = { };

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class ResumeComms final : public ServerPacket
        {
        public:
            explicit ResumeComms(ConnectionType connection) : ServerPacket(SMSG_RESUME_COMMS, 0, connection) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class ConnectToFailed final : public EarlyProcessClientPacket<ConnectToFailed>
        {
        public:
            explicit ConnectToFailed(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_CONNECT_TO_FAILED, std::move(packet)) { }

            ConnectToSerial Serial = ConnectToSerial::None;
            uint8 Con = 0;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class TC_GAME_API EnterEncryptedMode final : public ServerPacket
        {
        public:
            static bool InitializeEncryption();
            static void ShutdownEncryption();

            explicit EnterEncryptedMode(std::array<uint8, 32> const& encryptionKey, bool enabled) : ServerPacket(SMSG_ENTER_ENCRYPTED_MODE, 4 + 256 + 1),
                EncryptionKey(encryptionKey), Enabled(enabled)
            {
            }

            WorldPacket const* Write() override;

            int32 RegionGroup = 0;
            std::array<uint8, 32> const& EncryptionKey;
            bool Enabled = false;
        };

        class QueuedMessagesEnd final : public ClientPacket
        {
        public:
            explicit QueuedMessagesEnd(WorldPacket&& packet) : ClientPacket(CMSG_QUEUED_MESSAGES_END, std::move(packet)) { }

            void Read() override;

            uint32 Timestamp = 0;
        };

        // SMSG_SUSPEND_COMMS (12.1 0x4C0005) - one uint32, 4 bytes on the wire.
        // Measured: consumer 0x18C1610 of client 12.1.0.69382 reads exactly the first four bytes and echoes them
        // in CMSG_SUSPEND_COMMS_ACK; 78 captured packets over both build windows are 4 bytes without exception.
        //
        // WHERE THIS MAY BE SENT - the client enforces all of this and answers a violation with
        // CMSG_LOG_DISCONNECT(3) followed by closing the socket, so read it before adding a send site:
        //   * only on an ESTABLISHED base socket (client slot 0 = realm, slot 1 = instance) whose suspend flag is
        //     still 0. A socket that is still pending handover lives in slot idx|2 and the consumer rejects it on
        //     (idx & 2) != 0.
        //   * only AFTER that socket has entered encrypted mode - 0x4C0005 is not in the pre-encryption whitelist
        //     (bitmask 0x2A1F at RVA 0x389DC00 admits only 0x4C0000..0x4C0004, 0x4C0009, 0x4C000B, 0x4C000D and
        //     SMSG_AUTH_FAILED).
        //   * NEVER on a freshly connected instance socket. The NetClient constructor (0x18BEA60, store at
        //     0x18BEF6D: mov word ptr [rbx+0x218], 0x100) already marks slot 1 as suspended, which is why this
        //     server's unpaired SMSG_RESUME_COMMS works at all. Suspending it a second time is error case 3 -
        //     that exact mistake broke enter-world for every character once (88bcee58de, reverted by 5778fe4266).
        //   * once suspended, ONLY family 0x4C may travel over that socket until SMSG_RESUME_COMMS clears the
        //     flag (gate 0x18C0F60). Any other opcode in that window closes the connection.
        // Retail only ever does this when redirecting an ALREADY established slot, which this server never does.
        // There is deliberately no send site - see decision O1 in the conn_44_4C status file.
        class SuspendComms final : public ServerPacket
        {
        public:
            explicit SuspendComms(ConnectionType connection) : ServerPacket(SMSG_SUSPEND_COMMS, 4, connection) { }

            WorldPacket const* Write() override;

            uint32 SerialNumber = 0;     ///< opaque to the client, echoed back verbatim
        };

        // CMSG_SUSPEND_COMMS_ACK (12.1 0x440000) - two uint32, 8 bytes. Writer 0x5D54B0.
        // Serial echo verified 78/78 in the captures, always on the same connection index.
        class SuspendCommsAck final : public ClientPacket
        {
        public:
            explicit SuspendCommsAck(WorldPacket&& packet) : ClientPacket(CMSG_SUSPEND_COMMS_ACK, std::move(packet)) { }

            void Read() override;

            uint32 SerialNumber = 0;     ///< echoed from SMSG_SUSPEND_COMMS
            // Client clock in milliseconds, taken by 0x354ED50 at the moment the ack is built. It is NOT unix time
            // and NOT a server value: across 33 captured pairs (ClientTick - capture tick) is constant within a
            // single capture and different between captures, in the 1.7 h .. 7.7 d range of a process uptime.
            // Usable as a clock-delta sample, never as a value to validate against server time.
            uint32 ClientTick = 0;
        };

        // SMSG_DROP_NEW_CONNECTION (12.1 0x4C0007) - empty.
        // Measured: consumer 0x18C1500 never touches msg+0x20. Effect: the client tears down the PENDING socket of
        // the slot it arrives on (slot idx|2), delivers CMSG_LOG_DISCONNECT(11) to that pending socket, releases its
        // compression context and clears the slot. It is the counterpart of SMSG_CONNECT_TO and has to be sent on
        // the ESTABLISHED socket, not on the pending one. If the pending slot is empty the consumer does nothing.
        // UNVERIFIED: zero length is what the consumer reads, not what retail puts on the wire - the opcode occurs
        // in none of the 25 captures (0 raw occurrences of both 0x4C0007 and 0x490007), so no reference bytes exist.
        // For a server that produces the packet, empty is the only defensible choice.
        class DropNewConnection final : public ServerPacket
        {
        public:
            explicit DropNewConnection(ConnectionType connection) : ServerPacket(SMSG_DROP_NEW_CONNECTION, 0, connection) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_RESET_COMPRESSION_CONTEXT (12.1 0x4C000A) - empty.
        // Measured: consumer 0x18C0D90 is 0x36 bytes long and tail-jumps to zlib inflateReset (0x2C3BB0, identified
        // field by field down to the HEAD state magic 0x3F34) on the decompression stream of its own slot
        // (NetClient + 0x1C0 + 8*idx). It reads no payload.
        // The server MUST deflateReset its own send stream in the same breath: the client throws away its inflate
        // window, so everything it decompresses afterwards has to belong to a NEW deflate stream, or it aborts with
        // CMSG_LOG_DISCONNECT(17).
        // Note this opcode is not in the pre-encryption whitelist, but compression is bound to NeedsEncryption(), so
        // it can only ever be produced once encrypted mode is active.
        // UNVERIFIED: as with SMSG_DROP_NEW_CONNECTION, zero length is measured on the read side only - 0 packets in
        // 25 captures.
        class ResetCompressionContext final : public ServerPacket
        {
        public:
            explicit ResetCompressionContext(ConnectionType connection) : ServerPacket(SMSG_RESET_COMPRESSION_CONTEXT, 0, connection) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // One entry of CMSG_LATENCY_REPORT. Client type name JamLatencyReportEntry, proven by the destructor of the
        // message class (0x5D4890) freeing the element buffer tagged
        // WowGetRawTypeName<struct JamLatencyReportEntry>.
        // 21 bytes, NOT aligned - the uint8 in the middle breaks the alignment of the following uint64, so this has
        // to be read field by field and must never be memcpy'd over a struct. Wire offsets: 0, 4, 8, 9, 17.
        // Only three of the five fields are named by the reflection descriptor (tag record 0x388E9D8):
        // server@0x00, timestampMS@0x10, frame@0x18 (those are in-memory offsets, not wire offsets).
        struct LatencyReportEntry
        {
            // Named `server` in the descriptor. A packed triple, not an opcode: only 13 distinct values across
            // 20016 captured entries (e.g. 0x03050056, 0x01040054) and the low 24 bits match no Opcodes.h entry of
            // either build.
            // UNVERIFIED: meaning of the packed subfields.
            uint32 Server = 0;
            // UNVERIFIED: no name in the reflection descriptor. Observed range 0 .. 77035.
            uint32 Unknown4 = 0;
            // UNVERIFIED: no name in the reflection descriptor. Observed values are exactly {0, 12, 33}; 33 always
            // comes with Server == 0 && Unknown4 == 0 and marks the entry the sender appends itself (0x20E6F0).
            uint8 Unknown8 = 0;
            // Named `timestampMS`. Unix time in milliseconds - verified against the capture dates and independently
            // against the PKT record's own optData double; 0 null values in 20016 entries, monotonic across packets
            // (2849 comparisons, 0 violations).
            uint64 TimestampMS = 0;
            // Named `frame`. The client's frame rate. Strongest name proof of this family: the sender reads it from
            // 0x29AF640, and that very function is the implementation behind the Lua API GetFramerate.
            uint32 Frame = 0;
        };

        // CMSG_LATENCY_REPORT (12.1 0x44000F) - writer 0x5D6020, sender 0x20E6F0.
        // Length is 8 + 21*Count, verified on 4522 captured packets at 100.00 % against both
        // (len - 8) % 21 == 0 and (len - 8) / 21 == Count, with 0 outliers.
        // There is no reply and no Lua surface: the client sends this self-timed from C++, the Lua side can neither
        // trigger nor observe it, and the consumer sends nothing back. The server must not answer.
        class LatencyReport final : public ClientPacket
        {
        public:
            // The client cannot send more. Its latency ring buffer (NetClient + 0x58*(idx+2) + 0x10) has exactly
            // 16 slots; the highest Count actually observed in 4522 packets is 11.
            static constexpr std::size_t MaxEntries = 16;

            explicit LatencyReport(WorldPacket&& packet) : ClientPacket(CMSG_LATENCY_REPORT, std::move(packet)) { }

            void Read() override;

            // Stage index, NOT a report type and NOT a discriminator for different entry formats - the format is
            // identical for all three. Measured coupling over 4522 packets without a single exception:
            // Kind 0 -> Count 3, Kind 1 -> Count 7, Kind 2 -> Count 11, arriving as a triple roughly 200 ms apart,
            // and the body of each packet begins byte-identically with the complete body of its predecessor.
            // The list is therefore CUMULATIVE: whoever stores every packet stores every measurement three times.
            uint32 Kind = 0;
            Array<LatencyReportEntry, MaxEntries> Entries;
        };

        // CMSG_LOG_STREAMING_ERROR (12.1 0x44000B) - writer 0x5D5B20, object size 544.
        //   bits<9> MessageLen, then MessageLen bytes without NUL.
        // 9 bits because the client's buffer is 512 bytes (N = ceil(log2(512))); the writer splits the 9 bits into
        // Write<uint8>(len >> 1) plus the embedded low bit plus FlushBits, which is byte-identical to
        // WriteBits(len, 9); FlushBits().
        // Content is a free-form English CASC/TACT error message - the 512-byte buffer is a vsnprintf target in the
        // streaming logger 0x35B6050, wrapped as 'Streaming Error: %s', and only severity >= 4 reaches the error
        // ring (64 slots). It is diagnostics, not structured data: there is nothing to parse and no reply. The
        // Streaming Lua API has an empty Events table, so there is no client-observable effect either.
        class LogStreamingError final : public ClientPacket
        {
        public:
            static constexpr std::size_t MaxMessageLength = 511;    ///< 9 bits, and the client buffer is char[512]

            explicit LogStreamingError(WorldPacket&& packet) : ClientPacket(CMSG_LOG_STREAMING_ERROR, std::move(packet)) { }

            void Read() override;

            std::string Message;
        };

        ByteBuffer& operator<<(ByteBuffer& data, VirtualRealmInfo const& realmInfo);
        ByteBuffer& operator<<(ByteBuffer& data, VirtualRealmNameInfo const& realmInfo);
    }
}

#endif // TRINITYCORE_AUTHENTICATION_PACKETS_H

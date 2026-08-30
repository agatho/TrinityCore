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

        // THE CORPUS. Every "over the corpus" figure - in the classes below, and in AuthHandler.cpp,
        // MovementHandler.cpp, QueryPackets.h and Opcodes.cpp, which all point back here - is measured over ONE
        // set of captures, and it is this one, so that any of them can be recounted and none of them has to be
        // taken on trust: the 25 recordings whose build window this project has established - 12 in the 12.1 window
        // (builds 69273, 69299, 69382, 69404) and 13 in the 12.0.7 window (builds 68275, 68453, 68974) -
        // 2 376 635 records in total, deduplicated by content hash. Further recordings exist and are deliberately
        // EXCLUDED: they are 12.0.1 / 12.0.5 / legacy, and this project has not established their opcode
        // numbering. Counting a foreign build's number over a mixed corpus counts a different opcode - 0x490007 is
        // SMSG_DROP_NEW_CONNECTION in 12.0.7 but SMSG_QUERY_GAME_OBJECT_RESPONSE in 12.1 - and that is not a
        // theoretical objection: counting the 12.0.7 numbers over everything yields 16 SMSG_DROP_NEW_CONNECTION,
        // 55 SMSG_MULTIPLE_PACKETS and 13 CMSG_QUERY_PLAYER_NAMES_FOR_COMMUNITY where the build-resolved count is
        // 0 for all three. Where a figure belongs to one window only, the window is named at the figure.
        //
        // SMSG_SUSPEND_COMMS (12.1 0x4C0005) - one uint32, 4 bytes on the wire.
        // Measured: consumer 0x18C1610 of client 12.1.0.69382 reads exactly the first four bytes and echoes them
        // in CMSG_SUSPEND_COMMS_ACK. Over the corpus: 78 packets, all 4 bytes, no exception.
        //
        // WHERE THIS MAY BE SENT - the client enforces all of this and answers a violation with
        // CMSG_LOG_DISCONNECT(3) followed by closing the socket, so read it before adding a send site.
        // Decompiled consumer 0x18C1610, complete, RVAs against image base 0x7FF780FD0000:
        //     idx = index of the RECEIVING socket in NetClient+0x1A0[0..3]        // not found -> return
        //     if ((idx & 2) != 0 || NetClient[0x218 + (idx & 1)])                 // -> error 3, close socket
        //     else { send CMSG_SUSPEND_COMMS_ACK{ serial, clientTick }; NetClient[0x218 + (idx & 1)] = 1; }
        // So:
        //   * only on an ESTABLISHED base socket (client slot 0 = realm, slot 1 = instance) whose suspend flag is
        //     still 0. A socket that is still pending handover lives in slot idx|2 and is rejected on (idx & 2).
        //   * only AFTER that socket has entered encrypted mode - 0x4C0005 is not in the pre-encryption whitelist
        //     (bitmask 0x2A1F at RVA 0x389DC00 admits only 0x4C0000..0x4C0004, 0x4C0009, 0x4C000B, 0x4C000D and
        //     SMSG_AUTH_FAILED).
        //   * NEVER on the freshly connected socket of the handover itself. That socket is the PENDING one at
        //     idx|2, so it fails on (idx & 2) alone; and for the instance slot it fails a second time on the flag,
        //     because the NetClient constructor (0x18BEA60, store at 0x18BEF6D: mov word ptr [rbx+0x218], 0x100 -
        //     a 16 bit store, so flag[0] = 0 and flag[1] = 1) marks slot 1 as suspended from birth, which is why
        //     this server's unpaired SMSG_RESUME_COMMS works at all. Sending it there is error case 3 - that exact
        //     mistake broke enter-world for every character once (88bcee58de, reverted by 5778fe4266).
        //   * once suspended, ONLY family 0x4C may travel over that socket until SMSG_RESUME_COMMS clears the
        //     flag (gate 0x18C0F60). Any other opcode in that window closes the connection.
        //
        // WHEN RETAIL SENDS IT - the condition, not a description of one observed run. The flag at
        // NetClient+0x218+(idx&1) is 1 in exactly three situations: from the constructor for slot 1; after a
        // socket in slot idx was closed with idx != 0 (0x18BDC80, 0x18BDDC0: `test edi,edi; setnz al;
        // mov [r14+rbp+0x218], al` - so flag[idx] = (idx != 0)); and after a SMSG_SUSPEND_COMMS. The RESUME
        // consumer 0x18C1720 REQUIRES that flag to be set and answers a clear flag with error 3. Therefore:
        //   * INSTANCE slot (1), base slot empty -> the flag is already 1, from the constructor or from the close
        //     of the socket that was there, and RESUME alone completes the handover. A SUSPEND is forbidden here,
        //     see above. This is the only case this server produces, and it is why its unpaired RESUME works.
        //   * INSTANCE slot, base slot still holding an OPEN socket -> the flag is 0, because the RESUME that
        //     promoted that socket cleared it (0x18C1955), and the handover CANNOT complete without a SUSPEND on
        //     that open socket first.
        //   * REALM slot (0) -> always the second case, occupied or not: flag[0] is 0 from the constructor and
        //     closing slot 0 leaves it 0 (idx == 0 makes the setnz store a zero), so only a SUSPEND can ever set
        //     it. A realm redirect without one is impossible, which is what the corpus shows.
        // The observable consequence of the second case is that RESUME evicts the old socket with error 11 before
        // moving the pending one in (0x18C1720). Measured over the corpus, exception-free: of 103
        // SMSG_RESUME_COMMS, 78 have a SMSG_SUSPEND_COMMS inside their handover window and are followed by
        // CMSG_LOG_DISCONNECT(11), and 25 have neither. 78 is also the total number of SMSG_SUSPEND_COMMS in the
        // corpus, so every one of them belongs to such a handover and none is unaccounted for.
        //
        // READ THE CAPTURES CAREFULLY HERE, because they look like they say the opposite. A PKT connection index
        // is a socket SLOT that the sniffer reuses once a socket closes, so in 24 of the 25 captures the old and
        // the new socket of a handover carry the SAME index and the SUSPEND appears to sit on the fresh one - two
        // reviews have read it that way. garrisonlevel2upgrade.pkt is the one capture that gives them different
        // indices, and there SUSPEND+ACK are on the old socket (index 0) while RESUME is on the new one
        // (index 221). The client code above is the arbiter and agrees; the indices do not decide anything.
        //
        // WHY THERE IS NO CALL SITE HERE, as a checkable statement rather than a claim: this server never
        // authenticates a replacement socket while the base socket of that slot is still open, so it never meets
        // the condition above. The realm socket is never redirected (ConnectToSerial::Realm is declared and
        // unused; the branch is the comment in WorldSocket::HandleConnectToFailed). For the instance socket, the
        // only path to a CONNECT_TO is WorldSession::HandlePlayerLoginOpcode, which requires !PlayerLoading() and
        // no player, and both exits from a logged-in state close m_Socket[CONNECTION_TYPE_INSTANCE] first
        // (WorldSession::LogoutPlayer and ~WorldSession) - which restores flag[1] to 1 on the client side by the
        // rule above. The retry path in WorldSocket::HandleConnectToFailed fires only when the replacement never
        // connected. WorldSession::AddInstanceConnection carries the guard that detects the exception, because
        // in that state this server's unpaired SMSG_RESUME_COMMS is what gets error 3.
        // Decision O1 of unit conn_44_4C: this class and its acknowledgement are complete and registered, the
        // sending half is deliberately left without a caller, and D2/D3 are recorded as NOT met for this opcode.
        // Do NOT construct and send this directly when adding a caller. WorldSession::SendSuspendComms is the
        // single issuer of the serial: it mints the value, remembers it and registers the matching time sync
        // counter, and WorldSession::HandleSuspendCommsAck accepts the acknowledgement only for a serial issued
        // there. A hand rolled send would hand the serial space back to the client.
        class SuspendComms final : public ServerPacket
        {
        public:
            explicit SuspendComms(ConnectionType connection) : ServerPacket(SMSG_SUSPEND_COMMS, 4, connection) { }

            WorldPacket const* Write() override;

            uint32 SerialNumber = 0;     ///< opaque to the client, echoed back verbatim
        };

        // CMSG_SUSPEND_COMMS_ACK (12.1 0x440000) - two uint32, 8 bytes. Writer 0x5D54B0.
        // Over the corpus: 78 packets, all 8 bytes, serial echo 78/78, always on the same PKT connection index as
        // the SMSG_SUSPEND_COMMS it answers.
        class SuspendCommsAck final : public ClientPacket
        {
        public:
            explicit SuspendCommsAck(WorldPacket&& packet) : ClientPacket(CMSG_SUSPEND_COMMS_ACK, std::move(packet)) { }

            void Read() override;

            uint32 SerialNumber = 0;     ///< echoed from SMSG_SUSPEND_COMMS
            // Client clock in milliseconds, taken by 0x354ED50 at the moment the ack is built. It is NOT unix time
            // and NOT a server value: over the corpus, all 78 pairs spread across 24 recordings,
            // (ClientTick - PKT tick) takes at most two adjacent values within any one recording (spread 0 or
            // 1 ms, i.e. rounding) and a different value in every recording, in the 1.7 h .. 7.7 d range of a
            // process uptime. Usable as a clock-delta sample, never as a value to validate against server time.
            uint32 ClientTick = 0;
        };

        // SMSG_DROP_NEW_CONNECTION (12.1 0x4C0007) - empty.
        // Measured: consumer 0x18C1500 never touches msg+0x20. It resolves its target slot from the RECEIVING
        // socket, not from the payload: it searches NetClient+0x1A0[0..3] for the socket the message arrived on,
        // returns if it is not among the four, and otherwise tears down the PENDING socket at idx|2 - delivering
        // CMSG_LOG_DISCONNECT(11) to it, releasing its compression context and clearing the slot. If that pending
        // slot is empty it does nothing. So the packet is the counterpart of SMSG_CONNECT_TO, it must be sent on
        // the ESTABLISHED socket, and it can only ever clear the pending slot BELONGING TO that socket's index:
        // over the realm socket (index 0) only slot 2, never slot 3, which is where the instance handover pends.
        // NO SENDER. This class exists so the opcode is registered and typed; the one situation where a server
        // would want it - clearing a stale pending socket before retrying an instance handover - is unreachable
        // from the realm socket for exactly the reason above, and inventing another caller would be guessing.
        // See WorldSocket::HandleConnectToFailed for the full argument.
        // UNVERIFIED: zero length is what the consumer reads, not what retail puts on the wire. The opcode occurs
        // 0 times over the corpus - counted as 0x4C0007 in the 12.1 window and as 0x490007 in the 12.0.7 window,
        // never as one number over both, because in 12.1 numbering 0x490007 is SMSG_QUERY_GAME_OBJECT_RESPONSE
        // and counting it there would count that instead. So there are no reference bytes. Per
        // DEFINITION_OF_DONE_pro_opcode.md section 2 that absence says nothing about the opcode and everything
        // about our recording sessions, so it is not evidence for the structure either way - the structure rests
        // on the consumer above. For a server that produces the packet, empty is the only defensible choice.
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
        //
        // NO SENDER, and unlike the two classes above this one has a TRAP attached, so read this before using it.
        // The packet that actually goes out is an opcode substitution inside WorldSocket::WritePacketToBuffer,
        // where it is inseparable from the WorldSocket::ResetCompressionContext() call one line above it - the
        // deflateReset the client's inflateReset has to be matched by. This class does NOT carry that coupling. A
        // send through WorldSession::SendPacket would tell the client to throw away its inflate window while this
        // server keeps deflating into the old stream, and the client aborts the connection with
        // CMSG_LOG_DISCONNECT(17) on the next compressed frame. That failure looks like a compression bug
        // anywhere else, so: do not send this class. The two halves belong together and only WorldSocket can hold
        // them together, because the deflate stream is a WorldSocket member and ResetCompressionContext() is
        // private to it - a new need for this opcode is a new emit site inside WorldSocket next to the existing
        // one, not a SendPacket call from outside.
        // The class exists so the opcode is registered, typed and greppable, the same reason
        // WorldPackets::Auth::DropNewConnection does.
        // UNVERIFIED: as with SMSG_DROP_NEW_CONNECTION, zero length is measured on the read side only - the opcode
        // occurs 0 times over the corpus, counted per build window (0x4C000A in 12.1, 0x49000A in 12.0.7; in 12.1
        // numbering 0x49000A is SMSG_INVALIDATE_PAGE_TEXT, so one number over both windows would count that).
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
            // Named `server` in the descriptor. A packed triple, not an opcode: only 27 distinct values over the
            // corpus's 31 650 entries - 13 of them in the 12.1 window alone (e.g. 0x03050056, 0x01040054) - and
            // the low 24 bits match no Opcodes.h entry of either build.
            // UNVERIFIED: meaning of the packed subfields.
            uint32 Server = 0;
            // UNVERIFIED: no name in the reflection descriptor. Observed range over the corpus 0 .. 380 053
            // (0 .. 77 035 in the 12.1 window alone).
            uint32 Unknown4 = 0;
            // UNVERIFIED: no name in the reflection descriptor. Over the corpus the values are exactly
            // {0, 12, 33} - 9043 zeroes, 13 564 twelves, 9043 thirty-threes of the 31 650 entries - and 33 always
            // comes with Server == 0 && Unknown4 == 0 and marks the entry the sender appends itself (0x20E6F0).
            // This field is what makes the cumulative BLOCK structure of the list checkable, because its sequence
            // is fully determined by Kind, with no exception over the corpus's 4522 packets:
            //   Kind 0: (0, 12, 33)                                             1508 of 1508 packets
            //   Kind 1: (0, 12, 33, 12, 0, 12, 33)                              1507 of 1507
            //   Kind 2: (0, 12, 33, 12, 0, 12, 33, 12, 0, 12, 33)               1507 of 1507
            // Read as blocks that is [0, 12, 33] + [12, 0, 12, 33] + [12, 0, 12, 33]: one block per stage, each
            // one CLOSED by the 33. So a 33 marks the end of a block and never anything else - it occurs only at
            // index 2, 6 or 10 - and a Kind 2 packet carries three of them, one per stage, not one per packet.
            // The count follows: 1*1508 + 2*1507 + 3*1507 = 9043 blocks and 9043 thirty-threes.
            // Not one of those 9043 entries has Frame == 0; the Frame == 0 entries OPEN the blocks instead, see
            // HandleLatencyReport.
            uint8 Unknown8 = 0;
            // Named `timestampMS`. Unix time in milliseconds - verified against the capture dates and independently
            // against the PKT record's own optData double. Over the corpus: 0 null values in 31 650 entries, and
            // the per-packet maximum is monotonic across consecutive packets in 4498 comparisons with 0 violations.
            // Within a single packet it is NOT ordered - see HandleLatencyReport.
            uint64 TimestampMS = 0;
            // Named `frame`. The client's frame rate. Strongest name proof of this family: the sender reads it from
            // 0x29AF640, and that very function is the implementation behind the Lua API GetFramerate.
            uint32 Frame = 0;
        };

        // CMSG_LATENCY_REPORT (12.1 0x44000F) - writer 0x5D6020, sender 0x20E6F0.
        // Length is 8 + 21*Count, verified over the corpus on all 4522 packets (2860 in the 12.1 window, 1662 in
        // the 12.0.7 window) against both (len - 8) % 21 == 0 and (len - 8) / 21 == Count, with 0 outliers. Those
        // 4522 packets carry 31 650 entries; every entry-level figure in LatencyReportEntry above is that number's
        // denominator.
        // There is no reply and no Lua surface: the client sends this self-timed from C++, the Lua side can neither
        // trigger nor observe it, and the consumer sends nothing back. The server must not answer.
        class LatencyReport final : public ClientPacket
        {
        public:
            // UNVERIFIED: that 16 is really the ceiling. It is inferred from the client's latency ring buffer
            // (NetClient + 0x58*(idx+2) + 0x10), which has exactly 16 slots - not measured against a packet that
            // fills it, because the highest Count actually observed over the corpus's 4522 packets is 11.
            // If the inference is wrong this session's telemetry stops: Array::resize calls OnInvalidArraySize for
            // the larger Count, WorldSession::Update catches the resulting PacketArrayMaxCapacityException and the
            // handler never runs. That failure is NOT silent, and the marker does not sit here because it would
            // hide - the catch logs bbe.what() together with the opcode name on "network" at ERROR
            // (WorldSession.cpp, "... occured while parsing a packet (opcode: {}) ... Skipped packet."), and that
            // what() reads "Attempted to read more array elements from packet <Count> than allowed 16"
            // (PacketArrayMaxCapacityException, PacketUtilities.cpp). The line therefore names this constant, the
            // Count that exceeded it and CMSG_LATENCY_REPORT. worldserver.conf.dist declares no Logger.network of
            // its own, so the line falls to Logger.root=5 and is written in the shipped configuration.
            // The marker sits here because 16 is inferred and never measured, not because a wrong 16 would be quiet.
            static constexpr std::size_t MaxEntries = 16;

            explicit LatencyReport(WorldPacket&& packet) : ClientPacket(CMSG_LATENCY_REPORT, std::move(packet)) { }

            void Read() override;

            // Stage index, NOT a report type and NOT a discriminator for different entry formats - the format is
            // identical for all three. Measured coupling over the corpus's 4522 packets without a single
            // exception: Kind 0 -> Count 3 (1508 packets), Kind 1 -> Count 7 (1507), Kind 2 -> Count 11 (1507),
            // arriving as a triple roughly 200 ms apart, and the body of each packet begins byte-identically with
            // the complete body of its predecessor - checked on all 3014 consecutive Kind k -> Kind k+1 pairs,
            // 0 violations.
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
        // UNVERIFIED: the structure is unproven at the wire. The opcode occurs 0 times over the corpus, counted
        // per build window (0x44000B in 12.1, 0x41000B in 12.0.7 - in 12.1 numbering 0x41000B is
        // CMSG_MOVE_START_PITCH_UP, so one number over both windows would count that instead). So there are no
        // reference bytes and no round trip was possible; per DEFINITION_OF_DONE_pro_opcode.md section 2 the
        // absence is a statement about our recording sessions and not about the opcode. What the structure was
        // checked against is the client writer above plus the length rule it implies: 2..513 byte body, the 9 bit
        // length field padded out to two bytes by FlushBits plus 0..511 message bytes.
        class LogStreamingError final : public ClientPacket
        {
        public:
            // The client's message buffer is char[512], so the length field is ceil(log2(512)) = 9 bits wide and
            // the longest message that fits is 511 bytes. The two are one fact, so MaxMessageLength is derived
            // from the bit width rather than written out next to it - Read() uses MessageLengthBits, which makes
            // the declared bound the bound that is actually enforced instead of a second, independent claim.
            static constexpr uint32 MessageLengthBits = 9;
            static constexpr std::size_t MaxMessageLength = (std::size_t(1) << MessageLengthBits) - 1;
            static_assert(MaxMessageLength == 511, "9 bit length field, 512 byte client buffer");

            explicit LogStreamingError(WorldPacket&& packet) : ClientPacket(CMSG_LOG_STREAMING_ERROR, std::move(packet)) { }

            void Read() override;

            std::string Message;
        };

        ByteBuffer& operator<<(ByteBuffer& data, VirtualRealmInfo const& realmInfo);
        ByteBuffer& operator<<(ByteBuffer& data, VirtualRealmNameInfo const& realmInfo);
        ByteBuffer& operator>>(ByteBuffer& data, LatencyReportEntry& entry);
    }
}

#endif // TRINITYCORE_AUTHENTICATION_PACKETS_H

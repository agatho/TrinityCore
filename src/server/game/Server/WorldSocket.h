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

#ifndef TRINITYCORE_WORLD_SOCKET_H
#define TRINITYCORE_WORLD_SOCKET_H

#include "AsyncCallbackProcessor.h"
#include "AuthDefines.h"
#include "DatabaseEnvFwd.h"
#include "MPSCQueue.h"
#include "MessageBuffer.h"
#include "Socket.h"
#include "WorldPacket.h"
#include "WorldPacketCrypt.h"
#include <array>
#include <mutex>
#include <span>
#include <vector>

namespace JSON::RealmList
{
class RealmJoinTicket;
}

typedef struct z_stream_s z_stream;
class EncryptablePacket;
class WorldPacket;
class WorldSession;
enum ConnectionType : int8;
enum OpcodeClient : uint32;

class EncryptablePacket : public WorldPacket
{
public:
    EncryptablePacket(WorldPacket const& packet, bool encrypt) : WorldPacket(packet), _encrypt(encrypt)
    {
        SocketQueueLink.store(nullptr, std::memory_order_relaxed);
    }

    bool NeedsEncryption() const { return _encrypt; }

    std::atomic<EncryptablePacket*> SocketQueueLink;

private:
    bool _encrypt;
};

namespace WorldPackets::Auth
{
    class AuthSession;
    class AuthContinuedSession;
}

#pragma pack(push, 1)

struct PacketHeader
{
    uint32 Size;
    uint8 Tag[12];

    bool IsValidSize() const { return Size < 0x10000; }
};

struct IncomingPacketHeader : PacketHeader
{
    uint32 EncryptedOpcode;
};

#pragma pack(pop)

class TC_GAME_API WorldSocket final : public Trinity::Net::Socket<>
{
    static uint32 const MinSizeForCompression;

    // Limits for SMSG_MULTIPLE_PACKETS bundles. Neither is a client limit - the client's framing loop
    // (0x18C0490) stops when fewer than 4 bytes remain and its only hard bound is the uint16 inner length.
    // These keep one frame from growing without bound; bundling is only worthwhile for small packets anyway.
    // UNVERIFIED: the two values themselves. 0x2000 bytes and 64 entries are CHOSEN, not measured - no client
    // limit constrains them (see above) and no retail bundle is available to compare against, because the
    // sniffer hooks behind the transport decapsulation and never records one. A measurement needs a retail
    // capture taken below that layer.
    static uint32 const MaxBundlePayloadSize;
    static uint32 const MaxBundleEntries;

    static std::array<uint8, 32> const AuthCheckSeed;
    static std::array<uint8, 32> const SessionKeySeed;
    static std::array<uint8, 32> const ContinuedSessionSeed;
    static std::array<uint8, 32> const EncryptionKeySeed;

    using BaseSocket = Socket;

public:
    explicit WorldSocket(Trinity::Net::IoContextTcpSocket&& socket);
    ~WorldSocket();

    WorldSocket(WorldSocket const& right) = delete;
    WorldSocket(WorldSocket&& right) = delete;
    WorldSocket& operator=(WorldSocket const& right) = delete;
    WorldSocket& operator=(WorldSocket&& right) = delete;

    void Start() override;
    bool Update() override;

    void SendPacket(WorldPacket const& packet);

    ConnectionType GetConnectionType() const { return _type; }

    void SendAuthResponseError(uint32 code);
    void SetWorldSession(WorldSession* session);
    void SetSendBufferSize(std::size_t sendBufferSize) { _sendBufferSize = sendBufferSize; }

    void OnClose() override;
    Trinity::Net::SocketReadCallbackResult ReadHandler() override;

    void QueueQuery(QueryCallback&& queryCallback);

    void SendAuthSession();
    bool InitializeCompression();

protected:
    bool ReadHeaderHandler();

    enum class ReadDataHandlerResult
    {
        Ok = 0,
        Error = 1,
        WaitingForQuery = 2
    };

    ReadDataHandlerResult ReadDataHandler();
private:
    /// writes network.opcode log
    void LogOpcodeText(OpcodeClient opcode) const;
    void LogOpcodeText(OpcodeClient opcode, std::scoped_lock<std::mutex> const& guard) const;
    /// sends and logs network.opcode without accessing WorldSession
    void SendPacketAndLogOpcode(WorldPacket const& packet);
    void WritePacketToBuffer(EncryptablePacket const& packet, MessageBuffer& buffer);
    void WriteBundleToBuffer(std::span<EncryptablePacket* const> packets, MessageBuffer& buffer);
    bool CanBundle(EncryptablePacket const& packet) const;
    uint32 CompressPacket(uint8* buffer, WorldPacket const& packet);
    void ResetCompressionContext();

    ReadDataHandlerResult HandleAuthSession(WorldPacket&& packet);
    void HandleAuthSessionCallback(WorldPackets::Auth::AuthSession const* authSession, JSON::RealmList::RealmJoinTicket* joinTicket, PreparedResultSet const* result);
    ReadDataHandlerResult HandleAuthContinuedSession(WorldPacket&& packet);
    void HandleAuthContinuedSessionCallback(WorldPackets::Auth::AuthContinuedSession const* authSession, PreparedResultSet const* result);
    void LoadSessionPermissionsCallback(PreparedQueryResult result);
    ReadDataHandlerResult HandleKeepAlive();
    ReadDataHandlerResult HandleLogDisconnect(WorldPacket&& packet) const;
    ReadDataHandlerResult HandleConnectToFailed(WorldPacket&& packet);
    ReadDataHandlerResult HandlePing(WorldPacket&& packet);
    ReadDataHandlerResult HandleEnterEncryptedModeAck();

    ConnectionType _type;
    uint64 _key;

    std::array<uint8, 32> _serverChallenge;
    WorldPacketCrypt _authCrypt;
    SessionKey _sessionKey;
    std::array<uint8, 32> _encryptKey;

    TimePoint _lastPingTime;
    uint32 _overSpeedPings;

    std::mutex _worldSessionLock;
    WorldSession* _worldSession;
    bool _authed;
    bool _canRequestHotfixes;
    // Mirror of the client's per socket suspend flag, the second of the three receive gates every packet - and,
    // through the recursion in 0x18C0490, every BUNDLED packet - has to pass. True from the moment this socket is
    // known to be the instance connection, because the client's NetClient constructor already marks slot 1 as
    // suspended; cleared by SMSG_RESUME_COMMS and set again by SMSG_SUSPEND_COMMS, tracked in send order in
    // Update(). Read by CanBundle only - the single packet path does not need it, since a lone packet carries its
    // own opcode into the gate and the gate is the client's business, not ours.
    bool _clientSuspended;

    MessageBuffer _headerBuffer;
    MessageBuffer _packetBuffer;
    MPSCQueue<EncryptablePacket, &EncryptablePacket::SocketQueueLink> _bufferQueue;
    std::size_t _sendBufferSize;

    z_stream* _compressionStream;

    QueryCallbackProcessor _queryProcessor;
    std::string _ipCountry;
};

#endif

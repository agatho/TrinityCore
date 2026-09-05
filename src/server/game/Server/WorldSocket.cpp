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

#include "WorldSocket.h"
#include "AuthenticationPackets.h"
#include "BattlenetRpcErrorCodes.h"
#include "CharacterPackets.h"
#include "Config.h"
#include "CryptoHash.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "Errors.h"
#include "GameTime.h"
#include "HMAC.h"
#include "IPLocation.h"
#include "IpBanCheckConnectionInitializer.h"
#include "PacketLog.h"
#include "ProtobufJSON.h"
#include "QueryResultStructured.h"
#include "RealmList.h"
#include "RBAC.h"
#include "RealmList.pb.h"
#include "ScriptMgr.h"
#include "SessionKeyGenerator.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSocketMgr.h"
#include <algorithm>
#include <zlib.h>

#pragma pack(push, 1)

struct CompressedWorldPacket
{
    uint32 UncompressedSize;
    uint32 UncompressedAdler;
    uint32 CompressedAdler;
};

#pragma pack(pop)

uint32 const WorldSocket::MinSizeForCompression = 0x400;
uint32 const WorldSocket::MaxBundlePayloadSize = 0x2000;
uint32 const WorldSocket::MaxBundleEntries = 64;

std::array<uint8, 32> const WorldSocket::AuthCheckSeed = { 0xDE, 0x3A, 0x2A, 0x8E, 0x6B, 0x89, 0x52, 0x66, 0x88, 0x9D, 0x7E, 0x7A, 0x77, 0x1D, 0x5D, 0x1F,
    0x4E, 0xD9, 0x0C, 0x23, 0x9B, 0xCD, 0x0E, 0xDC, 0xD2, 0xE8, 0x04, 0x3A, 0x68, 0x64, 0xC7, 0xB0 };
std::array<uint8, 32> const WorldSocket::SessionKeySeed = { 0xE8, 0x1E, 0x8B, 0x59, 0x27, 0x62, 0x1E, 0xAA, 0x86, 0x15, 0x18, 0xEA, 0xC0, 0xBF, 0x66, 0x8C,
    0x6D, 0xBF, 0x83, 0x93, 0xBC, 0xAA, 0x80, 0x52, 0x5B, 0x1E, 0xDC, 0x23, 0xA0, 0x12, 0xB7, 0x50 };
std::array<uint8, 32> const WorldSocket::ContinuedSessionSeed = { 0x56, 0x5C, 0x61, 0x9C, 0x48, 0x3A, 0x52, 0x1F, 0x61, 0x5D, 0x05, 0x49, 0xB2, 0x9A, 0x39, 0xBF,
    0x4B, 0x97, 0xB0, 0x1B, 0xF9, 0x6C, 0xDE, 0xD6, 0x80, 0x1D, 0xAB, 0x26, 0x02, 0xA9, 0x9B, 0x9D };
std::array<uint8, 32> const WorldSocket::EncryptionKeySeed = { 0x71, 0xC9, 0xED, 0x5A, 0xA7, 0x0E, 0x4D, 0xFF, 0x4C, 0x36, 0xA6, 0x5A, 0x3E, 0x46, 0x8A, 0x4A,
    0x5D, 0xA1, 0x48, 0xC8, 0x30, 0x47, 0x4A, 0xDE, 0xF6, 0x0D, 0x6C, 0xBE, 0x6F, 0xE4, 0x55, 0x73 };

WorldSocket::WorldSocket(Trinity::Net::IoContextTcpSocket&& socket) : BaseSocket(std::move(socket)),
    _type(CONNECTION_TYPE_REALM), _key(0), _serverChallenge(), _sessionKey(), _encryptKey(), _overSpeedPings(0),
    _worldSession(nullptr), _authed(false), _canRequestHotfixes(true), _clientSuspended(false),
    _headerBuffer(sizeof(IncomingPacketHeader)), _sendBufferSize(4096), _compressionStream(nullptr)
{
}

WorldSocket::~WorldSocket()
{
    if (_compressionStream)
    {
        deflateEnd(_compressionStream);
        delete _compressionStream;
    }
}

struct WorldSocketProtocolInitializer final : Trinity::Net::SocketConnectionInitializer
{
    static constexpr std::string_view ServerConnectionInitialize = "WORLD OF WARCRAFT CONNECTION - SERVER TO CLIENT - V2\n";
    static constexpr std::string_view ClientConnectionInitialize = "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER - V2\n";

    explicit WorldSocketProtocolInitializer(WorldSocket* socket) : _socket(socket) { }

    void Start() override
    {
        _packetBuffer.Resize(ClientConnectionInitialize.length());

        AsyncRead();

        MessageBuffer initializer;
        initializer.Write(ServerConnectionInitialize.data(), ServerConnectionInitialize.length());

        // - IoContext.run thread, safe.
        _socket->QueuePacket(std::move(initializer));
    }

    void AsyncRead()
    {
        _socket->AsyncRead(
            [socketRef = _socket->weak_from_this(), self = static_pointer_cast<WorldSocketProtocolInitializer>(this->shared_from_this())]
            {
                if (!socketRef.expired())
                    return self->ReadHandler();

                return Trinity::Net::SocketReadCallbackResult::Stop;
            });
    }

    Trinity::Net::SocketReadCallbackResult ReadHandler();

    void HandleDataReady();

private:
    WorldSocket* _socket;
    MessageBuffer _packetBuffer;
};

void WorldSocket::Start()
{
    // build initializer chain
    std::array<std::shared_ptr<Trinity::Net::SocketConnectionInitializer>, 3> initializers =
    { {
        std::make_shared<Trinity::Net::IpBanCheckConnectionInitializer<WorldSocket>>(this),
        std::make_shared<WorldSocketProtocolInitializer>(this),
        std::make_shared<Trinity::Net::ReadConnectionInitializer<WorldSocket>>(this),
    } };

    Trinity::Net::SocketConnectionInitializer::SetupChain(initializers)->Start();
}

Trinity::Net::SocketReadCallbackResult WorldSocketProtocolInitializer::ReadHandler()
{
    MessageBuffer& packet = _socket->GetReadBuffer();
    if (packet.GetActiveSize() > 0 && _packetBuffer.GetRemainingSpace() > 0)
    {
        // need to receive the header
        std::size_t readHeaderSize = std::min(packet.GetActiveSize(), _packetBuffer.GetRemainingSpace());
        _packetBuffer.Write(packet.GetReadPointer(), readHeaderSize);
        packet.ReadCompleted(readHeaderSize);

        if (_packetBuffer.GetRemainingSpace() == 0)
        {
            HandleDataReady();
            return Trinity::Net::SocketReadCallbackResult::Stop;
        }

        // Couldn't receive the whole header this time.
        ASSERT(packet.GetActiveSize() == 0);
    }

    return Trinity::Net::SocketReadCallbackResult::KeepReading;
}

void WorldSocketProtocolInitializer::HandleDataReady()
{
    try
    {
        ByteBuffer buffer(std::move(_packetBuffer).Release());
        if (buffer.ReadString(ClientConnectionInitialize.length()) != ClientConnectionInitialize)
        {
            _socket->CloseSocket();
            return;
        }
    }
    catch (ByteBufferException const& ex)
    {
        TC_LOG_ERROR("network", "WorldSocket::InitializeHandler ByteBufferException {} occured while parsing initial packet from {}",
            ex.what(), _socket->GetRemoteIpAddress());
        _socket->CloseSocket();
        return;
    }

    if (!_socket->InitializeCompression())
        return;

    _socket->SendAuthSession();
    InvokeNext();
}

bool WorldSocket::InitializeCompression()
{
    _compressionStream = new z_stream();
    _compressionStream->zalloc = (alloc_func)nullptr;
    _compressionStream->zfree = (free_func)nullptr;
    _compressionStream->opaque = (voidpf)nullptr;
    _compressionStream->avail_in = 0;
    _compressionStream->next_in = nullptr;
    int32 z_res = deflateInit2(_compressionStream, sWorld->getIntConfig(CONFIG_COMPRESSION), Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (z_res != Z_OK)
    {
        CloseSocket();
        TC_LOG_ERROR("network", "Can't initialize packet compression (zlib: deflateInit) Error code: {} ({})", z_res, zError(z_res));
        return false;
    }

    return true;
}

bool WorldSocket::Update()
{
    EncryptablePacket* queued;
    if (_bufferQueue.Dequeue(queued))
    {
        bool const bundling = sWorldSocketMgr.IsPacketBundlingEnabled();

        // Allocate buffer only when it's needed but not on every Update() call.
        MessageBuffer buffer(_sendBufferSize);

        // Run of consecutive bundleable packets waiting to become one SMSG_MULTIPLE_PACKETS frame. Ownership of
        // everything in here belongs to this vector until FlushBundle deletes it.
        std::vector<EncryptablePacket*> bundle;
        std::size_t bundlePayloadSize = 0;

        auto writeSinglePacket = [&](EncryptablePacket* packet)
        {
            uint32 packetSize = packet->size() + 4 /*opcode*/;
            if (packetSize > MinSizeForCompression && packet->NeedsEncryption())
                packetSize = deflateBound(_compressionStream, packetSize) + sizeof(CompressedWorldPacket);

            // Flush current buffer if too small for next packet
            if (buffer.GetRemainingSpace() < packetSize + sizeof(PacketHeader))
            {
                QueuePacket(std::move(buffer));
                buffer.Resize(_sendBufferSize);
            }

            if (buffer.GetRemainingSpace() >= packetSize + sizeof(PacketHeader))
                WritePacketToBuffer(*packet, buffer);
            else    // single packet larger than _sendBufferSize
            {
                MessageBuffer packetBuffer(packetSize + sizeof(PacketHeader));
                WritePacketToBuffer(*packet, packetBuffer);
                QueuePacket(std::move(packetBuffer));
            }
        };

        auto flushBundle = [&]()
        {
            if (bundle.empty())
                return;

            // A bundle of one saves nothing and costs 6 bytes: the outer 16 byte PacketHeader plus 4 byte outer
            // opcode merely replace the ones the packet would have carried itself, while the 2 byte inner length
            // and the repeated 4 byte inner opcode are pure addition (n * 14 - 20 is -6 at n == 1). It would also
            // add a layer the client has to recurse through for nothing. Send it plainly.
            if (bundle.size() == 1)
                writeSinglePacket(bundle.front());
            else
            {
                std::size_t frameSize = sizeof(uint32) /*outer opcode*/ + bundlePayloadSize + sizeof(PacketHeader);
                if (buffer.GetRemainingSpace() < frameSize)
                {
                    QueuePacket(std::move(buffer));
                    buffer.Resize(std::max<std::size_t>(_sendBufferSize, frameSize));
                }

                WriteBundleToBuffer(bundle, buffer);
            }

            for (EncryptablePacket* bundled : bundle)
                delete bundled;

            bundle.clear();
            bundlePayloadSize = 0;
        };

        do
        {
            // CanBundle has to be asked BEFORE this packet is allowed to move the suspend flag, and the order is
            // load bearing in both directions. SMSG_RESUME_COMMS is asked while the socket still counts as
            // suspended, so it is refused and travels as a single packet - which is what makes it provably reach
            // the client ahead of everything its own arrival unblocks. SMSG_SUSPEND_COMMS is asked while the
            // socket still counts as open, which is correct for the same reason: at the moment it is written the
            // client has not yet closed the gate.
            bool const bundleThisPacket = bundling && CanBundle(*queued);
            switch (queued->GetOpcode())
            {
                case SMSG_SUSPEND_COMMS: _clientSuspended = true;  break;
                case SMSG_RESUME_COMMS:  _clientSuspended = false; break;
                default:                                           break;
            }

            if (bundleThisPacket)
            {
                std::size_t entrySize = sizeof(uint16) /*inner length*/ + sizeof(uint32) /*inner opcode*/ + queued->size();
                if (bundle.size() >= MaxBundleEntries || bundlePayloadSize + entrySize > MaxBundlePayloadSize)
                    flushBundle();

                bundle.push_back(queued);
                bundlePayloadSize += entrySize;
                continue;   // ownership is with `bundle` now
            }

            // Anything that cannot be bundled terminates the run, so the order packets were queued in is the order
            // they reach the client in.
            flushBundle();
            writeSinglePacket(queued);

            delete queued;
        } while (_bufferQueue.Dequeue(queued));

        flushBundle();

        if (buffer.GetActiveSize() > 0)
            QueuePacket(std::move(buffer));
    }

    if (!BaseSocket::Update())
        return false;

    _queryProcessor.ProcessReadyCallbacks();

    return true;
}

void WorldSocket::SendAuthSession()
{
    Trinity::Crypto::GetRandomBytes(_serverChallenge);

    WorldPackets::Auth::AuthChallenge challenge;
    challenge.Challenge = _serverChallenge;
    memcpy(challenge.DosChallenge.data(), Trinity::Crypto::GetRandomBytes<32>().data(), 32);
    challenge.DosZeroBits = 1;

    SendPacketAndLogOpcode(*challenge.Write());
}

void WorldSocket::OnClose()
{
    {
        std::scoped_lock sessionGuard(_worldSessionLock);
        _worldSession = nullptr;
    }
}

Trinity::Net::SocketReadCallbackResult WorldSocket::ReadHandler()
{
    MessageBuffer& packet = GetReadBuffer();
    while (packet.GetActiveSize() > 0)
    {
        if (_headerBuffer.GetRemainingSpace() > 0)
        {
            // need to receive the header
            std::size_t readHeaderSize = std::min(packet.GetActiveSize(), _headerBuffer.GetRemainingSpace());
            _headerBuffer.Write(packet.GetReadPointer(), readHeaderSize);
            packet.ReadCompleted(readHeaderSize);

            if (_headerBuffer.GetRemainingSpace() > 0)
            {
                // Couldn't receive the whole header this time.
                ASSERT(packet.GetActiveSize() == 0);
                break;
            }

            // We just received nice new header
            if (!ReadHeaderHandler())
            {
                CloseSocket();
                return Trinity::Net::SocketReadCallbackResult::Stop;
            }
        }

        // We have full read header, now check the data payload
        if (_packetBuffer.GetRemainingSpace() > 0)
        {
            // need more data in the payload
            std::size_t readDataSize = std::min(packet.GetActiveSize(), _packetBuffer.GetRemainingSpace());
            _packetBuffer.Write(packet.GetReadPointer(), readDataSize);
            packet.ReadCompleted(readDataSize);

            if (_packetBuffer.GetRemainingSpace() > 0)
            {
                // Couldn't receive the whole data this time.
                ASSERT(packet.GetActiveSize() == 0);
                break;
            }
        }

        // just received fresh new payload
        ReadDataHandlerResult result = ReadDataHandler();
        _headerBuffer.Reset();
        if (result != ReadDataHandlerResult::Ok)
        {
            if (result != ReadDataHandlerResult::WaitingForQuery)
                CloseSocket();

            return Trinity::Net::SocketReadCallbackResult::Stop;
        }
    }

    return Trinity::Net::SocketReadCallbackResult::KeepReading;
}

void WorldSocket::QueueQuery(QueryCallback&& queryCallback)
{
    _queryProcessor.AddCallback(std::move(queryCallback));
}

void WorldSocket::SetWorldSession(WorldSession* session)
{
    std::scoped_lock sessionGuard(_worldSessionLock);
    _worldSession = session;
    _authed = true;
}

bool WorldSocket::ReadHeaderHandler()
{
    ASSERT(_headerBuffer.GetActiveSize() == sizeof(IncomingPacketHeader), "Header size " SZFMTD " different than expected " SZFMTD, _headerBuffer.GetActiveSize(), sizeof(IncomingPacketHeader));

    IncomingPacketHeader* header = reinterpret_cast<IncomingPacketHeader*>(_headerBuffer.GetReadPointer());
    uint32 encryptedOpcode = header->EncryptedOpcode;

    if (!header->IsValidSize())
    {
        _authCrypt.PeekDecryptRecv(reinterpret_cast<uint8*>(&header->EncryptedOpcode), sizeof(encryptedOpcode));

        // CMSG_HOTFIX_REQUEST can be much larger than normal packets, allow receiving it once per session
        if (header->EncryptedOpcode != CMSG_HOTFIX_REQUEST || header->Size > 0x100000 || !_canRequestHotfixes)
        {
            TC_LOG_ERROR("network", "WorldSocket::ReadHeaderHandler(): client {} sent malformed packet (size: {}, opcode {})",
                GetRemoteIpAddress(), header->Size, uint32(header->EncryptedOpcode));
            return false;
        }
    }

    _packetBuffer.Resize(header->Size);
    _packetBuffer.Write(&encryptedOpcode, sizeof(encryptedOpcode));
    return true;
}

WorldSocket::ReadDataHandlerResult WorldSocket::ReadDataHandler()
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(_headerBuffer.GetReadPointer());

    if (!_authCrypt.DecryptRecv(_packetBuffer.GetReadPointer(), header->Size, header->Tag))
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadHeaderHandler(): client {} failed to decrypt packet (size: {})",
            GetRemoteIpAddress(), header->Size);
        return ReadDataHandlerResult::Error;
    }

    WorldPacket packet(std::move(_packetBuffer).Release(), GetConnectionType());
    OpcodeClient opcode = packet.read<OpcodeClient>();
    if (!opcodeTable.IsValid(opcode))
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadHeaderHandler(): client {} sent wrong opcode (opcode: {})",
            GetRemoteIpAddress(), uint32(opcode));
        return ReadDataHandlerResult::Error;
    }

    packet.SetOpcode(opcode);

    if (sPacketLog->CanLogPacket())
        sPacketLog->LogPacket(packet, CLIENT_TO_SERVER, GetRemoteIpAddress(), GetRemotePort(), GetConnectionType());

    switch (opcode)
    {
        case CMSG_PING:
            return HandlePing(std::move(packet));
        case CMSG_AUTH_SESSION:
            return HandleAuthSession(std::move(packet));
        case CMSG_AUTH_CONTINUED_SESSION:
            return HandleAuthContinuedSession(std::move(packet));
        case CMSG_KEEP_ALIVE:
            return HandleKeepAlive();
        case CMSG_LOG_DISCONNECT:
            return HandleLogDisconnect(std::move(packet));
        case CMSG_ENABLE_NAGLE:
            LogOpcodeText(CMSG_ENABLE_NAGLE);
            SetNoDelay(false);
            break;
        case CMSG_CONNECT_TO_FAILED:
            return HandleConnectToFailed(std::move(packet));
        case CMSG_ENTER_ENCRYPTED_MODE_ACK:
            return HandleEnterEncryptedModeAck();
        default:
        {
            // This is the only place the receive time is ever stamped, so the list must name every opcode whose
            // handler reads WorldPacket::GetReceivedTime - today exactly the callers of WorldSession::HandleTimeSync.
            // An opcode missing here arrives with a default constructed TimePoint, and getMSTimeDiff then wraps
            // instead of failing visibly, poisoning _timeSyncClockDeltaQueue with a delta off by ~2.1e9 ms.
            if (opcode == CMSG_TIME_SYNC_RESPONSE || opcode == CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE || opcode == CMSG_QUEUED_MESSAGES_END
                || opcode == CMSG_SUSPEND_COMMS_ACK)
                packet.SetReceiveTime(std::chrono::steady_clock::now());
            else if (opcode == CMSG_HOTFIX_REQUEST)
                _canRequestHotfixes = false;

            std::scoped_lock sessionGuard(_worldSessionLock);

            LogOpcodeText(opcode, sessionGuard);

            if (!_worldSession)
            {
                TC_LOG_ERROR("network.opcode", "WorldSocket::ReadDataHandler: Client not authed opcode {}", GetOpcodeNameForLogging(opcode));
                return ReadDataHandlerResult::Error;
            }

            if (!opcodeTable[opcode])
            {
                TC_LOG_ERROR("network.opcode", "WorldSocket::ReadDataHandler: No defined handler for opcode {} sent by {}", GetOpcodeNameForLogging(opcode), _worldSession->GetPlayerInfo());
                break;
            }

            // Our Idle timer will reset on any non PING opcodes on login screen, allowing us to catch people idling.
            _worldSession->ResetTimeOutTime(false);

            _worldSession->QueuePacket(std::move(packet));
            break;
        }
    }

    return ReadDataHandlerResult::Ok;
}

void WorldSocket::LogOpcodeText(OpcodeClient opcode) const
{
    TC_LOG_TRACE("network.opcode", "C->S: {} {}", GetRemoteIpAddress(), GetOpcodeNameForLogging(opcode));
}

void WorldSocket::LogOpcodeText(OpcodeClient opcode, std::scoped_lock<std::mutex> const& /*guard*/) const
{
    if (!_worldSession)
    {
        TC_LOG_TRACE("network.opcode", "C->S: {} {}", GetRemoteIpAddress(), GetOpcodeNameForLogging(opcode));
    }
    else
    {
        TC_LOG_TRACE("network.opcode", "C->S: {} {}", _worldSession->GetPlayerInfo(), GetOpcodeNameForLogging(opcode));
    }
}

void WorldSocket::SendPacketAndLogOpcode(WorldPacket const& packet)
{
    TC_LOG_TRACE("network.opcode", "S->C: {} {}", GetRemoteIpAddress(), GetOpcodeNameForLogging(static_cast<OpcodeServer>(packet.GetOpcode())));
    SendPacket(packet);
}

void WorldSocket::SendPacket(WorldPacket const& packet)
{
    if (!IsOpen())
        return;

    if (sPacketLog->CanLogPacket())
        sPacketLog->LogPacket(packet, SERVER_TO_CLIENT, GetRemoteIpAddress(), GetRemotePort(), GetConnectionType());

    _bufferQueue.Enqueue(new EncryptablePacket(packet, _authCrypt.IsInitialized()));
}

void WorldSocket::WritePacketToBuffer(EncryptablePacket const& packet, MessageBuffer& buffer)
{
    uint32 opcode = packet.GetOpcode();
    uint32 packetSize = packet.size();

    // Reserve space for buffer
    uint8* headerPos = buffer.GetWritePointer();
    buffer.WriteCompleted(sizeof(PacketHeader));
    uint8* dataPos = buffer.GetWritePointer();
    buffer.WriteCompleted(sizeof(opcode));

    if (packetSize > MinSizeForCompression && packet.NeedsEncryption())
    {
        CompressedWorldPacket cmp;
        cmp.UncompressedSize = packetSize + sizeof(opcode);
        cmp.UncompressedAdler = adler32(adler32(0x9827D8F1, (Bytef*)&opcode, sizeof(opcode)), packet.data(), packetSize);

        // Space for compression info - uncompressed size and checksums. Deliberately NOT committed with
        // WriteCompleted before the deflate result is known: on failure this frame turns into
        // SMSG_RESET_COMPRESSION_CONTEXT instead, and a committed write pointer could not be taken back.
        uint8* compressionInfo = buffer.GetWritePointer();
        uint8* compressedData = compressionInfo + sizeof(CompressedWorldPacket);

        uint32 compressedSize = CompressPacket(compressedData, packet);
        if (!compressedSize)
        {
            // deflate failed. Every compressed packet after a failed one belongs to a deflate stream the client can
            // no longer follow, and the client answers that with CMSG_LOG_DISCONNECT(17) plus a closed connection.
            // Recover instead: reset our deflate stream and tell the client to inflateReset its own, by turning
            // this frame into SMSG_RESET_COMPRESSION_CONTEXT (0x4C000A, empty payload - consumer 0x18C0D90 reads
            // nothing and tail-jumps into zlib inflateReset). Both sides then start a fresh deflate stream.
            // The packet whose compression failed is lost; before this recovery existed the whole connection was.
            TC_LOG_ERROR("network", "WorldSocket::WritePacketToBuffer: compression of opcode {} for {} failed, dropping the packet and resetting the compression context of both sides",
                opcode, GetRemoteIpAddress());

            ResetCompressionContext();

            opcode = SMSG_RESET_COMPRESSION_CONTEXT;
            packetSize = 0;
        }
        else
        {
            cmp.CompressedAdler = adler32(0x9827D8F1, compressedData, compressedSize);

            memcpy(compressionInfo, &cmp, sizeof(CompressedWorldPacket));
            buffer.WriteCompleted(sizeof(CompressedWorldPacket));
            buffer.WriteCompleted(compressedSize);
            packetSize = compressedSize + sizeof(CompressedWorldPacket);

            opcode = SMSG_COMPRESSED_PACKET;
        }
    }
    else if (!packet.empty())
        buffer.Write(packet.data(), packet.size());

    memcpy(dataPos, &opcode, sizeof(opcode));
    packetSize += sizeof(opcode);

    PacketHeader header;
    header.Size = packetSize;
    _authCrypt.EncryptSend(dataPos, header.Size, header.Tag);

    memcpy(headerPos, &header, sizeof(PacketHeader));
}

uint32 WorldSocket::CompressPacket(uint8* buffer, WorldPacket const& packet)
{
    uint32 opcode = packet.GetOpcode();
    uint32 bufferSize = deflateBound(_compressionStream, packet.size() + sizeof(opcode));

    _compressionStream->next_out = buffer;
    _compressionStream->avail_out = bufferSize;
    _compressionStream->next_in = (Bytef*)&opcode;
    _compressionStream->avail_in = sizeof(opcode);

    int32 z_res = deflate(_compressionStream, Z_NO_FLUSH);
    if (z_res != Z_OK)
    {
        TC_LOG_ERROR("network", "Can't compress packet opcode (zlib: deflate) Error code: {} ({}, msg: {})", z_res, zError(z_res), _compressionStream->msg);
        return 0;
    }

    _compressionStream->next_in = (Bytef*)packet.data();
    _compressionStream->avail_in = packet.size();

    z_res = deflate(_compressionStream, Z_SYNC_FLUSH);
    if (z_res != Z_OK)
    {
        TC_LOG_ERROR("network", "Can't compress packet data (zlib: deflate) Error code: {} ({}, msg: {})", z_res, zError(z_res), _compressionStream->msg);
        return 0;
    }

    return bufferSize - _compressionStream->avail_out;
}

// SMSG_MULTIPLE_PACKETS (12.1 0x4C000D). Framing read off the client's own framing function 0x18C0490:
//
//     uint32 opcode                    // 0x4C000D
//     while at least 4 bytes remain:
//         uint16 innerBodyLen          // length of the inner packet WITHOUT its 4 opcode bytes
//         bytes[innerBodyLen + 4]      // the complete inner packet, its opcode included
//
// The client recurses into 0x18C0490 for every inner packet, which means every inner packet passes all three
// receive gates again. Three consequences that this code depends on:
//   * the pre-encryption whitelist binds the INNER packets, not the outer frame. Said precisely, because the
//     obvious reading of it is wrong: bitmask 0x2A1F at RVA 0x389DC00 (quoted in full above
//     WorldPackets::Auth::SuspendComms) admits 0x4C0000..0x4C0004, 0x4C0009, 0x4C000B, 0x4C000D and
//     SMSG_AUTH_FAILED - and bit 13 of that mask IS 0x4C000D, so the frame itself would pass the gate before
//     encryption. What would not pass is its contents: every inner packet re-enters the same gate through the
//     recursion, and none of the handful of opcodes this server sends before SMSG_ENTER_ENCRYPTED_MODE that are
//     not themselves on the list would survive it. A bundle may therefore only be built once encryption is
//     active - CanBundle enforces that through NeedsEncryption().
//   * the suspend gate binds the INNER packets as well, and for the same reason. A suspended socket admits family
//     0x4C only (check 0x18C0F60), and every instance socket is suspended from the moment the client creates it -
//     so the enter-world traffic on a fresh instance socket must not be bundled until SMSG_RESUME_COMMS has gone
//     out. CanBundle enforces that through _clientSuspended; the full argument is there.
//   * a wrong innerBodyLen is NOT reported: the loop breaks silently when innerBodyLen + 4 exceeds the remaining
//     bytes and the rest of the frame is discarded without an error. Getting the length arithmetic wrong here
//     loses packets quietly, which is why bundling is a config switch and off by default.
//
// The gain per bundled packet is the 16 bytes of PacketHeader (uint32 Size + uint8 Tag[12], see WorldSocket.h)
// traded for a 2 byte length; the 4 byte opcode is paid either way. That is 14 bytes per packet, against the
// 20 bytes the outer frame costs once, so n bundled packets save n * 14 - 20 bytes - negative for a single
// packet and positive from two on. On top of that comes one WorldPacketCrypt::EncryptSend call for the whole
// frame instead of one per packet. There is no packet class for
// this opcode, exactly as with SMSG_COMPRESSED_PACKET: it is an opcode substitution in the write path and never
// travels through WorldSession::SendPacket. The client has no message class for it either - the factory stub scan
// over .text finds stubs for 0x4C0000..0x4C000C and none for 0x4C000D.
//
// UNVERIFIED: no client has ever parsed a bundle produced by this code, and there are no reference bytes for the
// opcode to round-trip against. The framing above is read out of the client's own framing function, so the field
// order is measured - what is not measured is that a frame this function writes is accepted. Step 2 of the
// verification loop is impossible here (the sniffer hooks behind the transport decapsulation, so a bundle never
// appears in any recording) and step 3 has not run. That is what keeps bundling off by default
// (WorldSocketMgr::StartNetwork) and what the debug line at the end of this function is for.
void WorldSocket::WriteBundleToBuffer(std::span<EncryptablePacket* const> packets, MessageBuffer& buffer)
{
    uint32 opcode = SMSG_MULTIPLE_PACKETS;

    // Reserve space for buffer
    uint8* headerPos = buffer.GetWritePointer();
    buffer.WriteCompleted(sizeof(PacketHeader));
    uint8* dataPos = buffer.GetWritePointer();
    buffer.WriteCompleted(sizeof(opcode));
    memcpy(dataPos, &opcode, sizeof(opcode));

    uint32 packetSize = sizeof(opcode);

    for (EncryptablePacket* packet : packets)
    {
        uint16 innerBodyLen = uint16(packet->size());
        uint32 innerOpcode = packet->GetOpcode();

        buffer.Write(&innerBodyLen, sizeof(innerBodyLen));
        buffer.Write(&innerOpcode, sizeof(innerOpcode));
        if (!packet->empty())
            buffer.Write(packet->data(), packet->size());

        packetSize += sizeof(innerBodyLen) + sizeof(innerOpcode) + innerBodyLen;
    }

    PacketHeader header;
    header.Size = packetSize;
    _authCrypt.EncryptSend(dataPos, header.Size, header.Tag);

    memcpy(headerPos, &header, sizeof(PacketHeader));

    // Deliberately logged: step 3 of the verification loop ("send it, watch the client react") has no Lua event
    // to watch for a framing opcode, and a wrong inner length is discarded by the client WITHOUT an error. This
    // line plus the CMSG_LOG_DISCONNECT reason that WorldSocket::HandleLogDisconnect already logs are the whole
    // test: bundles emitted and no reason 3 means the client accepted them. Turning that check into one login and
    // two greps is the point.
    TC_LOG_DEBUG("network", "WorldSocket::WriteBundleToBuffer: bundled {} packets into one SMSG_MULTIPLE_PACKETS frame of {} bytes for {}",
        packets.size(), packetSize, GetRemoteIpAddress());
}

bool WorldSocket::CanBundle(EncryptablePacket const& packet) const
{
    // Gate 1 of the client's receive path (see WriteBundleToBuffer). What this rejects is a bundle whose INNER
    // packets would be refused - the 0x4C000D frame itself is on the pre-encryption whitelist, its contents are
    // not. NeedsEncryption() is true from the moment the crypt is initialized, which is exactly the window in
    // which arbitrary opcodes are accepted.
    if (!packet.NeedsEncryption())
        return false;

    // Gate 2 of the same receive path, and the reason this function may not look at the packet alone. A suspended
    // socket accepts family 0x4C and NOTHING else - the check is 0x18C0F60, and any other opcode in that window
    // closes the connection; the full argument sits above WorldPackets::Auth::SuspendComms. Because 0x18C0490
    // recurses into itself for every inner packet, the inner opcodes face that gate too, so the 0x4C000D frame
    // being family 0x4C says nothing whatever about its contents.
    // This is not a theoretical window that only opens after an explicit SMSG_SUSPEND_COMMS - which this server
    // never sends. Every instance socket starts out suspended (see the _clientSuspended assignment in
    // HandleAuthContinuedSession), and the enter-world packets HandlePlayerLogin queues onto that socket are
    // candidates for the very first bundle on it. Without this gate, whether the first frame survives would rest
    // on nothing but the FIFO order happening to put SMSG_RESUME_COMMS in front - a promise neither made nor
    // enforced anywhere. Getting it wrong is not a lost packet but a closed connection: that exact gate violation
    // cost every character its world entry once (88bcee58de, reverted by 5778fe4266).
    // Refused outright instead of admitting family 0x4C only: the sole 0x4C packets this server sends in that
    // window are SMSG_RESUME_COMMS (0 bytes) and SMSG_SUSPEND_COMMS (4 bytes), so the most such a bundle could
    // save is 8 bytes, against giving up the ordering guarantee described in Update().
    if (_clientSuspended)
        return false;

    // Never bundle what WritePacketToBuffer would compress. Compression is stateful and per frame, so a compressed
    // bundle would have to be one deflate unit; keeping the two apart also keeps the failure modes apart.
    if (packet.size() > MinSizeForCompression)
        return false;

    // innerBodyLen is a uint16, so a body of 0x10000 bytes or more can never be bundled.
    return packet.size() <= 0xFFFF;
}

// Puts the server side deflate stream back to its initial state. Only ever called together with sending
// SMSG_RESET_COMPRESSION_CONTEXT, because a reset on one side alone desynchronizes the pair.
void WorldSocket::ResetCompressionContext()
{
    int32 z_res = deflateReset(_compressionStream);
    if (z_res != Z_OK)
        TC_LOG_ERROR("network", "WorldSocket::ResetCompressionContext: deflateReset failed for {}. Error code: {} ({})",
            GetRemoteIpAddress(), z_res, zError(z_res));
}

struct AccountInfo
{
    struct
    {
        uint32 Id;
        std::string Email;
        bool IsLockedToIP;
        std::string LastIP;
        std::string LockCountry;
        bool IsBanned;
    } BattleNet;

    struct
    {
        uint32 Id;
        std::array<uint8, 64> KeyData;
        uint8 Expansion;
        int64 MuteTime;
        uint32 Build;
        LocaleConstant Locale;
        uint32 Recruiter;
        std::string OS;
        Minutes TimezoneOffset;
        bool IsRecruiter;
        AccountTypes Security;
        bool IsBanned;
    } Game;

    bool IsBanned() const { return BattleNet.IsBanned || Game.IsBanned; }

    explicit AccountInfo(PreparedResultSet const* result)
    {
        // SELECT a.id AS accountId, a.session_key_bnet, ba.last_ip, ba.locked, ba.lock_country, a.expansion, a.mutetime, a.client_build, a.locale, a.recruiter, a.os, a.timezone_offset, ba.id AS bnet_account_id, ba.email as bnet_account_email, aa.SecurityLevel,
        // bab.unbandate > UNIX_TIMESTAMP() OR bab.unbandate = bab.bandate AS is_bnet_banned, ab.unbandate > UNIX_TIMESTAMP() OR ab.unbandate = ab.bandate AS is_banned, r.id AS recruitId
        // FROM account a LEFT JOIN account r ON a.id = r.recruiter LEFT JOIN battlenet_accounts ba ON a.battlenet_account = ba.id
        // LEFT JOIN account_access aa ON a.id = aa.AccountID AND aa.RealmID IN (-1, ?) LEFT JOIN battlenet_account_bans bab ON ba.id = bab.id LEFT JOIN account_banned ab ON a.id = ab.id AND ab.active = 1
        // WHERE a.username = ? AND LENGTH(a.session_key_bnet) = 64 ORDER BY aa.RealmID DESC LIMIT 1

        DEFINE_FIELD_ACCESSOR_CACHE_ANONYMOUS(PreparedResultSet, (account_id)(session_key_bnet)(last_ip)(locked)(lock_country)(expansion)(mutetime)(client_build)
            (locale)(recruiter)(os)(timezone_offset)(bnet_account_id)(bnet_account_email)(SecurityLevel)(is_bnet_banned)(is_banned)(recruitId)) fields { *result };

        Game.Id = fields.account_id().GetUInt32();
        Game.KeyData = fields.session_key_bnet().GetBinary<64>();
        BattleNet.LastIP = fields.last_ip().GetStringView();
        BattleNet.IsLockedToIP = fields.locked().GetBool();
        BattleNet.LockCountry = fields.lock_country().GetStringView();
        Game.Expansion = fields.expansion().GetUInt8();
        Game.MuteTime = fields.mutetime().GetInt64();
        Game.Build = fields.client_build().GetUInt32();
        Game.Locale = LocaleConstant(fields.locale().GetUInt8());
        Game.Recruiter = fields.recruiter().GetUInt32();
        Game.OS = fields.os().GetStringView();
        Game.TimezoneOffset = Minutes(fields.timezone_offset().GetInt16());
        BattleNet.Id = fields.bnet_account_id().GetUInt32();
        BattleNet.Email = fields.bnet_account_email().GetStringView();
        Game.Security = AccountTypes(fields.SecurityLevel().GetUInt8());
        BattleNet.IsBanned = fields.is_bnet_banned().GetUInt32() != 0;
        Game.IsBanned = fields.is_banned().GetUInt32() != 0;
        Game.IsRecruiter = fields.recruitId().GetUInt32() != 0;

        if (Game.Locale >= TOTAL_LOCALES)
            Game.Locale = LOCALE_enUS;
    }
};

WorldSocket::ReadDataHandlerResult WorldSocket::HandleAuthSession(WorldPacket&& packet)
{
    LogOpcodeText(CMSG_AUTH_SESSION);

    if (_authed)
    {
        std::scoped_lock guard(_worldSessionLock);
        TC_LOG_ERROR("network", "WorldSocket::ProcessIncoming: received duplicate CMSG_AUTH_SESSION from {}", _worldSession->GetPlayerInfo());
        return ReadDataHandlerResult::Error;
    }

    std::shared_ptr callbackData = std::make_shared<std::pair<WorldPackets::Auth::AuthSession, JSON::RealmList::RealmJoinTicket>>(
        std::piecewise_construct, std::forward_as_tuple(std::move(packet)), std::forward_as_tuple());
    if (!callbackData->first.ReadNoThrow())
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadDataHandler(): client {} sent malformed CMSG_AUTH_SESSION", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    if (!JSON::Deserialize(callbackData->first.RealmJoinTicket, &callbackData->second))
    {
        SendAuthResponseError(ERROR_WOW_SERVICES_INVALID_JOIN_TICKET);
        DelayedCloseSocket();
        return ReadDataHandlerResult::Error;
    }

    // Get the account information from the auth database
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_INFO_BY_NAME);
    stmt->setInt32(0, int32(sRealmList->GetCurrentRealmId().Realm));
    stmt->setString(1, callbackData->second.gameaccount());

    QueueQuery(LoginDatabase.AsyncQuery(stmt).WithPreparedCallback([this, callbackData = std::move(callbackData)](PreparedQueryResult const& result) mutable
    {
        HandleAuthSessionCallback(&callbackData->first, &callbackData->second, result.get());
    }));
    return ReadDataHandlerResult::WaitingForQuery;
}

void WorldSocket::HandleAuthSessionCallback(WorldPackets::Auth::AuthSession const* authSession, JSON::RealmList::RealmJoinTicket* joinTicket, PreparedResultSet const* result)
{
    // Stop if the account is not found
    if (!result)
    {
        // We can not log here, as we do not know the account. Thus, no accountId.
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Sent Auth Response (unknown account).");
        DelayedCloseSocket();
        return;
    }

    std::string address = GetRemoteIpAddress().to_string();

    AccountInfo account(result);

    ClientBuild::Info const* buildInfo = ClientBuild::GetBuildInfo(account.Game.Build);
    if (!buildInfo)
    {
        SendAuthResponseError(ERROR_BAD_VERSION);
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Missing client build info for build {} ({}).", account.Game.Build, address);
        DelayedCloseSocket();
        return;
    }

    ClientBuild::VariantId buildVariant =
    {
        .Platform = ClientBuild::Platform::Id(joinTicket->platform()),
        .Arch = ClientBuild::Arch::Id(joinTicket->clientarch()),
        .Type = ClientBuild::Type::Id(joinTicket->type())
    };
    auto clientBuildAuthKey = std::ranges::find(buildInfo->AuthKeys, buildVariant, &ClientBuild::AuthKey::Variant);
    if (clientBuildAuthKey == buildInfo->AuthKeys.end())
    {
        SendAuthResponseError(ERROR_BAD_VERSION);
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Missing client build auth key for build {} variant {}-{}-{} ({}).", account.Game.Build,
            buildVariant.Platform, buildVariant.Arch, buildVariant.Type, address);
        DelayedCloseSocket();
        return;
    }

    Trinity::Crypto::SHA512 digestKeyHash;
    digestKeyHash.UpdateData(account.Game.KeyData.data(), account.Game.KeyData.size());
    digestKeyHash.UpdateData(clientBuildAuthKey->Key.data(), clientBuildAuthKey->Key.size());
    digestKeyHash.Finalize();

    Trinity::Crypto::HMAC_SHA512 hmac(digestKeyHash.GetDigest());
    hmac.UpdateData(authSession->LocalChallenge);
    hmac.UpdateData(_serverChallenge);
    hmac.UpdateData(AuthCheckSeed);
    hmac.Finalize();

    // Check that Key and account name are the same on client and server
    if (memcmp(hmac.GetDigest().data(), authSession->Digest.data(), authSession->Digest.size()) != 0)
    {
        SendAuthResponseError(ERROR_DENIED);
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Authentication failed for account: {} ('{}') address: {}", account.Game.Id, joinTicket->gameaccount(), address);
        DelayedCloseSocket();
        return;
    }

    Trinity::Crypto::SHA512 keyData;
    keyData.UpdateData(account.Game.KeyData.data(), account.Game.KeyData.size());
    keyData.Finalize();

    Trinity::Crypto::HMAC_SHA512 sessionKeyHmac(keyData.GetDigest());
    sessionKeyHmac.UpdateData(_serverChallenge);
    sessionKeyHmac.UpdateData(authSession->LocalChallenge);
    sessionKeyHmac.UpdateData(SessionKeySeed);
    sessionKeyHmac.Finalize();

    SessionKeyGenerator<Trinity::Crypto::SHA512> sessionKeyGenerator(sessionKeyHmac.GetDigest());
    sessionKeyGenerator.Generate(_sessionKey.data(), 40);

    Trinity::Crypto::HMAC_SHA512 encryptKeyGen(_sessionKey);
    encryptKeyGen.UpdateData(authSession->LocalChallenge);
    encryptKeyGen.UpdateData(_serverChallenge);
    encryptKeyGen.UpdateData(EncryptionKeySeed);
    encryptKeyGen.Finalize();

    // only first 32 bytes of the hmac are used
    memcpy(_encryptKey.data(), encryptKeyGen.GetDigest().data(), 32);

    LoginDatabasePreparedStatement* stmt = nullptr;

    if (sWorld->getBoolConfig(CONFIG_ALLOW_LOGGING_IP_ADDRESSES_IN_DATABASE))
    {
        // As we don't know if attempted login process by ip works, we update last_attempt_ip right away
        stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_LAST_ATTEMPT_IP);
        stmt->setString(0, address);
        stmt->setString(1, joinTicket->gameaccount());
        LoginDatabase.Execute(stmt);
        // This also allows to check for possible "hack" attempts on account
    }

    stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_INFO_CONTINUED_SESSION);
    stmt->setBinary(0, _sessionKey);
    stmt->setUInt32(1, account.Game.Id);
    LoginDatabase.Execute(stmt);

    // First reject the connection if packet contains invalid data or realm state doesn't allow logging in
    if (sWorld->IsClosed())
    {
        SendAuthResponseError(ERROR_DENIED);
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: World closed, denying client ({}).", address);
        DelayedCloseSocket();
        return;
    }

    // Plunderstorm / WoW Labs event-realm routing: this one worldserver process also answers for the WoW Labs
    // event realm id (WowLabs.EventRealmId), so a client that picked Plunderstorm and reconnected via
    // C_RealmList.ConnectToEventRealm targets that id. Accept it here (same process, same loaded data - only the
    // realm identity + per-session game mode differ) and remember which realm this session hit.
    int32 const eventRealmId = sConfigMgr->GetIntDefault("WowLabs.EventRealmId", 0);
    bool const onEventRealm = eventRealmId != 0 && authSession->RealmID == uint32(eventRealmId);
    if (authSession->RealmID != sRealmList->GetCurrentRealmId().Realm && !onEventRealm)
    {
        SendAuthResponseError(ERROR_DENIED);
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Client {} requested connecting with realm id {} but this realm has id {} set in config.",
            address, authSession->RealmID, sRealmList->GetCurrentRealmId().Realm);
        DelayedCloseSocket();
        return;
    }

    if (IpLocationRecord const* location = sIPLocation->GetLocationRecord(address))
        _ipCountry = location->CountryCode;

    ///- Re-check ip locking (same check as in auth).
    if (account.BattleNet.IsLockedToIP)
    {
        if (account.BattleNet.LastIP != address)
        {
            SendAuthResponseError(ERROR_RISK_ACCOUNT_LOCKED);
            TC_LOG_DEBUG("network", "WorldSocket::HandleAuthSession: Sent Auth Response (Account IP differs. Original IP: {}, new IP: {}).", account.BattleNet.LastIP, address);
            // We could log on hook only instead of an additional db log, however action logger is config based. Better keep DB logging as well
            sScriptMgr->OnFailedAccountLogin(account.Game.Id);
            DelayedCloseSocket();
            return;
        }
    }
    else if (!account.BattleNet.LockCountry.empty() && account.BattleNet.LockCountry != "00" && !_ipCountry.empty())
    {
        if (account.BattleNet.LockCountry != _ipCountry)
        {
            SendAuthResponseError(ERROR_RISK_ACCOUNT_LOCKED);
            TC_LOG_DEBUG("network", "WorldSocket::HandleAuthSession: Sent Auth Response (Account country differs. Original country: {}, new country: {}).", account.BattleNet.LockCountry, _ipCountry);
            // We could log on hook only instead of an additional db log, however action logger is config based. Better keep DB logging as well
            sScriptMgr->OnFailedAccountLogin(account.Game.Id);
            DelayedCloseSocket();
            return;
        }
    }

    int64 mutetime = account.Game.MuteTime;
    //! Negative mutetime indicates amount of seconds to be muted effective on next login - which is now.
    if (mutetime < 0)
    {
        mutetime = GameTime::GetGameTime() - mutetime;

        stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_MUTE_TIME_LOGIN);
        stmt->setInt64(0, mutetime);
        stmt->setUInt32(1, account.Game.Id);
        LoginDatabase.Execute(stmt);
    }

    if (account.IsBanned())
    {
        SendAuthResponseError(ERROR_GAME_ACCOUNT_BANNED);
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Sent Auth Response (Account banned).");
        sScriptMgr->OnFailedAccountLogin(account.Game.Id);
        DelayedCloseSocket();
        return;
    }

    // Check locked state for server
    AccountTypes allowedAccountType = sWorld->GetPlayerSecurityLimit();
    TC_LOG_DEBUG("network", "Allowed Level: {} Player Level {}", allowedAccountType, account.Game.Security);
    if (allowedAccountType > SEC_PLAYER && account.Game.Security < allowedAccountType)
    {
        SendAuthResponseError(ERROR_SERVER_IS_PRIVATE);
        TC_LOG_DEBUG("network", "WorldSocket::HandleAuthSession: User tries to login but his security level is not enough");
        sScriptMgr->OnFailedAccountLogin(account.Game.Id);
        DelayedCloseSocket();
        return;
    }

    TC_LOG_DEBUG("network", "WorldSocket::HandleAuthSession: Client '{}' authenticated successfully from {}.", joinTicket->gameaccount(), address);

    if (sWorld->getBoolConfig(CONFIG_ALLOW_LOGGING_IP_ADDRESSES_IN_DATABASE))
    {
        // Update the last_ip in the database as it was successful for login
        stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_LAST_IP);

        stmt->setString(0, address);
        stmt->setString(1, joinTicket->gameaccount());

        LoginDatabase.Execute(stmt);
    }

    // At this point, we can safely hook a successful login
    sScriptMgr->OnAccountLogin(account.Game.Id);

    _authed = true;
    _worldSession = new WorldSession(account.Game.Id, std::move(*joinTicket->mutable_gameaccount()), account.BattleNet.Id,
        std::move(account.BattleNet.Email), static_pointer_cast<WorldSocket>(shared_from_this()), account.Game.Security,
        account.Game.Expansion, mutetime, std::move(account.Game.OS), account.Game.TimezoneOffset, account.Game.Build, buildVariant,
        account.Game.Locale, account.Game.Recruiter, account.Game.IsRecruiter);

    _worldSession->SetOnWowLabsRealm(onEventRealm);   // per-session Plunderstorm mode for event-realm connections

    QueueQuery(_worldSession->LoadPermissionsAsync().WithPreparedCallback([this](PreparedQueryResult result)
    {
        LoadSessionPermissionsCallback(std::move(result));
    }));
    AsyncRead(Trinity::Net::InvokeReadHandlerCallback<WorldSocket>{ .Socket = this });
}

void WorldSocket::LoadSessionPermissionsCallback(PreparedQueryResult result)
{
    // RBAC must be loaded before adding session to check for skip queue permission
    _worldSession->GetRBACData()->LoadFromDBCallback(std::move(result));

    SendPacketAndLogOpcode(*WorldPackets::Auth::EnterEncryptedMode(_encryptKey, true).Write());
}

WorldSocket::ReadDataHandlerResult WorldSocket::HandleAuthContinuedSession(WorldPacket&& packet)
{
    LogOpcodeText(CMSG_AUTH_CONTINUED_SESSION);

    if (_authed)
    {
        std::scoped_lock guard(_worldSessionLock);
        TC_LOG_ERROR("network", "WorldSocket::ProcessIncoming: received duplicate CMSG_AUTH_CONTINUED_SESSION from {}", _worldSession->GetPlayerInfo());
        return ReadDataHandlerResult::Error;
    }

    std::shared_ptr<WorldPackets::Auth::AuthContinuedSession> authSession = std::make_shared<WorldPackets::Auth::AuthContinuedSession>(std::move(packet));
    if (!authSession->ReadNoThrow())
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadDataHandler(): client {} sent malformed CMSG_AUTH_CONTINUED_SESSION", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    WorldSession::ConnectToKey key;
    key.Raw = authSession->Key;

    _type = ConnectionType(key.Fields.ConnectionType);
    if (_type != CONNECTION_TYPE_INSTANCE)
    {
        SendAuthResponseError(ERROR_DENIED);
        DelayedCloseSocket();
        return ReadDataHandlerResult::Error;
    }

    // The client considers this socket suspended before it has received anything at all: the NetClient constructor
    // (0x18BEA60, store at 0x18BEF6D `mov word ptr [rbx+0x218], 0x100`) marks slot 1 - the instance slot - as
    // suspended, which is why this server's unpaired SMSG_RESUME_COMMS works. Mirror that starting state, or
    // CanBundle would judge the first frame on a fresh instance socket against the wrong gate. See the argument
    // above WorldPackets::Auth::SuspendComms.
    _clientSuspended = true;

    uint32 accountId = uint32(key.Fields.AccountId);
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_INFO_CONTINUED_SESSION);
    stmt->setUInt32(0, accountId);

    QueueQuery(LoginDatabase.AsyncQuery(stmt).WithPreparedCallback([this, authSession = std::move(authSession)](PreparedQueryResult const& result) mutable
    {
        HandleAuthContinuedSessionCallback(authSession.get(), result.get());
    }));
    return ReadDataHandlerResult::WaitingForQuery;
}

void WorldSocket::HandleAuthContinuedSessionCallback(WorldPackets::Auth::AuthContinuedSession const* authSession, PreparedResultSet const* result)
{
    if (!result)
    {
        SendAuthResponseError(ERROR_DENIED);
        DelayedCloseSocket();
        return;
    }

    WorldSession::ConnectToKey key;
    _key = key.Raw = authSession->Key;

    uint32 accountId = uint32(key.Fields.AccountId);
    Field* fields = result->Fetch();
    std::string login = fields[0].GetString();
    _sessionKey = fields[1].GetBinary<SESSION_KEY_LENGTH>();

    Trinity::Crypto::HMAC_SHA512 hmac(_sessionKey);
    hmac.UpdateData(reinterpret_cast<uint8 const*>(&authSession->Key), sizeof(authSession->Key));
    hmac.UpdateData(authSession->LocalChallenge);
    hmac.UpdateData(_serverChallenge);
    hmac.UpdateData(ContinuedSessionSeed);
    hmac.Finalize();

    if (memcmp(hmac.GetDigest().data(), authSession->Digest.data(), authSession->Digest.size()))
    {
        TC_LOG_ERROR("network", "WorldSocket::HandleAuthContinuedSession: Authentication failed for account: {} ('{}') address: {}", accountId, login, GetRemoteIpAddress());
        DelayedCloseSocket();
        return;
    }

    Trinity::Crypto::HMAC_SHA512 encryptKeyGen(_sessionKey);
    encryptKeyGen.UpdateData(authSession->LocalChallenge);
    encryptKeyGen.UpdateData(_serverChallenge);
    encryptKeyGen.UpdateData(EncryptionKeySeed);
    encryptKeyGen.Finalize();

    // only first 32 bytes of the hmac are used
    memcpy(_encryptKey.data(), encryptKeyGen.GetDigest().data(), 32);

    SendPacketAndLogOpcode(*WorldPackets::Auth::EnterEncryptedMode(_encryptKey, true).Write());
    AsyncRead(Trinity::Net::InvokeReadHandlerCallback<WorldSocket>{ .Socket = this });
}

WorldSocket::ReadDataHandlerResult WorldSocket::HandleKeepAlive()
{
    std::scoped_lock sessionGuard(_worldSessionLock);

    LogOpcodeText(CMSG_KEEP_ALIVE, sessionGuard);

    if (!_worldSession)
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadDataHandler: client {} sent CMSG_KEEP_ALIVE without being authenticated", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    _worldSession->ResetTimeOutTime(true);
    return ReadDataHandlerResult::Ok;
}

WorldSocket::ReadDataHandlerResult WorldSocket::HandleLogDisconnect(WorldPacket&& packet) const
{
    LogOpcodeText(CMSG_LOG_DISCONNECT);

    WorldPackets::Auth::LogDisconnect logDisconnect(std::move(packet));
    if (!logDisconnect.ReadNoThrow())
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadDataHandler(): client {} sent malformed CMSG_LOG_DISCONNECT", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    TC_LOG_DEBUG("network", "WorldSocket::ReadDataHandler: client {} sent CMSG_LOG_DISCONNECT reason {}", GetRemoteIpAddress(), logDisconnect.Reason);
    return ReadDataHandlerResult::Ok;
}

WorldSocket::ReadDataHandlerResult WorldSocket::HandleConnectToFailed(WorldPacket&& packet)
{
    std::scoped_lock sessionGuard(_worldSessionLock);

    LogOpcodeText(CMSG_CONNECT_TO_FAILED, sessionGuard);

    WorldPackets::Auth::ConnectToFailed connectToFailed(std::move(packet));
    if (!connectToFailed.ReadNoThrow())
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadDataHandler(): client {} sent malformed CMSG_CONNECT_TO_FAILED", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    if (_worldSession)
    {
        if (_worldSession->PlayerLoading())
        {
            // Do NOT send SMSG_DROP_NEW_CONNECTION here, however tempting it looks. The client does not only send
            // CMSG_CONNECT_TO_FAILED when its connect attempt failed - it also sends it when it REFUSED the order,
            // because consumer 0x18C0FF0 bails out before touching the payload if the pending slot Con|2 is already
            // taken:
            //     018C1156  cmp qword [rsi + rax*8 + 0x1A0], 0   ; rax = Con|2
            //     018C115F  jne  018C14B0                        ; -> CMSG_CONNECT_TO_FAILED
            // A pending socket left over from an earlier attempt therefore makes every retry fail the same way, and
            // SMSG_DROP_NEW_CONNECTION is the opcode that clears a pending slot - but it cannot clear THIS one.
            // Consumer 0x18C1500 does not take the slot from the payload (there is none); it looks the RECEIVING
            // socket up in NetClient+0x1A0[0..3] and clears idx|2 of the index it finds. Sent over the realm socket
            // (index 0) it clears slot 2, the pending REALM slot, which this server never fills because it never
            // issues a CONNECT_TO with Con = 0. The pending slot of the instance handover is Con|2 = 3 (WorldSession
            // sets connectTo.Con = CONNECTION_TYPE_INSTANCE), and slot 3 is only reachable over the ESTABLISHED
            // instance socket at index 1 - which by definition does not exist while the handover to it is failing.
            // A send from here is a packet the client answers with nothing at all.
            switch (connectToFailed.Serial)
            {
                case WorldPackets::Auth::ConnectToSerial::WorldAttempt1:
                    _worldSession->SendConnectToInstance(WorldPackets::Auth::ConnectToSerial::WorldAttempt2);
                    break;
                case WorldPackets::Auth::ConnectToSerial::WorldAttempt2:
                    _worldSession->SendConnectToInstance(WorldPackets::Auth::ConnectToSerial::WorldAttempt3);
                    break;
                case WorldPackets::Auth::ConnectToSerial::WorldAttempt3:
                    _worldSession->SendConnectToInstance(WorldPackets::Auth::ConnectToSerial::WorldAttempt4);
                    break;
                case WorldPackets::Auth::ConnectToSerial::WorldAttempt4:
                    _worldSession->SendConnectToInstance(WorldPackets::Auth::ConnectToSerial::WorldAttempt5);
                    break;
                case WorldPackets::Auth::ConnectToSerial::WorldAttempt5:
                {
                    TC_LOG_ERROR("network", "{} failed to connect 5 times to world socket, aborting login", _worldSession->GetPlayerInfo());
                    _worldSession->AbortLogin(WorldPackets::Character::LoginFailureReason::NoWorld);
                    break;
                }
                default:
                    break;
            }
        }
        //else
        //{
        //    transfer_aborted when/if we get map node redirection
        //    SendPacketAndLogOpcode(*WorldPackets::Auth::ResumeComms().Write());
        //}
    }

    return ReadDataHandlerResult::Ok;
}

WorldSocket::ReadDataHandlerResult WorldSocket::HandleEnterEncryptedModeAck()
{
    LogOpcodeText(CMSG_ENTER_ENCRYPTED_MODE_ACK);

    _authCrypt.Init(_encryptKey);
    if (_type == CONNECTION_TYPE_REALM)
        sWorld->AddSession(_worldSession);
    else
        sWorld->AddInstanceSocket(static_pointer_cast<WorldSocket>(shared_from_this()), _key);

    return ReadDataHandlerResult::Ok;
}

void WorldSocket::SendAuthResponseError(uint32 code)
{
    WorldPackets::Auth::AuthResponse response;
    response.Result = code;
    SendPacketAndLogOpcode(*response.Write());
}

WorldSocket::ReadDataHandlerResult WorldSocket::HandlePing(WorldPacket&& packet)
{
    LogOpcodeText(CMSG_PING);

    WorldPackets::Auth::Ping ping(std::move(packet));
    if (!ping.ReadNoThrow())
    {
        TC_LOG_ERROR("network", "WorldSocket::ReadDataHandler(): client {} sent malformed CMSG_PING", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    TimePoint lastPingTime = std::exchange(_lastPingTime, TimePoint::clock::now());

    if (lastPingTime != TimePoint())
    {
        TimePoint::duration diff = _lastPingTime - lastPingTime;

        if (diff < 27s)
        {
            ++_overSpeedPings;

            uint32 maxAllowed = sWorld->getIntConfig(CONFIG_MAX_OVERSPEED_PINGS);

            if (maxAllowed && _overSpeedPings > maxAllowed)
            {
                bool ignoresOverspeedPingsLimit = [&]
                {
                    std::scoped_lock sessionGuard(_worldSessionLock);
                    return _worldSession && _worldSession->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_OVERSPEED_PING);
                }();

                if (!ignoresOverspeedPingsLimit)
                {
                    TC_LOG_ERROR("network", "WorldSocket::HandlePing: {} kicked for over-speed pings (address: {})",
                        _worldSession->GetPlayerInfo(), GetRemoteIpAddress());

                    return ReadDataHandlerResult::Error;
                }
            }
        }
        else
            _overSpeedPings = 0;
    }

    bool success = [&]
    {
        std::scoped_lock sessionGuard(_worldSessionLock);
        if (_worldSession)
        {
            _worldSession->SetLatency(ping.Latency);
            return true;
        }
        return false;
    }();

    if (!success)
    {
        TC_LOG_ERROR("network", "WorldSocket::HandlePing: peer sent CMSG_PING, but is not authenticated or got recently kicked, address = {}", GetRemoteIpAddress());
        return ReadDataHandlerResult::Error;
    }

    SendPacketAndLogOpcode(*WorldPackets::Auth::Pong(ping.Serial).Write());
    return ReadDataHandlerResult::Ok;
}

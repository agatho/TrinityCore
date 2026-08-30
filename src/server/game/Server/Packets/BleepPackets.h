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

#ifndef TRINITYCORE_BLEEP_PACKETS_H
#define TRINITYCORE_BLEEP_PACKETS_H

#include "Packet.h"
#include "PacketUtilities.h"
#include <string>
#include <vector>

// ----------------------------------------------------------------------------------------------
// Einheit w4_cmsg_43_3D - Block B8 "Bleep", Phase A.
//
// Bleep ist ein Proxy-/Relay-Dienst der NETZSCHICHT, nicht der Spielschicht: die Quelldateien
// heissen ...\WoW\Source\Net\NetClient\BleepPinger.cpp und ...\BleepTokenManager.cpp. Der Client
// holt eine Proxyliste, misst Latenz gegen die Proxys und verwaltet zeitlich begrenzte Tokens.
// Es gibt dafuer KEINE Lua-Oberflaeche - kein Namensraum, keine Funktion, kein Ereignis, kein
// Enum (dreifach geprueft: 613 Dateien generierter API-Doku, die gesamte Interface-Quelle, und
// 1735 aus der Client-Reflexion extrahierte Enums). Kein UI-Konsument heisst: kein Dialog kann
// haengen bleiben. Fehler werden ausschliesslich protokolliert.
//
// D1 ist fuer die Proxyliste am Draht BEWIESEN, nicht gerechnet: sechs echte
// SMSG_FETCH_BLEEP_PROXIES_RESPONSE-Pakete aus vier Aufnahmen (Builds 69299 / 69382 / 69404)
// wurden gegen diese Feldliste dekodiert und in allen sechs Faellen VOLLSTAENDIG konsumiert
// (Rest 0). Beispielelement: Address "137.221.74.103", PingToken "GlOR2uz+csxHNvGqTTU2ug==",
// ProxyId "bleep-proxy-ec04-eqda6.network.cloud.blizzard.net", PingTokenValidDuration
// 120'000'000'000 ns. Die Bit-Sektion 0x3B 0x06 0x20 zerlegt MSB-first zu
// 001110|11000|00110001|00000 = addrLen 14, tokenLen 24, proxyIdLen 49, 5 Bit Fuellung.
//
// FALLE, die zwei unabhaengige Quellen falsch beschreiben: derselbe logische Wert "proxy_id" hat
// ZWEI verschiedene Drahtformen.
//   BleepProxy.ProxyId (SMSG 0x450383):  Inline-char[256], bits<8>,  OHNE NUL
//   BleepToken.ProxyId (SMSG 0x450384,
//                       CMSG 0x4301A2/A3): Heap-String,    bits<24>, INKLUSIVE NUL
// Und die JamDynamicString sitzt auf ProxyId, NICHT auf Address - der Reflexionskatalog sagt das
// Gegenteil. Belegt in beiden Richtungen: Writer 0x6B2F80 (Zeilen 58-61, 109-111) und Reader
// 0x6B9060 (Zeilen 23, 25, 51 - ReadDynString 0x347D750), plus die Destruktorschleife 0x612DD0,
// die *(i+40) nur freigibt, wenn *(int64*)(i+56) >= 0 - also ein Heapzeiger, kein Inline-Puffer.
//
// SICHERHEIT: die Reader-Bitbreiten sind WEITER als die Zielpuffer.
//   BleepToken.Address: Puffer 46 (INET6_ADDRSTRLEN), Reader bits<6> = bis 63. Der Client
//   schreibt *((BYTE*)a1 + len + 80) = 0 in ein 128-Byte-Element - bei len > 47 also BIS ZU
//   16 BYTE UEBER DAS ELEMENT HINAUS. Wer die Puffergroesse aus der Bitbreite raet, dimensioniert
//   die Validierung um 16 Byte zu grosszuegig. Der Server darf Address nie laenger als 46 senden.
// ----------------------------------------------------------------------------------------------

namespace WorldPackets
{
    namespace Bleep
    {
        // Puffergroessen, aus den strnlen-Zaehlerkonstanten gemessen - NICHT aus den Bitbreiten
        // abgeleitet. Sie sind die Eingangsvalidierung und werden in ReadBleepToken
        // (BleepPackets.cpp) angewendet: ein zu langer Wert verwirft das Paket, statt spaeter in
        // der Fehlschlagsspiegelung von SMSG_REFRESH_BLEEP_TOKENS_RESPONSE wieder hinauszugehen.
        constexpr std::size_t MaxProxyAddressLength = 46;    // INET6_ADDRSTRLEN
        constexpr std::size_t MaxProxyTokenLength   = 24;    // 24 Zeichen Base64 (+ NUL im Puffer)
        constexpr std::size_t MaxProxyIdLength      = 255;   // Inline-char[256]

        // Element von CMSG_BLEEP_PONG, Client-Typ BleepPingData (280 Byte).
        struct BleepPingData
        {
            uint64 ClientPing = 0;          // Mittelwert ueber bis zu 20 RTT-Proben (0x18C80D0)
            uint64 PingsSent = 0;           // Zaehler +172, erhoeht je abgesetztem Ping
            uint64 PongsReceived = 0;       // Zaehler +176, Divisor des RTT-Mittelwerts
            std::string ProxyId;            // Puffer 256, ohne NUL
        };

        // Element von SMSG_FETCH_BLEEP_PROXIES_RESPONSE, Client-Typ BleepProxy (360 Byte).
        struct BleepProxy
        {
            std::string Address;                    // Puffer 46, bits<6>, ohne NUL
            uint64 PingPort = 0;
            std::string PingToken;                  // Puffer 25, bits<5>, ohne NUL
            uint64 PingTokenValidDuration = 0;      // Nanosekunden
            uint64 Port = 0;
            std::string ProxyId;                    // Puffer 256, bits<8>, ohne NUL
        };

        // Element von CMSG_REFRESH/EXPIRE_BLEEP_TOKENS und SMSG_REFRESH_BLEEP_TOKENS_RESPONSE,
        // Client-Typ BleepToken (128 Byte). Die Byte-Sektion ist NICHT Deklarationsreihenfolge:
        // TokenLifespanNanoSecs steht am Draht hinter der GESAMTEN Bit-Sektion und vor allen
        // drei Rohblöcken.
        struct BleepToken
        {
            std::string Token;                      // Puffer 25, bits<5>, ohne NUL
            uint64 TokenLifespanNanoSecs = 0;
            std::string ProxyId;                    // JamDynamicString, bits<24>, INKL. NUL
            std::string Address;                    // Puffer 46, bits<6>, ohne NUL
        };

        // 0x4301A1, Writer 0x6B2F50 - leere Nutzlast.
        // Am Draht belegt: 8 Pakete, 0 Byte; in sechs Aufnahmen jeweils unmittelbar gefolgt von
        // der ~2274-Byte-Antwort. Das einzige Paar dieses Satzes mit Draht-Beleg auf BEIDEN Seiten.
        // Zweiter Weg, der den Opcode umgeht: die Battle.net-RPC
        // "Command_FetchBleepProxiesRequest_v1", gesteuert von byte_7FF7855FBAD8.
        class FetchBleepProxies final : public ClientPacket
        {
        public:
            explicit FetchBleepProxies(WorldPacket&& packet) : ClientPacket(CMSG_FETCH_BLEEP_PROXIES, std::move(packet)) { }

            void Read() override { }
        };

        // 0x4301A0, Writer 0x6B2E00 - Antwort auf SMSG_BLEEP_PING, das in Familie 0x4C liegt
        // (0x4C000C), NICHT in 0x45. Wer das Paar ueber den Familienindex sucht, findet es nicht.
        // Hier ist das "u8" des Subplans wirklich ein Byte: ein 8-Bit-Laengenfeld bei leerem
        // Akkumulator, danach ein FLUSH, der nichts schreibt.
        class BleepPong final : public ClientPacket
        {
        public:
            explicit BleepPong(WorldPacket&& packet) : ClientPacket(CMSG_BLEEP_PONG, std::move(packet)) { }

            void Read() override;

            Array<BleepPingData, 64> PingData;
        };

        // 0x4301A2 / 0x4301A3 - EIN Serializer (0x6B2F80), zwei Opcodes. Die Huellen 0x6B3120
        // und 0x6B3170 schreiben nur den Opcode-Kopf und springen in denselben Rumpf; der Rumpf
        // schreibt selbst keinen Kopf. Die Nutzlast ist damit byteweise identisch.
        class RefreshBleepTokens final : public ClientPacket
        {
        public:
            explicit RefreshBleepTokens(WorldPacket&& packet) : ClientPacket(CMSG_REFRESH_BLEEP_TOKENS, std::move(packet)) { }

            void Read() override;

            Array<BleepToken, 64> Tokens;
        };

        class ExpireBleepTokens final : public ClientPacket
        {
        public:
            explicit ExpireBleepTokens(WorldPacket&& packet) : ClientPacket(CMSG_EXPIRE_BLEEP_TOKENS, std::move(packet)) { }

            void Read() override;

            Array<BleepToken, 64> Tokens;
        };

        // 0x450383, Reader 0x612BD0. Draht beidseitig belegt (siehe Kopfkommentar).
        class FetchBleepProxiesResponse final : public ServerPacket
        {
        public:
            explicit FetchBleepProxiesResponse() : ServerPacket(SMSG_FETCH_BLEEP_PROXIES_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            std::vector<BleepProxy> Proxies;
        };

        // 0x450384, Reader 0x612FB0 -> Rumpf 0x612DD0, Element 0x6B9060.
        // Es gibt KEIN Statusfeld und KEINE Erfolgsliste: die Nachricht traegt ausschliesslich
        // die Fehlschlaege, Erfolg ist Count == 0. Belegt am Konsumenten 0x18CA3D0:
        //   if (!v4[5])  ->  BleepTokenManager.cpp:207 "All refreshes succeded"
        //   sonst        ->  BleepTokenManager.cpp:236 "Token failure detected"
        // Die Zuordnung eines Fehlschlags laeuft ueber strcmp auf Token UND Address;
        // ProxyId wird dafuer NICHT verwendet.
        class RefreshBleepTokensResponse final : public ServerPacket
        {
        public:
            explicit RefreshBleepTokensResponse() : ServerPacket(SMSG_REFRESH_BLEEP_TOKENS_RESPONSE, 16) { }

            WorldPacket const* Write() override;

            std::vector<BleepToken> Failures;       // leer == alle Erneuerungen erfolgreich
            uint64 TokenLifespanNanoSecs = 0;       // neue Laufzeit fuer ALLE erfolgreichen Tokens
            uint32 Field64 = 0;                     // UNVERIFIED: gelesen, aber von keinem
                                                    // auffindbaren Konsumenten benutzt
        };
    }
}

#endif // TRINITYCORE_BLEEP_PACKETS_H

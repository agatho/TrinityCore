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

#ifndef TRINITYCORE_VOICE_CHAT_PACKETS_H
#define TRINITYCORE_VOICE_CHAT_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"
#include <string>

// ----------------------------------------------------------------------------------------------
// Einheit w4_cmsg_43_3D - Block B9 "Voice-Chat", Phase A.
// Build 12.1.0.69382, ImageBase 0x7FF780FD0000.
//
// DIESER BLOCK GALT ALS WAND UND IST KEINE MEHR - jedenfalls nicht strukturell. Der 68275-Lauf
// hielt alle drei SMSG fuer reflexionsverdeckt, weil der Deserializer-Eintrag der
// Scalar-Deleting-Destruktor ist und die Vtable nur GetOpcode und Destruktoren traegt. Ueber den
// DISPATCH-CASE kommt man trotzdem heran; alle drei Reader sind gelesen. Was bleibt, ist keine
// Strukturluecke, sondern eine DATENQUELLENluecke: die Antworten tragen extern erzeugte Daten
// (Sprachanbieter-Kanaltoken, STT-Token). Ein Realm ohne Sprachdienst kann diese Felder nicht
// fuellen - er kann aber sauber "nicht verfuegbar" sagen, und genau das tut dieser Block.
//
// DER GEMEINSAME STATUSKANAL, am Binary belegt: die Konsumenten von LOGIN_RESPONSE (0x19BAB20)
// und CHANNEL_INFO_RESPONSE (0x19B9640) rufen beide
//     sub_7FF7829895B0(*(uint32*)(msg + 0x24), *(uint8*)(msg + 0x20))
// -> uint8 @+0x20 ist der Statuscode, uint32 @+0x24 ein plattformspezifischer Zusatzcode.
//
// WELCHER STATUSCODE EHRLICH IST - und warum es genau dieser ist. Die Kette in 0x19B95B0:
//   if (status != 0) { sub_7FF782996B00(msg, status);            // feuert VOICE_CHAT_ERROR
//                      if (((status - 15) & 0xFD) == 0) { ... }  // NUR 15 und 17 setzen die
//                                                                // klebrigen "Sprache dauerhaft
//                                                                // nicht verfuegbar"-Riegel
//                      LABEL_4: <alle offenen Zustaende abraeumen>; return 0; }
// und die Sprungtabelle von sub_7FF782996B00 (0x19C6B00, Index bei 0x19C6B9C, `cmp dl,0x16`)
// merkt sich genau die Werte {0,3,12,13,14,15,16,18,22} als "erkannt"; alles andere faellt in
// den default-Zweig und laesst die Statusmarke des Clients veraltet stehen.
//   -> 18 ist erkannt, feuert VOICE_CHAT_ERROR, setzt KEINEN dauerhaften Riegel und raeumt den
//      Anmelde-/Beitrittsversuch sauber ab. Genau das soll passieren.
//
// ⚠ UNVERIFIED, und ausdruecklich gegen die Vorlage: der Brief nennt 18 "Disabled"
// (Enum.VoiceChatStatusCode). Die Bytefolge "Disabled\0" kommt im GESAMTEN Abbild nicht vor - der
// einzige Treffer ist "PlayerVoiceChatParentalDisabled". Es gibt im Binary keine auffindbare
// Name->Wert-Tabelle fuer dieses Enum (kein Zeiger, kein Offset, kein lea auf
// "VoiceChatStatusCode"/"VoiceChatStatusCodeMeta"). Das VERHALTEN von 18 ist vollstaendig belegt,
// der NAME nicht. Deshalb heisst die Konstante unten nach ihrer Wirkung, nicht nach dem Enum.
//
// ⚠ ZWEITE FALLE, gegenlaeufig zur CMSG-Seite: die Zeichenketten der drei ANTWORTEN gehen ueber
// ReadDynString (0x347D750), also mit einer Laenge INKLUSIVE NUL und dem NUL AM DRAHT - obwohl
// ihre Laengenfelder nur 6, 7 oder 9 Bit breit sind. Und Laenge 1 verbraucht NULL Byte:
//     if (a3 > 1) { consume a3 bytes; if (last != 0) return 0; } else { *dst = 0; return 1; }
// Ein Server, der fuer den LEEREN String len = 1 plus ein NUL-Byte schreibt, verschiebt den
// ganzen Rest um ein Byte. Leer heisst len = 0 und NULL Byte.
// ----------------------------------------------------------------------------------------------

namespace WorldPackets
{
    namespace VoiceChat
    {
        // Enum.VoiceChatStatusCode. Nur die Werte, die dieser Block wirklich benutzt oder
        // gegen die er pruefen muss; die uebrigen sind ohne Name->Wert-Tabelle nicht belegbar.
        enum VoiceChatStatusCode : uint8
        {
            VOICE_CHAT_STATUS_SUCCESS                       = 0,

            // Vom Client SELBST erzeugt, wenn channelType nicht in {2,3} liegt (Tor bei
            // 0x17A29A7) - erreicht den Server nie, hier nur zur Vollstaendigkeit.
            VOICE_CHAT_STATUS_UNSUPPORTED_CHANNEL_TYPE      = 19,

            // Der ehrliche Wert eines Realms ohne Sprachdienst: erkannt, feuert
            // VOICE_CHAT_ERROR, ohne dauerhaften Riegel, raeumt sauber ab. Siehe Kopfkommentar.
            VOICE_CHAT_STATUS_SERVICE_NOT_AVAILABLE         = 18
        };

        // Enum.ChatChannelType. Der Client laesst durch sein Sendetor NUR 2 und 3 hinaus
        // (Disassemblat 0x17A29A7: lea eax,[rbx-2] / cmp al,1 / jbe). Alles andere feuert
        // VOICE_CHAT_ERROR mit Statuscode 19, OHNE dass ein Paket den Client verlaesst.
        // UNVERIFIED: die Nummerierung 0 None / 1 Custom / 2 PrivateParty / 3 PublicParty /
        // 4 Communities stammt aus der Lua-Doku; aus dem Binary belegt ist nur, dass 2 und 3
        // die einzigen gesendeten Werte sind.
        enum ChatChannelType : uint8
        {
            CHAT_CHANNEL_TYPE_PRIVATE_PARTY = 2,
            CHAT_CHANNEL_TYPE_PUBLIC_PARTY  = 3
        };

        // 0x43013C, Writer 0x6AFDF0 - leere Nutzlast (Write<uint32>(opcode); return 1).
        // Sendestelle 0x19BAE00, zugleich der clientseitige Ratenbegrenzer: schlaegt er zu, feuert
        // der Client VOICE_CHAT_ERROR mit Statuscode 2, OHNE zu senden. Der Server sieht dann
        // nichts - das ist kein Ausbleiben einer Antwort, sondern ein Ausbleiben der Anfrage.
        class VoiceChatLogin final : public ClientPacket
        {
        public:
            explicit VoiceChatLogin(WorldPacket&& packet) : ClientPacket(CMSG_VOICE_CHAT_LOGIN, std::move(packet)) { }

            void Read() override { }
        };

        // 0x43013D, Writer 0x6AFED0 - bits<7> Len; FLUSH; char ChannelId[Len] OHNE NUL. 1..72 Byte.
        // Puffergroesse 71 aus der strnlen-Zaehlerkonstante gemessen; die Erzeugerstelle
        // (0x19C6DDC: mov eax, 0x46 / cmova) klemmt zusaetzlich auf 70 Zeichen + NUL. Der Server
        // prueft deshalb gegen 70, nicht gegen die 127, die bits<7> zuliesse.
        // STT ist Speech-to-Text: Lua C_VoiceChat.ActivateChannelTranscription(channelID).
        class VoiceChannelSttTokenRequest final : public ClientPacket
        {
        public:
            static constexpr std::size_t MaxChannelIdLength = 70;

            explicit VoiceChannelSttTokenRequest(WorldPacket&& packet) : ClientPacket(CMSG_VOICE_CHANNEL_STT_TOKEN_REQUEST, std::move(packet)) { }

            void Read() override;

            std::string ChannelId;
        };

        // 0x43013E, Writer 0x6AFFA0 - genau ein uint8, ohne Schiebeausdruck, also ein echtes Byte.
        class VoiceChatJoinChannel final : public ClientPacket
        {
        public:
            explicit VoiceChatJoinChannel(WorldPacket&& packet) : ClientPacket(CMSG_VOICE_CHAT_JOIN_CHANNEL, std::move(packet)) { }

            void Read() override;

            uint8 ChannelType = 0;
        };

        // 0x4502C8, Reader 0x606A60, Konsument 0x19BAB20.
        // uint8 Status; uint32 PlatformCode; bits<7> Len1; bits<6> Len2; bits<9> Len3; FLUSH;
        // dann die drei Zeichenketten, Laenge INKLUSIVE NUL. Minimal 8 Byte.
        // Der Konsument liest JEDES Feld - hier gibt es keine Fuellwerte.
        // UNVERIFIED: die Bedeutung der drei Zeichenketten (Anbieter-Host / Benutzerkennung /
        // Zugangsdaten) ist nicht bis zur SDK-Senke verfolgt.
        class VoiceLoginResponse final : public ServerPacket
        {
        public:
            explicit VoiceLoginResponse() : ServerPacket(SMSG_VOICE_LOGIN_RESPONSE, 8) { }

            WorldPacket const* Write() override;

            uint8 Status = VOICE_CHAT_STATUS_SERVICE_NOT_AVAILABLE;
            uint32 PlatformCode = 0;
            std::string Field1;
            std::string Field2;
            std::string Field3;
        };

        // 0x4502C9, Reader 0x606DD0 -> Rumpf 0x606C10, Konsument 0x19BABE0 -> 0x19B9640.
        // uint8 Status; uint32 PlatformCode; uint8 ChannelType; uint64; uint64; PackedGuid;
        // bits<7> Len1; bits<9> Len2; bits<7> Len3; FLUSH; drei Zeichenketten inkl. NUL.
        //
        // BEFUND, den keine Namensheuristik hergibt: die beiden uint64 und die ObjectGuid sind
        // FUELLWERTE. Der Konsument 0x19B9640 packt nur ChannelType um und setzt die beiden
        // Slots, die der nachgelagerte Nachschlag benutzt, ausdruecklich auf Null:
        //     v49.field_78 = *(_BYTE *)(a1 + 40);   // ChannelType
        //     v49.field_80 = 0;
        //     v49.field_88 = 0;
        // Die GUID wird nie angefasst. Sie werden nur gelesen, damit der Strom ausgerichtet bleibt.
        class VoiceChannelInfoResponse final : public ServerPacket
        {
        public:
            explicit VoiceChannelInfoResponse() : ServerPacket(SMSG_VOICE_CHANNEL_INFO_RESPONSE, 32) { }

            WorldPacket const* Write() override;

            uint8 Status = VOICE_CHAT_STATUS_SERVICE_NOT_AVAILABLE;
            uint32 PlatformCode = 0;
            uint8 ChannelType = 0;
            uint64 Field48 = 0;                 // vom Konsumenten NICHT gelesen
            uint64 Field56 = 0;                 // vom Konsumenten NICHT gelesen
            ObjectGuid Field64;                 // vom Konsumenten NICHT gelesen
            std::string Field80;
            std::string Field120;
            std::string Field160;
        };

        // 0x450311, Reader 0x60C070, Konsument 0x19BABD0 -> 0x19C6E40.
        // uint32 ErrorCode; bits<7> Len(ChannelId); bits<9> Len(Token); (16 Bit = 2 Byte, ohne
        // Fuellbits); dann beide Zeichenketten inkl. NUL. Minimal 6 Byte.
        // KEIN Statusbyte - anders als die beiden anderen Antworten beginnt diese mit uint32.
        //
        // ErrorCode ist ein FREIES uint32 ohne clientseitiges Enum: der Konsument vergleicht es
        // ausschliesslich gegen Null (`!*v20`) und gibt es sonst nur als %u in eine Debugzeile
        // ("OnChannelSttTokenError %u %u"). Jeder Wert ungleich 0 verhaelt sich identisch.
        //
        // ZWEI BEDINGUNGEN, ohne die der Client die Antwort STILL fallen laesst - kein Ereignis,
        // keine Meldung:
        //   * ChannelId muss nicht leer sein  (`if (a2[6])`)
        //   * ChannelId muss auf einen Kanal passen, den der Client schon kennt (`if (v8)`)
        // Deshalb spiegelt der Handler die ChannelId der Anfrage zurueck.
        class VoiceChannelSttTokenResponse final : public ServerPacket
        {
        public:
            explicit VoiceChannelSttTokenResponse() : ServerPacket(SMSG_VOICE_CHANNEL_STT_TOKEN_RESPONSE, 8) { }

            WorldPacket const* Write() override;

            uint32 ErrorCode = 0;
            std::string ChannelId;
            std::string Token;
        };
    }
}

#endif // TRINITYCORE_VOICE_CHAT_PACKETS_H

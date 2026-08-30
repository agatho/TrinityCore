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

// ================================================================================================
// Einheit w4_cmsg_43_3D - Block B9 "Voice-Chat", Phase A.
//
// Der Draht steht in Packets/VoiceChatPackets.{h,cpp}; hier steht die Wirkung.
//
// TrinityCore hat kein Sprach-Backend, und es soll hier auch keines erfinden. Der Punkt dieses
// Blocks ist ein anderer: der Client hat fuer "geht nicht" einen definierten, erkannten Weg, und
// den zu bedienen ist die vollstaendige Umsetzung - nicht ein Zugestaendnis. Dasselbe Muster wie
// CMSG_BONUS_ROLL auf feature/player-ui, das die definierte Fehlschlagsnachricht sendet, statt zu
// schweigen.
//
// Was Schweigen kostet: der Client haelt seinen Anmelde- und Beitrittszustand in
// byte_7FF785BDBF8D und einer Warteliste vom Typ blz::list_node<class ClientVoiceChannelInfoResponse
// const *>. Ohne Antwort wird die Warteliste nie abgearbeitet und der Zustand nie zurueckgesetzt.
// Mit Statuscode 18 laeuft LABEL_4 in 0x19B95B0, das genau diese Zustaende abraeumt.
//
// D3 ist damit erfuellt, nicht umgangen: der Fehlerpfad IST der einzige Pfad, den dieser Realm
// wahrheitsgemaess bedienen kann, und er ist der im Client vorgesehene.
//
// D4 - Persistenz: gegenstandslos. Es entsteht kein Serverzustand.
// ================================================================================================

#include "WorldSession.h"
#include "VoiceChatPackets.h"
#include "Log.h"

// 0x43013C -> 0x4502C8.
// Statuscode 18: erkannt (Sprungtabelle 0x19C6B9C), feuert VOICE_CHAT_ERROR, setzt KEINEN
// dauerhaften Riegel (nur 15 und 17 tun das) und raeumt den Anmeldeversuch sauber ab.
// Die drei Zeichenketten bleiben leer - Laenge 0, null Bytes, siehe die NUL-Falle im Header.
void WorldSession::HandleVoiceChatLogin(WorldPackets::VoiceChat::VoiceChatLogin& /*voiceChatLogin*/)
{
    WorldPackets::VoiceChat::VoiceLoginResponse response;
    response.Status = WorldPackets::VoiceChat::VOICE_CHAT_STATUS_SERVICE_NOT_AVAILABLE;
    response.PlatformCode = 0;

    TC_LOG_DEBUG("network.opcode", "CMSG_VOICE_CHAT_LOGIN from {} - no voice backend, answering with status {}",
        GetPlayerInfo(), uint32(response.Status));

    SendPacket(response.Write());
}

// 0x43013E -> 0x4502C9.
// Der Client laesst durch sein Sendetor (0x17A29A7) nur channelType 2 und 3 hinaus; alles andere
// feuert lokal VOICE_CHAT_ERROR mit Statuscode 19, ohne dass ein Paket hinausgeht. Ein anderer
// Wert am Draht kommt also nicht vom Retail-Client - er wird trotzdem geprueft, weil ein
// Angreifer nicht an dieses Tor gebunden ist.
// Der ChannelType wird in die Antwort zurueckgespiegelt: der Konsument 0x19B9640 ist das einzige
// Feld ausser Status und den Zeichenketten, das er ueberhaupt uebernimmt.
void WorldSession::HandleVoiceChatJoinChannel(WorldPackets::VoiceChat::VoiceChatJoinChannel& voiceChatJoinChannel)
{
    WorldPackets::VoiceChat::VoiceChannelInfoResponse response;
    response.ChannelType = voiceChatJoinChannel.ChannelType;
    response.PlatformCode = 0;

    if (voiceChatJoinChannel.ChannelType != WorldPackets::VoiceChat::CHAT_CHANNEL_TYPE_PRIVATE_PARTY
        && voiceChatJoinChannel.ChannelType != WorldPackets::VoiceChat::CHAT_CHANNEL_TYPE_PUBLIC_PARTY)
    {
        response.Status = WorldPackets::VoiceChat::VOICE_CHAT_STATUS_UNSUPPORTED_CHANNEL_TYPE;
        TC_LOG_DEBUG("network.opcode", "CMSG_VOICE_CHAT_JOIN_CHANNEL from {} with channelType {} which the retail client never sends",
            GetPlayerInfo(), voiceChatJoinChannel.ChannelType);
    }
    else
    {
        response.Status = WorldPackets::VoiceChat::VOICE_CHAT_STATUS_SERVICE_NOT_AVAILABLE;
        TC_LOG_DEBUG("network.opcode", "CMSG_VOICE_CHAT_JOIN_CHANNEL from {} channelType {} - no voice backend",
            GetPlayerInfo(), voiceChatJoinChannel.ChannelType);
    }

    SendPacket(response.Write());
}

// 0x43013D -> 0x450311.
// STT ist Speech-to-Text. Lua C_VoiceChat.ActivateChannelTranscription(channelID).
//
// Der Konsument 0x19C6E40 laesst die Antwort STILL fallen, wenn ChannelId leer ist oder auf
// keinen ihm bekannten Kanal passt - kein Ereignis, keine Meldung. Deshalb wird die ChannelId der
// Anfrage unveraendert zurueckgespiegelt; nur so erreicht die Absage ueberhaupt das UI.
// Bei ErrorCode != 0 (oder leerem Token) loescht der Client das Transkriptions-Flag des Kanals und
// feuert VOICE_CHAT_CHANNEL_TRANSCRIBING_CHANGED(channelID, false) - genau die richtige Aussage.
// ErrorCode ist ein freies uint32 ohne clientseitiges Enum; jeder Wert ungleich 0 wirkt gleich.
void WorldSession::HandleVoiceChannelSttTokenRequest(WorldPackets::VoiceChat::VoiceChannelSttTokenRequest& voiceChannelSttTokenRequest)
{
    // Puffergroesse 71, Erzeugerstelle klemmt auf 70 Zeichen. bits<7> liesse 127 zu - wer die
    // Schranke aus der Bitbreite raet, dimensioniert die Eingangsvalidierung fast doppelt zu weit.
    if (voiceChannelSttTokenRequest.ChannelId.length() > WorldPackets::VoiceChat::VoiceChannelSttTokenRequest::MaxChannelIdLength)
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_VOICE_CHANNEL_STT_TOKEN_REQUEST from {} with an over-long channel id ({} bytes)",
            GetPlayerInfo(), voiceChannelSttTokenRequest.ChannelId.length());
        return;
    }

    if (voiceChannelSttTokenRequest.ChannelId.empty())
    {
        // Der Client wuerde eine Antwort mit leerer ChannelId selbst verwerfen - sie zu senden
        // waere ein Paket ins Nichts.
        TC_LOG_DEBUG("network.opcode", "CMSG_VOICE_CHANNEL_STT_TOKEN_REQUEST from {} with an empty channel id", GetPlayerInfo());
        return;
    }

    WorldPackets::VoiceChat::VoiceChannelSttTokenResponse response;
    response.ChannelId = voiceChannelSttTokenRequest.ChannelId;
    response.ErrorCode = 1;             // freies uint32, nur "ungleich 0" ist bedeutungstragend
    // Token bleibt leer - Laenge 0, null Bytes.

    TC_LOG_DEBUG("network.opcode", "CMSG_VOICE_CHANNEL_STT_TOKEN_REQUEST from {} channel '{}' - no speech-to-text backend",
        GetPlayerInfo(), voiceChannelSttTokenRequest.ChannelId);

    SendPacket(response.Write());
}

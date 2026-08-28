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
// Einheit w4_cmsg_43_3D - Block B6 "Lobby-Matchmaker" (Plunderstorm / WoW Labs), Phase A.
//
// Der Draht steht in Packets/LobbyMatchmakerPackets.{h,cpp}; hier steht die Wirkung.
//
// TrinityCore hat keinen Matchmaker-Dienst, und dieser Block baut auch keinen. Er tut das, was
// der Client fuer diesen Fall vorsieht: er ANTWORTET ABLEHNEND, ueber genau die Kanaele, die der
// Client dafuer kennt. Der Unterschied zu Schweigen ist nicht kosmetisch - siehe unten.
//
// Zwei Kanaele, beide am Konsumenten ausgelesen, nicht aus Namen erschlossen:
//
//   SMSG_WOW_LABS_PARTY_ERROR (0x450333, bits<4>) traegt die Fehler ALLER Partei-Opcodes UND von
//   ENTER_QUEUE / ABANDON_QUEUE. Es gibt fuer sie kein eigenes Ergebnisfeld. Werte 7..15 werden
//   vom Konsumenten 0x22BEB0 STUMM verworfen (`cmp eax,6 / ja <ret>`) - das ist der Haengefall.
//
//   SMSG_LOBBY_MATCHMAKER_QUEUE_RESULT (0x450323, bits<3>) traegt den Warteschlangenzustand.
//   Status 0 und 7 fallen beim Konsumenten 0x2199170 durch den Epilog und tun LITERALLY NICHTS -
//   ebenfalls Haengefaelle. Status 6 (ERR_PLUNDERSTORM_CANNOT_QUEUE) ist der einzige Wert, der
//   eine sichtbare Ablehnung erzeugt und dabei inQueue auf 0 laesst: der case-6-Block ist woertlich
//   `mov ecx, 0x4A3; call 0x209ad90`, ohne einen Schreibzugriff auf den Warteschlangenzustand.
//
// WARUM DIE PARTEI-OPCODES 3 UND NICHT 4 BEKOMMEN: 3 ist PARTY_INVITE_INVALID, die Aussage
// "diese Einladung/Partei gibt es nicht" - richtig fuer einen Realm, der keine Lobbypartei fuehrt.
// 4 ist ENTER_QUEUE_FAILED und gehoert zur Warteschlange, nicht zur Partei.
//
// WAS DIESER BLOCK NICHT TUT: SMSG_LOBBY_MATCHMAKER_PARTY_INFO senden. Der vollstaendige
// Parteizustand ist gelesen (Reader 0x60CDE0, Element 232 Byte), aber ein Server ohne Partei hat
// nichts hineinzuschreiben, und ein leeres PARTY_INFO wuerde dem Client eine Partei melden, die
// es nicht gibt. Die Zuordnung "parteimutierendes CMSG -> PARTY_INFO" ist ausserdem nur
// STRUKTURELL begruendet: PARTY_INFO traegt kein Sequenzfeld, ein Echo ist am Draht gar nicht
// vorgesehen.
//
// D4 - Persistenz: FLUECHTIG, und ausdruecklich entschieden. Der Dienst lebt nur, solange die
// Lobby lebt; ein Warteschlangenplatz, der einen Realm-Neustart ueberlebt, waere ein Fehler.
// ================================================================================================

#include "WorldSession.h"
#include "LobbyMatchmakerPackets.h"
#include "Log.h"

namespace
{
void SendPartyError(WorldSession* session, WorldPackets::LobbyMatchmaker::WowLabsPartyError::ErrorType error)
{
    WorldPackets::LobbyMatchmaker::WowLabsPartyError packet;
    packet.Error = error;
    session->SendPacket(packet.Write());
}

void SendQueueResult(WorldSession* session, WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueResult::QueueStatus status)
{
    WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueResult packet;
    packet.Status = status;
    session->SendPacket(packet.Write());
}
}

// 0x43016B -> Fehlerkanal. Die Einladung kann nicht zugestellt werden, weil es keine Lobbypartei
// gibt, in die eingeladen werden koennte.
void WorldSession::HandleLobbyMatchmakerPartyInvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerPartyInvite& lobbyMatchmakerPartyInvite)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_PARTY_INVITE from {} target {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerPartyInvite.TargetGUID.ToString());

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x43016C. Es gibt keine Einladung, die angenommen werden koennte.
void WorldSession::HandleLobbyMatchmakerAcceptPartyInvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerAcceptPartyInvite& lobbyMatchmakerAcceptPartyInvite)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_ACCEPT_PARTY_INVITE from {} target {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerAcceptPartyInvite.TargetGUID.ToString());

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x43016D. Eine Ablehnung ist clientseitig folgenlos, wenn es die Einladung nicht gibt; der
// Fehlerkanal ist trotzdem die richtige Antwort, damit das UI nicht auf eine Bestaetigung wartet.
void WorldSession::HandleLobbyMatchmakerRejectPartyInvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerRejectPartyInvite& lobbyMatchmakerRejectPartyInvite)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_REJECT_PARTY_INVITE from {} target {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerRejectPartyInvite.TargetGUID.ToString());

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x43016E.
void WorldSession::HandleLobbyMatchmakerPartyUninvite(WorldPackets::LobbyMatchmaker::LobbyMatchmakerPartyUninvite& lobbyMatchmakerPartyUninvite)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_PARTY_UNINVITE from {} target {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerPartyUninvite.TargetGUID.ToString());

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x43016F, leere Nutzlast.
void WorldSession::HandleLobbyMatchmakerLeaveParty(WorldPackets::LobbyMatchmaker::LobbyMatchmakerLeaveParty& /*lobbyMatchmakerLeaveParty*/)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_LEAVE_PARTY from {} - no lobby matchmaker service", GetPlayerInfo());

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x430170, ein uint32 playlistEntryID. Das Feld gehoert in JamLobbyMatchmakerPartyInfo; ohne
// Partei gibt es nichts zu setzen.
void WorldSession::HandleLobbyMatchmakerSetPartyPlaylistEntry(WorldPackets::LobbyMatchmaker::LobbyMatchmakerSetPartyPlaylistEntry& lobbyMatchmakerSetPartyPlaylistEntry)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_SET_PARTY_PLAYLIST_ENTRY from {} playlist {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerSetPartyPlaylistEntry.PlaylistEntryID);

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x430171, ein Bit. Setzt isReady in JamLobbyMatchmakerPartyMemberInfo - ohne Partei gegenstandslos.
void WorldSession::HandleLobbyMatchmakerSetPlayerReady(WorldPackets::LobbyMatchmaker::LobbyMatchmakerSetPlayerReady& lobbyMatchmakerSetPlayerReady)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_SET_PLAYER_READY from {} ready {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerSetPlayerReady.IsReady);

    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::PARTY_INVITE_INVALID);
}

// 0x430173 -> SMSG_LOBBY_MATCHMAKER_QUEUE_RESULT mit Status 6.
// Status 6 ist der einzige, der eine sichtbare Ablehnung zeigt UND inQueue auf 0 laesst.
// Zusaetzlich der Fehlerkanal mit ENTER_QUEUE_FAILED: die beiden ergaenzen sich - das eine ist
// der Warteschlangenzustand, das andere die Fehlermeldung.
void WorldSession::HandleLobbyMatchmakerEnterQueue(WorldPackets::LobbyMatchmaker::LobbyMatchmakerEnterQueue& lobbyMatchmakerEnterQueue)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_ENTER_QUEUE from {} playlist {} character {} race {} class {} sex {} - no lobby matchmaker service",
        GetPlayerInfo(), lobbyMatchmakerEnterQueue.PlaylistEntryID,
        lobbyMatchmakerEnterQueue.CharacterGUID.ToString(),
        lobbyMatchmakerEnterQueue.Customization.RaceID,
        lobbyMatchmakerEnterQueue.Customization.ClassID,
        lobbyMatchmakerEnterQueue.Customization.SexID);

    SendQueueResult(this, WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueResult::CANNOT_QUEUE);
    SendPartyError(this, WorldPackets::LobbyMatchmaker::WowLabsPartyError::ENTER_QUEUE_FAILED);
}

// 0x430175, leere Nutzlast. Der Client will die Warteschlange verlassen. Er steht in keiner -
// aber der Zustand, den er annehmen soll, ist derselbe wie nach einem erfolgreichen Verlassen.
// Deshalb hier Status 3 (LEFT_QUEUE, ERR_LFG_LEFT_QUEUE) und NICHT der Fehlerkanal: der Client
// setzt damit inQueue = 0 und feuert LOBBY_MATCHMAKER_QUEUE_ABANDONED. Das ist die wahre Aussage
// "du stehst jetzt in keiner Warteschlange".
void WorldSession::HandleLobbyMatchmakerAbandonQueue(WorldPackets::LobbyMatchmaker::LobbyMatchmakerAbandonQueue& /*lobbyMatchmakerAbandonQueue*/)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_ABANDON_QUEUE from {} - no lobby matchmaker service", GetPlayerInfo());

    SendQueueResult(this, WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueResult::LEFT_QUEUE);
}

// 0x430174, ein Bit. Antwort auf SMSG_LOBBY_MATCHMAKER_QUEUE_PROPOSED, das dieser Realm nie
// sendet - die Nachricht ist also verwaist, wenn sie ankommt.
void WorldSession::HandleLobbyMatchmakerQueueProposalResponse(WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueProposalResponse& lobbyMatchmakerQueueProposalResponse)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_QUEUE_PROPSAL_RESPONSE from {} accept {} - no proposal was made by this realm",
        GetPlayerInfo(), lobbyMatchmakerQueueProposalResponse.Accept);

    SendQueueResult(this, WorldPackets::LobbyMatchmaker::LobbyMatchmakerQueueResult::CANNOT_QUEUE);
}

// 0x43017E und 0x430172 sind selbst ANTWORTEN - auf
// SMSG_LOBBY_MATCHMAKER_LOBBY_ACQUIRED_SERVER, dessen Konsument 0x22B990 ueber 0x1D55320 beide
// nacheinander sendet. Sie erwarten ihrerseits keine Antwort, und dieser Realm sendet die
// ausloesende Nachricht nie. Es wird deshalb nicht geantwortet - nicht aus Bequemlichkeit,
// sondern weil ein Rueckkanal im Protokoll nicht vorgesehen ist.
void WorldSession::HandleLobbyMatchmakerCreateCharacter(WorldPackets::LobbyMatchmaker::LobbyMatchmakerCreateCharacter& lobbyMatchmakerCreateCharacter)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_LOBBY_MATCHMAKER_CREATE_CHARACTER from {} ({}/{}) with {} customizations - this message is itself a reply, no counterpart exists",
        GetPlayerInfo(), lobbyMatchmakerCreateCharacter.Field32, lobbyMatchmakerCreateCharacter.Field33,
        lobbyMatchmakerCreateCharacter.Customizations.size());
}

void WorldSession::HandleRegisterFastLogin(WorldPackets::LobbyMatchmaker::RegisterFastLogin& registerFastLogin)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_REGISTER_FAST_LOGIN from {} doFastLogin {} to realm {} map {} from realm {} map {} - this message is itself a reply, no counterpart exists",
        GetPlayerInfo(), registerFastLogin.DoFastLogin,
        registerFastLogin.ToDestination.RealmAddress, registerFastLogin.ToDestination.MapID,
        registerFastLogin.FromDestination.RealmAddress, registerFastLogin.FromDestination.MapID);
}

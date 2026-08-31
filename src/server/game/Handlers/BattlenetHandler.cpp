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

#include "WorldSession.h"
#include "BattlenetPackets.h"
#include "WorldserverServiceDispatcher.h"
#include "ObjectDefines.h"
#include "Log.h"
#include "MiscPackets.h"

void WorldSession::HandleBattlenetChangeRealmTicket(WorldPackets::Battlenet::ChangeRealmTicket& changeRealmTicket)
{
    SetRealmListSecret(changeRealmTicket.Secret);

    WorldPackets::Battlenet::ChangeRealmTicketResponse realmListTicket;
    realmListTicket.Token = changeRealmTicket.Token;
    realmListTicket.Allow = true;
    realmListTicket.Ticket << "WorldserverRealmListTicket";

    SendPacket(realmListTicket.Write());
}

void WorldSession::HandleBattlenetRequest(WorldPackets::Battlenet::Request& request)
{
    sServiceDispatcher.Dispatch(this, request.Method.GetServiceHash(), request.Method.Token, request.Method.GetMethodId(), std::move(request.Data));
}

void WorldSession::SendBattlenetResponse(uint32 serviceHash, uint32 methodId, uint32 token, pb::Message const* response)
{
    WorldPackets::Battlenet::Response bnetResponse;
    bnetResponse.BnetStatus = ERROR_OK;
    bnetResponse.Method.Type = MAKE_PAIR64(methodId, serviceHash);
    bnetResponse.Method.ObjectId = 1;
    bnetResponse.Method.Token = token;

    if (int32 size = response->ByteSize(); size > 0)
    {
        bnetResponse.Data.resize(size);
        response->SerializePartialToArray(bnetResponse.Data.data(), size);
    }

    SendPacket(bnetResponse.Write());
}

void WorldSession::SendBattlenetRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request, std::function<void(MessageBuffer)> callback)
{
    _battlenetResponseCallbacks[_battlenetRequestToken] = std::move(callback);
    SendBattlenetRequest(serviceHash, methodId, request);
}

void WorldSession::SendBattlenetRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request)
{
    WorldPackets::Battlenet::Notification notification;
    notification.Method.Type = MAKE_PAIR64(methodId, serviceHash);
    notification.Method.ObjectId = 1;
    notification.Method.Token = _battlenetRequestToken++;

    if (int32 size = request->ByteSize(); size > 0)
    {
        notification.Data.resize(size);
        request->SerializePartialToArray(notification.Data.data(), size);
    }

    SendPacket(notification.Write());
}

// ================================================================================================
// Einheit w4_cmsg_43_3D - Block B4 "Bnet-Praesenz / Community", Phase A.
//
// Acht der neun B4-Opcodes; CMSG_REQUEST_CHAT_LOGIN liegt bei den uebrigen Misc-Opcodes, weil
// seine Paketklasse dort steht. CMSG_ACCOUNT_NOTIFICATION_ACKNOWLEDGED ist ABGEGEBEN an
// feature/bnet-presence - es ist die Quittung zu dem dort gebauten Abruf
// (HandleGetAccountNotifications, STATUS_AUTHED), und zwei Quittungswege waeren zwei Wahrheiten.
//
// Der Block hat eine Eigenheit, die die Abnahme bestimmt: SEINE ANTWORTEN LAUFEN GROSSTEILS NICHT
// UEBER DEN WELT-OPCODERAUM. Belegte Negativbefunde:
//  * CMSG_REQUEST_CHAT_LOGIN hat gar kein Welt-Gegenstueck. Beide Sendestellen (0x1C055F0 im
//    Charakterlisten-Konsumenten, 0x1D26CE0 beim Welteintritt) setzen konstant 1, und es gibt
//    KEINEN SMSG, dessen Konsument die beiden Zustandsflags byte_7FF7855FBB85 / ..86 liest oder
//    zuruecksetzt - der Zustand liegt vollstaendig im Client.
//  * CMSG_SEND_CHARACTER_CLUB_INVITATION wird ueber den Bnet-Kanal beantwortet. Sein uint32
//    stammt aus dem Bnet-RPC-Tokengenerator 0x34B67F0 (sieben Aufrufstellen, fuenf davon in
//    Funktionen, die CMSG_BATTLENET_REQUEST senden), der Client legt ihn als Schluessel in seine
//    Tabelle offener Einladungen, und SMSG_BATTLENET_RESPONSE benennt das passende Feld selbst
//    "token".
//  * CMSG_SAVE_ACCOUNT_DATA_EXPORT ist ein Einmalschuss ohne Antwort.
//  * Die beiden Gildenabfragen werden ueber Familie 0x51 beantwortet, nicht 0x45 - und deren
//    Dispatch laeuft ueber einen ZWEITEN Dispatcher (0x73A530) ganz ohne Thunk, bei 0x51002F ist
//    der Reader sogar inlined. Wer nur die Standardform sucht, haelt sie faelschlich fuer nicht
//    empfangbar.
//
// D4 - Persistenz: FLUECHTIG fuer das Praesenz-Abonnement (es gilt je Verbindung), sonst
// gegenstandslos, weil kein Serverzustand entsteht.
// ================================================================================================

// 0x430086, Writer 0x6A8AD0 -> Rumpf 0x6A8950.
// Gegenstueck SMSG_ADD_BATTLENET_FRIEND_RESPONSE (0x4500E6, Reader 0x5EA9B0):
//   uint64 inviteID; bits<5> result; bit hasValue; if (hasValue) uint32
// Der Konsument 0x2673670 bildet result auf GameError ab - fuenf belegte Klassen
// (879 ERR_BN_FRIEND_REQUEST_SENT als ERFOLGSmeldung, 1015 NOT_FOUND, 1016 NOT_VALID,
// 1017 NOT_ALLOWED, 1018 THROTTLED), result == 1 heisst "Einladung empfangen" und oeffnet
// CONFIRM_BATTLE_NET_FRIEND_INVITE_SHOW; 2,3,5,15,17,20..31 werden STUMM verworfen.
//
// Es wird bewusst NICHT geantwortet: TrinityCore hat keinen Battle.net-Freundesgraphen, und der
// einzige ehrliche Fehlercode waere einer der drei "not found/valid/allowed"-Klassen, deren
// genaue Zuordnung (welcher der je drei bis fuenf Rohwerte) nicht belegt ist. Ein geratener
// Fehlercode ist nach DoD Abschnitt 1 schlimmer als keine Antwort, weil der Client dafuer einen
// konkreten Text zeigt.
// UNVERIFIED: Bedeutung der Felder Field32 / Field44 / Field64 - Position und Breite sind am
// Writer belegt, die Semantik nicht.
void WorldSession::HandleAddBattlenetFriend(WorldPackets::Battlenet::AddBattlenetFriend& addBattlenetFriend)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_ADD_BATTLENET_FRIEND from {} target {} '{}'/'{}' - no battle.net friend graph in this tree",
        GetPlayerInfo(), addBattlenetFriend.PlayerGUID.ToString(),
        addBattlenetFriend.Field68, addBattlenetFriend.Field117);
}

// 0x430103, Writer 0x6ACD70 - getaggte Union, 5 oder 39 Byte.
// Kette: SMSG_BATTLENET_CHALLENGE_START (0x450239, Reader 0x5FE390: uint32 challengeID plus eine
// NEUN Bit breite Laenge und char[len] - die Challenge-URL, hoechstens 511 Zeichen)
//        -> CMSG_BATTLENET_CHALLENGE_RESPONSE (dieser Opcode)
//        -> SMSG_BATTLENET_CHALLENGE_ABORT (0x45023A, Reader 0x5FE470: uint32 challengeID plus
//           ein Bit, das der Konsument NICHT liest).
// Der Realm stellt keine Challenge, kann also auch keine beantwortet bekommen, die zu einer
// laufenden gehoert. Ohne SMSG_BATTLENET_CHALLENGE_START ist jede eingehende Antwort verwaist.
void WorldSession::HandleBattlenetChallengeResponse(WorldPackets::Battlenet::BattlenetChallengeResponse& battlenetChallengeResponse)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_BATTLENET_CHALLENGE_RESPONSE from {} challenge {} tag {} - no challenge was issued by this realm",
        GetPlayerInfo(), battlenetChallengeResponse.ChallengeID, battlenetChallengeResponse.Tag);
}

// 0x430126, Writer 0x6AF0E0 - bit Subscribe; uint32 Count; uint64 ClubIDs[Count].
// Lua C_Club.SetClubPresenceSubscription(clubId) bzw. ClearClubPresenceSubscription() (letzteres
// sendet dieselbe Nachricht mit Bit = 0 und Count = 0).
//
// Blizzards eigene Doku setzt die Obergrenze, und sie ist der Grund fuer die Pruefung hier:
// "You can only be subscribed to 0 or 1 clubs for presence." Der Serializer deckelt Count NICHT
// und liest ihn signed - die Schranke ist ausschliesslich serverseitig.
//
// Die Praesenzdaten kaemen ueber SMSG_BATCH_PRESENCE_SUBSCRIPTION (0x4502D9) zurueck. Das ist gut
// gestuetzt (gemeinsamer Speicher qword_7FF785CE8780, kein anderer Kandidat im Opcoderaum), bleibt
// aber VERMUTUNG, weil keine Anfrage-ID die beiden verknuepft - es wird deshalb nichts gesendet.
void WorldSession::HandleClubPresenceSubscribe(WorldPackets::Battlenet::ClubPresenceSubscribe& clubPresenceSubscribe)
{
    if (clubPresenceSubscribe.ClubIDs.size() > 1)
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_CLUB_PRESENCE_SUBSCRIBE from {} with {} clubs - the client may only subscribe to 0 or 1",
            GetPlayerInfo(), clubPresenceSubscribe.ClubIDs.size());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_CLUB_PRESENCE_SUBSCRIBE from {} subscribe {} clubs {} - no club presence service in this tree",
        GetPlayerInfo(), clubPresenceSubscribe.Subscribe, clubPresenceSubscribe.ClubIDs.size());
}

// 0x430128, Writer 0x6AF4A0 -> Rumpf 0x6AF2D0. ZWEI getrennte Bit-Sektionen mit je einem
// bits<9>-Laengenfeld; die beiden "u8" der Subplanzeile sind deshalb keine Felder.
// Beantwortet wird ueber den Bnet-Kanal (siehe Kopfkommentar), nicht ueber den Welt-Opcoderaum.
void WorldSession::HandleSendCharacterClubInvitation(WorldPackets::Battlenet::SendCharacterClubInvitation& sendCharacterClubInvitation)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SEND_CHARACTER_CLUB_INVITATION from {} token {} character {} - answered over the battle.net channel, not the world opcode space",
        GetPlayerInfo(), sendCharacterClubInvitation.Token, sendCharacterClubInvitation.CharacterGUID.ToString());
}

// 0x4300B8, Writer 0x6AA740 - guid + uint32.
// Lua C_StoreSecure.RequestCharacterGuildFollowInfo(guid, realmAddress) - Feld fuer Feld deckungs-
// gleich mit dem Draht. Gegenstueck SMSG_QUERY_GUILD_FOLLOW_INFO_RESPONSE liegt in Familie 0x51.
// Der Baum hat kein Gilden-Folgen-System (Guild Follow ist ein Shop-nahes Dienstmerkmal).
void WorldSession::HandleRequestCharacterGuildFollowInfo(WorldPackets::Battlenet::RequestCharacterGuildFollowInfo& requestCharacterGuildFollowInfo)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_REQUEST_CHARACTER_GUILD_FOLLOW_INFO from {} character {} realm {} - no guild follow service in this tree",
        GetPlayerInfo(), requestCharacterGuildFollowInfo.CharacterGUID.ToString(),
        requestCharacterGuildFollowInfo.VirtualRealmAddress);
}

// 0x43017B, Writer 0x6B22C0 - der uint32 der Subplanzeile IST der Listenzaehler, kein Skalar.
// Lua C_AccountServices.SaveAccountData(); der Client sendet in diesem Build immer Count == 0
// (die Auswahl trifft der Server), das Format traegt aber eine Liste.
// Gegenstueck SMSG_ACCOUNT_EXPORT_RESPONSE mit Enum.AccountExportResult (14 Werte: 0 Success,
// 1 UnknownError, 2 Cancelled, 3 ShuttingDown, 4 TimedOut, 5 NoAccountFound, ...,
// 10 Unavailable, ...). Es wird NICHT geantwortet: die Struktur des Gegenstuecks ist nicht
// nachgelesen, und der Opcode ist im Client ein Einmalschuss ohne wartenden Dialog - es haengt
// also nichts, wenn nichts kommt.
void WorldSession::HandleSaveAccountDataExport(WorldPackets::Battlenet::SaveAccountDataExport& saveAccountDataExport)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SAVE_ACCOUNT_DATA_EXPORT from {} for {} characters - no account export service in this tree",
        GetPlayerInfo(), saveAccountDataExport.Characters.size());
}

// 0x430196, Writer 0x6B2750 - uint32 RealmAddress, dann der Listenzaehler, dann die GUIDs.
// Lua C_StoreSecure.RequestRealmGuildMasterInfo(realmAddress) hat nur EIN Argument; die
// GUID-Liste stellt der Client selbst aus seiner Charakterliste zusammen.
// Gegenstueck SMSG_QUERY_REALM_GUILD_MASTER_INFO_RESPONSE liegt in Familie 0x51.
void WorldSession::HandleRequestRealmGuildMasterInfo(WorldPackets::Battlenet::RequestRealmGuildMasterInfo& requestRealmGuildMasterInfo)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_REQUEST_REALM_GUILD_MASTER_INFO from {} realm {} for {} characters - no cross realm guild master lookup in this tree",
        GetPlayerInfo(), requestRealmGuildMasterInfo.VirtualRealmAddress, requestRealmGuildMasterInfo.Characters.size());
}

// 0x430028, Writer 0x6A3600 - EIN Bit, nicht ein uint8.
// Methodische Falle, die hier wirklich zuschlaegt: an der Sendestelle steht "uint8 = 1" bei
// Objekt +0x20, am Writer 0x6A3600 aber ein eingebettetes Bit plus FlushBits. Das Rumpfbyte ist
// deshalb 0x80, nicht 0x01. Wer die Nutzlast an der Sendestelle abliest, bekommt das Objekt,
// nicht den Draht.
// Belegter Negativbefund: kein Welt-Gegenstueck (siehe Kopfkommentar). Die Chat-Anmeldung wird
// beantwortet, indem der Bnet-/Chatbaum ueber SMSG_BATTLENET_RESPONSE / SMSG_BATTLENET_NOTIFICATION
// mit Clubdaten versorgt wird - dieser Opcode hat dafuer keinen eigenen Rueckkanal.
void WorldSession::HandleRequestChatLogin(WorldPackets::Misc::RequestChatLogin& requestChatLogin)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_REQUEST_CHAT_LOGIN from {} login {} - no world counterpart exists, state lives entirely in the client",
        GetPlayerInfo(), requestChatLogin.Login);
}

void WorldSession::SendBattlenetResponse(uint32 serviceHash, uint32 methodId, uint32 token, uint32 status)
{
    WorldPackets::Battlenet::Response bnetResponse;
    bnetResponse.BnetStatus = BattlenetRpcErrorCode(status);
    bnetResponse.Method.Type = MAKE_PAIR64(methodId, serviceHash);
    bnetResponse.Method.ObjectId = 1;
    bnetResponse.Method.Token = token;

    SendPacket(bnetResponse.Write());
}


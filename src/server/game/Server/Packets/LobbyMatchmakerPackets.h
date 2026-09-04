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

#ifndef TRINITYCORE_LOBBY_MATCHMAKER_PACKETS_H
#define TRINITYCORE_LOBBY_MATCHMAKER_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"
#include "CharacterPackets.h"
#include <array>
#include <vector>

// ----------------------------------------------------------------------------------------------
// Einheit w4_cmsg_43_3D - Block B6 "Lobby-Matchmaker" (Plunderstorm / WoW Labs), Phase A.
// Build 12.1.0.69382, ImageBase 0x7FF780FD0000.
//
// Der Block ist protokollarm: sieben der zwoelf Nachrichten sind eine gepackte GUID, leer oder
// ein einzelnes Bit. Der Aufwand liegt im Dienst, nicht am Draht.
//
// KORREKTUR AN DER VORLAGE, ohne die dieser Block nicht baubar war: der gemeinsame Rumpf der
// Anpassungsdaten liegt bei RVA 0x6BEB30, NICHT bei 0x68EB30. Die Vorlage hatte sich beim
// Abziehen der Bildbasis verrechnet (IDA-Name sub_7FF78168EB30 minus 0x7FF780FD0000 = 0x6BEB30).
// Dateioffset 0x68EB30 liegt mitten in einer Instruktion - genau deshalb galt die Funktion als
// "nicht im Cache". Belegt am Aufruf: `006B2184  call 0x6beb30`. Dieselbe Verrechnung betrifft
// 0x6BEF20 (Rumpf von CMSG_REGISTER_FAST_LOGIN), 0x6DDC40, 0x6EEE10 und 0x6439D0.
//
// ZWEI STRUKTURBEFUNDE, die gegen die Arbeitshypothese laufen:
//
//  1. Die BEIDEN Anpassungslisten schreiben ihre Zaehler VORNE, hintereinander, und erst danach
//     kommen BEIDE Elementbloecke:
//         uint8 raceID; uint8 classID; uint8 sexID;
//         uint32 CountA; uint32 CountB;
//         { uint32 OptionID, uint32 ChoiceID } x CountA;
//         { uint32 OptionID, uint32 ChoiceID } x CountB;
//     NICHT "CountA, PaaraA, CountB, PaareB". Wer das so baut, verliert die Synchronisation bei
//     jeder nichtleeren Anpassung. Belegt an 0x6BEB76 / 0x6BEB82 (die beiden Write<uint32> stehen
//     direkt hintereinander) und unabhaengig an der LESESEITE 0x6BEC20, die spiegelbildlich zwei
//     Read<uint32> hintereinander ausfuehrt, bevor sie die Elementschleifen laeuft.
//
//  2. gameMode in JamFastLoginDestination ist ein uint8, KEIN uint32 (0x6BEF82:
//     `movzx edx, byte ptr [rdi+0x20]` -> Write<uint8> 0x35AFB40, ohne Schiebeausdruck und mit
//     leerem Akkumulator, also ein echtes Byte und keine getarnte Bitpackung).
//
//  3. CMSG_LOBBY_MATCHMAKER_CREATE_CHARACTER benutzt den gemeinsamen Rumpf NICHT. Es hat nur ZWEI
//     uint8 (kein drittes) und nur EINE Liste. Wer erwartet, dass es
//     JamFastLoginCharacterCustomization spiegelt, liegt falsch.
//
// Das Anpassungspaar { OptionID, ChoiceID } ist dasselbe, das der Baum bereits als
// WorldPackets::Character::ChrCustomizationChoice fuehrt - wiederverwendet, nicht neu gebaut.
// ----------------------------------------------------------------------------------------------

namespace WorldPackets
{
    namespace LobbyMatchmaker
    {
        // Gemeinsamer Rumpf, Serializer 0x6BEB30. 11 Byte fest + 8 * (CountA + CountB).
        // Keine Bits, kein Flush.
        struct CharacterCustomizationBlock
        {
            uint8 RaceID = 0;
            uint8 ClassID = 0;
            uint8 SexID = 0;
            Array<Character::ChrCustomizationChoice, 250> Customizations;
            Array<Character::ChrCustomizationChoice, 250> RandomCustomizations;
        };

        ByteBuffer& operator>>(ByteBuffer& data, CharacterCustomizationBlock& block);

        // Ein Ziel von CMSG_REGISTER_FAST_LOGIN, Client-Typ JamFastLoginDestination.
        struct FastLoginDestination
        {
            uint32 RealmAddress = 0;
            ObjectGuid CharacterGUID;
            uint8 GameMode = 0;                 // uint8, nicht uint32 - siehe Kopfkommentar
            uint32 MapID = 0;
            CharacterCustomizationBlock Customization;
        };

        ByteBuffer& operator>>(ByteBuffer& data, FastLoginDestination& destination);

        // 0x43016B / 0x43016C / 0x43016D / 0x43016E - Writer 0x6B1E70 / 0x6B1EC0 / 0x6B1F10 /
        // 0x6B1F60. Alle vier sind eine gepackte ObjectGuid, 2..18 Byte.
        class LobbyMatchmakerPartyInvite final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerPartyInvite(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_PARTY_INVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
        };

        class LobbyMatchmakerAcceptPartyInvite final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerAcceptPartyInvite(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_ACCEPT_PARTY_INVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
        };

        class LobbyMatchmakerRejectPartyInvite final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerRejectPartyInvite(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_REJECT_PARTY_INVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
        };

        class LobbyMatchmakerPartyUninvite final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerPartyUninvite(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_PARTY_UNINVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
        };

        // 0x43016F, Writer 0x6B1FB0 - literally `Write<uint32>(opcode); return 1;`.
        class LobbyMatchmakerLeaveParty final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerLeaveParty(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_LEAVE_PARTY, std::move(packet)) { }

            void Read() override { }
        };

        // 0x430175, Writer 0x6B2230 - ebenfalls leer.
        class LobbyMatchmakerAbandonQueue final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerAbandonQueue(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_ABANDON_QUEUE, std::move(packet)) { }

            void Read() override { }
        };

        // 0x430170, Writer 0x6B1FE0 - ein uint32.
        // playlistEntryID; dasselbe Feld traegt JamLobbyMatchmakerPartyInfo unter diesem Namen.
        class LobbyMatchmakerSetPartyPlaylistEntry final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerSetPartyPlaylistEntry(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_SET_PARTY_PLAYLIST_ENTRY, std::move(packet)) { }

            void Read() override;

            uint32 PlaylistEntryID = 0;
        };

        // 0x430171 / 0x430174, Writer 0x6B2030 / 0x6B21C0 - je ein Bit + FlushBits, 1 Byte.
        class LobbyMatchmakerSetPlayerReady final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerSetPlayerReady(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_SET_PLAYER_READY, std::move(packet)) { }

            void Read() override;

            bool IsReady = false;
        };

        class LobbyMatchmakerQueueProposalResponse final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerQueueProposalResponse(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_QUEUE_PROPSAL_RESPONSE, std::move(packet)) { }

            void Read() override;

            bool Accept = false;
        };

        // 0x430173, Writer 0x6B2150 - uint32; <Anpassungsblock>; PackedGuid. 17+8N .. 33+8N Byte.
        // UNVERIFIED: die Namen des fuehrenden uint32 und der abschliessenden GUID. Position und
        // Breite sind belegt, die Bedeutung nicht - es gibt fuer 0x430173 im ganzen Abbild nur
        // zwei Immediate-Stellen (Writer und GetOpcode-Stub), also keine benannte Aufrufstelle.
        class LobbyMatchmakerEnterQueue final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerEnterQueue(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_ENTER_QUEUE, std::move(packet)) { }

            void Read() override;

            uint32 PlaylistEntryID = 0;         // UNVERIFIED: Name geraten
            CharacterCustomizationBlock Customization;
            ObjectGuid CharacterGUID;           // UNVERIFIED: Name geraten
        };

        // 0x43017E, Writer 0x6B23E0 - uint8; uint8; uint32 Count; { uint32, uint32 } x Count.
        // 6 + 8*Count Byte. Benutzt den gemeinsamen Rumpf NICHT (siehe Kopfkommentar, Befund 3).
        // UNVERIFIED: welche der beiden uint8 Rasse und welche Geschlecht ist - die Struktur hat
        // kein Klassenfeld, die Zuordnung ist damit nicht aus der Stellung ableitbar.
        class LobbyMatchmakerCreateCharacter final : public ClientPacket
        {
        public:
            explicit LobbyMatchmakerCreateCharacter(WorldPacket&& packet) : ClientPacket(CMSG_LOBBY_MATCHMAKER_CREATE_CHARACTER, std::move(packet)) { }

            void Read() override;

            uint8 Field32 = 0;                  // UNVERIFIED: Rasse oder Geschlecht
            uint8 Field33 = 0;                  // UNVERIFIED: Rasse oder Geschlecht
            Array<Character::ChrCustomizationChoice, 250> Customizations;
        };

        // 0x430172, Writer 0x6B20B0 -> Rumpf 0x6BEF20.
        // bit doFastLogin; FLUSH; dann ZWEI FastLoginDestination (toDestination, fromDestination).
        // 45 .. 77+8N Byte.
        // Diese Nachricht ist selbst eine ANTWORT - auf SMSG_LOBBY_MATCHMAKER_LOBBY_ACQUIRED_SERVER
        // (Konsument 0x22B990 -> 0x1D55320 sendet sie zusammen mit
        // CMSG_LOBBY_MATCHMAKER_CREATE_CHARACTER). Sie erwartet ihrerseits keine Antwort.
        class RegisterFastLogin final : public ClientPacket
        {
        public:
            explicit RegisterFastLogin(WorldPacket&& packet) : ClientPacket(CMSG_REGISTER_FAST_LOGIN, std::move(packet)) { }

            void Read() override;

            bool DoFastLogin = false;
            FastLoginDestination ToDestination;
            FastLoginDestination FromDestination;
        };

        // ------------------------------------------------------------------------------------
        // Die Antwortseite - der Fehlerkanal dieses Blocks heisst NICHT, wie man denkt.
        // ------------------------------------------------------------------------------------

        // 0x450333, Reader 0x60DE70, Konsument 0x22BEB0. Genau EIN Feld: bits<4> ErrorType,
        // 1 Byte Nutzlast. Der Reader liest ein ganzes Byte und nimmt `b >> 4`.
        //
        // DAS IST DER FEHLERKANAL FUER DEN GANZEN BLOCK - fuer alle Partei-CMSG 0x43016B..0x430171
        // UND fuer ENTER_QUEUE / ABANDON_QUEUE. Wer B6 baut und ihn nicht bedient, hat ein System
        // ohne jede Fehlermeldung.
        //
        // Wertetabelle, aus der Sprungtabelle bei 0x22BF0C gelesen (nicht aus dem Dekompilat
        // uebernommen), mit den GameError-IDs aus der Anzeige 0x209AD90:
        //   0 -> 1180 ERR_WOW_LABS_PARTY_ERROR_TYPE_PARTY_IS_FULL
        //   1 -> 1181 ERR_WOW_LABS_PARTY_ERROR_TYPE_MAX_INVITE_SENT
        //   2 -> 1182 ERR_WOW_LABS_PARTY_ERROR_TYPE_PLAYER_ALREADY_INVITED
        //   3 -> 1183 ERR_WOW_LABS_PARTY_ERROR_TYPE_PARTY_INVITE_INVALID
        //   4 -> 1184 ERR_WOW_LABS_LOBBY_MATCHMAKER_ERROR_ENTER_QUEUE_FAILED
        //   5 -> 1185 ERR_WOW_LABS_LOBBY_MATCHMAKER_ERROR_LEAVE_QUEUE_FAILED
        //   6 -> 1184 (mit 4 zusammengelegt - beide Werte sind am Draht gueltig)
        //   7..15 -> `cmp eax,6 / ja` -> STUMM verworfen, kein Text, kein Ereignis.
        // Die 7..15 sind der Haengefall: nie senden.
        class WowLabsPartyError final : public ServerPacket
        {
        public:
            enum ErrorType : uint8
            {
                PARTY_IS_FULL           = 0,
                MAX_INVITE_SENT         = 1,
                PLAYER_ALREADY_INVITED  = 2,
                PARTY_INVITE_INVALID    = 3,
                ENTER_QUEUE_FAILED      = 4,
                LEAVE_QUEUE_FAILED      = 5
            };

            explicit WowLabsPartyError() : ServerPacket(SMSG_WOW_LABS_PARTY_ERROR, 1) { }

            WorldPacket const* Write() override;

            uint8 Error = PARTY_INVITE_INVALID;
        };

        // 0x450323, Reader 0x60D1A0, Konsument 0x2199170. Ein Feld: bits<3> Status, 1 Byte.
        //
        // Wertetabelle aus der Sprungtabelle bei 0x219938C (`dec eax; cmp eax,5; ja <Epilog>`):
        //   0     -> STUMM (faellt durch den Epilog) - der Haengefall
        //   1, 2  -> inQueue = 1, 840 ERR_QUEUED_PLUNDERSTORM, LOBBY_MATCHMAKER_QUEUE_STATUS_UPDATE
        //   3     -> inQueue = 0, 841 ERR_LFG_LEFT_QUEUE, LOBBY_MATCHMAKER_QUEUE_ABANDONED
        //   4     -> inQueue = 0, LOBBY_MATCHMAKER_QUEUE_EXPIRED, keine Meldung
        //   5     -> Uebergang in die Lobby, keine Meldung
        //   6     -> 1187 ERR_PLUNDERSTORM_CANNOT_QUEUE, RUEHRT KEINEN ZUSTAND AN
        //   7     -> STUMM
        //
        // 6 ist damit der einzige Wert, der eine sichtbare Ablehnung erzeugt UND inQueue auf 0
        // laesst: der case-6-Block ist woertlich nur `mov ecx, 0x4A3; call 0x209ad90`, ohne einen
        // einzigen Schreibzugriff auf [rdi+8] / [rdi+0x10]. Genau das braucht ein Realm ohne
        // Matchmaker.
        //
        // ⚠ LOBBY_MATCHMAKER_QUEUE_ERROR (0xA9CDB719C08D66DE) ist als Ereignisname registriert,
        // wird aber an KEINER Stelle im Abbild ausgeloest. Wer diesen Namen bedient, bedient ein
        // totes Ereignis. Und LOBBY_MATCHMAKER_QUEUE_POPPED kommt nicht von hier, sondern aus
        // SMSG_LOBBY_MATCHMAKER_QUEUE_PROPOSED.
        class LobbyMatchmakerQueueResult final : public ServerPacket
        {
        public:
            enum QueueStatus : uint8
            {
                QUEUED          = 1,
                QUEUED_ALT      = 2,
                LEFT_QUEUE      = 3,
                QUEUE_EXPIRED   = 4,
                LOBBY_ACQUIRED  = 5,
                CANNOT_QUEUE    = 6
            };

            explicit LobbyMatchmakerQueueResult() : ServerPacket(SMSG_LOBBY_MATCHMAKER_QUEUE_RESULT, 1) { }

            WorldPacket const* Write() override;

            uint8 Status = CANNOT_QUEUE;
        };

        // SMSG_LOBBY_MATCHMAKER_RECEIVE_INVITE (0x450321). Byte-aligned flat wire (parser sub_7FF7290B8440):
        // PackedGuid inviter, then a u8 length byte where the display-name length is the high 6 bits
        // (len = byte >> 2, low 2 bits unused here), then that many raw name bytes (no NUL). Drives the client
        // event NEW_MATCHMAKING_PARTY_INVITE.
        class LobbyMatchmakerReceiveInvite final : public ServerPacket
        {
        public:
            explicit LobbyMatchmakerReceiveInvite() : ServerPacket(SMSG_LOBBY_MATCHMAKER_RECEIVE_INVITE, 24) { }

            WorldPacket const* Write() override;

            ObjectGuid InviterGuid;
            std::string InviterName;
        };

        // SMSG_LOBBY_MATCHMAKER_PARTY_INVITE_REJECTED (0x450320). Parser sub_7FF7290B8390: just the rejector's
        // name as a u8 (len = byte >> 2) + raw bytes. No guid. Drives REJECTED_MATCHMAKING_PARTY_INVITE{name}.
        class LobbyMatchmakerPartyInviteRejected final : public ServerPacket
        {
        public:
            explicit LobbyMatchmakerPartyInviteRejected() : ServerPacket(SMSG_LOBBY_MATCHMAKER_PARTY_INVITE_REJECTED, 8) { }

            WorldPacket const* Write() override;

            std::string Name;
        };

        // SMSG_LOBBY_MATCHMAKER_PARTY_INFO (0x45031F). The lobby roster broadcast; drives LOBBY_MATCHMAKER_
        // PARTY_UPDATE. Byte-aligned flat wire (outer parser sub_7FF7290B81B0, member parser sub_7FF7291CCED0):
        //   PackedGuid Leader; u32 PlaylistEntry; u32 count(Members); u32 count(Invited); PackedGuid; PackedGuid;
        //   u8 flagByte {bits7:6=uint2, bit5, bit4, bit3}; Members[]; Invited[]
        // Each 232-byte member (in wire order):
        //   u8 M {nameLen = M>>2, bit1 = a bool}; PackedGuid; PackedGuid; u64; u8; u8; u32 countA;
        //   u32[19] loadout (PlunderstormItemDisplayID cosmetic - structure certain, label inferred);
        //   u32 countB; name[nameLen]; countA*{u32,u32}; countB*{u32,u32}
        // The two 232B lists are structurally identical; list1 = confirmed members, list2 = pending invitees
        // (inferred from the Lua GetCurrentParty vs GetPartyInvite split). The three member bools map to
        // isReady/isPartyLeader/isLocalPlayer (exact assignment not offline-provable). P0 sends the members
        // with the clear fields (guid, name, ready/leader/local) and leaves the cosmetic loadout + sub-lists 0.
        struct LobbyMatchmakerPartyInfoMember
        {
            ObjectGuid MemberGuid;                        // partyMemberGUID (first member guid)
            ObjectGuid AccountGuid;                       // second member guid (bnet/account)
            uint64 Field88 = 0;
            uint8 Field97 = 0;                             // one of isPartyLeader / isLocalPlayer
            uint8 Field98 = 0;                             // the other of the two
            bool ReadyBit = false;                        // M & 2
            std::string Name;
            std::array<uint32, 19> Loadout = { };         // PlunderstormItemDisplayID (cosmetic; 0 for now)
        };

        class LobbyMatchmakerPartyInfo final : public ServerPacket
        {
        public:
            explicit LobbyMatchmakerPartyInfo() : ServerPacket(SMSG_LOBBY_MATCHMAKER_PARTY_INFO, 64) { }

            WorldPacket const* Write() override;

            ObjectGuid LeaderGuid;
            uint32 PlaylistEntry = 0;
            ObjectGuid Guid3;                             // two outer guids of unproven role - left empty
            ObjectGuid Guid4;
            uint8 FlagByte = 0;
            std::vector<LobbyMatchmakerPartyInfoMember> Members;
            std::vector<LobbyMatchmakerPartyInfoMember> Invited;
        };

        // SMSG_LOBBY_MATCHMAKER_QUEUE_PROPOSED (0x45031C / wire 0x420320). The queue "pop": drives the client
        // event LOBBY_MATCHMAKER_QUEUE_POPPED, to which the client answers CMSG_..QUEUE_PROPSAL_RESPONSE{Accept}.
        // The body is one of the runtime-dispatched WoW-Labs messages whose exact framing is NOT statically
        // recoverable (opcode dwords are not immediates; needs a dynamic capture). The response correlates by
        // session, so P2 sends the pop with no modelled fields; a proposal id / countdown is added once captured.
        class LobbyMatchmakerQueueProposed final : public ServerPacket
        {
        public:
            explicit LobbyMatchmakerQueueProposed() : ServerPacket(SMSG_LOBBY_MATCHMAKER_QUEUE_PROPOSED, 4) { }

            WorldPacket const* Write() override;
        };

        // SMSG_LOBBY_MATCHMAKER_LOBBY_ACQUIRED_SERVER (0x45031E / wire 0x42031c). Sent once a proposal is fully
        // accepted: tells the client which server/instance to fast-login to (it then ForceLogout()s). The client
        // handler reads a dword at object offset +44; the fields below are the inferred fast-login target
        // (~ FastLoginDestination: realm address, a token, game mode, map id) and are populated best-effort in
        // P2, finalised in P3 when the MAP_WOWLABS instance handoff is built. Field framing is inferred.
        class LobbyMatchmakerLobbyAcquiredServer final : public ServerPacket
        {
        public:
            explicit LobbyMatchmakerLobbyAcquiredServer() : ServerPacket(SMSG_LOBBY_MATCHMAKER_LOBBY_ACQUIRED_SERVER, 16) { }

            WorldPacket const* Write() override;

            uint32 RealmAddress = 0;
            uint32 Token = 0;
            uint8 GameMode = 0;
            uint32 MapId = 0;
        };
    }

    // --------------------------------------------------------------------------------------
    // Einheit w4_cmsg_43_3D - Block B7 "WoW Labs / Plunderstorm", Phase A.
    //
    // DIESER BLOCK GALT ALS DOPPELT UNSICHER (Wert UND Zuordnung) und ist es nicht. Der Brief
    // stellt in Abschnitt 9.7 W1 einen Widerspruch fest - "SelectWoWLabsArea(areaID) nimmt eine
    // kleine Ganzzahl, der Serializer schreibt 4 x uint64 plus zwei Zeichenketten" - und
    // erwaegt einen Namensversatz um eine Position. Aufgeloest: der Versatz betraegt ELF
    // Positionen, und er ist kein Versatz, sondern eine Luecke in der Analyseliste.
    //
    //   0x3D02EE / EF / F0  sind C_Discord.GuildLink / GuildUnlink / SetGuildSetting
    //                       (Usage-Strings im Abbild), 0x3D02F1 eine unbenannte
    //                       Voice-/Discord-Kanalnachricht (RTTI VoiceDiscordChannel /
    //                       VoiceDiscordGuild an beiden Sendestellen).
    //   0x3D02FA / FB / FC  sind die WoW-Labs-Opcodes - IDENTISCH zu den Baumwerten.
    //
    // Die Discord-Opcodes stehen also bereits im 69382-Abbild; die Behauptung des Briefs
    // (Abschnitt 2 K1), sie seien erst in 69404 eingefuegt worden und haetten alles dahinter um
    // elf Plaetze verschoben, ist damit widerlegt. Ein Durchlauf ueber alle 2698
    // Opcode-Getter-Thunks des Abbilds reproduziert die Nummerierung des Baums.
    // Jede Bindung unten ist DOPPELT belegt: Immediate im Writer UND Vtable-Getter (+0x18).
    //
    // WAS DIE UMSETZUNG BESTIMMT: alle drei Sendestellen sind auf eine Spielregel gattert.
    //   QuerySelectedWoWLabsArea @0x17E08F0, QueryWoWLabsAreaInfo @0x17E0D86,
    //   SelectWoWLabsArea @0x17E11F0 - jeweils "mov ecx, 0xE; call 0x599860; test al,al; je"
    // 0x599860 ist C_GameRules.IsGameRuleActive (belegt ueber das Lua-Binding 0xF78F70), und
    // Regel 14 ist GameRule::CharacterlessLogin (Enum-Registrierung 0xF7B938, deckungsgleich mit
    // DBCEnums.h in diesem Baum). TrinityCore setzt in World.cpp nur TransmogEnabled und
    // HousingEnabled - Regel 14 ist NIE aktiv. Der Retail-Client sendet diese drei Opcodes auf
    // einem unveraenderten TC-Realm also gar nicht, und es gibt weder eine C++-Wartezustands-
    // struktur noch eine Rueckrufregistrierung in den drei Sendern: es kann nichts haengen.
    // Gebaut werden sie trotzdem vollstaendig - fuer den Fall, dass die Regel gesetzt wird.
    // --------------------------------------------------------------------------------------

    namespace WowLabs
    {
        // 0x3D02FA, Writer 0x6D3810, Vtable 0x3C01488, Getter 0x6D3850. PackedGuid - 2..18 Byte.
        // Lua C_WowLabsDataManager.QuerySelectedWoWLabsArea() (ohne Argument).
        class QuerySelectedWowLabsArea final : public ClientPacket
        {
        public:
            explicit QuerySelectedWowLabsArea(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_SELECTED_WOW_LABS_AREA, std::move(packet)) { }

            void Read() override;

            ObjectGuid PlayerGUID;
        };

        // 0x3D02FB, Writer 0x6D3860, Vtable 0x3C014B0, Getter 0x6D38A0. PackedGuid - 2..18 Byte.
        // Lua C_WowLabsDataManager.QueryWoWLabsAreaInfo() (ohne Argument).
        // Der Subplan fuehrt hier "u8" - das war die Nutzlast des Discord-Opcodes 0x3D02F0.
        class QueryWowLabsAreaInfo final : public ClientPacket
        {
        public:
            explicit QueryWowLabsAreaInfo(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_WOW_LABS_AREA_INFO, std::move(packet)) { }

            void Read() override;

            ObjectGuid PlayerGUID;
        };

        // 0x3D02FC, Writer 0x6D38B0, Vtable 0x3C01438, Getter 0x6D3900.
        // PackedGuid + uint32 WowLabsAreaID - 6..22 Byte.
        // Lua C_WowLabsDataManager.SelectWoWLabsArea(wowLabsAreaID); die Glue 0x17E11F0 legt die
        // 16-Byte-GUID auf Objekt +0x20 und das Lua-Argument auf +0x30 - Feld fuer Feld auf den
        // Writer passend. Der Subplan fuehrte hier "u64*4 + 2 Strings"; das war 0x3D02F1.
        class SelectWowLabsArea final : public ClientPacket
        {
        public:
            explicit SelectWowLabsArea(WorldPacket&& packet) : ClientPacket(CMSG_SELECT_WOW_LABS_AREA, std::move(packet)) { }

            void Read() override;

            ObjectGuid PlayerGUID;
            uint32 WowLabsAreaID = 0;
        };

        // 0x450328, Reader 0x60D440, Konsument 0x22BAC0. bits<1> Success - genau 1 Byte.
        //
        // Der EINZIGE Fehlerkanal dieses Blocks, und der Beleg ist eindeutig: GameError 1186
        // (ERR_WOW_LABS_SET_WOW_LABS_AREA_ID_FAILED) kommt im GESAMTEN Abbild genau einmal vor -
        // bei RVA 0x22BAED, in genau diesem Konsumenten.
        //
        //   0022BAC4  cmp byte ptr [rcx+0x20], 0     ; das Erfolgsbit
        //   0022BAC8  jne 0x22BAFB                   ; Erfolg -> schlichtes return, NICHTS
        //   0022BAE0  movabs rcx, 0x4428E114E69FF111 ; SELECT_WOW_LABS_AREA_FAILED
        //   0022BAED  mov ecx, 0x4A2                 ; 1186
        //   0022BAF6  jmp 0x209AD90                  ; GameError-Anzeige
        //
        // ERFOLG IST EIN STILLES NO-OP. Wer hier "Erfolg" zurueckmeldet, sagt dem Client nichts
        // und laesst die Auswahl im angefragten Zustand haengen. Ein echter Erfolg braucht ZWEI
        // Pakete: dieses mit Success = true, DANN SMSG_WOW_LABS_AREA_SELECTED mit der
        // bestaetigten ID.
        class WowLabsSetWowLabsAreaIdResponse final : public ServerPacket
        {
        public:
            explicit WowLabsSetWowLabsAreaIdResponse() : ServerPacket(SMSG_WOW_LABS_SET_WOW_LABS_AREA_ID_RESPONSE, 1) { }

            WorldPacket const* Write() override;

            bool Success = false;
        };

        // 0x450329 / 0x45032B, Reader 0x60D4B0 / 0x60D640 - byteweise gleich geformt, gleicher
        // Konsument 0x22BB00, gleiche Globale 0x43B7C50, gleiches Lua-Ereignis
        // WOW_LABS_AREA_SELECTED. int32 WowLabsAreaID - genau 4 Byte.
        //
        // ⚠ Der Reader nimmt den REST des Pakets als Zeiger (0x35AF730), OHNE Laengenpruefung -
        // die kann bauartbedingt nicht fehlschlagen. Bei leerer Nutzlast liest der Konsument vier
        // Byte HINTER dem Paketpuffer. Es sind deshalb immer genau 4 Byte zu senden.
        //
        // 0 ist die Marke "nichts ausgewaehlt": GetConfirmedWoWLabsArea (0x17DEF00) prueft
        // "test eax, eax; je" und gibt bei 0 Lua-nil zurueck, was RefreshAllData als echten
        // Zustand behandelt.
        //
        // UNVERIFIED: welcher der beiden Werte 0x450329 und 0x45032B welchen NAMEN traegt, ist
        // aus diesem Abbild nicht entscheidbar - die beiden sind strukturell und funktional
        // ununterscheidbar, und der Client haelt keine Namen fuer SMSG-Opcodes. Die Zuordnung
        // hier folgt Opcodes.h. Funktional ist sie ohne Belang.
        class QuerySelectedWowLabsAreaResponse final : public ServerPacket
        {
        public:
            explicit QuerySelectedWowLabsAreaResponse() : ServerPacket(SMSG_QUERY_SELECTED_WOW_LABS_AREA_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            int32 WowLabsAreaID = 0;            // 0 == nichts ausgewaehlt
        };

        // 0x45032B - die BESTAETIGUNG einer gesetzten Absprungzone, zweites Paket des
        // Erfolgspfads von CMSG_SELECT_WOW_LABS_AREA. Diesen Pfad geht der Baum nicht
        // (WowLabsHandler.cpp antwortet Success = false), es gibt hier also KEINE Sendestelle.
        // Gebaut und in Opcodes.cpp auf STATUS_NEVER freigegeben ist sie trotzdem, damit der
        // spaetere Erfolgspfad nicht stumm in der SendPacket-Sperre verschwindet. D3 offen.
        class WowLabsAreaSelected final : public ServerPacket
        {
        public:
            explicit WowLabsAreaSelected() : ServerPacket(SMSG_WOW_LABS_AREA_SELECTED, 4) { }

            WorldPacket const* Write() override;

            int32 WowLabsAreaID = 0;
        };

        // Element von SMSG_QUERY_WOW_LABS_AREA_INFO_RESPONSE, 20 Byte, Client-Typ
        // WoWLabsAreaOption. Die Feldnamen sind KEINE Vermutung: sie stammen aus dem
        // Lua-Marshaller 0x17E8800 (lua_setfield-Namen plus die jeweilige Push-Hilfsfunktion,
        // die die Typen unterscheidet - cvtdq2pd fuer int32, cvtps2pd fuer float32).
        struct WowLabsAreaOption
        {
            int32 WowLabsAreaID = 0;            // +0x00
            int32 AreaType = 0;                 // +0x04, Enum.WoWLabsAreaType
            float X = 0.0f;                     // +0x08
            float Y = 0.0f;                     // +0x0C
            float Z = 0.0f;                     // +0x10 - vom ausgelieferten UI nicht benutzt
        };

        // 0x45032A, Reader 0x60D530, Konsument 0x22BB40 -> Lua WOW_LABS_AREA_INFO_UPDATED.
        // uint32 Count; Element[Count] - 4 + 20*Count Byte.
        // Der Client deckelt Count NICHT (resize rechnet 20 * (int)n ohne Klemmung) - die
        // Schranke ist rein serverseitig.
        class QueryWowLabsAreaInfoResponse final : public ServerPacket
        {
        public:
            explicit QueryWowLabsAreaInfoResponse() : ServerPacket(SMSG_QUERY_WOW_LABS_AREA_INFO_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            std::vector<WowLabsAreaOption> Areas;
        };

        // 0x450327 - broadcast to the players of a match when its phase changes. The wire is a single uint32
        // state (clean-exe: the match-state consumer branches on one dword; state == 3 is the pre-match / area-
        // selection phase, GetConfirmedWoWLabsArea gates on it). The other phase values are not decidable from
        // the image and are marked provisional where they are used server-side.
        class WowLabsNotifyPlayersMatchStateChanged final : public ServerPacket
        {
        public:
            explicit WowLabsNotifyPlayersMatchStateChanged() : ServerPacket(SMSG_WOW_LABS_NOTIFY_PLAYERS_MATCH_STATE_CHANGED, 4) { }

            WorldPacket const* Write() override;

            uint32 State = 0;
        };

        // One row of the end-of-match summary. Field SET is authoritative from Blizzard's own API doc
        // (EndOfMatchUIDocumentation.lua: MatchDetail { type: MatchDetailType, value: number }); the concrete
        // wire type of 'value' (int here) is a best-effort - see the packet comment.
        struct MatchDetail
        {
            uint32 Type = 0;                    // MatchDetailType { Placement=0, Kills=1, PlunderAcquired=2 }
            int32 Value = 0;
        };

        // 0x450326 - the per-player end-of-match summary that feeds C_EndOfMatchUI.GetEndOfMatchDetails() and
        // fires SHOW_END_OF_MATCH_UI. Wire RE-confirmed from the client: GetEndOfMatchDetails (clean-exe
        // 0x140E1E7C0) reads the stored result as matchType@+12 (int32), matchEnded@+16 (bool), detailsList@+24
        // (8-byte MatchDetail { int32 type, int32 value }), in that order - matching EndOfMatchUIDocumentation.lua.
        // The JAM bool (bit) and array (uint32 count) conventions match the sibling area packets in this file.
        class WowLabsNotifyPlayersMatchEnd final : public ServerPacket
        {
        public:
            explicit WowLabsNotifyPlayersMatchEnd() : ServerPacket(SMSG_WOW_LABS_NOTIFY_PLAYERS_MATCH_END, 16) { }

            WorldPacket const* Write() override;

            uint32 MatchType = 1;               // EndOfMatchType { None=0, Plunderstorm=1 }
            bool MatchEnded = true;
            std::vector<MatchDetail> Details;
        };

        // 0x45032C - the storm "prediction" circle. RE-recovered (not a guess): the framed message object is
        // 48 bytes and equals struct WowLabsDataBR::CircleData; its front 16 bytes are an ObjectGuid (the client
        // builder gates on the guid type bits, (guid.hi >> 58) < 0x3A), followed by two Vector3 centres and two
        // radii - the current ring and the predicted (next) ring the client interpolates between locally. Wire:
        // PackedGuid + 8 floats, in member order.
        class WowLabsSetPredictionCircle final : public ServerPacket
        {
        public:
            explicit WowLabsSetPredictionCircle() : ServerPacket(SMSG_WOW_LABS_SET_PREDICTION_CIRCLE, 8 + 32) { }

            WorldPacket const* Write() override;

            ObjectGuid CircleGuid;
            float CenterCurrentX = 0.0f, CenterCurrentY = 0.0f, CenterCurrentZ = 0.0f;
            float CenterNextX = 0.0f, CenterNextY = 0.0f, CenterNextZ = 0.0f;
            float RadiusCurrent = 0.0f;
            float RadiusNext = 0.0f;
        };
    }
}

#endif // TRINITYCORE_LOBBY_MATCHMAKER_PACKETS_H

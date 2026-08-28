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

#ifndef TRINITYCORE_LIVE_REGION_PACKETS_H
#define TRINITYCORE_LIVE_REGION_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"
#include <string>
#include <vector>

// ----------------------------------------------------------------------------------------------
// Einheit w4_cmsg_43_3D - Block B5 "Live-Region-Kopie", Phase A.
//
// Vier Anfragen desselben Zuschnitts. Feldfolgen aus den Client-Serializern des Builds
// 12.1.0.69382 (ImageBase 0x7FF780FD0000); die Zuordnung Serializer <-> Opcode ist ueber
// vtbl[1]/vtbl[3] hart belegt, nicht aus Namensaehnlichkeit erschlossen:
//
//   0x4300E5  vtbl 0x7FF784BC8FC8  Writer 0x6AB3D0  Huelle 0x6AB530
//   0x4300E6  vtbl 0x7FF784BC8F28  Writer 0x6AB580  Huelle 0x6AB7B0
//   0x4300E7  vtbl 0x7FF784BC8F78  Writer 0x6AB800  Huelle 0x6AB980
//   0x4300E8  vtbl 0x7FF784BC8F50  Writer 0x6AB9D0  Huelle 0x6ABBD0
//
// ZWEI Befunde, die dem Subplan und dem Brief widersprechen und die Umsetzung bestimmen:
//
//  1. Das fuehrende uint32 ist NICHT die regionID, sondern der Anfrage-Token. Die regionID ist
//     ein uint8 dahinter. Belegt an der Lua-Glue (z.B. RequestAccountCharacters, RVA 0xBAC6C0):
//     das Lua-Argument landet auf msg+0x24 (uint8), der vorinkrementierte Zaehler bei
//     0x7FF787A6579C auf msg+0x20 (uint32) und wird zugleich als Lua-Rueckgabewert gesetzt.
//     Zaehlerblock: 0x7FF787A65790 KEY_BINDINGS, ..94 ACCOUNT_RESTORE, ..98 CHARACTER_COPY,
//     ..9C GET_ACCOUNT_CHARACTER_LIST.
//     Der Client verwirft eine Antwort mit falschem Token SPURLOS - der Server MUSS ihn
//     zurueckspiegeln, sonst bleibt COPY_IN_PROGRESS als modaler Spinner ohne Knoepfe stehen
//     und die Charakterauswahl ist tot.
//
//  2. Es ist NICHT "ein Serializer-Muster, viermal instanziiert". Die Reihenfolge des
//     Stringpaares ist in zwei der vier Faelle vertauscht - Bit-Reihenfolge UND Byte-Reihenfolge
//     zugleich. Beide Bit-Sektionen bleiben dabei 2 Byte lang, der Fehler ist am Draht also
//     nicht offensichtlich.
//       0x4300E5 / 0x4300E7:  bits<9> RealmName     dann bits<6> CharacterName
//       0x4300E6 / 0x4300E8:  bits<6> CharacterName dann bits<9> RealmName
//
// Puffergroessen: RealmName 257 (-> bits<9>), CharacterName 49 (-> bits<6>); beide ohne NUL am
// Draht. Aus der Bitbreite abgeleitete Schranken (511 / 63) waeren zu grosszuegig.
// Die Stringzuordnung ist ueber GetAccountCharacterInfo (RVA 0xB9A8C0) belegt: Rueckgabe 1
// "name" zeigt auf den 49-Byte-Puffer, Rueckgabe 2 "realmName" auf den 257-Byte-Puffer.
//
// regionID ist Cfg_Regions.Region_ID: 1 NA, 2 KR, 3 EU, 4 TW, 5 CN
// (characterCopyRegions, CharacterSelect.lua:19).
// ----------------------------------------------------------------------------------------------

namespace WorldPackets
{
    namespace LiveRegion
    {
        // 0x4300E5, Writer 0x6AB3D0 - 7..313 Byte.
        // Lua: RequestAccountCharacters(regionID [, realmName, characterName]) -> token
        class GetAccountCharacterList final : public ClientPacket
        {
        public:
            explicit GetAccountCharacterList(WorldPacket&& packet) : ClientPacket(CMSG_LIVE_REGION_GET_ACCOUNT_CHARACTER_LIST, std::move(packet)) { }

            void Read() override;

            uint32 Token = 0;
            uint8 RegionID = 0;
            std::string RealmName;
            std::string CharacterName;
        };

        // 0x4300E6, Writer 0x6AB580 - 16..338 Byte.
        // Lua: CopyAccountCharacterFromLive(regionID [, index, realmName, characterName]) -> token
        class CharacterCopy final : public ClientPacket
        {
        public:
            explicit CharacterCopy(WorldPacket&& packet) : ClientPacket(CMSG_LIVE_REGION_CHARACTER_COPY, std::move(packet)) { }

            void Read() override;

            uint32 Token = 0;
            uint8 RegionID = 0;
            uint32 VirtualRealmAddress = 0;     // UNVERIFIED: Name geraten; Offset/Breite belegt
            ObjectGuid CharacterGUID;
            uint8 Option0 = 0;                  // UNVERIFIED: der Client sendet konstant 0
            uint8 Option1 = 0;                  // UNVERIFIED: der Client sendet konstant 0
            uint8 Option2 = 0;                  // UNVERIFIED: der Client sendet konstant 0
            std::string CharacterName;
            std::string RealmName;
        };

        // 0x4300E7, Writer 0x6AB800 - 17..339 Byte.
        // Lua: CopyAccountDataFromLive(regionID [, index, realmName, characterName]) -> token
        class AccountRestore final : public ClientPacket
        {
        public:
            explicit AccountRestore(WorldPacket&& packet) : ClientPacket(CMSG_LIVE_REGION_ACCOUNT_RESTORE, std::move(packet)) { }

            void Read() override;

            uint32 Token = 0;
            uint8 RegionID = 0;
            ObjectGuid CharacterGUID;
            uint64 Field368 = 0;                // UNVERIFIED: der Client sendet konstant 0
            std::string RealmName;
            std::string CharacterName;
        };

        // 0x4300E8, Writer 0x6AB9D0 - 13..335 Byte.
        // Lua: CopyKeyBindingsFromLive(regionID [, index, realmName, characterName]) -> token
        class KeyBindingsCopy final : public ClientPacket
        {
        public:
            explicit KeyBindingsCopy(WorldPacket&& packet) : ClientPacket(CMSG_LIVE_REGION_KEY_BINDINGS_COPY, std::move(packet)) { }

            void Read() override;

            uint32 Token = 0;
            uint8 RegionID = 0;
            uint32 VirtualRealmAddress = 0;     // UNVERIFIED: Name geraten; Offset/Breite belegt
            ObjectGuid CharacterGUID;
            std::string CharacterName;
            std::string RealmName;
        };

        // ------------------------------------------------------------------------------------
        // Die Antwortseite. Alle vier SMSG stehen im Baum auf STATUS_UNHANDLED und sind damit
        // sendegesperrt - die Statuszeile gehoert zur Umsetzung, sonst sendet der Server sie nie.
        //
        // ES GIBT KEIN FEHLERCODE-ENUM, und das ist gemessen, nicht vermutet: alle vier
        // Konsumenten (0x1D267C0 / 0x1D26830 / 0x1D268C0 / 0x1D26910) lesen ausschliesslich das
        // 1-Bit-Erfolgsflag und reichen es mit dem Token an Lua weiter; eine Vergleichskette
        // existiert fuer diese Familie im Client nicht. Ein Fehlschlag ist deshalb
        // Success == false, und der Client zeigt dafuer den einen generischen Text COPY_FAILED.
        //
        // DER TOKEN MUSS ZURUECKGESPIEGELT WERDEN. Jede der vier Lua-Funktionen inkrementiert
        // ihren eigenen 32-Bit-Zaehler (Block 0x7FF787A65790..9C) und gibt ihn an Lua zurueck;
        // die Antwort traegt genau diesen Wert als erstes Feld. Ohne Ruecksspiegelung verwirft der
        // Client die Antwort SPURLOS - COPY_IN_PROGRESS bleibt als modaler Spinner ohne Knoepfe
        // stehen und die Charakterauswahl ist tot.
        //
        // Benennungsfallen, die so im Binary stehen und nicht zu "korrigieren" sind:
        // CHARACTER_COPY_RESULT loest das Lua-Ereignis CHAR_RESTORE_COMPLETE aus (nicht "COPY"),
        // und ACCOUNT_CHARACTER_LIST_RECIEVED ist Blizzards Tippfehler und der echte Ereignisname.
        // ------------------------------------------------------------------------------------

        // Element von SMSG_LIVE_REGION_GET_ACCOUNT_CHARACTER_LIST_RESULT.
        // Client-Typ JamCliAccountCharacterData, 368 Byte, Elementleser 0x69F600.
        // Zwei Felder sind ueber die Lua-Glue GetAccountCharacterInfo (RVA 0xB9A8C0) HART
        // belegt, nicht aus der Reihenfolge geraten:
        //   *(char*)(elem + 343) ist der Index in die Klassennamentabelle off_7FF785B85240
        //                        -> ClassID  (Lua-Rueckgabe 3 "className")
        //   *(uint8*)(elem + 345) -> ExperienceLevel  (Lua-Rueckgabe 4)
        //   elem + 36  -> name       (Puffer 49,  bits<6>)   Lua-Rueckgabe 1
        //   elem + 85  -> realmName  (Puffer 257, bits<9>)   Lua-Rueckgabe 2   (85 == 36 + 49)
        // Die uebrigen Felder sind in Position und Breite belegt, in der Bedeutung nicht.
        struct AccountCharacterData
        {
            ObjectGuid CharacterGUID;               // gepackt, Element +0
            ObjectGuid AccountGUID;                 // gepackt, Element +16; UNVERIFIED: Name geraten
            uint32 VirtualRealmAddress = 0;         // Element +32;  UNVERIFIED: Name geraten
            uint8 RaceID = 0;                       // Element +342; UNVERIFIED: Bedeutung geraten
            uint8 ClassID = 0;                      // Element +343; belegt
            uint8 SexID = 0;                        // Element +344; UNVERIFIED: Bedeutung geraten
            uint8 ExperienceLevel = 0;              // Element +345; belegt
            uint64 Field352 = 0;                    // UNVERIFIED: Bedeutung offen
            uint32 Field360 = 0;                    // UNVERIFIED: Bedeutung offen
            std::string Name;                       // Puffer 49,  ohne NUL am Draht
            std::string RealmName;                  // Puffer 257, ohne NUL am Draht
        };

        ByteBuffer& operator<<(ByteBuffer& data, AccountCharacterData const& character);

        // 0x45020C, Reader 0x5FBAB0.
        // Bit-Sektion des Elements: bits<6> len(Name) + bits<9> len(RealmName) = 15 Bit -> 2 Byte
        // mit einem Fuellbit. Nachgerechnet am Reader: *(elem+36) = B0 >> 2 (6 Bit),
        // *(elem+85) = ((B0 & 3) << 7) | (B1 >> 1) (9 Bit).
        class LiveRegionGetAccountCharacterListResult final : public ServerPacket
        {
        public:
            explicit LiveRegionGetAccountCharacterListResult() : ServerPacket(SMSG_LIVE_REGION_GET_ACCOUNT_CHARACTER_LIST_RESULT, 9) { }

            WorldPacket const* Write() override;

            uint32 Token = 0;                       // Echo aus der Anfrage - PFLICHT
            std::vector<AccountCharacterData> Characters;
            bool Success = false;
        };

        // 0x450218 / 0x450219 / 0x45021A, Reader 0x5FC2F0 / 0x5FC380 / 0x5FC410.
        // Alle drei sind instruktionsgleich: uint32 Token; bit Success; FLUSH. Genau 5 Byte.
        class LiveRegionCharacterCopyResult final : public ServerPacket
        {
        public:
            explicit LiveRegionCharacterCopyResult() : ServerPacket(SMSG_LIVE_REGION_CHARACTER_COPY_RESULT, 5) { }

            WorldPacket const* Write() override;

            uint32 Token = 0;                       // Echo aus der Anfrage - PFLICHT
            bool Success = false;
        };

        class LiveRegionAccountRestoreResult final : public ServerPacket
        {
        public:
            explicit LiveRegionAccountRestoreResult() : ServerPacket(SMSG_LIVE_REGION_ACCOUNT_RESTORE_RESULT, 5) { }

            WorldPacket const* Write() override;

            uint32 Token = 0;                       // Echo aus der Anfrage - PFLICHT
            bool Success = false;
        };

        class LiveRegionKeyBindingsCopyResult final : public ServerPacket
        {
        public:
            explicit LiveRegionKeyBindingsCopyResult() : ServerPacket(SMSG_LIVE_REGION_KEY_BINDINGS_COPY_RESULT, 5) { }

            WorldPacket const* Write() override;

            uint32 Token = 0;                       // Echo aus der Anfrage - PFLICHT
            bool Success = false;
        };
    }
}

#endif // TRINITYCORE_LIVE_REGION_PACKETS_H

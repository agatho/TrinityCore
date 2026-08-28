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
// Einheit w4_cmsg_43_3D - Block B5 "Live-Region-Kopie", Phase A.
//
// Vier Anfragen, vier Antworten. Der Draht steht in Packets/LiveRegionPackets.{h,cpp}; hier steht
// die Wirkung.
//
// DIE EINE REGEL, DIE DIESEN BLOCK TRAEGT: der Anfrage-Token MUSS zurueckgespiegelt werden.
// Jede der vier Lua-Funktionen inkrementiert ihren eigenen 32-Bit-Zaehler (geschlossener Block
// 0x7FF787A65790..9C) und gibt ihn an Lua zurueck; der Konsument vergleicht das erste Feld der
// Antwort damit. Passt es nicht, verwirft der Client die Antwort SPURLOS - COPY_IN_PROGRESS
// bleibt als modaler Spinner OHNE KNOEPFE stehen, und die Charakterauswahl ist damit tot. Ein
// Server, der nicht antwortet, hat denselben Effekt.
//
// Deshalb ist "wir koennen das nicht" hier KEIN Grund zu schweigen, sondern der Grund, mit
// Success = false zu antworten: der Client zeigt dann StaticPopup COPY_FAILED und gibt die
// Oberflaeche frei. Es gibt fuer diese Familie clientseitig KEIN Fehlercode-Enum - alle vier
// Konsumenten lesen nur das 1-Bit-Erfolgsflag (gemessen, nicht vermutet). "ging nicht" ist also
// die vollstaendige Fehlerinformation, die das Protokoll ueberhaupt transportieren kann, und D3
// ist damit erfuellt, nicht umgangen.
//
// D4 - Persistenz: ENTSCHIEDEN, aber nicht angelegt. Der Brief empfiehlt fuer diesen Block eine
// dauerhafte Auftragstabelle. Die waere sinnlos, solange es keinen Kopierdienst gibt, der
// Auftraege abarbeitet: eine Tabelle, in die nur geschrieben und aus der nie gelesen wird, ist
// kein geloestes D4, sondern ein leerer Migrationssatz. Sobald der Dienst entsteht, gehoert die
// Tabelle in dieselbe Aenderung wie er.
//
// AUSSERDEM, und ohne das sieht der Spieler den Knopf nie: der Einstieg haengt an
// C_CharacterServices.IsLiveRegionCharacterListEnabled() / IsLiveRegionCharacterCopyEnabled() /
// IsLiveRegionAccountCopyEnabled() / IsLiveRegionKeyBindingsCopyEnabled(). Sind alle vier false,
// blendet CharacterSelect.lua den CopyCharacterButton komplett aus. Diese Schalter kommen NICHT
// aus diesem Opcodesatz - sie sind Teil von FeatureSystemStatus und gehoeren zu der Einheit, die
// das haelt.
// ================================================================================================

#include "WorldSession.h"
#include "LiveRegionPackets.h"
#include "Log.h"

// WARUM HIER KEINE EINGANGSPRUEFUNG STEHT - die Werte sind gemessen, die Schranke waere trotzdem
// eine Attrappe. Bis 2026-08-28 stand hier ein IsPlausibleRequest(regionId, realmName,
// characterName), das genau EINER der vier Handler aufrief und das dort nur entschied, welche von
// zwei TC_LOG_DEBUG-Zeilen geschrieben wird - beide Zweige antworteten anschliessend dasselbe.
// Eine Pruefung ohne Folge ist schlimmer als keine: sie liest sich beim naechsten Anfassen als
// vorhandene Schranke, auf die man sich verlaesst. Sie ist deshalb gestrichen, und die Messungen
// stehen hier, wo sie gebraucht werden, sobald es etwas zu schuetzen gibt:
//
//  * regionID ist Cfg_Regions.Region_ID (characterCopyRegions, CharacterSelect.lua:19):
//    1 NA, 2 KR, 3 EU, 4 TW, 5 CN. Alles ausserhalb ist am Draht moeglich, aber kein gueltiger
//    Wert - ein Kopierdienst muesste den Bereich pruefen, BEVOR er eine Region nachschlaegt.
//  * Die Zeichenkettenpuffer des Clients sind 257 (RealmName) und 49 (CharacterName), aus den
//    strnlen-Zaehlerkonstanten der Serializer GEMESSEN und nicht aus den Bitbreiten abgeleitet:
//    bits<9> erlaubt 511 und bits<6> erlaubt 63. Wer die Schranke aus der Bitbreite raet, laesst
//    doppelt so lange Zeichenketten durch, wie ein Client je erzeugt.
//
// Heute ist das folgenlos, und zwar belegt, nicht vermutet: keines der vier Ergebnispakete traegt
// ein Feld aus der Anfrage zurueck ausser dem uint32-Token (LiveRegionPackets.h - drei mal
// {Token, Success}, einmal {Token, serverseitige Liste, Success}). Es geht also keine
// Clientzeichenkette an irgendeinen Client, und die Bitbreiten begrenzen die Belegung beim Lesen.
// Sobald RealmName oder CharacterName weitergereicht, gespeichert oder gespiegelt werden, gehoert
// die Pruefung zurueck - dann aber mit Wirkung: Ablehnung statt Protokollzeile.

// 0x4300E5 -> 0x45020C. Lua RequestAccountCharacters(regionID [, realmName, characterName])
// -> Ereignis ACCOUNT_CHARACTER_LIST_RECIEVED (Blizzards Tippfehler ist der echte Ereignisname).
// Einzige der vier Antworten mit Nutzlast: uint32 Token, Liste, bit Success. Die leere Liste mit
// Success = false ist die vollstaendige Aussage "diese Region liefert nichts".
void WorldSession::HandleLiveRegionGetAccountCharacterList(WorldPackets::LiveRegion::GetAccountCharacterList& getAccountCharacterList)
{
    WorldPackets::LiveRegion::LiveRegionGetAccountCharacterListResult result;
    result.Token = getAccountCharacterList.Token;
    result.Success = false;

    TC_LOG_DEBUG("network.opcode", "CMSG_LIVE_REGION_GET_ACCOUNT_CHARACTER_LIST from {} region {} realm '{}' character '{}' - no live region service configured",
        GetPlayerInfo(), getAccountCharacterList.RegionID, getAccountCharacterList.RealmName, getAccountCharacterList.CharacterName);

    SendPacket(result.Write());
}

// 0x4300E6 -> 0x450218. Lua CopyAccountCharacterFromLive(...)
// -> Ereignis CHAR_RESTORE_COMPLETE. Der Ereignisname weicht vom Opcodenamen ab; das ist so im
// Binary und nicht zu "korrigieren".
void WorldSession::HandleLiveRegionCharacterCopy(WorldPackets::LiveRegion::CharacterCopy& characterCopy)
{
    WorldPackets::LiveRegion::LiveRegionCharacterCopyResult result;
    result.Token = characterCopy.Token;
    result.Success = false;

    TC_LOG_DEBUG("network.opcode", "CMSG_LIVE_REGION_CHARACTER_COPY from {} region {} character {} '{}'-'{}' - no live region service configured",
        GetPlayerInfo(), characterCopy.RegionID, characterCopy.CharacterGUID.ToString(),
        characterCopy.CharacterName, characterCopy.RealmName);

    SendPacket(result.Write());
}

// 0x4300E7 -> 0x450219. Lua CopyAccountDataFromLive(...) -> Ereignis ACCOUNT_DATA_RESTORED.
void WorldSession::HandleLiveRegionAccountRestore(WorldPackets::LiveRegion::AccountRestore& accountRestore)
{
    WorldPackets::LiveRegion::LiveRegionAccountRestoreResult result;
    result.Token = accountRestore.Token;
    result.Success = false;

    TC_LOG_DEBUG("network.opcode", "CMSG_LIVE_REGION_ACCOUNT_RESTORE from {} region {} character {} '{}'-'{}' - no live region service configured",
        GetPlayerInfo(), accountRestore.RegionID, accountRestore.CharacterGUID.ToString(),
        accountRestore.CharacterName, accountRestore.RealmName);

    SendPacket(result.Write());
}

// 0x4300E8 -> 0x45021A. Lua CopyKeyBindingsFromLive(...)
// -> Ereignis KEY_BINDINGS_COPY_COMPLETE.
void WorldSession::HandleLiveRegionKeyBindingsCopy(WorldPackets::LiveRegion::KeyBindingsCopy& keyBindingsCopy)
{
    WorldPackets::LiveRegion::LiveRegionKeyBindingsCopyResult result;
    result.Token = keyBindingsCopy.Token;
    result.Success = false;

    TC_LOG_DEBUG("network.opcode", "CMSG_LIVE_REGION_KEY_BINDINGS_COPY from {} region {} character {} '{}'-'{}' - no live region service configured",
        GetPlayerInfo(), keyBindingsCopy.RegionID, keyBindingsCopy.CharacterGUID.ToString(),
        keyBindingsCopy.CharacterName, keyBindingsCopy.RealmName);

    SendPacket(result.Write());
}

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

namespace
{
// regionID ist Cfg_Regions.Region_ID (characterCopyRegions, CharacterSelect.lua:19):
// 1 NA, 2 KR, 3 EU, 4 TW, 5 CN. Alles ausserhalb ist am Draht moeglich, aber kein gueltiger Wert.
constexpr uint8 MinRegionId = 1;
constexpr uint8 MaxRegionId = 5;

// Puffergroessen aus den strnlen-Zaehlerkonstanten der Serializer GEMESSEN, nicht aus den
// Bitbreiten abgeleitet: bits<9> erlaubt 511 und bits<6> erlaubt 63, die Puffer fassen aber nur
// 257 bzw. 49. Wer die Schranke aus der Bitbreite raet, laesst doppelt so lange Zeichenketten
// durch, wie der Client je erzeugen kann. Ein Client kann das nicht senden, ein Angreifer schon.
constexpr std::size_t MaxRealmNameLength = 256;      // Puffer 257 einschliesslich NUL
constexpr std::size_t MaxCharacterNameLength = 48;   // Puffer 49 einschliesslich NUL

bool IsPlausibleRequest(uint8 regionId, std::string const& realmName, std::string const& characterName)
{
    return regionId >= MinRegionId && regionId <= MaxRegionId
        && realmName.length() <= MaxRealmNameLength
        && characterName.length() <= MaxCharacterNameLength;
}
}

// 0x4300E5 -> 0x45020C. Lua RequestAccountCharacters(regionID [, realmName, characterName])
// -> Ereignis ACCOUNT_CHARACTER_LIST_RECIEVED (Blizzards Tippfehler ist der echte Ereignisname).
// Einzige der vier Antworten mit Nutzlast: uint32 Token, Liste, bit Success. Die leere Liste mit
// Success = false ist die vollstaendige Aussage "diese Region liefert nichts".
void WorldSession::HandleLiveRegionGetAccountCharacterList(WorldPackets::LiveRegion::GetAccountCharacterList& getAccountCharacterList)
{
    WorldPackets::LiveRegion::LiveRegionGetAccountCharacterListResult result;
    result.Token = getAccountCharacterList.Token;
    result.Success = false;

    if (!IsPlausibleRequest(getAccountCharacterList.RegionID, getAccountCharacterList.RealmName, getAccountCharacterList.CharacterName))
        TC_LOG_DEBUG("network.opcode", "CMSG_LIVE_REGION_GET_ACCOUNT_CHARACTER_LIST from {} with implausible request (region {})",
            GetPlayerInfo(), getAccountCharacterList.RegionID);
    else
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

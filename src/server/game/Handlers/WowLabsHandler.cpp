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
// Einheit w4_cmsg_43_3D - Block B7 "WoW Labs / Plunderstorm", Phase A.
//
// Der Draht steht in Packets/LobbyMatchmakerPackets.{h,cpp}, Namensraum WorldPackets::WowLabs;
// dort steht auch, warum Subplan und Brief diesen Block drei Opcodes zu frueh gelesen haben.
//
// EINE TATSACHE VORWEG, weil sie die Abnahme bestimmt: alle drei Sendestellen sind auf
// C_GameRules.IsGameRuleActive(14) gattert - Regel 14 ist GameRule::CharacterlessLogin.
// TrinityCore setzt in World.cpp nur TransmogEnabled und HousingEnabled. Auf einem
// unveraenderten Realm sendet der Retail-Client diese drei Opcodes also NIE, und es gibt in
// keinem der drei Sender eine Wartezustandsstruktur oder Rueckrufregistrierung - es kann nichts
// haengen bleiben. Gebaut sind sie trotzdem vollstaendig, damit die Regel gesetzt werden kann,
// ohne dass jemand das Protokoll noch einmal erarbeiten muss.
//
// D4 - Persistenz: die ausgewaehlte Absprungzone ist Lobbyzustand und damit FLUECHTIG. Solange
// es keine Zonen gibt, entsteht ohnehin kein Zustand.
// ================================================================================================

#include "WorldSession.h"
#include "LobbyMatchmakerPackets.h"
#include "Log.h"

// 0x3D02FA -> 0x450329 mit AreaID 0.
// 0 ist die Marke "nichts ausgewaehlt", nicht ein Platzhalter: GetConfirmedWoWLabsArea
// (0x17DEF00) prueft `test eax, eax; je` und gibt bei 0 Lua-nil zurueck, was RefreshAllData als
// echten Zustand behandelt. Es MUESSEN genau vier Byte sein - der Reader 0x60D4B0 nimmt den Rest
// des Pakets als Zeiger und liest daraus vier Byte ohne Laengenpruefung; bei leerer Nutzlast
// laese der Konsument hinter den Paketpuffer.
void WorldSession::HandleQuerySelectedWowLabsArea(WorldPackets::WowLabs::QuerySelectedWowLabsArea& querySelectedWowLabsArea)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_QUERY_SELECTED_WOW_LABS_AREA from {} player {} - no WoW Labs system in this tree",
        GetPlayerInfo(), querySelectedWowLabsArea.PlayerGUID.ToString());

    WorldPackets::WowLabs::QuerySelectedWowLabsAreaResponse response;
    response.WowLabsAreaID = 0;
    SendPacket(response.Write());
}

// 0x3D02FB -> 0x45032A mit Count 0.
// Der Reader ueberspringt die Elementschleife bei Count == 0, der Konsument 0x22BB40 legt einen
// leeren Vektor an und feuert WOW_LABS_AREA_INFO_UPDATED; Lua faengt das mit
// `if TableIsEmpty(areas) then return; end` sauber ab. Ein klarer Leerzustand, nichts haengt.
void WorldSession::HandleQueryWowLabsAreaInfo(WorldPackets::WowLabs::QueryWowLabsAreaInfo& queryWowLabsAreaInfo)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_QUERY_WOW_LABS_AREA_INFO from {} player {} - no WoW Labs system in this tree",
        GetPlayerInfo(), queryWowLabsAreaInfo.PlayerGUID.ToString());

    SendPacket(WorldPackets::WowLabs::QueryWowLabsAreaInfoResponse().Write());
}

// 0x3D02FC -> 0x450328 mit Success = false.
//
// Das ist der EINZIGE Weg, dem Client "das ging nicht" zu sagen: GameError 1186
// (ERR_WOW_LABS_SET_WOW_LABS_AREA_ID_FAILED) kommt im gesamten Abbild genau einmal vor, im
// Konsumenten 0x22BAC0, und nur auf dem Misserfolgszweig.
//
// NICHT mit Success = true antworten: Erfolg ist dort ein stilles no-op (`cmp byte [rcx+0x20],0
// / jne <ret>`), der Client lernt nichts daraus, und die Auswahl bliebe im angefragten Zustand
// stehen. Ein echter Erfolg braeuchte ZWEI Pakete - dieses mit true, dann
// SMSG_WOW_LABS_AREA_SELECTED mit der bestaetigten ID.
//
// DARAUS FOLGT EINE LUECKE, DIE HIER STEHEN MUSS: SMSG_WOW_LABS_AREA_SELECTED (0x45032B) ist die
// EINZIGE der 16 in dieser Einheit auf STATUS_NEVER gedrehten SMSG, die im ganzen Baum KEINE
// Sendestelle hat - Paketklasse und Write sind gebaut, gesendet wird sie von nichts, weil der
// Erfolgspfad nicht existiert. Sie mit Success = false mitzuschicken waere eine Bestaetigung
// einer Auswahl, die nicht stattgefunden hat. Die Sendefreigabe bleibt trotzdem gedreht: wer den
// Erfolgspfad spaeter baut, laeuft sonst in die stille Sperre von WorldSession::SendPacket, die
// jedes STATUS_UNHANDLED-Paket verwirft, ohne es zu melden. D3 ist fuer diesen Opcode damit
// ausdruecklich OFFEN, nicht erfuellt - so auch in der Statusdatei unter smsg_freigaben gefuehrt.
//
// UNVERIFIED, ehrlich vermerkt: auch auf dem Misserfolgszweig ruft
// WM_WoWLabsAreaDataProvider:OnEvent nur RefreshAllData() und loescht self.requestedAreaID
// nicht. Der Spieler sieht die rote Fehlermeldung, aber die Auswahlnadel wird nicht wieder
// scharf. Das ist Verhalten des Blizzard-UI; keine Serverantwort kann es aufheben.
void WorldSession::HandleSelectWowLabsArea(WorldPackets::WowLabs::SelectWowLabsArea& selectWowLabsArea)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SELECT_WOW_LABS_AREA from {} player {} area {} - no WoW Labs system in this tree",
        GetPlayerInfo(), selectWowLabsArea.PlayerGUID.ToString(), selectWowLabsArea.WowLabsAreaID);

    WorldPackets::WowLabs::WowLabsSetWowLabsAreaIdResponse response;
    response.Success = false;
    SendPacket(response.Write());
}

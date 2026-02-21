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
#include "CharacterCache.h"
#include "DB2Stores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "MiscPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"

void WorldSession::HandleRequestCurrencyDataForAccountCharacters(WorldPackets::Misc::RequestCurrencyDataForAccountCharacters& /*packet*/)
{
    WorldPackets::Misc::AccountCharacterCurrencyLists response;
    SendPacket(response.Write());
}

void WorldSession::HandleTransferCurrencyFromAccountCharacter(WorldPackets::Misc::TransferCurrencyFromAccountCharacter& /*packet*/)
{
    WorldPackets::Misc::CurrencyTransferResult result;
    result.Result = AccountCurrencyTransferResult::CurrencyTransferDisabled;
    SendPacket(result.Write());
}

void WorldSession::HandleGetCharacterCurrencyTransferLog(WorldPackets::Misc::GetCharacterCurrencyTransferLog& /*packet*/)
{
    WorldPackets::Misc::CurrencyTransferLog response;
    SendPacket(response.Write());
}

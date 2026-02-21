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
#include "GameTime.h"
#include "Log.h"
#include "MiscPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"

void WorldSession::HandleRequestCurrencyDataForAccountCharacters(WorldPackets::Misc::RequestCurrencyDataForAccountCharacters& /*packet*/)
{
    uint32 bnetAccountId = GetBattlenetAccountId();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_CHARACTER_CURRENCIES);
    stmt->setUInt32(0, bnetAccountId);

    _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this](PreparedQueryResult result)
    {
        WorldPackets::Misc::AccountCharacterCurrencyLists response;

        if (result)
        {
            std::unordered_map<uint64, size_t> characterIndexMap;

            do
            {
                Field* fields = result->Fetch();
                uint64 guid = fields[0].GetUInt64();
                std::string name = fields[1].GetString();
                uint8 classId = fields[2].GetUInt8();
                uint32 level = fields[3].GetUInt32();
                uint32 currencyId = fields[4].GetUInt16();
                uint32 quantity = fields[5].GetUInt32();

                // Skip the currently logged-in character
                Player* player = GetPlayer();
                if (player && player->GetGUID().GetCounter() == guid)
                    continue;

                CurrencyTypesEntry const* currencyType = sCurrencyTypesStore.LookupEntry(currencyId);
                if (!currencyType || !currencyType->IsAccountTransferable())
                    continue;

                if (quantity == 0)
                    continue;

                // Add character info if not already added
                auto [it, inserted] = characterIndexMap.try_emplace(guid, response.Characters.size());
                if (inserted)
                {
                    WorldPackets::Misc::AccountCharacterCurrencyLists::CharacterCurrencyData charData;
                    charData.CharacterGUID = ObjectGuid::Create<HighGuid::Player>(guid);
                    charData.CharacterName = std::move(name);
                    charData.ClassID = classId;
                    charData.Level = level;
                    response.Characters.push_back(std::move(charData));
                }

                WorldPackets::Misc::AccountCharacterCurrencyLists::CurrencyQuantityData currencyData;
                currencyData.CharacterGUID = ObjectGuid::Create<HighGuid::Player>(guid);
                currencyData.CurrencyTypeID = currencyId;
                currencyData.Quantity = quantity;
                response.CurrencyData.push_back(std::move(currencyData));

            } while (result->NextRow());
        }

        SendPacket(response.Write());
    }));
}

void WorldSession::HandleTransferCurrencyFromAccountCharacter(WorldPackets::Misc::TransferCurrencyFromAccountCharacter& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Validate currency
    CurrencyTypesEntry const* currencyType = sCurrencyTypesStore.LookupEntry(packet.CurrencyID);
    if (!currencyType)
    {
        WorldPackets::Misc::CurrencyTransferResult result;
        result.CurrencyID = packet.CurrencyID;
        result.Result = AccountCurrencyTransferResult::InvalidCurrency;
        SendPacket(result.Write());
        return;
    }

    if (!currencyType->IsAccountTransferable())
    {
        WorldPackets::Misc::CurrencyTransferResult result;
        result.CurrencyID = packet.CurrencyID;
        result.Result = AccountCurrencyTransferResult::InvalidCurrency;
        SendPacket(result.Write());
        return;
    }

    if (packet.Quantity <= 0)
    {
        WorldPackets::Misc::CurrencyTransferResult result;
        result.CurrencyID = packet.CurrencyID;
        result.Result = AccountCurrencyTransferResult::InsufficientCurrency;
        SendPacket(result.Write());
        return;
    }

    // Check source character is not logged in
    if (ObjectAccessor::FindPlayer(packet.SourceCharacterGUID))
    {
        WorldPackets::Misc::CurrencyTransferResult result;
        result.CurrencyID = packet.CurrencyID;
        result.Result = AccountCurrencyTransferResult::CharacterLoggedIn;
        SendPacket(result.Write());
        return;
    }

    // Verify source character belongs to same bnet account
    uint32 bnetAccountId = GetBattlenetAccountId();
    ObjectGuid::LowType sourceGuid = packet.SourceCharacterGUID.GetCounter();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_CHARACTER_CURRENCIES);
    stmt->setUInt32(0, bnetAccountId);

    int32 currencyId = packet.CurrencyID;
    int32 requestedQuantity = packet.Quantity;

    _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this, currencyId, requestedQuantity, sourceGuid, bnetAccountId](PreparedQueryResult result)
    {
        Player* player = GetPlayer();
        if (!player)
            return;

        CurrencyTypesEntry const* currencyType = sCurrencyTypesStore.LookupEntry(currencyId);
        if (!currencyType)
            return;

        // Find source character's currency quantity
        bool sourceFound = false;
        int32 sourceQuantity = 0;

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint64 guid = fields[0].GetUInt64();
                uint32 rowCurrencyId = fields[4].GetUInt16();
                uint32 quantity = fields[5].GetUInt32();

                if (guid == sourceGuid && rowCurrencyId == static_cast<uint32>(currencyId))
                {
                    sourceFound = true;
                    sourceQuantity = quantity;
                    break;
                }
            } while (result->NextRow());
        }

        if (!sourceFound)
        {
            WorldPackets::Misc::CurrencyTransferResult transferResult;
            transferResult.CurrencyID = currencyId;
            transferResult.Result = AccountCurrencyTransferResult::NoValidSourceCharacter;
            SendPacket(transferResult.Write());
            return;
        }

        if (sourceQuantity < requestedQuantity)
        {
            WorldPackets::Misc::CurrencyTransferResult transferResult;
            transferResult.CurrencyID = currencyId;
            transferResult.Result = AccountCurrencyTransferResult::InsufficientCurrency;
            SendPacket(transferResult.Write());
            return;
        }

        // Calculate received amount after transfer percentage
        int32 receivedAmount = static_cast<int32>(std::floor(requestedQuantity * currencyType->AccountTransferPercentage / 100.0f));
        if (receivedAmount <= 0)
        {
            WorldPackets::Misc::CurrencyTransferResult transferResult;
            transferResult.CurrencyID = currencyId;
            transferResult.Result = AccountCurrencyTransferResult::InsufficientCurrency;
            SendPacket(transferResult.Write());
            return;
        }

        // Check destination max quantity
        uint32 currentQuantity = player->GetCurrencyQuantity(currencyId);
        uint32 maxQuantity = player->GetCurrencyMaxQuantity(currencyType);
        if (maxQuantity && (currentQuantity + receivedAmount) > maxQuantity)
        {
            WorldPackets::Misc::CurrencyTransferResult transferResult;
            transferResult.CurrencyID = currencyId;
            transferResult.Result = AccountCurrencyTransferResult::MaxQuantity;
            SendPacket(transferResult.Write());
            return;
        }

        // Execute transfer in a transaction
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Subtract from source (offline character, direct DB update)
        int32 newSourceQuantity = sourceQuantity - requestedQuantity;
        CharacterDatabasePreparedStatement* updateStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PLAYER_CURRENCY_QUANTITY);
        updateStmt->setUInt32(0, newSourceQuantity);
        updateStmt->setUInt64(1, sourceGuid);
        updateStmt->setUInt16(2, currencyId);
        trans->Append(updateStmt);

        // Log the transfer
        CharacterDatabasePreparedStatement* logStmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_WARBAND_CURRENCY_TRANSFER_LOG);
        logStmt->setUInt32(0, bnetAccountId);
        logStmt->setUInt32(1, currencyId);
        logStmt->setUInt64(2, sourceGuid);
        logStmt->setUInt64(3, player->GetGUID().GetCounter());
        logStmt->setInt32(4, requestedQuantity);
        logStmt->setUInt32(5, uint32(GameTime::GetGameTime()));
        trans->Append(logStmt);

        CharacterDatabase.CommitTransaction(trans);

        // Add currency to destination (online player)
        player->ModifyCurrency(currencyId, receivedAmount, CurrencyGainSource::AccountCopy);

        // Send success result
        WorldPackets::Misc::CurrencyTransferResult transferResult;
        transferResult.CurrencyID = currencyId;
        transferResult.Quantity = receivedAmount;
        transferResult.TotalQuantity = player->GetCurrencyQuantity(currencyId);
        transferResult.Result = AccountCurrencyTransferResult::Ok;
        SendPacket(transferResult.Write());
    }));
}

void WorldSession::HandleGetCharacterCurrencyTransferLog(WorldPackets::Misc::GetCharacterCurrencyTransferLog& /*packet*/)
{
    uint32 bnetAccountId = GetBattlenetAccountId();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_WARBAND_CURRENCY_TRANSFER_LOG);
    stmt->setUInt32(0, bnetAccountId);

    _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this](PreparedQueryResult result)
    {
        WorldPackets::Misc::CurrencyTransferLog response;

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                WorldPackets::Misc::CurrencyTransferLog::CurrencyTransferLogEntry entry;
                entry.CurrencyTypeID = fields[0].GetUInt32();
                entry.SourceCharacterGUID = ObjectGuid::Create<HighGuid::Player>(fields[1].GetUInt64());
                entry.DestCharacterGUID = ObjectGuid::Create<HighGuid::Player>(fields[2].GetUInt64());
                entry.Quantity = fields[3].GetInt32();
                entry.Timestamp = fields[4].GetUInt32();
                response.Entries.push_back(std::move(entry));

            } while (result->NextRow());
        }

        SendPacket(response.Write());
    }));
}

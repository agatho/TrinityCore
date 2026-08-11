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
#include "World.h"

void WorldSession::HandleRequestCurrencyDataForAccountCharacters(WorldPackets::Misc::RequestCurrencyDataForAccountCharacters& /*packet*/)
{
    uint32 bnetAccountId = GetBattlenetAccountId();

    // Refuse enumeration for an unlinked account: WHERE battlenetAccount = 0 would otherwise
    // match every un-backfilled character on the realm (MJ-2).
    if (!bnetAccountId)
    {
        SendPacket(WorldPackets::Misc::AccountCharacterCurrencyLists().Write());
        return;
    }

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

    auto sendResult = [this, currencyId = packet.CurrencyID](AccountCurrencyTransferResult res)
    {
        WorldPackets::Misc::CurrencyTransferResult result;
        result.CurrencyID = currencyId;
        result.Result = res;
        SendPacket(result.Write());
    };

    // Warband economy operations require a linked Battle.net account. With bnetId == 0 every
    // unlinked account collapses into one shared namespace and could enumerate/drain any
    // character (MJ-2), so refuse outright.
    uint32 bnetAccountId = GetBattlenetAccountId();
    if (!bnetAccountId)
    {
        sendResult(AccountCurrencyTransferResult::ServerError);
        return;
    }

    // Validate currency
    CurrencyTypesEntry const* currencyType = sCurrencyTypesStore.LookupEntry(packet.CurrencyID);
    if (!currencyType || !currencyType->IsAccountTransferable())
    {
        sendResult(AccountCurrencyTransferResult::InvalidCurrency);
        return;
    }

    if (packet.Quantity <= 0)
    {
        sendResult(AccountCurrencyTransferResult::InsufficientCurrency);
        return;
    }

    // Check source character is not logged in
    if (ObjectAccessor::FindPlayer(packet.SourceCharacterGUID))
    {
        sendResult(AccountCurrencyTransferResult::CharacterLoggedIn);
        return;
    }

    ObjectGuid sourceCharacterGuid = packet.SourceCharacterGUID;
    ObjectGuid::LowType sourceGuid = sourceCharacterGuid.GetCounter();

    // Reserve the source character for the duration of the read-modify-write. Two overlapping
    // transfers from the same source (a same-session burst or two same-bnet sessions) would
    // otherwise both read the same stale balance and each credit their destination while the
    // source is debited once — the TOCTOU currency dupe (CR-4). BeginCurrencyTransfer is an
    // atomic test-and-set; the reservation is released on every exit path of the callback.
    if (!sWorld->BeginCurrencyTransfer(sourceCharacterGuid))
    {
        sendResult(AccountCurrencyTransferResult::TransactionInProgress);
        return;
    }

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_CHARACTER_CURRENCIES);
    stmt->setUInt32(0, bnetAccountId);

    int32 currencyId = packet.CurrencyID;
    int32 requestedQuantity = packet.Quantity;

    _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this, currencyId, requestedQuantity, sourceGuid, sourceCharacterGuid, bnetAccountId](PreparedQueryResult result)
    {
        // Always release the source reservation, whatever happens below.
        auto finish = [&](AccountCurrencyTransferResult res, int32 quantity, int32 total)
        {
            sWorld->EndCurrencyTransfer(sourceCharacterGuid);

            WorldPackets::Misc::CurrencyTransferResult transferResult;
            transferResult.CurrencyID = currencyId;
            transferResult.Quantity = quantity;
            transferResult.TotalQuantity = total;
            transferResult.Result = res;
            SendPacket(transferResult.Write());
        };

        Player* player = GetPlayer();
        if (!player)
        {
            // Destination logged out mid-transfer: release the reservation, nothing to send.
            sWorld->EndCurrencyTransfer(sourceCharacterGuid);
            return;
        }

        CurrencyTypesEntry const* currencyType = sCurrencyTypesStore.LookupEntry(currencyId);
        if (!currencyType)
        {
            finish(AccountCurrencyTransferResult::InvalidCurrency, 0, 0);
            return;
        }

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
            finish(AccountCurrencyTransferResult::NoValidSourceCharacter, 0, 0);
            return;
        }

        if (sourceQuantity < requestedQuantity)
        {
            finish(AccountCurrencyTransferResult::InsufficientCurrency, 0, 0);
            return;
        }

        // Calculate received amount after transfer percentage
        int32 receivedAmount = static_cast<int32>(std::floor(requestedQuantity * currencyType->AccountTransferPercentage / 100.0f));
        if (receivedAmount <= 0)
        {
            finish(AccountCurrencyTransferResult::InsufficientCurrency, 0, 0);
            return;
        }

        // Check destination max quantity
        uint32 currentQuantity = player->GetCurrencyQuantity(currencyId);
        uint32 maxQuantity = player->GetCurrencyMaxQuantity(currencyType);
        if (maxQuantity && (currentQuantity + receivedAmount) > maxQuantity)
        {
            finish(AccountCurrencyTransferResult::MaxQuantity, 0, 0);
            return;
        }

        // Debit the source and write the log in a single transaction. The decrement is guarded
        // (SET Quantity = Quantity - ? WHERE ... AND Quantity >= ?) so the source balance can
        // never go negative even under a race, and the source reservation above guarantees no
        // concurrent transfer observed the same pre-debit balance.
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabasePreparedStatement* updateStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PLAYER_CURRENCY_QUANTITY_GUARDED);
        updateStmt->setInt32(0, requestedQuantity);
        updateStmt->setUInt64(1, sourceGuid);
        updateStmt->setUInt16(2, currencyId);
        updateStmt->setInt32(3, requestedQuantity);
        trans->Append(updateStmt);

        // Log both the sent (requested) and received (post-tax) amounts (MJ-4 / MN-1).
        CharacterDatabasePreparedStatement* logStmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_WARBAND_CURRENCY_TRANSFER_LOG);
        logStmt->setUInt32(0, bnetAccountId);
        logStmt->setUInt32(1, currencyId);
        logStmt->setUInt64(2, sourceGuid);
        logStmt->setUInt64(3, player->GetGUID().GetCounter());
        logStmt->setInt32(4, requestedQuantity);
        logStmt->setInt32(5, receivedAmount);
        logStmt->setUInt32(6, uint32(GameTime::GetGameTime()));
        trans->Append(logStmt);

        CharacterDatabase.CommitTransaction(trans);

        // Credit the destination (online player, in-memory). Ordered AFTER the source debit
        // commits so a crash in between loses currency (recoverable from the log) rather than
        // duplicating it.
        player->ModifyCurrency(currencyId, receivedAmount, CurrencyGainSource::AccountCopy);

        finish(AccountCurrencyTransferResult::Ok, receivedAmount, player->GetCurrencyQuantity(currencyId));
    }));
}

void WorldSession::HandleGetCharacterCurrencyTransferLog(WorldPackets::Misc::GetCharacterCurrencyTransferLog& /*packet*/)
{
    uint32 bnetAccountId = GetBattlenetAccountId();

    if (!bnetAccountId)
    {
        SendPacket(WorldPackets::Misc::CurrencyTransferLog().Write());
        return;
    }

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

                // SELECT order: currencyTypeId, source, dest, quantity(sent), receivedQuantity, timestamp
                WorldPackets::Misc::CurrencyTransferLog::CurrencyTransferLogEntry entry;
                entry.CurrencyTypeID = fields[0].GetUInt32();
                entry.SourceCharacterGUID = ObjectGuid::Create<HighGuid::Player>(fields[1].GetUInt64());
                entry.DestCharacterGUID = ObjectGuid::Create<HighGuid::Player>(fields[2].GetUInt64());
                entry.QuantitySent = fields[3].GetInt32();
                entry.QuantityReceived = fields[4].GetInt32();
                entry.Timestamp = fields[5].GetUInt32();
                response.Entries.push_back(std::move(entry));

            } while (result->NextRow());
        }

        SendPacket(response.Write());
    }));
}

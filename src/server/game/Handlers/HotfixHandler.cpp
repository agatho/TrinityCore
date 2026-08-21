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
#include "GameTime.h"
#include "HotfixPackets.h"
#include "Log.h"
#include "MapUtils.h"
#include "World.h"

void WorldSession::HandleDBQueryBulk(WorldPackets::Hotfix::DBQueryBulk& dbQuery)
{
    DB2StorageBase const* store = sDB2Manager.GetStorage(dbQuery.TableHash);
    for (WorldPackets::Hotfix::DBQueryBulk::DBQueryRecord const& record : dbQuery.Queries)
    {
        WorldPackets::Hotfix::DBReply dbReply;
        dbReply.TableHash = dbQuery.TableHash;
        dbReply.RecordID = record.RecordID;

        if (store && store->HasRecord(record.RecordID))
        {
            dbReply.Status = DB2Manager::HotfixRecord::Status::Valid;
            dbReply.Timestamp = GameTime::GetGameTime();
            store->WriteRecord(record.RecordID, GetSessionDbcLocale(), dbReply.Data);

            if (std::vector<DB2Manager::HotfixOptionalData> const* optionalDataEntries = sDB2Manager.GetHotfixOptionalData(dbQuery.TableHash, record.RecordID, GetSessionDbcLocale()))
            {
                for (DB2Manager::HotfixOptionalData const& optionalData : *optionalDataEntries)
                {
                    dbReply.Data << uint32(optionalData.Key);
                    dbReply.Data.append(optionalData.Data.data(), optionalData.Data.size());
                }
            }
        }
        else
        {
            TC_LOG_TRACE("network", "CMSG_DB_QUERY_BULK: {} requested non-existing entry {} in datastore: {}", GetPlayerInfo(), record.RecordID, dbQuery.TableHash);
            dbReply.Timestamp = GameTime::GetGameTime();
        }

        SendPacket(dbReply.Write());
    }
}

void WorldSession::SendAvailableHotfixes()
{
    WorldPackets::Hotfix::AvailableHotfixes availableHotfixes;
    availableHotfixes.VirtualRealmAddress = GetVirtualRealmAddress();

    for (auto const& [pushId, push] : sDB2Manager.GetHotfixData())
    {
        if (!(push.AvailableLocalesMask & (1 << GetSessionDbcLocale())))
            continue;

        availableHotfixes.Hotfixes.insert(push.Records.front().ID);
    }

    SendPacket(availableHotfixes.Write());
}

// Shared record builder for SMSG_HOTFIX_CONNECT and SMSG_HOTFIX_MESSAGE - both carry the identical
// payload (client element reader RVA 0x72AEA0 is used for both cases of the family 0x49 dispatcher).
void WorldSession::BuildHotfixRecords(std::span<int32 const> hotfixIds, std::vector<WorldPackets::Hotfix::HotfixData>& records, ByteBuffer& content) const
{
    DB2Manager::HotfixContainer const& hotfixes = sDB2Manager.GetHotfixData();
    records.reserve(records.size() + hotfixIds.size());
    for (int32 hotfixId : hotfixIds)
    {
        if (DB2Manager::HotfixPush const* hotfixRecords = Trinity::Containers::MapGetValuePtr(hotfixes, hotfixId))
        {
            for (DB2Manager::HotfixRecord const& hotfixRecord : hotfixRecords->Records)
            {
                if (!(hotfixRecord.AvailableLocalesMask & (1 << GetSessionDbcLocale())))
                    continue;

                WorldPackets::Hotfix::HotfixData& hotfixData = records.emplace_back();
                hotfixData.Record = hotfixRecord;
                if (hotfixRecord.HotfixStatus == DB2Manager::HotfixRecord::Status::Valid)
                {
                    DB2StorageBase const* storage = sDB2Manager.GetStorage(hotfixRecord.TableHash);
                    if (storage && storage->HasRecord(uint32(hotfixRecord.RecordID)))
                    {
                        std::size_t pos = content.size();
                        storage->WriteRecord(uint32(hotfixRecord.RecordID), GetSessionDbcLocale(), content);

                        if (std::vector<DB2Manager::HotfixOptionalData> const* optionalDataEntries = sDB2Manager.GetHotfixOptionalData(hotfixRecord.TableHash, hotfixRecord.RecordID, GetSessionDbcLocale()))
                        {
                            for (DB2Manager::HotfixOptionalData const& optionalData : *optionalDataEntries)
                            {
                                content << uint32(optionalData.Key);
                                content.append(optionalData.Data.data(), optionalData.Data.size());
                            }
                        }

                        hotfixData.Size = content.size() - pos;
                    }
                    else if (std::vector<uint8> const* blobData = sDB2Manager.GetHotfixBlobData(hotfixRecord.TableHash, hotfixRecord.RecordID, GetSessionDbcLocale()))
                    {
                        hotfixData.Size = blobData->size();
                        content.append(blobData->data(), blobData->size());
                    }
                    else
                        // Do not send Status::Valid when we don't have a hotfix blob for current locale
                        hotfixData.Record.HotfixStatus = storage ? DB2Manager::HotfixRecord::Status::RecordRemoved : DB2Manager::HotfixRecord::Status::Invalid;
                }
            }
        }
    }
}

// Unsolicited push of hotfix records to an already connected client (SMSG_HOTFIX_MESSAGE, client
// handler RVA 0x4A9BB0 - applies the records to the client's DB2 stores exactly like the reply to
// CMSG_HOTFIX_REQUEST does). TrinityCore currently mutates hotfix data only during startup, so no
// core code path triggers this yet; the sender exists for scripts and for a future runtime reload.
void WorldSession::SendHotfixMessage(std::span<int32 const> hotfixIds)
{
    WorldPackets::Hotfix::HotfixMessage hotfixMessage;
    BuildHotfixRecords(hotfixIds, hotfixMessage.Hotfixes, hotfixMessage.HotfixContent);
    if (hotfixMessage.Hotfixes.empty())
        return;

    SendPacket(hotfixMessage.Write());
}

void WorldSession::HandleHotfixRequest(WorldPackets::Hotfix::HotfixRequest& hotfixQuery)
{
    WorldPackets::Hotfix::HotfixConnect hotfixQueryResponse;
    BuildHotfixRecords(hotfixQuery.Hotfixes, hotfixQueryResponse.Hotfixes, hotfixQueryResponse.HotfixContent);

    SendPacket(hotfixQueryResponse.Write());
}

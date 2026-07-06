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
#include "ReferAFriendPackets.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryCallback.h"
#include "QuestDef.h"
#include <string>

// Builds and sends SMSG_RAF_ACCOUNT_INFO for this account, listing the accounts it has recruited. The recruit list
// is an account-wide login-DB lookup, so it is resolved with an async callback rather than blocking the world thread.
void WorldSession::SendRafAccountInfo(uint32 field)
{
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_RAF_RECRUITS);
    stmt->setUInt32(0, GetBattlenetAccountId());

    GetQueryProcessor().AddCallback(LoginDatabase.AsyncQuery(stmt).WithPreparedCallback([this, field](PreparedQueryResult result)
    {
        WorldPackets::RaF::RafAccountInfo response;
        response.Field20 = field;   // echo the client's leading field

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                WorldPackets::RaF::RafRecruit& recruit = response.Recruits.emplace_back();
                recruit.Fields[0] = fields[0].GetUInt32();   // recruit account id
                recruit.Name = fields[1].GetString();
            } while (result->NextRow());
        }

        SendPacket(response.Write());
    }));
}

// The client opens the RAF panel by requesting account info.
void WorldSession::HandleGetRafAccountInfo(WorldPackets::RaF::GetRafAccountInfo& packet)
{
    SendRafAccountInfo(packet.Field);
}

// The client asks the server to mint (or re-fetch) this account's recruitment code. The code is a stable,
// per-account token another account supplies when it is recruited; it is persisted and the RAF panel is refreshed.
void WorldSession::HandleRafGenerateRecruitmentLink(WorldPackets::RaF::RafGenerateRecruitmentLink& packet)
{
    uint32 accountId = GetBattlenetAccountId();
    std::string code = "R" + std::to_string(accountId);   // stable, unique per account

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_RAF_CODE);
    stmt->setUInt32(0, accountId);
    stmt->setString(1, code);
    LoginDatabase.Execute(stmt);

    SendRafAccountInfo(packet.Field);
}

void WorldSession::SendClaimRafRewardResult(uint32 result)
{
    WorldPackets::RaF::ClaimRafRewardResponse response;
    response.Result = result;
    SendPacket(response.Write());
}

// Claims a specific Recruit-A-Friend reward activity. Each activity maps (via RafActivity.db2) to a RewardQuest
// that delivers the actual reward, so we grant that quest's rewards through the normal quest reward path. A claim
// is honoured only when the account has recruited someone and has not already claimed this activity - a
// server-authoritable gate. (The exact Blizzlike gate is a recruited-months threshold evaluated from the
// CriteriaTree; those months are external subscription data the server lacks offline, so recruit-count stands in
// for it here.)
void WorldSession::HandleRafClaimActivityReward(WorldPackets::RaF::RafClaimActivityReward& packet)
{
    RafActivityEntry const* activity = sRafActivityStore.LookupEntry(packet.ActivityID);
    if (!activity)
    {
        SendClaimRafRewardResult(1);   // unknown activity (Result != 0 -> failure; exact codes unconfirmed)
        return;
    }

    uint32 accountId = GetBattlenetAccountId();
    uint32 activityId = packet.ActivityID;
    int32 rewardQuestId = activity->RewardQuestID;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_RAF_CLAIM_ELIGIBILITY);
    stmt->setUInt32(0, accountId);   // recruiterAccountId (recruit count)
    stmt->setUInt32(1, accountId);   // accountId (already-claimed check)
    stmt->setUInt32(2, activityId);

    GetQueryProcessor().AddCallback(LoginDatabase.AsyncQuery(stmt).WithPreparedCallback([this, accountId, activityId, rewardQuestId](PreparedQueryResult result)
    {
        uint64 recruitCount = 0;
        uint64 alreadyClaimed = 0;
        if (result)
        {
            Field* fields = result->Fetch();
            recruitCount = fields[0].GetUInt64();
            alreadyClaimed = fields[1].GetUInt64();
        }

        if (alreadyClaimed > 0 || recruitCount < 1)
        {
            SendClaimRafRewardResult(1);   // already claimed, or not eligible
            return;
        }

        Player* player = GetPlayer();
        if (!player)
            return;

        if (Quest const* quest = sObjectMgr->GetQuestTemplate(uint32(rewardQuestId)))
            player->RewardQuest(quest, LootItemType::Item, 0, player, false);

        LoginDatabasePreparedStatement* ins = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_RAF_CLAIMED);
        ins->setUInt32(0, accountId);
        ins->setUInt32(1, activityId);
        LoginDatabase.Execute(ins);

        SendClaimRafRewardResult(0);   // success
    }));
}

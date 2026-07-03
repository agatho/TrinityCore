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
#include "ChallengeMode.h"
#include "ChallengeModeMgr.h"
#include "ChallengeModePackets.h"
#include "Item.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"

void WorldSession::HandleRequestMythicPlusSeasonData(WorldPackets::ChallengeMode::RequestMythicPlusSeasonData& /*requestMythicPlusSeasonData*/)
{
    WorldPackets::ChallengeMode::MythicPlusSeasonData response;
    response.IsMythicPlusActive = sChallengeModeMgr.GetActiveSeasonId() != 0;
    SendPacket(response.Write());
}

void WorldSession::HandleRequestMythicPlusAffixes(WorldPackets::ChallengeMode::RequestMythicPlusAffixes& /*requestMythicPlusAffixes*/)
{
    WorldPackets::ChallengeMode::MythicPlusCurrentAffixes response;

    int32 const seasonId = int32(sChallengeModeMgr.GetActiveSeasonId());
    for (uint32 affixId : sChallengeModeMgr.GetWeeklyAffixes())
    {
        WorldPackets::ChallengeMode::CurrentAffix& affix = response.Affixes.emplace_back();
        affix.KeystoneAffixID = int32(affixId);
        affix.SeasonID = seasonId;
    }

    SendPacket(response.Write());
}

void WorldSession::HandleStartChallengeMode(WorldPackets::ChallengeMode::StartChallengeMode& startChallengeMode)
{
    Player* player = GetPlayer();

    // The keystone the player slotted into the font of power.
    Item* keystone = player->GetItemByPos(startChallengeMode.Bag, startChallengeMode.Slot);
    if (!keystone)
    {
        player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    uint32 const mapChallengeModeId = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID);
    uint32 const keystoneLevel = keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL);
    if (!mapChallengeModeId || !keystoneLevel)
        return;

    // The player must be standing in the matching Mythic Keystone instance for that dungeon.
    Map* map = player->GetMap();
    InstanceMap* instanceMap = map->ToInstanceMap();
    if (!instanceMap || !map->IsMythicPlus())
        return;

    if (sChallengeModeMgr.GetMapIdForChallengeMode(mapChallengeModeId) != map->GetId())
        return;

    ChallengeMode* challenge = instanceMap->GetChallengeMode();
    if (!challenge || challenge->IsActive() || challenge->IsCompleted())
        return;

    std::array<uint32, 4> const affixes =
    {
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_1),
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_2),
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_3),
        keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_4)
    };

    challenge->Start(mapChallengeModeId, keystoneLevel, affixes, player->GetGUID(), keystone->GetGUID());
}

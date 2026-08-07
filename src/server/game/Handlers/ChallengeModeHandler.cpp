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
#include "Config.h"
#include "Item.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Map.h"
#include "MythicPlusData.h"
#include "Player.h"

// NOTE (assembly): the Great Vault reward-item logic for the Mythic+ row deliberately does NOT live in this file.
// It is a service on ChallengeModeMgr (BuildMythicPlusVaultOptions / ClaimMythicPlusVaultReward /
// GetMythicPlusVaultSlotForThreshold) so that whichever handler an assembly binds to CMSG_REQUEST_WEEKLY_REWARDS /
// CMSG_CLAIM_WEEKLY_REWARD can drive it. On this branch that is the pair below, over the ChallengeMode packet
// family; integration/all-systems binds WeeklyRewardHandler.cpp over the WeeklyRewards packet family (it also
// serves the Raid and World vault rows) and calls the same service for the Mythic+ row. There is exactly one
// handler bound per opcode in either assembly - the granting rules are shared, never duplicated.

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

    // Only the actual Mythic Keystone item may start a run (config-tunable; 0 disables the check).
    if (uint32 keystoneItemId = sChallengeModeMgr.GetKeystoneItemId())
        if (keystone->GetEntry() != keystoneItemId)
            return;

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

    // Retail activates the run from the Font of Power pedestal. When enforced, require the pedestal gameobject
    // near the player; lenient by default because the GO spawn is world-DB content.
    if (sConfigMgr->GetBoolDefault("ChallengeMode.RequireFontOfPower", false))
        if (!player->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_CHALLENGE_MODE_REWARD, 40.0f))
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

void WorldSession::HandleMythicPlusRequestMapStats(WorldPackets::ChallengeMode::MythicPlusRequestMapStats& /*request*/)
{
    Player* player = GetPlayer();

    WorldPackets::ChallengeMode::MythicPlusAllMapStats response;
    response.Field80 = sChallengeModeMgr.GetActiveSeasonId();

    // One map-stat row per dungeon the player has a recorded best run for. The requester is the sole member; the
    // full party roster is not persisted server-side. MapChallengeModeID/BestLevel/DurationMs/Affixes are populated;
    // the remaining scalar slots await a live sniff to map (the wire is exact, so a zero there is not a desync).
    if (MythicPlusData* data = player->GetMythicPlusData())
    {
        for (auto const& [challengeModeId, run] : data->GetBestRuns())
        {
            WorldPackets::ChallengeMode::MythicPlusMapStat& mapStat = response.MapStats.emplace_back();
            mapStat.MapChallengeModeID = challengeModeId;
            mapStat.BestLevel = run.Level;
            mapStat.DurationMs = run.DurationMs;
            mapStat.Affixes = run.Affixes;

            WorldPackets::ChallengeMode::MythicPlusMapStatMember& member = mapStat.Members.emplace_back();
            member.PlayerGUID = player->GetGUID();
        }
    }

    SendPacket(response.Write());
}

// NOTE: feature/mythic-plus also implemented WorldSession::HandleRequestWeeklyRewards /
// HandleClaimWeeklyReward here, over WorldPackets::ChallengeMode::* packet classes. integration already
// binds CMSG_REQUEST_WEEKLY_REWARDS / CMSG_CLAIM_WEEKLY_REWARD to the WorldPackets::WeeklyRewards::*
// handlers in WeeklyRewardHandler.cpp, which cover all three vault rows (Dungeon / Raid / World) rather
// than the Mythic+ row alone. One opcode can only have one handler, so the multi-row version stays bound
// and the Mythic+-only duplicates are not carried into the assembly. The one behaviour they had that the
// bound handler lacked - refreshing the carried keystone when the vault is opened after a weekly reset -
// is called from WeeklyRewardHandler.cpp instead. Folding the Mythic+ reward-item rolling into
// WeeklyRewardsMgr is a follow-up on feature/mythic-plus, not something to hand-write on integration.

void WorldSession::HandleResetChallengeMode(WorldPackets::ChallengeMode::ResetChallengeMode& /*resetChallengeMode*/)
{
    InstanceMap* instanceMap = GetPlayer()->GetMap()->ToInstanceMap();
    if (!instanceMap)
        return;

    // Abort the active run and stop the timer. Trash/boss respawn goes through the standard instance reset path.
    if (ChallengeMode* challenge = instanceMap->GetChallengeMode())
    {
        // Only the player who started the run (the keystone owner) may reset it. Without this any group member -
        // or anyone who wandered into the instance - could send CMSG_RESET_CHALLENGE_MODE and abort the whole
        // party's active keystone at any time.
        if (challenge->GetStarterGuid() != GetPlayer()->GetGUID())
            return;

        if (challenge->IsActive())
        {
            challenge->Reset();

            // Notify the party UI that the keystone was reset (SMSG_CHALLENGE_MODE_RESET carries the instance MapID).
            WorldPackets::ChallengeMode::ChallengeModeReset resetPacket;
            resetPacket.MapID = instanceMap->GetId();
            instanceMap->SendToPlayers(resetPacket.Write());
        }
    }
}

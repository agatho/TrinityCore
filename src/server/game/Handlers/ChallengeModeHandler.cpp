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
#include "ChallengeModeMgr.h"
#include "ChallengeModePackets.h"

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

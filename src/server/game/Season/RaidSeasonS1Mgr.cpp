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

#include "RaidSeasonS1Mgr.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"

RaidSeasonS1Mgr& RaidSeasonS1Mgr::Instance()
{
    static RaidSeasonS1Mgr instance;
    return instance;
}

void RaidSeasonS1Mgr::Initialize()
{
    _curioRewards.clear();

    // Optional world content: the Tier-35 token the Chiming Void Curio yields per (class, slot, difficulty).
    // This mapping is server content -- it is NOT in any client DB2 -- and the token item ids are CAPTURE-BLOCKED
    // at 68887, so the table ships EMPTY. Loading it here is realm-safe: an absent/empty table => idle no-op, and
    // the attested delivery surface (vendor trade at NPC Kirana) is world SQL that needs no core code.
    //
    // Schema (ships in sql/updates/world/master/..._world_raid_season_s1.sql):
    //   raid_season_curio_reward(ClassID TINYINT, InventoryType TINYINT, DifficultyID SMALLINT, TokenItemID INT)
    if (QueryResult result = WorldDatabase.Query("SELECT ClassID, InventoryType, DifficultyID, TokenItemID FROM raid_season_curio_reward"))
    {
        do
        {
            Field* fields = result->Fetch();
            CurioRewardRow row;
            row.ClassID       = fields[0].GetUInt8();
            row.InventoryType = fields[1].GetUInt8();
            row.DifficultyId  = Difficulty(fields[2].GetInt16());
            row.TokenItemID   = fields[3].GetUInt32();
            _curioRewards.push_back(row);
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> RaidSeasonS1Mgr initialised ({} Chiming Void Curio reward rows).", _curioRewards.size());
}

// --- flexible Mythic (15-25) recognition ---

bool RaidSeasonS1Mgr::IsFlexMythicRaid(Difficulty difficulty)
{
    return difficulty == RaidSeasonS1::SPOREFALL_MYTHIC_FLEX;
}

bool RaidSeasonS1Mgr::IsSporefallFlexMythic(uint32 mapId, Difficulty difficulty)
{
    return mapId == RaidSeasonS1::SPOREFALL_MAP_ID && IsFlexMythicRaid(difficulty);
}

uint8 RaidSeasonS1Mgr::GetFlexMythicMinPlayers()
{
    return RaidSeasonS1::FLEX_MYTHIC_MIN_PLAYERS;
}

uint8 RaidSeasonS1Mgr::GetFlexMythicMaxPlayers()
{
    return RaidSeasonS1::FLEX_MYTHIC_MAX_PLAYERS;
}

// --- Chiming Void Curio redemption seam ---

bool RaidSeasonS1Mgr::IsCurio(Item const* item)
{
    return item && item->GetEntry() == RaidSeasonS1::CURIO_ITEM_ID;
}

uint32 RaidSeasonS1Mgr::GetCurioReward(uint8 classId, uint8 inventoryType, Difficulty difficulty) const
{
    // Most-specific match first (exact class+slot+difficulty), then progressively wildcarded (0/DIFFICULTY_NONE).
    uint32 best = 0;
    int bestScore = -1;
    for (CurioRewardRow const& row : _curioRewards)
    {
        if (row.ClassID != 0 && row.ClassID != classId)
            continue;
        if (row.InventoryType != 0 && row.InventoryType != inventoryType)
            continue;
        if (row.DifficultyId != DIFFICULTY_NONE && row.DifficultyId != difficulty)
            continue;

        int score = (row.ClassID != 0 ? 4 : 0) + (row.InventoryType != 0 ? 2 : 0) + (row.DifficultyId != DIFFICULTY_NONE ? 1 : 0);
        if (score > bestScore)
        {
            bestScore = score;
            best = row.TokenItemID;
        }
    }
    return best;
}

uint32 RaidSeasonS1Mgr::RedeemCurio(Player* /*player*/, Item* curio, uint8 /*chosenInventoryType*/) const
{
    if (!IsCurio(curio))
        return 0;

    // CAPTURE-BLOCKED: the Tier-35 token ids the Curio yields per (class, slot, difficulty) are not in DB2 @68887
    // and raid_season_curio_reward ships empty, so no server-side grant is performed here. The ATTESTED redemption
    // is the vendor trade at NPC Kirana (npc_vendor + ItemExtendedCost item-cost, shipped as world SQL). When the
    // token ids are captured, seed raid_season_curio_reward and wire this seam to consume the Curio + grant the
    // chosen token (respecting the item's difficulty bonus so a Mythic Curio yields a Mythic token).
    // TODO(RAID-S1): implement server-driven omni redemption once token ids are captured.
    return 0;
}

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

#include "DelvesCompanion.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "TemporarySummon.h"

namespace Delves
{

void DelvesCompanion::LoadFromDB(uint32 battlenetAccountId, CompanionState& state)
{
    // Load companion state from characters.delve_companion
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_DELVE_COMPANION);
    stmt->setUInt32(0, battlenetAccountId);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        Field* fields = result->Fetch();
        state.CompanionId       = fields[0].GetUInt32();
        state.Level             = fields[1].GetUInt32();
        state.Xp                = fields[2].GetUInt32();
        state.SelectedRole      = static_cast<CompanionRole>(fields[3].GetUInt8());
        state.CombatCurioNodeId = fields[4].GetUInt32();
        state.UtilityCurioNodeId = fields[5].GetUInt32();

        TC_LOG_DEBUG("scripts.delves", "Loaded companion state for account {}: id={}, level={}, role={}",
            battlenetAccountId, state.CompanionId, state.Level, static_cast<uint8>(state.SelectedRole));
    }
    else
    {
        // Default companion state - first companion (Brann)
        PlayerCompanionInfoEntry const* defaultInfo = GetDefaultCompanionInfo();
        if (defaultInfo)
            state.CompanionId = defaultInfo->ID;
        state.Level = 1;
        state.Xp = 0;
        state.SelectedRole = CompanionRole::Dps;

        TC_LOG_DEBUG("scripts.delves", "No companion data for account {}, using defaults (companion {})",
            battlenetAccountId, state.CompanionId);
    }
}

void DelvesCompanion::SaveToDB(uint32 battlenetAccountId, CompanionState const& state)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_DELVE_COMPANION);
    stmt->setUInt32(0, battlenetAccountId);
    stmt->setUInt32(1, state.CompanionId);
    stmt->setUInt32(2, state.Level);
    stmt->setUInt32(3, state.Xp);
    stmt->setUInt8(4, static_cast<uint8>(state.SelectedRole));
    stmt->setUInt32(5, state.CombatCurioNodeId);
    stmt->setUInt32(6, state.UtilityCurioNodeId);
    CharacterDatabase.Execute(stmt);
}

Creature* DelvesCompanion::SpawnCompanion(InstanceMap* map, Player* owner, CompanionState const& state, Position const& pos)
{
    PlayerCompanionInfoEntry const* info = sPlayerCompanionInfoStore.LookupEntry(state.CompanionId);
    if (!info)
    {
        TC_LOG_ERROR("scripts.delves", "Failed to spawn companion: PlayerCompanionInfo entry {} not found", state.CompanionId);
        return nullptr;
    }

    // Use the CreatureDisplayInfoID to find the creature template
    // The companion creature entry should be configured in creature_template with the companion AI script
    uint32 creatureEntry = info->CreatureDisplayInfoID; // TODO: Map to actual creature_template entry

    TempSummon* companion = map->SummonCreature(creatureEntry, pos, nullptr, 0ms, owner);
    if (!companion)
    {
        TC_LOG_ERROR("scripts.delves", "Failed to summon companion creature entry {} at ({}, {}, {})",
            creatureEntry, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
        return nullptr;
    }

    // Configure the companion
    companion->SetFaction(owner->GetFaction());
    companion->SetReactState(REACT_HELPER);

    // Scale stats based on companion level
    // TODO Phase 3+: Apply level-based stat scaling, role-specific auras

    TC_LOG_DEBUG("scripts.delves", "Spawned companion {} (entry {}, level {}, role {}) for player {}",
        companion->GetGUID().ToString(), creatureEntry, state.Level,
        static_cast<uint8>(state.SelectedRole), owner->GetName());

    return companion;
}

void DelvesCompanion::DespawnCompanion(InstanceMap* /*map*/, ObjectGuid companionGuid)
{
    // Companion is a TempSummon, will be cleaned up with the instance
    // This can be used for explicit despawn if needed
    TC_LOG_DEBUG("scripts.delves", "DespawnCompanion called for {}", companionGuid.ToString());
}

PlayerCompanionInfoEntry const* DelvesCompanion::GetDefaultCompanionInfo()
{
    // Return the first companion entry (Brann Bronzebeard)
    for (PlayerCompanionInfoEntry const* entry : sPlayerCompanionInfoStore)
        return entry;

    return nullptr;
}

uint32 DelvesCompanion::GetCompanionDisplayId(PlayerCompanionInfoEntry const* info)
{
    if (!info)
        return 0;
    return info->CreatureDisplayInfoID;
}

void DelvesCompanion::AwardCompanionXP(uint32 battlenetAccountId, CompanionState& state, uint32 xpAmount)
{
    uint32 maxLevel = GetMaxCompanionLevel();
    if (state.Level >= maxLevel)
        return;

    state.Xp += xpAmount;

    // Level up loop
    while (state.Level < maxLevel)
    {
        uint32 xpNeeded = GetXPForLevel(state.Level);
        if (state.Xp < xpNeeded)
            break;

        state.Xp -= xpNeeded;
        state.Level++;

        TC_LOG_DEBUG("scripts.delves", "Companion leveled up to {} for account {}", state.Level, battlenetAccountId);
    }

    // Cap XP at max level
    if (state.Level >= maxLevel)
        state.Xp = 0;

    SaveToDB(battlenetAccountId, state);
}

uint32 DelvesCompanion::GetXPForLevel(uint32 level)
{
    // Approximate XP curve based on retail data
    // Total XP 1-60 is ~3,989,497 post-reduction
    // Average ~68k per level, scaling upward
    if (level == 0)
        return 0;

    return 40000 + (level * 1000);
}

uint32 DelvesCompanion::GetMaxCompanionLevel()
{
    // TODO: Determine from active season
    return MAX_COMPANION_LEVEL_S1; // 60 for Season 1
}

} // namespace Delves

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

// ============================================================================
// Patch 12.1.0 (build 69299) Housing Blueprint account store + requirement gate.
// RE spec: c:\dumps\tools\dump121\housing\housing_12_1_spec.md (§4, §6).
//
// Blueprints are per-BNet-account, NOT DB2-backed (no new DB2 record class in 12.1,
// spec §0). This is an in-memory authoritative store; DB persistence is a documented
// follow-up (see .cpp) — the logic here is real, not a stub.
// ============================================================================

#ifndef TRINITYCORE_HOUSING_BLUEPRINT_MGR_H
#define TRINITYCORE_HOUSING_BLUEPRINT_MGR_H

#include "Define.h"
#include "HousingDefines.h"
#include <string>
#include <unordered_map>
#include <vector>

class Housing;

// One stored blueprint (meta + contents + import requirements captured at export).
struct HousingStoredBlueprint
{
    uint64 Id = 0;
    std::string Uuid;
    std::string Name;
    HousingBlueprintType Type = HousingBlueprintType::House;
    int64 DateCreated = 0;
    int64 DateDeleted = 0;
    uint8 Flags = 0;

    // Contents (JamBlueprintItemList) — the item ID sets this blueprint reproduces.
    std::vector<uint32> DecorIDs;
    std::vector<uint32> DyeItemIDs;
    std::vector<uint32> RoomIDs;
    std::vector<uint32> FixtureIDs;

    // Requirements captured at export, checked on import (spec §6):
    int32 RequiredFaction = -1;     // -1 = none; else NeighborhoodFactionRestriction value
    uint32 RequiredHouseType = 0;   // 0 = none; else HouseExteriorWmoData id
    uint8 RequiredHouseSize = 0;    // 0 = none; else HousingFixtureSize
};

class TC_GAME_API HousingBlueprintMgr
{
public:
    HousingBlueprintMgr() = default;
    HousingBlueprintMgr(HousingBlueprintMgr const&) = delete;
    HousingBlueprintMgr& operator=(HousingBlueprintMgr const&) = delete;

    static HousingBlueprintMgr& Instance();

    // Collection accessors (per BNet account).
    std::vector<HousingStoredBlueprint> const& GetCollection(uint32 bnetAccountId) const;
    HousingStoredBlueprint const* Get(uint32 bnetAccountId, uint64 blueprintId) const;

    // Create/rename/delete. Create returns the assigned blueprint id, or 0 if the
    // per-account cap is reached (spec §5 caps).
    uint64 Create(uint32 bnetAccountId, HousingStoredBlueprint blueprint);
    bool Rename(uint32 bnetAccountId, uint64 blueprintId, std::string const& newName);
    bool Delete(uint32 bnetAccountId, uint64 blueprintId);

    // How many blueprints (total / backups) the account currently holds — cap checks.
    uint32 GetCount(uint32 bnetAccountId) const;
    uint32 GetBackupCount(uint32 bnetAccountId) const;

    // Import requirement gate — returns a HousingBlueprintUnmetRequirementFlags bitmask
    // (0 = all requirements met). Compares the blueprint's captured requirements against
    // the target house (spec §6).
    uint32 CheckRequirements(HousingStoredBlueprint const& blueprint, Housing const* targetHouse) const;

private:
    // bnetAccountId -> blueprints
    std::unordered_map<uint32, std::vector<HousingStoredBlueprint>> _byAccount;
    uint64 _nextId = 1;
    static std::vector<HousingStoredBlueprint> const _emptyCollection;
};

#define sHousingBlueprintMgr HousingBlueprintMgr::Instance()

#endif // TRINITYCORE_HOUSING_BLUEPRINT_MGR_H

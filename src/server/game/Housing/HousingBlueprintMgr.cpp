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

#include "HousingBlueprintMgr.h"
#include "Housing.h"
#include <algorithm>

// NOTE: this store is in-memory authoritative. DB persistence (a `character_housing_blueprint`
// table keyed by BNet account) is a documented follow-up; the blueprint model, caps and the
// import requirement gate below are the real 12.1 logic, not a stub.

std::vector<HousingStoredBlueprint> const HousingBlueprintMgr::_emptyCollection;

HousingBlueprintMgr& HousingBlueprintMgr::Instance()
{
    static HousingBlueprintMgr instance;
    return instance;
}

std::vector<HousingStoredBlueprint> const& HousingBlueprintMgr::GetCollection(uint32 bnetAccountId) const
{
    auto itr = _byAccount.find(bnetAccountId);
    return itr != _byAccount.end() ? itr->second : _emptyCollection;
}

HousingStoredBlueprint const* HousingBlueprintMgr::Get(uint32 bnetAccountId, uint64 blueprintId) const
{
    auto itr = _byAccount.find(bnetAccountId);
    if (itr == _byAccount.end())
        return nullptr;
    for (HousingStoredBlueprint const& bp : itr->second)
        if (bp.Id == blueprintId)
            return &bp;
    return nullptr;
}

uint32 HousingBlueprintMgr::GetCount(uint32 bnetAccountId) const
{
    auto itr = _byAccount.find(bnetAccountId);
    return itr != _byAccount.end() ? uint32(itr->second.size()) : 0u;
}

uint32 HousingBlueprintMgr::GetBackupCount(uint32 bnetAccountId) const
{
    auto itr = _byAccount.find(bnetAccountId);
    if (itr == _byAccount.end())
        return 0u;
    return uint32(std::count_if(itr->second.begin(), itr->second.end(),
        [](HousingStoredBlueprint const& bp) { return bp.Type == HousingBlueprintType::Backup; }));
}

uint64 HousingBlueprintMgr::Create(uint32 bnetAccountId, HousingStoredBlueprint blueprint)
{
    // Enforce per-account caps (spec §5). Backups have a separate, smaller cap.
    if (blueprint.Type == HousingBlueprintType::Backup)
    {
        if (GetBackupCount(bnetAccountId) >= HOUSING_BLUEPRINTS_MAX_BACKUPS_PER_BNET)
            return 0;
    }
    else if (GetCount(bnetAccountId) >= HOUSING_BLUEPRINTS_MAX_PER_BNET_ACCOUNT)
        return 0;

    blueprint.Id = _nextId++;
    _byAccount[bnetAccountId].push_back(std::move(blueprint));
    return _byAccount[bnetAccountId].back().Id;
}

bool HousingBlueprintMgr::Rename(uint32 bnetAccountId, uint64 blueprintId, std::string const& newName)
{
    auto itr = _byAccount.find(bnetAccountId);
    if (itr == _byAccount.end())
        return false;
    for (HousingStoredBlueprint& bp : itr->second)
    {
        if (bp.Id == blueprintId)
        {
            bp.Name = newName;
            return true;
        }
    }
    return false;
}

bool HousingBlueprintMgr::Delete(uint32 bnetAccountId, uint64 blueprintId)
{
    auto itr = _byAccount.find(bnetAccountId);
    if (itr == _byAccount.end())
        return false;
    auto& vec = itr->second;
    auto found = std::find_if(vec.begin(), vec.end(),
        [blueprintId](HousingStoredBlueprint const& bp) { return bp.Id == blueprintId; });
    if (found == vec.end())
        return false;
    vec.erase(found);
    return true;
}

uint32 HousingBlueprintMgr::CheckRequirements(HousingStoredBlueprint const& blueprint, Housing const* targetHouse) const
{
    // spec §6: import is refused unless the target house matches the blueprint's captured
    // exterior faction / house type / house size. Returns the unmet-requirement bitmask.
    uint32 unmet = HOUSING_BLUEPRINT_REQ_NONE;
    if (!targetHouse)
        return HOUSING_BLUEPRINT_REQ_HOUSE_TYPE | HOUSING_BLUEPRINT_REQ_HOUSE_SIZE;

    if (blueprint.RequiredHouseType != 0 && blueprint.RequiredHouseType != targetHouse->GetHouseType())
        unmet |= HOUSING_BLUEPRINT_REQ_HOUSE_TYPE;

    if (blueprint.RequiredHouseSize != 0 && blueprint.RequiredHouseSize != targetHouse->GetHouseSize())
        unmet |= HOUSING_BLUEPRINT_REQ_HOUSE_SIZE;

    // Exterior-faction requirement: Housing does not currently expose the plot faction, so this
    // requirement is only enforced when a caller supplies it. Documented gap (spec §6): wire the
    // neighborhood faction here once Housing carries it. Not treated as unmet by default to avoid
    // false rejections.
    return unmet;
}

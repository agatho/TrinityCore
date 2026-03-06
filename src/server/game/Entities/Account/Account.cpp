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

#include "Account.h"
#include "Map.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"
#include "WorldSession.h"

namespace Battlenet
{
Account::Account(WorldSession* session, ObjectGuid guid, std::string&& name) : m_session(session), m_name(std::move(name))
{
    _Create(guid);

    // All three housing fragments belong on the BNetAccount entity (analysis doc verified).
    m_entityFragments.Add(WowCS::EntityFragment::FHousingPlayerHouse_C, false, WowCS::GetRawFragmentData(m_housingPlayerHouseData));
    m_entityFragments.Add(WowCS::EntityFragment::FNeighborhoodMirrorData_C, false, WowCS::GetRawFragmentData(m_neighborhoodMirrorData));
    m_entityFragments.Add(WowCS::EntityFragment::FHousingStorage_C, false, WowCS::GetRawFragmentData(m_housingStorageData));

    // Default value
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::DecorMaxOwnedCount), 5000);
}

void Account::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&Account::m_housingPlayerHouseData);
    m_values.ClearChangesMask(&Account::m_neighborhoodMirrorData);
    m_values.ClearChangesMask(&Account::m_housingStorageData);
    BaseEntity::ClearUpdateMask(remove);
}


std::string Account::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return m_name;
}

void Account::BuildUpdate(UpdateDataMapType& data_map)
{
    BuildUpdateChangesMask();

    if (Player* owner = m_session->GetPlayer())
        BuildFieldsUpdate(owner, data_map);

    ClearUpdateMask(false);
}

std::string Account::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nName: {}", BaseEntity::GetDebugInfo(), m_name);
}

UF::UpdateFieldFlag Account::GetUpdateFieldFlagsFor(Player const* target) const
{
    if (*target->m_playerData->BnetAccount == GetGUID())
        return UF::UpdateFieldFlag::Owner;

    return UF::UpdateFieldFlag::None;
}

bool Account::AddToObjectUpdate()
{
    if (Player* owner = m_session->GetPlayer(); owner && owner->IsInWorld())
    {
        owner->GetMap()->AddUpdateObject(this);
        return true;
    }

    return false;
}

void Account::RemoveFromObjectUpdate()
{
    if (Player* owner = m_session->GetPlayer(); owner && owner->IsInWorld())
        owner->GetMap()->RemoveUpdateObject(this);
}

void Account::SendUpdateToPlayer(Player* player)
{
    // BaseEntity::SendUpdateToPlayer is const and skips BuildUpdateChangesMask(),
    // so ContentsChangedMask is 0 and the VALUES_UPDATE contains no fragment data.
    // We must compute the mask before serializing so that pending fragment changes
    // (e.g., FHousingStorage_C populated by PopulateCatalogStorageEntries) are included.
    BuildUpdateChangesMask();
    BaseEntity::SendUpdateToPlayer(player);
    ClearUpdateMask(false);
}

void Account::SetHousingDecorStorageEntry(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 sourceType, std::string sourceValue)
{
    auto ref = m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::Decor, decorGuid);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::HouseGUID), houseGuid);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::SourceType), sourceType);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::SourceValue), std::move(sourceValue));
}

void Account::RemoveHousingDecorStorageEntry(ObjectGuid decorGuid)
{
    auto setter = m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::Decor);
    RemoveMapUpdateFieldValue(setter, decorGuid);
}

// FHousingPlayerHouse_C setters
void Account::SetHouseType(uint32 houseType)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::HouseType), houseType);
}

void Account::SetHouseSize(uint32 houseSize)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::HouseSize), houseSize);
}

void Account::SetHousePlotIndex(int32 plotIndex)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::PlotIndex), plotIndex);
}

void Account::SetHouseLevel(uint32 level)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::Level), level);
}

void Account::SetHouseFavor(uint64 favor)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::Favor), favor);
}

void Account::SetHouseBudgets(uint32 interiorDecor, uint32 exteriorDecor, uint32 room, uint32 fixture)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::InteriorDecorPlacementBudget), interiorDecor);
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::ExteriorDecorPlacementBudget), exteriorDecor);
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::RoomPlacementBudget), room);
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::ExteriorFixtureBudget), fixture);
}

void Account::SetHouseBnetAccount(ObjectGuid bnetAccountGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::BnetAccount), bnetAccountGuid);
}

void Account::SetHouseEntityGUID(ObjectGuid entityGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::EntityGUID), entityGuid);
}

// FNeighborhoodMirrorData_C setters
void Account::SetNeighborhoodMirrorName(std::string const& name)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Name), name);
}

void Account::SetNeighborhoodMirrorOwnerGUID(ObjectGuid ownerGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::OwnerGUID), ownerGuid);
}

void Account::AddNeighborhoodMirrorHouse(ObjectGuid houseGuid, ObjectGuid ownerGuid)
{
    UF::PlayerHouseInfo& houseInfo = AddDynamicUpdateFieldValue(m_values.ModifyValue(&Account::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Houses));
    houseInfo.HouseGUID = houseGuid;
    houseInfo.OwnerGUID = ownerGuid;
}

void Account::ClearNeighborhoodMirrorHouses()
{
    ClearDynamicUpdateFieldValues(m_values.ModifyValue(&Account::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Houses));
}

void Account::AddNeighborhoodMirrorManager(ObjectGuid bnetAccountGuid, ObjectGuid playerGuid)
{
    UF::HousingOwner& manager = AddDynamicUpdateFieldValue(m_values.ModifyValue(&Account::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Managers));
    manager.BnetAccountGUID = bnetAccountGuid;
    manager.PlayerGUID = playerGuid;
}

void Account::ClearNeighborhoodMirrorManagers()
{
    ClearDynamicUpdateFieldValues(m_values.ModifyValue(&Account::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Managers));
}

}

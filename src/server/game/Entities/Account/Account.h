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

#ifndef TRINITYCORE_ACCOUNT_H
#define TRINITYCORE_ACCOUNT_H

#include "BaseEntity.h"

class WorldSession;

namespace Battlenet
{
class Account final : public BaseEntity
{
public:
    explicit Account(WorldSession* session, ObjectGuid guid, std::string&& name);

    void ClearUpdateMask(bool remove) override;

    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;

    void BuildUpdate(UpdateDataMapType& data_map) override;

    std::string GetDebugInfo() const override;

    // Direct send that builds ContentsChangedMask before serializing.
    // BaseEntity::SendUpdateToPlayer is const and doesn't call BuildUpdateChangesMask(),
    // so the VALUES_UPDATE packet is empty when called directly from handlers.
    // This override ensures fragment changes are detected before the send.
    void SendUpdateToPlayer(Player* player);

    // Housing storage data (decor catalog)
    void SetHousingDecorStorageEntry(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 sourceType, std::string sourceValue = {});
    void RemoveHousingDecorStorageEntry(ObjectGuid decorGuid);

    // FHousingPlayerHouse_C setters
    void SetHouseType(uint32 houseType);
    void SetHouseSize(uint32 houseSize);
    void SetHousePlotIndex(int32 plotIndex);
    void SetHouseLevel(uint32 level);
    void SetHouseFavor(uint64 favor);
    void SetHouseBudgets(uint32 interiorDecor, uint32 exteriorDecor, uint32 room, uint32 fixture);
    void SetHouseBnetAccount(ObjectGuid bnetAccountGuid);
    void SetHouseEntityGUID(ObjectGuid entityGuid);

    // FNeighborhoodMirrorData_C setters
    void SetNeighborhoodMirrorName(std::string const& name);
    void SetNeighborhoodMirrorOwnerGUID(ObjectGuid ownerGuid);
    void AddNeighborhoodMirrorHouse(ObjectGuid houseGuid, ObjectGuid ownerGuid);
    void ClearNeighborhoodMirrorHouses();
    void AddNeighborhoodMirrorManager(ObjectGuid bnetAccountGuid, ObjectGuid playerGuid);
    void ClearNeighborhoodMirrorManagers();

    UF::UpdateField<UF::HousingPlayerHouseData, int32(WowCS::EntityFragment::FHousingPlayerHouse_C), 0> m_housingPlayerHouseData;
    UF::UpdateField<UF::NeighborhoodMirrorData, int32(WowCS::EntityFragment::FNeighborhoodMirrorData_C), 0> m_neighborhoodMirrorData;
    UF::UpdateField<UF::HousingStorageData, int32(WowCS::EntityFragment::FHousingStorage_C), 0> m_housingStorageData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;

    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;

private:
    WorldSession* m_session;
    std::string m_name;
};
}

#endif // TRINITYCORE_ACCOUNT_H

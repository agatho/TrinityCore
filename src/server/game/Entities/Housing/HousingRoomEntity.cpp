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

#include "HousingRoomEntity.h"
#include "Log.h"
#include "Map.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"

HousingRoomEntity::HousingRoomEntity()
    : WorldObject(false)
{
    m_objectTypeId = TYPEID_HOUSING_ENTITY; // 18 — retail objectType for housing entities

    m_updateFlag.HasEntityPosition = true;
    m_updateFlag.Stationary = true;

    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoom_C, false, WowCS::GetRawFragmentData(m_housingRoomData));
    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false, WowCS::GetRawFragmentData(m_mirroredPositionData));
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HousingRoom, false);
}

bool HousingRoomEntity::Create(ObjectGuid guid, Map* map, Position const& pos)
{
    _Create(guid);
    SetMap(map);
    Relocate(pos);
    SetObjectScale(1.0f);

    if (!GetMap()->AddToMap(this))
        return false;

    return true;
}

void HousingRoomEntity::AddToWorld()
{
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().Insert<HousingRoomEntity>(this);
        WorldObject::AddToWorld();
    }
}

void HousingRoomEntity::RemoveFromWorld()
{
    if (IsInWorld())
    {
        WorldObject::RemoveFromWorld();
        GetMap()->GetObjectsStore().Remove<HousingRoomEntity>(this);
    }
}

void HousingRoomEntity::BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // objectType=18 entities use entity fragments only — no CGObject/UnitData/etc.
    // The fragment data is serialized by the standard BaseEntity fragment loop
    // in Object::BuildCreateUpdateBlockForPlayer.
}

void HousingRoomEntity::BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // VALUES updates use the standard fragment change mask system
}

std::string HousingRoomEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingRoom";
}

std::string HousingRoomEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingRoomEntity pos: ({:.1f}, {:.1f}, {:.1f})",
        Object::GetDebugInfo(),
        GetPositionX(), GetPositionY(), GetPositionZ());
}

UF::UpdateFieldFlag HousingRoomEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

void HousingRoomEntity::ClearValuesChangesMask()
{
    m_values.ClearChangesMask(&HousingRoomEntity::m_housingRoomData);
    m_values.ClearChangesMask(&HousingRoomEntity::m_mirroredPositionData);
    Object::ClearValuesChangesMask();
}

bool HousingRoomEntity::AddToObjectUpdate()
{
    GetMap()->AddUpdateObject(this);
    return true;
}

void HousingRoomEntity::RemoveFromObjectUpdate()
{
    GetMap()->RemoveUpdateObject(this);
}

void HousingRoomEntity::SetHouseGUID(ObjectGuid houseGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::HouseGUID), houseGuid);
}

void HousingRoomEntity::SetHouseRoomID(int32 roomId)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::HouseRoomID), roomId);
}

void HousingRoomEntity::SetFlags(int32 flags)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::Flags), flags);
}

void HousingRoomEntity::SetFloorIndex(int32 floorIndex)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::FloorIndex), floorIndex);
}

void HousingRoomEntity::AddMeshObject(ObjectGuid meshObjectGuid)
{
    AddDynamicUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
        .ModifyValue(&UF::HousingRoomData::MeshObjects)) = meshObjectGuid;
}

void HousingRoomEntity::AddDoor(int32 roomComponentID, Position const& offset, uint8 connectionType, ObjectGuid attachedRoomGuid)
{
    auto doorRef = AddDynamicUpdateFieldValue(
        m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
            .ModifyValue(&UF::HousingRoomData::Doors));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentID).SetValue(roomComponentID);
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentOffset).SetValue(
        TaggedPosition<Position::XYZ>(offset.GetPositionX(), offset.GetPositionY(), offset.GetPositionZ()));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentType).SetValue(connectionType);
    doorRef.ModifyValue(&UF::HousingDoorData::AttachedRoomGUID).SetValue(attachedRoomGuid);
}

void HousingRoomEntity::SetMirroredPosition(Position const& pos, QuaternionData const& rot,
    float scale, ObjectGuid attachParent, uint8 attachFlags)
{
    auto posData = m_values.ModifyValue(&HousingRoomEntity::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rot);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachFlags);
}

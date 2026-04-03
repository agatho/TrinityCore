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
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"

HousingRoomEntity::HousingRoomEntity(Map* map, ObjectGuid guid, Position const& pos)
    : _map(map), _stationaryPosition(pos)
{
    _Create(guid);

    // Retail room entities have HasEntityPosition + Stationary in COB.
    m_updateFlag.HasEntityPosition = true;
    m_updateFlag.Stationary = true;

    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoom_C, false, WowCS::GetRawFragmentData(m_housingRoomData));
    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false, WowCS::GetRawFragmentData(m_mirroredPositionData));
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HousingRoom, false);
}

void HousingRoomEntity::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&HousingRoomEntity::m_housingRoomData);
    m_values.ClearChangesMask(&HousingRoomEntity::m_mirroredPositionData);
    BaseEntity::ClearUpdateMask(remove);
}

std::string HousingRoomEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingRoom";
}

void HousingRoomEntity::BuildUpdate(UpdateDataMapType& data_map)
{
    BuildUpdateChangesMask();

    // Send to all players on this map
    Map::PlayerList const& players = _map->GetPlayers();
    for (MapReference const& ref : players)
        if (Player* player = ref.GetSource())
            BuildFieldsUpdate(player, data_map);

    ClearUpdateMask(false);
}

void HousingRoomEntity::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
        return;

    ByteBuffer& buf = data->GetBuffer();
    buf << uint8(UPDATETYPE_CREATE_OBJECT);
    buf << GetGUID();
    buf << uint8(m_objectTypeId); // NUM_CLIENT_OBJECT_TYPES (18)

    // Custom movement block: HasEntityPosition + Stationary.
    // Can't use BaseEntity::BuildMovementUpdate because it casts to WorldObject
    // for Stationary position, but HousingRoomEntity is not a WorldObject.
    buf.WriteBit(m_updateFlag.HasEntityPosition);
    buf.WriteBit(false);  // NoBirthAnim
    buf.WriteBit(false);  // EnablePortals
    buf.WriteBit(false);  // PlayHoverAnim
    buf.WriteBit(false);  // ThisIsYou
    buf.WriteBit(false);  // HasMovementUpdate
    buf.WriteBit(false);  // HasMovementTransport
    buf.WriteBit(m_updateFlag.Stationary);
    buf.WriteBit(false);  // CombatVictim
    buf.WriteBit(false);  // ServerTime
    buf.WriteBit(false);  // Vehicle
    buf.WriteBit(false);  // AnimKit
    buf.WriteBit(false);  // Rotation
    buf.WriteBit(false);  // GameObject
    buf.WriteBit(false);  // SmoothPhasing
    buf.WriteBit(false);  // SceneObject
    buf.WriteBit(false);  // ActivePlayer
    buf.WriteBit(false);  // Conversation
    buf.WriteBit(false);  // Room
    buf.WriteBit(false);  // Decor
    buf.WriteBit(false);  // MeshObject
    buf.FlushBits();

    buf << uint32(0); // PauseTimesCount

    // Stationary position (sniff-verified: 4 floats XYZО)
    buf << _stationaryPosition.PositionXYZOStream();

    // FieldFlags + entity fragments + values data (standard BaseEntity logic)
    UF::UpdateFieldFlag fieldFlags = GetUpdateFieldFlagsFor(target);
    std::size_t sizePos = buf.wpos();
    buf << uint32(0);
    buf << uint8(fieldFlags);
    BuildEntityFragments(buf, m_entityFragments.GetIds());

    for (std::size_t i = 0; i < m_entityFragments.UpdateableCount; ++i)
    {
        WowCS::EntityFragment fragmentId = m_entityFragments.Updateable.Ids[i];
        if (WowCS::IsIndirectFragment(fragmentId))
            buf << uint8(1);

        WowCS::EntityFragmentInfo->SerializeCreate[static_cast<std::size_t>(m_entityFragments.Updateable.Ids[i])](
            m_entityFragments.Updateable.Data[i], fieldFlags, buf, target, this);
    }

    buf.put<uint32>(sizePos, buf.wpos() - sizePos - 4);
    data->AddUpdateBlock();

    // Diagnostic: dump the CREATE block for debugging
    std::size_t blockEnd = buf.wpos();
    std::size_t blockStart = sizePos - 1; // uff byte before sizePos
    // Actually dump from the GUID position backwards
    // Dump first 40 bytes of this entity's CREATE block for wire format debugging
    std::size_t entityStart = blockEnd - (blockEnd - sizePos + 4 + 1 + 9 + 1 + 3 + 4); // approximate
    std::string hexDump;
    // Dump from sizePos-30 to sizePos+10 to capture GUID+objectType+COB+PauseTime+fieldBlockSize
    std::size_t dumpStart = (sizePos > 30) ? sizePos - 30 : 0;
    std::size_t dumpEnd = std::min(sizePos + 20, blockEnd);
    for (std::size_t i = dumpStart; i < dumpEnd; ++i)
        hexDump += Trinity::StringFormat("{:02x} ", buf[i]);
    TC_LOG_ERROR("housing", "HousingRoomEntity::BuildCreate: guid={} objectType={} "
        "fieldBlockSize={} fragments=[{}] stationaryPos=({:.1f},{:.1f},{:.1f}) "
        "rawBytes(around fieldBlockSize): {}",
        GetGUID().ToString(), uint32(m_objectTypeId),
        buf.wpos() - sizePos - 4,
        m_entityFragments.Count,
        _stationaryPosition.GetPositionX(), _stationaryPosition.GetPositionY(), _stationaryPosition.GetPositionZ(),
        hexDump);
}

std::string HousingRoomEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingRoomEntity pos: ({:.1f}, {:.1f}, {:.1f})",
        BaseEntity::GetDebugInfo(),
        _stationaryPosition.GetPositionX(), _stationaryPosition.GetPositionY(), _stationaryPosition.GetPositionZ());
}

UF::UpdateFieldFlag HousingRoomEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

bool HousingRoomEntity::AddToObjectUpdate()
{
    _map->AddUpdateObject(this);
    return true;
}

void HousingRoomEntity::RemoveFromObjectUpdate()
{
    _map->RemoveUpdateObject(this);
}

void HousingRoomEntity::SetHouseGUID(ObjectGuid houseGuid)
{
    _houseGuid = houseGuid;
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
    // HousingDoorData has HasChangesMask<5> so AddDynamicUpdateFieldValue returns
    // MutableFieldReferenceWithChangesMask. Use ModifyValue() to set sub-fields.
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

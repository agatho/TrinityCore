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

#include "HousingMirrorEntity.h"
#include "Map.h"
#include "StringFormat.h"
#include "UpdateData.h"

HousingMirrorEntity::HousingMirrorEntity(Map* map, ObjectGuid guid) : _map(map)
{
    _Create(guid);

    // Retail emits these with objectType = 18 (TYPEID_HOUSING_ENTITY).
    m_objectTypeId = TYPEID_HOUSING_ENTITY;

    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false,
        WowCS::GetRawFragmentData(m_mirroredPositionData));
}

HousingMirrorEntity::~HousingMirrorEntity() = default;

void HousingMirrorEntity::InitPositionData(ObjectGuid attachParent,
    Position const& position, QuaternionData const& rotation,
    float scale, uint8 attachmentFlags, bool isExteriorRoot)
{
    auto posData = m_values.ModifyValue(&HousingMirrorEntity::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rotation);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachmentFlags);

    // Retail house-exterior root mirrors carry BOTH tags (Group A at idx 9984
    // in dump_12.0.1.66838_2026-04-15_09-35-59). Mesh-level mirrors (Group B)
    // carry neither. We expose isExteriorRoot for the house-exterior case;
    // callers default it false for mesh-level mirrors.
    if (isExteriorRoot)
    {
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, false);
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorRoot, false);
    }
}

void HousingMirrorEntity::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&HousingMirrorEntity::m_mirroredPositionData);
    BaseEntity::ClearUpdateMask(remove);
}

std::string HousingMirrorEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingMirror";
}

void HousingMirrorEntity::BuildUpdate(UpdateDataMapType& /*data_map*/)
{
    // Mirror position is set once at spawn and does not change. No-op update.
    ClearUpdateMask(false);
}

std::string HousingMirrorEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingMirrorEntity", BaseEntity::GetDebugInfo());
}

UF::UpdateFieldFlag HousingMirrorEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

bool HousingMirrorEntity::AddToObjectUpdate()
{
    // Mirror fields are static once set; it never asks the map to queue it.
    return false;
}

void HousingMirrorEntity::RemoveFromObjectUpdate()
{
}

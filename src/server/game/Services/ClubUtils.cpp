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

#include "ClubUtils.h"
#include "RealmList.h"

uint64 Battlenet::Services::Clubs::CreateClubMemberId(ObjectGuid guid)
{
    return guid.GetCounter() | (uint64(sRealmList->GetCurrentRealmId().Realm & 0xFFF) << 48);
}

ObjectGuid Battlenet::Services::Clubs::GetGuidFromClubMemberId(uint64 memberId)
{
    // The layout CreateClubMemberId above actually produces, bit for bit:
    //   bits  0..39  the character counter - ObjectGuid::GetCounter masks HighGuid::Player with 0xFFFFFFFFFF
    //                (ObjectGuid.h, GetCounter), so the counter is FORTY bits wide, not forty-eight
    //   bits 40..47  ALWAYS ZERO. Nothing writes them: the counter cannot reach them and the realm id starts at
    //                bit 48. A member id with any of them set was therefore not minted by CreateClubMemberId.
    //   bits 48..59  the realm id, masked to 12 bits
    //   bits 60..63  always zero
    // Both of the following rejections matter, and for different reasons. The realm test says "this id belongs to
    // some other realm"; the reserved-bits test says "no realm minted this id at all". Folding the reserved bits
    // into the counter instead - which the previous 0x0000FFFFFFFFFFFF mask did - handed them straight to
    // ObjectGuid::Create<HighGuid::Player>, which stores its argument unmasked (ObjectGuid.cpp, CreatePlayer), so
    // a crafted member id produced a guid that CreateClubMemberId can never have produced. Harmless in the end
    // (the character cache misses and the caller answers PermanentFailure) but harmless for the wrong reason, and
    // this function is documented as the inverse - so it has to actually be one.
    if (memberId & UI64LIT(0xF000000000000000))
        return ObjectGuid::Empty;

    if (memberId & UI64LIT(0x0000FF0000000000))
        return ObjectGuid::Empty;

    if (uint32((memberId >> 48) & 0xFFF) != (sRealmList->GetCurrentRealmId().Realm & 0xFFF))
        return ObjectGuid::Empty;

    return ObjectGuid::Create<HighGuid::Player>(memberId & UI64LIT(0x000000FFFFFFFFFF));
}

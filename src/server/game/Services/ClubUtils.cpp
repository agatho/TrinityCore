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
    // The realm id occupies bits 48-59; anything from another realm cannot be resolved from our character cache.
    if (uint32((memberId >> 48) & 0xFFF) != (sRealmList->GetCurrentRealmId().Realm & 0xFFF))
        return ObjectGuid::Empty;

    return ObjectGuid::Create<HighGuid::Player>(memberId & UI64LIT(0x0000FFFFFFFFFFFF));
}

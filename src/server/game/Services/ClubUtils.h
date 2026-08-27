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

#ifndef TRINITYCORE_CLUB_UTILS_H
#define TRINITYCORE_CLUB_UTILS_H

#include "ObjectGuid.h"

namespace Battlenet::Services::Clubs
{
uint64 CreateClubMemberId(ObjectGuid guid);

// Inverse of CreateClubMemberId. Returns an empty guid for every member id this realm cannot have minted, which
// is two distinct cases: an id carrying another realm's id in bits 48..59, and an id with bits 40..47 or 60..63
// set - bits CreateClubMemberId leaves at zero, so no realm produced it. Either way nothing here can resolve it.
ObjectGuid GetGuidFromClubMemberId(uint64 memberId);
}

#endif // TRINITYCORE_CLUB_UTILS_H

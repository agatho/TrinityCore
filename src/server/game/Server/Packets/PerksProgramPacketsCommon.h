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

#ifndef TRINITYCORE_PERKS_PROGRAM_PACKETS_COMMON_H
#define TRINITYCORE_PERKS_PROGRAM_PACKETS_COMMON_H

#include "PacketUtilities.h"

namespace WorldPackets::PerksProgram
{
struct PerksVendorItem
{
    int32 VendorItemID = 0;
    int32 MountID = 0;
    int32 BattlePetSpeciesID = 0;
    int32 TransmogSetID = 0;
    int32 ItemModifiedAppearanceID = 0;
    int32 TransmogIllusionID = 0;
    int32 ToyID = 0;
    int32 WarbandSceneID = 0;
    int32 Price = 0;
    int32 OriginalPrice = 0;
    Timestamp<> AvailableUntil;
    bool Disabled = false;
    bool DoesNotExpire = false;

    friend bool operator==(PerksVendorItem const& left, PerksVendorItem const& right) = default;
};

ByteBuffer& operator<<(ByteBuffer& data, PerksVendorItem const& perksVendorItem);

// The client's PerksRecentPurchasesData. 13 bytes on the wire (the in-memory struct is 24 -- the allocator
// stride is not the wire width). Read in two places by the 12.1.0.69382 client, byte for byte the same both
// times: family dispatcher RVA 0x67A010 case 6488068 (SMSG_RESPONSE_PERK_RECENT_PURCHASES) and the type 2/3
// branch of the SMSG_PERKS_PROGRAM_RESULT reader RVA 0x74CE10.
struct PerksRecentPurchase
{
    int32 PerksVendorItemID = 0;   // key of the client's purchase hash map
    uint64 PurchaseTime = 0;       // unix seconds of the purchase
    bool Refundable = false;       // whether this purchase can still be refunded (cleanly-revocable reward)

    friend bool operator==(PerksRecentPurchase const& left, PerksRecentPurchase const& right) = default;
};

ByteBuffer& operator<<(ByteBuffer& data, PerksRecentPurchase const& purchase);
}

#endif // TRINITYCORE_PERKS_PROGRAM_PACKETS_COMMON_H

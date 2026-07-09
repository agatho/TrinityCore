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

#ifndef TRINITYCORE_PERKS_PROGRAM_PACKETS_H
#define TRINITYCORE_PERKS_PROGRAM_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PerksProgramPacketsCommon.h"
#include <vector>

namespace WorldPackets::PerksProgram
{
class PerksProgramStatusRequest final : public ClientPacket
{
public:
    explicit PerksProgramStatusRequest(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_STATUS_REQUEST, std::move(packet)) { }

    void Read() override { }
};

// CMSG_PERKS_PROGRAM_REQUEST_PURCHASE wire (12.0.7.68275, from the client serializer sub_7FF72914B790):
//   uint32 PerksVendorItemID, PackedGUID VendorGUID (the interacted Trading Post vendor).
class PerksProgramRequestPurchase final : public ClientPacket
{
public:
    explicit PerksProgramRequestPurchase(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_PURCHASE, std::move(packet)) { }

    void Read() override;

    int32 PerksVendorItemID = 0;
    ObjectGuid VendorGUID;
};

// CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES wire (12.0.7.68275, client serializer sub_7FF7291E0B20): no payload.
class PerksProgramGetRecentPurchases final : public ClientPacket
{
public:
    explicit PerksProgramGetRecentPurchases(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES, std::move(packet)) { }

    void Read() override { }
};

// SMSG_RESPONSE_PERK_RECENT_PURCHASES wire (12.0.7.68275, client deserializer sub_7FF72911D110 case 0x5E0004):
//   uint32 Count, then Count x { uint32 PerksVendorItemID, uint64 PurchaseTime, uint8 (bit7 = Refundable) }.
class ResponsePerkRecentPurchases final : public ServerPacket
{
public:
    ResponsePerkRecentPurchases() : ServerPacket(SMSG_RESPONSE_PERK_RECENT_PURCHASES) { }

    WorldPacket const* Write() override;

    struct RecentPurchase
    {
        int32 PerksVendorItemID = 0;
        uint64 PurchaseTime = 0;   // unix seconds of the purchase
        bool Refundable = false;   // whether this purchase can still be refunded (cleanly-revocable reward)
    };

    std::vector<RecentPurchase> Purchases;
};

// CMSG_PERKS_PROGRAM_REQUEST_REFUND wire (12.0.7.68275, from the client serializer sub_7FF72914B8F0):
//   uint32 PerksVendorItemID, PackedGUID VendorGUID. Byte-identical to the purchase request.
class PerksProgramRequestRefund final : public ClientPacket
{
public:
    explicit PerksProgramRequestRefund(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_REFUND, std::move(packet)) { }

    void Read() override;

    int32 PerksVendorItemID = 0;
    ObjectGuid VendorGUID;
};

// CMSG_PERKS_PROGRAM_REQUEST_CART_CHECKOUT wire (12.0.7.68275, from the client serializer sub_7FF72914B860):
//   uint32 ItemCount, PackedGUID VendorGUID, uint32 PerksVendorItemIDs[ItemCount].
class PerksProgramRequestCartCheckout final : public ClientPacket
{
public:
    explicit PerksProgramRequestCartCheckout(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_CART_CHECKOUT, std::move(packet)) { }

    void Read() override;

    ObjectGuid VendorGUID;
    std::vector<int32> PerksVendorItemIDs;
};

// SMSG_PERKS_PROGRAM_VENDOR_UPDATE wire (12.0.7.68275, from the client deserializer sub_7FF72911D110 case 6160384):
//   uint32 VendorItemCount, then VendorItemCount x PerksVendorItem. No header precedes the count.
class PerksProgramVendorUpdate final : public ServerPacket
{
public:
    explicit PerksProgramVendorUpdate() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE) { }

    WorldPacket const* Write() override;

    std::vector<PerksVendorItem> VendorItems;
};
}

#endif // TRINITYCORE_PERKS_PROGRAM_PACKETS_H

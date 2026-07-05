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

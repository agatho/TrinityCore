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

#include "BattlePayPackets.h"

namespace WorldPackets::BattlePay
{
WorldPacket const* ProductListResponse::Write()
{
    if (RawData && !RawData->empty())
        _worldPacket.append(RawData->data(), RawData->size());

    return &_worldPacket;
}

void StartPurchase::Read()
{
    _worldPacket >> ProductID;
    _worldPacket >> ScalarU64;
    Flag = _worldPacket.ReadBit();
}

void OpenCheckout::Read()
{
    _worldPacket >> DistributionID;
}

WorldPacket const* StartPurchaseResponse::Write()
{
    _worldPacket << ResultA;
    _worldPacket << ResultB;
    _worldPacket << PurchaseID;

    return &_worldPacket;
}

WorldPacket const* GetPurchaseListResponse::Write()
{
    _worldPacket << Result;
    _worldPacket << uint32(Purchases.size());
    for (PurchaseRecord const& p : Purchases)
    {
        _worldPacket << p.PurchaseID;
        _worldPacket << p.Status;
        _worldPacket << p.ResultCode;
        _worldPacket << p.ProductID;
        _worldPacket << uint8(0);       // walletName: empty (8-bit length primitive, value 0)
        _worldPacket << p.BasePrice;
        _worldPacket << p.UserPrice;
        _worldPacket << p.TimeCreated;
    }

    return &_worldPacket;
}

WorldPacket const* PurchaseUpdate::Write()
{
    _worldPacket << Result;
    _worldPacket << uint32(Purchases.size());
    for (PurchaseRecord const& p : Purchases)
    {
        _worldPacket << p.PurchaseID;
        _worldPacket << p.Status;
        _worldPacket << p.ResultCode;
        _worldPacket << p.ProductID;
        _worldPacket << uint8(0);       // walletName: empty (8-bit length primitive, value 0)
        _worldPacket << p.BasePrice;
        _worldPacket << p.UserPrice;
        _worldPacket << p.TimeCreated;
    }

    return &_worldPacket;
}

WorldPacket const* EnumVasPurchaseStatesResponse::Write()
{
    // Six-bit count, then flush. With no purchases this is the single 0x00 byte retail sends.
    _worldPacket << Bits<6>(0);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* VasGetServiceStatusResponse::Write()
{
    _worldPacket << Bits<4>(ServiceStatus);
    _worldPacket << Bits<4>(Unknown);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
}

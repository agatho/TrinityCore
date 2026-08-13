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

// CMSG_PERKS_PROGRAM_ITEMS_REFRESHED (0x3A02B5): no payload. The client sends it to ask the server to resend the
// current Trading Post listing (retail answers each one with a VENDOR_UPDATE resend).
class PerksProgramItemsRefreshed final : public ClientPacket
{
public:
    explicit PerksProgramItemsRefreshed(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_ITEMS_REFRESHED, std::move(packet)) { }

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

// CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS (0x290017): no payload. Sent on the realm connection right before
// CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES whenever the Trading Post / Traveler's Log UI opens, and again from
// MonthlyActivitiesFrameMixin:UpdateActivities when a new threshold has just been earned. It is
// C_PerksProgram.RequestPendingChestRewards(); the answer is SMSG_RESPONSE_PERK_PENDING_REWARDS.
class PerksProgramRequestPendingRewards final : public ClientPacket
{
public:
    explicit PerksProgramRequestPendingRewards(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS, std::move(packet)) { }

    void Read() override { }
};

// SMSG_RESPONSE_PERK_PENDING_REWARDS wire (12.0.7.68275, client deserializer sub_7FF72911D110 case 0x5E0003 ->
// element reader sub_7FF7291ECA20):
//   uint32 Count, then Count x VirtualCurrencyTransaction (the client's own name for the element -- the array
//   destructor sub_7FF729135FA0 frees it as WowGetRawTypeName<struct VirtualCurrencyTransaction>):
//     uint8  bits: TransactionType in the top 3 bits (WriteBits(type, 3) + FlushBits)
//     if TransactionType == 4: bit-packed string length, int32, then the string
//     PackedGuid Owner
//     int32 Amount
//     if TransactionType == 1: int32, int32
//     if TransactionType == 2: int32 ActivityMonthID, int32 ThresholdOrderIndex
//     if TransactionType == 3 or 7: int32 PerksVendorItemID, int32, int32
//     if TransactionType == 5: uint64
//     if TransactionType == 6: int32
//
// The Trading Post message handler (client sub_7FF72AE18390) turns each transaction into a
// PerksProgramPendingChestRewards Lua record, and that copy is what names the fields above: TransactionType ->
// rewardTypeID, Amount -> rewardAmount, the type-2 pair -> activityMonthID and thresholdOrderIndex, the first
// type-3/7 int32 -> perksVendorItemID. Owner is not read by that handler at all.
// Verified against both captured forms: the 4-byte Count = 0 body (68453, 68974) and a 3-entry 64-byte body
// (68275 b_pets), which decodes as three TransactionType 2 records sharing one BNetAccount guid (HighGuid 30,
// low 0x12564980), Amount 100, ActivityMonthID 43, ThresholdOrderIndex 1/2/3 -- zero bytes left over.
//
// Only the TransactionType 2 shape is modelled here: it is the only one observed on the wire and the only one
// the Traveler's Log consumes, and guessing the payload of the other seven would be inventing wire format.
class ResponsePerkPendingRewards final : public ServerPacket
{
public:
    // TransactionType 2 = an earned-but-not-yet-handed-out Trading Post activity threshold reward. The client
    // draws these as the glowing, uncollected Traveler's Log chest and as the "uncollected Tender" currency
    // tooltip line (Blizzard_MonthlyActivities.lua HasPendingReward, Blizzard_PerksProgramProducts.lua
    // HasTenderToRetrieve).
    static constexpr uint8 TransactionTypeActivityThreshold = 2;

    ResponsePerkPendingRewards() : ServerPacket(SMSG_RESPONSE_PERK_PENDING_REWARDS) { }

    WorldPacket const* Write() override;

    struct PendingReward
    {
        ObjectGuid Owner;               // account the reward belongs to; retail sends the BNetAccount guid
        int32 Amount = 0;               // Trader's Tender still owed for the threshold
        int32 ActivityMonthID = 0;      // must equal PerksActivitiesInfo.activePerksMonth or the client ignores it
        int32 ThresholdOrderIndex = 0;  // PerksActivityThreshold order index the reward belongs to
    };

    std::vector<PendingReward> Rewards;
};

// SMSG_PERKS_ANIM_TOGGLE_KILL_SWITCH wire (12.0.7.68275, client deserializer sub_7FF72911D110 case 0x5E0007):
// a single byte holding two bits. The message handler (client sub_7FF72A720610) stores them in the two globals
// behind C_PerksProgram.IsAttackAnimToggleEnabled() (byte_7FF72D5110DF, bit 7) and
// C_PerksProgram.IsMountSpecialAnimToggleEnabled() (byte_7FF72D5113E8, bit 6). Retail sends it on the realm
// connection during the login burst, between SMSG_FEATURE_SYSTEM_STATUS and SMSG_MOTD, and every capture across
// 68275 / 68453 / 68974 carries the same single byte 0xC0 -- both toggles enabled.
class PerksAnimToggleKillSwitch final : public ServerPacket
{
public:
    PerksAnimToggleKillSwitch() : ServerPacket(SMSG_PERKS_ANIM_TOGGLE_KILL_SWITCH, 1) { }

    WorldPacket const* Write() override;

    bool AttackAnimToggleEnabled = false;
    bool MountSpecialAnimToggleEnabled = false;
};

// SMSG_PERKS_PROGRAM_ACTIVITY_COMPLETE wire (12.0.7.68275, client deserializer sub_7FF72911D110 case 0x5E0005):
// the body is read as one trailing blob and the handler (client sub_7FF72AE14F00) takes its first uint32 as a
// PerksActivity id, looks the row up and pushes it into the pending-completion list that feeds
// C_PerksActivities.GetPerksActivitiesPendingCompletion() and the PERKS_ACTIVITY_COMPLETED event. Retail sends
// exactly four bytes (68974 worldquest-shop capture: A1 01 00 00 = activity 417) on the realm connection.
class PerksProgramActivityComplete final : public ServerPacket
{
public:
    PerksProgramActivityComplete() : ServerPacket(SMSG_PERKS_PROGRAM_ACTIVITY_COMPLETE, 4) { }

    WorldPacket const* Write() override;

    uint32 PerksActivityID = 0;
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

// CMSG_PERKS_PROGRAM_SET_FROZEN_VENDOR_ITEM (0x3A02BA). Serializer sub_7FF72914B9C0 writes { bit Frozen; uint32
// PerksVendorItemID; PackedGuid NpcGUID }. Frozen=true pins the item so it carries to the next Trading Post
// rotation (shown with the "frozen" indicator); Frozen=false clears the pin.
class PerksProgramSetFrozenVendorItem final : public ClientPacket
{
public:
    explicit PerksProgramSetFrozenVendorItem(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_SET_FROZEN_VENDOR_ITEM, std::move(packet)) { }

    void Read() override;

    bool Frozen = false;
    int32 PerksVendorItemID = 0;
    ObjectGuid NpcGUID;
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

// SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE wire (12.0.7.68275, from the client deserializer sub_7FF72911D110 case 6160385):
//   uint32 CompletedActivityCount; uint64 PeriodEnd; uint64 PeriodStart; uint32 Unused;
//   uint32 CompletedActivityID[CompletedActivityCount].
// The id list is the player's COMPLETED Trading Post activities for the period (the client already
// has the activity catalogue from PerksActivity.db2 and marks each id it receives as completed).
class PerksProgramActivityUpdate final : public ServerPacket
{
public:
    explicit PerksProgramActivityUpdate() : ServerPacket(SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE) { }

    WorldPacket const* Write() override;

    uint64 PeriodStart = 0;
    uint64 PeriodEnd = 0;
    uint32 Unused = 0;
    std::vector<uint32> CompletedActivityIDs;
};

// SMSG_PERKS_PROGRAM_RESULT (0x5E0002) is deliberately NOT implemented. Its layout is decoded (client
// deserializer sub_7FF72911D110 case 0x5E0002 -> sub_7FF7291EC6C0) and was replayed byte-for-byte against the
// 2215-byte 68974 capture, which closes with zero leftover bytes:
//   uint8  bits: ResultType (4 bits), a 2-bit field, one bit "has trailing uint64", one spare bit
//   if ResultType == 4: int32, int32, int32, uint32 Count, Count x int32
//   if ResultType == 2 or 3: int32, uint32 PurchaseCount
//   if ResultType == 5: PackedGuid, PackedGuid, uint32 VendorItemCount, then seven int32
//   if ResultType == 9: uint32 VendorItemCount
//   if ResultType == 8: int32
//   if "has trailing uint64": uint64
//   if ResultType == 2 or 3: PurchaseCount x { int32 PerksVendorItemID, uint64 PurchaseTime, uint8 bit7 }
//   if ResultType == 5 or 9: VendorItemCount x PerksVendorItem (49 bytes, the operator<< in
//                            PerksProgramPacketsCommon.cpp -- the capture confirms that writer byte for byte)
// The capture is ResultType 5: two Creature guids, 44 vendor items and seven zero int32s. We cannot populate it
// honestly: only ResultType 5 has ever been observed, the meaning of the 4-bit ResultType, the 2-bit field, the
// second Creature guid (it differs between the two captures while the first stays put) and all seven trailing
// int32s are unknown, and the vendor listing it carries is already delivered by SMSG_PERKS_PROGRAM_VENDOR_UPDATE
// (0x5E0000), which the client parses with the very same JamPerksVendorItem reader. Implementing it would mean
// guessing eight fields to duplicate a message that already works. Note for whoever picks it up: retail sends
// this one on the INSTANCE connection, not the realm connection.
}

#endif // TRINITYCORE_PERKS_PROGRAM_PACKETS_H

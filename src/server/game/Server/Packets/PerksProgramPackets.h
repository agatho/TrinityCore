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
#include "Optional.h"
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

// CMSG_PERKS_PROGRAM_ITEMS_REFRESHED (12.1 value 0x3D02B1). This message is NOT empty and it does NOT mean
// "resend the current listing" -- both of which this class used to claim. The 12.1.0.69382 serializer RVA
// 0x6D1E90 writes
//   Write<uint32>(3998385)                      // = 0x3D02B1, the opcode
//   Write<uint32>(*(this+40))                   // Count
//   for i in 0..Count-1: Write<uint32>(...)     // Count x uint32
// so the body is uint32 Count followed by Count uint32s -- 4 + 4*Count bytes. Reading nothing left those bytes
// unconsumed on every one of these packets, which CallHandlerWrapper's LogUnprocessedTail reports as a tail.
//
// WHAT THE CLIENT IS ASKING. The message is built in PerksProgramMgr::SetRecentPurchases (client RVA 0x253E020,
// reached from SMSG_RESPONSE_PERK_RECENT_PURCHASES and from SMSG_PERKS_PROGRAM_RESULT type 2). That function
// walks the account's purchase list and looks every PerksVendorItemID up in the client's vendor-item map; the
// ids it does NOT find are collected into this array, and the message is sent only if the array is non-empty.
//
// So the ids are PerksVendorItem.ID values the client owns a purchase record for but has no vendor-item data
// for -- typically items bought in an earlier Trading Post rotation. The id space is proven: the same key is
// looked up in the DB2 store whose meta names it "PerksVendorItem" (LayoutHash 0x96A2B1EB), which is also what
// backs C_PerksProgram.GetVendorItemInfo(vendorItemID).
//
// Answering with SMSG_PERKS_PROGRAM_VENDOR_UPDATE is actively wrong: that message enters the client's merge
// routine with flag 0, which CLEARS the vendor map before refilling it, so it drops data instead of adding the
// missing rows -- and it can never contain the requested ids anyway, since the client only asks about items
// that are not in the current listing. The additive path is SMSG_PERKS_PROGRAM_RESULT type 9 (flag 1).
//
// UNVERIFIED: Blizzard's own name for the field. The client has no name string for it and the message has no
// Lua binding, so "RequestedVendorItemIDs" is our name; the CONTENT is not a guess.
class PerksProgramItemsRefreshed final : public ClientPacket
{
public:
    explicit PerksProgramItemsRefreshed(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_ITEMS_REFRESHED, std::move(packet)) { }

    void Read() override;

    std::vector<int32> RequestedVendorItemIDs;
};

// CMSG_PERKS_PROGRAM_REQUEST_PURCHASE wire, re-checked against 12.1.0.69382 client serializer RVA 0x6D1F40
// (it writes opcode 3998387 = 0x3D02B3, then Write<uint32>, then WritePackedGuid):
//   uint32 PerksVendorItemID, PackedGUID VendorGUID (the interacted Trading Post vendor).
// Unchanged from 12.0.7.68275 (serializer sub_7FF72914B790 there).
class PerksProgramRequestPurchase final : public ClientPacket
{
public:
    explicit PerksProgramRequestPurchase(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_PURCHASE, std::move(packet)) { }

    void Read() override;

    int32 PerksVendorItemID = 0;
    ObjectGuid VendorGUID;
};

// CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS (12.1 value 0x2A0017): no payload -- 12.1.0.69382 serializer RVA
// 0x746D80 writes the opcode (2752535 = 0x2A0017) and nothing else. Sent on the realm connection right before
// CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES whenever the Trading Post / Traveler's Log UI opens, and again from
// MonthlyActivitiesFrameMixin:UpdateActivities when a new threshold has just been earned. It is
// C_PerksProgram.RequestPendingChestRewards(); the answer is SMSG_RESPONSE_PERK_PENDING_REWARDS.
class PerksProgramRequestPendingRewards final : public ClientPacket
{
public:
    explicit PerksProgramRequestPendingRewards(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS, std::move(packet)) { }

    void Read() override { }
};

// SMSG_RESPONSE_PERK_PENDING_REWARDS wire, re-checked against 12.1.0.69382: family dispatcher RVA 0x67A010
// case 6488067 (= 0x630003) -> element reader RVA 0x74D190, which is field-for-field what the 12.0.7.68275
// reader sub_7FF7291ECA20 did (the comment below was written from that build):
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

// SMSG_PERKS_ANIM_TOGGLE_KILL_SWITCH wire, re-checked against 12.1.0.69382: family dispatcher RVA 0x67A010
// case 6488071 (= 0x630007) reads one byte and takes (byte >= 0x80) and (byte & 0x40) -- two MSB-first bits
// plus six padding bits, exactly one byte. Both Lua getters return the stored byte unchanged (no negation),
// so the polarity is POSITIVE despite the "kill switch" name: 1 = allowed.
// A single byte holding two bits. The message handler (client sub_7FF72A720610) stores them in the two globals
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

// SMSG_PERKS_PROGRAM_ACTIVITY_COMPLETE wire, re-checked against 12.1.0.69382: family dispatcher RVA 0x67A010
// case 6488069 (= 0x630005) parses nothing -- it takes a raw pointer to the remaining bytes (RVA 0x35AF730).
// WARNING: an empty body therefore hands the consumer a zero-length buffer which it dereferences; never send
// this message without its payload.
// The body is read as one trailing blob and the handler (client sub_7FF72AE14F00) takes its first uint32 as a
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

// CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES wire: no payload -- 12.1.0.69382 serializer RVA 0x746DB0 writes the
// opcode (2752536 = 0x2A0018) and nothing else. Same in 12.0.7.68275 (serializer sub_7FF7291E0B20).
class PerksProgramGetRecentPurchases final : public ClientPacket
{
public:
    explicit PerksProgramGetRecentPurchases(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES, std::move(packet)) { }

    void Read() override { }
};

// SMSG_RESPONSE_PERK_RECENT_PURCHASES wire, re-checked against 12.1.0.69382: family dispatcher RVA 0x67A010
// case 6488068 (= 0x630004) reads
//   uint32 Count, then Count x { uint32 PerksVendorItemID, uint64 PurchaseTime, uint8 (bit7 = Refundable) }.
// 13 bytes per element on the wire, so 4 + 13*Count. The client's PerksRecentPurchasesData struct is 24 bytes
// in memory -- the allocator stride is NOT the wire width.
class ResponsePerkRecentPurchases final : public ServerPacket
{
public:
    ResponsePerkRecentPurchases() : ServerPacket(SMSG_RESPONSE_PERK_RECENT_PURCHASES) { }

    WorldPacket const* Write() override;

    // The element is shared with the type 2/3 branch of SMSG_PERKS_PROGRAM_RESULT -- same 13 bytes, same reader.
    using RecentPurchase = PerksRecentPurchase;

    std::vector<PerksRecentPurchase> Purchases;
};

// CMSG_PERKS_PROGRAM_REQUEST_REFUND wire, re-checked against 12.1.0.69382 client serializer RVA 0x6D20A0
// (opcode 3998389 = 0x3D02B5):
//   uint32 PerksVendorItemID, PackedGUID VendorGUID. Byte-identical to the purchase request.
class PerksProgramRequestRefund final : public ClientPacket
{
public:
    explicit PerksProgramRequestRefund(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_REFUND, std::move(packet)) { }

    void Read() override;

    int32 PerksVendorItemID = 0;
    ObjectGuid VendorGUID;
};

// CMSG_PERKS_PROGRAM_SET_FROZEN_VENDOR_ITEM (12.1 value 0x3D02B6). 12.1.0.69382 serializer RVA 0x6D2170 writes
// opcode 3998390, then one bit, then FlushBits (RVA 0x5D4EA0), then Write<uint32>, then WritePackedGuid:
// { bit Frozen; FLUSH; uint32 PerksVendorItemID; PackedGuid NpcGUID }. Frozen=true pins the item so it carries to the next Trading Post
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

// CMSG_PERKS_PROGRAM_REQUEST_CART_CHECKOUT wire, re-checked against 12.1.0.69382 client serializer RVA 0x6D2010
// (opcode 3998388 = 0x3D02B4):
//   uint32 ItemCount, PackedGUID VendorGUID, uint32 PerksVendorItemIDs[ItemCount].
// Mind the order: the count is written BEFORE the guid, the array elements only after it.
class PerksProgramRequestCartCheckout final : public ClientPacket
{
public:
    explicit PerksProgramRequestCartCheckout(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_CART_CHECKOUT, std::move(packet)) { }

    void Read() override;

    ObjectGuid VendorGUID;
    std::vector<int32> PerksVendorItemIDs;
};

// SMSG_PERKS_PROGRAM_VENDOR_UPDATE wire, re-checked against 12.1.0.69382: family dispatcher RVA 0x67A010 case
// 6488064 (= 0x630000) reads
//   uint32 VendorItemCount, then VendorItemCount x JamPerksVendorItem (element reader RVA 0x73EAE0).
// No header precedes the count. 49 bytes per element (u64 + 10 x u32 + two bits + flush), so 4 + 49*Count;
// the in-memory struct is 56 bytes, which is NOT the wire width. Fires PERKS_PROGRAM_DATA_REFRESH.
class PerksProgramVendorUpdate final : public ServerPacket
{
public:
    explicit PerksProgramVendorUpdate() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE) { }

    WorldPacket const* Write() override;

    std::vector<PerksVendorItem> VendorItems;
};

// SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE wire, re-checked against 12.1.0.69382: family dispatcher RVA 0x67A010
// case 6488065 (= 0x630001) reads
//   uint32 CompletedActivityCount; uint64 PeriodEnd; uint64 PeriodStart; uint32 PerksUIThemeID;
//   uint32 CompletedActivityID[CompletedActivityCount].
// Length 24 + 4*Count, which all 154 captured instances satisfy without remainder.
// The id list is the player's COMPLETED Trading Post activities for the period (the client already
// has the activity catalogue from PerksActivity.db2 and marks each id it receives as completed).
//
// PeriodEnd comes FIRST: the consumer (client RVA 0x253A080) hands the first uint64 on to RVA 0x253C3D0,
// which stores it in the global that C_PerksActivities.GetPerksActivitiesInfo turns into a countdown
// (remaining = value - now, clamped at 0) -- only an END can produce a remaining time.
class PerksProgramActivityUpdate final : public ServerPacket
{
public:
    explicit PerksProgramActivityUpdate() : ServerPacket(SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE) { }

    WorldPacket const* Write() override;

    uint64 PeriodStart = 0;
    uint64 PeriodEnd = 0;

    // PerksUITheme.ID -- the seasonal skin the Trading Post and the Traveler's Log draw around themselves.
    // The client stores the value in the global behind C_PerksActivities.GetPerksUIThemePrefix(), looks the
    // row up in the PerksUITheme store and concatenates its UiTextureKit prefix into atlas names
    // ("perks-theme-<prefix>-tp-topbig" and friends). This field was called "Unused" here, which was wrong.
    //
    // UNVERIFIED: which theme belongs to a given period. There is NO derivation in the game data: the 11
    // PerksUITheme rows are holiday skins (winterveil, hallowsend, LoveIsInTheAir, hordevsalliance, ...), no
    // .dbd in the whole set references PerksUITheme, the table has no month/holiday/period column, and
    // PerksActivityThresholdGroup has no theme column. The mapping lives on Blizzard's server, not in the DB2s.
    //
    // 0 is therefore the correct default rather than a placeholder: it is also what retail sends in 118 of the
    // 154 captured instances (the other values seen are 6 and 13), and the client handles it cleanly -- the
    // prefix comes back empty, C_Texture.GetAtlasInfo fails and PerksProgramThemeContainerMixin:OnShow blanks
    // its textures. The frame is a decorative overlay, so the only consequence is no seasonal border. Sending a
    // guessed id would instead show the wrong holiday frame all month.
    uint32 PerksUIThemeID = 0;

    std::vector<uint32> CompletedActivityIDs;
};

// SMSG_PERKS_PROGRAM_DISABLED (12.1 value 0x630006) wire, from the 12.1.0.69382 family dispatcher RVA
// 0x67A010 case 6488070 (= 0x630006): the case reads ONE byte and takes bit 7 of it (`byte >> 7`), so the body is a
// single MSB-first bit followed by seven padding bits -- exactly one byte, no other field.
//
// Effect, from the message handler (client RVA 0x253D660), which is the only source for this -- no Lua file
// describes it:
//     if (Disabled) FireEvent(0x68FB1588FB765D68);   // PERKS_PROGRAM_DISABLED
//     FireEvent(0xAD667B77A37D3DC8);                 // PERKS_PROGRAM_CLOSE  -- UNCONDITIONAL
// (both hashes resolved against the build's murmur3-x64-128-low64 event table). The close event does not hang
// off the bit: the message always shuts the Trading Post window, and additionally reports "disabled" when the
// bit is set. Sending it with Disabled = false is therefore a plain "close the window", not a no-op.
//
// What it does NOT do: the client only raises an OK popup (ERR_PERKS_PROGRAM_DISABLED) and closes the frame.
// It hides no frame and disables no button, so the server has to shut the system down itself -- see the
// CONFIG_PERKS_PROGRAM_ENABLED gates in PerksProgramHandler.cpp.
class PerksProgramDisabled final : public ServerPacket
{
public:
    PerksProgramDisabled() : ServerPacket(SMSG_PERKS_PROGRAM_DISABLED, 1) { }

    WorldPacket const* Write() override;

    bool Disabled = true;
};

// SMSG_PERKS_PROGRAM_RESULT (12.1 value 0x630002) is a TAGGED UNION, not a flat catch-all: one header byte
// selects exactly ONE payload branch. Decoded from the 12.1.0.69382 reader RVA 0x74CE10 (reached from family
// dispatcher RVA 0x67A010 case 6488066), instruction by instruction:
//
//     bits4 Type          // header byte >> 4
//     bits2 Err           // next two bits, MSB-first
//     bits1 HasStamp
//     bits1 (spare)
//     FLUSH               // exactly this one byte
//        ... exactly ONE branch, chosen by Type ...
//     if (HasStamp) uint64 Stamp        // LAST field of the message, after the branch arrays
//
// The position of Stamp was the one open question about this message. It is settled: the reader's final
// statement is `if (*(a1+48)) { Read<uint64>(); }`, after every branch including its element loops. An earlier
// note in this file put it before the trailing arrays; that reading also satisfied the length arithmetic
// because HasStamp is 0 in both available captures, but it is wrong.
//
// Branches, with the consumer (client RVA 0x253CFE0) that gives each field its meaning:
//   Type 0,1,6,7  (no payload)  pure error carriers
//   Type 2        int32 PerksVendorItemID; uint32 N; N x PerksRecentPurchase
//                 -> PERKS_PROGRAM_PURCHASE_SUCCESS(vendorItemID). The array REPLACES the client's whole
//                    recent-purchases map, so it must be the complete current list, not a delta.
//   Type 3        same shape as Type 2
//                 -> PERKS_PROGRAM_REFUND_SUCCESS(vendorItemID); the client drops the item from its purchase
//                    map. The array is read and discarded here, but it must still be on the wire.
//   Type 4        int32; int32; int32 TenderAwarded; uint32 N; N x int32
//                 -> PERKS_PROGRAM_CURRENCY_AWARDED(TenderAwarded) when > 0, and UNCONDITIONALLY makes the
//                    client send CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS back. The first two int32 and the
//                    array are never read by the consumer.
//   Type 5        PackedGuid VendorGUID; PackedGuid DisplayGUID; uint32 N; 7 x int32; N x PerksVendorItem
//                 -> starts PlayerInteractionType::PerksProgramVendor (57) client-side, loads the vendor list
//                    as a FULL replace, and fires PERKS_PROGRAM_OPEN.
//   Type 8        int32 TenderAwarded  -> PERKS_PROGRAM_CURRENCY_AWARDED, no follow-up request
//   Type 9        uint32 N; N x PerksVendorItem  -> vendor list MERGE (not a replace) + PERKS_PROGRAM_DATA_REFRESH
//
// MIND THE ORDER IN TYPE 5: the count is read BEFORE the seven int32, the elements only after them. Pairing
// the count directly with its elements shifts the message by 28 bytes.
//
// THIS MESSAGE IS THE ONLY WAY THE TRADING POST OPENS. ShowPerksProgramFrame() has exactly one caller,
// GameEvent.HandlePerksProgramOpen (Blizzard_Game/Mainline/EventImplementation.lua:552), routed from the
// PERKS_PROGRAM_OPEN event; and that event's hash (0x6D1C227CE2DF5BF0) is fired at exactly one site in the
// whole client image -- RVA 0x2540652, the Type 5 callback. There is no Lua path and no other packet that
// opens the window.
//
// D1: the two available captures decode with zero bytes left over.
//   * 68974, 2215 bytes, Type 5: header 0x50; two Creature PackedGuids of 13 bytes each; count 44; 28 zero
//     bytes (the seven int32); 44 x 49-byte PerksVendorItem. 1+13+13+4+28+44*49 = 2215.
//   * 66220, 17 bytes, Type 4: header 0x40; int32 3; int32 3; int32 700 (Trader's Tender awarded); count 0.
//     1+4+4+4+4 = 17.
// No 12.1 capture of this opcode exists (0 in all 51 recordings), so the structure is carried by the reader,
// not by 12.1 reference bytes.
//
// ERROR CHANNEL. On Err != 0 the consumer looks up GameError = dword[0x39D14D0 + 4*(Err + 4*Type)] and calls
// the error display at RVA 0x209AD90, which discards anything >= 1243. The table holds 8 rows of 4 and is
// identical in every row: Err 1 -> 1053 ERR_CANT_DO_THAT_RIGHT_NOW, Err 2 -> 1053, Err 3 -> 1 ERR_INTERNAL_ERROR.
// Then it fires PERKS_PROGRAM_RESULT_ERROR, which has no payload and puts the Trading Post UI into a global
// error state that only reopening clears.
//
// *** Err != 0 IS ONLY SAFE WITH Type 0..7. *** Type 8 or 9 with Err == 2 reads past the end of that table
// into pointer data, yielding a large negative value that passes the >= 1243 guard and crashes the client.
// SendPerksProgramResultError() below is the only intended way to build an error and cannot express that
// combination.
class PerksProgramResult final : public ServerPacket
{
public:
    // bits4 discriminant. Only the values the server can fill honestly are named; 1, 6 and 7 behave exactly
    // like 0 (empty error carriers) and 10-15 are inert.
    enum ResultType : uint8
    {
        ResultTypeError             = 0,
        ResultTypePurchaseSuccess   = 2,
        ResultTypeRefundSuccess     = 3,
        ResultTypeTenderAwarded     = 4,
        ResultTypeVendorOpen        = 5,
        ResultTypeTenderGranted     = 8,
        ResultTypeVendorMerge       = 9
    };

    // bits2. The client maps these through the GameError table above; 1 and 2 are indistinguishable to the
    // player (both show ERR_CANT_DO_THAT_RIGHT_NOW), 3 shows ERR_INTERNAL_ERROR.
    enum ResultError : uint8
    {
        ResultErrorNone             = 0,
        ResultErrorCantDoThat       = 1,
        ResultErrorCantDoThatAlt    = 2,
        ResultErrorInternal         = 3
    };

    explicit PerksProgramResult(ResultType type = ResultTypeError) : ServerPacket(SMSG_PERKS_PROGRAM_RESULT), Type(type) { }

    WorldPacket const* Write() override;

    ResultType Type;
    ResultError Err = ResultErrorNone;

    // UNVERIFIED: no capture has this bit set and no consumer reads the value -- the whole access set of the
    // consumer RVA 0x253CFE0 lacks both the flag and the field. Left unset; only the reader proves it exists.
    Optional<uint64> Stamp;

    int32 PerksVendorItemID = 0;                    // Type 2, 3
    std::vector<PerksRecentPurchase> Purchases;     // Type 2 (complete list), Type 3 (read and discarded)
    int32 TenderAwarded = 0;                        // Type 4, 8
    ObjectGuid VendorGUID;                          // Type 5: the interacted Trading Post vendor
    ObjectGuid DisplayGUID;                         // Type 5, see the note in Write()
    std::vector<PerksVendorItem> VendorItems;       // Type 5 (full replace), Type 9 (merge)
};

}

#endif // TRINITYCORE_PERKS_PROGRAM_PACKETS_H

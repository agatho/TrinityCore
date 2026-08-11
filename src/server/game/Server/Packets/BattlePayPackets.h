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

#ifndef TRINITYCORE_BATTLE_PAY_PACKETS_H
#define TRINITYCORE_BATTLE_PAY_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include <string>
#include <vector>

namespace WorldPackets
{
    namespace BattlePay
    {
        // Client requests the shop catalog. Body carries a locale/region selector we do not need.
        class GetProductList final : public ClientPacket
        {
        public:
            explicit GetProductList(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_GET_PRODUCT_LIST, std::move(packet)) { }

            void Read() override { }
        };

        // Client requests the account purchase/distribution list.
        class GetPurchaseList final : public ClientPacket
        {
        public:
            explicit GetPurchaseList(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_GET_PURCHASE_LIST, std::move(packet)) { }

            void Read() override { }
        };

        // The 12.0.7 catalog is a nested reflection bitstream that cannot be re-serialized field-by-field
        // offline (see docs). For P0 we replay a byte-exact, client-validated catalog blob captured from a
        // real 68275 session, so the shop opens and displays real products. RawData is the message BODY
        // (opcode dword already stripped); the ServerPacket base prepends the opcode header.
        class ProductListResponse final : public ServerPacket
        {
        public:
            explicit ProductListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_PRODUCT_LIST_RESPONSE) { }

            WorldPacket const* Write() override;

            std::vector<uint8> const* RawData = nullptr;
        };

        // JamBattlePayDeliverable - what a product actually hands over. Layout recovered from the client's
        // own parser (sub_7FF729139460 @ image base 0x7FF728AA0000) and validated byte-exact against the
        // 68275 capture, where the embedded record decodes with zero remainder to catalog deliverable
        // 1161 { type = 1 CharacterBoost, boostID = 11, flags = 1620 } - the very values our decoded
        // catalog carries for that deliverable, which is what closes the loop on this struct.
        //
        // `Type` is the catalog's own deliverable vocabulary: 1 CharacterBoost, 2 BattlePet, 3 Mount,
        // 4 WowToken, 5 NameChange, 6 FactionChange, 8 RaceChange, 11 CharacterTransfer, 13 TransmogSet,
        // 14 Item/Toy, 18 GameUpgrade, 26 TransmogEnsemble.
        struct DistributionDeliverable
        {
            uint32 DeliverableID = 0;
            uint32 Type = 0;
            uint32 ItemID = 0;
            uint32 Quantity = 0;
            uint32 MountSpellID = 0;
            uint32 BattlePetCreatureID = 0;
            uint32 BoostID = 0;
            uint32 Flags = 0;
            uint32 TransItemModifiedAppearanceID = 0;
            uint32 TransmogSetID = 0;
            uint32 CharTitleID = 0;
            uint32 SpellItemEnchantmentID = 0;
            uint32 WarbandSceneID = 0;
            std::string Name;
            bool AlreadyOwns = false;
            // Choices and DisplayInfo are never emitted: DisplayInfo is a ~21 KB bit-packed struct that
            // has NOT been decoded (it is absent from both capture samples, has_displayInfo = 0), so we
            // always write the "no choices, no display info" form the captures show.
        };

        // JamBattlePayDistributionObject - one entitlement. Layout recovered from the client's parser
        // (sub_7FF729139990) and validated byte-exact: the captured 101-byte body decodes to
        // { distributionID = 0x1E828000009EECAC, status = 1, deliverableID = 1161,
        //   licenseGameAccountGUID = packed mask 0x800F, targetPlayer = empty, realms = 0,
        //   purchaseID = 0, manualReview = 0, flags = 0x80 (hasDeliverable) } + the 55-byte deliverable,
        // consuming all 101 bytes with nothing left over.
        struct DistributionObject
        {
            uint64 DistributionID = 0;
            uint32 Status = 0;              // 1 = available; the only value ever observed on the wire
            uint32 DeliverableID = 0;
            ObjectGuid LicenseGameAccountGUID;
            ObjectGuid TargetPlayer;        // empty while unassigned
            uint32 TargetNativeRealm = 0;
            uint32 TargetVirtualRealm = 0;
            uint64 PurchaseID = 0;
            uint32 ManualReview = 0;
            bool Revoked = false;
            Optional<DistributionDeliverable> Deliverable;
        };

        // Sent unsolicited at session start (character select) and again at login. There is no CMSG that
        // requests it - confirmed against the client's opcode table: no CMSG_BATTLE_PAY_GET_DISTRIBUTION_LIST
        // exists in this build. The client's StoreFrame_IsLoading gate keeps the Shop on "Loading, please
        // wait" until C_StoreSecure.HasDistributionList() flips, which this response does.
        //
        // Header PROVEN: the captured body starts `00 00 00 00 00 20`, and `uint32 Result` followed by
        // WriteBits(1, 11) + FlushBits() reproduces those six bytes exactly. The client reads the count
        // back as `(b0 << 3) | (b1 >> 5)`, which is precisely TC's 11-bit MSB-first encoding. An empty
        // list is the 6-byte form, also observed live.
        //
        // RawData replays the captured blob instead, and is what we send while entitlements are off.
        class GetDistributionListResponse final : public ServerPacket
        {
        public:
            explicit GetDistributionListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_DISTRIBUTION_LIST_RESPONSE) { }

            WorldPacket const* Write() override;

            std::vector<uint8> const* RawData = nullptr;

            bool BuildFromObjects = false;
            uint32 Result = 0;                              // PurchaseResult; 0 = Ok in every capture
            std::vector<DistributionObject> Distributions;
        };

        // Pushes a single changed entitlement: the body is exactly one DistributionObject with NO header
        // of any kind (the client's parser sub_7FF7290A8090 does nothing but read the object). Proven by
        // the 68275 capture, where this opcode appears 297 times with a 101-byte body byte-identical to
        // the object embedded in the 107-byte list response. Receiving it makes the client fire
        // PRODUCT_DISTRIBUTIONS_UPDATED, which refreshes the character-select token row and the Shop.
        class DistributionUpdate final : public ServerPacket
        {
        public:
            explicit DistributionUpdate() : ServerPacket(SMSG_BATTLE_PAY_DISTRIBUTION_UPDATE, 101) { }

            WorldPacket const* Write() override;

            DistributionObject Distribution;
        };

        // Client asks to apply one owned entitlement to a character it picked. Layout PROVEN by
        // disassembling the client's serializer (sub_7FF729079930): it writes the opcode and then
        // exactly uint32, uint64, PackedGuid, uint32. This packet appears in none of our captures, so the
        // field NAMES below are the plausible reading rather than a client-derived fact - which is why
        // the handler additionally validates both ids against values this server issued before acting.
        class DistributionAssignToTarget final : public ClientPacket
        {
        public:
            explicit DistributionAssignToTarget(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_DISTRIBUTION_ASSIGN_TO_TARGET, std::move(packet)) { }

            void Read() override;

            uint32 ClientToken = 0;
            uint64 DistributionID = 0;
            ObjectGuid TargetCharacter;
            uint32 ProductChoice = 0;       // a ChrSpecialization id for boosts, per the external fork
        };

        // Server's answer to an assign. Structure PROVEN by disassembling the client's parser
        // (sub_7FF7290A8F70): uint32, uint32, uint64. Never observed on the wire, so the field meanings
        // are inferred - the first uint32 is taken to be the PurchaseResult the client surfaces through
        // PRODUCT_ASSIGN_TO_TARGET_FAILED, and the middle uint32 is sent as 0 because we do not know it.
        class StartDistributionAssignToTargetResponse final : public ServerPacket
        {
        public:
            explicit StartDistributionAssignToTargetResponse() : ServerPacket(SMSG_BATTLE_PAY_START_DISTRIBUTION_ASSIGN_TO_TARGET_RESPONSE, 16) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            uint32 Unknown = 0;
            uint64 DistributionID = 0;
        };

        // Client initiates an in-game purchase. Layout from the client Write method (0x5d9f90):
        // u32, u64, then a 1-bit bool. The u32 is the strong candidate for the productID (the setter is
        // Warden-obfuscated so the exact semantic is runtime-confirmed via the handler's diagnostic log).
        class StartPurchase final : public ClientPacket
        {
        public:
            explicit StartPurchase(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_START_PURCHASE, std::move(packet)) { }

            void Read() override;

            // Layout settled from live 505-byte packets (three clicks, two products):
            //   @0  u32 ClientToken - a per-session click counter (observed 1, 2, 3)
            //   @4  u32 ProductID   - the entry productID the client buys by; stable across two
            //                         attempts at the same pet (1448, 1448) and different for
            //                         another card (1061). This is entryInfo.productID, which
            //                         Blizzard_StoreUI passes to C_StoreSecure.PurchaseProduct().
            //   @8  u32 (always 0 so far)
            //   then bit-packed lengths + "win" (platform) + a ~480 char client attestation blob
            //   { "RGKY" : ..., "CPGE" : ... } which we do not need and do not parse.
            // The old reader took the FIRST scalar as the product, i.e. the click counter, so no
            // purchase ever resolved.
            uint32 ClientToken = 0;
            uint32 ProductID = 0;
            uint32 Unused = 0;
            bool Flag = false;
        };

        // Client opens the checkout. The u32 is the ClientToken the server must echo back verbatim in
        // SMSG_GENERATE_SSO_TOKEN_RESPONSE (proven 1:1 in all 8 captures - checkout #N -> response #N
        // with the same u32). It is not a distributionID. See COMMERCE_AUDIT C-09.
        class OpenCheckout final : public ClientPacket
        {
        public:
            explicit OpenCheckout(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_OPEN_CHECKOUT, std::move(packet)) { }

            void Read() override;

            uint32 ClientToken = 0;
        };

        // Server-driven purchase confirmation prompt (retail interposes this between StartPurchase and
        // completion; it clears the client's WaitingOnConfirmation and shows the confirm dialog, whose
        // C_StoreSecure.GetConfirmationInfo() reads productID, walletName and the current price).
        //
        // INFERRED LAYOUT - NOT byte-verified: the 68275 client read struct (sub_7FF7290A91A0, opcode
        // 0x420232) is an opaque nested reflection struct in the RE dump and this opcode never appears
        // on-wire in any of the 8 captures (retail hands purchases to web checkout). The field set below
        // is the classic JamBattlePayConfirmPurchase shape that GetConfirmationInfo consumes. Because a
        // malformed packet could disconnect a live client, sending this is gated behind the
        // Shop.PurchaseConfirmation config (default off) until a live client validates the layout; the
        // proven direct-grant path (StartPurchase -> grant/charge -> PurchaseUpdate) stays the default.
        class ConfirmPurchase final : public ServerPacket
        {
        public:
            explicit ConfirmPurchase() : ServerPacket(SMSG_BATTLE_PAY_CONFIRM_PURCHASE, 8 + 4 + 8 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint64 PurchaseID = 0;
            uint32 ProductID = 0;
            uint64 CurrentPriceFixedPoint = 0;  // wire fixed-point /100000 (same scale as the catalog)
            uint32 ServerToken = 0;             // echoed back in the response so we can match the prompt
        };

        // Client's answer to the confirmation prompt. Layout byte-grounded from the 68275 client read of
        // CMSG_BATTLE_PAY_CONFIRM_PURCHASE_RESPONSE (0x4000fb): a u32 then a 1-bit bool. The u32 echoes
        // our ServerToken; the bool is confirm(true)/cancel(false).
        class ConfirmPurchaseResponse final : public ClientPacket
        {
        public:
            explicit ConfirmPurchaseResponse(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_CONFIRM_PURCHASE_RESPONSE, std::move(packet)) { }

            void Read() override;

            uint32 ServerToken = 0;
            bool Confirmed = false;
        };

        // Server ack for StartPurchase. Layout from the client read ctor (0x608ec0): u32, u32, u64.
        class StartPurchaseResponse final : public ServerPacket
        {
        public:
            explicit StartPurchaseResponse() : ServerPacket(SMSG_BATTLE_PAY_START_PURCHASE_RESPONSE, 16) { }

            WorldPacket const* Write() override;

            uint32 ResultA = 0;
            uint32 ResultB = 0;
            uint64 PurchaseID = 0;
        };

        // One JamBattlePayPurchase record. Wire order (68974 capture, TESTER_SNIFF2_LINDORMI_MINE):
        // fields below in declaration order, then a record-final u8 walletName length (sent empty, see .cpp).
        struct PurchaseRecord
        {
            uint64 PurchaseID = 0;
            int32 Status = 0;       // BattlepayPurchaseStatus: live 68974 completed purchases carry 6 (failed VAS: 12)
            int32 ResultCode = 0;   // PurchaseResult: Ok=0
            uint32 ProductID = 0;
            uint64 BasePrice = 0;
            uint64 UserPrice = 0;
            int64 TimeCreated = 0;
        };

        // Server drives purchase progress/completion. Layout from client read ctor (0x6090d0):
        // u32 result, then a u32-counted vector of JamBattlePayPurchase. status=6 signals completion
        // (live 68974 value; see PurchaseRecord) and the record echoes the productID delivered.
        class PurchaseUpdate final : public ServerPacket
        {
        public:
            explicit PurchaseUpdate() : ServerPacket(SMSG_BATTLE_PAY_PURCHASE_UPDATE) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            std::vector<PurchaseRecord> Purchases;
        };

        // Reply to CMSG_BATTLE_PAY_GET_PURCHASE_LIST. Body layout is identical to
        // SMSG_BATTLE_PAY_PURCHASE_UPDATE: { uint32 Result, uint32 Count, Count x PurchaseRecord }.
        // Proven against a live sniff: a retail account with 9 purchases produced a 413-byte body, and
        // 8 (header) + 9 * 45 (PurchaseRecord = u64+i32+i32+u32+u64+u64+i64+u8) == 413 exactly. The
        // record layout (walletName length record-final) matches the fixed PurchaseUpdate serializer.
        class GetPurchaseListResponse final : public ServerPacket
        {
        public:
            explicit GetPurchaseListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            std::vector<PurchaseRecord> Purchases;
        };
    }
}

#endif // TRINITYCORE_BATTLE_PAY_PACKETS_H

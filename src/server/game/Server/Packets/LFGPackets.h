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

#ifndef TRINITYCORE_LFG_PACKETS_H
#define TRINITYCORE_LFG_PACKETS_H

#include "Packet.h"
#include "PacketUtilities.h"
#include "ItemPacketsCommon.h"
#include "LFGPacketsCommon.h"
#include "Optional.h"
#include <array>

namespace lfg
{
    enum LfgTeleportResult : uint8;
}

namespace WorldPackets
{
    namespace LFG
    {
        class DFJoin final : public ClientPacket
        {
        public:
            explicit DFJoin(WorldPacket&& packet) : ClientPacket(CMSG_DF_JOIN, std::move(packet)) { }

            void Read() override;

            bool QueueAsGroup = false;
            bool Mercenary = false;
            Optional<uint8> PartyIndex;
            uint8 Roles = 0;
            Array<uint32, 50> Slots;
        };

        class DFLeave final : public ClientPacket
        {
        public:
            explicit DFLeave(WorldPacket&& packet) : ClientPacket(CMSG_DF_LEAVE, std::move(packet)) { }

            void Read() override;

            RideTicket Ticket;
        };

        class DFProposalResponse final : public ClientPacket
        {
        public:
            explicit DFProposalResponse(WorldPacket&& packet) : ClientPacket(CMSG_DF_PROPOSAL_RESPONSE, std::move(packet)) { }

            void Read() override;

            RideTicket Ticket;
            uint64 InstanceID = 0;
            uint32 ProposalID = 0;
            bool Accepted = false;
        };

        class DFSetRoles final : public ClientPacket
        {
        public:
            explicit DFSetRoles(WorldPacket&& packet) : ClientPacket(CMSG_DF_SET_ROLES, std::move(packet)) { }

            void Read() override;

            uint8 RolesDesired = 0;
            Optional<uint8> PartyIndex;
        };

        class DFBootPlayerVote final : public ClientPacket
        {
        public:
            explicit DFBootPlayerVote(WorldPacket&& packet) : ClientPacket(CMSG_DF_BOOT_PLAYER_VOTE, std::move(packet)) { }

            void Read() override;

            bool Vote = false;
        };

        class DFTeleport final : public ClientPacket
        {
        public:
            explicit DFTeleport(WorldPacket&& packet) : ClientPacket(CMSG_DF_TELEPORT, std::move(packet)) { }

            void Read() override;

            bool TeleportOut = false;
        };

        class DFGetSystemInfo final : public ClientPacket
        {
        public:
            explicit DFGetSystemInfo(WorldPacket&& packet) : ClientPacket(CMSG_DF_GET_SYSTEM_INFO, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool Player = false;
        };

        // Client serializer 12.0.7.68275 @ RVA 0x5D1980 (image base 0x7FF728AA0000):
        //   write_u32(0x400036) ; write_PackedGuid(+0x20) ; write_u32(+0x30) ; write_u32(+0x34)
        //   write_u64(+0x38)    ; WriteBit(+0x40) ; FlushBits          <- byte for byte a RideTicket
        //   WriteBit(+0x48)     ; FlushBits
        // The ticket is the one the client stashed from SMSG_LFG_EXPAND_SEARCH_PROMPT (handler @ RVA 0x2301A90
        // copies the 40 bytes at msg+0x20 straight into a global), echoed back unchanged.
        // In practice Accepted is always true: the LFG_QUEUE_EXPAND static popup wires OnAccept to
        // C_LFGInfo.ConfirmLfgExpandSearch and gives button2 (NO) no handler at all, so a decline sends nothing.
        class DFConfirmExpandSearch final : public ClientPacket
        {
        public:
            explicit DFConfirmExpandSearch(WorldPacket&& packet) : ClientPacket(CMSG_DF_CONFIRM_EXPAND_SEARCH, std::move(packet)) { }

            void Read() override;

            RideTicket Ticket;
            bool Accepted = false;
        };

        class DFGetJoinStatus final : public ClientPacket
        {
        public:
            explicit DFGetJoinStatus(WorldPacket&& packet) : ClientPacket(CMSG_DF_GET_JOIN_STATUS, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_DF_READY_CHECK_RESPONSE (0x430048) - Client 12.1.0.69382, serializer @ RVA 0x6A5240:
        //   Write<uint32>(0x430048) ; bit has(PartyIndex) ; bit IsReady ; FlushBits ; if (has) Write<uint8>(PartyIndex)
        // Payload is 1..2 bytes. The presence bit is written FIRST (it is the more significant bit of the
        // accumulator), exactly like CMSG_SET_EVERYONE_IS_ASSISTANT (0x430046, serializer 0x6A5080) whose
        // TrinityCore Read() (PartyPackets.cpp) already uses OptionalInit-then-flag ordering.
        // Sender: the global Lua binding CompleteLFGReadyCheck(isReady) (thunk 0x24CCA50 -> body 0x24CC200).
        // The client only sends while a ready check is running (`cmp byte [0x4D195C9], 2`) and echoes back the
        // first uint8 of SMSG_LFG_READY_CHECK_UPDATE.
        // UNVERIFIED: the NAME PartyIndex. Position and optionality are measured; the name is the reading that
        // matches every other CMSG_DF_* message. The wire layout does not depend on it.
        class DFReadyCheckResponse final : public ClientPacket
        {
        public:
            explicit DFReadyCheckResponse(WorldPacket&& packet) : ClientPacket(CMSG_DF_READY_CHECK_RESPONSE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool IsReady = false;
        };

        // CMSG_LFG_LOREWALKING_UPDATE_REQUEST (0x3D0259) - Client 12.1.0.69382, serializer @ RVA 0x6D1520:
        //   Write<uint32>(0x3D0259) ; Write<uint32>(**(uint32**)(obj+32))
        // Exactly one uint32 on the wire (the double indirection is a pointer-held field, not a second read).
        // Send site 0x24C15B9 inside 0x24C0A70 (the routine that also processes SMSG_LFG_UPDATE_STATUS and
        // the queue status). The client sends this with NO Lua call - it arises purely inside the
        // queue-status path, gated on a bit-set membership test (bit 13 or bit 32 of a small uint32 vector
        // hanging off a singleton; the same test gates the alternate-progression content filter at
        // 0x2322810, so this is the Chromie-Time / Lorewalking family of gates).
        //
        // The payload is resolved, not guessed. At 0x24C0F29 the client fetches the LFGDungeons record for
        // Slots[0] & 0xFFFFF and reads reflection index 9 through the 16-bit field getter, then sign-extends
        // (`movsx ebx, ax`). The store at 0x655FCE0 is LFGDungeons: its DB2Meta at 0x3B17ED0 names it
        // literally, carries FileDataID 1361033, 33 fields, 71 packed record bytes and layout hash
        // 0x34B02DE8 - which is exactly the newest LAYOUT in WoWDBDefs/definitions/LFGDungeons.dbd. Walking
        // the meta's packed-offset table at 0x3B181B0 yields per-field widths that match the .dbd column
        // list byte for byte and terminate at 71, which establishes that reflection index N is the N-th
        // non-ID .dbd column. Index 9 is therefore MapID<16>, at packed offset 0x18.
        // Because MapID is SIGNED, a MapID of -1 arrives as 0xFFFFFFFF - hence int32, not uint32.
        class LFGLorewalkingUpdateRequest final : public ClientPacket
        {
        public:
            explicit LFGLorewalkingUpdateRequest(WorldPacket&& packet) : ClientPacket(CMSG_LFG_LOREWALKING_UPDATE_REQUEST, std::move(packet)) { }

            void Read() override;

            int32 MapID = 0;                // LFGDungeons.MapID of Slots[0], sign-extended from int16
        };

        // Two opcodes of family 0x5A are deliberately NOT built here, and this is the record of why.
        //
        //   0x5A0007  dispatcher case @ RVA 0x755AAD, hook slot 0x462ED20 = NULL, exactly 1 xref, no registrar
        //   0x5A0013  dispatcher case @ RVA 0x7561DB, hook slot 0x462ED28 = NULL, exactly 1 xref, no registrar
        //
        // Both cases read no field at all: they take a raw pointer to the rest of the payload (0x35AF730,
        // which copies nothing and does not even store the length) and hand it on, and both share the stub
        // serializer 0x217F10 (`return 0;`). TrinityCore 12.1 gives neither a name.
        //
        // "NULL hook with exactly one xref and no registrar" is the signature of a consumer that lives in
        // Blizzard's DEVELOPER build, not in the retail client. That does NOT mean the opcodes are unused -
        // it means their counterpart is not in the public client, so their format cannot be derived
        // offline: the field structure only comes into existence inside the consumer, and there is no
        // consumer to read. (Contrast SMSG_OPEN_LFG_DUNGEON_FINDER, whose case is equally "raw" but whose
        // consumer 0x24C4E30 dereferences the pointer as a uint32 - there "raw" was decidable.)
        //
        // Consequence for acceptance: step 3 of the verification loop - send it and watch the UI react -
        // does not apply to these two. They are documented, not built.

        struct LFGBlackListSlot
        {
            LFGBlackListSlot() = default;
            LFGBlackListSlot(uint32 slot, uint32 reason, int32 subReason1, int32 subReason2, uint32 softLock)
                : Slot(slot), Reason(reason), SubReason1(subReason1), SubReason2(subReason2), SoftLock(softLock) { }

            uint32 Slot = 0;
            uint32 Reason = 0;
            int32 SubReason1 = 0;
            int32 SubReason2 = 0;
            uint32 SoftLock = 0;
        };

        struct LFGBlackList
        {
            Optional<ObjectGuid> PlayerGuid;
            std::vector<LFGBlackListSlot> Slot;
        };

        struct LfgPlayerQuestRewardItem
        {
            LfgPlayerQuestRewardItem() = default;
            LfgPlayerQuestRewardItem(int32 itemId, int32 quantity) : ItemID(itemId), Quantity(quantity) { }

            int32 ItemID = 0;
            int32 Quantity = 0;
        };

        struct LfgPlayerQuestRewardCurrency
        {
            LfgPlayerQuestRewardCurrency() = default;
            LfgPlayerQuestRewardCurrency(int32 currencyID, int32 quantity) : CurrencyID(currencyID), Quantity(quantity) { }

            int32 CurrencyID = 0;
            int32 Quantity = 0;
        };

        struct LfgPlayerQuestReward
        {
            uint8 Mask = 0;                                             // Roles required for this reward, only used by ShortageReward in SMSG_LFG_PLAYER_INFO
            int32 RewardMoney = 0;                                      // Only used by SMSG_LFG_PLAYER_INFO
            int32 RewardXP = 0;
            std::vector<LfgPlayerQuestRewardItem> Item;
            std::vector<LfgPlayerQuestRewardCurrency> Currency;         // Only used by SMSG_LFG_PLAYER_INFO
            std::vector<LfgPlayerQuestRewardCurrency> BonusCurrency;    // Only used by SMSG_LFG_PLAYER_INFO
            Optional<int32> RewardSpellID;                              // Only used by SMSG_LFG_PLAYER_INFO
            Optional<int32> ArtifactXPCategory;
            Optional<uint64> ArtifactXP;
            Optional<int32> Honor;                                      // Only used by SMSG_REQUEST_PVP_REWARDS_RESPONSE
        };

        struct LfgPlayerDungeonInfo
        {
            uint32 Slot = 0;
            int32 CompletionQuantity = 0;
            int32 CompletionLimit = 0;
            int32 CompletionCurrencyID = 0;
            int32 SpecificQuantity = 0;
            int32 SpecificLimit = 0;
            int32 OverallQuantity = 0;
            int32 OverallLimit = 0;
            int32 PurseWeeklyQuantity = 0;
            int32 PurseWeeklyLimit = 0;
            int32 PurseQuantity = 0;
            int32 PurseLimit = 0;
            int32 Quantity = 0;
            uint32 CompletedMask = 0;
            uint32 EncounterMask = 0;
            bool FirstReward = false;
            bool ShortageEligible = false;
            LfgPlayerQuestReward Rewards;
            std::vector<LfgPlayerQuestReward> ShortageReward;
        };

        class LfgPlayerInfo final : public ServerPacket
        {
        public:
            explicit LfgPlayerInfo() : ServerPacket(SMSG_LFG_PLAYER_INFO) { }

            WorldPacket const* Write() override;

            LFGBlackList BlackList;
            std::vector<LfgPlayerDungeonInfo> Dungeon;
        };

        // SMSG_REQUEST_PVP_REWARDS_RESPONSE (0x4B0014). Reply to the empty CMSG_REQUEST_PVP_REWARDS
        // (0x3D0041); in every capture the reply follows the request 100-250 ms later, 1:1.
        //
        // The body is a FIXED thirteen activity blocks - there is no count field - plus two loose flag bytes.
        // Each block is exactly LfgPlayerQuestReward above, which is why that struct already carries the
        // `Honor` optional.
        //
        // FIELD ORDER: the two flag bytes MOVED between builds, and Write() emits the 12.1 order.
        //   12.0.7 (68275/68453): Block[0], u8, u8, Block[1..12].
        //   12.1   (69382):       Block[0..12], u8, u8.
        // Both readings are measured, not assumed, and they disagree. For 12.0.7 all six captured bodies
        // (304, 304, 348, 348, 584, 592 - two of the files are byte-identical copies) parse to exactly zero
        // bytes left over with the flags after block 0, and parse to a hard desync with the flags at the end:
        // implausible array counts inside block 1 on five of six, four bytes left over on the sixth. Since the
        // blocks are variable length, that is discriminating and not a coincidence of equal totals. The
        // 12.0.7 client agrees with its own capture: its reader sub_7FF7290FB600 calls the per-block reader
        // sub_7FF7291DAB70 thirteen times with the two loose u8 reads after block 0.
        // For 12.1 the reader at 0x732900 settles it the other way: 0x732919..0x7329CD are thirteen
        // `lea rcx, [rdi + 0x20 + 0x80*k]; call 0x756BA0` block reads, and the two `call 0x35AF050`
        // (Read<uint8>) only follow at 0x7329DF and 0x732A72. Verified by disassembling the range directly.
        // No 12.1 capture of this opcode exists, so the flip is not confirmed on the wire in that build - but
        // the client binary is the arbiter over an older capture, and 12.1 is the build this tree serves:
        // sql/updates/auth/master/2026_08_22_00_auth.sql adds build 69404 (12.1.0), and the opcode table is on
        // the 12.1 values (SMSG_PONG = 0x4C0009). Emitting the 12.0.7 order here would desync every reply.
        // Reverting for a 12.0.7 realm is one loop and one pair of writes, right here in Write().
        //
        // Retail leaves blocks it has nothing to say about entirely zero - the rated Blitz capture sent 11
        // of 13 populated, a levelling character only 4 - so an unimplemented activity is written empty
        // rather than omitted, and that is a wire-legal state rather than a stub.
        class RequestPvpRewardsResponse final : public ServerPacket
        {
        public:
            // Slot order is not guesswork: each block lands in its own client global, and each global is
            // read by exactly one C_PvP getter which embeds its own name string as the Lua arg-check
            // argument, so the blocks are self-labelling. Blocks 3/4 and 5/6/8/10 are the two multiplexed
            // getters (GetArenaRewards by team size, GetBrawlRewards by brawl type).
            enum PvpRewardSlot : uint8
            {
                RandomBattleground      = 0,        // C_PvP.GetRandomBGRewards
                RatedBattleground       = 1,        // C_PvP.GetRatedBGRewards
                ArenaSkirmish           = 2,        // C_PvP.GetArenaSkirmishRewards
                Arena2v2                = 3,        // C_PvP.GetArenaRewards(2)
                Arena3v3                = 4,        // C_PvP.GetArenaRewards(3)
                BrawlBattleground       = 5,        // C_PvP.GetBrawlRewards(Battleground)
                BrawlArena              = 6,        // C_PvP.GetBrawlRewards(Arena), aliased by (LFG)
                RandomEpicBattleground  = 7,        // C_PvP.GetRandomEpicBGRewards
                BrawlSoloShuffle        = 8,        // C_PvP.GetBrawlRewards(SoloShuffle)
                RatedSoloShuffle        = 9,        // C_PvP.GetRatedSoloShuffleRewards
                BrawlSoloRbg            = 10,       // C_PvP.GetBrawlRewards(SoloRbg)
                BattlegroundBlitz       = 11,       // C_PvP.GetRatedSoloRBGRewards
                RandomTrainingGround    = 12,       // C_PvP.GetRandomTrainingGroundRewards
                MaxPvpRewardSlot        = 13
            };

            explicit RequestPvpRewardsResponse() : ServerPacket(SMSG_REQUEST_PVP_REWARDS_RESPONSE) { }

            WorldPacket const* Write() override;

            std::array<LfgPlayerQuestReward, MaxPvpRewardSlot> Activity = { };

            // The two loose bytes are read as two plain uint8 and then tested bit by bit. What the captures
            // actually carried, so the account below can be checked against it:
            //   BrawlFlags  0x02 in the two non-PvP captures, 0x03 in the rated Blitz one.
            //   ExtraFlags  0xC0 in all six occurrences.
            //
            // Four of those set bits are understood. BrawlFlags 0x08/0x04/0x02 and ExtraFlags 0x40 are the
            // per-brawl-type "has already won this brawl" markers the client returns as the extra `hasWon`
            // value from GetBrawlRewards. We run no brawl rotation, so nothing here has been won and all four
            // are written false - which is why BrawlFlags leaves as 0x00 although no capture was 0x00, and
            // why ExtraFlags leaves as 0x80 although every capture was 0xC0. That is a decided value, not an
            // omission.
            //
            // UNVERIFIED: ExtraFlags 0x80. Set in all six occurrences and gates something inside the client's
            // Rated Solo Shuffle path; the branch it feeds was not followed to a conclusion. It is written at
            // the value that was observed and nothing more.
            //
            // UNVERIFIED: BrawlFlags 0x01. Set in the rated Blitz capture, clear in the two non-PvP ones. It
            // is not one of the four brawl markers above and it is not a bit that was zero everywhere, so
            // neither reading covers it and its meaning is not established. It is written CLEAR: we cannot
            // say what setting it would promise the client, and clear is the value two of the three distinct
            // captures carried. A capture taken from a session with and without an active rated Blitz week
            // would decide it - see aufnahme_noetig.
            uint8 BrawlFlags = 0x00;
            uint8 ExtraFlags = 0x80;
        };

        // SMSG_REQUEST_PVP_REWARDS_RESPONSE (0x480014). Reply to the empty CMSG_REQUEST_PVP_REWARDS
        // (0x3A0041); in every capture the reply follows the request 100-250 ms later, 1:1.
        //
        // The body is a FIXED thirteen activity blocks - there is no count field - with two loose bytes
        // after the first block. Each block is exactly LfgPlayerQuestReward above, which is why that struct
        // already carries the `Honor` optional. Decoded from all 6 occurrences in the 12.0.7 captures
        // (bodies 304, 304, 348, 348, 584, 592); the parser consumes every one with zero bytes left over,
        // and the client reader sub_7FF7290FB600 independently calls the per-block reader sub_7FF7291DAB70
        // exactly thirteen times with the same two loose u8 reads after block 0.
        //
        // Retail leaves blocks it has nothing to say about entirely zero - the rated Blitz capture sent 11
        // of 13 populated, a levelling character only 4 - so an unimplemented activity is written empty
        // rather than omitted, and that is a wire-legal state rather than a stub.
        // SMSG_REQUEST_PVP_REWARDS_RESPONSE (0x480014). Reply to the empty CMSG_REQUEST_PVP_REWARDS
        // (0x3A0041); in every capture the reply follows the request 100-250 ms later, 1:1.
        //
        // The body is a FIXED thirteen activity blocks - there is no count field - with two loose bytes
        // after the first block. Each block is exactly LfgPlayerQuestReward above, which is why that struct
        // already carries the `Honor` optional. Decoded from all 6 occurrences in the 12.0.7 captures
        // (bodies 304, 304, 348, 348, 584, 592); the parser consumes every one with zero bytes left over,
        // and the client reader sub_7FF7290FB600 independently calls the per-block reader sub_7FF7291DAB70
        // exactly thirteen times with the same two loose u8 reads after block 0.
        //
        // Retail leaves blocks it has nothing to say about entirely zero - the rated Blitz capture sent 11
        // of 13 populated, a levelling character only 4 - so an unimplemented activity is written empty
        // rather than omitted, and that is a wire-legal state rather than a stub.
        class LfgPartyInfo final : public ServerPacket
        {
        public:
            explicit LfgPartyInfo() : ServerPacket(SMSG_LFG_PARTY_INFO) { }

            WorldPacket const* Write() override;

            std::vector<LFGBlackList> Player;
        };

        class LFGUpdateStatus final : public ServerPacket
        {
        public:
            explicit LFGUpdateStatus() : ServerPacket(SMSG_LFG_UPDATE_STATUS) { }

            WorldPacket const* Write() override;

            RideTicket Ticket;
            uint8 SubType = 0;
            uint32 Reason = 0;
            std::vector<uint32> Slots;
            uint8 RequestedRoles = 0;
            std::vector<ObjectGuid> SuspendedPlayers;
            uint32 QueueMapID = 0;
            bool NotifyUI = false;
            bool IsParty = false;
            bool Joined = false;
            bool LfgJoined = false;
            bool Queued = false;
            bool Brawl = false;
        };

        class RoleChosen final : public ServerPacket
        {
        public:
            explicit RoleChosen() : ServerPacket(SMSG_ROLE_CHOSEN, 16 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            uint8 RoleMask = 0;
            bool Accepted = false;
        };

        struct LFGRoleCheckUpdateMember
        {
            LFGRoleCheckUpdateMember() = default;
            LFGRoleCheckUpdateMember(ObjectGuid guid, uint8 rolesDesired, uint8 level, bool roleCheckComplete)
                : Guid(guid), RolesDesired(rolesDesired), Level(level), RoleCheckComplete(roleCheckComplete) { }

            ObjectGuid Guid;
            uint8 RolesDesired = 0;
            uint8 Level = 0;
            bool RoleCheckComplete = false;
        };

        class LFGRoleCheckUpdate final : public ServerPacket
        {
        public:
            explicit LFGRoleCheckUpdate() : ServerPacket(SMSG_LFG_ROLE_CHECK_UPDATE) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;
            uint8 RoleCheckStatus = 0;
            std::vector<uint32> JoinSlots;
            std::vector<uint64> BgQueueIDs;
            int32 GroupFinderActivityID = 0;
            std::vector<LFGRoleCheckUpdateMember> Members;
            bool IsBeginning = false;
            bool IsRequeue = false;
        };

        // One member row of SMSG_LFG_READY_CHECK_UPDATE. Element type ClientLFGReadyCheckUpdateMember
        // (type-name literal @ RVA 0x3C22810, in-memory stride 24); there is no separate reader - the
        // dispatcher case 0x755902 decodes it inline. Wire: PackedGuid + ONE byte (bit7 IsReady, bit6 Unk1).
        // Note this is NOT the same shape as ClientLFGRoleCheckUpdateMember, which carries one byte more
        // payload per member (guid, u8 RolesDesired, u8 Level, bit).
        struct LFGReadyCheckUpdateMember
        {
            LFGReadyCheckUpdateMember() = default;
            LFGReadyCheckUpdateMember(ObjectGuid guid, bool isReady) : Guid(guid), IsReady(isReady) { }

            ObjectGuid Guid;
            bool IsReady = false;
            // UNVERIFIED: the second bit of every member (client struct offset +17) has no reader anywhere in
            // the decompiled image - its meaning is unknown. Send 0 until a capture shows otherwise.
            bool Unk1 = false;
        };

        // SMSG_LFG_READY_CHECK_UPDATE (0x5A0006) - Client 12.1.0.69382, dispatcher case @ RVA 0x755902,
        // consumer @ RVA 0x24C2920 (hook slot 0x462ED10).
        // Wire:  u8 PartyIndex ; u8 ReadyCheckStatus ; u32 BgQueueIDs.Count ; u32 Members.Count ;
        //        BgQueueIDs[] (u64 each) ; Members[] ; one byte with bit7 = IsRequeue.
        // Minimum 11 bytes (both counts zero). Each single-bit field occupies a whole byte - the client reads
        // four separate Read<uint8> and masks them; there is no shared bit block and no FlushBits in the case.
        //
        // This is the LFG READINESS check - a fourth, disjoint system next to the LFG proposal
        // (SMSG_LFG_PROPOSAL_UPDATE / LFGDungeonReadyPopup), the LFG role check
        // (SMSG_LFG_ROLE_CHECK_UPDATE / LFDRoleCheckPopup) and the party ready check
        // (WorldPackets::Party::ReadyCheckStarted / ReadyCheckFrame). Its dialog is LFGReadyCheckPopup.
        //
        // D3 - the consumer's comparison chain on ReadyCheckStatus (GameError via 0x209AD90):
        //      2 -> ERR_LFG_READY_CHECK_INITIATED      (829) and, per ready member, LFG_READY_CHECK_PLAYER_IS_READY
        //      3 -> ERR_LFG_READY_CHECK_FAILED_TIMEOUT (804)
        //      4 -> ERR_LFG_READY_CHECK_ABORTED        (844)
        //      5 -> ERR_LFG_READY_CHECK_FAILED         (803)
        // Always: LFG_READY_CHECK_UPDATE. If Status == 2 AND the receiver's own member entry has IsReady == 0:
        // LFG_READY_CHECK_SHOW(IsRequeue); otherwise LFG_READY_CHECK_HIDE. Any status outside 2..5 therefore
        // closes the dialog without an error message - that is the "finished" case.
        //
        // BgQueueIDs is what turns this into the battleground variant: GetLFGReadyCheckUpdate (0x24CF010)
        // returns (Status == 2, Count != 0) and the UI reads that as (readyCheckInProgress,
        // readyCheckIsBattleground); GetLFGReadyCheckUpdateBattlegroundInfo (0x24CF100) takes BgQueueIDs[0]
        // and uses (uint16)queueId as a BattlemasterList key. With an empty vector the client shows UNKNOWN as
        // the queue name and creates no queue-status entry (LFGReadyCheck.lua:13-17, QueueStatusFrame.lua:671-680).
        //
        // UNVERIFIED: the field NAMES PartyIndex / ReadyCheckStatus / BgQueueIDs. They are taken from the
        // sister opcode SMSG_LFG_ROLE_CHECK_UPDATE (0x5A0005, reader 0x754550), whose read sequence
        // u8,u8,u32 cnt,u32 cnt,u32,u32 cnt,... maps one-to-one onto LFGRoleCheckUpdate below. IsRequeue is
        // NOT a guess - the generated doc declares LFG_READY_CHECK_SHOW's payload as { Name="isRequeue" }.
        class LFGReadyCheckUpdate final : public ServerPacket
        {
        public:
            explicit LFGReadyCheckUpdate() : ServerPacket(SMSG_LFG_READY_CHECK_UPDATE, 1 + 1 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;
            uint8 ReadyCheckStatus = 0;
            std::vector<uint64> BgQueueIDs;
            std::vector<LFGReadyCheckUpdateMember> Members;
            bool IsRequeue = false;
        };

        // SMSG_LFG_READY_CHECK_RESULT (0x5A001E) - Client 12.1.0.69382, dispatcher case @ RVA 0x7567FA,
        // consumer @ RVA 0x24C31D0. Wire: PackedGuid Player ; one byte with bit7 = Ready. 3..19 bytes.
        // The field names are Blizzard's own: the consumer opens with the debug format string
        //   "LFGMessage: READY_CHECK_RESULT - %s, Ready: %s"     (string @ RVA 0x3DAEE08)
        // from the ...\Ui\LFGInfo.cpp string block. The first %s is the formatted GUID, not the name.
        // The consumer resolves the player name from its character-name cache first: if that lookup
        // fails, neither the event nor the GameError fires.
        // D3 - there is NO Lua event named LFG_READY_CHECK_RESULT. The consumer branches:
        //   Ready == true  -> LFG_READY_CHECK_PLAYER_IS_READY(name)
        //   Ready == false -> GameError 831 ERR_LFG_PLAYER_DECLINED_READY_CHECK *and* LFG_READY_CHECK_DECLINED(name)
        // Sending only the true flank leaves both the decline text and the decline event out.
        class LFGReadyCheckResult final : public ServerPacket
        {
        public:
            explicit LFGReadyCheckResult() : ServerPacket(SMSG_LFG_READY_CHECK_RESULT, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            bool Ready = false;
        };

        // SMSG_LFG_INSTANCE_SHUTDOWN_COUNTDOWN (0x5A0009) - Client 12.1.0.69382, dispatcher case @ RVA 0x755BAD,
        // consumer @ RVA 0x24C18C0. Wire: RideTicket ; uint32 TimeLeft. 23..39 bytes.
        // The consumer formats TimeLeft with INT_GENERAL_DURATION (duration formatter 0x36F49B0) into the
        // GlobalString INSTANCE_SHUTDOWN_MESSAGE and prints it through the system-chat sink 0x20A7880
        // (verified: 0x24C18C0 calls the duration formatter 0x36F49B0 at 0x24C1942 and 0x20A7880 at
        // 0x24C196E; 0x20A7880 is a function start with 190 code xrefs, while the 0x2007880 this comment
        // used to name is not a function start at all - it falls inside sub_2005D80 and has no xref).
        // There is NO Lua event, no CVar and no UI state - the time formatting is what proves TimeLeft is a
        // duration in SECONDS. Do not confuse this with INSTANCE_BOOT_START/_STOP (kick from the LFG instance
        // group) or INSTANCE_ABANDON_VOTE_FINISHED (vote to abandon) - different systems, similar names.
        class LFGInstanceShutdownCountdown final : public ServerPacket
        {
        public:
            explicit LFGInstanceShutdownCountdown() : ServerPacket(SMSG_LFG_INSTANCE_SHUTDOWN_COUNTDOWN, 16 + 4 + 4 + 8 + 1 + 4) { }

            WorldPacket const* Write() override;

            RideTicket Ticket;
            uint32 TimeLeft = 0;            // seconds
        };

        // SMSG_LFG_SUSPEND_LOREWALKING (0x5A0021) - Client 12.1.0.69382, dispatcher case @ RVA 0x756942,
        // consumer @ RVA 0x24C50A0. Wire: exactly ONE byte, bit7 = Suspend.
        // The consumer is four instructions:
        //     cmp byte ptr [rcx+0x20], 0 / jne ret / mov ecx, 0x340 (=832) / jmp 0x209AD90 (GameError)
        // so Suspend == 0 raises GameError 832 ERR_LFG_LOREWALKING and Suspend == 1 is silent. No Lua event,
        // no UI state, no global.
        // The semantics are the INVERSE of what the name suggests: the bit does not ask the client to pause
        // Lorewalking, it reports "Lorewalking is suspended, you may queue". A false is the REFUSAL and is
        // what carries the error text.
        // Lorewalking itself is a campaign-bound scenario mode that hides quests and blocks LFG queueing
        // (TextureKit "lorewalking-scenario", CVar lorewalkingCampaignID @ RVA 0x3C06260,
        // LOREWALKING_QUESTS_HIDDEN_HELP_TIP). It is NOT the "old raids in their original form" mode.
        class LFGSuspendLorewalking final : public ServerPacket
        {
        public:
            explicit LFGSuspendLorewalking() : ServerPacket(SMSG_LFG_SUSPEND_LOREWALKING, 1) { }

            WorldPacket const* Write() override;

            bool Suspend = false;
        };

        // SMSG_SET_DF_FAST_LAUNCH_RESULT (0x5A0012) - Client 12.1.0.69382, dispatcher case @ RVA 0x75617E,
        // consumer @ RVA 0x24C4E70. Wire: exactly ONE byte, bit7 = LfgFastLaunch. The field name is literal:
        // the consumer stores the bit into the global 0x4C9B57B and prints "lfgFastLaunch set to %s"
        // (format string @ RVA 0x3DAEDF0) to the console.
        // This is an UNSOLICITED server push. CMSG_SET_DF_FAST_LAUNCH does not exist - not in TrinityCore and
        // not in the client: a full catalogue of the client's message classes (2684 classes off the shared
        // vtable guard slot 0x7FF781687A40, containing all 1023 TC CMSG) has no unknown 1-bit message in
        // families 0x43 or 0x3D, and the only xref of the format string is inside this receive handler.
        // So the half pair is correct and is not a violation of the "never implement pairs alone" rule.
        // Effect: the only other reader of the global is the Lua getter binding 0x24D4ED0 (the 22-field LFG
        // activity table), in the chain `(flags & 0x1000) == 0 || lfgFastLaunch || ...`. The flag therefore
        // BYPASSES AN UNLOCK GATE ON LFG ENTRIES - it is not a "fast start of the dungeon finder".
        class SetDFFastLaunchResult final : public ServerPacket
        {
        public:
            explicit SetDFFastLaunchResult() : ServerPacket(SMSG_SET_DF_FAST_LAUNCH_RESULT, 1) { }

            WorldPacket const* Write() override;

            bool LfgFastLaunch = false;
        };

        class LFGJoinResult final : public ServerPacket
        {
        public:
            explicit LFGJoinResult() : ServerPacket(SMSG_LFG_JOIN_RESULT) { }

            WorldPacket const* Write() override;

            RideTicket Ticket;
            int32 Result = 0;
            uint8 ResultDetail = 0;
            std::vector<LFGBlackList> BlackList;
            std::vector<std::string_view> BlackListNames;
        };

        class LFGQueueStatus final : public ServerPacket
        {
        public:
            explicit LFGQueueStatus() : ServerPacket(SMSG_LFG_QUEUE_STATUS, 16 + 4 + 4 + 4 + 4 + 4 + 4 + 4 * 3 + 3 + 4) { }

            WorldPacket const* Write() override;

            RideTicket Ticket;
            uint32 Slot = 0;
            uint32 AvgWaitTimeMe = 0;
            uint32 AvgWaitTime = 0;
            uint32 AvgWaitTimeByRole[3] = { };
            uint8 LastNeeded[3] = { };
            uint32 QueuedTime = 0;
        };

        struct LFGPlayerRewards
        {
            LFGPlayerRewards() = default;
            LFGPlayerRewards(int32 id, uint32 quantity, int32 bonusQuantity, bool isCurrency)
                : Quantity(quantity), BonusQuantity(bonusQuantity)
            {
                if (!isCurrency)
                {
                    RewardItem.emplace();
                    RewardItem->ItemID = id;
                }
                else
                {
                    RewardCurrency = id;
                }
            }

            Optional<Item::ItemInstance> RewardItem;
            Optional<int32> RewardCurrency;
            uint32 Quantity = 0;
            int32 BonusQuantity = 0;
        };

        class LFGPlayerReward final : public ServerPacket
        {
        public:
            explicit LFGPlayerReward() : ServerPacket(SMSG_LFG_PLAYER_REWARD) { }

            WorldPacket const* Write() override;

            uint32 QueuedSlot = 0;
            uint32 ActualSlot = 0;
            int32 RewardMoney = 0;
            int32 AddedXP = 0;
            std::vector<LFGPlayerRewards> Rewards;
        };

        struct LfgBootInfo
        {
            bool VoteInProgress = false;
            bool VotePassed = false;
            bool MyVoteCompleted = false;
            bool MyVote = false;
            ObjectGuid Target;
            uint32 TotalVotes = 0;
            uint32 BootVotes = 0;
            int32 TimeLeft = 0;
            uint32 VotesNeeded = 0;
            std::string Reason;
        };

        class LfgBootPlayer final : public ServerPacket
        {
        public:
            explicit LfgBootPlayer() : ServerPacket(SMSG_LFG_BOOT_PLAYER) { }

            WorldPacket const* Write() override;

            LfgBootInfo Info;
        };

        struct LFGProposalUpdatePlayer
        {
            uint8 Roles = 0;
            bool Me = false;
            bool SameParty = false;
            bool MyParty = false;
            bool Responded = false;
            bool Accepted = false;
        };

        class LFGProposalUpdate final : public ServerPacket
        {
        public:
            explicit LFGProposalUpdate() : ServerPacket(SMSG_LFG_PROPOSAL_UPDATE) { }

            WorldPacket const* Write() override;

            RideTicket Ticket;
            uint64 InstanceID = 0;
            uint32 ProposalID = 0;
            uint32 Slot = 0;
            int8 State = 0;
            uint32 CompletedMask = 0;
            uint32 EncounterMask = 0;
            uint8 PromisedShortageRolePriority = 0;
            bool ValidCompletedMask = false;
            bool ProposalSilent = false;
            bool FailedByMyParty = false;
            std::vector<LFGProposalUpdatePlayer> Players;
        };

        class LFGDisabled final : public ServerPacket
        {
        public:
            explicit LFGDisabled() : ServerPacket(SMSG_LFG_DISABLED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class LFGOfferContinue final : public ServerPacket
        {
        public:
            explicit LFGOfferContinue(uint32 slot) : ServerPacket(SMSG_LFG_OFFER_CONTINUE, 4), Slot(slot) { }

            WorldPacket const* Write() override;

            uint32 Slot = 0;
        };

        class LFGTeleportDenied final : public ServerPacket
        {
        public:
            explicit LFGTeleportDenied(lfg::LfgTeleportResult reason) : ServerPacket(SMSG_LFG_TELEPORT_DENIED, 1), Reason(reason) { }

            WorldPacket const* Write() override;

            lfg::LfgTeleportResult Reason;
        };

        // SMSG_OPEN_LFG_DUNGEON_FINDER (0x5A0015): makes the client open its native dungeon/group finder panel
        // preselected to the given LFGDungeons.db2 id. Used to surface the BfA warfront war-table assault entry,
        // whose "Join Battle" button then sends CMSG_DF_JOIN back with slot = dungeonID | (TypeID << 24).
        //
        // !! INFERRED (needs sniff validation): no serializer and no 0x811C9DC5 reflection descriptor for this
        // opcode was recovered offline; a single uint32 dungeon id is the asserted body. Senders must gate this
        // behind a config opt-in (see WarfrontMgr::IsNativeUiEnabled / Warfront.NativeUI.Enable).
        class OpenLfgDungeonFinder final : public ServerPacket
        {
        public:
            explicit OpenLfgDungeonFinder(uint32 dungeonId = 0) : ServerPacket(SMSG_OPEN_LFG_DUNGEON_FINDER, 4), DungeonID(dungeonId) { }

            WorldPacket const* Write() override;

            uint32 DungeonID;   // INFERRED (needs sniff validation)
        };

        // Dispatcher case 5636127 (0x56001F) inside the 0x56 group dispatcher @ RVA 0x739420 is a single
        // RideTicket read and nothing else; the registered handler @ RVA 0x2301A90 copies msg+0x20..+0x48
        // (guid 16 + id 4 + type 4 + time 8 + bit) into a global and fires SHOW_LFG_EXPAND_SEARCH_PROMPT,
        // which LFGInfoDocumentation.lua declares with an empty payload. So: ticket only.
        class LFGExpandSearchPrompt final : public ServerPacket
        {
        public:
            explicit LFGExpandSearchPrompt() : ServerPacket(SMSG_LFG_EXPAND_SEARCH_PROMPT, 16 + 4 + 4 + 8 + 1) { }

            WorldPacket const* Write() override;

            RideTicket Ticket;
        };

        // Dispatcher case 5636116 (0x560014) only takes a reference to the remaining bytes - the parse is
        // deferred to the handler. That handler (registration site RVA 0x1EE580E stores RVA 0x2301A40 into
        // the slot at RVA 0x4404890) reads exactly three dwords off the tail:
        //   eax = [payload+0] -> arg0 ; eax = [payload+4] -> arg1 ; eax = [payload+8] -> arg2
        // and fires a three-number Lua event - LFG_INVALID_ERROR_MESSAGE(reason, subReason1, subReason2).
        // Reason is Enum.LFGSlotInvalidReason (see lfg::LfgSlotInvalidReason).
        class LFGSlotInvalid final : public ServerPacket
        {
        public:
            explicit LFGSlotInvalid() : ServerPacket(SMSG_LFG_SLOT_INVALID, 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 Reason = 0;
            int32 SubReason1 = 0;
            int32 SubReason2 = 0;
        };

        // SMSG_OPEN_LFG_DUNGEON_FINDER (0x560015): makes the client open its native dungeon/group finder panel
        // preselected to the given LFGDungeons.db2 id. Used to surface the BfA warfront war-table assault entry,
        // whose "Join Battle" button then sends CMSG_DF_JOIN back with slot = dungeonID | (TypeID << 24).
        //
        // !! INFERRED (needs sniff validation): no serializer and no 0x811C9DC5 reflection descriptor for this
        // opcode was recovered offline; a single uint32 dungeon id is the asserted body. Senders must gate this
        // behind a config opt-in (see WarfrontMgr::IsNativeUiEnabled / Warfront.NativeUI.Enable).
        // Dispatcher case 5636127 (0x56001F) inside the 0x56 group dispatcher @ RVA 0x739420 is a single
        // RideTicket read and nothing else; the registered handler @ RVA 0x2301A90 copies msg+0x20..+0x48
        // (guid 16 + id 4 + type 4 + time 8 + bit) into a global and fires SHOW_LFG_EXPAND_SEARCH_PROMPT,
        // which LFGInfoDocumentation.lua declares with an empty payload. So: ticket only.
        // Dispatcher case 5636116 (0x560014) only takes a reference to the remaining bytes - the parse is
        // deferred to the handler. That handler (registration site RVA 0x1EE580E stores RVA 0x2301A40 into
        // the slot at RVA 0x4404890) reads exactly three dwords off the tail:
        //   eax = [payload+0] -> arg0 ; eax = [payload+4] -> arg1 ; eax = [payload+8] -> arg2
        // and fires a three-number Lua event - LFG_INVALID_ERROR_MESSAGE(reason, subReason1, subReason2).
        // Reason is Enum.LFGSlotInvalidReason (see lfg::LfgSlotInvalidReason).
    }
}

#endif // TRINITYCORE_LFG_PACKETS_H

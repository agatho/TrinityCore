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

        class DFGetJoinStatus final : public ClientPacket
        {
        public:
            explicit DFGetJoinStatus(WorldPacket&& packet) : ClientPacket(CMSG_DF_GET_JOIN_STATUS, std::move(packet)) { }

            void Read() override { }
        };

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
    }
}

#endif // TRINITYCORE_LFG_PACKETS_H

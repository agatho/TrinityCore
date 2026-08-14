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

#ifndef TRINITYCORE_BATTLEGROUND_PACKETS_H
#define TRINITYCORE_BATTLEGROUND_PACKETS_H

#include "Packet.h"
#include "LFGPacketsCommon.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "PacketUtilities.h"
#include "Position.h"
#include <array>

namespace WorldPackets
{
    namespace Battleground
    {
        class SeasonInfo final : public ServerPacket
        {
        public:
            explicit SeasonInfo() : ServerPacket(SMSG_SEASON_INFO, 4 + 4 + 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            int32 MythicPlusDisplaySeasonID = 0;
            int32 MythicPlusMilestoneSeasonID = 0;
            int32 PreviousArenaSeason = 0;
            int32 CurrentArenaSeason = 0;
            int32 PvpSeasonID = 0;
            int32 ConquestWeeklyProgressCurrencyID = 0;
            int32 Unknown1027_1 = 0;
            bool WeeklyRewardChestsEnabled = false;
            bool CurrentArenaSeasonUsesTeams = false;
            bool PreviousArenaSeasonUsesTeams = false;
        };

        class AreaSpiritHealerQuery final : public ClientPacket
        {
        public:
            explicit AreaSpiritHealerQuery(WorldPacket&& packet) : ClientPacket(CMSG_AREA_SPIRIT_HEALER_QUERY, std::move(packet)) { }

            void Read() override;

            ObjectGuid HealerGuid;
        };

        class AreaSpiritHealerQueue final : public ClientPacket
        {
        public:
            explicit AreaSpiritHealerQueue(WorldPacket&& packet) : ClientPacket(CMSG_AREA_SPIRIT_HEALER_QUEUE, std::move(packet)) { }

            void Read() override;

            ObjectGuid HealerGuid;
        };

        class AreaSpiritHealerTime final : public ServerPacket
        {
        public:
            explicit AreaSpiritHealerTime() : ServerPacket(SMSG_AREA_SPIRIT_HEALER_TIME, 14 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid HealerGuid;
            int32 TimeLeft = 0;
        };

        class HearthAndResurrect final : public ClientPacket
        {
        public:
            explicit HearthAndResurrect(WorldPacket&& packet) : ClientPacket(CMSG_HEARTH_AND_RESURRECT, std::move(packet)) { }

            void Read() override { }
        };

        class PVPLogDataRequest final : public ClientPacket
        {
        public:
            explicit PVPLogDataRequest(WorldPacket&& packet) : ClientPacket(CMSG_PVP_LOG_DATA, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_SURRENDER_ARENA -- the client's arena "Surrender"/forfeit button. Opcode-only signal (empty payload,
        // confirmed by client RE: no wire fields). The sender's team concedes the active arena match.
        class SurrenderArena final : public ClientPacket
        {
        public:
            explicit SurrenderArena(WorldPacket&& packet) : ClientPacket(CMSG_SURRENDER_ARENA, std::move(packet)) { }

            void Read() override { }
        };

        struct PVPMatchStatistics
        {
            struct RatingData
            {
                RatingData() { } // work around clang bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101227

                int32 Prematch[2] = { };
                int32 Postmatch[2] = { };
                int32 PrematchMMR[2] = { };
            };

            struct HonorData
            {
                HonorData() { } // work around clang bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101227

                uint32 HonorKills = 0;
                uint32 Deaths = 0;
                uint32 ContributionPoints = 0;
            };

            struct PVPMatchPlayerPVPStat
            {
                PVPMatchPlayerPVPStat() : PvpStatID(0), PvpStatValue(0) { }
                PVPMatchPlayerPVPStat(int32 pvpStatID, int32 pvpStatValue) : PvpStatID(pvpStatID), PvpStatValue(pvpStatValue) { }

                int32 PvpStatID;
                int32 PvpStatValue;
            };

            struct PVPMatchPlayerStatistics
            {
                ObjectGuid PlayerGUID;
                uint32 Kills = 0;
                int32 Faction = 0;
                bool IsInWorld = false;
                Optional<HonorData> Honor;
                uint32 DamageDone = 0;
                uint32 HealingDone = 0;
                Optional<uint32> PreMatchRating;
                Optional<int32> RatingChange;
                Optional<uint32> PreMatchMMR;
                Optional<int32> MmrChange;
                Optional<uint32> PostMatchMMR;
                std::vector<PVPMatchPlayerPVPStat> Stats;
                int32 PrimaryTalentTree = 0;
                int8 Sex = 0;
                int8 Race = 0;
                int8 Class = 0;
                int32 CreatureID = 0;
                int32 HonorLevel = 0;
                int32 Role = 0;
            };

            std::vector<PVPMatchPlayerStatistics> Statistics;
            Optional<RatingData> Ratings;
            std::array<int8, 2> PlayerCount = { };
        };

        class PVPMatchStatisticsMessage final : public ServerPacket
        {
        public:
            explicit PVPMatchStatisticsMessage() : ServerPacket(SMSG_PVP_MATCH_STATISTICS, 0) { }

            WorldPacket const* Write() override;

            PVPMatchStatistics Data;
        };

        struct BattlefieldStatusHeader
        {
            WorldPackets::LFG::RideTicket Ticket;
            std::vector<uint64> QueueID;
            uint8 RangeMin = 0;
            uint8 RangeMax = 0;
            uint8 TeamSize = 0;
            uint32 InstanceID = 0;
            bool RegisteredMatch = false;
            bool TournamentRules = false;
        };

        class BattlefieldStatusNone final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusNone() : ServerPacket(SMSG_BATTLEFIELD_STATUS_NONE, 16 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            WorldPackets::LFG::RideTicket Ticket;
        };

        class BattlefieldStatusNeedConfirmation final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusNeedConfirmation() : ServerPacket(SMSG_BATTLEFIELD_STATUS_NEED_CONFIRMATION, 4 + 4 + sizeof(BattlefieldStatusHeader) + 1) { }

            WorldPacket const* Write() override;

            uint32 Timeout = 0;
            uint32 Mapid = 0;
            BattlefieldStatusHeader Hdr;
            uint8 Role = 0;
        };

        class BattlefieldStatusActive final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusActive() : ServerPacket(SMSG_BATTLEFIELD_STATUS_ACTIVE, sizeof(BattlefieldStatusHeader) + 4 + 1 + 1 + 4 + 4) { }

            WorldPacket const* Write() override;

            BattlefieldStatusHeader Hdr;
            uint32 ShutdownTimer = 0;
            int8 ArenaFaction = 0;
            bool LeftEarly = false;
            bool Brawl = false;
            uint32 StartTimer = 0;
            uint32 Mapid = 0;
        };

        class BattlefieldStatusQueued final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusQueued() : ServerPacket(SMSG_BATTLEFIELD_STATUS_QUEUED, 4 + sizeof(BattlefieldStatusHeader) + 1 + 1 + 1 + 4) { }

            WorldPacket const* Write() override;

            uint32 AverageWaitTime = 0;
            BattlefieldStatusHeader Hdr;
            bool AsGroup = false;
            bool SuspendedQueue = false;
            bool EligibleForMatchmaking = false;
            uint32 WaitTime = 0;
            int32 SpecSelected = 0;
        };

        class BattlefieldStatusFailed final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusFailed() : ServerPacket(SMSG_BATTLEFIELD_STATUS_FAILED, 8 + 16 + 4 + 16 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint64 QueueID = 0;
            ObjectGuid ClientID;
            int32 Reason = 0;
            WorldPackets::LFG::RideTicket Ticket;
        };

        // SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS (0x48000D) and
        // SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED (0x48000E) are DELIBERATELY not implemented.
        //
        // Their wire form is fully known - it was decoded byte for byte from the 18 + 2 occurrences in
        // C:\sniff\rated BG 12.0.7.pkt and confirmed against the client readers - and it is recorded here so
        // the work is not repeated. (Note that "rbg rated BG 12.0.7.pkt" is a byte-identical copy of that
        // file, so this is one capture and not two.)
        //
        //   0x48000D  BattlefieldStatusHeader (the struct above, read by sub_7FF7290FAAE0 - identical field
        //             order to operator<<(ByteBuffer&, BattlefieldStatusHeader const&))
        //             uint32 MapID, uint32 TimeoutMs (30000 in every capture). The first of the two is only
        //             INFERRED to be a map: it holds 2656, 2107 and then 2245 across the three proposal
        //             runs, and the match that finally started ran on map 2245 - plus the already-working
        //             siblings above write Mapid immediately before Timeout in exactly this position.
        //             uint8 SlotsPerSide[2] and uint8 AwaitedPerSide[2], written index-interleaved as
        //                 SlotsPerSide[0], AwaitedPerSide[0], SlotsPerSide[1], AwaitedPerSide[1],
        //             then the 3x3 role block below.
        //   0x48000E  BattlefieldStatusHeader, then the 3x3 role block below. Nothing else.
        //
        // The role block is read by sub_7FF7290FAD90 into three uint8[3] arrays A/B/C indexed by role, and
        // the wire order is index-interleaved: A[0],B[0],C[0], A[1],B[1],C[1], A[2],B[2],C[2], then one bit
        // plus a flush. The role order is TANK, HEALER, DAMAGER - read straight out of the client's own
        // name table at 0x7FF72C3C5DE0. The consumers (sub_7FF72AAB93E0 for 0x48000D, sub_7FF72AAB97D0 for
        // 0x48000E) build a vector of a struct the client itself names PvpRoleQueueInfo, whose fields are
        // { roleName, A[i] + B[i] + C[i], B[i], C[i] } - so the three counts SUM to that role's requirement.
        // The captured rated Blitz match reads as 0 tanks / 4 healers / 12 damagers = 16 players = 8v8, and
        // every one of the 18 samples balances: sum(A) equals AwaitedPerSide[0] + AwaitedPerSide[1] and
        // sum(B) equals the players already secured.
        //
        // Re-verified 2026-08-14 by decoding all 20 bodies again from scratch: the header is 46 bytes here
        // (9-byte packed guid + 4 + 4 + 8 + 1 flush for the RideTicket, then count 1, RangeMin 0, RangeMax 90,
        // TeamSize 0, InstanceID 0, one 8-byte QueueID 0x1F1000000019044D, then the two bits + flush), Ticket
        // Type reads 1 = RideType::Battlegrounds, and Ticket.Time decodes as a sane unix timestamp only as an
        // int64. 17 of the 18 0x48000D samples satisfy BOTH invariants above exactly; the 18th is the terminal
        // message that zeroes the whole role block once the match is formed, so "secured" no longer applies to
        // it. Nothing in either body is unaccounted for.
        //
        // What is missing is not the layout, it is the server state - but the earlier claim that EVERY number
        // would have to be fabricated was too strong, and is corrected here so the next reader weighs the real
        // obstacle rather than a wrong one. Three of the numbers are in fact derivable for the Blitz solo
        // queue: the per-role targets fall straight out of MaxPlayersPerTeam and
        // CONFIG_BATTLEGROUND_BLITZ_HEALERS_PER_TEAM (8 per side and 2 healers per side reproduce the captured
        // 0 tanks / 4 healers / 12 damagers exactly), B follows from the healer/other partition
        // CheckSoloQueueMatch already builds, and SlotsPerSide is just MaxPlayersPerTeam.
        //
        // What genuinely does not exist is the rest:
        //   - There is no group-proposal phase for battlegrounds anywhere in this core. LFG has LfgProposal
        //     with an id, a state machine, a cancel deadline and a per-player accept tri-state; the BG invite
        //     flow has only ginfo->IsInvitedToBGInstanceGUID plus a RemoveInviteTime, no acceptance ledger,
        //     and a decline removes ONLY the decliner while everyone else keeps their invite. So 0x48000E -
        //     "they did not all accept" - has no event that could ever fire it, and 0x48000D would be
        //     asserting a 30 second proposal that is not running.
        //   - MapID is unknowable at that point: no instance is chosen until CreateNewBattleground, which
        //     happens after the queue has already decided to pop.
        //   - The per-side split of AwaitedPerSide has no source. Only the SUM is real; sides are not assigned
        //     until the selection pools are filled.
        //   - TANK and DAMAGER cannot be told apart. lfg::PLAYER_ROLE_HEALER is the only role bit examined
        //     anywhere in Battlegrounds/; a tank-flagged queuer lands in "others" alongside the damage dealers.
        // So they stay STATUS_UNHANDLED. Their CONNECTION_TYPE_REALM declaration is however confirmed correct
        // rather than merely a safe default: retail sends both on connection index 0 in the capture, unlike
        // SMSG_BATTLEFIELD_STATUS_QUEUED which it sends on index 1.

        class BattlemasterJoin final : public ClientPacket
        {
        public:
            explicit BattlemasterJoin(WorldPacket&& packet) : ClientPacket(CMSG_BATTLEMASTER_JOIN, std::move(packet)) { }

            void Read() override;

            Array<uint64, 1> QueueIDs;
            uint8 Roles = 0;
            std::array<int32, 2> BlacklistMap = { };
        };

        class BattlemasterJoinArena final : public ClientPacket
        {
        public:
            explicit BattlemasterJoinArena(WorldPacket&& packet) : ClientPacket(CMSG_BATTLEMASTER_JOIN_ARENA, std::move(packet)) { }

            void Read() override;

            uint8 TeamSizeIndex = 0;
            uint8 Roles = 0;
        };

        // CMSG_BATTLEMASTER_JOIN_RATED_BG_BLITZ (0x3B00BE), body = exactly 1 byte.
        //
        // The client serializer at VA 0x7FF729153060 writes a single uint8 from obj+0x20 and returns;
        // C_PvP.JoinRatedBGBlitz (RVA 0x1278130) fills that byte with (selectedPvpRoles & ChrClasses.RolesMask).
        // The bits are the same LFG role flags the rest of the core already uses (lfg::PLAYER_ROLE_*):
        // 0x01 leader, 0x02 tank, 0x04 healer, 0x08 damage - confirmed from SetPVPRoles (VA 0x7FF72AACBE70),
        // which builds the mask as tank?2 | healer?4 | dps?8, and from the one live capture of this opcode
        // (C:\sniff\rated BG 12.0.7.pkt record 10612, body = 04 = HEALER; the server's reply 366 ms later
        // carried SpecSelected 257 = Holy Priest, a healer spec).
        //
        // NOTE the field order differs from BattlemasterJoinArena above, which reads TeamSizeIndex THEN Roles.
        // Here Roles comes first and there is no second byte at all.
        class BattlemasterJoinRatedBGBlitz final : public ClientPacket
        {
        public:
            explicit BattlemasterJoinRatedBGBlitz(WorldPacket&& packet)
                : ClientPacket(CMSG_BATTLEMASTER_JOIN_RATED_BG_BLITZ, std::move(packet)) { }

            void Read() override;

            uint8 Roles = 0;
        };

        // CMSG_BATTLEMASTER_JOIN_SKIRMISH (0x3B00BF), body = 3 bytes.
        //
        // Serializer VA 0x7FF729153120 writes obj+0x20 then obj+0x21, then one bit from obj+0x22 and flushes.
        // Producers are C_PvP.JoinSkirmish(id) (RVA 0x2024F30) and C_PvP.RequeueSkirmish() (RVA 0x2025000).
        //
        // WARNING: the field order is the REVERSE of BattlemasterJoinArena. There the wire is
        // TeamSizeIndex then Roles; here Roles comes FIRST. JoinSkirmish stores the role mask with
        // `mov byte [rsp+0x40], al` and the bracket with `mov word [rsp+0x41], 4`. Copy-pasting the arena
        // reader would silently swap the two bytes.
        //
        // Bracket is the client's own API parameter name (C_PvP.GetSkirmishInfo(pvpBracket)). Its enum
        // identity is UNKNOWN and it is deliberately NOT mapped onto BattlegroundBracketId or
        // PVPBracketTypes: the client's valid bracket space from GetPersonalRatedInfo is {0,1,2,3,6,8} and
        // does not contain 4. JoinSkirmish only ever sends 4 (and hard-rejects anything else with the Lua
        // error "Invalid bracket id."); RequeueSkirmish sends 255 together with the Requeue bit set.
        class BattlemasterJoinSkirmish final : public ClientPacket
        {
        public:
            explicit BattlemasterJoinSkirmish(WorldPacket&& packet)
                : ClientPacket(CMSG_BATTLEMASTER_JOIN_SKIRMISH, std::move(packet)) { }

            void Read() override;

            uint8 Roles = 0;
            uint8 Bracket = 0;
            bool Requeue = false;
        };

        // CMSG_JOIN_RATED_BATTLEGROUND (0x3A0025), body = exactly 1 byte: uint8 Roles.
        //
        // Same shape as the Blitz join despite the different opcode group. Client serializer
        // VA 0x7FF7291455E0 writes one uint8 from obj+0x20; producer is the Lua binding
        // JoinRatedBattlefield (RVA 0x2024540), called with no arguments from Blizzard_PVPUI.lua.
        // The byte is the LFG role mask (0x01 leader, 0x02 tank, 0x04 healer, 0x08 damage).
        class JoinRatedBattleground final : public ClientPacket
        {
        public:
            explicit JoinRatedBattleground(WorldPacket&& packet)
                : ClientPacket(CMSG_JOIN_RATED_BATTLEGROUND, std::move(packet)) { }

            void Read() override;

            uint8 Roles = 0;
        };

        // CMSG_BATTLEMASTER_JOIN_BRAWL (0x3B00C2), body = 2 bytes.
        //
        // Client serializer VA 0x7FF7291531A0: after the opcode header it writes one uint8 from obj+0x20,
        // then a single bit from obj+0x21 and flushes. Derived by the same reading of the same three
        // helpers (0x7FF72BE6CE60 = header, 0x7FF72BE6CD20 = uint8, 0x7FF729064E60 = bits+flush) that
        // reproduces the three siblings already implemented here byte for byte:
        //   0x7FF729153060 (Blitz)     header + uint8 obj+0x20                       -> uint8 Roles
        //   0x7FF7291455E0 (RatedBG)   header + uint8 obj+0x20                       -> uint8 Roles
        //   0x7FF729153120 (Skirmish)  header + uint8 obj+0x20 + uint8 obj+0x21 + bit -> Roles, Bracket, Requeue
        //
        // The producer is the Lua binding C_PvP.JoinBrawl([isSpecialBrawl]) at RVA 0x1277770. It fills
        // obj+0x20 with the role mask exactly as C_PvP.JoinRatedBGBlitz does - `movzx esi, al` from the
        // allowed-roles call at 0x7FF72A99E020 ANDed with the player's selected roles byte, and it refuses
        // to send at all (error 0x33A) when that intersection is empty - and obj+0x21 with the parsed
        // isSpecialBrawl argument (`mov byte [rsp+0x51], r14b` at 0x7FF729D178FA, r14b = byte [rbp+0x70],
        // the bool the argument parser wrote).
        class BattlemasterJoinBrawl final : public ClientPacket
        {
        public:
            explicit BattlemasterJoinBrawl(WorldPacket&& packet)
                : ClientPacket(CMSG_BATTLEMASTER_JOIN_BRAWL, std::move(packet)) { }

            void Read() override;

            uint8 Roles = 0;
            bool IsSpecialBrawl = false;
        };

        class BattlefieldLeave final : public ClientPacket
        {
        public:
            explicit BattlefieldLeave(WorldPacket&& packet) : ClientPacket(CMSG_BATTLEFIELD_LEAVE, std::move(packet)) { }

            void Read() override { }
        };

        class BattlefieldPort final : public ClientPacket
        {
        public:
            explicit BattlefieldPort(WorldPacket&& packet) : ClientPacket(CMSG_BATTLEFIELD_PORT, std::move(packet)) { }

            void Read() override;

            WorldPackets::LFG::RideTicket Ticket;
            bool AcceptedInvite = false;
        };

        class BattlefieldListRequest final : public ClientPacket
        {
        public:
            explicit BattlefieldListRequest(WorldPacket&& packet) : ClientPacket(CMSG_BATTLEFIELD_LIST, std::move(packet)) { }

            void Read() override;

            int32 ListID = 0;
        };

        class BattlefieldList final : public ServerPacket
        {
        public:
            explicit BattlefieldList() : ServerPacket(SMSG_BATTLEFIELD_LIST, 1 + 1 + 16 + 1 + 1 + 1 + 4 + 1 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid BattlemasterGuid;
            int32 BattlemasterListID = 0;
            uint8 MinLevel = 0;
            uint8 MaxLevel = 0;
            std::vector<int32> Battlefields;    // Players cannot join a specific battleground instance anymore - this is always empty
            bool PvpAnywhere = false;
            bool HasRandomWinToday = false;
        };

        class GetPVPOptionsEnabled final : public ClientPacket
        {
        public:
            explicit GetPVPOptionsEnabled(WorldPacket&& packet) : ClientPacket(CMSG_GET_PVP_OPTIONS_ENABLED, std::move(packet)) { }

            void Read() override { }
        };

        class PVPOptionsEnabled final : public ServerPacket
        {
        public:
            explicit PVPOptionsEnabled() : ServerPacket(SMSG_PVP_OPTIONS_ENABLED, 1) { }

            WorldPacket const* Write() override;

            bool RatedBattlegrounds = false;
            bool PugBattlegrounds = false;
            bool WargameBattlegrounds = false;
            bool WargameArenas = false;
            bool RatedArenas = false;
            bool ArenaSkirmish = false;
            bool SoloShuffle = false;
            bool RatedSoloShuffle = false;
            bool BattlegroundBlitz = false;
            bool RatedBattlegroundBlitz = false; // solo rbg
        };

        class RequestBattlefieldStatus final : public ClientPacket
        {
        public:
            explicit RequestBattlefieldStatus(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_BATTLEFIELD_STATUS, std::move(packet)) { }

            void Read() override { }
        };

        class ReportPvPPlayerAFK final : public ClientPacket
        {
        public:
            explicit ReportPvPPlayerAFK(WorldPacket&& packet) : ClientPacket(CMSG_REPORT_PVP_PLAYER_AFK, std::move(packet)) { }

            void Read() override;

            ObjectGuid Offender;
        };

        class ReportPvPPlayerAFKResult final : public ServerPacket
        {
        public:
            explicit ReportPvPPlayerAFKResult() : ServerPacket(SMSG_REPORT_PVP_PLAYER_AFK_RESULT, 16 + 1 + 1 + 1) { }

            WorldPacket const* Write() override;

            enum ResultCode : uint8
            {
                PVP_REPORT_AFK_SUCCESS = 0,
                PVP_REPORT_AFK_GENERIC_FAILURE = 1, // there are more error codes but they are impossible to receive without modifying the client
                PVP_REPORT_AFK_SYSTEM_ENABLED = 5,
                PVP_REPORT_AFK_SYSTEM_DISABLED = 6
            };

            ObjectGuid Offender;
            uint8 NumPlayersIHaveReported = 0;
            uint8 NumBlackMarksOnOffender = 0;
            uint8 Result = PVP_REPORT_AFK_GENERIC_FAILURE;
        };

        struct BattlegroundPlayerPosition
        {
            ObjectGuid Guid;
            TaggedPosition<Position::XY> Pos;
            int8 IconID = 0;
            int8 ArenaSlot = 0;
        };

        class BattlegroundPlayerPositions final : public ServerPacket
        {
        public:
            explicit BattlegroundPlayerPositions() : ServerPacket(SMSG_BATTLEGROUND_PLAYER_POSITIONS, 4) { }

            WorldPacket const* Write() override;

            std::vector<BattlegroundPlayerPosition> FlagCarriers;
        };

        class BattlegroundPlayerJoined final : public ServerPacket
        {
        public:
            explicit BattlegroundPlayerJoined() : ServerPacket(SMSG_BATTLEGROUND_PLAYER_JOINED, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
        };

        class BattlegroundPlayerLeft final : public ServerPacket
        {
        public:
            explicit BattlegroundPlayerLeft() : ServerPacket(SMSG_BATTLEGROUND_PLAYER_LEFT, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
        };

        class DestroyArenaUnit final : public ServerPacket
        {
        public:
            explicit DestroyArenaUnit() : ServerPacket(SMSG_DESTROY_ARENA_UNIT, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
        };

        class RequestPVPRewards final : public ClientPacket
        {
        public:
            explicit RequestPVPRewards(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_PVP_REWARDS, std::move(packet)) { }

            void Read() override { }
        };

        class RequestRatedPvpInfo final : public ClientPacket
        {
        public:
            explicit RequestRatedPvpInfo(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_RATED_PVP_INFO, std::move(packet)) { }

            void Read() override { }
        };

        class RequestScheduledPvpInfo final : public ClientPacket
        {
        public:
            explicit RequestScheduledPvpInfo(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_SCHEDULED_PVP_INFO, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE (0x480015). This is the packet that tells the client which
        // PvP Brawl is currently running; there is no other source. Its handler, VA 0x7FF72AAC2120, is the only
        // writer of the two globals the whole brawl UI reads: dword_7FF72F082BB8 (the active brawl) and
        // dword_7FF72F082BBC (the active special-event brawl). Both are PvpBrawl.db2 row ids - the handler feeds
        // each straight into GetRow(&off_7FF72EDDD740 /*PvpBrawl.db2*/, id). C_PvP.GetAvailableBrawlInfo and
        // C_PvP.JoinBrawl read those same globals, so with this packet unsent the Brawl button can never become
        // queueable and CMSG_BATTLEMASTER_JOIN_BRAWL is never produced.
        //
        // The previous field names here (Field1..Flag2) were a guess at a flat layout. The real wire is
        // conditional, and the earlier "uint8 Field1" was in fact the two presence bits. Client deserializer at
        // 0x7FF7290FCA84 (message vtable 0x7FF72C4BA618, whose slot 3 is the GetOpcode thunk 0x7FF7290FB7E0 for
        // 0x480015 - that is what pins this decode to this opcode):
        //     read one byte; bit 7 -> HasBrawl, bit 6 -> HasSpecialEventBrawl
        //     if (HasBrawl)             uint32, uint32, one bit
        //     if (HasSpecialEventBrawl) uint32, uint32, one bit
        // (0x7FF72BE6C370 and 0x7FF72BE6C410 are the 1-byte and 4-byte stream reads; the "bits" are single
        // flushed bytes tested against 0x80, i.e. TrinityCore's Bits<1> + FlushBits, MSB first.)
        //
        // Confirmed on the wire. Every live 12.0.7 capture in C:\sniff carries the identical 19-byte body, e.g.
        // C:\sniff\b_pets12.0.7.pkt tick 23699:
        //     C0 | 78 00 00 00 | 47 0F 01 00 | 80 | 9B 00 00 00 | 00 00 00 00 | 80
        //     C0            = both presence bits set
        //     BrawlID 0x78  = 120 = PvpBrawl.db2 "Brawl: Classic Ashran"  (BattlemasterList 1021)
        //     0x00010F47    = 69447 seconds until the brawl rotates
        //     0x80          = CanQueue
        //     BrawlID 0x9B  = 155 = PvpBrawl.db2 "Decor Duel"             (no BattlemasterList - LFG brawl)
        //
        // CanQueue is the client's own field name: the handler stores this bit at PvpBrawlInfo+0x80, which
        // PvpInfoDocumentation.lua's PvpBrawlInfo structure names `canQueue`. The builder behind
        // C_PvP.GetAvailableBrawlInfo then computes `endTime = arrivalTime + (canQueue ? SecondsUntilNextChange
        // : 0)` and returns nil once now > endTime (0x7FF72AABEB7F-0x7FF72AABEBA6), so sending it false
        // advertises a brawl that expires the instant it arrives.
        //
        // The second block's SecondsUntilNextChange is read off the wire and then dropped on the floor: the
        // handler never stores it, and C_PvP.GetSpecialEventBrawlInfo hardcodes canQueue = 1. It is written
        // here only because the client's reader consumes the four bytes.
        class RequestScheduledPvpInfoResponse final : public ServerPacket
        {
        public:
            explicit RequestScheduledPvpInfoResponse() : ServerPacket(SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE, 1 + 4 + 4 + 1 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            struct BrawlInfo
            {
                uint32 BrawlID = 0;                     // PvpBrawl.db2 row id
                uint32 SecondsUntilNextChange = 0;
                bool CanQueue = false;
            };

            Optional<BrawlInfo> Brawl;
            Optional<BrawlInfo> SpecialEventBrawl;
        };

        class RatedPvpInfo final : public ServerPacket
        {
        public:
            explicit RatedPvpInfo() : ServerPacket(SMSG_RATED_PVP_INFO, 9 * sizeof(BracketInfo)) { }

            WorldPacket const* Write() override;

            struct BracketInfo
            {
                int32 PersonalRating = 0;
                int32 Ranking = 0;
                int32 SeasonPlayed = 0;
                int32 SeasonWon = 0;
                int32 SeasonFactionPlayed = 0;
                int32 SeasonFactionWon = 0;
                int32 WeeklyPlayed = 0;
                int32 WeeklyWon = 0;
                int32 RoundsSeasonPlayed = 0;
                int32 RoundsSeasonWon = 0;
                int32 RoundsWeeklyPlayed = 0;
                int32 RoundsWeeklyWon = 0;
                int32 BestWeeklyRating = 0;
                int32 LastWeeksBestRating = 0;
                int32 BestSeasonRating = 0;
                int32 PvpTierID = 0;
                int32 SeasonPvpTier = 0;
                int32 BestWeeklyPvpTier = 0;
                uint8 BestSeasonPvpTierEnum = 0;
                bool Disqualified = false;
            } Bracket[9];
        };

        struct RatedMatchDeserterPenalty
        {
            int32 PersonalRatingChange = 0;
            int32 QueuePenaltySpellID = 0;
            WorldPackets::Duration<Milliseconds, int32> QueuePenaltyDuration;
        };

        enum class PVPMatchState : uint8
        {
            Waiting     = 0,
            StartUp     = 1,
            Engaged     = 2,
            PostRound   = 3,
            Inactive    = 4,
            Complete    = 5
        };

        class PVPMatchInitialize final : public ServerPacket
        {
        public:
            // Reserve size only (no wire effect). StartTime and Duration are each EIGHT bytes on the wire,
            // not four - see the evidence block on PVPMatchInitialize::Write(). The old 4+1+4+4+1+4+1 spelling
            // understated both and has already misled one reader into "hunting" a nonexistent 4-byte hole.
            // 27 = base packet; a present DeserterPenalty adds 12 more for 39 total.
            explicit PVPMatchInitialize() : ServerPacket(SMSG_PVP_MATCH_INITIALIZE, 4 + 1 + 8 + 8 + 1 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 MapID = 0;
            PVPMatchState State = PVPMatchState::Inactive;
            Timestamp<> StartTime;
            WorldPackets::Duration<Seconds> Duration;
            Optional<RatedMatchDeserterPenalty> DeserterPenalty;
            uint8 ArenaFaction = 0;
            uint32 BattlemasterListID = 0;
            bool Registered = false;
            bool AffectsRating = false;
        };

        class PVPMatchSetState final : public ServerPacket
        {
        public:
            explicit PVPMatchSetState(PVPMatchState state) : ServerPacket(SMSG_PVP_MATCH_SET_STATE, 1), State(state) { }

            WorldPacket const* Write() override;

            PVPMatchState State;
        };

        class PVPMatchComplete final : public ServerPacket
        {
        public:
            explicit PVPMatchComplete() : ServerPacket(SMSG_PVP_MATCH_COMPLETE) { }

            WorldPacket const* Write() override;

            int32 Winner = 0;
            WorldPackets::Duration<Seconds> Duration;
            Optional<PVPMatchStatistics> LogData;
            uint32 SoloShuffleStatus = 0;
        };

        // SMSG_BATTLEGROUND_POINTS (0x480028), body = exactly 3 bytes.
        //
        // Client reader at VA 0x7FF7290FD3F3: one uint16 (helper 0x7FF72BE6C3C0) then one byte whose top bit
        // is taken as a bool (helper 0x7FF72BE6C370 followed by `shr al, 7`). The handler, VA 0x7FF72AABB450,
        // is a single statement - `scores[Team] = Points` - writing an int[2] at 0x7FF72F082C38 and then
        // firing one Lua event. So the bool is nothing but the index into the client's two-team score array.
        //
        // Which index is which faction is settled by C:\sniff\rated BG 12.0.7.pkt, which carries 322 of these
        // for one complete Deephaul Ravine style resource race. The Team=false stream ends on exactly 1500 -
        // the cap SMSG_BATTLEGROUND_INIT announced in the same match - while the Team=true stream stops at
        // 1427, and SMSG_PVP_MATCH_COMPLETE names winner 0. Winner 0 is PVP_TEAM_HORDE, and that packet's
        // field positions are independently pinned by its Duration of 496, which matches the 496575 ms
        // between SMSG_PVP_MATCH_SET_STATE(Engaged) and SMSG_PVP_MATCH_SET_STATE(Inactive) in the capture.
        // Team is therefore the PvPTeamId, false = PVP_TEAM_HORDE (0), true = PVP_TEAM_ALLIANCE (1) - the
        // OPPOSITE order from TeamId, which Battleground::m_TeamScores is indexed by.
        //
        // Retail sends this only when a team's score actually moves: across all 322 captured packets no
        // stream ever repeats a value, even though the source battleground ticks every two seconds.
        class BattlegroundPoints final : public ServerPacket
        {
        public:
            explicit BattlegroundPoints() : ServerPacket(SMSG_BATTLEGROUND_POINTS, 2 + 1) { }

            WorldPacket const* Write() override;

            uint16 BgPoints = 0;
            bool Team = false;
        };

        // SMSG_BATTLEGROUND_INIT (0x480029), body = exactly 6 bytes.
        //
        // The reader at VA 0x7FF7290FD47E does not parse this one: helper 0x7FF72BE6C980 just hands the
        // handler a pointer to the remaining bytes. The field split comes from the handler instead,
        // VA 0x7FF72AABB490, which does exactly two things with that blob:
        //   [0..3] uint32  ->  dword_7FF72CEEAF04 = clientNowMs - value, i.e. the client keeps the offset
        //                      between its own millisecond clock and ours so it can reconstruct server time.
        //   [4..5] uint16  ->  written into BOTH halves of the int[2] at 0x7FF72F082C40, the per-team score
        //                      cap that sits directly next to the score array SMSG_BATTLEGROUND_POINTS
        //                      writes. Guarded by `if (value)`, so a zero cap is ignored outright - which is
        //                      why this core only sends the packet for battlegrounds that declare a cap.
        // Field names below are ours; the client exports none. The sole capture reads
        // 73 E0 B5 38 | DC 05 = { 951820403, 1500 }, and 1500 is the cap the winning team stopped on.
        class BattlegroundInit final : public ServerPacket
        {
        public:
            explicit BattlegroundInit() : ServerPacket(SMSG_BATTLEGROUND_INIT, 4 + 2) { }

            WorldPacket const* Write() override;

            uint32 ServerTime = 0;
            uint16 MaxPoints = 0;
        };

        // Match kinds the client can name. This is not a guessed enum: it is the client's own label table at
        // 0x7FF72C3C61B0, read in order, which SMSG_PVP_MATCH_START's uint8 indexes directly (see below).
        // It also finally explains the bracket space referenced by BattlemasterJoinSkirmish above - the
        // rated-only subset {0,1,2,3,6,8} that C_PvP.GetPersonalRatedInfo accepts, and the value 4 that
        // C_PvP.JoinSkirmish always sends, are exactly the Skirmish and rated entries of this table.
        enum class PVPMatchBracket : uint8
        {
            Arena2v2            = 0,    // "2v2"
            Arena3v3            = 1,    // "3v3"
            Arena5v5            = 2,    // "5v5"
            RatedBattleground   = 3,    // "Rated BG"
            Skirmish            = 4,    // "Skirmish"
            BrawlSoloShuffle    = 5,    // "Brawl Solo Shuffle"
            RatedSoloShuffle    = 6,    // "Rated Solo Shuffle"
            BrawlSoloRBG        = 7,    // "Brawl Solo RBG"
            RatedSoloRBG        = 8     // "Rated Solo RBG"
        };

        // SMSG_PVP_MATCH_START (0x48002D), body = 22 bytes in the one capture we have.
        //
        // Reader at VA 0x7FF7290FD73D, in wire order: uint32, uint32, uint8, one bit + flush, uint32 element
        // count, int64, then that many 720-byte elements. The capture's count is 0, which accounts for all
        // 22 bytes with nothing left over: C5 08 00 00 | 29 00 00 00 | 08 | 00 | 00 00 00 00 | B5 F5 4B 6A ...
        //
        // The handler, VA 0x7FF72AABBAD0, formats the client's combat-log line
        //     "ARENA_MATCH_START,%d,%d,%s,%d"
        // from fields 1, 2, 4 and the bit, where %s is off_7FF72C3C61B0[field3] - so field 3 is a
        // PVPMatchBracket. It also stashes field 1 in dword_7FF72D34F5BC and derives a match-kind byte from
        // field 3 (5 or 6, the two Solo Shuffle entries, take a different branch from everything else).
        //
        // Field by field:
        //   MapID        2245, the same value SMSG_PVP_MATCH_INITIALIZE carried for this match 126 s earlier.
        //   ArenaSeason  INFERRED, not proven. The capture's value is 41, and the SMSG_SEASON_INFO sent to
        //                the same session reports CurrentArenaSeason 41 (PreviousArenaSeason 40,
        //                PvpSeasonID 39), so 41 is the current arena season and nothing else in the session
        //                matches it. The client only ever prints this field, so a wrong value costs a wrong
        //                number in a combat-log line and nothing more.
        //   Bracket      8 = "Rated Solo RBG". The capture is a rated Battleground Blitz, and the queue id
        //                the server echoed, 0x1F1000000019044D, decodes to BattlemasterListId 1101 =
        //                BATTLEGROUND_BLITZ. The label and the queue agree.
        //   Unknown1207  MEANING UNKNOWN. It is a single bit, it is only ever printed as the last %d of the
        //                combat-log line, and it was false in the only observation. It is not "rated": this
        //                match was rated and the bit was clear. We write the observed value and no more.
        //   Statistics   A counted array of 720-byte records read by sub_7FF729112EB0. It was empty in the
        //                capture, and its element layout is unverified, so Write() emits the observed count
        //                of zero and this packet carries no per-player payload.
        //   StartTime    1783169973, seven seconds after SMSG_PVP_MATCH_INITIALIZE's StartTime and 29 ms
        //                after SMSG_PVP_MATCH_SET_STATE(Engaged) - i.e. the moment the gates open.
        class PVPMatchStart final : public ServerPacket
        {
        public:
            explicit PVPMatchStart() : ServerPacket(SMSG_PVP_MATCH_START, 4 + 4 + 1 + 1 + 4 + 8) { }

            WorldPacket const* Write() override;

            uint32 MapID = 0;
            uint32 ArenaSeason = 0;
            PVPMatchBracket Bracket = PVPMatchBracket::Arena2v2;
            bool Unknown1207 = false;
            Timestamp<> StartTime;
        };

        enum class BattlegroundCapturePointState : uint8
        {
            Neutral             = 1,
            ContestedHorde      = 2,
            ContestedAlliance   = 3,
            HordeCaptured       = 4,
            AllianceCaptured    = 5
        };

        struct BattlegroundCapturePointInfo
        {
            ObjectGuid Guid;
            TaggedPosition<Position::XY> Pos;
            BattlegroundCapturePointState State = BattlegroundCapturePointState::Neutral;
            Timestamp<> CaptureTime;
            Duration<Milliseconds, uint32> CaptureTotalDuration;
        };

        class UpdateCapturePoint final : public ServerPacket
        {
        public:
            explicit UpdateCapturePoint() : ServerPacket(SMSG_UPDATE_CAPTURE_POINT) { }

            WorldPacket const* Write() override;

            BattlegroundCapturePointInfo CapturePointInfo;
        };

        class CapturePointRemoved final : public ServerPacket
        {
        public:
            explicit CapturePointRemoved() : ServerPacket(SMSG_CAPTURE_POINT_REMOVED) { }
            explicit CapturePointRemoved(ObjectGuid capturePointGUID) : ServerPacket(SMSG_CAPTURE_POINT_REMOVED), CapturePointGUID(capturePointGUID) { }

            WorldPacket const* Write() override;

            ObjectGuid CapturePointGUID;
        };

        // War Games: an arranged PvP practice match between two premade groups. The initiating party leader
        // picks a battleground/arena and challenges the leader of another group; that leader accepts or declines.
        // Wire recovered byte-exact from the 12.0.7 client (send serializers sub_7FF72906F9F0 / sub_7FF72906FC80,
        // SMSG deserializer sub_7FF729086A80). Field48/Field52 identify the chosen battleground selection; QueueID
        // is the packed queue descriptor echoed back by the opponent to correlate the response.
        class StartWarGame final : public ClientPacket
        {
        public:
            explicit StartWarGame(WorldPacket&& packet) : ClientPacket(CMSG_START_WAR_GAME, std::move(packet)) { }

            void Read() override;

            ObjectGuid OpposingPartyMember;      // a member of the group being challenged
            uint32 BattlemasterListID = 0;       // wire uint32 @48 (chosen BG/arena; inferred)
            uint16 Bracket = 0;                   // wire uint16 @52 (bracket/team-size selector; inferred)
            uint64 QueueID = 0;                   // wire uint64 @56 (packed queue descriptor)
            bool TournamentRules = false;         // trailing bit
        };

        class AcceptWargameInvite final : public ClientPacket
        {
        public:
            explicit AcceptWargameInvite(WorldPacket&& packet) : ClientPacket(CMSG_ACCEPT_WARGAME_INVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid OpposingPartyMember;      // the challenger (initiator) whose invite this answers
            uint64 QueueID = 0;
            bool Accept = false;
        };

        class CheckWargameEntry final : public ServerPacket
        {
        public:
            explicit CheckWargameEntry() : ServerPacket(SMSG_CHECK_WARGAME_ENTRY, 16 + 8 + 8 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid OpposingPartyMember;      // the challenger's group leader
            uint64 QueueID = 0;
            uint64 Time = 0;                      // wire uint64 (response window; sent 0 until timed out in P1)
            bool TournamentRules = false;
        };

        class WargameRequestSuccessfullySentToOpponent final : public ServerPacket
        {
        public:
            explicit WargameRequestSuccessfullySentToOpponent() : ServerPacket(SMSG_WARGAME_REQUEST_SUCCESSFULLY_SENT_TO_OPPONENT, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid OpposingPartyMember;      // the opposing group leader the challenge was sent to
        };

        class WargameRequestOpponentResponse final : public ServerPacket
        {
        public:
            explicit WargameRequestOpponentResponse() : ServerPacket(SMSG_WARGAME_REQUEST_OPPONENT_RESPONSE, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid OpposingPartyMember;      // the responder (opposing group leader)
            bool Accepted = false;
        };
    }
}

#endif // TRINITYCORE_BATTLEGROUND_PACKETS_H

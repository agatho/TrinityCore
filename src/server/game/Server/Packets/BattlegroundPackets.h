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
            // ChrSpecializationRole of the invited player - Tank 0, Healer 1, Dps 2 - NOT an lfg role mask.
            // Kept as a plain uint8 because that is what goes on the wire and what the neighbouring role
            // counters in this file use; BattlegroundMgr::BuildBattlegroundStatusNeedConfirmation carries the
            // evidence for the numbering and for what the client does with it.
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

        // SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS (0x4B000D) and
        // SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED (0x4B000E).
        // SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS (0x48000D) and
        // SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED (0x48000E).
        //
        // Both were decoded byte for byte from the 18 + 2 occurrences in C:\sniff\rated BG 12.0.7.pkt and
        // confirmed against the client readers. (Note that "rbg rated BG 12.0.7.pkt" is a byte-identical copy
        // of that file, so this is one capture and not two.) The decode is kept here because the wire form is
        // the only place the field MEANINGS are written down.
        //
        // NUMBERING: the header lines above carry the 12.1 opcode values, which is what this tree's
        // Opcodes.h holds (SMSG_PONG = 0x4C0009). Every bare 0x48xxxx below is the SAME opcode under
        // the 12.0.7 numbering the capture and the 68275 client use - 12.1 shifted the families up by
        // three, so 0x48000D and 0x4B000D are one opcode, not two. The capture-side values are left
        // unconverted on purpose: they are what a re-read of the .pkt actually shows.
        //
        //   0x48000D  BattlefieldStatusHeader - 46 bytes: 9-byte packed guid + Id + Type + int64 Time + a bit
        //             flush for the RideTicket, then QueueID count 1, RangeMin 0, RangeMax 90, TeamSize 0,
        //             InstanceID 0, the 8-byte QueueID, then the RegisteredMatch/TournamentRules bits and a
        //             flush (captured 0x80, i.e. a rated match). Read by sub_7FF7290FAAE0, identical field
        //             order to operator<<(ByteBuffer&, BattlefieldStatusHeader const&).
        //             Then uint32 MapID, uint32 TimeoutMs (30000 in every capture),
        //             uint8 SlotsPerSide[2] and uint8 AwaitedPerSide[2] written index-interleaved as
        //                 SlotsPerSide[0], AwaitedPerSide[0], SlotsPerSide[1], AwaitedPerSide[1],
        //             then the 3x3 role block below. 68-byte body.
        //   0x48000E  BattlefieldStatusHeader, then the 3x3 role block. Nothing else. 56-byte body.
        //
        // The role block is read by sub_7FF7290FAD90 into three uint8[3] arrays indexed by role - Awaited at
        // struct +0, Secured at +3, Lost at +6 - and the wire order is index-interleaved:
        // Awaited[0],Secured[0],Lost[0], Awaited[1],Secured[1],Lost[1], Awaited[2],Secured[2],Lost[2].
        // One bit and a flush follow; sub_7FF7290FAD90 stores it (shr al, 7) at struct +9, and the consumer
        // sub_7FF72AAB93E0 tests exactly that byte (cmp byte ptr [r15+0x81], 0) and SKIPS building the whole
        // role list when it is clear - so it is a "these role counts are populated" gate, not padding. It is
        // set in all 20 captured bodies. The role order is TANK, HEALER, DAMAGER, read out of the client's own
        // name table at 0x7FF72C3C5DE0; that is the same numbering as ChrSpecializationRole, which is what
        // lets the server fill these counters from a player's actual specialization.
        // The consumers (sub_7FF72AAB93E0 for 0x48000D, sub_7FF72AAB97D0 for 0x48000E) build a vector of a
        // struct the client names PvpRoleQueueInfo, whose fields are
        // { roleName, Awaited[i] + Secured[i] + Lost[i], Secured[i], Lost[i] } - so the three counters SUM to
        // that role's requirement and the client shows Secured out of that sum.
        //
        // Semantics, established by re-decoding all 20 bodies with their tick timestamps:
        //   Awaited = still to be secured, Secured = secured already, Lost = failed to secure.
        //   In 0x48000D, Lost is 0 in every sample and sum(Awaited) == AwaitedPerSide[0] + AwaitedPerSide[1].
        //   In 0x48000E, Awaited is 0 and Lost holds exactly the Awaited vector of the immediately preceding
        //   0x48000D - the proposal collapsed and the outstanding players became losses. Both invariants hold
        //   in every sample, and Awaited+Secured+Lost is the constant [0, 4, 12] = 16 players = 8v8 throughout.
        //
        // Which index of SlotsPerSide/AwaitedPerSide is which faction: NEITHER, and that is measured rather
        // than assumed. NOTE THE IMAGE BASE - every VA in the next two paragraphs is from wow_dump.bin.i64
        // as it loads today, at 0x7FF780FD0000, while the sub_7FF729.../sub_7FF72A... VAs elsewhere in this
        // file come from the older analysis DB based at 0x7FF728900000. The two do NOT convert by a flat
        // rebase; do not mix them. The reliable bridge is c:\dumps\_4b_hookstate.txt, which lists the
        // registrar of every 0x4B opcode at today's base.
        // The two pairs land at +144/+148 and +152/+156 of the 176-byte per-queue record the
        // client allocates in Ui\PvpInfo.cpp:2175, and that record is reachable only through its two list
        // heads (0x7FF7853B37D0, 0x7FF7853B37D8) and its finder (0x7FF78318F930). Enumerating every function
        // that references any of the three gives 41, and exactly three of them touch those four slots: the
        // allocator, which zeroes them; the handler above (0x7FF783190300), which writes them; and ONE reader
        // - the legacy Lua global GetWarGameQueueStatus (binding table 0x7FF7853B3B10 -> sub_7FF7831A2690),
        // which pushes them as four bare numbers in the order SlotsPerSide[1], SlotsPerSide[0],
        // AwaitedPerSide[1], AwaitedPerSide[0] and labels neither index. Widening the sweep one call level
        // over all 109 direct callees of those 41 adds nothing: the seven that touch +0x90..+0x9C do so on
        // other types - a qword at +0x90 beside a byte at +0x98, a 0..5 state byte - and none reads all four.
        // No file in the retail FrameXML calls GetWarGameQueueStatus at all. The single asymmetry that does
        // exist, the reader returning index 1 before index 0, carries no name, because no UI names the
        // returns; it is the whole of what the client says about the ordering.
        //
        // The index is therefore a free convention, and this is exactly where the sister field parts company:
        // SMSG_BATTLEGROUND_POINTS.Team indexes a faction-labelled score array in the client, so its
        // PvPTeamId inversion is load bearing, while nothing downstream of these four bytes is labelled at
        // all. SendProposalStatus fills them by TeamId (alliance 0), the same order BattlegroundQueue keeps
        // its selection pools in, so the server stays self-consistent; no observer can tell the difference.
        //
        // The capture also settles that this is a BOUNDED proposal and not an open-ended queue readout. Three
        // runs, each with its own MapID, each inside the advertised 30000 ms:
        //   map 2656, ticks 138410..166467 = 28.1 s, then 0x48000E with Lost = [0,2,4] (6 outstanding)
        //   map 2107, ticks 871891..894382 = 22.5 s, then 0x48000E with Lost = [0,0,1] (1 outstanding)
        //   map 2245, ticks 906202..911487 =  5.3 s, then a terminal 0x48000D that zeroes AwaitedPerSide and
        //                                            the whole role block - the match formed.
        //
        // This core runs that phase for the Battleground Blitz solo queue, so every field written below is
        // real: BattlegroundQueue creates the battleground FIRST - which is what makes MapID and the per-side
        // split exist at all - invites its members with a 30 s deadline, and ports nobody until all of them
        // have accepted. See BattlegroundProposal in BattlegroundQueue.h. Secured counts accepted members,
        // Awaited the ones still deciding, and a decline or a deadline miss collapses the proposal into
        // 0x48000E with the outstanding members as Lost. Retail's per-run MapID churn falls out for free: a
        // collapsed proposal drops its battleground and the next attempt calls CreateNewBattleground again.
        //
        // Their CONNECTION_TYPE_REALM declaration is confirmed rather than merely a safe default: retail sends
        // both on connection index 0 in the capture. An earlier revision of this comment added "unlike
        // SMSG_BATTLEFIELD_STATUS_QUEUED on index 1" - that was wrong, and it was the only reason this branch
        // ever had to suspect the QUEUED line in Opcodes.cpp. Recounted over all 75 .pkt files under C:/sniff,
        // the entire status block is on index 0 without exception: QUEUED 102/102, WAIT_FOR_GROUPS 36/36,
        // GROUP_PROPOSAL_FAILED 4/4, NEED_CONFIRMATION 6/6, ACTIVE 4/4, NONE 2/2 - and in the same files the
        // second socket does exist and does carry traffic (SMSG_BATTLEGROUND_POINTS 644/644 and
        // SMSG_REQUEST_PVP_REWARDS_RESPONSE 8/8 on index 1), so index 0 here is a real choice by retail and
        // not an artefact of a single-socket capture. CONNECTION_TYPE_REALM stands for all of them.

        // The per-role counters below are indexed in wire order, which is identical to ChrSpecializationRole
        // (Tank 0, Healer 1, Dps 2) - and that enum is what the queue code actually indexes with
        // (PlayerQueueInfo::Role, RoleIndex, FoldRole), so no second enum is declared here for it.
        // both on connection index 0 in the capture, unlike SMSG_BATTLEFIELD_STATUS_QUEUED on index 1.

        // Wire order of the per-role counters, identical to ChrSpecializationRole (Tank 0, Healer 1, Dps 2).
        enum class PvpQueueRole : uint8
        {
            Tank    = 0,
            Healer  = 1,
            Damager = 2
        };

        std::size_t constexpr PVP_QUEUE_ROLE_COUNT = 3;

        struct PvpRoleQueueCounts
        {
            std::array<uint8, PVP_QUEUE_ROLE_COUNT> Awaited = { };  // still to be secured
            std::array<uint8, PVP_QUEUE_ROLE_COUNT> Secured = { };  // secured already
            std::array<uint8, PVP_QUEUE_ROLE_COUNT> Lost = { };     // failed to secure
            // Gates the client's entire role display - see sub_7FF72AAB93E0. Set in all 20 captured bodies.
            bool HasRoleCounts = true;
        };

        class BattlefieldStatusWaitForGroups final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusWaitForGroups() : ServerPacket(SMSG_BATTLEFIELD_STATUS_WAIT_FOR_GROUPS, sizeof(BattlefieldStatusHeader) + 4 + 4 + 4 + 10) { }

            WorldPacket const* Write() override;

            BattlefieldStatusHeader Hdr;
            uint32 Mapid = 0;
            uint32 Timeout = 0;
            std::array<uint8, 2> SlotsPerSide = { };
            std::array<uint8, 2> AwaitedPerSide = { };
            PvpRoleQueueCounts Roles;
        };

        class BattlefieldStatusGroupProposalFailed final : public ServerPacket
        {
        public:
            explicit BattlefieldStatusGroupProposalFailed() : ServerPacket(SMSG_BATTLEFIELD_STATUS_GROUP_PROPOSAL_FAILED, sizeof(BattlefieldStatusHeader) + 10) { }

            WorldPacket const* Write() override;

            BattlefieldStatusHeader Hdr;
            PvpRoleQueueCounts Roles;
        };

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

        // CMSG_BATTLEMASTER_JOIN_RATED_BG_BLITZ (0x3E00C0), body = exactly 1 byte.
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

        // CMSG_BATTLEMASTER_JOIN_SKIRMISH (0x3E00C1), body = 3 bytes.
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

        // CMSG_JOIN_RATED_BATTLEGROUND (0x3D0025), body = exactly 1 byte: uint8 Roles.
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

        // CMSG_BATTLEMASTER_JOIN_BRAWL (0x3E00C4), body = 2 bytes.
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

        // Refusal of a CMSG_BATTLEFIELD_PORT. We send zero bytes. Note the client does not require that: like
        // every raw-pointer opcode (dispatcher case 4915211 calls 0x35AF730, which hands over the whole unread
        // rest as an opaque blob) it would swallow any length silently - this consumer simply never reads it.
        // The consumer 0x21C23E0 is 33 bytes: call 0x1DF32B0, test rax/rax, and on non-null a tail call to
        // ShowSystemMessage(0xAB) at 0x209AD90. 0x1DF32B0 is not a predicate about this message - it has 3287
        // call sites, 0x209AD90 itself is one of them, and it returns a pointer that the caller null-checks.
        // It is also obfuscated: hexrays gives up on it (JUMPOUT), and its prologue takes its own return address
        // from [rbp+8] and tests that the byte at retaddr-5 is 0xE8, i.e. that it was entered by a direct call.
        // So the message shows unconditionally in practice; the null check is not a condition we can trigger.
        // String id 0xAB is entry 171 of the client error table at 0x43D55C0 (stride 24, +0 the ERR_
        // key, +8 the display type): ERR_PLAYER_DEAD, type 2. Type 2 fires the Lua event UI_ERROR_MESSAGE, so
        // this shows as red text in UIErrorsFrame; type 0 is the one that goes to the chat frame.
        // The TrinityCore name is historical - the client carries no "port denied" text, and no SMSG_/CMSG_
        // strings at all.
        class BattlefieldPortDenied final : public ServerPacket
        {
        public:
            explicit BattlefieldPortDenied() : ServerPacket(SMSG_BATTLEFIELD_PORT_DENIED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class BattlefieldListRequest final : public ClientPacket
        {
        public:
            explicit BattlefieldListRequest(WorldPacket&& packet) : ClientPacket(CMSG_BATTLEFIELD_LIST, std::move(packet)) { }

            void Read() override;

            int32 ListID = 0;
        };

        // Rate-limit refusal for CMSG_BATTLEMASTER_HELLO, sent instead of a SMSG_BATTLEFIELD_LIST. Zero bytes,
        // same raw-pointer shape as BattlefieldPortDenied (dispatcher case 4915212). The consumer 0x1E20180 is a
        // 10-byte tail call - mov ecx, 2F8h ; jmp 0x209AD90 - with no guard and no condition. 0x2F8 is entry 760
        // of the error table at 0x43D55C0: ERR_BATTLEGROUND_INFO_THROTTLED, display type 0, which is the chat
        // frame path - not UIErrorsFrame, that is type 2. The tag matches the opcode name exactly, which is
        // what pins the whole table indexing.
        // Its client registrar (0x1E2F5D0) sits in the NPC interaction code rather than in the PvP queue
        // registrar block (0x21BCF40) - the throttle belongs to talking to a battlemaster, not to the queue.
        class BattlegroundInfoThrottled final : public ServerPacket
        {
        public:
            explicit BattlegroundInfoThrottled() : ServerPacket(SMSG_BATTLEGROUND_INFO_THROTTLED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
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

        // SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE (0x4B0015). This is the packet that tells the client which
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
        // SMSG_REQUEST_SCHEDULED_PVP_INFO_RESPONSE (0x480015): describes the currently-scheduled special PvP event
        // (e.g. a PvP brawl / timewalking-PvP rotation). Wire from the client reader (all_smsg_layouts):
        // { uint8; uint32; uint32; bit; uint32; uint32; bit }. TrinityCore has no PvP-event scheduler, so the
        // response is all-inactive (every field 0 / flag false) -- the truthful "no scheduled PvP event", mirroring
        // how HandleRequestRatedPvpInfo answers with a default/empty RatedPvpInfo.
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
            uint8 Field1 = 0;
            uint32 Field2 = 0;
            uint32 Field3 = 0;
            bool Flag1 = false;
            uint32 Field4 = 0;
            uint32 Field5 = 0;
            bool Flag2 = false;
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

        // SMSG_BATTLEGROUND_POINTS (0x4B0028), body = exactly 3 bytes.
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

        // SMSG_BATTLEGROUND_INIT (0x4B0029), body = exactly 6 bytes.
        // SMSG_BATTLEGROUND_INIT (0x480029), body = exactly 6 bytes.
        //
        // The reader at VA 0x7FF7290FD47E does not parse this one: helper 0x7FF72BE6C980 just hands the
        // handler a pointer to the remaining bytes. The field split comes from the handler instead,
        // VA 0x7FF72AABB490, which does exactly two things with that blob:
        //   [0..3] uint32  ->  dword_7FF72CEEAF04 = clientNowMs - value, i.e. the client keeps the offset
        //                      between its own millisecond clock and ours so it can reconstruct server time.
        //   [4..5] uint16  ->  written into BOTH halves of the int[2] at 0x7FF72F082C40, the per-team score
        //                      cap that sits directly next to the score array SMSG_BATTLEGROUND_POINTS
        //                      writes. Guarded by `if (value)`, so a zero cap is ignored outright.
        //
        // The asymmetry between those two fields is the whole reason this packet goes out for EVERY match and
        // not only for the resource races. Field 1 is written before the guard and outside it, and its target
        // is the client's only source for that offset: at today's image base the store is the single writer
        // of dword_7FF785778580 (sub_7FF783192490+0x1A), against seven reads in four functions. Two of those
        // are the arena crowd-control bar - C_PvP.GetArenaCrowdControlDuration (sub_7FF78238D340) and
        // C_PvP.GetArenaCrowdControlInfo (sub_7FF78238DA40) - and the other two feed C_Commentator's spell
        // charges; all four add the offset to a SERVER millisecond stamp and then compare the result against
        // the client's own clock. An arena declares no score cap, so gating the whole send on the cap would
        // have left every arena with an offset of zero and a CC bar computed across two unrelated epochs.
        // Sending it with MaxPoints = 0 costs nothing: the client's own `if (value)` discards the zero cap,
        // and with it both Lua events, so a capless match gets the clock and nothing else.
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

        // SMSG_PVP_MATCH_START (0x4B002D), body = 22 bytes in the one capture we have.
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
            // UNVERIFIED: that field 2 is the arena season. Inferred from one capture in which its value, 41,
            // equals the CurrentArenaSeason of the SMSG_SEASON_INFO sent to the same session and nothing else
            // in that session. The client only prints it, so a wrong value costs one wrong number in a
            // combat-log line. Settled by any capture in a different season, or by naming the client global.
            uint32 ArenaSeason = 0;
            PVPMatchBracket Bracket = PVPMatchBracket::Arena2v2;
            // UNVERIFIED: the meaning of this bit. It was false in the only observation and is only ever
            // printed as the last %d of "ARENA_MATCH_START,%d,%d,%s,%d". It is NOT "rated" - the capture was a
            // rated match with the bit clear. We write the observed value; the sender below never sets it.
            bool Unknown1207 = false;
            Timestamp<> StartTime;

            // UNVERIFIED: the layout of the per-player statistics element. The wire carries a counted array of
            // records read by sub_7FF729112EB0 (720 bytes each in client memory) after StartTime; the count was
            // 0 in the only capture, which is why the 22-byte body parses with nothing left over. Write()
            // therefore emits the observed count of zero and this packet carries no per-player payload. A
            // capture of a match end with populated statistics is what decodes it - see aufnahme_noetig.
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

        // The full set of capture points, sent once when a player enters the battleground; SMSG_UPDATE_CAPTURE_POINT
        // carries the per-point deltas afterwards. Both paths stay live - the list consumer 0x21C2520 and the
        // single consumer 0x21C25C0 write into the same array (qword_7FF7877DAE18) and both add the client's
        // millisecond tick onto CaptureTime, so the init does not suppress later updates.
        //
        // Because they share that array, the snapshot and the deltas must share a socket, or a player entering a
        // running capture game can take a delta before the snapshot and have the snapshot write the stale State
        // back. All three capture-point messages therefore sit on CONNECTION_TYPE_INSTANCE, and that is measured,
        // not chosen: a census of every .pkt under C:/sniff (75 files) finds SMSG_UPDATE_CAPTURE_POINT 148 times
        // and SMSG_CAPTURE_POINT_REMOVED 44 times, every single body on connection index 1, across two captures
        // whose realm socket carries 5402 records each - so index 0 is well populated and the result is signal,
        // not an artefact. SMSG_MAP_OBJECTIVES_INIT itself appears in no capture; it follows its own array.
        // Wire: uint32 count then count elements, list reader 0x732C20 stepping 48 bytes into element reader
        // 0x731F80. There are no other scalars in the message. Note that the 48 bytes are the CLIENT struct
        // size; on the wire Guid is a packed guid, so elements are variable length.
        class MapObjectivesInit final : public ServerPacket
        {
        public:
            explicit MapObjectivesInit() : ServerPacket(SMSG_MAP_OBJECTIVES_INIT, 4) { }

            WorldPacket const* Write() override;

            std::vector<BattlegroundCapturePointInfo> CapturePoints;
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
    }
}

#endif // TRINITYCORE_BATTLEGROUND_PACKETS_H

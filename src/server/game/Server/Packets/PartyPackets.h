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

#ifndef TRINITYCORE_PARTY_PACKETS_H
#define TRINITYCORE_PARTY_PACKETS_H

#include "Packet.h"
#include "AuthenticationPackets.h"
#include "ObjectGuid.h"
#include "MythicPlusPacketsCommon.h"
#include "Optional.h"
#include "Position.h"

class Player;
struct RaidMarker;
enum class PingSubjectType : uint8;
enum class RestrictPingsTo : int32;

namespace WorldPackets
{
    namespace Party
    {
        class PartyCommandResult final : public ServerPacket
        {
        public:
            explicit PartyCommandResult() : ServerPacket(SMSG_PARTY_COMMAND_RESULT, 23) { }

            WorldPacket const* Write() override;

            std::string Name;
            uint8 Command = 0u;
            uint8 Result = 0u;
            uint32 ResultData = 0u;
            ObjectGuid ResultGUID;
        };

        class PartyInviteClient final : public ClientPacket
        {
        public:
            explicit PartyInviteClient(WorldPacket&& packet) : ClientPacket(CMSG_PARTY_INVITE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            uint32 ProposedRoles = 0;
            std::string TargetName;
            std::string TargetRealm;
            ObjectGuid TargetGUID;
        };

        class PartyInvite final : public ServerPacket
        {
        public:
            explicit PartyInvite() : ServerPacket(SMSG_PARTY_INVITE, 55) { }

            WorldPacket const* Write() override;

            void Initialize(Player const* inviter, int32 proposedRoles, bool canAccept);

            bool ShouldSquelch = false;
            bool AllowMultipleRoles = false;
            bool QuestSessionActive = false;
            bool IsCrossFaction = false;
            uint16 InviterCfgRealmID = 0;

            bool CanAccept = false;

            // Inviter
            Auth::VirtualRealmInfo InviterRealm;
            ObjectGuid InviterGUID;
            ObjectGuid InviterBNetAccountId;
            std::string InviterName;

            // Realm
            bool IsXRealm = false;
            bool IsXNativeRealm = false;

            // Lfg
            uint8 ProposedRoles = 0;
            uint32 LfgCompletedMask = 0;
            std::vector<uint32> LfgSlots;
        };

        // Asks the group leader to confirm an invite that a third party set in motion; it does not
        // confirm anything by itself. The client queues it (ring buffer of 4, class
        // ClientConfirmPartyInvite, 832 bytes) and fires GROUP_INVITE_CONFIRMATION without payload;
        // the UI then pulls the values back through GetInviteConfirmationInfo() and
        // C_PartyInfo.GetInviteReferralInfo(). The answer arrives as
        // CMSG_QUICK_JOIN_RESPOND_TO_INVITE - with the two guids swapped, see WorldSession.
        //
        // There is no confirmationType on the wire. The client derives it:
        //   ShowChatLine set                -> 1 LE_INVITE_CONFIRMATION_REQUEST
        //   else ReferredByGUID not empty   -> 2 LE_INVITE_CONFIRMATION_SUGGEST
        //   else                            -> 3 LE_INVITE_CONFIRMATION_QUEUE_WARNING
        // ShowChatLine additionally triggers the ERR_REQUESTED_INVITE_TO_GROUP_SS chat line; the two
        // effects cannot be separated.
        //
        // Layout from reader RVA 0x605BF0 -> 0x605880 at build 12.1.0.69382, verified field by field
        // against the decompiled body. Read order is authoritative and is reproduced exactly below.
        class ConfirmPartyInvite final : public ServerPacket
        {
        public:
            explicit ConfirmPartyInvite() : ServerPacket(SMSG_CONFIRM_PARTY_INVITE, 16 + 16 + 12 + 2 + 16 + 17 + 2 + 18 + 2 + 12) { }

            WorldPacket const* Write() override;

            // Fills the fields the invite confirmation tooltip needs. Leaving Level, SpecID or
            // ItemLevel at 0 leaves the client tooltip stuck on RETRIEVING_DATA.
            void InitializeApplicant(Player const* applicant);

            // Must carry HighGuid::Party (or LMMParty) - the client validates the high type while
            // copying the message and drops entries that carry anything else. Comes back as the
            // second guid of CMSG_QUICK_JOIN_RESPOND_TO_INVITE.
            ObjectGuid PartyGUID;
            // Must carry HighGuid::Player - validated the same way, and additionally checked in the
            // response path. This is the queue key the UI answers with, so it has to be unique per
            // outstanding confirmation. Comes back as the first guid of the response.
            ObjectGuid ApplicantGUID;

            // Queues the applicant cannot be pulled out of; handed to Lua by
            // C_PartyInfo.GetInviteConfirmationInvalidQueues. Empty is the normal case.
            std::vector<uint32> InvalidLFG;         // LFGDungeons.ID
            std::vector<uint32> InvalidLFGList;     // premade group finder listing ids
            std::vector<uint64> InvalidPvP;         // battleground / arena queue ids

            bool ShowChatLine = false;              // -> confirmationType 1 plus a chat line
            bool Unused801 = false;                 // read by the client, never queried by the UI

            ObjectGuid ReferredByGUID;              // set -> confirmationType 2
            uint32 ReferralUnused776 = 0;
            uint64 ReferredByClubID = 0;
            uint8 ReferralRelation = 0;             // Enum.PartyRequestJoinRelation
            uint32 ReferralUnused796 = 0;
            std::string ReferredByName;             // clamped to 305 on write, client buffer is 306
            uint64 ReferralUnused808 = 0;

            uint8 Level = 0;
            uint32 SpecID = 0;                      // ChrSpecialization.ID
            uint32 ItemLevel = 0;
            uint8 FactionGroupMask = 0;             // FactionGroup mask, not a row id
            uint8 Unused830 = 0;

            bool RolesInvalid = false;
            bool WillConvertToRaid = false;
            bool IsCrossFaction = false;
            std::string Name;                       // client buffer is 306 bytes, clamped on write
        };

        // The answer to SMSG_CONFIRM_PARTY_INVITE - Accept/Decline of the leader's confirmation
        // dialog, sent by C.RespondToInviteConfirmation(guid, accept) (GameDialogDefs.lua:336/339).
        // Writer RVA 0x6AF830 (0x6AF7C0 payload), response builder RVA 0x21981D0, build
        // 12.1.0.69382. 5..37 bytes.
        //
        // THE TWO GUIDS COME BACK SWAPPED. The client keys its pending queue on the applicant guid,
        // and that key is written first: wire guid #1 is SMSG_CONFIRM_PARTY_INVITE.ApplicantGUID
        // (its field 2), wire guid #2 is SMSG_CONFIRM_PARTY_INVITE.PartyGUID (its field 1). A
        // server that assumes the order of the SMSG assigns the answer to the wrong applicant.
        //
        // Guid #1 is coerced to HighGuid::Player rather than checked: the helper at RVA 0x200CF0
        // passes a player guid and an empty guid through and replaces anything else with an empty
        // guid, without logging. The only outright refusal on the way in is an IsEmpty test on the
        // Lua argument. So a wrong high type does not produce a malformed packet - it produces one
        // with an empty first guid, which finds no pending confirmation here and is dropped.
        class QuickJoinRespondToInvite final : public ClientPacket
        {
        public:
            explicit QuickJoinRespondToInvite(WorldPacket&& packet) : ClientPacket(CMSG_QUICK_JOIN_RESPOND_TO_INVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid ApplicantGUID;       // wire guid #1
            ObjectGuid PartyGUID;           // wire guid #2
            bool Accept = false;
        };

        class PartyInviteResponse final : public ClientPacket
        {
        public:
            explicit PartyInviteResponse(WorldPacket&& packet) : ClientPacket(CMSG_PARTY_INVITE_RESPONSE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool Accept = false;
            Optional<uint8> RolesDesired;
        };

        class PartyUninvite final : public ClientPacket
        {
        public:
            explicit PartyUninvite(WorldPacket&& packet) : ClientPacket(CMSG_PARTY_UNINVITE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            ObjectGuid TargetGUID;
            std::string Reason;
        };

        class GroupDecline final : public ServerPacket
        {
        public:
            explicit GroupDecline(std::string const& name) : ServerPacket(SMSG_GROUP_DECLINE, 2 + name.size()), Name(name) { }

            WorldPacket const* Write() override;

            std::string Name;
        };

        class GroupUninvite final : public ServerPacket
        {
        public:
            explicit GroupUninvite() : ServerPacket(SMSG_GROUP_UNINVITE, 1) { }

            WorldPacket const* Write() override;

            uint8 Reason = 0;
        };

        class RequestPartyMemberStats final : public ClientPacket
        {
        public:
            explicit RequestPartyMemberStats(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_PARTY_MEMBER_STATS, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            Array<ObjectGuid, 40> Targets;
        };

        struct PartyMemberPhase
        {
            uint32 Flags = 0u;
            uint16 Id = 0u;
        };

        struct PartyMemberPhaseStates
        {
            uint32 PhaseShiftFlags = 0;
            ObjectGuid PersonalGUID;
            std::vector<PartyMemberPhase> List;
        };

        struct PartyMemberAuraStates
        {
            int32 SpellID = 0;
            uint16 Flags = 0;
            uint32 ActiveFlags = 0u;
            std::vector<float> Points;
        };

        struct PartyMemberPetStats
        {
            ObjectGuid GUID;
            std::string Name;
            int16 ModelId = 0;

            int32 CurrentHealth = 0;
            int32 MaxHealth = 0;

            std::vector<PartyMemberAuraStates> Auras;
        };

        struct CTROptions
        {
            std::span<uint32 const> ConditionalFlags;
            int8 FactionGroup = 0;
            uint32 ChromieTimeExpansionMask = 0;
        };

        struct PartyMemberStats
        {
            uint16 Level = 0;
            uint32 Status = 0;

            int32 CurrentHealth = 0;
            int32 MaxHealth = 0;

            uint8 PowerType = 0u;
            uint16 CurrentPower = 0;
            uint16 MaxPower = 0;

            uint16 ZoneID = 0;
            int16 PositionX = 0;
            int16 PositionY = 0;
            int16 PositionZ = 0;

            int32 VehicleSeat = 0;

            PartyMemberPhaseStates Phases;
            std::vector<PartyMemberAuraStates> Auras;
            Optional<PartyMemberPetStats> PetStats;

            uint16 PowerDisplayID = 0;
            uint16 SpecID = 0;
            uint16 WmoGroupID = 0;
            uint32 WmoDoodadPlacementID = 0;
            int8 PartyType[2] = { };

            CTROptions ChromieTime;

            MythicPlus::DungeonScoreSummary DungeonScore;
        };

        class PartyMemberFullState final : public ServerPacket
        {
        public:
            explicit PartyMemberFullState() : ServerPacket(SMSG_PARTY_MEMBER_FULL_STATE, 80) { }

            WorldPacket const* Write() override;
            void Initialize(Player const* player);

            bool ForEnemy = false;
            ObjectGuid MemberGuid;
            PartyMemberStats MemberStats;
        };

        class SetPartyLeader final : public ClientPacket
        {
        public:
            explicit SetPartyLeader(WorldPacket&& packet) : ClientPacket(CMSG_SET_PARTY_LEADER, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            ObjectGuid TargetGUID;
        };

        class SetRole final : public ClientPacket
        {
        public:
            explicit SetRole(WorldPacket&& packet) : ClientPacket(CMSG_SET_ROLE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            ObjectGuid TargetGUID;
            uint8 Role = 0;
        };

        class RoleChangedInform final : public ServerPacket
        {
        public:
            explicit RoleChangedInform() : ServerPacket(SMSG_ROLE_CHANGED_INFORM, 41) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;
            ObjectGuid From;
            ObjectGuid ChangedUnit;
            uint8 OldRole = 0;
            uint8 NewRole = 0;
        };

        class LeaveGroup final : public ClientPacket
        {
        public:
            explicit LeaveGroup(WorldPacket&& packet) : ClientPacket(CMSG_LEAVE_GROUP, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
        };

        class SetLootMethod final : public ClientPacket
        {
        public:
            explicit SetLootMethod(WorldPacket&& packet) : ClientPacket(CMSG_SET_LOOT_METHOD, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            ObjectGuid LootMasterGUID;
            uint8 LootMethod = 0u;
            uint32 LootThreshold = 0u;
        };

        class MinimapPingClient final : public ClientPacket
        {
        public:
            explicit MinimapPingClient(WorldPacket&& packet) : ClientPacket(CMSG_MINIMAP_PING, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            float PositionX = 0.f;
            float PositionY = 0.f;
        };

        class MinimapPing final : public ServerPacket
        {
        public:
            explicit MinimapPing() : ServerPacket(SMSG_MINIMAP_PING, 24) { }

            WorldPacket const* Write() override;

            ObjectGuid Sender;
            float PositionX = 0.f;
            float PositionY = 0.f;
        };

        class UpdateRaidTarget final : public ClientPacket
        {
        public:
            explicit UpdateRaidTarget(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_RAID_TARGET, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            ObjectGuid Target;
            int8 Symbol = 0;
        };

        class SendRaidTargetUpdateSingle final : public ServerPacket
        {
        public:
            explicit SendRaidTargetUpdateSingle() : ServerPacket(SMSG_SEND_RAID_TARGET_UPDATE_SINGLE, 34) { }

            WorldPacket const* Write() override;

            int8 PartyIndex = 0;
            ObjectGuid Target;
            ObjectGuid ChangedBy;
            int8 Symbol = 0;
        };

        class SendRaidTargetUpdateAll final : public ServerPacket
        {
        public:
            explicit SendRaidTargetUpdateAll() : ServerPacket(SMSG_SEND_RAID_TARGET_UPDATE_ALL, 1 + 8 * (1 + 16)) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;
            std::vector<std::pair<uint8, ObjectGuid>> TargetIcons;
        };

        class ConvertRaid final : public ClientPacket
        {
        public:
            explicit ConvertRaid(WorldPacket&& packet) : ClientPacket(CMSG_CONVERT_RAID, std::move(packet)) { }

            void Read() override;

            bool Raid = false;
        };

        class RequestPartyJoinUpdates final : public ClientPacket
        {
        public:
            explicit RequestPartyJoinUpdates(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_PARTY_JOIN_UPDATES, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
        };

        class SetAssistantLeader final : public ClientPacket
        {
        public:
            explicit SetAssistantLeader(WorldPacket&& packet) : ClientPacket(CMSG_SET_ASSISTANT_LEADER, std::move(packet)) { }

            void Read() override;

            ObjectGuid Target;
            Optional<uint8> PartyIndex;
            bool Apply = false;
        };

        class SetPartyAssignment final : public ClientPacket
        {
        public:
            explicit SetPartyAssignment(WorldPacket&& packet) : ClientPacket(CMSG_SET_PARTY_ASSIGNMENT, std::move(packet)) { }

            void Read() override;
            int32 Assignment = 0;
            Optional<uint8> PartyIndex;
            ObjectGuid Target;
            bool Set = false;
        };

        class DoReadyCheck final : public ClientPacket
        {
        public:
            explicit DoReadyCheck(WorldPacket&& packet) : ClientPacket(CMSG_DO_READY_CHECK, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
        };

        class ReadyCheckStarted final : public ServerPacket
        {
        public:
            explicit ReadyCheckStarted() : ServerPacket(SMSG_READY_CHECK_STARTED, 37) { }

            WorldPacket const* Write() override;

            int8 PartyIndex = 0;
            ObjectGuid PartyGUID;
            ObjectGuid InitiatorGUID;
            WorldPackets::Duration<Milliseconds> Duration;
        };

        class ReadyCheckResponseClient final : public ClientPacket
        {
        public:
            explicit ReadyCheckResponseClient(WorldPacket&& packet) : ClientPacket(CMSG_READY_CHECK_RESPONSE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool IsReady = false;
        };

        class ReadyCheckResponse final : public ServerPacket
        {
        public:
            explicit ReadyCheckResponse() : ServerPacket(SMSG_READY_CHECK_RESPONSE, 19) { }

            WorldPacket const* Write() override;

            ObjectGuid PartyGUID;
            ObjectGuid Player;
            bool IsReady = false;
        };

        class ReadyCheckCompleted final : public ServerPacket
        {
        public:
            explicit ReadyCheckCompleted() : ServerPacket(SMSG_READY_CHECK_COMPLETED, 17) { }

            WorldPacket const* Write() override;

            int8 PartyIndex = 0;
            ObjectGuid PartyGUID;
        };

        class RequestRaidInfo final : public ClientPacket
        {
        public:
            explicit RequestRaidInfo(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_RAID_INFO, std::move(packet)) { }

            void Read() override { }
        };

        class OptOutOfLoot final : public ClientPacket
        {
        public:
            explicit OptOutOfLoot(WorldPacket&& packet) : ClientPacket(CMSG_OPT_OUT_OF_LOOT, std::move(packet)) { }

            void Read() override;

            bool PassOnLoot = false;
        };

        class InitiateRolePoll final : public ClientPacket
        {
        public:
            explicit InitiateRolePoll(WorldPacket&& packet) : ClientPacket(CMSG_INITIATE_ROLE_POLL, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
        };

        class RolePollInform final : public ServerPacket
        {
        public:
            explicit RolePollInform() : ServerPacket(SMSG_ROLE_POLL_INFORM, 17) { }

            WorldPacket const* Write() override;

            int8 PartyIndex = 0;
            ObjectGuid From;
        };

        class GroupNewLeader final : public ServerPacket
        {
        public:
            explicit GroupNewLeader() : ServerPacket(SMSG_GROUP_NEW_LEADER, 14) { }

            WorldPacket const* Write() override;

            int8 PartyIndex = 0;
            std::string Name;
        };

        // Sent to every member of a group that holds a premade group finder listing when the
        // leadership changes. The client (LFGListMgr consumer at 12.1.0.69382 RVA 0x24DEB10) fires
        // LFG_GROUP_DELISTED_LEADERSHIP_CHANGE unconditionally and offers to drop the listing, and
        // additionally LFG_LIST_CENSORED_ACTIVE_ENTRY_UPDATE when NewLeaderGUID matches its own
        // reference guid. Listing name and the 60 second delist grace are produced by the client,
        // not carried on the wire.
        class PartyNotifyLFGLeaderChange final : public ServerPacket
        {
        public:
            explicit PartyNotifyLFGLeaderChange() : ServerPacket(SMSG_PARTY_NOTIFY_LFG_LEADER_CHANGE, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid NewLeaderGUID;
        };

        // Sent to the group leader when a player without invite permission asks for somebody to be
        // invited. Both strings are display text, not identifiers: the client (consumer at RVA
        // 0x1E20770) shows ERR_INFORM_SUGGEST_INVITE_SS (two names) or ERR_INFORM_SUGGEST_INVITE_S
        // (SuggesterName only) in the chat frame. An empty SuggesterName makes the client do nothing.
        class SuggestInviteInform final : public ServerPacket
        {
        public:
            explicit SuggestInviteInform() : ServerPacket(SMSG_SUGGEST_INVITE_INFORM, 3 + 12 + 12) { }

            WorldPacket const* Write() override;

            // client buffers are 306 (SuggesterName) and 310 (TargetName) bytes and the reader
            // NUL terminates in place, so both are clamped to 305 / 309 on write
            std::string SuggesterName;
            std::string TargetName;
        };

        struct LeaverInfo
        {
            ObjectGuid BnetAccountGUID;
            float LeaveScore = 0.0f;
            uint32 SeasonID = 0;
            uint32 TotalLeaves = 0;
            uint32 TotalSuccesses = 0;
            int32 ConsecutiveSuccesses = 0;
            Timestamp<> LastPenaltyTime;
            Timestamp<> LeaverExpirationTime;
            int32 Flags = 0;
            bool LeaverStatus = false;
        };

        struct PartyPlayerInfo
        {
            ObjectGuid GUID;
            std::string Name;
            std::string VoiceStateID;   // same as bgs.protocol.club.v1.MemberVoiceState.id
            LeaverInfo Leaver;
            uint8 Class = 0u;
            uint8 Subgroup = 0u;
            uint8 Flags = 0u;
            uint8 RolesAssigned = 0u;
            uint8 RolesUnk_1210 = 0u;   // forces role displayed to be tank if this field contains tank role and RolesAssigned is dps
            uint8 FactionGroup = 0u;
            bool FromSocialQueue = false;
            bool VoiceChatSilenced = false;
            bool Connected = false;
        };

        struct PartyLFGInfo
        {
            uint32 Slot = 0;
            uint8 MyFlags = 0;
            uint32 MyRandomSlot = 0;
            uint8 MyPartialClear = 0;
            float MyGearDiff = 0.0f;
            uint8 MyStrangerCount = 0;
            uint8 MyKickVoteCount = 0;
            uint8 BootCount = 0;
            bool Aborted = false;
            bool MyFirstReward = false;
        };

        struct PartyLootSettings
        {
            uint8 Method = 0u;
            ObjectGuid LootMaster;
            uint8 Threshold = 0u;
        };

        struct PartyDifficultySettings
        {
            int16 DungeonDifficultyID = 0u;
            int16 RaidDifficultyID = 0u;
            int16 LegacyRaidDifficultyID = 0u;
        };

        struct ChallengeModeData
        {
            int32 MapID = 0;
            int32 InitialPlayerCount = 0;
            uint64 InstanceID = 0;
            Timestamp<> StartTime;
            ObjectGuid KeystoneOwnerGUID;
            ObjectGuid LeaverGUID;
            Duration<Milliseconds> InstanceAbandonVoteCooldown;
            bool IsActive = false;
            bool HasRestrictions = false;
            bool CanVoteAbandon = false;
        };

        class PartyUpdate final : public ServerPacket
        {
        public:
            explicit PartyUpdate() : ServerPacket(SMSG_PARTY_UPDATE, 200) { }

            WorldPacket const* Write() override;

            uint16 PartyFlags = 0;
            uint8 PartyIndex = 0;
            uint8 PartyType = 0;

            ObjectGuid PartyGUID;
            ObjectGuid LeaderGUID;
            uint8 LeaderFactionGroup = 0;

            int32 MyIndex = 0;
            int32 SequenceNum = 0;

            RestrictPingsTo PingRestriction = { };

            std::vector<PartyPlayerInfo> PlayerList;

            Optional<ChallengeModeData> ChallengeMode;
            Optional<PartyLFGInfo> LfgInfos;
            Optional<PartyLootSettings> LootSettings;
            Optional<PartyDifficultySettings> DifficultySettings;
        };

        class SetEveryoneIsAssistant final : public ClientPacket
        {
        public:
            explicit SetEveryoneIsAssistant(WorldPacket&& packet) : ClientPacket(CMSG_SET_EVERYONE_IS_ASSISTANT, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool EveryoneIsAssistant = false;
        };

        class ChangeSubGroup final : public ClientPacket
        {
        public:
            explicit ChangeSubGroup(WorldPacket&& packet) : ClientPacket(CMSG_CHANGE_SUB_GROUP, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
            Optional<uint8> PartyIndex;
            uint8 NewSubGroup = 0u;
        };

        class SwapSubGroups final : public ClientPacket
        {
        public:
            explicit SwapSubGroups(WorldPacket&& packet) : ClientPacket(CMSG_SWAP_SUB_GROUPS, std::move(packet)) { }

            void Read() override;

            ObjectGuid FirstTarget;
            ObjectGuid SecondTarget;
            Optional<uint8> PartyIndex;
        };

        class ClearRaidMarker final : public ClientPacket
        {
        public:
            explicit ClearRaidMarker(WorldPacket&& packet) : ClientPacket(CMSG_CLEAR_RAID_MARKER, std::move(packet)) { }

            void Read() override;

            uint8 MarkerId = 0u;
        };

        class RaidMarkersChanged final : public ServerPacket
        {
        public:
            explicit RaidMarkersChanged() : ServerPacket(SMSG_RAID_MARKERS_CHANGED, 6) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;
            uint32 ActiveMarkers = 0u;

            std::vector<RaidMarker const*> RaidMarkers;
        };

        class PartyKillLog final : public ServerPacket
        {
        public:
            explicit PartyKillLog() : ServerPacket(SMSG_PARTY_KILL_LOG, 2 * 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            ObjectGuid Victim;
        };

        class GroupDestroyed final : public ServerPacket
        {
        public:
            explicit GroupDestroyed() : ServerPacket(SMSG_GROUP_DESTROYED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // Sent to the player the server removed from a group on its own initiative (no kicker).
        // Carries no payload at all - the client consumer (RVA 0x1E20110) is two instructions and
        // unconditionally shows ERR_UNINVITE_YOU ("you have been removed"). It therefore goes to the
        // removed player only, never to the remaining group.
        //
        // The wire (0 bytes) and the effect above are measured at build 12.1.0.69382. UNVERIFIED:
        // when retail sends it. No recording of this opcode exists, and SMSG_GROUP_UNINVITE would
        // fit a server initiated removal just as well, so the trigger chosen in Group::RemoveMember
        // is a decision of this branch, not a reconstruction of retail (brief 7.2, 11.1 O-B).
        class GroupAutoKick final : public ServerPacket
        {
        public:
            explicit GroupAutoKick() : ServerPacket(SMSG_GROUP_AUTO_KICK, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class BroadcastSummonCast final : public ServerPacket
        {
        public:
            explicit BroadcastSummonCast() : ServerPacket(SMSG_BROADCAST_SUMMON_CAST, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Target;
        };

        class BroadcastSummonResponse final : public ServerPacket
        {
        public:
            explicit BroadcastSummonResponse() : ServerPacket(SMSG_BROADCAST_SUMMON_RESPONSE, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Target;
            bool Accepted = false;
        };

        // Values of SummonRaidMemberValidateEntry::Reason. Taken from the comparison chain of the
        // client consumer at 12.1.0.69382 RVA 0x1E2E860, which turns each value into a GlobalStrings
        // lookup and one chat line per player. There is no Lua enum for this - the names below are
        // the GlobalStrings keys themselves.
        enum class SummonRaidMemberValidateReason : uint8
        {
            None            = 0,    // consumer falls through - produces no output at all, never send this
            RealmMismatch   = 1,    // RAID_SUMMON_FAILED_REALM_MISMATCH
            RaidLocked      = 2,    // RAID_SUMMON_FAILED_RAID_LOCKED
            MapCondition    = 3,    // RAID_SUMMON_FAILED_MAP_CONDITION
            DeadOrGhost     = 4,    // RAID_SUMMON_FAILED_DEAD_OR_GHOST
            Offline         = 5     // RAID_SUMMON_FAILED_OFFLINE
        };

        struct SummonRaidMemberValidateEntry
        {
            ObjectGuid PlayerGUID;
            SummonRaidMemberValidateReason Reason = SummonRaidMemberValidateReason::None;
        };

        // Sent to the summoner listing, per target player, why that player is not being summoned.
        // The client resolves every PlayerGUID through its name cache and prints one chat line each;
        // a guid the receiver cannot resolve stays silent, and so does Reason::None.
        class SummonRaidMemberValidateFailed final : public ServerPacket
        {
        public:
            explicit SummonRaidMemberValidateFailed() : ServerPacket(SMSG_SUMMON_RAID_MEMBER_VALIDATE_FAILED, 4 + 17) { }

            WorldPacket const* Write() override;

            std::vector<SummonRaidMemberValidateEntry> Entries;
        };

        class SetRestrictPingsToAssistants final : public ClientPacket
        {
        public:
            explicit SetRestrictPingsToAssistants(WorldPacket&& packet) : ClientPacket(CMSG_SET_RESTRICT_PINGS_TO_ASSISTANTS, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            RestrictPingsTo RestrictTo = { };
        };

        class SendPingUnit final : public ClientPacket
        {
        public:
            explicit SendPingUnit(WorldPacket&& packet) : ClientPacket(CMSG_SEND_PING_UNIT, std::move(packet)) { }

            void Read() override;

            ObjectGuid SenderGUID;
            ObjectGuid TargetGUID;
            PingSubjectType Type = { };
            uint32 PinFrameID = 0;
            Duration<Milliseconds, int32> PingDuration;
            float Health = 1.0f; // range 0-1
            float Mana = 1.0f; // range 0-1
            bool IsUnitFrameStatusTextPing = false; // prints health (and mana if healer) in chat
            Optional<uint32> CreatureID;
            Optional<uint32> SpellOverrideNameID;
        };

        class ReceivePingUnit final : public ServerPacket
        {
        public:
            explicit ReceivePingUnit() : ServerPacket(SMSG_RECEIVE_PING_UNIT, 16 + 16 + 1 + 4 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid SenderGUID;
            ObjectGuid TargetGUID;
            PingSubjectType Type = { };
            uint32 PinFrameID = 0;
            Duration<Milliseconds, int32> PingDuration;
            float Health = 1.0f; // range 0-1
            float Mana = 1.0f; // range 0-1
            bool IsUnitFrameStatusTextPing = false; // prints health (and mana if healer) in chat
            Optional<uint32> CreatureID;
            Optional<uint32> SpellOverrideNameID;
        };

        class SendPingWorldPoint final : public ClientPacket
        {
        public:
            explicit SendPingWorldPoint(WorldPacket&& packet) : ClientPacket(CMSG_SEND_PING_WORLD_POINT, std::move(packet)) { }

            void Read() override;

            ObjectGuid SenderGUID;
            uint32 MapID = 0;
            TaggedPosition<Position::XYZ> Point;
            PingSubjectType Type = { };
            uint32 PinFrameID = 0;
            ObjectGuid Transport;
            Duration<Milliseconds, int32> PingDuration;
        };

        class ReceivePingWorldPoint final : public ServerPacket
        {
        public:
            explicit ReceivePingWorldPoint() : ServerPacket(SMSG_RECEIVE_PING_WORLD_POINT, 16 + 4 + 4 * 3 + 1 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid SenderGUID;
            uint32 MapID = 0;
            TaggedPosition<Position::XYZ> Point;
            PingSubjectType Type = { };
            uint32 PinFrameID = 0;
            Duration<Milliseconds, int32> PingDuration;
            ObjectGuid Transport;
        };

        class SendPingCooldown final : public ClientPacket
        {
        public:
            explicit SendPingCooldown(WorldPacket&& packet) : ClientPacket(CMSG_SEND_PING_COOLDOWN, std::move(packet)) { }

            void Read() override;

            ObjectGuid SenderGUID;
            uint32 PinFrameID = 0;
            uint32 SpellID = 0;
            uint32 ItemID = 0;
            WorldPackets::Duration<Milliseconds, int32> Duration;
            WorldPackets::Duration<Milliseconds, int32> Remaining;
            PingSubjectType Type = { };
            uint32 SpellCategoryID = 0;
        };

        class ReceivePingCooldown final : public ServerPacket
        {
        public:
            explicit ReceivePingCooldown() : ServerPacket(SMSG_RECEIVE_PING_COOLDOWN, 16 + 4 + 4 + 4 + 4 + 4 + 1 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid SenderGUID;
            uint32 PinFrameID = 0;
            uint32 SpellID = 0;
            uint32 ItemID = 0;
            WorldPackets::Duration<Milliseconds, int32> Duration;
            WorldPackets::Duration<Milliseconds, int32> Remaining;
            PingSubjectType Type = { };
            uint32 SpellCategoryID = 0;
        };

        class CancelPingPin final : public ServerPacket
        {
        public:
            explicit CancelPingPin() : ServerPacket(SMSG_CANCEL_PING_PIN, 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid SenderGUID;
            uint32 PinFrameID = 0;
        };
    }
}

#endif // TRINITYCORE_PARTY_PACKETS_H

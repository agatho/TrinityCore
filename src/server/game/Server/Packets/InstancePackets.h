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

#ifndef TRINITYCORE_INSTANCE_PACKETS_H
#define TRINITYCORE_INSTANCE_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"

namespace WorldPackets
{
    namespace Instance
    {
        class UpdateLastInstance final : public ServerPacket
        {
        public:
            explicit UpdateLastInstance() : ServerPacket(SMSG_UPDATE_LAST_INSTANCE, 4) { }

            WorldPacket const* Write() override;

            uint32 MapID = 0;
        };

        // This packet is no longer sent - it is only here for documentation purposes
        class UpdateInstanceOwnership final : public ServerPacket
        {
        public:
            explicit UpdateInstanceOwnership() : ServerPacket(SMSG_UPDATE_INSTANCE_OWNERSHIP, 4) { }

            WorldPacket const* Write() override;

            int32 IOwnInstance = 0; // Used to control whether "Reset all instances" button appears on the UI - Script_CanShowResetInstances()
                                    // but it has been deperecated in favor of simply checking group leader, being inside an instance or using dungeon finder
        };

        struct InstanceLock
        {
            uint64 InstanceID = 0u;
            uint32 MapID = 0u;
            uint32 DifficultyID = 0u;
            int32 TimeRemaining = 0;
            uint32 CompletedMask = 0u;

            bool Locked = false;
            bool Extended = false;
        };

        class InstanceInfo final : public ServerPacket
        {
        public:
            explicit InstanceInfo() : ServerPacket(SMSG_INSTANCE_INFO, 4) { }

            WorldPacket const* Write() override;

            std::vector<InstanceLock> LockList;
        };

        class ResetInstances final : public ClientPacket
        {
        public:
            explicit ResetInstances(WorldPacket&& packet) : ClientPacket(CMSG_RESET_INSTANCES, std::move(packet)) { }

            void Read() override { }
        };

        class InstanceReset final : public ServerPacket
        {
        public:
            explicit InstanceReset() : ServerPacket(SMSG_INSTANCE_RESET, 4) { }

            WorldPacket const* Write() override;

            uint32 MapID = 0;
        };

        class InstanceResetFailed final : public ServerPacket
        {
        public:
            explicit InstanceResetFailed() : ServerPacket(SMSG_INSTANCE_RESET_FAILED, 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 MapID = 0;
            uint8 ResetFailedReason = 0;
        };

        class ResetFailedNotify final : public ServerPacket
        {
        public:
            explicit ResetFailedNotify() : ServerPacket(SMSG_RESET_FAILED_NOTIFY, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceSaveCreated final : public ServerPacket
        {
        public:
            explicit InstanceSaveCreated() : ServerPacket(SMSG_INSTANCE_SAVE_CREATED, 1) { }

            WorldPacket const* Write() override;

            bool Gm = false;
        };

        class InstanceLockResponse final : public ClientPacket
        {
        public:
            explicit InstanceLockResponse(WorldPacket&& packet) : ClientPacket(CMSG_INSTANCE_LOCK_RESPONSE, std::move(packet)) { }

            void Read() override;

            bool AcceptLock = false;
        };

        class RaidGroupOnly final : public ServerPacket
        {
        public:
            explicit RaidGroupOnly() : ServerPacket(SMSG_RAID_GROUP_ONLY, 4 + 4) { }

            WorldPacket const* Write() override;

            int32 Delay = 0;
            uint32 Reason = 0;
        };

        class PendingRaidLock final : public ServerPacket
        {
        public:
            explicit PendingRaidLock() : ServerPacket(SMSG_PENDING_RAID_LOCK, 4 + 4) { }

            WorldPacket const* Write() override;

            int32 TimeUntilLock = 0;
            uint32 CompletedMask = 0;
            bool Extending = false;
            bool WarningOnly = false;
        };

        class RaidInstanceMessage final : public ServerPacket
        {
        public:
            explicit RaidInstanceMessage() : ServerPacket(SMSG_RAID_INSTANCE_MESSAGE, 1 + 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            int32 Type = 0;
            uint32 MapID = 0;
            uint32 DifficultyID = 0;
            int32 TimeLeft = 0;
            std::string_view WarningMessage;    // GlobalStrings tag
            bool Locked = false;
            bool Extended = false;
        };

        class InstanceEncounterEngageUnit final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEngageUnit() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_ENGAGE_UNIT, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint8 TargetFramePriority = 0; // used to set the initial position of the frame if multiple frames are sent
        };

        class InstanceEncounterDisengageUnit final : public ServerPacket
        {
        public:
            explicit InstanceEncounterDisengageUnit() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_DISENGAGE_UNIT, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
        };

        class InstanceEncounterChangePriority final : public ServerPacket
        {
        public:
            explicit InstanceEncounterChangePriority() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_CHANGE_PRIORITY, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint8 TargetFramePriority = 0; // used to update the position of the unit's current frame
        };

        class InstanceEncounterTimerStart final : public ServerPacket
        {
        public:
            explicit InstanceEncounterTimerStart() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_TIMER_START, 4) { }

            WorldPacket const* Write() override;

            int32 TimeRemaining = 0;
        };

        class InstanceEncounterObjectiveStart final : public ServerPacket
        {
        public:
            explicit InstanceEncounterObjectiveStart() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_OBJECTIVE_START, 4) { }

            WorldPacket const* Write() override;

            int32 ObjectiveID = 0;
        };

        class InstanceEncounterObjectiveUpdate final : public ServerPacket
        {
        public:
            explicit InstanceEncounterObjectiveUpdate() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_OBJECTIVE_UPDATE, 4 + 4) { }

            WorldPacket const* Write() override;

            int32 ObjectiveID = 0;
            int32 ProgressAmount = 0;
        };

        class InstanceEncounterObjectiveComplete final : public ServerPacket
        {
        public:
            explicit InstanceEncounterObjectiveComplete() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_OBJECTIVE_COMPLETE, 4) { }

            WorldPacket const* Write() override;

            int32 ObjectiveID = 0;
        };

        class InstanceEncounterPhaseShiftChanged final : public ServerPacket
        {
        public:
            explicit InstanceEncounterPhaseShiftChanged() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_PHASE_SHIFT_CHANGED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceEncounterStart final : public ServerPacket
        {
        public:
            explicit InstanceEncounterStart() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_START, 16) { }

            WorldPacket const* Write() override;

            uint32 InCombatResCount = 0; // amount of usable battle ressurections
            uint32 MaxInCombatResCount = 0;
            uint32 CombatResChargeRecovery = 0;
            uint32 NextCombatResChargeTime = 0;
            bool InProgress = true;
        };

        class InstanceEncounterEnd final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEnd() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_END, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceEncounterInCombatResurrection final : public ServerPacket
        {
        public:
            explicit InstanceEncounterInCombatResurrection() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_IN_COMBAT_RESURRECTION, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceEncounterGainCombatResurrectionCharge final : public ServerPacket
        {
        public:
            explicit InstanceEncounterGainCombatResurrectionCharge() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_GAIN_COMBAT_RESURRECTION_CHARGE, 4 + 4) { }

            WorldPacket const* Write() override;

            int32 InCombatResCount = 0;
            uint32 CombatResChargeRecovery = 0;
        };

        class BossKill final : public ServerPacket
        {
        public:
            explicit BossKill() : ServerPacket(SMSG_BOSS_KILL, 4) { }

            WorldPacket const* Write() override;
            uint32 DungeonEncounterID = 0;
        };

        // ============================================================
        // Realm-wide encounter packets
        // ============================================================

        class EncounterStart final : public ServerPacket
        {
        public:
            explicit EncounterStart() : ServerPacket(SMSG_ENCOUNTER_START, 4 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 DungeonEncounterID = 0;
            uint32 DifficultyID = 0;
            uint32 GroupSize = 0;
            int32 UnkEncounterDataID = 0;
        };

        class EncounterEnd final : public ServerPacket
        {
        public:
            explicit EncounterEnd() : ServerPacket(SMSG_ENCOUNTER_END, 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 DungeonEncounterID = 0;
            uint32 DifficultyID = 0;
            uint32 GroupSize = 0;
            bool Success = false;
        };

        // ============================================================
        // Release control packets
        // ============================================================

        class InstanceEncounterUpdateAllowReleaseInProgress final : public ServerPacket
        {
        public:
            explicit InstanceEncounterUpdateAllowReleaseInProgress() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_UPDATE_ALLOW_RELEASE_IN_PROGRESS, 1) { }

            WorldPacket const* Write() override;

            bool AllowRelease = false;
        };

        class InstanceEncounterUpdateSuppressRelease final : public ServerPacket
        {
        public:
            explicit InstanceEncounterUpdateSuppressRelease() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_UPDATE_SUPPRESS_RELEASE, 1) { }

            WorldPacket const* Write() override;

            bool SuppressRelease = false;
        };

        // ============================================================
        // Instance group/difficulty packets
        // ============================================================

        class InstanceGroupSizeChanged final : public ServerPacket
        {
        public:
            explicit InstanceGroupSizeChanged() : ServerPacket(SMSG_INSTANCE_GROUP_SIZE_CHANGED, 4) { }

            WorldPacket const* Write() override;

            uint32 GroupSize = 0;
        };

        class LegacyLootRules final : public ServerPacket
        {
        public:
            explicit LegacyLootRules() : ServerPacket(SMSG_LEGACY_LOOT_RULES, 1) { }

            WorldPacket const* Write() override;

            bool LegacyRulesActive = false;
        };

        // ============================================================
        // Abandon vote packets
        // ============================================================

        class StartInstanceAbandonVote final : public ClientPacket
        {
        public:
            explicit StartInstanceAbandonVote(WorldPacket&& packet) : ClientPacket(CMSG_START_INSTANCE_ABANDON_VOTE, std::move(packet)) { }

            void Read() override { }
        };

        class InstanceAbandonVoteResponse final : public ClientPacket
        {
        public:
            explicit InstanceAbandonVoteResponse(WorldPacket&& packet) : ClientPacket(CMSG_INSTANCE_ABANDON_VOTE_RESPONSE, std::move(packet)) { }

            void Read() override;

            bool Accept = false;
        };

        class InstanceAbandonVoteStarted final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVoteStarted() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_STARTED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceAbandonVoteCompleted final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVoteCompleted() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_COMPLETED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceAbandonVotePlayerLeft final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVotePlayerLeft() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_PLAYER_LEFT, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class InstanceAbandonVoteResponseSMSG final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVoteResponseSMSG() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_RESPONSE, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // ============================================================
        // Difficulty packets
        // ============================================================

        class SetDifficultyID final : public ClientPacket
        {
        public:
            explicit SetDifficultyID(WorldPacket&& packet) : ClientPacket(CMSG_SET_DIFFICULTY_ID, std::move(packet)) { }

            void Read() override;

            uint32 DifficultyID = 0;
        };

        class ToggleDifficulty final : public ClientPacket
        {
        public:
            explicit ToggleDifficulty(WorldPacket&& packet) : ClientPacket(CMSG_TOGGLE_DIFFICULTY, std::move(packet)) { }

            void Read() override { }
        };

        class ChangePlayerDifficultyResult final : public ServerPacket
        {
        public:
            explicit ChangePlayerDifficultyResult() : ServerPacket(SMSG_CHANGE_PLAYER_DIFFICULTY_RESULT, 4) { }

            WorldPacket const* Write() override;

            uint8 Result = 0;
            // 0 = success, various error codes
        };

    }
}

#endif // TRINITYCORE_INSTANCE_PACKETS_H

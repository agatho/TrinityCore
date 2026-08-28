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
#include "Optional.h"
#include "PacketUtilities.h"
#include <vector>

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
            uint32 MapID = 0u;
            int16 DifficultyID = 0;
            uint64 InstanceID = 0u;
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
            int16 DifficultyID = 0;
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

        // Sent when a group member is inside a different instance id of the same map than the rest of the party.
        // Payload is empty - client reader 0x450020 (0x5DAD20) exposes no fields, consumer 0x209AA20 only fires
        // the argument-less lua event ENTERED_DIFFERENT_INSTANCE_FROM_PARTY (PartyInfoDocumentation.lua:635).
        class DifferentInstanceFromParty final : public ServerPacket
        {
        public:
            explicit DifferentInstanceFromParty() : ServerPacket(SMSG_DIFFERENT_INSTANCE_FROM_PARTY, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // Opens the "vote to abandon instance" dialog on every group member.
        // Client reader 0x5E4820, consumer 0x2192960 (fires INSTANCE_ABANDON_VOTE_STARTED).
        class InstanceAbandonVoteStarted final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVoteStarted() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_STARTED, 1 + 18 + 18 + 8 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;                   // UNVERIFIED: read by the client (struct +32) but never used by any consumer, no lua enum exists for it
            ObjectGuid PartyGUID;
            ObjectGuid InitiatorGUID;               // client marks this member as having voted yes, the server must count it the same way
            Duration<Milliseconds, uint64> VoteDuration; // a duration of 0 shows no dialog at all (InstanceAbandon.lua:200-201)
            uint32 VotesRequired = 0;               // UNVERIFIED: name from C_PartyInfo.GetInstanceAbandonVoteRequirements()
            uint32 KeystoneOwnerVoteWeight = 0;     // UNVERIFIED: dito
        };

        // Carries somebody else's vote. Named after the lua event it raises (INSTANCE_ABANDON_VOTE_UPDATED) because
        // the CMSG of the same opcode name is already InstanceAbandonVoteResponse.
        // Client reader 0x5E4940, consumer 0x2192A60. This is the only vote message without the leading uint8.
        class InstanceAbandonVoteUpdated final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVoteUpdated() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_RESPONSE, 18 + 18 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid PartyGUID;
            ObjectGuid VoterGUID;
            bool Accept = false;
        };

        // Closes the vote. Client reader 0x5E4A10, consumer 0x2192B20 -> 0x218F740, fires INSTANCE_ABANDON_VOTE_FINISHED(votePassed).
        class InstanceAbandonVoteCompleted final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVoteCompleted() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_COMPLETED, 1 + 18 + 8 + 8 + 1) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;                   // UNVERIFIED: read by the client (struct +32) but never used
            ObjectGuid PartyGUID;
            // UNVERIFIED: the two cooldowns are assigned by shape, not by measurement - field 3 lands in group+800/+808,
            // field 4 in group+792, and lua offers exactly the two matching getters
            // (GetInstanceAbandonVoteCooldownTime / GetInstanceAbandonShutdownTime)
            Duration<Milliseconds, uint64> VoteCooldown;
            Duration<Milliseconds, uint64> ShutdownTime;
            bool VotePassed = false;                // written last on the wire although the client stores it at +56
        };

        // Aborts a running vote because a member left the instance. Not interchangeable with a failed
        // InstanceAbandonVoteCompleted: consumer 0x2192C00 additionally stores the guid and prints
        // VOTE_TO_ABANDON_PLAYER_LEFT for everybody except the player who left.
        class InstanceAbandonVotePlayerLeft final : public ServerPacket
        {
        public:
            explicit InstanceAbandonVotePlayerLeft() : ServerPacket(SMSG_INSTANCE_ABANDON_VOTE_PLAYER_LEFT, 1 + 18 + 18) { }

            WorldPacket const* Write() override;

            uint8 PartyIndex = 0;                   // UNVERIFIED: read by the client (struct +32) but never used
            ObjectGuid PartyGUID;
            ObjectGuid PlayerGUID;
        };

        // Client reader 0x5F5B00 uses the opaque message class, consumer 0x2098A20 reads *(uint32*)blob into the
        // global 0x6808EE8 and fires INSTANCE_GROUP_SIZE_CHANGED. The client validates nothing.
        class InstanceGroupSizeChanged final : public ServerPacket
        {
        public:
            explicit InstanceGroupSizeChanged() : ServerPacket(SMSG_INSTANCE_GROUP_SIZE_CHANGED, 4) { }

            WorldPacket const* Write() override;

            uint32 GroupSize = 0;
        };

        // Flags the player as a mythic+ deserter. Payload is empty - consumer 0x24B6050 sets the state behind
        // C_InstanceLeaver.IsPlayerLeaver() and fires INSTANCE_LEAVER_STATUS_CHANGED(true).
        class SetInstanceLeaver final : public ServerPacket
        {
        public:
            explicit SetInstanceLeaver() : ServerPacket(SMSG_SET_INSTANCE_LEAVER, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // Clears the deserter flag. Payload is empty - 33 of 33 captured packets are exactly 0 bytes.
        // Consumer 0x24B5FD0, fires INSTANCE_LEAVER_STATUS_CHANGED(false).
        class UnsetInstanceLeaver final : public ServerPacket
        {
        public:
            explicit UnsetInstanceLeaver() : ServerPacket(SMSG_UNSET_INSTANCE_LEAVER, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        struct EncounterTimelineEventDelay
        {
            Duration<Milliseconds, uint32> Delay;   // remaining time from the moment the packet is received, not an absolute point in time
            bool IsApproximation = false;
        };

        // ClientEncounterEvent, the element type shared by the whole SMSG_INSTANCE_ENCOUNTER_EVENT_* block.
        // Element reader 0x65E9B0. Field order is the 12.1.0.69382 one - it differs from 12.0.x, where the
        // Paused/IsBlockedByCondition bit byte came before the delay array instead of after it.
        struct EncounterTimelineEvent
        {
            uint8 Unused0 = 1;                      // UNVERIFIED: capture only ever shows 1 and 2, no consumer reads it
            uint32 EventInstanceID = 0;             // runtime id assigned by the server, hash key on the client. 0 is reserved (ENCOUNTER_TIMELINE_INVALID_EVENT)
            uint32 EventID = 0;                     // EncounterEvent.ID - the static identifier, looked up by C_EncounterEvents.GetEventInfo
            uint32 SpellID = 0;
            uint32 Flags = 0;                       // bit 0 skips the element (test byte [rbx+0x28],1 in 0x2106960)
            uint32 BroadcastTextID = 0;             // UNVERIFIED: only the value range matches, no consumer resolves it
            uint32 Unused25 = 0;                    // UNVERIFIED: 0 in the whole capture corpus
            uint32 IconFileID = 0;                  // 0 is legal, the client then falls back to the spell icon
            ObjectGuid CasterGUID;
            uint32 TimeQueuedMS = 0;                // UNVERIFIED: milliseconds, but no consumer was found that
                                                    // reads it, so the epoch it counts from is a guess. Filled
                                                    // from the server's monotonic clock (host start), truncated
                                                    // to 32 bits and therefore wrapping after ~49.7 days
            uint32 Icons = 0;                       // Enum.EncounterEventIconmask
            WorldPackets::Duration<Milliseconds, uint32> Duration;  // qualified: the member name hides the template
            WorldPackets::Duration<Milliseconds, uint32> MaxQueueDuration; // measured as Delay + Duration in 64 of 64 captured events
            uint8 Severity = 0;                     // Enum.EncounterEventSeverity
            std::vector<EncounterTimelineEventDelay> Delays;
            bool Paused = false;
            bool IsBlockedByCondition = false;
        };

        // Replaces the client's timeline with the given list. Handler 0x2105360, mode 0.
        class InstanceEncounterEventSequence final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventSequence() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_SEQUENCE, 4) { }

            WorldPacket const* Write() override;

            std::vector<EncounterTimelineEvent> Events;
        };

        // Late-joiner resynchronisation. Handler 0x21053C0 drops the message unless EncounterGUID matches the
        // current encounter, then converts every delay against its own clock - so the delays must be sent as
        // remaining time, not as absolute time.
        class InstanceEncounterTimelineSync final : public ServerPacket
        {
        public:
            explicit InstanceEncounterTimelineSync() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_TIMELINE_SYNC, 4 + 18 + 4) { }

            WorldPacket const* Write() override;

            std::vector<EncounterTimelineEvent> Events;
            ObjectGuid EncounterGUID;
            uint32 Unused = 0;                      // UNVERIFIED: read into struct +72 and never used by the handler
        };

        // Appends to the client's timeline. Handler 0x2105390, mode 1.
        class InstanceEncounterEventAppend final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventAppend() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_APPEND, 4) { }

            WorldPacket const* Write() override;

            std::vector<EncounterTimelineEvent> Events;
        };

        // Shares handler 0x2105360 with InstanceEncounterEventSequence - for the client the two are identical,
        // the distinction is purely server side (respawn timers instead of the in-encounter timeline).
        class InstanceEncounterEventRespawn final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventRespawn() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_RESPAWN, 4 + 4) { }

            WorldPacket const* Write() override;

            std::vector<EncounterTimelineEvent> Events;
            uint32 Unused = 0;                      // UNVERIFIED: read into struct +56 and never used by the handler
        };

        // Toggles IsBlockedByCondition on a single already known event. Handler 0x2105980 looks the event up by
        // EventID *and* CasterGUID - not by EventInstanceID - and silently drops the message if neither matches.
        // Everything else in the payload is ignored, but it still has to be complete and correctly sized.
        class InstanceEncounterEventBlockedChanged final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventBlockedChanged() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_BLOCKED_CHANGED, 72) { }

            WorldPacket const* Write() override;

            EncounterTimelineEvent Event;
        };

        // Not a ClientEncounterEvent - this message has its own fixed field order. Client reader 0x5FD9C0,
        // handler 0x2105A60 writes Paused / IsBlockedByCondition into the tracked event.
        class InstanceEncounterEventCastUpdate final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventCastUpdate() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_CAST_UPDATE, 4 + 4 + 18 + 4 + 1 + 1 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 EventInstanceID = 0;
            uint32 EventID = 0;                     // UNVERIFIED: derived from the corpus - same values as EncounterTimelineEvent::EventID
            ObjectGuid CasterGUID;
            uint32 DungeonEncounterID = 0;
            uint8 CastState = 0;                    // the handler branches on == 1
            int8 Index = 0;                         // read signed, a negative value skips the handler's loop
            uint32 TimeQueuedMS = 0;                // UNVERIFIED: derived, matches EncounterTimelineEvent::TimeQueuedMS in shape
            Duration<Milliseconds, uint32> Delay;   // UNVERIFIED: derived, matches EncounterTimelineEventDelay::Delay in shape
            bool Unknown62 = true;                  // UNVERIFIED: 0x80 in 52 of 52 captured packets, no consumer found for struct +62
            bool Paused = false;
            bool IsBlockedByCondition = false;
        };

        // C_PartyInfo.StartInstanceAbandonVote() takes no arguments (PartyInfoDocumentation.lua:590), but the
        // PartyIndex is not a Lua argument - the client takes it from the party context, exactly as it does for the
        // equally argument-less C_PartyInfo.DoReadyCheck(). Client serializer sub_7FF781675A80 (opcode immediate
        // 4391008 = 0x430060) is instruction-for-instruction the same function as CMSG_DO_READY_CHECK's
        // sub_7FF781675940: one presence bit, bit flush, then the uint8 only if the bit was set.
        class StartInstanceAbandonVote final : public ClientPacket
        {
        public:
            explicit StartInstanceAbandonVote(WorldPacket&& packet) : ClientPacket(CMSG_START_INSTANCE_ABANDON_VOTE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
        };

        // C_PartyInfo.SetInstanceAbandonVoteResponse(response) takes exactly one bool (PartyInfoDocumentation.lua:550);
        // the PartyIndex again comes from the party context, not from Lua. Client serializer sub_7FF781675B00 (opcode
        // immediate 4391009 = 0x430061) is the same function as CMSG_READY_CHECK_RESPONSE's sub_7FF7816759C0:
        // presence bit, response bit, bit flush, then the uint8 only if the presence bit was set.
        class InstanceAbandonVoteResponse final : public ClientPacket
        {
        public:
            explicit InstanceAbandonVoteResponse(WorldPacket&& packet) : ClientPacket(CMSG_INSTANCE_ABANDON_VOTE_RESPONSE, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool Accept = false;
        };
    }
}

#endif // TRINITYCORE_INSTANCE_PACKETS_H

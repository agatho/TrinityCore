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

#include "InstancePackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Instance
{
WorldPacket const* UpdateLastInstance::Write()
{
    _worldPacket << uint32(MapID);

    return &_worldPacket;
}

WorldPacket const* UpdateInstanceOwnership::Write()
{
    _worldPacket << int32(IOwnInstance);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, InstanceLock const& lockInfos)
{
    data << uint32(lockInfos.MapID);
    data << int16(lockInfos.DifficultyID);
    data << uint64(lockInfos.InstanceID);
    data << int32(lockInfos.TimeRemaining);
    data << uint32(lockInfos.CompletedMask);

    data << Bits<1>(lockInfos.Locked);
    data << Bits<1>(lockInfos.Extended);

    data.FlushBits();

    return data;
}

WorldPacket const* InstanceInfo::Write()
{
    _worldPacket << Size<int32>(LockList);

    for (InstanceLock const& instanceLock : LockList)
        _worldPacket << instanceLock;

    return &_worldPacket;
}

WorldPacket const* InstanceReset::Write()
{
    _worldPacket << uint32(MapID);

    return &_worldPacket;
}

WorldPacket const* InstanceResetFailed::Write()
{
    _worldPacket << uint32(MapID);
    _worldPacket << Bits<2>(ResetFailedReason);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InstanceSaveCreated::Write()
{
    _worldPacket << Bits<1>(Gm);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void InstanceLockResponse::Read()
{
    _worldPacket >> Bits<1>(AcceptLock);
}

WorldPacket const* RaidGroupOnly::Write()
{
    _worldPacket << Delay;
    _worldPacket << Reason;

    return &_worldPacket;
}

WorldPacket const* PendingRaidLock::Write()
{
    _worldPacket << int32(TimeUntilLock);
    _worldPacket << uint32(CompletedMask);
    _worldPacket << Bits<1>(Extending);
    _worldPacket << Bits<1>(WarningOnly);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* RaidInstanceMessage::Write()
{
    _worldPacket << int32(Type);
    _worldPacket << uint32(MapID);
    _worldPacket << int16(DifficultyID);
    _worldPacket << int32(TimeLeft);
    _worldPacket << SizedString::BitsSize<8>(WarningMessage);
    _worldPacket << Bits<1>(Locked);
    _worldPacket << Bits<1>(Extended);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(WarningMessage);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEngageUnit::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint8(TargetFramePriority);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterDisengageUnit::Write()
{
    _worldPacket << Unit;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterChangePriority::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint8(TargetFramePriority);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterTimerStart::Write()
{
    _worldPacket << int32(TimeRemaining);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterObjectiveStart::Write()
{
    _worldPacket << int32(ObjectiveID);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterObjectiveUpdate::Write()
{
    _worldPacket << int32(ObjectiveID);
    _worldPacket << int32(ProgressAmount);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterObjectiveComplete::Write()
{
    _worldPacket << int32(ObjectiveID);

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterStart::Write()
{
    _worldPacket << uint32(InCombatResCount);
    _worldPacket << uint32(MaxInCombatResCount);
    _worldPacket << uint32(CombatResChargeRecovery);
    _worldPacket << uint32(NextCombatResChargeTime);
    _worldPacket << Bits<1>(InProgress);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterGainCombatResurrectionCharge::Write()
{
    _worldPacket << int32(InCombatResCount);
    _worldPacket << uint32(CombatResChargeRecovery);

    return &_worldPacket;
}

WorldPacket const* BossKill::Write()
{
    _worldPacket << uint32(DungeonEncounterID);

    return &_worldPacket;
}

WorldPacket const* InstanceAbandonVoteStarted::Write()
{
    _worldPacket << uint8(PartyIndex);
    _worldPacket << PartyGUID;
    _worldPacket << InitiatorGUID;
    _worldPacket << VoteDuration;
    _worldPacket << uint32(VotesRequired);
    _worldPacket << uint32(KeystoneOwnerVoteWeight);

    return &_worldPacket;
}

WorldPacket const* InstanceAbandonVoteUpdated::Write()
{
    _worldPacket << PartyGUID;
    _worldPacket << VoterGUID;
    _worldPacket << Bits<1>(Accept);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InstanceAbandonVoteCompleted::Write()
{
    _worldPacket << uint8(PartyIndex);
    _worldPacket << PartyGUID;
    _worldPacket << VoteCooldown;
    _worldPacket << ShutdownTime;
    _worldPacket << Bits<1>(VotePassed);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InstanceAbandonVotePlayerLeft::Write()
{
    _worldPacket << uint8(PartyIndex);
    _worldPacket << PartyGUID;
    _worldPacket << PlayerGUID;

    return &_worldPacket;
}

WorldPacket const* InstanceGroupSizeChanged::Write()
{
    _worldPacket << uint32(GroupSize);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, EncounterTimelineEvent const& encounterEvent)
{
    data << uint8(encounterEvent.Unused0);
    data << uint32(encounterEvent.EventInstanceID);
    data << uint32(encounterEvent.EventID);
    data << uint32(encounterEvent.SpellID);
    data << Size<uint32>(encounterEvent.Delays);
    data << uint32(encounterEvent.Flags);
    data << uint32(encounterEvent.BroadcastTextID);
    data << uint32(encounterEvent.Unused25);
    data << uint32(encounterEvent.IconFileID);
    data << encounterEvent.CasterGUID;
    data << uint32(encounterEvent.TimeQueuedMS);
    data << uint32(encounterEvent.Icons);
    data << encounterEvent.Duration;
    data << encounterEvent.MaxQueueDuration;
    data << uint8(encounterEvent.Severity);

    // deferred array payload - in 12.1 it precedes the trailing bit section, in 12.0.x it followed it
    for (EncounterTimelineEventDelay const& delay : encounterEvent.Delays)
    {
        data << delay.Delay;
        data << Bits<1>(delay.IsApproximation);
        data.FlushBits();
    }

    data << Bits<1>(encounterEvent.Paused);
    data << Bits<1>(encounterEvent.IsBlockedByCondition);
    data.FlushBits();

    return data;
}

WorldPacket const* InstanceEncounterEventSequence::Write()
{
    _worldPacket << Size<uint32>(Events);

    for (EncounterTimelineEvent const& encounterEvent : Events)
        _worldPacket << encounterEvent;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterTimelineSync::Write()
{
    _worldPacket << Size<uint32>(Events);
    _worldPacket << EncounterGUID;
    _worldPacket << uint32(Unused);

    for (EncounterTimelineEvent const& encounterEvent : Events)
        _worldPacket << encounterEvent;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEventAppend::Write()
{
    _worldPacket << Size<uint32>(Events);

    for (EncounterTimelineEvent const& encounterEvent : Events)
        _worldPacket << encounterEvent;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEventRespawn::Write()
{
    _worldPacket << Size<uint32>(Events);
    _worldPacket << uint32(Unused);

    for (EncounterTimelineEvent const& encounterEvent : Events)
        _worldPacket << encounterEvent;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEventBlockedChanged::Write()
{
    _worldPacket << Event;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEventCastUpdate::Write()
{
    _worldPacket << uint32(EventInstanceID);
    _worldPacket << uint32(EventID);
    _worldPacket << CasterGUID;
    _worldPacket << uint32(DungeonEncounterID);
    _worldPacket << uint8(CastState);
    _worldPacket << int8(Index);
    _worldPacket << uint32(TimeQueuedMS);
    _worldPacket << Delay;
    _worldPacket << Bits<1>(Unknown62);
    _worldPacket << Bits<1>(Paused);
    _worldPacket << Bits<1>(IsBlockedByCondition);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void InstanceAbandonVoteResponse::Read()
{
    _worldPacket >> Bits<1>(Accept);
}
}

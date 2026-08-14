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

WorldPacket const* EncounterStart::Write()
{
    _worldPacket << uint32(DungeonEncounterID);
    _worldPacket << uint16(DifficultyID);
    _worldPacket << uint32(GroupSize);
    _worldPacket << uint32(0);      // trailing array count; empty in every capture, element layout unknown

    return &_worldPacket;
}

WorldPacket const* EncounterEnd::Write()
{
    _worldPacket << uint32(DungeonEncounterID);
    _worldPacket << uint16(DifficultyID);
    _worldPacket << uint32(GroupSize);
    _worldPacket << uint32(DurationMs);
    _worldPacket << Bits<1>(Success);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

// Wire order is the read order of the shared element parser sub_7FF7291132C0 - see the long comment
// on EncounterTimelineEvent in InstancePackets.h. Note that the nested cast list contributes its COUNT
// up front, between SpellID and Unused_28, and its entries only at the very end of the element.
ByteBuffer& operator<<(ByteBuffer& data, EncounterTimelineEvent const& timelineEvent)
{
    data << uint8(timelineEvent.Severity);
    data << uint32(timelineEvent.EventID);
    data << uint32(timelineEvent.EncounterEventID);
    data << int32(timelineEvent.SpellID);
    data << uint32(timelineEvent.Casts.size());
    data << uint32(timelineEvent.Unused_28);
    data << uint32(timelineEvent.Unknown_2C);
    data << uint32(timelineEvent.Unused_30);
    data << int32(timelineEvent.IconFileID);
    data << timelineEvent.Caster;
    data << uint32(timelineEvent.Timestamp);
    data << uint32(timelineEvent.Unknown_54);
    data << uint32(timelineEvent.MaxQueueDuration);
    data << uint32(timelineEvent.Duration);
    data << uint8(timelineEvent.CastState);
    data << Bits<1>(timelineEvent.UnkBit7);
    data << Bits<1>(timelineEvent.UnkBit6);
    data.FlushBits();

    for (EncounterTimelineCast const& cast : timelineEvent.Casts)
    {
        data << uint32(cast.TimeToCastMs);
        data << Bits<1>(cast.UnkBit7);
        data.FlushBits();
    }

    return data;
}

WorldPacket const* InstanceEncounterEventSequence::Write()
{
    _worldPacket << uint32(Events.size());
    for (EncounterTimelineEvent const& timelineEvent : Events)
        _worldPacket << timelineEvent;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEventAppend::Write()
{
    _worldPacket << uint32(Events.size());
    for (EncounterTimelineEvent const& timelineEvent : Events)
        _worldPacket << timelineEvent;

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterEventCastUpdate::Write()
{
    _worldPacket << uint32(EventID);
    _worldPacket << uint32(EncounterEventID);
    _worldPacket << Caster;
    _worldPacket << uint32(DungeonEncounterID);
    _worldPacket << uint8(CastState);
    _worldPacket << uint8(Unknown_3D);
    _worldPacket << uint32(Timestamp);
    _worldPacket << uint32(TimeToCastMs);
    _worldPacket << Bits<1>(UnkBit7);
    _worldPacket << Bits<1>(UnkBit6);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void RequestInstanceEncounterEventSync::Read()
{
    _worldPacket >> Unit;
}

WorldPacket const* InstanceEncounterUpdateAllowReleaseInProgress::Write()
{
    _worldPacket << Bits<1>(AllowRelease);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InstanceEncounterUpdateSuppressRelease::Write()
{
    _worldPacket << Bits<1>(SuppressRelease);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InstanceGroupSizeChanged::Write()
{
    _worldPacket << uint32(GroupSize);

    return &_worldPacket;
}

WorldPacket const* LegacyLootRules::Write()
{
    _worldPacket << Bits<1>(LegacyRulesActive);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void InstanceAbandonVoteResponse::Read()
{
    _worldPacket >> Bits<1>(Accept);
}

void SetDifficultyID::Read()
{
    _worldPacket >> DifficultyID;
}

WorldPacket const* ChangePlayerDifficultyResult::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}
}

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

        // ============================================================
        // Realm-wide encounter packets
        // ============================================================

        // SMSG_ENCOUNTER_START (0x420226) = 14 bytes:
        //   uint32 DungeonEncounterID, uint16 DifficultyID, uint32 GroupSize, uint32 <trailing array count>.
        //
        // DifficultyID is a uint16, NOT a uint32, and the packet ends with an array count rather than a
        // data id. Client parser RVA 0x608700 reads uint32 / uint16 (helper 0x33CC3C0) / uint32 / uint32,
        // and four captures in C:\sniff\m+ run12.0.7.pkt are each exactly 14 bytes, e.g.
        //   03 0A 00 00 | 08 00 | 05 00 00 00 | 00 00 00 00
        //   = encounter 2563, difficulty 8 (Mythic Keystone), group 5, count 0.
        //
        // This was previously written as four uint32s (16 bytes), which shifted everything after the
        // difficulty: the client reconstructed GroupSize as (DifficultyID >> 16) | (GroupSize << 16).
        // The trailing array is always empty in every capture, so its element layout is unknown and it is
        // modelled as a bare count of 0 rather than guessed at.
        class EncounterStart final : public ServerPacket
        {
        public:
            explicit EncounterStart() : ServerPacket(SMSG_ENCOUNTER_START, 4 + 2 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 DungeonEncounterID = 0;
            uint16 DifficultyID = 0;
            uint32 GroupSize = 0;
        };

        // SMSG_ENCOUNTER_END (0x420227) = 15 bytes:
        //   uint32 DungeonEncounterID, uint16 DifficultyID, uint32 GroupSize, uint32 DurationMs,
        //   then one bit-packed byte carrying Success at 0x80.
        //
        // Client parser RVA 0x608820. Two problems with the previous version: DifficultyID was a uint32,
        // and the DurationMs field was missing entirely, so the packet was 13 bytes where the client reads
        // 15 - it under-ran the buffer and Success was read from whatever followed.
        //
        // DurationMs is the encounter's elapsed time in milliseconds. That is not a guess: across the four
        // START/END pairs in C:\sniff\m+ run12.0.7.pkt the field tracks the sniff's own tick delta between
        // the pair - 74898 vs 74894, 101076 vs 101076 (exact to the millisecond), 85874 vs 86388,
        // 115044 vs 114500.
        class EncounterEnd final : public ServerPacket
        {
        public:
            explicit EncounterEnd() : ServerPacket(SMSG_ENCOUNTER_END, 4 + 2 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 DungeonEncounterID = 0;
            uint16 DifficultyID = 0;
            uint32 GroupSize = 0;
            uint32 DurationMs = 0;
            bool Success = false;
        };

        // ============================================================
        // Encounter timeline packets (12.0 "boss ability timeline" HUD)
        // ============================================================
        //
        // The client side of this is C_EncounterTimeline (Blizzard_APIDocumentationGenerated/
        // EncounterTimelineDocumentation.lua in the 12.0.7.68275 UI source). The server pushes a set of
        // upcoming boss abilities, each with a countdown, and tells the client when one of them is
        // actually being cast.
        //
        // Every message in the family carries the same 104-byte-in-memory element, parsed by
        // sub_7FF7291132C0 (image base 0x7FF728AA0000). Disassembling that parser gives the wire order
        // with no ambiguity, because it is a flat run of reader calls:
        //
        //   byte                          -> +0x00
        //   uint32 x3                     -> +0x04 +0x08 +0x0C
        //   uint32                        -> passed to the vector-grow helper sub_7FF7291158B0(this+0x10)
        //                                    i.e. it is the COUNT of a nested list, not a scalar
        //   uint32 x4                     -> +0x28 +0x2C +0x30 +0x34
        //   ObjectGuid (packed)           -> +0x38
        //   uint32 x4                     -> +0x4C +0x54 +0x58 +0x5C
        //   byte                          -> +0x60
        //   byte, bit-unpacked            -> bit7 -> +0x48, bit6 -> +0x50
        //   then COUNT times: uint32 -> elem+0, byte bit7 -> elem+4
        //
        // That gives 67 fixed bytes (with a 16-byte packed GUID) plus 5 bytes per nested entry. It is
        // confirmed by the captures: in C:\sniff\m+ run12.0.7.pkt the 292-byte SEQUENCE is
        // 4 + 4*72 and the 217-byte SEQUENCE is 4 + 3*71, the 71 arising because that encounter's caster
        // GUID packs to 15 bytes instead of 16. Zero slack in either case.
        //
        // Field meanings, strongest evidence first:
        //
        //  Severity          (+0x00) observed 1 and 2. Named after EncounterEventSeverity
        //                    (Low=0/Medium=1/High=2) and EncounterEvent.db2's Severity column. INFERRED -
        //                    the only other single-byte enum in range would be EncounterTimelineEventSource,
        //                    which is excluded because server-pushed events are Source=Encounter=0 and this
        //                    byte is never 0.
        //  EventID           (+0x04) PROVEN. Monotonically increasing instance id (1,2,3,4 then 5,6 from
        //                    APPENDs, then 7..). CAST_UPDATE refers back to it.
        //  EncounterEventID  (+0x08) observed 278-285, stable 1:1 with (SpellID, IconFileID) across every
        //                    packet. INFERRED to be the EncounterEvent.db2 row id ("ID of the encounter
        //                    event record" - EncounterEventInfo.encounterEventID).
        //  SpellID           (+0x0C) observed 377034/376997/377004 and 388544/388567/388796/388923.
        //  Unused_28         (+0x28) 0 in all 11 observed elements.
        //  Unknown_2C        (+0x2C) 0 in most elements; 230954, 223315 and 308255 observed. NOT a
        //                    FileDataID (230954 resolves to a draenei face texture in the CASC listfile,
        //                    i.e. noise). Left unnamed.
        //  Unused_30         (+0x30) 0 in all 11 observed elements.
        //  IconFileID        (+0x34) PROVEN. All 7 distinct observed values resolve in the CASC listfile to
        //                    interface/icons/*.blp (spell_lifegivingseed, spell_nature_earthquake,
        //                    ability_smash, inv_misc_branch_01, inv_icon_wing06b,
        //                    inv_misc_raptortalon_nightmare, ability_vehicle_sonicshockwave). 7/7.
        //  Caster            (+0x38) packed GUID, HighGuid 8 (Creature) / 9 (Vehicle). Identical for every
        //                    element of a given packet.
        //  Timestamp         (+0x4C) PROVEN to be a millisecond clock: two SEQUENCE packets 59026 sniff-ms
        //                    apart differ by 59025 here, and an APPEND 9025 ms later differs by 9019.
        //                    Written as GameTime::GetGameTimeMS().
        //  Unknown_54        (+0x54) was recorded as "0 in all 11 observed elements" when only the SEQUENCE
        //                    packets of one capture had been decoded. Decoding the APPENDs too disproves
        //                    that: it is 128 in every element carrying EncounterEventID 294 (spell 1282251)
        //                    and 0 everywhere else, i.e. it is a per-event flag byte in the low bits of a
        //                    uint32, not padding. Meaning still unknown, so it is written as 0.
        //  MaxQueueDuration  (+0x58) 5000 in all 11 observed elements. Named after
        //                    EncounterTimelineEventInfo.maxQueueDuration ("hold duration for this event
        //                    after it reaches the end of the timeline"). INFERRED.
        //  Duration          (+0x5C) PROVEN to equal TimeToCastMs + MaxQueueDuration in all 11 elements
        //                    (23000/18000, 60000/55000, 14000/9000, 35000/30000, 33000/28000, 38000/33000,
        //                    25000/20000, 10000/5000, 19000/14000, ...). Named after
        //                    EncounterTimelineEventInfo.duration.
        //  CastState         (+0x60) 2 in every SEQUENCE/APPEND element, 1 in every CAST_UPDATE. That is
        //                    exactly EncounterEventCastState (Casting=1, NotCasting=2, Expired=3), and the
        //                    CAST_UPDATE parser's constructor pre-seeds this byte with 2 before reading.
        //  UnkBit7/UnkBit6   both false in every observed element.
        //  Casts             nested list, count 1 in every observed element. The uint32 is PROVEN to be the
        //                    milliseconds from Timestamp until the ability is cast: eight independent
        //                    CAST_UPDATEs land at their event's value to within the sniff's own tick
        //                    resolution (18000 vs 18027, 30000 vs 30029, 28000 vs 27998, 33000 vs 33000,
        //                    55000 vs 55074, 9000 vs 9025, 5000 vs 5026). The per-entry bit is false
        //                    throughout.
        struct EncounterTimelineCast
        {
            uint32 TimeToCastMs = 0;
            bool UnkBit7 = false;
        };

        struct EncounterTimelineEvent
        {
            uint8 Severity = 0;
            uint32 EventID = 0;
            uint32 EncounterEventID = 0;
            int32 SpellID = 0;
            uint32 Unused_28 = 0;
            uint32 Unknown_2C = 0;
            uint32 Unused_30 = 0;
            int32 IconFileID = 0;
            ObjectGuid Caster;
            uint32 Timestamp = 0;
            uint32 Unknown_54 = 0;
            uint32 MaxQueueDuration = 0;
            uint32 Duration = 0;
            uint8 CastState = 2;                    // EncounterEventCastState::NotCasting
            bool UnkBit7 = false;
            bool UnkBit6 = false;
            std::vector<EncounterTimelineCast> Casts;
        };

        ByteBuffer& operator<<(ByteBuffer& data, EncounterTimelineEvent const& timelineEvent);

        // SMSG_INSTANCE_ENCOUNTER_EVENT_SEQUENCE (0x420228): uint32 count, then count elements.
        // Parser sub_7FF7290A8910 reads exactly that and nothing else. Observed in
        // C:\sniff\m+ run12.0.7.pkt at 292 bytes (count 4), 217 bytes (count 3) and 4 bytes (count 0) -
        // the empty form is what retail sends at the moment the encounter ends, to clear the timeline.
        class InstanceEncounterEventSequence final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventSequence() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_SEQUENCE, 4) { }

            WorldPacket const* Write() override;

            std::vector<EncounterTimelineEvent> Events;
        };

        // SMSG_INSTANCE_ENCOUNTER_EVENT_APPEND (0x42022A): same shape as SEQUENCE (parser
        // sub_7FF7290A8AE0). Observed at 76 bytes = 4 + 1*72. Retail sends one of these at the instant a
        // scheduled ability fires, carrying the next occurrence of that same ability.
        class InstanceEncounterEventAppend final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventAppend() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_APPEND, 4 + 72) { }

            WorldPacket const* Write() override;

            std::vector<EncounterTimelineEvent> Events;
        };

        // SMSG_INSTANCE_ENCOUNTER_EVENT_CAST_UPDATE (0x42022D), parser sub_7FF7290A8D40:
        //   uint32 EventID, uint32 EncounterEventID, ObjectGuid Caster, uint32 DungeonEncounterID,
        //   uint8 CastState, uint8 Unknown_3D, uint32 Timestamp, uint32 TimeToCastMs, bit7|bit6 byte.
        // 4+4+16+4+1+1+4+4+1 = 39, and 38 when the GUID packs to 15 bytes. Both sizes are in the capture.
        //
        // The 39/38 split is ENTIRELY the PackedGUID width - there is no optional or conditional field in
        // this packet. Across all 52 CAST_UPDATEs in the 68275-family captures, the 45 that are 39 bytes
        // carry a 16-byte caster GUID and the 7 that are 38 bytes carry a 15-byte one, and every one of the
        // 52 parses to exactly zero trailing slack. The same effect explains SEQUENCE's 292 (4 + 4*72) and
        // 217 (4 + 3*71). All 7 short ones belong to DungeonEncounterID 2564: every caster in these
        // captures shares low mask 0xE7, but 2564's caster is the one whose high qword ends in a zero byte
        // (high = 0x202F313BC0BB3E00), so its high mask is 0xFE instead of 0xFF and one data byte drops.
        // Nothing about the packet's field set changes - only how many bytes the GUID spends.
        //
        // DungeonEncounterID is not a guess: the two CAST_UPDATE runs in C:\sniff\m+ run12.0.7.pkt carry
        // 0x0A03 (2563) and 0x0A04 (2564), and SMSG_ENCOUNTER_START at the very same sniff ticks (496010,
        // 743769) carries those same two DungeonEncounterIDs - see the EncounterStart comment above.
        //
        // TimeToCastMs here is the event's ORIGINAL countdown, not the residual at cast time: CAST_UPDATE
        // for EventID 1 arrives 18027 ms after the SEQUENCE that created it with 18000, and reports 18000,
        // not 0. Same for 30000, 28000, 33000, 55000, 9000, 5000.
        //
        // The trailing byte is 0x80 in all 52 captured CAST_UPDATEs, i.e. bit7 set, while it is 0x00 in
        // every SEQUENCE/APPEND element. Whatever the bit means, it flips on when the cast happens.
        //
        // CastState is NOT always 1. 49 of the 52 carry 1 (Casting) with Unknown_3D 0, but 3 carry
        // CastState 3 (EncounterEventCastState::Expired) with Unknown_3D 0xFF and TimeToCastMs 0. Those 3
        // are the retail "the queued ability was never actually cast" branch, and they land at
        // Timestamp + Duration, i.e. the event's cast moment plus MaxQueueDuration - matching to within
        // 190/258/71 ms on the three occurrences, which is far too tight to be coincidence. All 3 are the
        // same ability (EncounterEventID 294) that casts normally on other pulls, so state 1 and state 3
        // are genuine alternatives for one event, not a property of the ability.
        //
        // The server does not emit the Expired branch. Emitting it needs a "the scheduled cast did not
        // happen" signal, and InstanceScript's timeline fires state 1 unconditionally when the countdown
        // reaches zero because the script that scheduled the ability is the same thing that casts it.
        // The fields are here so the branch can be expressed the moment a caller can distinguish the two;
        // guessing a trigger for it would be inventing behaviour, so it is left unsent and documented.
        class InstanceEncounterEventCastUpdate final : public ServerPacket
        {
        public:
            explicit InstanceEncounterEventCastUpdate() : ServerPacket(SMSG_INSTANCE_ENCOUNTER_EVENT_CAST_UPDATE, 4 + 4 + 16 + 4 + 1 + 1 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 EventID = 0;
            uint32 EncounterEventID = 0;
            ObjectGuid Caster;
            uint32 DungeonEncounterID = 0;
            uint8 CastState = 1;                    // EncounterEventCastState::Casting; 3 == Expired
            uint8 Unknown_3D = 0;                   // 0 alongside CastState 1, 0xFF alongside CastState 3
            uint32 Timestamp = 0;
            uint32 TimeToCastMs = 0;
            bool UnkBit7 = true;                    // 0x80 in every capture
            bool UnkBit6 = false;
        };

        // CMSG_REQUEST_INSTANCE_ENCOUNTER_EVENT_SYNC (0x3A0196), serializer sub_7FF7291490C0: a single
        // packed GUID and nothing else. The two captures in C:\sniff\m+ run12.0.7.pkt are both 9 bytes -
        // masks 0x0F/0xE0 (4 + 3 set bits) plus 7 data bytes - and decode to the capturing player's own
        // GUID (HighGuid 2 = Player). The client sends it shortly after entering the world.
        class RequestInstanceEncounterEventSync final : public ClientPacket
        {
        public:
            explicit RequestInstanceEncounterEventSync(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_INSTANCE_ENCOUNTER_EVENT_SYNC, std::move(packet)) { }

            void Read() override;

            ObjectGuid Unit;
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

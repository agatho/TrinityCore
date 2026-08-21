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

#ifndef TRINITYCORE_COMBAT_PACKETS_H
#define TRINITYCORE_COMBAT_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "UnitDefines.h"

class Unit;
enum Powers : int8;

namespace WorldPackets
{
    namespace Combat
    {
        class AttackSwing final : public ClientPacket
        {
        public:
            explicit AttackSwing(WorldPacket&& packet) : ClientPacket(CMSG_ATTACK_SWING, std::move(packet)) { }

            void Read() override;

            ObjectGuid Victim;
        };

        class AttackSwingError final : public ServerPacket
        {
        public:
            explicit AttackSwingError() : ServerPacket(SMSG_ATTACK_SWING_ERROR, 4) { }
            explicit AttackSwingError(AttackSwingErr reason) : ServerPacket(SMSG_ATTACK_SWING_ERROR, 4), Reason(reason) { }

            WorldPacket const* Write() override;

            AttackSwingErr Reason = AttackSwingErr::CantAttack;
        };

        class AttackStop final : public ClientPacket
        {
        public:
            explicit AttackStop(WorldPacket&& packet) : ClientPacket(CMSG_ATTACK_STOP, std::move(packet)) { }

            void Read() override { }
        };

        class AttackStart final : public ServerPacket
        {
        public:
            explicit AttackStart() : ServerPacket(SMSG_ATTACK_START, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Attacker;
            ObjectGuid Victim;
        };

        class SAttackStop final : public ServerPacket
        {
        public:
            explicit SAttackStop() : ServerPacket(SMSG_ATTACK_STOP, 16 + 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Attacker;
            ObjectGuid Victim;
            bool NowDead = false;
        };

        // A swing the server threw away before it could land. Despite the "event failed" name this is not a
        // log message: hook slot 0x462E0D8 holds the same consumer as SMSG_ATTACK_STOP (slot 0x462E0D0), the
        // 9-byte thunk 0x1F79D90 -> 0x1F79C90 (both slots written at 0x210AD2 and 0x210B26), so the client ends
        // the attack exactly as it would on an attack stop - auto attack off, target marker and swing timer
        // cleared - and fires PLAYER_LEAVE_COMBAT, though only when the resolved unit is the local player.
        // Only the first guid is resolved; the second is read and dropped, and the whole handler no-ops if the
        // first does not resolve. Wire (dispatcher case 4915225): two calls to ReadPackedGuid 0x36012B0 and
        // nothing else - notably NOT the trailing NowDead bit that SMSG_ATTACK_STOP carries, so the two are not
        // wire-identical despite sharing a consumer. Byte-exact against five 24-byte 12.1 packets.
        class CombatEventFailed final : public ServerPacket
        {
        public:
            explicit CombatEventFailed() : ServerPacket(SMSG_COMBAT_EVENT_FAILED, 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Attacker;
            ObjectGuid Victim;
        };

        struct ThreatInfo
        {
            ObjectGuid UnitGUID;
            int64 Threat = 0;
        };

        class ThreatUpdate final : public ServerPacket
        {
        public:
            explicit ThreatUpdate() : ServerPacket(SMSG_THREAT_UPDATE, 24) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            std::vector<ThreatInfo> ThreatList;
        };

        class HighestThreatUpdate final : public ServerPacket
        {
        public:
            explicit HighestThreatUpdate() : ServerPacket(SMSG_HIGHEST_THREAT_UPDATE, 44) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            std::vector<ThreatInfo> ThreatList;
            ObjectGuid HighestThreatGUID;
        };

        class ThreatRemove final : public ServerPacket
        {
        public:
            explicit ThreatRemove() : ServerPacket(SMSG_THREAT_REMOVE, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid AboutGUID; // Unit to remove threat from (e.g. player, pet, guardian)
            ObjectGuid UnitGUID; // Unit being attacked (e.g. creature, boss)
        };

        class AIReaction final : public ServerPacket
        {
        public:
            explicit AIReaction() : ServerPacket(SMSG_AI_REACTION, 12) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            uint32 Reaction = 0;
        };

        class CancelCombat final : public ServerPacket
        {
        public:
            explicit CancelCombat() : ServerPacket(SMSG_CANCEL_COMBAT, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        struct PowerUpdatePower
        {
            PowerUpdatePower(int32 power, uint8 powerType) : Power(power), PowerType(powerType) { }

            int32 Power = 0;
            uint8 PowerType = 0;
        };

        class PowerUpdate final : public ServerPacket
        {
        public:
            explicit PowerUpdate() : ServerPacket(SMSG_POWER_UPDATE, 16 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
            std::vector<PowerUpdatePower> Powers;
        };

        class InterruptPowerRegen final : public ServerPacket
        {
        public:
            explicit InterruptPowerRegen(Powers powerType) : ServerPacket(SMSG_INTERRUPT_POWER_REGEN, 1), PowerType(powerType) { }

            WorldPacket const* Write() override;

            Powers PowerType;
        };

        class SetSheathed final : public ClientPacket
        {
        public:
            explicit SetSheathed(WorldPacket&& packet) : ClientPacket(CMSG_SET_SHEATHED, std::move(packet)) { }

            void Read() override;

            int32 CurrentSheathState = 0;
            bool Animate = true;
        };

        class CancelAutoRepeat final : public ServerPacket
        {
        public:
            explicit CancelAutoRepeat() : ServerPacket(SMSG_CANCEL_AUTO_REPEAT, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
        };

        // Restarts the client side ranged swing cooldown. The payload is a raw-pointer opcode (dispatcher case
        // 4915235 only takes a pointer to the unread rest via 0x35AF730); the format lives in the consumer
        // 0x1D8A150, which dereferences **(_DWORD **)(a1 + 32) - exactly one uint32 at the buffer start.
        // The value is a DURATION IN MILLISECONDS, not a spell or item id. Two independent proofs: 0x1D8A150
        // walks the action bar and builds the same cooldown record as the regular cooldown handler 0x1D89890,
        // where +32 is the duration and +28/+52 the start time (which the client sets to "now" here); and the
        // value flows into 0x1D824C0, which does imul rcx, rbx, 0xF4240 - milliseconds to nanoseconds - before
        // arming a timer. Zero is meaningful: test r15d,r15d at 0x1D8A275 skips both the record and the timer
        // while still firing the UI refresh, so 0 means "apply no cooldown, redraw". All five 12.1 packets in
        // the captures are 4 bytes of zero.
        // UNVERIFIED: whether 0 also REMOVES an already running cooldown. 0x1DD64C0 is a pool append with no
        // erase path on this code path, so the stronger reading is not supported by the disassembly.
        class ResetRangedCombatTimer final : public ServerPacket
        {
        public:
            explicit ResetRangedCombatTimer() : ServerPacket(SMSG_RESET_RANGED_COMBAT_TIMER, 4) { }

            WorldPacket const* Write() override;

            uint32 Cooldown = 0;
        };

        class HealthUpdate final : public ServerPacket
        {
        public:
            explicit HealthUpdate() : ServerPacket(SMSG_HEALTH_UPDATE, 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
            int64 Health = 0;
        };

        class ThreatClear final : public ServerPacket
        {
        public:
            explicit ThreatClear() : ServerPacket(SMSG_THREAT_CLEAR, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
        };

        class PvPCredit final : public ServerPacket
        {
        public:
            explicit PvPCredit() : ServerPacket(SMSG_PVP_CREDIT, 4 + 16 + 4) { }

            WorldPacket const* Write() override;

            int32 OriginalHonor = 0;
            int32 Honor = 0;
            ObjectGuid Target;
            int8 Rank = 0;
        };

        class BreakTarget final : public ServerPacket
        {
        public:
            explicit BreakTarget() : ServerPacket(SMSG_BREAK_TARGET, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
        };
    }
}

#endif // TRINITYCORE_COMBAT_PACKETS_H

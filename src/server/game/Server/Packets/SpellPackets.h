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

#ifndef TRINITYCORE_SPELL_PACKETS_H
#define TRINITYCORE_SPELL_PACKETS_H

#include "CombatLogPacketsCommon.h"
#include "CraftingPacketsCommon.h"
#include "ItemPacketsCommon.h"
#include "MovementInfo.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "PacketUtilities.h"
#include "Position.h"
#include <array>

namespace UF
{
    struct ChrCustomizationChoice;
}

namespace WorldPackets
{
    namespace Spells
    {
        class CancelAura final : public ClientPacket
        {
        public:
            explicit CancelAura(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_AURA, std::move(packet)) { }

            void Read() override;

            ObjectGuid CasterGUID;
            int32 SpellID = 0;
        };

        class CancelAutoRepeatSpell final : public ClientPacket
        {
        public:
            explicit CancelAutoRepeatSpell(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_AUTO_REPEAT_SPELL, std::move(packet)) { }

            void Read() override { }
        };

        class CancelChannelling final : public ClientPacket
        {
        public:
            explicit CancelChannelling(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_CHANNELLING, std::move(packet)) { }

            void Read() override;

            int32 ChannelSpell = 0;
            int32 Reason = 0; // 40 = /run SpellStopCasting(), 16 = movement/SpellAuraInterruptFlags::Moving, 41 = turning/SpellAuraInterruptFlags::Turning
            // does not match SpellCastResult enum
        };

        class CancelGrowthAura final : public ClientPacket
        {
        public:
            explicit CancelGrowthAura(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_GROWTH_AURA, std::move(packet)) { }

            void Read() override { }
        };

        class CancelMountAura final : public ClientPacket
        {
        public:
            explicit CancelMountAura(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_MOUNT_AURA, std::move(packet)) { }

            void Read() override { }
        };

        class CancelModSpeedNoControlAuras final : public ClientPacket
        {
        public:
            explicit CancelModSpeedNoControlAuras(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_MOD_SPEED_NO_CONTROL_AURAS, std::move(packet)) { }

            void Read() override;

            ObjectGuid TargetGUID;
        };

        class PetCancelAura final : public ClientPacket
        {
        public:
            explicit PetCancelAura(WorldPacket&& packet) : ClientPacket(CMSG_PET_CANCEL_AURA, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGUID;
            uint32 SpellID = 0;
        };

        class SendKnownSpells final : public ServerPacket
        {
        public:
            explicit SendKnownSpells() : ServerPacket(SMSG_SEND_KNOWN_SPELLS, 5) { }

            WorldPacket const* Write() override;

            bool InitialLogin = false;
            std::vector<uint32> KnownSpells;
            std::vector<uint32> FavoriteSpells; // tradeskill recipes
        };

        class UpdateActionButtons final : public ServerPacket
        {
        public:
            static std::size_t constexpr NumActionButtons = 180;

            explicit UpdateActionButtons() : ServerPacket(SMSG_UPDATE_ACTION_BUTTONS, NumActionButtons * 8 + 1) { }

            WorldPacket const* Write() override;

            std::array<uint64, NumActionButtons> ActionButtons = { };
            uint8 Reason = 0;
            /*
                Reason can be 0, 1, 2
                0 - Sends initial action buttons, client does not validate if we have the spell or not
                1 - Used used after spec swaps, client validates if a spell is known.
                2 - Clears the action bars client sided. This is sent during spec swap before unlearning and before sending the new buttons
            */
        };

        class SetActionButton final : public ClientPacket
        {
        public:
            explicit SetActionButton(WorldPacket&& packet) : ClientPacket(CMSG_SET_ACTION_BUTTON, std::move(packet)) { }

            void Read() override;

            uint64 Action = 0; ///< two packed values (action and type)
            uint8 Index = 0;
        };

        class SendUnlearnSpells final : public ServerPacket
        {
        public:
            explicit SendUnlearnSpells() : ServerPacket(SMSG_SEND_UNLEARN_SPELLS, 4) { }

            WorldPacket const* Write() override;

            std::vector<uint32> Spells;
        };

        struct AuraDataInfo
        {
            ObjectGuid CastID;
            int32 SpellID = 0;
            SpellCastVisual Visual;
            uint16 Flags = 0;
            uint32 ActiveFlags = 0;
            uint16 CastLevel = 1;
            uint8 Applications = 1;
            int32 ContentTuningID = 0;
            Optional<ContentTuningParams> ContentTuning;
            Optional<ObjectGuid> CastUnit;
            Optional<ObjectGuid> CastItem;
            Optional<int32> Duration;
            Optional<int32> Remaining;
            Optional<float> TimeMod;
            std::vector<float> Points;
            std::vector<float> EstimatedPoints;
            TaggedPosition<Position::XYZ> DstLocation;
        };

        struct AuraInfo
        {
            uint16 Slot = 0;
            Optional<AuraDataInfo> AuraData;
        };

        class AuraUpdate final : public ServerPacket
        {
        public:
            explicit AuraUpdate() : ServerPacket(SMSG_AURA_UPDATE) { }

            WorldPacket const* Write() override;

            bool UpdateAll = false;
            ObjectGuid UnitGUID;
            std::vector<AuraInfo> Auras;
        };

        struct TargetLocation
        {
            ObjectGuid Transport;
            TaggedPosition<Position::XYZ> Location;
        };

        struct SpellTargetData
        {
            uint32 Flags = 0;
            bool HousingIsResident = false;
            ObjectGuid Unit;
            ObjectGuid Item;
            ObjectGuid HousingGUID;
            Optional<TargetLocation> SrcLocation;
            Optional<TargetLocation> DstLocation;
            Optional<float> Orientation;
            Optional<int32> MapID;
            std::string Name;
        };

        struct MissileTrajectoryRequest
        {
            float Pitch = 0.0f;
            float Speed = 0.0f;
        };

        struct SpellWeight
        {
            uint32 Type = 0;
            int32 ID = 0;
            uint32 Quantity = 0;
        };

        struct SpellCraftingReagent
        {
            int32 Slot = 0;
            int32 Quantity = 0;
            Crafting::CraftingReagentBase Reagent;
            Optional<uint8> Source;
        };

        struct SpellExtraCurrencyCost
        {
            int32 CurrencyID = 0;
            int32 Count = 0;
        };

        struct SpellCastRequest
        {
            ObjectGuid CastID;
            int32 SpellID = 0;
            SpellCastVisual Visual;
            uint8 SendCastFlags = 0;
            SpellTargetData Target;
            Optional<Duration<Milliseconds, uint32>> ReceiveTime;
            MissileTrajectoryRequest MissileTrajectory;
            Optional<MovementInfo> MoveUpdate;
            std::vector<SpellWeight> Weight;
            Array<SpellCraftingReagent, 6> CraftingReagents;
            Array<SpellCraftingReagent, 6> RemovedReagents;
            Array<SpellExtraCurrencyCost, 5 /*MAX_ITEM_EXT_COST_CURRENCIES*/> ExtraCurrencyCosts;
            Optional<uint64> CraftingOrderID;
            uint8 CraftingCastFlags = 0; // 1 = ApplyConcentration
            ObjectGuid CraftingNPC;
            std::array<int32, 3> Misc = { };
        };

        class CastSpell final : public ClientPacket
        {
        public:
            explicit CastSpell(WorldPacket&& packet) : ClientPacket(CMSG_CAST_SPELL, std::move(packet)) { }

            void Read() override;

            SpellCastRequest Cast;
        };

        class PetCastSpell final : public ClientPacket
        {
        public:
            explicit PetCastSpell(WorldPacket&& packet) : ClientPacket(CMSG_PET_CAST_SPELL, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGUID;
            SpellCastRequest Cast;
        };

        class UseItem final : public ClientPacket
        {
        public:
            explicit UseItem(WorldPacket&& packet) : ClientPacket(CMSG_USE_ITEM, std::move(packet)) { }

            void Read() override;

            uint8 PackSlot = 0;
            uint8 Slot = 0;
            ObjectGuid CastItem;
            SpellCastRequest Cast;
        };

        class SpellPrepare final : public ServerPacket
        {
        public:
            explicit SpellPrepare() : ServerPacket(SMSG_SPELL_PREPARE, 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid ClientCastID;
            ObjectGuid ServerCastID;
        };

        struct SpellHitStatus
        {
            SpellHitStatus() { }
            SpellHitStatus(uint8 reason) : Reason(reason) { }

            uint8 Reason = 0;
        };

        struct SpellMissStatus
        {
            SpellMissStatus() { }
            SpellMissStatus(uint8 reason, uint8 reflectStatus) : Reason(reason), ReflectStatus(reflectStatus) { }

            uint8 Reason = 0;
            uint8 ReflectStatus = 0;
        };

        struct SpellPowerData
        {
            int32 Cost = 0;
            int8 Type = 0;
        };

        struct RuneData
        {
            uint8 Start = 0;
            uint8 Count = 0;
            std::vector<uint8> Cooldowns;
        };

        struct MissileTrajectoryResult
        {
            uint32 TravelTime = 0;
            float Pitch = 0.0f;
        };

        struct CreatureImmunities
        {
            uint32 School = 0;
            uint32 Value = 0;
        };

        struct SpellHealPrediction
        {
            ObjectGuid BeaconGUID;
            uint32 Points = 0;
            uint32 Type = 0;
        };

        struct SpellCastData
        {
            ObjectGuid CasterGUID;
            ObjectGuid CasterUnit;
            ObjectGuid CastID;
            ObjectGuid OriginalCastID;
            int32 SpellID       = 0;
            SpellCastVisual Visual;
            uint32 CastFlags    = 0;
            uint32 CastFlagsEx  = 0;
            uint32 CastFlagsEx2 = 0;
            uint32 CastTime     = 0;
            std::vector<ObjectGuid> HitTargets;
            std::vector<ObjectGuid> MissTargets;
            std::vector<SpellHitStatus> HitStatus;
            std::vector<SpellMissStatus> MissStatus;
            SpellTargetData Target;
            std::vector<SpellPowerData> RemainingPower;
            Optional<RuneData> RemainingRunes;
            MissileTrajectoryResult MissileTrajectory;
            int32 AmmoDisplayID = 0;
            uint8 DestLocSpellCastIndex = 0;
            std::vector<TargetLocation> TargetPoints;
            CreatureImmunities Immunities;
            SpellHealPrediction Predict;
        };

        class SpellStart final : public ServerPacket
        {
        public:
            explicit SpellStart() : ServerPacket(SMSG_SPELL_START) { }

            WorldPacket const* Write() override;

            SpellCastData Cast;
        };

        class SpellGo final : public CombatLog::CombatLogServerPacket
        {
        public:
            explicit SpellGo() : CombatLog::CombatLogServerPacket(SMSG_SPELL_GO) { }

            WorldPacket const* Write() override;

            SpellCastData Cast;
        };

        struct LearnedSpellInfo
        {
            int32 SpellID = 0;
            bool Favorite = false;
            Optional<int32> EquipableSpellInvSlot;
            Optional<int32> Superceded;
            Optional<int32> TraitDefinitionID;
        };

        class LearnedSpells final : public ServerPacket
        {
        public:
            explicit LearnedSpells() : ServerPacket(SMSG_LEARNED_SPELLS, 9) { }

            WorldPacket const* Write() override;

            std::vector<LearnedSpellInfo> ClientLearnedSpellData;
            uint32 SpecializationID = 0;
            int32 MinActionBarSlot = 0;                     ///< Where to start pushing spells on action bar
            bool SuppressMessaging = false;
            bool TraitGrantedByAura = false;
        };

        class SupercededSpells final : public ServerPacket
        {
        public:
            explicit SupercededSpells() : ServerPacket(SMSG_SUPERCEDED_SPELLS, 4 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            std::vector<LearnedSpellInfo> ClientLearnedSpellData;
        };

        class SpellFailure final : public ServerPacket
        {
        public:
            explicit SpellFailure() : ServerPacket(SMSG_SPELL_FAILURE, 16 + 4 + 8 + 2 + 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterUnit;
            uint32 SpellID  = 0;
            SpellCastVisual Visual;
            uint16 Reason   = 0;
            ObjectGuid CastID;
            ObjectGuid FailedBy;            ///< Unit that caused the spell to fail, set for SPELL_FAILED_INTERRUPTED_COMBAT
        };

        class SpellFailedOther final : public ServerPacket
        {
        public:
            explicit SpellFailedOther() : ServerPacket(SMSG_SPELL_FAILED_OTHER, 16 + 4 + 8 + 1 + 16 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterUnit;
            uint32 SpellID  = 0;
            SpellCastVisual Visual;
            uint8 Reason    = 0;
            ObjectGuid CastID;
            ObjectGuid FailedBy;            ///< Unit that caused the spell to fail, set for SPELL_FAILED_INTERRUPTED_COMBAT
        };

        class TC_GAME_API CastFailed final : public ServerPacket
        {
        public:
            explicit CastFailed() : ServerPacket(SMSG_CAST_FAILED, 16 + 4 + 8 + 4 + 4 + 4 + 16) { }

            WorldPacket const* Write() override;

            ObjectGuid CastID;
            int32 SpellID             = 0;
            SpellCastVisual Visual;
            int32 Reason              = 0;
            int32 FailedArg1          = -1;
            int32 FailedArg2          = -1;
            ObjectGuid FailedBy;            ///< Unit that caused the spell to fail, set for SPELL_FAILED_INTERRUPTED_COMBAT
        };

        class TC_GAME_API PetCastFailed final : public ServerPacket
        {
        public:
            explicit PetCastFailed() : ServerPacket(SMSG_PET_CAST_FAILED, 16+ 4 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid CastID;
            int32 SpellID = 0;
            int32 Reason = 0;
            int32 FailedArg1 = -1;
            int32 FailedArg2 = -1;
        };

        struct SpellModifierData
        {
            float ModifierValue = 0.0f;
            uint8 ClassIndex = 0;
        };

        struct SpellModifier
        {
            uint8 ModIndex = 0;
            std::vector<SpellModifierData> ModifierData;
        };

        class TC_GAME_API SetSpellModifier final : public ServerPacket
        {
        public:
            explicit SetSpellModifier(OpcodeServer opcode) : ServerPacket(opcode, 20) { }

            WorldPacket const* Write() override;

            std::vector<SpellModifier> Modifiers;
        };

        class UnlearnedSpells final : public ServerPacket
        {
        public:
            explicit UnlearnedSpells() : ServerPacket(SMSG_UNLEARNED_SPELLS, 4) { }

            WorldPacket const* Write() override;

            std::vector<uint32> SpellID;
            bool SuppressMessaging = false;
            bool TraitGrantedByAura = false;
        };

        class CooldownEvent final : public ServerPacket
        {
        public:
            explicit CooldownEvent() : ServerPacket(SMSG_COOLDOWN_EVENT, 1 + 4) { }
            explicit CooldownEvent(bool isPet, int32 spellId) : ServerPacket(SMSG_COOLDOWN_EVENT, 1 + 4), IsPet(isPet), SpellID(spellId) { }

            WorldPacket const* Write() override;

            bool IsPet = false;
            int32 SpellID = 0;
        };

        class ClearCooldowns final : public ServerPacket
        {
        public:
            explicit ClearCooldowns() : ServerPacket(SMSG_CLEAR_COOLDOWNS, 4 + 1) { }

            WorldPacket const* Write() override;

            std::vector<int32> SpellID;
            bool IsPet = false;
        };

        class ClearCooldown final : public ServerPacket
        {
        public:
            explicit ClearCooldown() : ServerPacket(SMSG_CLEAR_COOLDOWN, 1 + 4 + 1) { }

            WorldPacket const* Write() override;

            bool IsPet = false;
            int32 SpellID = 0;
            bool ClearOnHold = false;
        };

        class ModifyCooldown final : public ServerPacket
        {
        public:
            explicit ModifyCooldown() : ServerPacket(SMSG_MODIFY_COOLDOWN, 1 + 4 + 4) { }

            WorldPacket const* Write() override;

            bool IsPet = false;
            bool SkipCategory = false;
            int32 DeltaTime = 0;
            int32 SpellID = 0;
        };

        class UpdateCooldown final : public ServerPacket
        {
        public:
            explicit UpdateCooldown() : ServerPacket(SMSG_UPDATE_COOLDOWN, 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
            float ModChange = 1.0f;
            float ModRate = 1.0f;
        };

        struct SpellCooldownStruct
        {
            SpellCooldownStruct() { }
            SpellCooldownStruct(uint32 spellId, uint32 forcedCooldown) : SrecID(spellId), ForcedCooldown(forcedCooldown) { }

            uint32 SrecID = 0;
            uint32 ForcedCooldown = 0;
            float ModRate = 1.0f;
        };

        class TC_GAME_API SpellCooldown : public ServerPacket
        {
        public:
            explicit SpellCooldown() : ServerPacket(SMSG_SPELL_COOLDOWN, 4 + 16 + 1) { }

            WorldPacket const* Write() override;

            std::vector<SpellCooldownStruct> SpellCooldowns;
            ObjectGuid Caster;
            uint8 Flags = 0;
        };

        struct SpellHistoryEntry
        {
            uint32 SpellID = 0;
            uint32 ItemID = 0;
            uint32 Category = 0;
            int32 RecoveryTime = 0;
            int32 CategoryRecoveryTime = 0;
            float ModRate = 1.0f;
            bool OnHold = false;
            Optional<int32> RecoveryTimeStartOffset;
            Optional<int32> CategoryRecoveryTimeStartOffset;
        };

        class SendSpellHistory final : public ServerPacket
        {
        public:
            explicit SendSpellHistory() : ServerPacket(SMSG_SEND_SPELL_HISTORY, 4) { }

            WorldPacket const* Write() override;

            std::vector<SpellHistoryEntry> Entries;
        };

        class ClearAllSpellCharges final : public ServerPacket
        {
        public:
            explicit ClearAllSpellCharges() : ServerPacket(SMSG_CLEAR_ALL_SPELL_CHARGES, 1) { }

            WorldPacket const* Write() override;

            bool IsPet = false;
        };

        class ClearSpellCharges final : public ServerPacket
        {
        public:
            explicit ClearSpellCharges() : ServerPacket(SMSG_CLEAR_SPELL_CHARGES, 1 + 4) { }

            WorldPacket const* Write() override;

            bool IsPet = false;
            int32 Category = 0;
        };

        class SetSpellCharges final : public ServerPacket
        {
        public:
            explicit SetSpellCharges() : ServerPacket(SMSG_SET_SPELL_CHARGES, 4 + 4 + 1 + 4 + 1) { }

            WorldPacket const* Write() override;

            bool IsPet = false;
            uint32 Category = 0;
            uint32 NextRecoveryTime = 0;
            uint8 ConsumedCharges = 0;
            float ChargeModRate = 1.0f;
        };

        class UpdateChargeCategoryCooldown final : public ServerPacket
        {
        public:
            explicit UpdateChargeCategoryCooldown() : ServerPacket(SMSG_UPDATE_CHARGE_CATEGORY_COOLDOWN, 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            int32 Category = 0;
            float ModChange = 1.0f;
            float ModRate = 1.0f;
            bool Snapshot = false;
        };

        struct SpellChargeEntry
        {
            uint32 Category = 0;
            uint32 NextRecoveryTime = 0;
            float ChargeModRate = 1.0f;
            uint8 ConsumedCharges = 0;
        };

        class SendSpellCharges final : public ServerPacket
        {
        public:
            explicit SendSpellCharges() : ServerPacket(SMSG_SEND_SPELL_CHARGES, 4) { }

            WorldPacket const* Write() override;

            std::vector<SpellChargeEntry> Entries;
        };

        class ClearTarget final : public ServerPacket
        {
        public:
            explicit ClearTarget() : ServerPacket(SMSG_CLEAR_TARGET, 8) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
        };

        class CancelOrphanSpellVisual final : public ServerPacket
        {
        public:
            explicit CancelOrphanSpellVisual() : ServerPacket(SMSG_CANCEL_ORPHAN_SPELL_VISUAL, 4) { }

            WorldPacket const* Write() override;

            int32 SpellVisualID = 0;
        };

        class CancelSpellVisual final : public ServerPacket
        {
        public:
            explicit CancelSpellVisual() : ServerPacket(SMSG_CANCEL_SPELL_VISUAL, 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Source;
            int32 SpellVisualID = 0;
        };

        class CancelSpellVisualKit final : public ServerPacket
        {
        public:
            explicit CancelSpellVisualKit() : ServerPacket(SMSG_CANCEL_SPELL_VISUAL_KIT, 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Source;
            int32 SpellVisualKitID = 0;
            bool MountedVisual = false;
        };

        class PlayOrphanSpellVisual final : public ServerPacket
        {
        public:
            explicit PlayOrphanSpellVisual() : ServerPacket(SMSG_PLAY_ORPHAN_SPELL_VISUAL, 16 + 3 * 4 + 4 + 1 + 4 + 3 * 4 + 3 * 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Target; // Exclusive with TargetLocation
            ObjectGuid TargetTransport;
            TaggedPosition<Position::XYZ> SourceLocation;
            int32 SpellVisualID = 0;
            bool SpeedAsTime = false;
            float TravelSpeed = 0.0f;
            float LaunchDelay = 0.0f;
            float MinDuration = 0.0f;
            TaggedPosition<Position::XYZ> SourceRotation; // Vector of rotations, Orientation is z
            TaggedPosition<Position::XYZ> TargetLocation; // Exclusive with Target
        };

        class PlaySpellVisual final : public ServerPacket
        {
        public:
            explicit PlaySpellVisual() : ServerPacket(SMSG_PLAY_SPELL_VISUAL, 16 + 16 + 2 + 4 + 1 + 2 + 4 + 4 * 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Source;
            ObjectGuid Target;
            ObjectGuid Transport;                         // Used when Target = Empty && (SpellVisual::Flags & 0x400) == 0
            TaggedPosition<Position::XYZ> TargetPosition; // Overrides missile destination for SpellVisual::SpellVisualMissileSetID
            uint32 SpellVisualID = 0;
            float TravelSpeed = 0.0f;
            uint8 HitReason = 0;
            uint8 MissReason = 0;
            uint8 ReflectStatus = 0;
            float LaunchDelay = 0.0f;
            float MinDuration = 0.0f;
            bool SpeedAsTime = false;
        };

        class PlaySpellVisualKit final : public ServerPacket
        {
        public:
            explicit PlaySpellVisualKit() : ServerPacket(SMSG_PLAY_SPELL_VISUAL_KIT, 16 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            int32 KitRecID = 0;
            int32 KitType = 0;
            uint32 Duration = 0;
            bool MountedVisual = false;
        };

        class SpellVisualLoadScreen final : public ServerPacket
        {
        public:
            explicit SpellVisualLoadScreen(int32 spellVisualKitId, Milliseconds duration) : ServerPacket(SMSG_SPELL_VISUAL_LOAD_SCREEN, 4 + 4),
                SpellVisualKitID(spellVisualKitId), Duration(duration) { }

            WorldPacket const* Write() override;

            int32 SpellVisualKitID = 0;
            WorldPackets::Duration<Milliseconds, int32> Duration;
            int32 Delay = 0;
            bool Unknown_1210 = false;
        };

        class CancelCast final : public ClientPacket
        {
        public:
            explicit CancelCast(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_CAST, std::move(packet)) { }

            void Read() override;

            uint32 SpellID = 0;
            ObjectGuid CastID;
        };

        class OpenItem final : public ClientPacket
        {
        public:
            explicit OpenItem(WorldPacket&& packet) : ClientPacket(CMSG_OPEN_ITEM, std::move(packet)) { }

            void Read() override;

            uint8 Slot = 0;
            uint8 PackSlot = 0;
        };

        struct SpellChannelStartInterruptImmunities
        {
            int32 SchoolImmunities = 0;
            int32 Immunities = 0;
        };

        struct SpellTargetedHealPrediction
        {
            ObjectGuid TargetGUID;
            SpellHealPrediction Predict;
        };

        class SpellChannelStart final : public ServerPacket
        {
        public:
            explicit SpellChannelStart() : ServerPacket(SMSG_SPELL_CHANNEL_START, 4 + 16 + 4) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
            SpellCastVisual Visual;
            Optional<SpellChannelStartInterruptImmunities> InterruptImmunities;
            ObjectGuid CasterGUID;
            Optional<SpellTargetedHealPrediction> HealPrediction;
            uint32 ChannelDuration = 0;
        };

        class SpellChannelUpdate final : public ServerPacket
        {
        public:
            explicit SpellChannelUpdate() : ServerPacket(SMSG_SPELL_CHANNEL_UPDATE, 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            int32 TimeRemaining = 0;
            ObjectGuid FailedBy;            ///< Unit that caused the spell to fail, set for SPELL_FAILED_INTERRUPTED_COMBAT
        };

        class SpellEmpowerStart final : public ServerPacket
        {
        public:
            explicit SpellEmpowerStart() : ServerPacket(SMSG_SPELL_EMPOWER_START, 16 + 16 + 4 + 8 + 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid CastID;
            ObjectGuid CasterGUID;
            int32 SpellID = 0;
            SpellCastVisual Visual;
            Duration<Milliseconds, uint32> EmpowerDuration;
            Duration<Milliseconds, uint32> MinHoldTime;
            Duration<Milliseconds, uint32> HoldAtMaxTime;
            std::vector<ObjectGuid> Targets;
            std::vector<Duration<Milliseconds, uint32>> StageDurations;
            Optional<SpellChannelStartInterruptImmunities> InterruptImmunities;
            Optional<SpellTargetedHealPrediction> HealPrediction;
        };

        class SpellEmpowerUpdate final : public ServerPacket
        {
        public:
            explicit SpellEmpowerUpdate() : ServerPacket(SMSG_SPELL_EMPOWER_UPDATE, 16 + 16 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid CastID;
            ObjectGuid CasterGUID;
            Duration<Milliseconds, int32> TimeRemaining;
            std::vector<Duration<Milliseconds, uint32>> StageDurations;
            uint8 Status = 0;
            ObjectGuid FailedBy;            ///< Unit that caused the spell to fail, set for SPELL_FAILED_INTERRUPTED_COMBAT
        };

        class SetEmpowerMinHoldStagePercent final : public ClientPacket
        {
        public:
            explicit SetEmpowerMinHoldStagePercent(WorldPacket&& packet) : ClientPacket(CMSG_SET_EMPOWER_MIN_HOLD_STAGE_PERCENT, std::move(packet)) { }

            void Read() override;

            float MinHoldStagePercent = 1.0f;
        };

        class SpellEmpowerRelease final : public ClientPacket
        {
        public:
            explicit SpellEmpowerRelease(WorldPacket&& packet) : ClientPacket(CMSG_SPELL_EMPOWER_RELEASE, std::move(packet)) { }

            void Read() override;

            int32 SpellID = 0;
        };

        class SpellEmpowerRestart final : public ClientPacket
        {
        public:
            explicit SpellEmpowerRestart(WorldPacket&& packet) : ClientPacket(CMSG_SPELL_EMPOWER_RESTART, std::move(packet)) { }

            void Read() override;

            int32 SpellID = 0;
        };

        class SpellEmpowerSetStage final : public ServerPacket
        {
        public:
            explicit SpellEmpowerSetStage() : ServerPacket(SMSG_SPELL_EMPOWER_SET_STAGE, 16 + 16 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid CastID;
            ObjectGuid CasterGUID;
            int32 Stage = 0;
        };

        class ResurrectRequest final : public ServerPacket
        {
        public:
            explicit ResurrectRequest() : ServerPacket(SMSG_RESURRECT_REQUEST, 16 + 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid ResurrectOffererGUID;
            uint32 ResurrectOffererVirtualRealmAddress  = 0;
            uint32 PetNumber                            = 0;
            int32 SpellID                               = 0;
            bool UseTimer                               = false;
            bool Sickness                               = false;
            std::string Name;
        };

        class UnlearnSkill final : public ClientPacket
        {
        public:
            explicit UnlearnSkill(WorldPacket&& packet) : ClientPacket(CMSG_UNLEARN_SKILL, std::move(packet)) { }

            void Read() override;

            uint32 SkillLine = 0;
        };

        class SelfRes final : public ClientPacket
        {
        public:
            explicit SelfRes(WorldPacket&& packet) : ClientPacket(CMSG_SELF_RES, std::move(packet)) { }

            void Read() override;

            int32 SpellID = 0;
        };

        class GetMirrorImageData final : public ClientPacket
        {
        public:
            explicit GetMirrorImageData(WorldPacket&& packet) : ClientPacket(CMSG_GET_MIRROR_IMAGE_DATA, std::move(packet)) {}

            void Read() override;

            ObjectGuid UnitGUID;
            int32 DisplayID = 0;
        };

        class MirrorImageComponentedData final : public ServerPacket
        {
        public:
            explicit MirrorImageComponentedData();
            ~MirrorImageComponentedData();

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            int32 ChrModelID = 0;
            int32 SpellVisualKitID = 0;
            int32 Unused_1115 = 0;
            uint8 RaceID = 0;
            uint8 Gender = 0;
            uint8 ClassID = 0;
            std::vector<UF::ChrCustomizationChoice> Customizations;
            ObjectGuid GuildGUID;

            std::vector<int32> ItemDisplayID;
        };

        class MirrorImageCreatureData final : public ServerPacket
        {
        public:
            explicit MirrorImageCreatureData() : ServerPacket(SMSG_MIRROR_IMAGE_CREATURE_DATA, 8 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            int32 DisplayID = 0;
            int32 SpellVisualKitID = 0;
        };

        class SpellClick final : public ClientPacket
        {
        public:
            explicit SpellClick(WorldPacket&& packet) : ClientPacket(CMSG_SPELL_CLICK, std::move(packet)) { }

            void Read() override;

            ObjectGuid SpellClickUnitGuid;
            bool TryAutoDismount = false;
        };

        class ResyncRunes final : public ServerPacket
        {
        public:
            explicit ResyncRunes(size_t size) : ServerPacket(SMSG_RESYNC_RUNES, 1 + 1 + 4 + size) { }

            WorldPacket const* Write() override;

            RuneData Runes;
        };

        class AddRunePower final : public ServerPacket
        {
        public:
            explicit AddRunePower() : ServerPacket(SMSG_ADD_RUNE_POWER, 4) { }

            WorldPacket const* Write() override;

            uint32 AddedRunesMask = 0;
        };

        class MissileTrajectoryCollision final : public ClientPacket
        {
        public:
            explicit MissileTrajectoryCollision(WorldPacket&& packet) : ClientPacket(CMSG_MISSILE_TRAJECTORY_COLLISION, std::move(packet)) { }

            void Read() override;

            ObjectGuid Target;
            int32 SpellID = 0;
            ObjectGuid CastID;
            TaggedPosition<Position::XYZ> CollisionPos;
        };

        class NotifyMissileTrajectoryCollision final : public ServerPacket
        {
        public:
            explicit NotifyMissileTrajectoryCollision() : ServerPacket(SMSG_NOTIFY_MISSILE_TRAJECTORY_COLLISION, 8 + 1 + 12) { }

            WorldPacket const* Write() override;

            ObjectGuid Caster;
            ObjectGuid CastID;
            TaggedPosition<Position::XYZ> CollisionPos;
        };

        class UpdateMissileTrajectory final : public ClientPacket
        {
        public:
            explicit UpdateMissileTrajectory(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_MISSILE_TRAJECTORY, std::move(packet)) { }

            void Read() override;

            ObjectGuid Guid;
            ObjectGuid CastID;
            uint32 MoveMsgID = 0;
            int32 SpellID = 0;
            float Pitch = 0.0f;
            float Speed = 0.0f;
            TaggedPosition<Position::XYZ> FirePos;
            TaggedPosition<Position::XYZ> ImpactPos;
            Optional<MovementInfo> Status;
        };

        class UpdateAuraVisual final : public ClientPacket
        {
        public:
            explicit UpdateAuraVisual(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_SPELL_VISUAL, std::move(packet)) { }

            void Read() override;

            int32 SpellID = 0;
            SpellCastVisual Visual;
            ObjectGuid TargetGUID;
        };

        class SpellDelayed final : public ServerPacket
        {
        public:
            explicit SpellDelayed() : ServerPacket(SMSG_SPELL_DELAYED, sizeof(ObjectGuid) + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Caster;
            int32 ActualDelay = 0;
        };

        class DispelFailed final : public ServerPacket
        {
        public:
            explicit DispelFailed() : ServerPacket(SMSG_DISPEL_FAILED, 16 + 16 + 4 + 4 + 4 /* predict a single failure on average */) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            ObjectGuid VictimGUID;
            uint32 SpellID = 0;
            std::vector<int32> FailedSpells;
        };

        class CustomLoadScreen final : public ServerPacket
        {
        public:
            explicit CustomLoadScreen(uint32 teleportSpellId, uint32 loadingScreenId) : ServerPacket(SMSG_CUSTOM_LOAD_SCREEN, 4 + 4),
                TeleportSpellID(teleportSpellId), LoadingScreenID(loadingScreenId) { }

            WorldPacket const* Write() override;

            uint32 TeleportSpellID;
            uint32 LoadingScreenID;
        };

        class MountResult final : public ServerPacket
        {
        public:
            explicit MountResult() : ServerPacket(SMSG_MOUNT_RESULT, 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
        };

        class ApplyMountEquipmentResult final : public ServerPacket
        {
        public:
            enum ApplyResult : int32
            {
                Success = 0,
                Failure = 1
            };

            explicit ApplyMountEquipmentResult() : ServerPacket(SMSG_APPLY_MOUNT_EQUIPMENT_RESULT, 16 + 4 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid ItemGUID;
            int32 ItemID = 0;
            ApplyResult Result = Success;
        };

        class MissileCancel final : public ServerPacket
        {
        public:
            explicit MissileCancel() : ServerPacket(SMSG_MISSILE_CANCEL, 21) { }

            WorldPacket const* Write() override;

            ObjectGuid OwnerGUID;
            bool Reverse = false;
            int32 SpellID = 0;
        };

        class TradeSkillSetFavorite final : public ClientPacket
        {
        public:
            explicit TradeSkillSetFavorite(WorldPacket&& packet) : ClientPacket(CMSG_TRADE_SKILL_SET_FAVORITE, std::move(packet)) { }

            void Read() override;

            int32 RecipeID = 0;
            bool IsFavorite = false;
        };

        // SMSG_XP_AWARDED_FROM_CURRENCY (0x450341), reader 0x60ED40:
        //     Read<uint32> x3, then Read<uint8> followed by ">> 7" (a one bit section) - 13 bytes.
        // The opcode name is TrinityCore convention and is misleading: the consumer (0x24006B0) fires
        // Lua TRADE_SKILL_CURRENCY_REWARD_RESULT (murmur3 0xB2B6D1FA06DDBC2A) with one argument of
        // type CraftingCurrencyResultData, whose field order is
        //     currencyID, quantity, operationID, firstCraftReward, showCurrencyText
        // (TradeSkillUITypesDocumentation.lua:110-120). That structure has no XP field at all. This is
        // the currency reward of a crafting order, handled in
        // Blizzard_ProfessionsCraftingOutputLog.lua:262 and :344-347; OperationID is the key that ties
        // child results to their parent result (:359-370). The sender belongs in the crafting order
        // code, not in Player::GiveXP.
        //
        // The consumer has three silent drop conditions - it fires nothing at all unless the bit is
        // set, OperationID != 0 and Quantity > 0 (signed compare). A server that leaves one of these
        // at zero looks exactly like a server that sent nothing, and logs nothing either.
        class XPAwardedFromCurrency final : public ServerPacket
        {
        public:
            explicit XPAwardedFromCurrency() : ServerPacket(SMSG_XP_AWARDED_FROM_CURRENCY, 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            int32 CurrencyID = 0;               ///< CurrencyTypes::ID
            int32 Quantity = 0;                 ///< must be > 0 or the client drops the message
            int32 OperationID = 0;              ///< must be != 0 or the client drops the message
            /// UNVERIFIED: proven to be a gate the client requires to be set; which of the two Lua
            /// bools it carries is not decidable offline - the consumer writes both as constants
            /// (mov word ptr [rsp+0x2c], 1). Named after the more likely of the two.
            bool FirstCraftReward = false;
        };

        class KeyboundOverride final : public ClientPacket
        {
        public:
            explicit KeyboundOverride(WorldPacket&& packet) : ClientPacket(CMSG_KEYBOUND_OVERRIDE,  std::move(packet)) { }

            void Read() override;

            uint16 OverrideID = 0;
        };

        class CancelQueuedSpell final : public ClientPacket
        {
        public:
            explicit CancelQueuedSpell(WorldPacket&& packet) : ClientPacket(CMSG_CANCEL_QUEUED_SPELL, std::move(packet)) { }

            void Read() override { }
        };

        // ---------------------------------------------------------------------
        // Familie 0x67 - Phase A (Client-Build 12.1.0.69382, alle RVA gegen ImageBase)
        // Feldfolge und Feldbreiten stammen aus dem Client-Binary, gegengerechnet an den
        // Referenzpaketen aus C:/dumps/fam67_sniff_sizes_12_1.json.
        // ---------------------------------------------------------------------

        // SMSG_SET_FLAT_SPELL_PVP_MODIFIER / SMSG_SET_PCT_SPELL_PVP_MODIFIER (0x670029 / 0x67002A)
        // ACHTUNG: ModIndex ist hier uint32, bei den Nicht-PvP-Zwillingen uint8.
        // Beleg: Elementleser 0x68A1D0 ruft R32 @0x35AF190 (Aufruf 0x68A1F5, Ablage
        // `mov [rbx], eax` @0x68A205), waehrend der Nicht-PvP-Leser 0x68A0F0 R8 @0x35AF050
        // ruft (`mov [rbx], al` @0x68A12B). Kein Schiebeausdruck, also echte Felder.
        // Draht: 0x67002A min 17 B = 4 + 4 + 4 + 5 -- mit uint8 waeren es 14.
        struct SpellPvpModifier
        {
            uint32 ModIndex = 0;                            ///< SpellPvpModifier, 0..9 (NICHT SpellModOp)
            std::vector<SpellModifierData> ModifierData;
        };

        class TC_GAME_API SetSpellPvpModifier final : public ServerPacket
        {
        public:
            explicit SetSpellPvpModifier(OpcodeServer opcode) : ServerPacket(opcode, 4 + 4 + 4 + 5) { }

            WorldPacket const* Write() override;

            std::vector<SpellPvpModifier> Modifiers;
        };

        // SMSG_SPELL_CATEGORY_COOLDOWN (0x670006) - festes Quadrupel, KEINE Liste.
        // Beleg: Handler 0x74E94D..0x74EA15 (inline im Dispatcher 0x74E520):
        //   R32 @0x74E987 -> Category, R32 @0x74E9A6 -> ModCooldown (ms),
        //   R32 @0x74E9C5 (movss 0x74E9CA/0x74E9DC, Vorbelegung 1.0f @0x74E97A) -> ModRate,
        //   R8 @0x74E9E9 mit `shr al,7` @0x74E9F5 -> IsPet.
        // Draht: 1 Paket a 13 B, `9e040000 905f0100 0000803f 00`.
        // Wachbedingung im Konsumenten 0x1E2E6F0: `if (Category && ModCooldown)`, sonst
        // verwirft der Client die Nachricht wortlos.
        class SpellCategoryCooldown final : public ServerPacket
        {
        public:
            explicit SpellCategoryCooldown() : ServerPacket(SMSG_SPELL_CATEGORY_COOLDOWN, 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            int32 Category = 0;
            Duration<Milliseconds, int32> ModCooldown;
            float ModRate = 1.0f;
            bool IsPet = false;
        };

        // SMSG_SPELL_FAILURE_MESSAGE (0x67004D)
        // Das uint32 ist ein SpellCastResult, KEINE SpellID. Beleg: Konsument 0x1D89100
        //   01D89108  mov ecx, [rcx]        ; *(uint32*)payload
        //   01D8910A  call 0x1DB4E80        ; CastResult -> "SPELL_FAILED_*"-Schluessel
        //   01D8911D  call 0x567420         ; GlobalStrings -> lokalisierter Text
        //   01D89125  mov ecx, 0x38         ; GameError 56 = ERR_SPELL_FAILED_S
        // 0x1DB4E80 ist derselbe Mapper, der die 324-Werte-Tabelle SpellCastResult aufloest.
        class SpellFailureMessage final : public ServerPacket
        {
        public:
            explicit SpellFailureMessage() : ServerPacket(SMSG_SPELL_FAILURE_MESSAGE, 4) { }
            explicit SpellFailureMessage(int32 reason) : ServerPacket(SMSG_SPELL_FAILURE_MESSAGE, 4), Reason(reason) { }

            WorldPacket const* Write() override;

            int32 Reason = 0;                               ///< SpellCastResult (0..323)
        };

        // SMSG_SCRIPT_CAST (0x670049) - der Client wirkt den Zauber ohne Spielereingabe.
        // Beleg: Konsument 0x1E24FB0 liest den ersten uint32 und ruft 0x1D4F8E0(ctx, SpellID, ...).
        class ScriptCast final : public ServerPacket
        {
        public:
            explicit ScriptCast() : ServerPacket(SMSG_SCRIPT_CAST, 4) { }
            explicit ScriptCast(int32 spellId) : ServerPacket(SMSG_SCRIPT_CAST, 4), SpellID(spellId) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
        };

        // SMSG_PUSH_SPELL_TO_ACTION_BAR (0x670044)
        // Draht: 2 Pakete a 4 B (`80af0500`). Slot 0x43AA350 haelt ein Event-Objekt mit
        // getaggtem Zeiger (Wert 0x1) - kein Abonnent im statischen Abbild, deshalb ist kein
        // Konsument auslesbar.
        // UNVERIFIED: dass das uint32 eine SpellID ist, folgt aus der konstanten Paketgroesse
        // und dem Opcodenamen, nicht aus einem Leser.
        class PushSpellToActionBar final : public ServerPacket
        {
        public:
            explicit PushSpellToActionBar() : ServerPacket(SMSG_PUSH_SPELL_TO_ACTION_BAR, 4) { }
            explicit PushSpellToActionBar(int32 spellId) : ServerPacket(SMSG_PUSH_SPELL_TO_ACTION_BAR, 4), SpellID(spellId) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
        };

        // SMSG_REMOVE_SPELL_FROM_ACTION_BAR (0x670045)
        // UNVERIFIED: kein Referenzpaket und kein Konsument im Abbild; die Struktur ist allein
        // aus der Symmetrie zu SMSG_PUSH_SPELL_TO_ACTION_BAR abgeleitet.
        class RemoveSpellFromActionBar final : public ServerPacket
        {
        public:
            explicit RemoveSpellFromActionBar() : ServerPacket(SMSG_REMOVE_SPELL_FROM_ACTION_BAR, 4) { }
            explicit RemoveSpellFromActionBar(int32 spellId) : ServerPacket(SMSG_REMOVE_SPELL_FROM_ACTION_BAR, 4), SpellID(spellId) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
        };

        // SMSG_RESTART_GLOBAL_COOLDOWN (0x670054)
        // Beleg: Case 0x751D43 - RGUID @0x36012B0 gefolgt von R32 @0x35AF190. Laenge 6..22 B.
        // Kein Referenzpaket in den 12 Aufnahmen.
        class RestartGlobalCooldown final : public ServerPacket
        {
        public:
            explicit RestartGlobalCooldown() : ServerPacket(SMSG_RESTART_GLOBAL_COOLDOWN, 18 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            int32 SpellID = 0;
        };

        // SMSG_CHEAT_IGNORE_DIMISHING_RETURNS (0x670002) - ein Bit, danach FlushBits (1 B Draht).
        // Der Handler-Slot zeigt im Retail-Client auf den `return 0`-Stub: die Nachricht wird
        // angenommen und bewirkt dort nichts. Der Serverzustand dahinter ist trotzdem real.
        // UNVERIFIED: welche WIRKUNGSRICHTUNG das Bit meint (DR auf dem Schaltenden gegen DR
        // seiner eigenen Kontrollzauber), ist unbelegt und kann aus dem Client nicht belegt werden
        // - kein Konsument. Begruendet und markiert an der wirksamen Stelle:
        // Unit::ApplyDiminishingToDuration.
        class CheatIgnoreDiminishingReturns final : public ServerPacket
        {
        public:
            explicit CheatIgnoreDiminishingReturns() : ServerPacket(SMSG_CHEAT_IGNORE_DIMISHING_RETURNS, 1) { }
            explicit CheatIgnoreDiminishingReturns(bool enable) : ServerPacket(SMSG_CHEAT_IGNORE_DIMISHING_RETURNS, 1), Enable(enable) { }

            WorldPacket const* Write() override;

            bool Enable = false;
        };

        // SMSG_NOTIFY_DEST_LOC_SPELL_CAST (0x670036) - Zielort-Geschoss.
        // Beleg: Leser 0x68A980 (ein Basisblock, keine Schleife, keine Bit-Sektion, kein String):
        //   RGUID 0x68AA03 / 0x68AA10, R32 0x68AA2D..0x68AB40, R8 0x68AB62, RGUID 0x68AB78.
        // Laenge 55..103 B.
        // CastIndex muss pro Caster STRENG MONOTON wachsen: Handler 0x1D89F30 verwirft die
        // Nachricht bei `CastIndex - node[0x40] <= 0` (jle 0x1D89F94) - lautlos, ohne Log.
        class NotifyDestLocSpellCast final : public ServerPacket
        {
        public:
            explicit NotifyDestLocSpellCast() : ServerPacket(SMSG_NOTIFY_DEST_LOC_SPELL_CAST, 18 + 18 + 12 * 4 + 1 + 18) { }

            WorldPacket const* Write() override;

            ObjectGuid Caster;
            ObjectGuid DestTransport;                       ///< TransportTracking::TrackingInfo, kein zweites Ziel
            int32 SpellID = 0;
            SpellCastVisual Visual;
            TaggedPosition<Position::XYZ> SourceLoc;
            TaggedPosition<Position::XYZ> DestLoc;
            float Pitch = 0.0f;
            float Speed = 0.0f;
            Duration<Milliseconds, uint32> TravelTime;
            uint8 CastIndex = 0;                            ///< muss pro Caster streng monoton steigen
            ObjectGuid CastID;                              ///< HighGuid::Cast (47)
        };

        // SMSG_AURA_POINTS_DEPLETED (0x670012)
        // Beleg: Case 0x74F2EB - RGUID @0x74F316, R16 @0x74F32E (`mov [rsp+0x60], ax`),
        // R8 @0x74F350 (`mov [rsp+0x62], al`); keine Schiebeausdruecke, also echte Felder.
        // Konsument 0x1EF8DF0: Slot indiziert die Aurenliste der Einheit (Schranke unit[0x5F8],
        // Schrittweite 0x108), EffectIndex indiziert deren Points-Array und wird auf 0 gesetzt.
        // Draht: 190 Pakete, 12..19 B; Beispiel `0fe0 407fab09 441608 0e00 00` = 9 + 2 + 1.
        class AuraPointsDepleted final : public ServerPacket
        {
        public:
            explicit AuraPointsDepleted() : ServerPacket(SMSG_AURA_POINTS_DEPLETED, 18 + 2 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            uint16 Slot = 0;                                ///< Aurenslot wie in SMSG_AURA_UPDATE
            uint8 EffectIndex = 0;                          ///< Index in AuraDataInfo::Points
        };

        // SMSG_RESUME_CAST (0x67002E)
        // Beleg: Case 0x75052F - RGUID 0x750565 (msg+0x20), Unterleser 0x6BF980 (2x R32 =
        // SpellCastVisual, msg+0x30), RGUID 0x75057D (msg+0x38), RGUID 0x750589 (msg+0x48),
        // R32 0x75059E (msg+0x58).
        // Konsument 0x1D88DC0 prueft msg+0x38 auf HighGuid::Cast (47) und uebergibt
        // (Unit, CastID, msg+0x58, msg+0x30, msg+0x48) an 0x1F2B340, das
        //   unit+944 = CastID | unit+960 = msg+0x58 | unit+964 = Visual | unit+984 = msg+0x48
        // setzt - dieselben Slots, die der lokale Pfad 0x1F2B160 aus castObj+96 / +72 / +84 /
        // +136 fuellt. castObj+72 ist die SpellID (0x1DC7440 setzt sie aus Arg 2), castObj+136
        // wird von `UnitShouldDisplaySpellTargetName` (0x1728F10) als Einheiten-GUID
        // aufgeloest, ist also das Zauberziel.
        // Draht: 102 Pakete, 37..61 B. Beispielpaket 38 B = 9 + 8 + 15 + 2 + 4; dessen dritte
        // GUID traegt den Typ 47 (0xbc >> 2), die vierte ist leer.
        class ResumeCast final : public ServerPacket
        {
        public:
            explicit ResumeCast() : ServerPacket(SMSG_RESUME_CAST, 18 + 8 + 18 + 18 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            SpellCastVisual Visual;
            ObjectGuid CastID;                              ///< HighGuid::Cast (47)
            ObjectGuid TargetGUID;
            int32 SpellID = 0;
        };

        // SMSG_RESUME_CAST_BAR (0x670031)
        // Beleg: Case 0x7507B6 - RGUID 0x7507E8 (msg+0x20), RGUID 0x7507F4 (msg+0x30),
        // R32 0x750809 (msg+0x40), Unterleser 0x6BF980 (SpellCastVisual, msg+0x44),
        // R32 0x750833 (msg+0x4C), R32 0x750851 (msg+0x50), R8 0x750870 mit `shr al,7`
        // (msg+0x54) und - nur wenn gesetzt - R32 0x750892 / 0x7508AA (msg+0x58 / msg+0x5C).
        // Konsument 0x1D8AE20: SpellID gegen den Spell-Store geprueft (0x4DBFA0), Endzeit
        // `now + TimeRemaining`, Startzeit `now + TimeRemaining - CastTime`; das optionale
        // Paar landet in unit+972 / +976 - denselben Slots, die SMSG_SPELL_CHANNEL_START
        // (Konsument 0x1D85B90) aus seinem InterruptImmunities-Paar fuellt und die
        // SMSG_SPELL_START ueber 0x1D85110 aus JamSpellCastData+1704 (`immunities`) speist.
        // Feuert UNIT_SPELLCAST_START / _CHANNEL_START / _EMPOWER_START.
        // Draht: 364 Pakete, 32..53 B. Beispielpaket 32 B = 9 + 2 + 4 + 8 + 4 + 4 + 1.
        class ResumeCastBar final : public ServerPacket
        {
        public:
            explicit ResumeCastBar() : ServerPacket(SMSG_RESUME_CAST_BAR, 18 + 18 + 4 + 8 + 4 + 4 + 1 + 8) { }

            WorldPacket const* Write() override;

            ObjectGuid CasterGUID;
            ObjectGuid TargetGUID;
            int32 SpellID = 0;
            SpellCastVisual Visual;
            Duration<Milliseconds, uint32> TimeRemaining;
            Duration<Milliseconds, uint32> CastTime;
            Optional<SpellChannelStartInterruptImmunities> InterruptImmunities;
        };

        ByteBuffer& operator>>(ByteBuffer& buffer, SpellCastRequest& request);
    }
}

#endif // TRINITYCORE_SPELL_PACKETS_H

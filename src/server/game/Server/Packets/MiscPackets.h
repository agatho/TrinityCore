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

#ifndef TRINITYCORE_MISC_PACKETS_H
#define TRINITYCORE_MISC_PACKETS_H

#include "Packet.h"
#include "CollectionMgr.h"
#include "CUFProfile.h"
#include "ItemPacketsCommon.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "PacketUtilities.h"
#include "Position.h"
#include "SharedDefines.h"
#include "WowTime.h"
#include <array>
#include <map>

enum class CountdownTimerType : int32;
enum class DisplayToastType : uint8;
enum class DisplayToastMethod : uint8;
enum UnitStandStateType : uint8;
enum WeatherState : uint32;

namespace WorldPackets
{
    namespace Misc
    {
        class BindPointUpdate final : public ServerPacket
        {
        public:
            explicit BindPointUpdate() : ServerPacket(SMSG_BIND_POINT_UPDATE, 20) { }

            WorldPacket const* Write() override;

            uint32 BindMapID = 0;
            TaggedPosition<Position::XYZ> BindPosition;
            uint32 BindAreaID = 0;
        };

        class PlayerBound final : public ServerPacket
        {
        public:
            explicit PlayerBound() : ServerPacket(SMSG_PLAYER_BOUND, 16 + 4) { }
            explicit PlayerBound(ObjectGuid binderId, uint32 areaId) : ServerPacket(SMSG_PLAYER_BOUND, 16 + 4),
                BinderID(binderId), AreaID(areaId) { }

            WorldPacket const* Write() override;

            ObjectGuid BinderID;
            uint32 AreaID = 0;
        };

        class InvalidatePlayer final : public ServerPacket
        {
        public:
            explicit InvalidatePlayer() : ServerPacket(SMSG_INVALIDATE_PLAYER, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
        };

        class LoginSetTimeSpeed final : public ServerPacket
        {
        public:
            explicit LoginSetTimeSpeed() : ServerPacket(SMSG_LOGIN_SET_TIME_SPEED, 20) { }

            WorldPacket const* Write() override;

            float NewSpeed = 0.0f;
            int32 ServerTimeHolidayOffset = 0;
            WowTime GameTime;
            WowTime ServerTime;
            int32 GameTimeHolidayOffset = 0;
        };

        class ResetWeeklyCurrency final : public ServerPacket
        {
        public:
            explicit ResetWeeklyCurrency() : ServerPacket(SMSG_RESET_WEEKLY_CURRENCY, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class SetCurrency final : public ServerPacket
        {
        public:
            explicit SetCurrency() : ServerPacket(SMSG_SET_CURRENCY, 12) { }

            WorldPacket const* Write() override;

            int32 Type = 0;
            int32 Quantity = 0;
            CurrencyGainFlags Flags = CurrencyGainFlags(0);
            std::vector<Item::UiEventToast> Toasts;
            Optional<int32> WeeklyQuantity;
            Optional<int32> TrackedQuantity;
            Optional<int32> MaxQuantity;
            Optional<int32> TotalEarned;
            Optional<int32> QuantityChange;
            Optional<CurrencyGainSource> QuantityGainSource;
            Optional<CurrencyDestroyReason> QuantityLostSource;
            Optional<uint32> FirstCraftOperationID;
            Optional<Timestamp<>> NextRechargeTime;
            Optional<Timestamp<>> RechargeCycleStartTime;
            Optional<int32> OverflownCurrencyID;    // what currency was originally changed but couldn't be incremented because of a cap
            bool SuppressChatLog = false;
        };

        class SetCurrencyFlags final : public ClientPacket
        {
        public:
            explicit SetCurrencyFlags(WorldPacket&& packet) : ClientPacket(CMSG_SET_CURRENCY_FLAGS, std::move(packet)) { }

            void Read() override;

            uint32 CurrencyID = 0;
            CurrencyDbFlags Flags = { };
        };

        class SetSelection final : public ClientPacket
        {
        public:
            explicit SetSelection(WorldPacket&& packet) : ClientPacket(CMSG_SET_SELECTION, std::move(packet)) { }

            void Read() override;

            ObjectGuid Selection; ///< Target
        };

        class SetupCurrency final : public ServerPacket
        {
        public:
            struct Record
            {
                int32 Type = 0;                       // ID from CurrencyTypes.dbc
                int32 Quantity = 0;
                Optional<int32> WeeklyQuantity;       // Currency count obtained this Week.
                Optional<int32> MaxWeeklyQuantity;    // Weekly Currency cap.
                Optional<int32> TrackedQuantity;
                Optional<int32> MaxQuantity;
                Optional<int32> TotalEarned;
                Optional<Timestamp<>> NextRechargeTime;
                Optional<Timestamp<>> RechargeCycleStartTime;
                uint8 Flags = 0;
            };

            explicit SetupCurrency() : ServerPacket(SMSG_SETUP_CURRENCY, 22) { }

            WorldPacket const* Write() override;

            std::vector<Record> Data;
        };

        class ViolenceLevel final : public ClientPacket
        {
        public:
            explicit ViolenceLevel(WorldPacket&& packet) : ClientPacket(CMSG_VIOLENCE_LEVEL, std::move(packet)) { }

            void Read() override;

            int8 ViolenceLvl = -1; ///< 0 - no combat effects, 1 - display some combat effects, 2 - blood, 3 - bloody, 4 - bloodier, 5 - bloodiest
        };

        class TimeSyncRequest final : public ServerPacket
        {
        public:
            explicit TimeSyncRequest() : ServerPacket(SMSG_TIME_SYNC_REQUEST, 4) { }

            WorldPacket const* Write() override;

            uint32 SequenceIndex = 0;
        };

        class TimeSyncResponse final : public ClientPacket
        {
        public:
            explicit TimeSyncResponse(WorldPacket&& packet) : ClientPacket(CMSG_TIME_SYNC_RESPONSE, std::move(packet)) { }

            void Read() override;

            TimePoint GetReceivedTime() const { return _worldPacket.GetReceivedTime(); }

            uint32 ClientTime = 0; // Client ticks in ms
            uint32 SequenceIndex = 0; // Same index as in request
        };

        class TriggerCinematic final : public ServerPacket
        {
        public:
            explicit TriggerCinematic() : ServerPacket(SMSG_TRIGGER_CINEMATIC, 4) { }

            WorldPacket const* Write() override;

            uint32 CinematicID = 0;
            ObjectGuid ConversationGuid;
        };

        class TriggerMovie final : public ServerPacket
        {
        public:
            explicit TriggerMovie() : ServerPacket(SMSG_TRIGGER_MOVIE, 4) { }

            WorldPacket const* Write() override;

            uint32 MovieID = 0;
        };

        class ServerTimeOffsetRequest final : public ClientPacket
        {
        public:
            explicit ServerTimeOffsetRequest(WorldPacket&& packet) : ClientPacket(CMSG_SERVER_TIME_OFFSET_REQUEST, std::move(packet)) { }

            void Read() override { }
        };

        class ServerTimeOffset final : public ServerPacket
        {
        public:
            explicit ServerTimeOffset() : ServerPacket(SMSG_SERVER_TIME_OFFSET, 4) { }

            WorldPacket const* Write() override;

            Timestamp<> Time;
        };

        class TutorialFlags : public ServerPacket
        {
        public:
            explicit TutorialFlags() : ServerPacket(SMSG_TUTORIAL_FLAGS, 32) { }

            WorldPacket const* Write() override;

            std::array<uint32, MAX_ACCOUNT_TUTORIAL_VALUES> TutorialData = { };
        };

        class TutorialSetFlag final : public ClientPacket
        {
        public:
            explicit TutorialSetFlag(WorldPacket&& packet) : ClientPacket(CMSG_TUTORIAL, std::move(packet)) { }

            void Read() override;

            uint8 Action = 0;
            uint32 TutorialBit = 0;
        };

        class WorldServerInfo final : public ServerPacket
        {
        public:
            explicit WorldServerInfo() : ServerPacket(SMSG_WORLD_SERVER_INFO, 26) { }

            WorldPacket const* Write() override;

            int16 DifficultyID      = 0;
            bool IsTournamentRealm  = false;
            bool XRealmPvpAlert     = false;
            bool BlockExitingLoadingScreen = false;     // when set to true, sending SMSG_UPDATE_OBJECT with CreateObject Self bit = true will not hide loading screen
                                                        // instead it will be done after this packet is sent again with false in this bit and SMSG_UPDATE_OBJECT Values for player
            Optional<uint32> RestrictedAccountMaxLevel;
            Optional<uint64> RestrictedAccountMaxMoney;
            Optional<uint32> InstanceGroupSize;

            ObjectGuid HouseGUID;
            ObjectGuid HouseOwnerAccountGUID;
            ObjectGuid HouseCosmeticOwnerGUID;
            ObjectGuid NeighborhoodGUID;
        };

        class SetDungeonDifficulty final : public ClientPacket
        {
        public:
            explicit SetDungeonDifficulty(WorldPacket&& packet) : ClientPacket(CMSG_SET_DUNGEON_DIFFICULTY, std::move(packet)) { }

            void Read() override;

            int16 DifficultyID = 0;
        };

        class SetRaidDifficulty final : public ClientPacket
        {
        public:
            explicit SetRaidDifficulty(WorldPacket&& packet) : ClientPacket(CMSG_SET_RAID_DIFFICULTY, std::move(packet)) { }

            void Read() override;

            int32 Legacy = 0;
            int16 DifficultyID = 0;
        };

        class DungeonDifficultySet final : public ServerPacket
        {
        public:
            explicit DungeonDifficultySet() : ServerPacket(SMSG_SET_DUNGEON_DIFFICULTY, 4) { }

            WorldPacket const* Write() override;

            int16 DifficultyID = 0;
        };

        class RaidDifficultySet final : public ServerPacket
        {
        public:
            explicit RaidDifficultySet() : ServerPacket(SMSG_RAID_DIFFICULTY_SET, 4 + 1) { }

            WorldPacket const* Write() override;

            int32 Legacy = 0;
            int16 DifficultyID = 0;
        };

        class CorpseReclaimDelay : public ServerPacket
        {
        public:
            explicit CorpseReclaimDelay() : ServerPacket(SMSG_CORPSE_RECLAIM_DELAY, 4) { }

            WorldPacket const* Write() override;

            uint32 Remaining = 0;
        };

        class DeathReleaseLoc : public ServerPacket
        {
        public:
            explicit DeathReleaseLoc() : ServerPacket(SMSG_DEATH_RELEASE_LOC, 4 + (3 * 4)) { }

            WorldPacket const* Write() override;

            int32 MapID = 0;
            TaggedPosition<Position::XYZ> Loc;
        };

        class PortGraveyard final : public ClientPacket
        {
        public:
            explicit PortGraveyard(WorldPacket&& packet) : ClientPacket(CMSG_CLIENT_PORT_GRAVEYARD, std::move(packet)) { }

            void Read() override { }
        };

        class PreRessurect : public ServerPacket
        {
        public:
            explicit PreRessurect() : ServerPacket(SMSG_PRE_RESSURECT, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid PlayerGUID;
        };

        class ReclaimCorpse final : public ClientPacket
        {
        public:
            explicit ReclaimCorpse(WorldPacket&& packet) : ClientPacket(CMSG_RECLAIM_CORPSE, std::move(packet)) { }

            void Read() override;

            ObjectGuid CorpseGUID;
        };

        class RepopRequest final : public ClientPacket
        {
        public:
            explicit RepopRequest(WorldPacket&& packet) : ClientPacket(CMSG_REPOP_REQUEST, std::move(packet)) { }

            void Read() override;

            bool CheckInstance = false;
        };

        class RequestCemeteryList final : public ClientPacket
        {
        public:
            explicit RequestCemeteryList(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_CEMETERY_LIST, std::move(packet)) { }

            void Read() override { }
        };

        class RequestCemeteryListResponse final : public ServerPacket
        {
        public:
            explicit RequestCemeteryListResponse() : ServerPacket(SMSG_REQUEST_CEMETERY_LIST_RESPONSE, 1) { }

            WorldPacket const* Write() override;

            bool IsGossipTriggered = false;
            std::vector<uint32> CemeteryID;
        };

        class ResurrectResponse final : public ClientPacket
        {
        public:
            explicit ResurrectResponse(WorldPacket&& packet) : ClientPacket(CMSG_RESURRECT_RESPONSE, std::move(packet)) { }

            void Read() override;

            ObjectGuid Resurrecter;
            uint32 Response = 0;
        };

        class TC_GAME_API Weather final : public ServerPacket
        {
        public:
            explicit Weather() : ServerPacket(SMSG_WEATHER, 4 + 4 + 1) { }
            explicit Weather(WeatherState weatherID, float intensity = 0.0f, bool abrupt = false) : ServerPacket(SMSG_WEATHER, 4 + 4 + 1),
                Abrupt(abrupt), Intensity(intensity), WeatherID(weatherID) { }

            WorldPacket const* Write() override;

            bool Abrupt = false;
            float Intensity = 0.0f;
            WeatherState WeatherID = WeatherState(0);
        };

        class StandStateChange final : public ClientPacket
        {
        public:
            explicit StandStateChange(WorldPacket&& packet) : ClientPacket(CMSG_STAND_STATE_CHANGE, std::move(packet)) { }

            void Read() override;

            UnitStandStateType StandState = UnitStandStateType(0);
        };

        class StandStateUpdate final : public ServerPacket
        {
        public:
            explicit StandStateUpdate() : ServerPacket(SMSG_STAND_STATE_UPDATE, 4 + 1) { }
            explicit StandStateUpdate(UnitStandStateType state, uint32 animKitID) : ServerPacket(SMSG_STAND_STATE_UPDATE, 4 + 1),
                AnimKitID(animKitID), State(state) { }

            WorldPacket const* Write() override;

            uint32 AnimKitID = 0;
            UnitStandStateType State = UnitStandStateType(0);
        };

        class SetAnimTier final : public ServerPacket
        {
        public:
            explicit SetAnimTier(): ServerPacket(SMSG_SET_ANIM_TIER, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint8 Tier = 0;
        };

        class StartMirrorTimer final : public ServerPacket
        {
        public:
            explicit StartMirrorTimer() : ServerPacket(SMSG_START_MIRROR_TIMER, 1 + 4 + 4 + 4 + 4 + 1) { }
            explicit StartMirrorTimer(uint8 timer, int32 value, int32 maxValue, int32 scale, int32 spellID, bool paused)
                : ServerPacket(SMSG_START_MIRROR_TIMER, 1 + 4 + 4 + 4 + 4 + 1),
                Timer(timer), Scale(scale), MaxValue(maxValue), SpellID(spellID), Value(value), Paused(paused) { }

            WorldPacket const* Write() override;

            uint8 Timer = 0;
            int32 Scale = 0;
            int32 MaxValue = 0;
            int32 SpellID = 0;
            int32 Value = 0;
            bool Paused = false;
        };

        class PauseMirrorTimer final : public ServerPacket
        {
        public:
            explicit PauseMirrorTimer() : ServerPacket(SMSG_PAUSE_MIRROR_TIMER, 1 + 1) { }
            explicit PauseMirrorTimer(uint8 timer, bool paused) : ServerPacket(SMSG_PAUSE_MIRROR_TIMER, 1 + 1),
                Timer(timer), Paused(paused) { }

            WorldPacket const* Write() override;

            uint8 Timer = 0;
            bool Paused = true;
        };

        class StopMirrorTimer final : public ServerPacket
        {
        public:
            explicit StopMirrorTimer() : ServerPacket(SMSG_STOP_MIRROR_TIMER, 1) { }
            explicit StopMirrorTimer(uint8 timer) : ServerPacket(SMSG_STOP_MIRROR_TIMER, 1), Timer(timer) { }

            WorldPacket const* Write() override;

            uint8 Timer = 0;
        };

        class ExplorationExperience final : public ServerPacket
        {
        public:
            explicit ExplorationExperience() : ServerPacket(SMSG_EXPLORATION_EXPERIENCE, 8) { }
            explicit ExplorationExperience(int32 experience, int32 areaID) : ServerPacket(SMSG_EXPLORATION_EXPERIENCE, 8),
                Experience(experience), AreaID(areaID) { }

            WorldPacket const* Write() override;

            int32 Experience = 0;
            int32 AreaID = 0;
        };

        class LevelUpInfo final : public ServerPacket
        {
        public:
            explicit LevelUpInfo() : ServerPacket(SMSG_LEVEL_UP_INFO, 60) { }

            WorldPacket const* Write() override;

            int32 Level = 0;
            int32 HealthDelta = 0;
            std::array<int32, MAX_POWERS_PER_CLASS> PowerDelta = { };
            std::array<int32, MAX_STATS> StatDelta = { };
            int32 NumNewTalents = 0;
            int32 NumNewPvpTalentSlots = 0;
        };

        class PlayMusic final : public ServerPacket
        {
        public:
            explicit PlayMusic() : ServerPacket(SMSG_PLAY_MUSIC, 4) { }
            explicit PlayMusic(uint32 soundKitID) : ServerPacket(SMSG_PLAY_MUSIC, 4), SoundKitID(soundKitID) { }

            WorldPacket const* Write() override;

            uint32 SoundKitID = 0;
        };

        class RandomRollClient final : public ClientPacket
        {
        public:
            explicit RandomRollClient(WorldPacket&& packet) : ClientPacket(CMSG_RANDOM_ROLL, std::move(packet)) { }

            void Read() override;

            int32 Min = 0;
            int32 Max = 0;
            Optional<uint8> PartyIndex;
        };

        class RandomRoll final : public ServerPacket
        {
        public:
            explicit RandomRoll() : ServerPacket(SMSG_RANDOM_ROLL, 16 + 16 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Roller;
            ObjectGuid RollerWowAccount;
            int32 Min = 0;
            int32 Max = 0;
            int32 Result = 0;
        };

        class EnableBarberShop final : public ServerPacket
        {
        public:
            explicit EnableBarberShop() : ServerPacket(SMSG_ENABLE_BARBER_SHOP, 1) { }

            WorldPacket const* Write() override;

            uint32 CustomizationFeatureMask = 0;
        };

        struct PhaseShiftDataPhase
        {
            uint32 PhaseFlags = 0;
            uint16 Id = 0;
        };

        struct PhaseShiftData
        {
            uint32 PhaseShiftFlags = 0;
            std::vector<PhaseShiftDataPhase> Phases;
            ObjectGuid PersonalGUID;
        };

        class PhaseShiftChange final : public ServerPacket
        {
        public:
            explicit PhaseShiftChange() : ServerPacket(SMSG_PHASE_SHIFT_CHANGE, 16 + 4 + 4 + 16 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Client;
            PhaseShiftData Phaseshift;
            std::vector<uint16> PreloadMapIDs;
            std::vector<uint16> UiMapPhaseIDs;
            std::vector<uint16> VisibleMapIDs;
        };

        class ZoneUnderAttack final : public ServerPacket
        {
        public:
            explicit ZoneUnderAttack() : ServerPacket(SMSG_ZONE_UNDER_ATTACK, 4) { }

            WorldPacket const* Write() override;

            int32 AreaID = 0;
        };

        class DurabilityDamageDeath final : public ServerPacket
        {
        public:
            explicit DurabilityDamageDeath() : ServerPacket(SMSG_DURABILITY_DAMAGE_DEATH, 4) { }

            WorldPacket const* Write() override;

            int32 Percent = 0;
        };

        class ObjectUpdateFailed final : public ClientPacket
        {
        public:
            explicit ObjectUpdateFailed(WorldPacket&& packet) : ClientPacket(CMSG_OBJECT_UPDATE_FAILED, std::move(packet)) { }

            void Read() override;

            ObjectGuid ObjectGUID;
        };

        class ObjectUpdateRescued final : public ClientPacket
        {
        public:
            explicit ObjectUpdateRescued(WorldPacket&& packet) : ClientPacket(CMSG_OBJECT_UPDATE_RESCUED, std::move(packet)) { }

            void Read() override;

            ObjectGuid ObjectGUID;
        };

        class PlayObjectSound final : public ServerPacket
        {
        public:
            explicit PlayObjectSound() : ServerPacket(SMSG_PLAY_OBJECT_SOUND, 16 + 16 + 4 + 4 * 3 + 4) { }
            explicit PlayObjectSound(ObjectGuid targetObjectGUID, ObjectGuid sourceObjectGUID, int32 soundKitID, TaggedPosition<::Position::XYZ> position, int32 broadcastTextID)
                : ServerPacket(SMSG_PLAY_OBJECT_SOUND, 16 + 16 + 4 + 4 * 3),
                TargetObjectGUID(targetObjectGUID), SourceObjectGUID(sourceObjectGUID), SoundKitID(soundKitID), Position(position),
                BroadcastTextID(broadcastTextID) { }

            WorldPacket const* Write() override;

            ObjectGuid TargetObjectGUID;
            ObjectGuid SourceObjectGUID;
            int32 SoundKitID = 0;
            TaggedPosition<::Position::XYZ> Position;
            int32 BroadcastTextID = 0;
        };

        class TC_GAME_API PlaySound final : public ServerPacket
        {
        public:
            explicit PlaySound() : ServerPacket(SMSG_PLAY_SOUND, 16 + 4 + 4) { }
            explicit PlaySound(ObjectGuid sourceObjectGuid, int32 soundKitID, int32 broadcastTextId) : ServerPacket(SMSG_PLAY_SOUND, 16 + 4 + 4),
                SourceObjectGuid(sourceObjectGuid), SoundKitID(soundKitID), BroadcastTextID(broadcastTextId) { }

            WorldPacket const* Write() override;

            ObjectGuid SourceObjectGuid;
            int32 SoundKitID = 0;
            int32 BroadcastTextID = 0;
        };

        class PlaySpeakerbotSound final : public ServerPacket
        {
        public:
            explicit PlaySpeakerbotSound(ObjectGuid const& sourceObjectGUID, int32 soundKitID)
                : ServerPacket(SMSG_PLAY_SPEAKERBOT_SOUND, 20), SourceObjectGUID(sourceObjectGUID), SoundKitID(soundKitID) { }

            WorldPacket const* Write() override;

            ObjectGuid SourceObjectGUID;
            int32 SoundKitID = 0;
        };

        class StopSpeakerbotSound final : public ServerPacket
        {
        public:
            explicit StopSpeakerbotSound(ObjectGuid const& sourceObjectGUID)
                : ServerPacket(SMSG_STOP_SPEAKERBOT_SOUND, 16), SourceObjectGUID(sourceObjectGUID) { }

            WorldPacket const* Write() override;

            ObjectGuid SourceObjectGUID;
        };

        class CompleteCinematic final : public ClientPacket
        {
        public:
            explicit CompleteCinematic(WorldPacket&& packet) : ClientPacket(CMSG_COMPLETE_CINEMATIC, std::move(packet)) { }

            void Read() override { }
        };

        class NextCinematicCamera final : public ClientPacket
        {
        public:
            explicit NextCinematicCamera(WorldPacket&& packet) : ClientPacket(CMSG_NEXT_CINEMATIC_CAMERA, std::move(packet)) { }

            void Read() override { }
        };

        class CompleteMovie final : public ClientPacket
        {
        public:
            explicit CompleteMovie(WorldPacket&& packet) : ClientPacket(CMSG_COMPLETE_MOVIE, std::move(packet)) { }

            void Read() override { }
        };

        class FarSight final : public ClientPacket
        {
        public:
            explicit FarSight(WorldPacket&& packet) : ClientPacket(CMSG_FAR_SIGHT, std::move(packet)) { }

            void Read() override;

            bool Enable = false;
        };

        class SaveCUFProfiles final : public ClientPacket
        {
        public:
            explicit SaveCUFProfiles(WorldPacket&& packet) : ClientPacket(CMSG_SAVE_CUF_PROFILES, std::move(packet)) { }

            void Read() override;

            Array<std::unique_ptr<CUFProfile>, MAX_CUF_PROFILES> CUFProfiles;
        };

        class LoadCUFProfiles final : public ServerPacket
        {
        public:
            explicit LoadCUFProfiles() : ServerPacket(SMSG_LOAD_CUF_PROFILES, 20) { }

            WorldPacket const* Write() override;

            std::vector<CUFProfile const*> CUFProfiles;
        };

        class PlayOneShotAnimKit final : public ServerPacket
        {
        public:
            explicit PlayOneShotAnimKit() : ServerPacket(SMSG_PLAY_ONE_SHOT_ANIM_KIT, 7 + 2) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint16 AnimKitID = 0;
        };

        class SetAIAnimKit final : public ServerPacket
        {
        public:
            explicit SetAIAnimKit() : ServerPacket(SMSG_SET_AI_ANIM_KIT, 16 + 2) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint16 AnimKitID = 0;
        };

        class SetMovementAnimKit final : public ServerPacket
        {
        public:
            explicit SetMovementAnimKit() : ServerPacket(SMSG_SET_MOVEMENT_ANIM_KIT, 16 + 2) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint16 AnimKitID = 0;
        };

        class SetMeleeAnimKit final : public ServerPacket
        {
        public:
            explicit SetMeleeAnimKit() : ServerPacket(SMSG_SET_MELEE_ANIM_KIT, 16 + 2) { }

            WorldPacket const* Write() override;

            ObjectGuid Unit;
            uint16 AnimKitID = 0;
        };

        class SetPlayHoverAnim final : public ServerPacket
        {
        public:
            explicit SetPlayHoverAnim() : ServerPacket(SMSG_SET_PLAY_HOVER_ANIM, 16 + 1) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            bool PlayHoverAnim = false;
        };

        class OpeningCinematic final : public ClientPacket
        {
        public:
            explicit OpeningCinematic(WorldPacket&& packet) : ClientPacket(CMSG_OPENING_CINEMATIC, std::move(packet)) { }

            void Read() override { }
        };

        class TogglePvP final : public ClientPacket
        {
        public:
            explicit TogglePvP(WorldPacket&& packet) : ClientPacket(CMSG_TOGGLE_PVP, std::move(packet)) { }

            void Read() override { }
        };

        class SetPvP final : public ClientPacket
        {
        public:
            explicit SetPvP(WorldPacket&& packet) : ClientPacket(CMSG_SET_PVP, std::move(packet)) { }

            void Read() override;

            bool EnablePVP = false;
        };

        class SetWarMode final : public ClientPacket
        {
        public:
            explicit SetWarMode(WorldPacket&& packet) : ClientPacket(CMSG_SET_WAR_MODE, std::move(packet)) { }

            void Read() override;

            bool Enable = false;
        };

        class AccountHeirloomUpdate final : public ServerPacket
        {
        public:
            explicit AccountHeirloomUpdate() : ServerPacket(SMSG_ACCOUNT_HEIRLOOM_UPDATE) { }

            WorldPacket const* Write() override;

            bool IsFullUpdate = false;
            std::map<uint32, HeirloomData> const* Heirlooms = nullptr;
            int32 ItemCollectionType = 0;
        };

        class MountSpecial final : public ClientPacket
        {
        public:
            explicit MountSpecial(WorldPacket&& packet) : ClientPacket(CMSG_MOUNT_SPECIAL_ANIM, std::move(packet)) { }

            void Read() override;

            Array<int32, 2> SpellVisualKitIDs;
            int32 SequenceVariation = 0;
        };

        class SpecialMountAnim final : public ServerPacket
        {
        public:
            explicit SpecialMountAnim() : ServerPacket(SMSG_SPECIAL_MOUNT_ANIM, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            std::vector<int32> SpellVisualKitIDs;
            int32 SequenceVariation = 0;
        };

        class CrossedInebriationThreshold final : public ServerPacket
        {
        public:
            explicit CrossedInebriationThreshold() : ServerPacket(SMSG_CROSSED_INEBRIATION_THRESHOLD, 16 + 4 + 4) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
            int32 ItemID = 0;
            int32 Threshold = 0;
        };

        class SetTaxiBenchmarkMode final : public ClientPacket
        {
        public:
            explicit SetTaxiBenchmarkMode(WorldPacket&& packet) : ClientPacket(CMSG_SET_TAXI_BENCHMARK_MODE, std::move(packet)) { }

            void Read() override;

            bool Enable = false;
        };

        class OverrideLight final : public ServerPacket
        {
        public:
            explicit OverrideLight() : ServerPacket(SMSG_OVERRIDE_LIGHT, 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            int32 AreaLightID = 0;
            int32 TransitionMilliseconds = 0;
            int32 OverrideLightID = 0;
        };

        class TC_GAME_API DisplayGameError final : public ServerPacket
        {
        public:
            explicit DisplayGameError(GameError error) : ServerPacket(SMSG_DISPLAY_GAME_ERROR, 4 + 1), Error(error) { }
            explicit DisplayGameError(GameError error, int32 arg) : ServerPacket(SMSG_DISPLAY_GAME_ERROR, 4 + 1 + 4), Error(error), Arg(arg) { }
            explicit DisplayGameError(GameError error, int32 arg1, int32 arg2) : ServerPacket(SMSG_DISPLAY_GAME_ERROR, 4 + 1 + 4 + 4), Error(error), Arg(arg1), Arg2(arg2) { }

            WorldPacket const* Write() override;

            GameError Error;
            Optional<int32> Arg;
            Optional<int32> Arg2;
        };

        class AccountMountUpdate final : public ServerPacket
        {
        public:
            explicit AccountMountUpdate() : ServerPacket(SMSG_ACCOUNT_MOUNT_UPDATE) { }

            WorldPacket const* Write() override;

            bool IsFullUpdate = false;
            MountContainer const* Mounts = nullptr;
        };

        class MountSetFavorite final : public ClientPacket
        {
        public:
            explicit MountSetFavorite(WorldPacket&& packet) : ClientPacket(CMSG_MOUNT_SET_FAVORITE, std::move(packet)) { }

            void Read() override;

            uint32 MountSpellID = 0;
            bool IsFavorite = false;
        };

        class CloseInteraction final : public ClientPacket
        {
        public:
            explicit CloseInteraction(WorldPacket&& packet) : ClientPacket(CMSG_CLOSE_INTERACTION, std::move(packet)) { }

            void Read() override;

            ObjectGuid SourceGuid;
        };

        class StartTimer final : public ServerPacket
        {
        public:
            explicit StartTimer() : ServerPacket(SMSG_START_TIMER, 8 + 4 + 8 + 1 + 16) { }

            WorldPacket const* Write() override;

            Duration<Seconds> TotalTime;
            Duration<Seconds> TimeLeft;
            CountdownTimerType Type = {};
            Optional<ObjectGuid> PlayerGuid;
        };

        class QueryCountdownTimer final : public ClientPacket
        {
        public:
            explicit QueryCountdownTimer(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_COUNTDOWN_TIMER, std::move(packet)) { }

            void Read() override;

            CountdownTimerType TimerType = {};
        };

        class ConversationLineStarted final : public ClientPacket
        {
        public:
            explicit ConversationLineStarted(WorldPacket&& packet) : ClientPacket(CMSG_CONVERSATION_LINE_STARTED, std::move(packet)) { }

            void Read() override;

            ObjectGuid ConversationGUID;
            uint32 LineID = 0;
        };

        class RequestLatestSplashScreen final : public ClientPacket
        {
        public:
            explicit RequestLatestSplashScreen(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_LATEST_SPLASH_SCREEN, std::move(packet)) { }

            void Read() override { }
        };

        class SplashScreenShowLatest final : public ServerPacket
        {
        public:
            explicit SplashScreenShowLatest() : ServerPacket(SMSG_SPLASH_SCREEN_SHOW_LATEST, 4) { }

            WorldPacket const* Write() override;

            int32 UISplashScreenID = 0;
        };

        class DisplayToast final : public ServerPacket
        {
        public:
            explicit DisplayToast() : ServerPacket(SMSG_DISPLAY_TOAST) { }

            WorldPacket const* Write() override;

            uint64 Quantity = 0;
            uint32 QuestID = 0;
            ::DisplayToastMethod DisplayToastMethod = { };
            bool Mailed = false;
            DisplayToastType Type = { };
            bool IsSecondaryResult = false;
            Item::ItemInstance Item;
            int32 LootSpec = 0;
            ::Gender Gender = GENDER_NONE;
            bool BonusRoll = false;
            bool ForceToast = false;    ///< Ignores ITEM_FLAG3_DO_NOT_TOAST
            uint32 CurrencyID = 0;
        };

        class AccountWarbandSceneUpdate final : public ServerPacket
        {
        public:
            explicit AccountWarbandSceneUpdate() : ServerPacket(SMSG_ACCOUNT_WARBAND_SCENE_UPDATE) { }

            WorldPacket const* Write() override;

            bool IsFullUpdate = false;
            WarbandSceneCollectionContainer const* WarbandScenes = nullptr;
        };

        // GM / Cheat / Debug block - client 12.1.0.69382, families 0x45 (SMSG) and 0x3D (CMSG).
        // Every wire layout below is taken from the client reader/writer, not from a recording:
        // none of these eleven opcodes occurs in any of our 2.38M analysed sniff records
        // (the situations that trigger them - GM account, kiosk client, developer build -
        // were never recorded and are not obtainable). See AGENT_BRIEF_W4_GM_DEBUG.md.

        // Enum.ConsoleColorType, ConsoleDocumentation.lua:219-240 (12 values).
        // The client indexes its colour table at 0x4374620 with this value and performs
        // NO range check (ConsoleGetColorFromType @ 0x0D38780) - values >= 12 read out of bounds.
        enum class ConsoleColorType : uint32
        {
            DefaultColor        = 0,
            InputColor          = 1,
            EchoColor           = 2,
            ErrorColor          = 3,
            WarningColor        = 4,
            GlobalColor         = 5,
            AdminColor          = 6,
            HighlightColor      = 7,
            BackgroundColor     = 8,
            ClickbufferColor    = 9,
            PrivateColor        = 10,
            DefaultGreen        = 11,

            Max                 = 12
        };

        // Number of debug views the client knows, table at 0x43BD1C0, read by DebugViewName @ 0x1CDC460.
        constexpr uint32 MAX_DEBUG_VIEWS = 40;

        // Reader 0x5F5E10: Read<uint8> + shr 7 -> one bit, 1 byte on the wire.
        // Consumer 0x1E1DAE0 writes "Godmode enabled"/"Godmode disabled" to the developer
        // console and does nothing else - the message is the receipt, not the mechanism.
        class GodMode final : public ServerPacket
        {
        public:
            explicit GodMode(bool enable = false) : ServerPacket(SMSG_GOD_MODE, 1), Enable(enable) { }

            WorldPacket const* Write() override;

            bool Enable = false;
        };

        // Reader 0x5EE690, bit-identical to GodMode. Consumer 0x1E2EDF0 prints
        // "Pet Godmode enabled"/"Pet Godmode disabled".
        class PetGodMode final : public ServerPacket
        {
        public:
            explicit PetGodMode(bool enable = false) : ServerPacket(SMSG_PET_GOD_MODE, 1), Enable(enable) { }

            WorldPacket const* Write() override;

            bool Enable = false;
        };

        // Reader 0x5F8C80, bit-identical to GodMode. Consumer 0x1DB7360 rebuilds every cooldown
        // display: spellbook, shapeshift bar, bags, and fires the Lua event PET_BAR_UPDATE_COOLDOWN
        // (hash 0xAE0EB1A02CFEB3DD, handled in PetActionBar.lua:88). It does not suppress cooldowns
        // itself - that stays server side (Spell.cpp).
        class CooldownCheat final : public ServerPacket
        {
        public:
            explicit CooldownCheat(bool enable = false) : ServerPacket(SMSG_COOLDOWN_CHEAT, 1), Enable(enable) { }

            WorldPacket const* Write() override;

            bool Enable = false;
        };

        // Reader 0x5EA250: bits<14> length (including the terminating NUL), flush, uint32 colour,
        // then the string. Consumer 0x1D26CD0 -> ConsoleWrite(COLOR_T, char const*) @ 0x32D240,
        // which feeds the developer console, ConsoleLog.cpp and the Lua event
        // CONSOLE_MESSAGE(message, colorType) handled in Blizzard_Console.lua:132-135.
        // Payload is 6 bytes for empty text, at most 16389 bytes (14 bit length -> 16384 buffer).
        class ConsoleWrite final : public ServerPacket
        {
        public:
            explicit ConsoleWrite() : ServerPacket(SMSG_CONSOLE_WRITE, 2 + 4 + 128) { }

            WorldPacket const* Write() override;

            // 16384 byte buffer, length includes the NUL, and ReadDynString rejects a length
            // whose last byte is not 0 - so 16382 payload characters is the hard maximum.
            static constexpr std::size_t MaxTextLength = 16382;

            std::string Text;
            ConsoleColorType ColorType = ConsoleColorType::DefaultColor;
        };

        // Reader 0x5EE9E0 is the opaque message class (no Read<T> at all), so the payload is defined
        // by the consumer 0x1CE3840: it reads exactly one float at offset 0 and stores it in the
        // global 0x43BF260. The only reader of that global, 0x1CDB530, advances the world clock by
        // one game minute per accumulated unit - the float is GAME MINUTES PER REAL SECOND, i.e. the
        // runtime version of the NewSpeed field of SMSG_LOGIN_SET_TIME_SPEED, not a movement or
        // animation rate. The client clamps to [1/60, 60]; 0.01666667f is real time and is exactly
        // the value TrinityCore hardcodes at login (Player.cpp, SendInitialPacketsBeforeAddToMap).
        class GameSpeedSet final : public ServerPacket
        {
        public:
            explicit GameSpeedSet(float speed = 0.01666667f) : ServerPacket(SMSG_GAME_SPEED_SET, 4), Speed(speed) { }

            WorldPacket const* Write() override;

            static constexpr float MinSpeed = 0.01666667f;
            static constexpr float MaxSpeed = 60.0f;

            float Speed = 0.01666667f;
        };

        // Empty by design: the consumer 0x4EF5F0 never touches the payload. What it does instead is
        // walk all 40 debug views and answer with one CMSG_SET_GAME_EVENT_DEBUG_VIEW_STATE(i, true)
        // per view that has a local listener - a re-subscription after a server restart or resync.
        // Do not send this without being able to receive the answer.
        class DebugMenuManagerFullUpdate final : public ServerPacket
        {
        public:
            explicit DebugMenuManagerFullUpdate() : ServerPacket(SMSG_DEBUG_MENU_MANAGER_FULL_UPDATE, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // Reader 0x5DE620: three scalars, then BOTH array counts, then both array payloads.
        // The obvious "count, array, count, array" order would produce a wrong wire format, and
        // because the consumer is the deliberate no-op stub at 0x1D9E30 (bytes c2 00 00 = ret 0)
        // the mistake would never surface. Payload is 20 + 4 * (Cooldowns + RuneTypes) bytes.
        class RuneRegenDebug final : public ServerPacket
        {
        public:
            explicit RuneRegenDebug() : ServerPacket(SMSG_RUNE_REGEN_DEBUG, 20) { }

            WorldPacket const* Write() override;

            // UNVERIFIED: the three scalars have no JAM descriptor, no consumer and no Lua side.
            // The names below follow Player::ResyncRunes, which keeps exactly the two lists that
            // the two arrays carry - that is an analogy, not a measurement.
            uint32 RegenTimer = 0;
            uint32 BaseCooldown = 0;
            uint32 ActiveRuneMask = 0;
            std::vector<int32> Cooldowns;   // UNVERIFIED: element meaning
            std::vector<int32> RuneTypes;   // UNVERIFIED: element meaning
        };

        // Reader 0x5F9FC0: PackedGuid, bits<9> length, flush, then the raw characters.
        // No NUL on the wire - the client appends it itself. The 9 bit width is confirmed twice:
        // the reader folds two bytes as (B0 << 1) | (B1 >> 7), and the message object is 560 bytes
        // with a 48 byte header, leaving exactly the 512 byte buffer that 9 bits address.
        // Consumer is the no-op stub 0x1D9E30 - the retail client parses and discards this.
        // UNVERIFIED: namespace of AnimName (open question F2). Neither AnimationData nor AnimKit
        // carries animation names, so it stays undecided whether this is a DB2 backed name or a
        // client internal one; the field is carried through verbatim.
        class ForceAnim final : public ServerPacket
        {
        public:
            explicit ForceAnim() : ServerPacket(SMSG_FORCE_ANIM, 18 + 2 + 32) { }

            WorldPacket const* Write() override;

            static constexpr std::size_t MaxAnimNameLength = 511;

            ObjectGuid UnitGUID;
            std::string AnimName;
        };

        // Reader 0x5FA0A0, field names from the JAM type ForceAnimationsData (tag 0x38951B8).
        // Both array counts precede both payloads, same trap as RuneRegenDebug. Speed is a float:
        // movss emission at 0x5FA17D/0x5FA18B, reader default 0x3F800000, JAM descriptor 0x37AB200
        // (the float descriptor), and the field name all agree. BoneType is a full byte, not a bit.
        // Consumer is the no-op stub 0x1D9E30.
        class ForceAnimations final : public ServerPacket
        {
        public:
            explicit ForceAnimations() : ServerPacket(SMSG_FORCE_ANIMATIONS, 18 + 17) { }

            WorldPacket const* Write() override;

            ObjectGuid UnitGUID;
            std::vector<int32> AnimIDs;     // AnimationData::ID, DB2 FDID 1375431
            std::vector<uint8> Variations;
            uint32 LoopCount = 1;
            float Speed = 1.0f;
            uint8 BoneType = 0;
        };

        // Writer 0x6CD330 (body 0x6A35C0), identity proven through vtable slot 3 at 0x6CD390:
        // mov dword ptr [rdx], 0x3d009a. One bit, 1 byte, and the client hardcodes it to true -
        // there is no Kiosk.DisableGodMode, the state is meant to expire with the session.
        // Sent from Lua Kiosk.EnableGodMode() (0x11684B0), called in
        // Blizzard_Kiosk/Housing/Game.lua:186 after C_Housing.IsInsideHouse(): a kiosk visitor
        // must not be able to die in the demo house. Gated on the kiosk mode flag inside the
        // client, so a normal retail client never sends it.
        class KioskEnableGodMode final : public ClientPacket
        {
        public:
            explicit KioskEnableGodMode(WorldPacket&& packet) : ClientPacket(CMSG_KIOSK_ENABLE_GOD_MODE, std::move(packet)) { }

            void Read() override;

            bool Enable = false;
        };

        // Writer 0x6CCC40 (body 0x6A31A0): uint32 ViewIndex, then one bit State, then flush - 5 bytes.
        // Despite the name this has nothing to do with game_event/GameEventMgr: ViewIndex is an index
        // 0..39 into the compiled-in debug view table at 0x43BD1C0 ("Area Triggers", "AI Brain",
        // "Behavior Tree", "Pathing", "Aura Debugger", ...), whose entries match the message set of
        // family 0x4D. This is the subscription switch of the AI debug channel.
        // Two senders: 0x4EF5F0 (the answer to SMSG_DEBUG_MENU_MANAGER_FULL_UPDATE) and 0x4EF456
        // (a client console command of the form "<view name> <0|1>").
        class SetGameEventDebugViewState final : public ClientPacket
        {
        public:
            explicit SetGameEventDebugViewState(WorldPacket&& packet) : ClientPacket(CMSG_SET_GAME_EVENT_DEBUG_VIEW_STATE, std::move(packet)) { }

            void Read() override;

            uint32 ViewIndex = 0;
            bool State = false;
        };
    }
}

#endif // TRINITYCORE_MISC_PACKETS_H

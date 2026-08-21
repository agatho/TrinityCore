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
#include <string>
#include <string_view>
#include <vector>

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

        /*
         * Client family 0x64 (12.1.0.69382) - player state and UI remote control.
         *
         * Every wire layout below is read off the client dispatcher SMSG_Dispatch_fam_64 @ RVA 0x67C100
         * (image base 0x7FF780FD0000) unless a different reader RVA is named on the class.
         * Bit sections are MSB-first and are flushed before every byte-aligned field, which is exactly
         * ByteBuffer::WriteBits / FlushBits.
         */

        // Enum.SubscriptionInterstitialType - APIDoc/ExpansionDocumentation.lua:284-294
        enum class SubscriptionInterstitialType : uint8
        {
            Standard    = 0,
            LeftNpeArea = 1,
            MaxLevel    = 2     // shows the Upgrade button instead of the Subscribe button
        };

        // WIRE values of the CMSG_SUBSCRIPTION_INTERSTITIAL_RESPONSE answer - these are NOT the Lua
        // enum values. C_...SendSubscriptionInterstitialResponse (RVA 0xE948D0) remaps
        // Enum.SubscriptionInterstitialResponseType (APIDoc/ExpansionDocumentation.lua:271-281)
        // before writing bits<3>; the three-way branch at 0x7FF781E64987 is explicit:
        //   Lua Clicked(0)     -> wire 1     (body byte 0x20)
        //   Lua Closed(1)      -> wire 0     (body byte 0x00)
        //   Lua WebRedirect(2) -> wire 4     (body byte 0x80)
        // Taking the Lua numbering for the wire would silently swap Clicked and Closed.
        enum class SubscriptionInterstitialResponseType : uint8
        {
            Closed      = 0,
            Clicked     = 1,
            WebRedirect = 4
        };

        // SMSG_FAILED_PLAYER_CONDITION - wire 0x640002, 4 bytes.
        // Consumer 0x20970C0 reads exactly one uint32, looks it up in the PlayerCondition store
        // (off_7FF785B8D120, meta string "PlayerCondition") and prints Failure_description_lang.
        // An id without a failure description produces GameError 527 and no visible message.
        class FailedPlayerCondition final : public ServerPacket
        {
        public:
            explicit FailedPlayerCondition() : ServerPacket(SMSG_FAILED_PLAYER_CONDITION, 4) { }
            explicit FailedPlayerCondition(int32 playerConditionID)
                : ServerPacket(SMSG_FAILED_PLAYER_CONDITION, 4), PlayerConditionID(playerConditionID) { }

            WorldPacket const* Write() override;

            int32 PlayerConditionID = 0;
        };

        // SMSG_GM_REQUEST_PLAYER_INFO - wire 0x640003, 1..64 bytes.
        // Dispatcher case, literally:
        //   Read<uint8>(B); Flag = B >= 0x80; len = (B >> 1) & 0x3F; ReadBytes(len)
        // Both bit fields live in ONE bit section - a flush between them writes a byte too many.
        // UNVERIFIED: meaning of Flag. The retail consumer (hook 0x462DF10) is the shared no-op stub,
        // so the binary gives the structure but not the semantics.
        class GMRequestPlayerInfo final : public ServerPacket
        {
        public:
            explicit GMRequestPlayerInfo() : ServerPacket(SMSG_GM_REQUEST_PLAYER_INFO, 1 + 63) { }

            WorldPacket const* Write() override;

            std::string_view Name;      // client buffer is 64 bytes -> bits<6>, max 63 characters
            bool Flag = false;
        };

        // SMSG_GM_PLAYER_INFO - wire 0x64000E, reader RVA 0x67B1E0.
        // ObjectGuid, 2x uint32, uint8 (a full byte, not a bit), 9x uint32, then one bit section
        // holding four string lengths, then the four strings (no NUL on the wire).
        // Bit widths are cross-checked against the client buffer sizes:
        //   49 -> bits<6>, 97 -> bits<7>, 1281 -> bits<11>, 1281 -> bits<11>  (35 bits, 5 padding bits)
        // UNVERIFIED: meaning of every field. Consumer (hook 0x462DE70) is the shared no-op stub.
        class GMPlayerInfo final : public ServerPacket
        {
        public:
            explicit GMPlayerInfo() : ServerPacket(SMSG_GM_PLAYER_INFO, 16 + 11 * 4 + 1 + 5) { }

            WorldPacket const* Write() override;

            ObjectGuid Player;
            int32 Data1 = 0;
            int32 Data2 = 0;
            uint8 Data3 = 0;
            std::array<int32, 9> Data4 = { };
            std::string_view Text1;     // buffer   49 -> bits<6>,  max   48 characters
            std::string_view Text2;     // buffer   97 -> bits<7>,  max   96 characters
            std::string_view Text3;     // buffer 1281 -> bits<11>, max 1280 characters
            std::string_view Text4;     // buffer 1281 -> bits<11>, max 1280 characters
        };

        // SMSG_PLAYER_CONDITION_RESULT - wire 0x640013.
        // UNVERIFIED: the payload is undetermined. The dispatcher hands the consumer a raw pointer to
        // the unread rest of the stream (0x35AF730) and parses nothing itself; the consumer
        // (hook 0x462DE78) is the shared no-op stub, so there is no reader to derive a layout from.
        // There is no JamType and no sniff packet either. Sent empty until that changes - do not
        // invent fields here.
        class PlayerConditionResult final : public ServerPacket
        {
        public:
            explicit PlayerConditionResult() : ServerPacket(SMSG_PLAYER_CONDITION_RESULT, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_PLAYER_SKINNED - wire 0x64000F, 1 byte.
        // Case: Read<uint8> >> 7. Fires Lua PLAYER_SKINNED(hasFreeRepop)
        // (hash 0x69E51F7A6E5DEAE8). GameEvent.HandlePlayerSkinned closes the three resurrect
        // popups and prints DEATH_CORPSE_SKINNED; it ignores the argument.
        class PlayerSkinned final : public ServerPacket
        {
        public:
            explicit PlayerSkinned() : ServerPacket(SMSG_PLAYER_SKINNED, 1) { }
            explicit PlayerSkinned(bool freeForAll) : ServerPacket(SMSG_PLAYER_SKINNED, 1), FreeForAll(freeForAll) { }

            WorldPacket const* Write() override;

            bool FreeForAll = false;    // Lua calls it hasFreeRepop
        };

        // SMSG_PLAYER_TUTORIAL_UNHIGHLIGHT_SPELL - wire 0x640015, empty.
        // Lua TUTORIAL_UNHIGHLIGHT_SPELL, hash 0x7D0D6305B5100CA5, no arguments.
        class PlayerTutorialUnhighlightSpell final : public ServerPacket
        {
        public:
            explicit PlayerTutorialUnhighlightSpell() : ServerPacket(SMSG_PLAYER_TUTORIAL_UNHIGHLIGHT_SPELL, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_PLAYER_TUTORIAL_HIGHLIGHT_SPELL - wire 0x640016, 5..132 bytes.
        // Case: Read<uint32>, then 0x5D5340 = ReadBits(7), then ReadBytes(len) into a 128 byte buffer.
        // Lua TUTORIAL_HIGHLIGHT_SPELL, hash 0x8661FCAFDC55E1EA, payload { int SpellID; char* Tag }.
        // Tag is a GlobalString KEY, not display text: Blizzard_BoostTutorial.lua:200 does
        // `_G[textID] or textID`, so an unknown key is shown verbatim instead of being dropped.
        class PlayerTutorialHighlightSpell final : public ServerPacket
        {
        public:
            explicit PlayerTutorialHighlightSpell() : ServerPacket(SMSG_PLAYER_TUTORIAL_HIGHLIGHT_SPELL, 4 + 1 + 127) { }

            WorldPacket const* Write() override;

            int32 SpellID = 0;
            std::string_view GlobalStringTag;    // client buffer 128 -> bits<7>, max 127 characters
        };

        // SMSG_PLAYER_OPEN_SUBSCRIPTION_INTERSTITIAL - wire 0x640017, 1 byte.
        // Case calls 0x5D4FD0 = ReadBits(2) (mask `a2 & 3`); consumer 0x209CEA0 distinguishes 0/1/2 only.
        // Sniff (12.0.7): the single recorded packet is 1 byte 0x80 = 0b10...... = 2 = MaxLevel.
        // Lua SHOW_SUBSCRIPTION_INTERSTITIAL, hash 0x55507E51164C9D2B.
        class PlayerOpenSubscriptionInterstitial final : public ServerPacket
        {
        public:
            explicit PlayerOpenSubscriptionInterstitial() : ServerPacket(SMSG_PLAYER_OPEN_SUBSCRIPTION_INTERSTITIAL, 1) { }

            WorldPacket const* Write() override;

            SubscriptionInterstitialType Type = SubscriptionInterstitialType::Standard;
        };

        // Element of SMSG_SCHEDULED_AREA_POI_UPDATE_RESPONSE, 24 bytes on the wire.
        // Array element type is ScheduledAreaPOIEventData (resizer RVA 0x692060 names it).
        struct ScheduledAreaPoiEvent
        {
            uint64 StartTime = 0;               // UNVERIFIED: read as uint64 at element offset +0
            uint64 EndTime = 0;                 // UNVERIFIED: read as uint64 at element offset +8
            int32 EventSchedulerEventID = 0;    // consumer 0x2428BB0 looks this up in DB2 EventSchedulerEvent
            int32 Data = 0;                     // UNVERIFIED: read as uint32 at element offset +20
        };

        // SMSG_SCHEDULED_AREA_POI_UPDATE_RESPONSE - wire 0x64001A.
        // Case, in order: Read<uint32> CountA; resize; Read<uint32> CountB; resize;
        //                 CountA x Read<uint32>; CountB x { u64, u64, u32, u32 }
        // Both counts come first, then both payload arrays - the array rule from
        // BEFUND_ai_debug_kanal_4D_69382.md 3.4.
        // The TrinityCore name is misleading in 12.1: the consumer resolves the second array against
        // DB2 EventSchedulerEvent (store off_7FF787548ED0, meta string "EventSchedulerEvent") and fires
        // Lua EVENT_SCHEDULER_UPDATE (hash 0x1FD62AAC7F60989D), not AREA_POIS_UPDATED. The first array
        // still carries AreaPOI ids.
        class ScheduledAreaPoiUpdateResponse final : public ServerPacket
        {
        public:
            explicit ScheduledAreaPoiUpdateResponse() : ServerPacket(SMSG_SCHEDULED_AREA_POI_UPDATE_RESPONSE, 4 + 4) { }

            WorldPacket const* Write() override;

            std::vector<int32> AreaPoiIDs;
            std::vector<ScheduledAreaPoiEvent> Events;
        };

        // SMSG_PLAYER_BONUS_ROLL_FAILED - wire 0x640022, empty.
        // Lua BONUS_ROLL_FAILED, hash 0xB334D6421BDC2DAD, no arguments. The client shows no reason,
        // so an error code on the wire would have no effect.
        class PlayerBonusRollFailed final : public ServerPacket
        {
        public:
            explicit PlayerBonusRollFailed() : ServerPacket(SMSG_PLAYER_BONUS_ROLL_FAILED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_CHECK_ABANDON_NPE - wire 0x640024, empty.
        // Lua LEAVING_TUTORIAL_AREA, hash 0xF9F41B606E43E658, no arguments; routed to
        // GameEvent.HandleLeavingTutorialArea -> StaticPopup_Show("LEAVING_TUTORIAL_AREA")
        // (EventImplementation.lua:259, EventRouting.lua:57). The warning text is client side and
        // faction dependent (NPE_ABANDON_A_WARNING / NPE_ABANDON_H_WARNING). The client answers with
        // CMSG_ABANDON_NPE_RESPONSE.
        class CheckAbandonNPE final : public ServerPacket
        {
        public:
            explicit CheckAbandonNPE() : ServerPacket(SMSG_CHECK_ABANDON_NPE, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // SMSG_PLAYER_SHOW_UI_EVENT_TOAST - wire 0x640025, 4 bytes.
        // Subscriber 0x2264F10 -> 0x225CFF0 reads one uint32 and resolves it in DB2 UIEventToast
        // (store meta string "UIEventToast"). Sniff 12.1: 3 packets, 4 bytes each, ids 341/304/327.
        // Hard client side gate (0x225CFF0): only UIEventToast.EventType in
        // { 12, 13, 16, 17, 18, 19, 20, 22 } is processed, anything else is dropped without a message.
        // This is NOT the embedded WorldPackets::Item::UiEventToast pair - the standalone opcode
        // carries a bare uint32.
        class PlayerShowUiEventToast final : public ServerPacket
        {
        public:
            explicit PlayerShowUiEventToast() : ServerPacket(SMSG_PLAYER_SHOW_UI_EVENT_TOAST, 4) { }
            explicit PlayerShowUiEventToast(int32 uiEventToastID)
                : ServerPacket(SMSG_PLAYER_SHOW_UI_EVENT_TOAST, 4), UiEventToastID(uiEventToastID) { }

            WorldPacket const* Write() override;

            int32 UiEventToastID = 0;   // -> UIEventToastEntry::ID (WoWDBDefs calls the column EventToastID)
        };

        // SMSG_PLAYER_SHOW_GENERIC_WIDGET_DISPLAY - wire 0x64002A, 4 bytes.
        // Subscriber 0x2490DB0 -> 0x24907D0 reads one uint32 and resolves it in DB2
        // UIGenericWidgetDisplay (store off_7FF787558FD0). Lua GENERIC_WIDGET_DISPLAY_SHOW,
        // hash 0x75039F3661BC513E. It carries the table id, not a raw UiWidgetSet::ID.
        class PlayerShowGenericWidgetDisplay final : public ServerPacket
        {
        public:
            explicit PlayerShowGenericWidgetDisplay() : ServerPacket(SMSG_PLAYER_SHOW_GENERIC_WIDGET_DISPLAY, 4) { }
            explicit PlayerShowGenericWidgetDisplay(int32 uiGenericWidgetDisplayID)
                : ServerPacket(SMSG_PLAYER_SHOW_GENERIC_WIDGET_DISPLAY, 4), UiGenericWidgetDisplayID(uiGenericWidgetDisplayID) { }

            WorldPacket const* Write() override;

            int32 UiGenericWidgetDisplayID = 0;     // -> UIGenericWidgetDisplay::ID
        };

        // SMSG_PLAYER_SHOW_PARTY_POSE_UI - wire 0x64002B, 5 bytes.
        // Case: Read<uint32>, then Read<uint8> >> 7. Lua SHOW_PARTY_POSE_UI, hash 0xA1EC8E9745484821,
        // payload { int PartyPoseID; bool Won }. EventImplementation.lua:163-165 passes the first
        // argument to C_PartyPose.GetPartyPoseInfoByID - so this is a UiPartyPose::ID, not a MapID.
        // The Victory bit switches only the model scene and the sound; the title text is shared.
        class PlayerShowPartyPoseUI final : public ServerPacket
        {
        public:
            explicit PlayerShowPartyPoseUI() : ServerPacket(SMSG_PLAYER_SHOW_PARTY_POSE_UI, 4 + 1) { }

            WorldPacket const* Write() override;

            int32 PartyPoseID = 0;      // -> UiPartyPose::ID
            bool Victory = false;
        };

        // SMSG_PLAYER_SHOW_ARROW_CALLOUT - wire 0x64002C, 4 bytes.
        // Subscriber 0x2328040 -> 0x2327730 reads one uint32 and resolves it in DB2 UIArrowCallout
        // (store off_7FF78754A4B0). Lua SHOW_ARROW_CALLOUT, hash 0xE999F5CBCB426641, payload is the
        // eight field calloutInfo table - which the client builds FROM the DB2 row, not from the wire.
        // Two client side gates: UIArrowCallout field 5 is a PlayerConditionID that must pass, and the
        // account CVar `acknowledgedArrowCallouts` (32 bit field) must not have bit CalloutID set.
        class PlayerShowArrowCallout final : public ServerPacket
        {
        public:
            explicit PlayerShowArrowCallout() : ServerPacket(SMSG_PLAYER_SHOW_ARROW_CALLOUT, 4) { }
            explicit PlayerShowArrowCallout(int32 arrowCalloutID)
                : ServerPacket(SMSG_PLAYER_SHOW_ARROW_CALLOUT, 4), ArrowCalloutID(arrowCalloutID) { }

            WorldPacket const* Write() override;

            int32 ArrowCalloutID = 0;   // -> UIArrowCallout::ID
        };

        // SMSG_PLAYER_HIDE_ARROW_CALLOUT - wire 0x64002D, 4 bytes.
        // Subscriber 0x2327FF0 -> 0x23275F0. Lua HIDE_ARROW_CALLOUT, hash 0x086B0038DB2C8725.
        class PlayerHideArrowCallout final : public ServerPacket
        {
        public:
            explicit PlayerHideArrowCallout() : ServerPacket(SMSG_PLAYER_HIDE_ARROW_CALLOUT, 4) { }
            explicit PlayerHideArrowCallout(int32 arrowCalloutID)
                : ServerPacket(SMSG_PLAYER_HIDE_ARROW_CALLOUT, 4), ArrowCalloutID(arrowCalloutID) { }

            WorldPacket const* Write() override;

            int32 ArrowCalloutID = 0;
        };

        // SMSG_PLAYER_ACKNOWLEDGE_ARROW_CALLOUT - wire 0x64002E, 4 bytes.
        // Subscriber 0x2327F60 sets the CVar bit for this id and then hides it via 0x23275F0.
        // This is the server -> client direction: "treat this callout as acknowledged".
        // There is no client -> server counterpart - C_ArrowCalloutManager.AcknowledgeCallout (0xA9A4E0)
        // only writes the CVar bit locally and sends nothing, so the server never learns about a
        // player side acknowledgement and has to track it itself if it needs to know.
        class PlayerAcknowledgeArrowCallout final : public ServerPacket
        {
        public:
            explicit PlayerAcknowledgeArrowCallout() : ServerPacket(SMSG_PLAYER_ACKNOWLEDGE_ARROW_CALLOUT, 4) { }
            explicit PlayerAcknowledgeArrowCallout(int32 arrowCalloutID)
                : ServerPacket(SMSG_PLAYER_ACKNOWLEDGE_ARROW_CALLOUT, 4), ArrowCalloutID(arrowCalloutID) { }

            WorldPacket const* Write() override;

            int32 ArrowCalloutID = 0;
        };

        // SMSG_PLAYER_END_OF_MATCH_DETAILS - wire 0x640030, 13 bytes.
        // Case: 3x Read<uint32>, then Read<uint8> >> 7.
        // Subscriber 0x2424C70 files the three values under Enum.MatchDetailType
        // { Placement = 0, Kills = 1, PlunderAcquired = 2 } and hardcodes matchType = 1 (Plunderstorm),
        // which is why the type is not on the wire. Lua SHOW_END_OF_MATCH_UI, hash 0xBEB85F3DE5B8121B.
        // Placement is 1 based (Blizzard_EndOfMatchUI.lua:34-42: 1 = won, 2 = lost).
        // MatchEnded = false means "you died, the match continues" and keeps the spectate path open.
        class PlayerEndOfMatchDetails final : public ServerPacket
        {
        public:
            explicit PlayerEndOfMatchDetails() : ServerPacket(SMSG_PLAYER_END_OF_MATCH_DETAILS, 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            int32 Placement = 0;            // 1 based
            int32 Kills = 0;
            int32 PlunderAcquired = 0;
            bool MatchEnded = false;
        };

        // SMSG_CHALLENGE_MODE_SET_LEAVER_PENALTY_TIMER - wire 0x640031, 4 bytes.
        // Consumer 0x235D550: `v1 = (int)(float)((float)(int)**(_QWORD**)(msg + 32) * 1000.0)` - the
        // value is seconds and is turned into an absolute millisecond deadline.
        // Seconds != 0 fires CHALLENGE_MODE_LEAVER_TIMER_STARTED (hash 0x762E5814E6D1F595),
        // Seconds == 0 fires CHALLENGE_MODE_LEAVER_TIMER_ENDED (hash 0x762A15EA11CAA7BD).
        // One opcode serves both directions; the dialog is rebuilt on PLAYER_ENTERING_WORLD
        // (InstanceAbandon.lua:146-148), so the server must resend after zone change and relog.
        // UNVERIFIED: field width. The consumer dereferences the raw buffer as _QWORD* but only ever
        // uses the low int half, so 8 bytes are not strictly excluded; uint32 is the derived reading.
        class ChallengeModeSetLeaverPenaltyTimer final : public ServerPacket
        {
        public:
            explicit ChallengeModeSetLeaverPenaltyTimer() : ServerPacket(SMSG_CHALLENGE_MODE_SET_LEAVER_PENALTY_TIMER, 4) { }
            explicit ChallengeModeSetLeaverPenaltyTimer(Seconds timer)
                : ServerPacket(SMSG_CHALLENGE_MODE_SET_LEAVER_PENALTY_TIMER, 4), Timer(timer) { }

            WorldPacket const* Write() override;

            Duration<Seconds, int32> Timer;
        };

        // One name/value pair of JamClientPlayerUploadScreenshotHeader, 80 bytes in the client struct.
        // UNVERIFIED (deduced from the shape, no type name proves it): HTTP request headers.
        struct UploadScreenshotHeaderField
        {
            std::string Name;       // bits<10> length INCLUDING the NUL
            std::string Value;      // bits<10> length INCLUDING the NUL
        };

        // JamClientPlayerUploadScreenshotHeader, reader RVA 0x67BF70:
        //   bits<13> UrlLen (incl. NUL); FlushBits; uint32 HeaderCount; JamDynamicString Url;
        //   HeaderCount x { bits<10> NameLen; bits<10> ValueLen; FlushBits; string Name; string Value }
        // The 13/10/10 widths are read off the byte recombinations in the reader, not guessed:
        //   UrlLen   = (B0 << 5) | (B1 >> 3)
        //   NameLen  = (C0 << 2) | (C1 >> 6)
        //   ValueLen = ((C1 & 0x3F) << 4) | (C2 >> 4)
        // Every string is a JamDynamicString: the length includes the terminator and the client
        // rejects the whole message if the last byte is not 0x00 (0x347D750 returns 0).
        struct UploadScreenshotHeader
        {
            std::string Url;
            std::vector<UploadScreenshotHeaderField> Headers;
        };

        // SMSG_PLAYER_UPLOAD_SCREENSHOT - wire 0x640032.
        // NOTE: inert in the retail client. off_7FF7855FDCC0[0] is set to nullptr by 0x226D10 and has
        // no other writer, and there is no Lua surface at all (no UploadScreenshot / no
        // PLAYER_UPLOAD_SCREENSHOT anywhere in the UI source). The message is real and fully specified;
        // its consumer sits in the developer client.
        class PlayerUploadScreenshot final : public ServerPacket
        {
        public:
            explicit PlayerUploadScreenshot() : ServerPacket(SMSG_PLAYER_UPLOAD_SCREENSHOT, 2 + 4) { }

            WorldPacket const* Write() override;

            UploadScreenshotHeader Header;
        };

        // SMSG_PLAYER_DELAYED_UPLOAD_SCREENSHOT - wire 0x640033.
        // Case: Read<uint8> >> 7 first, then the same reader 0x67BF70 on an embedded object.
        // Same inert-in-retail caveat as SMSG_PLAYER_UPLOAD_SCREENSHOT.
        class PlayerDelayedUploadScreenshot final : public ServerPacket
        {
        public:
            explicit PlayerDelayedUploadScreenshot() : ServerPacket(SMSG_PLAYER_DELAYED_UPLOAD_SCREENSHOT, 1 + 2 + 4) { }

            WorldPacket const* Write() override;

            UploadScreenshotHeader Header;
            bool Delayed = false;
        };

        // CMSG_ABANDON_NPE_RESPONSE - answer to SMSG_CHECK_ABANDON_NPE. Client wire 0x3D029C.
        // Writer RVA 0x6D1CB0: Write<uint32>(opcode), then a single inline bit, then FlushBits.
        // Body is exactly 1 byte, the flag in bit 7.
        // Two Lua entry points feed the same opcode and differ only in that bit:
        //   C_Tutorial.AbandonTutorialArea()  (RVA 0x1676430) stores 1 - leave for good
        //   C_Tutorial.ReturnToTutorialArea() (RVA 0x1676D20) stores 0 - go back to the tutorial zone
        // (GameDialogDefs.lua:1202/1205 = the two buttons of StaticPopupDialogs["LEAVING_TUTORIAL_AREA"])
        class AbandonNPEResponse final : public ClientPacket
        {
        public:
            explicit AbandonNPEResponse(WorldPacket&& packet) : ClientPacket(CMSG_ABANDON_NPE_RESPONSE, std::move(packet)) { }

            void Read() override;

            bool Abandon = false;
        };

        // CMSG_SUBSCRIPTION_INTERSTITIAL_RESPONSE - answer to SMSG_PLAYER_OPEN_SUBSCRIPTION_INTERSTITIAL.
        // Client wire 0x3D0293, writer RVA 0x6D1C50: Write<uint32>(opcode), WriteBits(3), FlushBits.
        // Body is exactly 1 byte. Triggered by SendSubscriptionInterstitialResponse(response)
        // (ExpansionDocumentation.lua:220-228) from Blizzard_SubscriptionInterstitialUI.lua:27/29/123.
        // See SubscriptionInterstitialResponseType for the Lua -> wire remap.
        class SubscriptionInterstitialResponse final : public ClientPacket
        {
        public:
            explicit SubscriptionInterstitialResponse(WorldPacket&& packet) : ClientPacket(CMSG_SUBSCRIPTION_INTERSTITIAL_RESPONSE, std::move(packet)) { }

            void Read() override;

            SubscriptionInterstitialResponseType Response = SubscriptionInterstitialResponseType::Closed;
        };

        // CMSG_REQUEST_SCHEDULED_AREA_POI_UPDATE - request for SMSG_SCHEDULED_AREA_POI_UPDATE_RESPONSE.
        // Client wire 0x3D0238, writer RVA 0x6D1230 is `Write<uint32>(opcode); return 1;` - empty body,
        // 4 bytes total. Sent by C_EventScheduler.RequestEvents() (RVA 0xE88F10, no arguments,
        // EventSchedulerUIDocumentation.lua:116-119) from EventScheduler.lua:227/762, client throttled.
        class RequestScheduledAreaPoiUpdate final : public ClientPacket
        {
        public:
            explicit RequestScheduledAreaPoiUpdate(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_SCHEDULED_AREA_POI_UPDATE, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_BONUS_ROLL - client wire 0x3D025D, writer RVA 0x6D16E0 is
        // `Write<uint32>(opcode); return 1;` - empty body, 4 bytes total.
        class BonusRoll final : public ClientPacket
        {
        public:
            explicit BonusRoll(WorldPacket&& packet) : ClientPacket(CMSG_BONUS_ROLL, std::move(packet)) { }

            void Read() override { }
        };
    }
}

#endif // TRINITYCORE_MISC_PACKETS_H

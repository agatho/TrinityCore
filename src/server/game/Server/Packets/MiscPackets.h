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
#include <span>
#include <string>
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

        // ------------------------------------------------------------------------------------
        // Einheit w4_cmsg_43_3D - Sendeseite der Sammelfamilien 0x43 / 0x3D, Phase A.
        // Alle Feldfolgen stammen aus dem Client-Serializer des Builds 12.1.0.69382
        // (ImageBase 0x7FF780FD0000, Writer-RVA je Klasse angegeben). Wo kein Sniff-Paket
        // vorliegt, steht die Laengenrechnung min..max im Kommentar - siehe DoD D1.
        // ------------------------------------------------------------------------------------

        // Writer 0x6CBEA0 - leere Nutzlast (Write<uint32>(0x3D0032); return 1).
        class UsedFollow final : public ClientPacket
        {
        public:
            explicit UsedFollow(WorldPacket&& packet) : ClientPacket(CMSG_USED_FOLLOW, std::move(packet)) { }

            void Read() override { }
        };

        // Writer 0x6D29C0 - leere Nutzlast.
        class SeamlessTransferComplete final : public ClientPacket
        {
        public:
            explicit SeamlessTransferComplete(WorldPacket&& packet) : ClientPacket(CMSG_SEAMLESS_TRANSFER_COMPLETE, std::move(packet)) { }

            void Read() override { }
        };

        // Writer 0x6D1870 - leere Nutzlast.
        class ReportServerLag final : public ClientPacket
        {
        public:
            explicit ReportServerLag(WorldPacket&& packet) : ClientPacket(CMSG_REPORT_SERVER_LAG, std::move(packet)) { }

            void Read() override { }
        };

        // Writer 0x6CD470 - leere Nutzlast.
        class ResetChallengeModeCheat final : public ClientPacket
        {
        public:
            explicit ResetChallengeModeCheat(WorldPacket&& packet) : ClientPacket(CMSG_RESET_CHALLENGE_MODE_CHEAT, std::move(packet)) { }

            void Read() override { }
        };

        // Writer 0x6D2710 - leere Nutzlast. Lua C_SpectatingUI.LeaveSpectateMode().
        class SpectateEnd final : public ClientPacket
        {
        public:
            explicit SpectateEnd(WorldPacket&& packet) : ClientPacket(CMSG_SPECTATE_END, std::move(packet)) { }

            void Read() override { }
        };

        // Writer 0x6D2650 - ein eingebettetes Bit + FlushBits, 1 Byte.
        // Lua C_SpectatingUI.SpectateChange(nextTarget: bool).
        class SpectateChange final : public ClientPacket
        {
        public:
            explicit SpectateChange(WorldPacket&& packet) : ClientPacket(CMSG_SPECTATE_CHANGE, std::move(packet)) { }

            void Read() override;

            bool NextTarget = false;
        };

        // Writer 0x6D26C0 - gepackte ObjectGuid, 2..18 Byte.
        class SpectateSetNextTarget final : public ClientPacket
        {
        public:
            explicit SpectateSetNextTarget(WorldPacket&& packet) : ClientPacket(CMSG_SPECTATE_SET_NEXT_TARGET, std::move(packet)) { }

            void Read() override;

            ObjectGuid Target;
        };

        // Writer 0x6AB1E0 - ein eingebettetes Bit + FlushBits, 1 Byte.
        class LowLevelRaid1 final : public ClientPacket
        {
        public:
            explicit LowLevelRaid1(WorldPacket&& packet) : ClientPacket(CMSG_LOW_LEVEL_RAID1, std::move(packet)) { }

            void Read() override;

            bool Enable = false;
        };

        // Writer 0x6AFAD0 - ein eingebettetes Bit + FlushBits, 1 Byte.
        // Am Draht belegt: 37 Pakete, konstant 1 Byte.
        class QuickJoinAutoAcceptRequests final : public ClientPacket
        {
        public:
            explicit QuickJoinAutoAcceptRequests(WorldPacket&& packet) : ClientPacket(CMSG_QUICK_JOIN_AUTO_ACCEPT_REQUESTS, std::move(packet)) { }

            void Read() override;

            bool AutoAccept = false;
        };

        // Writer 0x6A3600 - ein eingebettetes Bit + FlushBits, 1 Byte.
        // Beide Sendestellen setzen konstant 1; das Objektfeld ist ein uint8, der Draht ein Bit
        // (Rumpfbyte 0x80, nicht 0x01).
        class RequestChatLogin final : public ClientPacket
        {
        public:
            explicit RequestChatLogin(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_CHAT_LOGIN, std::move(packet)) { }

            void Read() override;

            bool Login = false;
        };

        // Writer 0x6CD620 - ein uint32.
        class ClassTalentsNotifyEmptyConfig final : public ClientPacket
        {
        public:
            explicit ClassTalentsNotifyEmptyConfig(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_NOTIFY_EMPTY_CONFIG, std::move(packet)) { }

            void Read() override;

            uint32 ConfigID = 0;
        };

        // Writer 0x6D2550 - ein uint32. Am Draht belegt: 1 Paket, konstant 4 Byte.
        class ClassTalentsNotifyValidationFailed final : public ClientPacket
        {
        public:
            explicit ClassTalentsNotifyValidationFailed(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_NOTIFY_VALIDATION_FAILED, std::move(packet)) { }

            void Read() override;

            uint32 ConfigID = 0;
        };

        // Writer 0x6D21F0 - leere Nutzlast.
        class TraitsTalentTestUnlearnSpells final : public ClientPacket
        {
        public:
            explicit TraitsTalentTestUnlearnSpells(WorldPacket&& packet) : ClientPacket(CMSG_TRAITS_TALENT_TEST_UNLEARN_SPELLS, std::move(packet)) { }

            void Read() override { }
        };

        // Writer 0x6AAA60 - ein uint32.
        class GMTicketAcknowledgeSurvey final : public ClientPacket
        {
        public:
            explicit GMTicketAcknowledgeSurvey(WorldPacket&& packet) : ClientPacket(CMSG_GM_TICKET_ACKNOWLEDGE_SURVEY, std::move(packet)) { }

            void Read() override;

            uint32 CaseIndex = 0;
        };

        // Writer 0x6CEF70 - gepackte ObjectGuid.
        class AddAccountCosmetic final : public ClientPacket
        {
        public:
            explicit AddAccountCosmetic(WorldPacket&& packet) : ClientPacket(CMSG_ADD_ACCOUNT_COSMETIC, std::move(packet)) { }

            void Read() override;

            ObjectGuid ItemGUID;
        };

        // Writer 0x6D0550 - gepackte ObjectGuid (der Handwerks-NPC).
        class UpdateCraftingNpcRecipes final : public ClientPacket
        {
        public:
            explicit UpdateCraftingNpcRecipes(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_CRAFTING_NPC_RECIPES, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
        };

        // Writer 0x6D1710 - guid + uint32. Lua C_IslandsQueue.QueueForIsland(difficultyID);
        // die GUID ist der Warteschlangen-NPC.
        class IslandQueue final : public ClientPacket
        {
        public:
            explicit IslandQueue(WorldPacket&& packet) : ClientPacket(CMSG_ISLAND_QUEUE, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            uint32 DifficultyID = 0;
        };

        // Writer 0x6ABE20 - guid + uint32 + uint32.
        // Ausgeloest durch Klick auf einen "trade:"-Chat-Hyperlink (ItemRef.lua:46).
        class ShowTradeSkill final : public ClientPacket
        {
        public:
            explicit ShowTradeSkill(WorldPacket&& packet) : ClientPacket(CMSG_SHOW_TRADE_SKILL, std::move(packet)) { }

            void Read() override;

            ObjectGuid PlayerGUID;
            uint32 SpellID = 0;
            uint32 SkillLineID = 0;
        };

        // ------------------------------------------------------------------------------------
        // KORREKTUR gegen Subplan UND Brief - der folgenreichste Befund dieser Einheit.
        //
        // Subplan und Brief fuehren CMSG_QUEST_DRIVEN_SCENARIO_STATE_CHANGE mit Writer 0x6D3160
        // und der Nutzlast "guid u32 u32", und der Brief erklaert den Wertunterschied zum Baum
        // (0x3D02EB im Analysebuild gegen 0x3D02F6 im Baum) mit einer Umnummerierung durch fuenf
        // in 69404 eingefuegte Discord-Opcodes. BEIDES IST FALSCH, und zwar nachpruefbar:
        //
        //  * Es gab keine Umnummerierung. Die Discord-Opcodes stehen bereits im 69382-Abbild
        //    (0x3D02EC..0x3D02F0, u.a. C_Discord.GuildLink / GuildUnlink / SetGuildSetting mit
        //    ihren Usage-Strings). Ein Durchlauf ueber ALLE 2698 Opcode-Getter-Thunks des Abbilds
        //    reproduziert die Nummerierung des Baums fuer den ganzen Block 0x3D02Dx..0x3D030x.
        //  * Writer 0x6D3160 schreibt das Immediate 3998443 = 0x3D02EB, und 0x3D02EB ist
        //    CMSG_TRANSFER_CURRENCY_FROM_ACCOUNT_CHARACTER - belegt an der Lua-Glue 0xDC0210 mit
        //    dem Usage-String "C_CurrencyInfo.RequestCurrencyFromAccountCharacter(
        //    sourceCharacterGUID, currencyID, quantity)" und am Registerfluss der drei Argumente
        //    auf msg+0x20 / +0x30 / +0x34. Dieser Opcode gehoert NICHT zu dieser Einheit.
        //  * Der echte Writer ist 0x6D3660, Immediate 3998454 = 0x3D02F6, Vtable 0x3C01558,
        //    Opcode-Getter 0x6D37B0 (je genau ein Treffer im ganzen Abbild). Sein Destruktor
        //    0x6DD2C0 gibt zwei Arrays frei, deren Allokator-Literale
        //    WowGetRawTypeName<struct JamScenarioStageInfo> und <struct JamScenarioCurrencyInfo>
        //    tragen - das ist die harte Typbindung, keine Namensaehnlichkeit. Die Sendestelle
        //    0x20F55A0 schlaegt zusaetzlich in QuestDrivenScenario.db2 und ScenarioStep.db2 nach.
        //
        // Der Schaden ohne diese Korrektur waere still gewesen: eine Feldliste aus drei Feldern
        // fuer eine Nachricht mit mindestens 38 Byte fester Breite - der Parser haette bei jedem
        // Paket ueberlaufen.
        //
        // Draht (Writer 0x6D3660): keine Bits, kein Flush, keine Zeichenketten, keine gepackte
        // GUID. Feste Breite 38 Byte, dazu 24 Byte je Stufe und 8 Byte je Waehrungspaar.
        // ------------------------------------------------------------------------------------

        // Element, Client-Typ JamScenarioStageInfo, 24 Byte.
        struct ScenarioStageInfo
        {
            int64 StartTime = 0;                // Unix-Sekunden (GetSystemTimeAsFileTime + Bias)
            int64 EndTime = 0;                  // Unix-Sekunden, -1 als Ueberlaufmarke moeglich
            uint32 Field16 = 0;                 // UNVERIFIED: auf dem beobachteten Pfad immer 0
            uint32 StepOrderIndex = 0;          // ScenarioStep.OrderIndex (DB2-Feld 8, 8 Bit)
        };

        // Element, Client-Typ JamScenarioCurrencyInfo, 8 Byte - ein {Schluessel, Wert}-Paar aus
        // einer chained_hash_node<pair<int,int>>.
        struct ScenarioCurrencyInfo
        {
            uint32 CurrencyID = 0;
            uint32 Quantity = 0;
        };

        // Writer 0x6D3660 (NICHT 0x6D3160 - siehe Kommentarblock oben).
        class QuestDrivenScenarioStateChange final : public ClientPacket
        {
        public:
            explicit QuestDrivenScenarioStateChange(WorldPacket&& packet) : ClientPacket(CMSG_QUEST_DRIVEN_SCENARIO_STATE_CHANGE, std::move(packet)) { }

            void Read() override;

            uint8 StateChangeType = 0;          // an den Aufrufstellen 0..4 beobachtet
            uint32 ScenarioID = 0;              // Globale 0x680A730, ueber SMSG_SCENARIO_STATE
                                                // (0x4500D4, ScenarioID@+0x30) gegengeprueft
            uint32 QuestDrivenScenarioID = 0;   // QuestDrivenScenario.db2, Satz-ID
            int64 Field48 = 0;                  // UNVERIFIED: Szenario-Singleton +0x18,
                                                // vermutlich Startzeit
            int64 ClientUnixTime = 0;           // Unix-Sekunden
            uint32 Field64 = 0;                 // UNVERIFIED: Singleton +0x0C
            uint8 Field68 = 0;                  // UNVERIFIED: Singleton +0x10, boolesch
            Array<ScenarioStageInfo, 64> Stages;
            Array<ScenarioCurrencyInfo, 64> Currencies;
        };

        // Writer 0x6D2740 - uint32 VOR der GUID.
        // Lua C_WorldLootObject.OnWorldLootObjectClick(unitToken, isLeftClick).
        class WorldLootObjectClick final : public ClientPacket
        {
        public:
            explicit WorldLootObjectClick(WorldPacket&& packet) : ClientPacket(CMSG_WORLD_LOOT_OBJECT_CLICK, std::move(packet)) { }

            void Read() override;

            uint32 ClickType = 0;
            ObjectGuid ObjectGUID;
        };

        // Writer 0x6D1B90 - alle fuenf Felder stammen aus EINER angehaengten Struktur:
        // uint32 @+0, dann vier uint8 @+4..+7. Lua
        // C_LegendaryCrafting.UpgradeRuneforgeLegendary(runeforgeLegendary: ItemLocation,
        // upgradeItem: ItemLocation) - die vier uint8 sind zwei ItemLocations (Tasche, Platz).
        class UpgradeRuneforgeLegendary final : public ClientPacket
        {
        public:
            explicit UpgradeRuneforgeLegendary(WorldPacket&& packet) : ClientPacket(CMSG_UPGRADE_RUNEFORGE_LEGENDARY, std::move(packet)) { }

            void Read() override;

            uint32 Field0 = 0;
            uint8 LegendaryBagSlot = 0;
            uint8 LegendarySlot = 0;
            uint8 UpgradeItemBagSlot = 0;
            uint8 UpgradeItemSlot = 0;
        };

        // Writer 0x6AFB60 - ein uint8. Am Draht belegt: 22 Pakete, konstant 1 Byte.
        // Enum.ExcludedCensorSources als Bitmaske: 1 Friends, 2 Guild, 4..128 Reserve1..6.
        class SetExcludedChatCensorSources final : public ClientPacket
        {
        public:
            explicit SetExcludedChatCensorSources(WorldPacket&& packet) : ClientPacket(CMSG_SET_EXCLUDED_CHAT_CENSOR_SOURCES, std::move(packet)) { }

            void Read() override;

            uint8 Sources = 0;
        };

        // Writer 0x6A83E0 - Hausmuster der 0x43-Gruppenopcodes:
        //   bit has(PartyIndex); bit Silence; FlushBits; guid Target; if (has) uint8 PartyIndex
        // Die Nachricht hat dadurch ZWEI Groessen: 3..20 Byte.
        class SilenceTalkerInParty final : public ClientPacket
        {
        public:
            explicit SilenceTalkerInParty(WorldPacket&& packet) : ClientPacket(CMSG_SILENCE_PARTY_TALKER, std::move(packet)) { }

            void Read() override;

            Optional<uint8> PartyIndex;
            bool Silence = false;
            ObjectGuid Target;
        };

        // Writer 0x6A2940 - uint32 Kind; uint32 Size; byte Data[Size].
        // Am Draht belegt: 1398 Pakete, 40..16280 Byte -> 8 + Size geht auf.
        class Warden3Data final : public ClientPacket
        {
        public:
            explicit Warden3Data(WorldPacket&& packet) : ClientPacket(CMSG_WARDEN3_DATA, std::move(packet)) { }

            void Read() override;

            uint32 Kind = 0;
            std::span<uint8> Data;
        };

        // Element von CMSG_ADDON_LIST, Serializer 0x69FDE0, Client-Typ JamCliAddOnInfo (88 Byte).
        // Beide Laengen sind 10 Bit und schliessen die NUL EIN.
        //
        // Der Elementkopf ist GENAU 3 Byte. Die Schreibfolge des Subplans
        // ('bitflush bits2 bitflush u8 bits2 u8*2 FLUSH bytes*2') zaehlt drei Write<uint8>
        // - am Serializer sind es zwei ausgefuehrte. Aufgeloest an 0x69FDE0:
        //   1. Write<uint8>((Name.len+1) >> 2) - obere 8 der 10 Bit, direkt in den Strom,
        //      weil der Bitakkumulator am Elementanfang leer ist.
        //   2. WriteBits((Name.len+1) & 3, 2) - 0x5D4A20, Akkumulator = 2 Bit.
        //   3. (Version.len+1) >> 2: EIN logischer Write<uint8>, im Dekompilat als
        //      'if (pending) ... else ...' mit ZWEI Write<uint8>-Aufrufen sichtbar. Genau
        //      einer laeuft - der if-Zweig schiebt das Byte durch den Akkumulator
        //      ((v8 >> pending) | (accum << (8-pending))), der else-Zweig schreibt direkt.
        //   4. WriteBits((Version.len+1) & 3, 2), dann Flag1 und Flag2 als je 1 Bit.
        //      Die Write<uint8> in den Zweigen 'v13 == 8' bzw. 'v13 == 7' sind der inline
        //      ausgeschriebene Uebertrag von 0x5D4A20 (dort steht derselbe Aufruf). Der
        //      Akkumulator steht hier auf 4 bzw. 5 Bit, keiner der beiden Zweige laeuft.
        //   5. FlushBits 0x5D4EA0, Fall 6 -> ein Byte, LSB-seitig mit 2 Null-Bit gepolstert.
        //      Das ist MSB-first und deckt sich mit ByteBuffer::ReadBits.
        // Summe 10 + 10 + 1 + 1 = 22 Bit -> 3 Byte. Danach Name und Version als je
        // (len+1) Rohbytes ueber 0x35B01C0, und nur wenn das Laengenfeld > 0 ist.
        // Das Laengenfeld ist 10 Bit breit, also hoechstens 1023 Byte je Zeichenkette.
        struct AddonInfo
        {
            std::string Name;
            std::string Version;
            bool Flag1 = false;
            bool Flag2 = false;
        };

        // Writer 0x6A1C70 (Opcode-Immediate 4390916 = 0x430004; Rumpf ohne Immediate 0x6A1BB0).
        // Die drei Kopffelder sind Echos aus SMSG_ADDON_LIST_REQUEST.
        // Laengenrechnung, ohne Sniff gefuehrt: Kopf = ObjectGuid (2..18, Primitive 0x36012E0,
        // dieselbe wie in StartSpectatorWarGame 25..57) + uint32 + uint32 + uint8 + uint32 Count
        // = 13 feste Byte -> 15..31 bei Count 0. Je Element 3 Byte Kopf, dazu 0 oder 2..1023 Byte
        // je Zeichenkette -> 3..2049. Mit der serverseitigen Schranke von 512 Elementen:
        // min..max = 15..1049119.
        class AddonList final : public ClientPacket
        {
        public:
            explicit AddonList(WorldPacket&& packet) : ClientPacket(CMSG_ADDON_LIST, std::move(packet)) { }

            void Read() override;

            ObjectGuid RequestGUID;
            uint32 Field30 = 0;
            uint32 Field38 = 0;
            uint8 Field34 = 0;
            Array<AddonInfo, 512> AddOns;   // der Client deckelt nicht - Schranke serverseitig
        };

        // Writer 0x6AE330 -> Rumpf 0x6AD300, Fuellfunktion 0x20B530.
        // 294 Byte im Sniff (12.1.0.69273_preyandwqpart1.pkt), Feldliste gegen dieses Paket
        // byteweise nachgerechnet: 141 Byte feste Breite + 104 Bit Bit-Sektion (13 Byte)
        // + 140 Byte Rohbytes = 294. Die zehn Zeichenketten stehen OHNE NUL am Draht.
        class EngineSurvey final : public ClientPacket
        {
        public:
            explicit EngineSurvey(WorldPacket&& packet) : ClientPacket(CMSG_ENGINE_SURVEY, std::move(packet)) { }

            void Read() override;

            uint32 SurveyVersion = 0;               // CVar "engineSurvey", Registrar 0x126B60
            uint32 SurveyPatch = 0;                 // CVar "engineSurveyPatch", Registrar 0x126C20
            uint32 CpuVendorID = 0;                 // CPU-Info-Singleton 0x2D3B70
            uint32 CpuPackages = 0;                 // belegtes Paket: 1
            uint32 CpuCores = 0;                    // belegtes Paket: 6   (Ryzen 5 3600)
            uint32 CpuThreads = 0;                  // belegtes Paket: 12  (Ryzen 5 3600)
            uint8 Const2 = 0;                       // UNVERIFIED: im belegten Paket konstant 2
            uint8 CpuField1C = 0;
            uint32 Reserved0 = 0;                   // UNVERIFIED: im belegten Paket konstant 0
            uint8 OsField0 = 0;                     // OS-Info-Singleton 0x2CC8C0
            uint32 OsMajorVersion = 0;              // belegtes Paket: 10
            uint32 OsMinorVersion = 0;              // belegtes Paket: 0
            uint32 OsField10 = 0;                   // UNVERIFIED: belegtes Paket 7663, passt in
                                                    // der Reihenfolge nicht zu RtlGetVersion
            uint32 OsBuildNumber = 0;               // belegtes Paket: 19045 (Windows 10 22H2)
            uint64 PhysicalMemory = 0;              // belegtes Paket: 0x7FBB29000 = 31,9 GiB
            uint32 Field240 = 0;
            uint8 MonitorCountMinusOne = 0;
            uint32 DesktopWidth = 0;
            uint32 DesktopHeight = 0;
            uint32 MonitorWidth = 0;
            uint32 MonitorHeight = 0;
            uint32 GpuVendorID = 0;                 // PCI-Vendor  (belegt: 0x10DE NVIDIA)
            uint32 GpuDeviceID = 0;                 // PCI-Device  (belegt: 0x1D01 GT 1030)
            uint8 GxField0 = 0;
            uint8 GxField1 = 0;
            uint8 GxField2 = 0;
            uint8 GxField3 = 0;
            uint64 DedicatedVideoMemory = 0;        // belegtes Paket: 1967 (MB, GT 1030)
            uint64 SharedSystemMemory = 0;          // belegtes Paket: 16349 (MB)
            uint32 GxApi = 0;
            uint8 OsField4 = 0;                     // UNVERIFIED: Herkunft nicht aufgeloest
            uint32 OsField18 = 0;
            uint32 OsField1C = 0;
            uint32 OsField20 = 0;
            uint32 OsField24 = 0;
            uint64 CpuFeatureMask = 0;
            uint32 CpuExtra = 0;
            uint16 CpuField14 = 0;
            uint8 CpuField16 = 0;
            uint8 CpuField17 = 0;

            std::array<bool, 44> Flags = { };

            std::string CpuVendor;                  // Puffer 64  (belegt: "AuthenticAMD")
            std::string CpuBrand;                   // Puffer 64  (belegt: CPUID-Brandstring)
            std::string GpuName;                    // Puffer 64  (belegt: Adaptername)
            std::string OsName;                     // Puffer 128
            std::string OsExtra;                    // Puffer 64
            std::string BaseBoardManufacturer;      // Puffer 128, HKLM\...\BIOS\BaseBoardManufacturer
            std::string BaseBoardProduct;           // Puffer 128, ...\BaseBoardProduct
            std::string BiosVendor;                 // Puffer 128, ...\BIOSVendor
            std::string BiosReleaseDate;            // Puffer 16,  ...\BIOSReleaseDate
            std::string BiosVersion;                // Puffer 16,  ...\BIOSVersion
        };

        // Ein Eintrag von CMSG_START_SPECTATOR_WAR_GAME. Der Writer 0x6A2150 schreibt ZWEI
        // ausgeschriebene Bloecke, keine Schleife - es gibt keinen Count am Draht.
        struct SpectatorWarGamePlayer
        {
            ObjectGuid PlayerGUID;
            uint32 VirtualRealmAddress = 0;     // UNVERIFIED: Name geraten, Breite/Position belegt
            uint16 RealmIndex = 0;              // UNVERIFIED: Name geraten, Breite/Position belegt
        };

        // Writer 0x6A2150 - 25..57 Byte.
        // Das abschliessende "FLUSH" des Subplans ist ein echtes 1-Bit-Feld (Objekt +0x58).
        class StartSpectatorWarGame final : public ClientPacket
        {
        public:
            explicit StartSpectatorWarGame(WorldPacket&& packet) : ClientPacket(CMSG_START_SPECTATOR_WAR_GAME, std::move(packet)) { }

            void Read() override;

            std::array<SpectatorWarGamePlayer, 2> Players = { };
            uint64 QueueID = 0;
            bool TournamentRules = false;
        };

        // Writer 0x6AF830 - 5..37 Byte. Auch hier ist das "FLUSH" ein echtes Bit (Objekt +0x40).
        // Lua C.RespondToInviteConfirmation(guid, accept), Sender 0x21981D0.
        class QuickJoinRespondToInvite final : public ClientPacket
        {
        public:
            explicit QuickJoinRespondToInvite(WorldPacket&& packet) : ClientPacket(CMSG_QUICK_JOIN_RESPOND_TO_INVITE, std::move(packet)) { }

            void Read() override;

            ObjectGuid QueueGUID;
            ObjectGuid ApplicantGUID;
            bool Accept = false;
        };

        // Writer 0x6AF6B0 - 11 .. 27+18*n Byte.
        // Lua C_SocialQueue.SignalToastDisplayed(groupGUID, priority) - daher der float.
        class QuickJoinSignalToastDisplayed final : public ClientPacket
        {
        public:
            explicit QuickJoinSignalToastDisplayed(WorldPacket&& packet) : ClientPacket(CMSG_QUICK_JOIN_SIGNAL_TOAST_DISPLAYED, std::move(packet)) { }

            void Read() override;

            ObjectGuid GroupGUID;
            float Priority = 0.0f;
            Array<ObjectGuid, 100> Members;     // der Client deckelt Count NICHT - Schranke ist serverseitig
            bool Flag0 = false;                 // UNVERIFIED: Bedeutung offen (Objekt +0x50)
            bool Flag1 = false;                 // UNVERIFIED: Bedeutung offen (Objekt +0x51)
        };

        // Writer 0x6AFA80 -> Rumpf 0x6AF8C0 - 18..597 Byte.
        // Bit-Sektion: bits<9> lenName; bits<9> lenRealm; 1 Bit -> genau 3 Byte.
        // Die "bitflush" des Subplans sind die oberen acht Bit je 9-Bit-Laenge, kein Padding.
        // Lua C_PartyInfo.RequestInviteFromUnit(targetName, tank, healer, dps);
        // die drei Rollen liegen als Maske im uint8 (2 Tank, 4 Healer, 8 Damage).
        class QuickJoinRequestInvite final : public ClientPacket
        {
        public:
            explicit QuickJoinRequestInvite(WorldPacket&& packet) : ClientPacket(CMSG_QUICK_JOIN_REQUEST_INVITE, std::move(packet)) { }

            void Read() override;

            uint32 QueueID = 0;
            ObjectGuid GroupGUID;
            uint64 ClubID = 0;                  // UNVERIFIED: Bedeutung offen (Objekt +0x268)
            uint8 Roles = 0;
            bool Flag = false;                  // UNVERIFIED: Bedeutung offen (Objekt +0x271)
            std::string TargetName;             // Puffer 306
            std::string TargetRealm;            // Puffer 257
        };

        // Writer 0x6B15B0 -> Rumpf 0x6B1420 - 13..592 Byte.
        // NICHT baugleich zum Geschwister 0x430131: die Bit-Sektion hat nur 18 Bit (kein drittes
        // Bit), Objekt +0x268 ist ein uint32 statt uint64, und das Rollen-Byte fehlt ganz.
        // Beide Bit-Sektionen sind trotzdem 3 Byte lang - wer sie fuer baugleich haelt, verliert
        // danach genau 5 Byte und verschiebt beide Zeichenketten.
        class QuickJoinRequestInviteWithConfirmation final : public ClientPacket
        {
        public:
            explicit QuickJoinRequestInviteWithConfirmation(WorldPacket&& packet) : ClientPacket(CMSG_QUICK_JOIN_REQUEST_INVITE_WITH_CONFIRMATION, std::move(packet)) { }

            void Read() override;

            uint32 QueueID = 0;
            ObjectGuid GroupGUID;
            uint32 RequestID = 0;               // monoton steigender Clientzaehler, 0x2 12E92F0:103
            std::string TargetName;             // Puffer 306
            std::string TargetRealm;            // Puffer 257
        };

        // 0x4502CC. Die definierte Abschaltmeldung des Warden3-Kanals.
        // Draht am Reader 0x606F30 nachgelesen: er nimmt den REST des Pakets als
        // undurchsichtigen Block (sub 0x35AF730 mit *(buf+20) - *(buf+24) Byte). Eine LEERE
        // Nutzlast ist damit gueltig - der Reader akzeptiert jede Laenge einschliesslich 0.
        // Die Zuordnung ist doppelt belegt: Opcode-Getter 0x606FA0 schreibt 4522700 = 0x4502CC,
        // und die Registrierung 0x226436 haengt den Konsumenten 0x1CE2CD0 an dieses Global.
        // UNVERIFIED: der Konsument liegt nicht im Dekompilat-Cache. Dass der Client nach dieser
        // Nachricht aufhoert, CMSG_WARDEN3_DATA zu senden, ist NICHT am Konsumenten belegt,
        // sondern aus dem Namen und der Gegennachricht SMSG_WARDEN3_ENABLED (0x4502CB, im Sniff
        // 15 Pakete a 4 Byte) abgeleitet.
        class Warden3Disabled final : public ServerPacket
        {
        public:
            explicit Warden3Disabled() : ServerPacket(SMSG_WARDEN3_DISABLED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // Writer 0x6B2860 - 9..~16 MB theoretisch, an der Erzeugerstelle 58..74 Byte.
        // bits24 traegt die Laenge INKLUSIVE NUL; der FlushBits danach schreibt NULL Byte,
        // weil 24 mod 8 == 0 - wer ein Fuellbyte einplant, verschiebt alles Nachfolgende.
        // Der String ist Base64 einer 36-Zeichen-UUID, an der Erzeugerstelle also 48 Zeichen.
        class ServerValidationSignatureRequest final : public ClientPacket
        {
        public:
            explicit ServerValidationSignatureRequest(WorldPacket&& packet) : ClientPacket(CMSG_SERVER_VALIDATION_SIGNATURE_REQUEST, std::move(packet)) { }

            void Read() override;

            std::string Signature;
            uint32 RequestID = 0;               // UNVERIFIED: Anfrage-/Kontextkennung, Semantik offen
            ObjectGuid Guid;
        };
    }
}

#endif // TRINITYCORE_MISC_PACKETS_H

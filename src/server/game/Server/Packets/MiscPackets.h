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
#include <vector>
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

        // CMSG_GET_CHARACTER_CURRENCY_TRANSFER_LOG (0x29001F): the client opens the account/warband currency
        // "transfer history" panel. SNIFF-CONFIRMED empty body (size 4 = bare opcode). Answered with
        // SMSG_CURRENCY_TRANSFER_LOG.
        class GetCharacterCurrencyTransferLog final : public ClientPacket
        {
        public:
            explicit GetCharacterCurrencyTransferLog(WorldPacket&& packet) : ClientPacket(CMSG_GET_CHARACTER_CURRENCY_TRANSFER_LOG, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_CURRENCY_TRANSFER_LOG (0x420355): the history of account/warband currency transfers involving this
        // character. Wire recovered byte-exact from a live 12.0.7 sniff (C:\sniff\b_pets12.0.7.pkt, a 3-entry
        // capture parses to the byte, 0 leftover): { u32 count; count x entry }, entry =
        // { PackedGuid Source; PackedGuid Dest; u32 CurrencyID; u32 Quantity; u32 Field3; u64 TransferTime }.
        // (CurrencyID was a constant 1792 across the capture; Quantity/Field3 are the two per-transfer amounts Ã¢â‚¬â€�
        // exact roles not offline-confirmable but they are only written when real entries exist.) TrinityCore does
        // not implement account currency transfer, so there is no transfer history to report and the reply is a bare
        // header (count 0) Ã¢â‚¬â€� the truthful "no transfers" answer, which clears the client's transfer-history panel.
        class CurrencyTransferLog final : public ServerPacket
        {
        public:
            struct CurrencyTransferLogEntry
            {
                ObjectGuid SourceCharacterGUID;
                ObjectGuid DestCharacterGUID;
                int32 CurrencyTypeID = 0;
                int32 QuantityReceived = 0;
                int32 QuantitySent = 0;
                uint32 Timestamp = 0;
            };

            explicit CurrencyTransferLog() : ServerPacket(SMSG_CURRENCY_TRANSFER_LOG) { }

            WorldPacket const* Write() override;

            std::vector<CurrencyTransferLogEntry> Entries;
        };

        // CMSG_REQUEST_CURRENCY_DATA_FOR_ACCOUNT_CHARACTERS (0x29001F... 0x29001E): empty body; the client asks for
        // every account character's currency totals (the warband currency view). Answered with
        // SMSG_ACCOUNT_CHARACTER_CURRENCY_LISTS.
        class RequestCurrencyDataForAccountCharacters final : public ClientPacket
        {
        public:
            explicit RequestCurrencyDataForAccountCharacters(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_CURRENCY_DATA_FOR_ACCOUNT_CHARACTERS, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_ACCOUNT_CHARACTER_CURRENCY_LISTS (0x420353): the per-account-character currency totals. Wire from the
        // client reader (sub_7FF7290BB150): { uint32 count; count x { uint32 CurrencyID; PackedGuid Character;
        // uint32 Quantity; uint32 WeeklyQuantity; uint32 MaxQuantity; bit }; bit trailing }. TrinityCore does not
        // aggregate other characters' currencies for this view, so the reply is empty (count 0) -- the truthful "no
        // account-character currency data", which clears the client's warband-currency panel.
        class AccountCharacterCurrencyLists final : public ServerPacket
        {
        public:
            struct CharacterCurrencyData
            {
                ObjectGuid CharacterGUID;
                std::string CharacterName;
                uint8 ClassID = 0;
                int32 Level = 0;
            };

            struct CurrencyQuantityData
            {
                ObjectGuid CharacterGUID;
                int32 CurrencyTypeID = 0;
                int32 Quantity = 0;
            };

            explicit AccountCharacterCurrencyLists() : ServerPacket(SMSG_ACCOUNT_CHARACTER_CURRENCY_LISTS) { }

            WorldPacket const* Write() override;

            std::vector<CharacterCurrencyData> Characters;
            std::vector<CurrencyQuantityData> CurrencyData;
        };

        // SMSG_REATTACH_RESURRECT (0x4201F3): login-sequence resurrect-state reattach (sniff 68275:
        // sent between SETUP_CURRENCY and ALL_ACHIEVEMENT_DATA; body is two zero bytes when no
        // resurrect offer is pending, the only state captured).
        class ReattachResurrect final : public ServerPacket
        {
        public:
            explicit ReattachResurrect() : ServerPacket(SMSG_REATTACH_RESURRECT, 2) { }

            WorldPacket const* Write() override;

            uint8 Unknown1 = 0;
            uint8 Unknown2 = 0;
        };

        // SMSG_CLEAR_RESURRECT (0x420013): empty body; sniff 68275 sends it right after the
        // MOVE_UPDATE_TELEPORT on instance entry - any pending resurrect offer is void on map change.
        class ClearResurrect final : public ServerPacket
        {
        public:
            explicit ClearResurrect() : ServerPacket(SMSG_CLEAR_RESURRECT, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
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

        // Client -> server. "Sync N was the first request after a world change but carried a
        // SequenceIndex != 0, which I do not accept - reset the counter." Emitted by the world-entry
        // guard 0x1E2B340, which replaces the regular SMSG_TIME_SYNC_REQUEST handler for exactly one
        // request after entering a world. Payload is the rejected index and nothing else
        // (writer 0x69AC90, client 12.1.0.69382).
        class TimeSyncResponseFailed final : public ClientPacket
        {
        public:
            explicit TimeSyncResponseFailed(WorldPacket&& packet) : ClientPacket(CMSG_TIME_SYNC_RESPONSE_FAILED, std::move(packet)) { }

            void Read() override;

            uint32 SequenceIndex = 0;
        };

        // Client -> server. "Syncs SequenceIndexA..SequenceIndexB expired, do not account for them."
        // Both fields are read from +0x14 of two records of the client's open-sync list, which is the
        // SequenceIndex slot - the second field is NOT a client time, even though the writer body
        // (0x69ACE0) looks identical to the one of CMSG_TIME_ADJUSTMENT_RESPONSE. Producer 0x1896AD0.
        class TimeSyncResponseDropped final : public ClientPacket
        {
        public:
            explicit TimeSyncResponseDropped(WorldPacket&& packet) : ClientPacket(CMSG_TIME_SYNC_RESPONSE_DROPPED, std::move(packet)) { }

            void Read() override;

            uint32 SequenceIndexA = 0; ///< oldest still open sync record
            uint32 SequenceIndexB = 0; ///< most recent one
        };

        // Client -> server. "Everything up to and including MaxSequenceIndex is settled." Always
        // follows a CMSG_TIME_SYNC_RESPONSE_DROPPED (0x1896BA9), and in the recordings it always sits
        // immediately before the client restarts its counter at 0. Writer 0x69ADA0.
        class DiscardedTimeSyncAcks final : public ClientPacket
        {
        public:
            explicit DiscardedTimeSyncAcks(WorldPacket&& packet) : ClientPacket(CMSG_DISCARDED_TIME_SYNC_ACKS, std::move(packet)) { }

            void Read() override;

            uint32 MaxSequenceIndex = 0;
        };

        // Server -> client. Scales the rate at which the client's clock advances. The consumer
        // (0x1E2B3B0) reads field 0 as an integer and field 1 with movss - it is a float, not a
        // second uint32 - and logs "Time elapse scaled by %g to %g" (format string 0x3D015E0).
        class TimeAdjustment final : public ServerPacket
        {
        public:
            explicit TimeAdjustment() : ServerPacket(SMSG_TIME_ADJUSTMENT, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 SequenceIndex = 0;
            float TimeScale = 1.0f;
        };

        // Client -> server, the answer to SMSG_TIME_ADJUSTMENT. Layout-identical to
        // CMSG_TIME_SYNC_RESPONSE (writer 0x69AD40 == 0x69AC30 apart from the opcode): the time scale
        // is not echoed back. The client queues adjustments in the same list as ordinary sync
        // requests (kind 1 instead of 0), so the answer is a clock sample like any other.
        class TimeAdjustmentResponse final : public ClientPacket
        {
        public:
            explicit TimeAdjustmentResponse(WorldPacket&& packet) : ClientPacket(CMSG_TIME_ADJUSTMENT_RESPONSE, std::move(packet)) { }

            void Read() override;

            TimePoint GetReceivedTime() const { return _worldPacket.GetReceivedTime(); }

            uint32 SequenceIndex = 0;
            uint32 ClientTime = 0; // Client ticks in ms
        };

        // Sent when the client throws away time sync work it had queued, typically around a map
        // transfer. Everything up to and including MaxSequenceIndex will never be answered.

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

        // Values recovered from the 12.0.7 client's own game-error table (the handler indexes it
        // with these ids and each entry names one ERR_DIFFICULTY_* string), so the names below are
        // the client's, not invented. Which trailing fields are present depends on the value -
        // see ChangePlayerDifficultyResult::Write.
        enum class ChangePlayerDifficultyResultCode : uint8
        {
            Cooldown                        = 0,    // ERR_DIFFICULTY_CHANGE_COOLDOWN_S, or
                                                    // ERR_DIFFICULTY_CHANGE_COMBAT_COOLDOWN_S when InCombat is set
            WorldState                      = 1,    // ERR_DIFFICULTY_CHANGE_WORLDSTATE
            Encounter                       = 2,    // ERR_DIFFICULTY_CHANGE_ENCOUNTER
            Combat                          = 3,    // ERR_DIFFICULTY_CHANGE_COMBAT
            PlayerBusy                      = 4,    // ERR_DIFFICULTY_CHANGE_PLAYER_BUSY
            PlayerOnVehicle                 = 5,    // ERR_DIFFICULTY_CHANGE_PLAYER_ON_VEHICLE
            Pending                         = 6,    // no error text; client arms a deadline at now + Cooldown
            AlreadyStarted                  = 7,    // ERR_DIFFICULTY_CHANGE_ALREADY_STARTED
            MapDifficultyMessage            = 8,    // client displays MapDifficulty.db2 Message_lang of MapDifficultyID
            OtherHeroic                     = 9,    // ERR_DIFFICULTY_CHANGE_OTHER_HEROIC_S, %s = name of PlayerGUID
            HeroicInstanceAlreadyRunning    = 10,   // ERR_DIFFICULTY_CHANGE_HEROIC_INSTANCE_ALREADY_RUNNING
            DisabledInLFG                   = 11,   // ERR_DIFFICULTY_DISABLED_IN_LFG
            Success                         = 12    // client stores DifficultyID if MapID is the map it is on
        };

        // Layout taken from the client's deserializer, which switches on Result to decide what else
        // to read; both captured 12.0.7 bodies re-encode byte for byte through it (Result 12 with
        // MapID 2526 + DifficultyID 8, and Result 6 with a negative Cooldown).
        class ChangePlayerDifficultyResult final : public ServerPacket
        {
        public:
            explicit ChangePlayerDifficultyResult(ChangePlayerDifficultyResultCode result)
                : ServerPacket(SMSG_CHANGE_PLAYER_DIFFICULTY_RESULT, 1 + 8), Result(result) { }

            WorldPacket const* Write() override;

            ChangePlayerDifficultyResultCode Result;
            bool InCombat = false;                  // only read for Cooldown and Pending
            int64 Cooldown = 0;                     // seconds; only for Cooldown and Pending
            int32 MapID = 0;                        // only for Success
            uint16 DifficultyID = 0;                // only for Success
            int32 MapDifficultyID = 0;              // only for MapDifficultyMessage
            ObjectGuid PlayerGUID;                  // only for OtherHeroic
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

        // Empty client request sent when the client believes the player is wrongly stuck in combat.
        class ReportStuckInCombat final : public ClientPacket
        {
        public:
            explicit ReportStuckInCombat(WorldPacket&& packet) : ClientPacket(CMSG_REPORT_STUCK_IN_COMBAT, std::move(packet)) { }

            void Read() override { }
        };

        // Player chooses which graveyard (WorldSafeLocs id) they prefer to resurrect at in the current zone.
        class SetPreferredCemetery final : public ClientPacket
        {
        public:
            explicit SetPreferredCemetery(WorldPacket&& packet) : ClientPacket(CMSG_SET_PREFERRED_CEMETERY, std::move(packet)) { }

            void Read() override;

            uint32 CemeteryID = 0;
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

        class GetAccountNotifications final : public ClientPacket
        {
        public:
            explicit GetAccountNotifications(WorldPacket&& packet) : ClientPacket(CMSG_GET_ACCOUNT_NOTIFICATIONS, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_ACCOUNT_NOTIFICATIONS_RESPONSE (0x420310): wire is a single uint32 count followed by that
        // many notification entries. TrinityCore has no account-notification system, so the count is
        // always 0 (an honest empty list), which is exactly what live 12.0.7 captures show (4-byte payload).
        class AccountNotificationsResponse final : public ServerPacket
        {
        public:
            explicit AccountNotificationsResponse() : ServerPacket(SMSG_ACCOUNT_NOTIFICATIONS_RESPONSE, 4) { }

            WorldPacket const* Write() override;
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

        // The result codes of SMSG_LEVEL_LINKING_RESULT. There is no Lua enum for this - the client
        // has no LEVEL_LINK event at all (LevelLinkDocumentation.lua:42-44 has an empty Events block).
        // The values come from the consumer's compare chain (0x1DE03D0):
        //     0f b6 51 20   movzx edx, byte ptr [rcx+0x20]   ; Result
        //     85 d2 / 74 0f je   .linked                     ; Result == 0
        //     83 fa 01 / 75 17   jne  .ret                   ; Result >= 2 -> nothing at all
        //     b9 68 04 00 00     mov  ecx, 0x468              ; 1128, ERR_LEVEL_LINKING_RESULT_UNLINKED
        //   .linked:
        //     8b 51 38           mov  edx, [rcx+0x38]         ; the uint32 as format argument
        //     b9 67 04 00 00     mov  ecx, 0x467              ; 1127, ERR_LEVEL_LINKING_RESULT_LINKED
        // Both are shown in the UIErrorsFrame. NoResult is also the value the reader pre-seeds the
        // field with before reading, i.e. the client's neutral "say nothing" value.
        enum class LevelLinkingResultType : uint8
        {
            Linked      = 0,    ///< ERR_LEVEL_LINKING_RESULT_LINKED, "Your level is now restricted to %d."
            Unlinked    = 1,    ///< ERR_LEVEL_LINKING_RESULT_UNLINKED, "Your level is no longer restricted."
            NoResult    = 2     ///< client shows nothing
        };

        // SMSG_LEVEL_LINKING_RESULT (0x4502EA), reader 0x6091E0:
        //     Read<uint8> (no shift - a real byte aligned uint8), ReadPackedGuid, Read<uint32>
        // RestrictedLevel is the single %d of ERR_LEVEL_LINKING_RESULT_LINKED (GlobalStrings 41311)
        // and is only read on the Linked branch.
        class LevelLinkingResult final : public ServerPacket
        {
        public:
            explicit LevelLinkingResult() : ServerPacket(SMSG_LEVEL_LINKING_RESULT, 1 + 16 + 4) { }

            WorldPacket const* Write() override;

            LevelLinkingResultType Result = LevelLinkingResultType::NoResult;
            /// UNVERIFIED: the client reads this GUID and then never uses it. Per
            /// DEFINITION_OF_DONE ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â§4.1 an unread field is a filler, but the server still has to put
            /// something there; the party sync partner is the likely meaning (see
            /// REQUEST_INVITE_CONFIRMATION.partyLevelLink, PartyInfoDocumentation.lua:871-885).
            ObjectGuid PlayerGUID;
            uint32 RestrictedLevel = 0;
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

        class MountClearFanfare final : public ClientPacket
        {
        public:
            explicit MountClearFanfare(WorldPacket&& packet) : ClientPacket(CMSG_MOUNT_CLEAR_FANFARE, std::move(packet)) { }

            void Read() override;

            uint32 MountSpellID = 0;
        };

        class CloseInteraction final : public ClientPacket
        {
        public:
            explicit CloseInteraction(WorldPacket&& packet) : ClientPacket(CMSG_CLOSE_INTERACTION, std::move(packet)) { }

            void Read() override;

            ObjectGuid SourceGuid;
        };

        // Subsystem-specific interaction closers (empty wire). The client sends these when the player leaves the
        // runeforge (legendary crafting) or trait-system window; the server clears the matching interaction gate.
        class CloseRuneforgeInteraction final : public ClientPacket
        {
        public:
            explicit CloseRuneforgeInteraction(WorldPacket&& packet) : ClientPacket(CMSG_CLOSE_RUNEFORGE_INTERACTION, std::move(packet)) { }

            void Read() override { }
        };

        class CloseTraitSystemInteraction final : public ClientPacket
        {
        public:
            explicit CloseTraitSystemInteraction(WorldPacket&& packet) : ClientPacket(CMSG_CLOSE_TRAIT_SYSTEM_INTERACTION, std::move(packet)) { }

            void Read() override { }
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

        // Cancels the SMSG_START_TIMER countdown of a given type. Wire (12.0.7/68275) is a single
        // uint32 carrying the CountdownTimerType - verified against the client deserializer for
        // SMSG_STOP_TIMER (0x45003F), which performs exactly one 4-byte read.
        class StopTimer final : public ServerPacket
        {
        public:
            explicit StopTimer() : ServerPacket(SMSG_STOP_TIMER, 4) { }

            WorldPacket const* Write() override;

            CountdownTimerType Type = {};
        };

        // One entry of the client's "world elapsed timer" list (client type name: JamElaspedTimer).
        //
        // Wire, derived from the 68275 client deserializers and cross-checked against the
        // known-good SMSG_START_TIMER layout in the same extraction:
        //     { int64 CurrentDuration; uint32 TimerID; }
        // i.e. the 8-byte duration comes FIRST. (This is a field-order/width change from the
        // 7.3.5-era layout, where TimerID came first and the duration was a uint32.)
        //
        // TimerID indexes WorldElapsedTimer.db2. The client reads the timer *type* from that DB2
        // row - it is NOT on the wire - and Blizzard_ScenarioObjectiveTracker only renders rows
        // whose Type is ChallengeMode(1) or ProvingGround(2). See ElapsedTimerMgr.h.
        struct ElapsedTimer
        {
            Duration<Seconds> CurrentDuration;
            uint32 TimerID = 0;
        };

        ByteBuffer& operator<<(ByteBuffer& data, ElapsedTimer const& timer);

        // Starts (or re-bases) a single elapsed timer. CurrentDuration is the time already elapsed;
        // the client free-runs its own clock from that baseline.
        class StartElapsedTimer final : public ServerPacket
        {
        public:
            explicit StartElapsedTimer() : ServerPacket(SMSG_START_ELAPSED_TIMER, 8 + 4) { }

            WorldPacket const* Write() override;

            ElapsedTimer Timer;
        };

        // Bulk form, used to resynchronise every active timer on zone-in / relog. The client's
        // PLAYER_ENTERING_WORLD handler calls GetWorldElapsedTimers(), so this is the packet that
        // repopulates that list.
        class StartElapsedTimers final : public ServerPacket
        {
        public:
            explicit StartElapsedTimers() : ServerPacket(SMSG_START_ELAPSED_TIMERS, 4) { }

            WorldPacket const* Write() override;

            std::vector<ElapsedTimer> Timers;
        };

        // Wire: { uint32 TimerID; bit KeepTimer; } - verified against the client deserializer,
        // which reads the flag as the top bit of one byte (matching OptionalInit/FlushBits packing).
        class StopElapsedTimer final : public ServerPacket
        {
        public:
            explicit StopElapsedTimer() : ServerPacket(SMSG_STOP_ELAPSED_TIMER, 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 TimerID = 0;
            bool KeepTimer = false;
        };


        // One entry of the client's "world elapsed timer" list (client type name: JamElaspedTimer).
        //
        // Wire, derived from the 68275 client deserializers and cross-checked against the
        // known-good SMSG_START_TIMER layout in the same extraction:
        //     { int64 CurrentDuration; uint32 TimerID; }
        // i.e. the 8-byte duration comes FIRST. (This is a field-order/width change from the
        // 7.3.5-era layout, where TimerID came first and the duration was a uint32.)
        //
        // TimerID indexes WorldElapsedTimer.db2. The client reads the timer *type* from that DB2
        // row - it is NOT on the wire - and Blizzard_ScenarioObjectiveTracker only renders rows
        // whose Type is ChallengeMode(1) or ProvingGround(2). See ElapsedTimerMgr.h.

        ByteBuffer& operator<<(ByteBuffer& data, ElapsedTimer const& timer);

        // Starts (or re-bases) a single elapsed timer. CurrentDuration is the time already elapsed;
        // the client free-runs its own clock from that baseline.

        // Bulk form, used to resynchronise every active timer on zone-in / relog. The client's
        // PLAYER_ENTERING_WORLD handler calls GetWorldElapsedTimers(), so this is the packet that
        // repopulates that list.

        // Wire: { uint32 TimerID; bit KeepTimer; } - verified against the client deserializer,
        // which reads the flag as the top bit of one byte (matching OptionalInit/FlushBits packing).

        class QueryCountdownTimer final : public ClientPacket
        {
        public:
            explicit QueryCountdownTimer(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_COUNTDOWN_TIMER, std::move(packet)) { }

            void Read() override;

            CountdownTimerType TimerType = {};
        };

        class DoCountdown final : public ClientPacket
        {
        public:
            explicit DoCountdown(WorldPacket&& packet) : ClientPacket(CMSG_DO_COUNTDOWN, std::move(packet)) { }

            void Read() override;

            uint32 TotalTime = 0;       // countdown duration in seconds
            Optional<uint8> Type;       // present only when the client sends a timer type
            bool Flag = false;
        };

        class GetRemainingGameTime final : public ClientPacket
        {
        public:
            explicit GetRemainingGameTime(WorldPacket&& packet) : ClientPacket(CMSG_GET_REMAINING_GAME_TIME, std::move(packet)) { }

            void Read() override { }
        };

        class GetRemainingGameTimeResponse final : public ServerPacket
        {
        public:
            explicit GetRemainingGameTimeResponse() : ServerPacket(SMSG_GET_REMAINING_GAME_TIME_RESPONSE, 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            uint32 SecondsRemaining = 0;
            uint32 GameTimeParam = 0;
            bool Unlimited = false;
        };

        class SetStopConversation final : public ClientPacket
        {
        public:
            explicit SetStopConversation(WorldPacket&& packet) : ClientPacket(CMSG_SET_STOP_CONVERSATION, std::move(packet)) { }

            void Read() override;

            ObjectGuid ConversationGUID;
        };

        class ConversationLineStarted final : public ClientPacket
        {
        public:
            explicit ConversationLineStarted(WorldPacket&& packet) : ClientPacket(CMSG_CONVERSATION_LINE_STARTED, std::move(packet)) { }

            void Read() override;

            ObjectGuid ConversationGUID;
            uint32 LineID = 0;
        };

        // Sent once the client has the cinematic belonging to a conversation loaded and ready to play.
        // Wire layout recovered offline from the client's own serializer (build 68275, serializer RVA
        // 0x6B3C70): a single PackedGuid, the same shape as SetStopConversation. Corroborated by
        // SMSG_TRIGGER_CINEMATIC already carrying a ConversationGuid - that is the guid echoed back here.
        class ConversationCinematicReady final : public ClientPacket
        {
        public:
            explicit ConversationCinematicReady(WorldPacket&& packet) : ClientPacket(CMSG_CONVERSATION_CINEMATIC_READY, std::move(packet)) { }

            void Read() override;

            ObjectGuid ConversationGUID;
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

        /*
         * Client family 0x64 (12.1.0.69382) - player state and UI remote control.
         *
         * Every wire layout below is read off the client dispatcher SMSG_Dispatch_fam_64 @ RVA 0x67C100
         * (image base 0x7FF780FD0000) unless a different reader RVA is named on the class.
         * Bit sections are MSB-first and are flushed before every byte-aligned field, which is exactly
         * ByteBuffer::WriteBits / FlushBits.
         *
         * UNVERIFIED (whole family, one reservation, stated once here instead of on 23 classes):
         * the layouts come from 12.1.0.69382, where this family is 0x64, but this tree still numbers
         * the family 0x5F (12.0.7.68275) in Opcodes.h. Nothing here has been proven against a running
         * 12.1 client, because a 12.1 client would not map the 0x5F values registered here onto the
         * 0x64 dispatcher cases the layouts were read from. Renumbering the family is a chain move
         * across every unit and belongs to the orchestrator, not here.
         * What HAS been checked is that the older build agrees wherever it can be made to speak.
         * There are two independent cross-build witnesses and together they cover 12 of the 23.
         *
         * (a) The 68275 dispatcher extraction (C:\dumps\all_smsg_layouts_68275.json, family 0x5F)
         *     resolves a concrete field list for TEN of the 23, and all ten match the 12.1 layout
         *     field for field:
         *       0x5F0003  u8 + bytes[len]                     (bits<1>+bits<6> in one byte, string)
         *       0x5F0006  uint32 + bool(bit7)
         *       0x5F000E  ObjectGuid+u32+u32+u8+9*u32+5*u8+4 strings   (5*u8 = the 35-bit section)
         *       0x5F000F  bool(bit7)
         *       0x5F0016  uint32 + bytes[len]
         *       0x5F001A  u32,u32,u32,<8>,<8>,u32,u32          (both counts first, 24 B per element)
         *       0x5F002B  uint32 + bool(bit7)
         *       0x5F0030  3*uint32 + bool(bit7)
         *       0x5F0032  struct{u8,u8,u32,string,u8,u8,u8,string,string}
         *       0x5F0033  bool(bit7) + that same struct
         *     0x5F0017 comes back as "varbits" - a bit field whose width the extractor did not
         *     resolve: consistent with bits<2>, but not by itself conclusive. The other twelve stop
         *     at "bytes[rest]" (the case hands the consumer the raw buffer), so for them the older
         *     build neither confirms nor contradicts.
         *     0x5F0033 is worth singling out: the struct restarts with its OWN u8,u8 length pair
         *     after the leading bit. That is the older build independently confirming that the
         *     Delayed bit occupies a byte of its own - see PlayerDelayedUploadScreenshot::Write.
         *
         * (b) Reference bytes exist on BOTH sides of the renumbering for three of the messages, and
         *     they are identical in length and content across it. Full scan of all 75 PKT 3.1
         *     recordings under C:\sniff (9.93M records, 28 builds):
         *       0x..0006  136 packets over 18 builds - 106 as 0x5F (65940..68974), 30 as 0x64
         *                 (69273..69404, 13 of them at the target builds 69382/69404). Every one is
         *                 5 bytes and every one is 00 00 00 00 00.
         *       0x..0017  3 packets (68453, 69382, 69404). Every one is 1 byte, 0x80.
         *       0x..0025  17 packets - 10 as 0x5F, 7 as 0x64. Every one is 4 bytes.
         *     The two prefixes never co-occur in a single build, which is a second and independent
         *     witness that the renumbering moved the family as a block.
         *
         * What that leaves: 02, 05, 13, 15, 22, 24, 2A, 2C, 2D, 2E and 31 have no cross-build
         * evidence of any kind. For those eleven, D1 rests on the 12.1 dispatcher case alone.
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
        // Sniff: 3 recorded packets over the full C:\sniff scan - build 68453 as 0x5F0017, builds
        // 69382 and 69404 as 0x640017. All three are 1 byte 0x80 = 0b10...... = 2 = MaxLevel, so the
        // bits<2> reading is confirmed at the exact target build and across the renumbering.
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
        // (store meta string "UIEventToast"). Sniff, full C:\sniff scan: 17 packets, every one 4 bytes
        // - 10 as 0x5F0025 (builds 66562/66709/68275/68974, ids 183, 288, 345, 346, 370) and 7 as
        // 0x640025 (build 69273, ids 304, 327, 341). Confirms the bare uint32 on both sides of the
        // renumbering. No packet at 69382/69404, so no reference bytes at the exact target build.
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
        //
        // UNVERIFIED: when the server sends it. What the id means is settled, what triggers it is not.
        // The only consumer is EventRouting.lua:98 -> GameEvent.HandleShowPartyPoseUi ->
        // ShowMatchCelebrationPartyPoseFrame, i.e. Blizzard_MatchCelebrationPartyPoseUI ("End of match
        // celebration screen"). Which match, the UI does not say. UiPartyPose.db2 at 69382 has 19 rows
        // and narrows it a long way: IDs 6/107-120 are Island Expeditions and Warfronts, which do NOT
        // use this packet at all (they run over ISLAND_COMPLETED / WARFRONT_COMPLETED and resolve the
        // pose with GetPartyPoseInfoByMapID). The four rows that carry the fields this frame actually
        // reads are 121-123 (map 2695, Plunderstorm - "You placed #%1986c", "Tournament Winner!") and
        // 124 (map 2664 Fungal Folly, "Delve Complete!"). So the probable triggers are a Plunderstorm
        // match ending and a delve being completed - probable, not proven: no recording of either
        // situation exists, and the tree has neither system.
        // What IS ruled out is the battleground: no row of UiPartyPose names a battleground or arena
        // map, and the one recording that plays a rated battleground through to SMSG_PVP_MATCH_COMPLETE
        // (rated BG 12.0.7.pkt, 397916 records) contains no party pose packet. An earlier version of
        // this unit sent the pose from Battleground::EndBattleground; that call was removed rather
        // than marked, because a map lookup against a table with no battleground row could never have
        // fired anyway. Player::SendPartyPoseUI and .debug send partypose stay - they are how the
        // opcode gets tested and how a Plunderstorm or delve implementation will use it.
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
        // Case (RVA 0x67DEF2): Read<uint8> >> 7 first - a WHOLE byte for the one bit - then the same
        // reader 0x67BF70 on an embedded object, which restarts with two fresh byte reads for its
        // bits<13>. The bit therefore does NOT share a section with the url length; this message is
        // exactly one byte longer than SMSG_PLAYER_UPLOAD_SCREENSHOT. Min 7 bytes (1 + 2 + 4, empty
        // url, no headers); max 7 + 8191 + N * 2049.
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
        // Sniff: 3 recorded packets over the full C:\sniff scan, all 1 byte - 68453 (0x3A0297) 0x00,
        // 69382 (0x3D0293) 0x20, 69404 (0x3D0293) 0x00. Read as bits<3> MSB-first those are wire 0,
        // 1, 0, i.e. Closed / Clicked / Closed - so two of the three remapped values in the enum
        // above are confirmed by recorded bytes; only WebRedirect (wire 4, body 0x80) is not.
        // Build 69382 carries the complete round trip: SMSG 0x640017 body 0x80 (MaxLevel) answered by
        // CMSG 0x3D0293 body 0x20 (Clicked). That is the only recorded request/response pair of this
        // unit, and it sits at the exact target build.
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

        class ChromieTimeSelectExpansion final : public ClientPacket
        {
        public:
            explicit ChromieTimeSelectExpansion(WorldPacket&& packet) : ClientPacket(CMSG_CHROMIE_TIME_SELECT_EXPANSION, std::move(packet)) { }

            void Read() override;

            ObjectGuid Vendor;     // packed GUID of the Chromie NPC the player is interacting with
            int32 ExpansionID = 0; // UIChromieTimeExpansionInfo.ID (NOT the Expansions enum)
        };

        class ChromieTimeSelectExpansionSuccess final : public ServerPacket
        {
        public:
            ChromieTimeSelectExpansionSuccess() : ServerPacket(SMSG_CHROMIE_TIME_SELECT_EXPANSION_SUCCESS, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class TimerunningSeasonEnded final : public ServerPacket
        {
        public:
            TimerunningSeasonEnded() : ServerPacket(SMSG_TIMERUNNING_SEASON_ENDED, 4) { }

            WorldPacket const* Write() override;

            uint32 SeasonID = 0;
        };

        // Wire layout (12.0.5, confirmed via sniff):
        //   block { uint32 ConditionalFlagsCount; uint8 FactionGroup; uint32 ChromieTimeExpansionMask;
        //           uint32 ConditionalFlags[ConditionalFlagsCount]; }
        //   Two consecutive blocks: [Previous, Current].
        //   The first send of a session carries a default-empty Previous block (capture A rec 721);
        //   later no-transition pulses send [current, current]; state changes send [pre, post].
        struct CTROptionsBlock
        {
            std::vector<uint32> ConditionalFlags;
            uint8 FactionGroup = 0;
            uint32 ChromieTimeExpansionMask = 0;
        };

        class SetCtrOptions final : public ServerPacket
        {
        public:
            SetCtrOptions() : ServerPacket(SMSG_SET_CTR_OPTIONS, 26) { }

            WorldPacket const* Write() override;

            CTROptionsBlock Previous;
            CTROptionsBlock Current;
        };

        // SMSG_DISPLAY_WORLD_TEXT (0x420296) — floats a server-authored, already-formatted string in
        // the 3D world (the engine behind the Lua AddWorldText / AddCustomWorldText bindings), NOT a
        // chat line, NOT a centre-screen notification and NOT the Lua combat-text system. The client
        // handler runs the text through the string-token formatter with Arg1/Arg2 as the two numeric
        // substitution arguments, then hands it to the world-text renderer; it raises no Lua event and
        // performs no DB2 lookup, so the display string is entirely the server's to compose.
        //
        // Wire, verified against build-68275/68974 captures (5 distinct bodies, zero leftover bytes):
        //   PackedGuid Guid    — anchor unit; a null guid makes the client fall back to the receiver
        //   uint32     Arg1
        //   uint32     Arg2
        //   Bits<12>   Text length, then FlushBits (the 4 pad bits are 0 in every sample)
        //   char[len]  Text    — no NUL on the wire
        //
        // It is a shared channel: retail sends "|cff94008B+XP" anchored on the creature you killed,
        // "|cnGOLD_FONT_COLOR:+Gold|r" and "|cnYELLOW_FONT_COLOR:+Neighborly|r" with a null guid, and
        // "|cff19FF19+Satisfaction|r" anchored on a player. Do not model it as any one system's packet.
        class DisplayWorldText final : public ServerPacket
        {
        public:
            explicit DisplayWorldText() : ServerPacket(SMSG_DISPLAY_WORLD_TEXT) { }

            WorldPacket const* Write() override;

            ObjectGuid Guid;
            uint32 Arg1 = 0;
            uint32 Arg2 = 0;
            std::string Text;
        };


        class TransferCurrencyFromAccountCharacter final : public ClientPacket
        {
        public:
            explicit TransferCurrencyFromAccountCharacter(WorldPacket&& packet)
                : ClientPacket(CMSG_TRANSFER_CURRENCY_FROM_ACCOUNT_CHARACTER, std::move(packet)) { }

            void Read() override;

            ObjectGuid SourceCharacterGUID;
            int32 CurrencyID = 0;
            int32 Quantity = 0;
        };



        class CurrencyTransferResult final : public ServerPacket
        {
        public:
            explicit CurrencyTransferResult() : ServerPacket(SMSG_CURRENCY_TRANSFER_RESULT) { }

            WorldPacket const* Write() override;

            int32 CurrencyID = 0;
            int32 Quantity = 0;
            int32 TotalQuantity = 0;
            AccountCurrencyTransferResult Result = AccountCurrencyTransferResult::Ok;
        };

    }
}

#endif // TRINITYCORE_MISC_PACKETS_H

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

#include "MiscPackets.h"
#include "Errors.h"
#include "PacketOperators.h"
#include "PacketUtilities.h"
#include "Player.h"

namespace WorldPackets::Misc
{
WorldPacket const* BindPointUpdate::Write()
{
    _worldPacket << BindPosition;
    _worldPacket << uint32(BindMapID);
    _worldPacket << uint32(BindAreaID);

    return &_worldPacket;
}

WorldPacket const* PlayerBound::Write()
{
    _worldPacket << BinderID;
    _worldPacket << uint32(AreaID);

    return &_worldPacket;
}

WorldPacket const* InvalidatePlayer::Write()
{
    _worldPacket << Guid;

    return &_worldPacket;
}

WorldPacket const* LoginSetTimeSpeed::Write()
{
    _worldPacket << ServerTime;
    _worldPacket << GameTime;
    _worldPacket << float(NewSpeed);
    _worldPacket << uint32(ServerTimeHolidayOffset);
    _worldPacket << uint32(GameTimeHolidayOffset);

    return &_worldPacket;
}

WorldPacket const* SetCurrency::Write()
{
    _worldPacket << int32(Type);
    _worldPacket << int32(Quantity);
    _worldPacket << uint32(Flags);
    _worldPacket << Size<uint32>(Toasts);

    for (Item::UiEventToast const& toast : Toasts)
        _worldPacket << toast;

    _worldPacket << OptionalInit(WeeklyQuantity);
    _worldPacket << OptionalInit(TrackedQuantity);
    _worldPacket << OptionalInit(MaxQuantity);
    _worldPacket << OptionalInit(TotalEarned);
    _worldPacket << Bits<1>(SuppressChatLog);
    _worldPacket << OptionalInit(QuantityChange);
    _worldPacket << OptionalInit(QuantityGainSource);
    _worldPacket << OptionalInit(QuantityLostSource);
    _worldPacket << OptionalInit(FirstCraftOperationID);
    _worldPacket << OptionalInit(NextRechargeTime);
    _worldPacket << OptionalInit(RechargeCycleStartTime);
    _worldPacket << OptionalInit(OverflownCurrencyID);
    _worldPacket.FlushBits();

    if (WeeklyQuantity)
        _worldPacket << int32(*WeeklyQuantity);

    if (TrackedQuantity)
        _worldPacket << int32(*TrackedQuantity);

    if (MaxQuantity)
        _worldPacket << int32(*MaxQuantity);

    if (TotalEarned)
        _worldPacket << int32(*TotalEarned);

    if (QuantityChange)
        _worldPacket << int32(*QuantityChange);

    if (QuantityGainSource)
        _worldPacket << int32(*QuantityGainSource);

    if (QuantityLostSource)
        _worldPacket << int32(*QuantityLostSource);

    if (FirstCraftOperationID)
        _worldPacket << uint32(*FirstCraftOperationID);

    if (NextRechargeTime)
        _worldPacket << *NextRechargeTime;

    if (RechargeCycleStartTime)
        _worldPacket << *RechargeCycleStartTime;

    if (OverflownCurrencyID)
        _worldPacket << int32(*OverflownCurrencyID);

    return &_worldPacket;
}

void SetCurrencyFlags::Read()
{
    _worldPacket >> CurrencyID;
    _worldPacket >> As<uint8>(Flags);
}

void SetSelection::Read()
{
    _worldPacket >> Selection;
}

void SetPreferredCemetery::Read()
{
    _worldPacket >> CemeteryID;
}

WorldPacket const* SetupCurrency::Write()
{
    _worldPacket << Size<uint32>(Data);

    for (Record const& data : Data)
    {
        _worldPacket << int32(data.Type);
        _worldPacket << int32(data.Quantity);
        _worldPacket << uint8(data.Flags);

        _worldPacket << OptionalInit(data.WeeklyQuantity);
        _worldPacket << OptionalInit(data.MaxWeeklyQuantity);
        _worldPacket << OptionalInit(data.TrackedQuantity);
        _worldPacket << OptionalInit(data.MaxQuantity);
        _worldPacket << OptionalInit(data.TotalEarned);
        _worldPacket << OptionalInit(data.NextRechargeTime);
        _worldPacket << OptionalInit(data.RechargeCycleStartTime);
        _worldPacket.FlushBits();

        if (data.WeeklyQuantity)
            _worldPacket << uint32(*data.WeeklyQuantity);
        if (data.MaxWeeklyQuantity)
            _worldPacket << uint32(*data.MaxWeeklyQuantity);
        if (data.TrackedQuantity)
            _worldPacket << uint32(*data.TrackedQuantity);
        if (data.MaxQuantity)
            _worldPacket << int32(*data.MaxQuantity);
        if (data.TotalEarned)
            _worldPacket << int32(*data.TotalEarned);
        if (data.NextRechargeTime)
            _worldPacket << *data.NextRechargeTime;
        if (data.RechargeCycleStartTime)
            _worldPacket << *data.RechargeCycleStartTime;
    }

    return &_worldPacket;
}

WorldPacket const* ReattachResurrect::Write()
{
    _worldPacket << uint8(Unknown1);
    _worldPacket << uint8(Unknown2);

    return &_worldPacket;
}

void ViolenceLevel::Read()
{
    _worldPacket >> ViolenceLvl;
}

WorldPacket const* TimeSyncRequest::Write()
{
    _worldPacket << SequenceIndex;

    return &_worldPacket;
}

void TimeSyncResponse::Read()
{
    _worldPacket >> SequenceIndex;
    _worldPacket >> ClientTime;
}

void TimeSyncResponseFailed::Read()
{
    _worldPacket >> SequenceIndex;
}

void TimeSyncResponseDropped::Read()
{
    _worldPacket >> SequenceIndexA;
    _worldPacket >> SequenceIndexB;
}

void DiscardedTimeSyncAcks::Read()
{
    _worldPacket >> MaxSequenceIndex;
}

WorldPacket const* TimeAdjustment::Write()
{
    _worldPacket << uint32(SequenceIndex);
    _worldPacket << float(TimeScale);

    return &_worldPacket;
}

void TimeAdjustmentResponse::Read()
{
    _worldPacket >> SequenceIndex;
    _worldPacket >> ClientTime;
}

WorldPacket const* TriggerCinematic::Write()
{
    _worldPacket << uint32(CinematicID);
    _worldPacket << ConversationGuid;

    return &_worldPacket;
}

WorldPacket const* TriggerMovie::Write()
{
    _worldPacket << uint32(MovieID);

    return &_worldPacket;
}

WorldPacket const* ServerTimeOffset::Write()
{
    _worldPacket << Time;

    return &_worldPacket;
}

WorldPacket const* TutorialFlags::Write()
{
    _worldPacket.append(TutorialData);

    return &_worldPacket;
}

void TutorialSetFlag::Read()
{
    _worldPacket >> Bits<2>(Action);

    if (Action == TUTORIAL_ACTION_UPDATE)
        _worldPacket >> TutorialBit;
}

WorldPacket const* WorldServerInfo::Write()
{
    _worldPacket << int16(DifficultyID);
    _worldPacket << HouseGUID;
    _worldPacket << HouseOwnerAccountGUID;
    _worldPacket << HouseCosmeticOwnerGUID;
    _worldPacket << NeighborhoodGUID;
    _worldPacket << Bits<1>(IsTournamentRealm);
    _worldPacket << Bits<1>(XRealmPvpAlert);
    _worldPacket << Bits<1>(BlockExitingLoadingScreen);
    _worldPacket << OptionalInit(RestrictedAccountMaxLevel);
    _worldPacket << OptionalInit(RestrictedAccountMaxMoney);
    _worldPacket << OptionalInit(InstanceGroupSize);
    _worldPacket.FlushBits();

    if (RestrictedAccountMaxLevel)
        _worldPacket << uint32(*RestrictedAccountMaxLevel);

    if (RestrictedAccountMaxMoney)
        _worldPacket << uint64(*RestrictedAccountMaxMoney);

    if (InstanceGroupSize)
        _worldPacket << uint32(*InstanceGroupSize);

    return &_worldPacket;
}

void SetDungeonDifficulty::Read()
{
    _worldPacket >> DifficultyID;
}

void SetRaidDifficulty::Read()
{
    _worldPacket >> Legacy;
    _worldPacket >> DifficultyID;
}

WorldPacket const* ChangePlayerDifficultyResult::Write()
{
    // The client reads one byte and splits it Result = b >> 4, InCombat = (b >> 3) & 1, which is
    // what these two bit writes plus the flush produce. Both captured bodies open with exactly
    // this: 0xC0 = Result 12 / InCombat 0, and 0x60 = Result 6 / InCombat 0.
    _worldPacket << Bits<4>(Result);
    _worldPacket << Bits<1>(InCombat);
    _worldPacket.FlushBits();

    // Which trailing fields exist is decided by Result in the client's own reader; everything
    // not listed here is the leading byte and nothing else.
    switch (Result)
    {
        case ChangePlayerDifficultyResultCode::Cooldown:
        case ChangePlayerDifficultyResultCode::Pending:
            _worldPacket << int64(Cooldown);
            break;
        case ChangePlayerDifficultyResultCode::MapDifficultyMessage:
            _worldPacket << int32(MapDifficultyID);
            break;
        case ChangePlayerDifficultyResultCode::OtherHeroic:
            _worldPacket << PlayerGUID;
            break;
        case ChangePlayerDifficultyResultCode::Success:
            _worldPacket << int32(MapID);
            _worldPacket << uint16(DifficultyID);
            break;
        default:
            break;
    }

    return &_worldPacket;
}

WorldPacket const* DungeonDifficultySet::Write()
{
    _worldPacket << int16(DifficultyID);

    return &_worldPacket;
}

WorldPacket const* RaidDifficultySet::Write()
{
    _worldPacket << int32(Legacy);
    _worldPacket << int16(DifficultyID);

    return &_worldPacket;
}

WorldPacket const* CorpseReclaimDelay::Write()
{
    _worldPacket << Remaining;

    return &_worldPacket;
}

WorldPacket const* DeathReleaseLoc::Write()
{
    _worldPacket << MapID;
    _worldPacket << Loc;

    return &_worldPacket;
}

WorldPacket const* PreRessurect::Write()
{
    _worldPacket << PlayerGUID;

    return &_worldPacket;
}

void ReclaimCorpse::Read()
{
    _worldPacket >> CorpseGUID;
}

void RepopRequest::Read()
{
    _worldPacket >> Bits<1>(CheckInstance);
}

WorldPacket const* RequestCemeteryListResponse::Write()
{
    _worldPacket << Bits<1>(IsGossipTriggered);
    _worldPacket.FlushBits();

    _worldPacket << Size<uint32>(CemeteryID);
    for (uint32 cemetery : CemeteryID)
        _worldPacket << cemetery;

    return &_worldPacket;
}

void ResurrectResponse::Read()
{
    _worldPacket >> Resurrecter;
    _worldPacket >> Response;
}

WorldPacket const* Weather::Write()
{
    _worldPacket << uint32(WeatherID);
    _worldPacket << float(Intensity);
    _worldPacket << Bits<1>(Abrupt);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void StandStateChange::Read()
{
    _worldPacket >> As<uint8>(StandState);
}

WorldPacket const* StandStateUpdate::Write()
{
    _worldPacket << As<uint8>(State);
    _worldPacket << uint32(AnimKitID);

    return &_worldPacket;
}

WorldPacket const* SetAnimTier::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint8(Tier);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* StartMirrorTimer::Write()
{
    _worldPacket << uint8(Timer);
    _worldPacket << int32(Value);
    _worldPacket << int32(MaxValue);
    _worldPacket << int32(Scale);
    _worldPacket << int32(SpellID);
    _worldPacket << Bits<1>(Paused);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* PauseMirrorTimer::Write()
{
    _worldPacket << uint8(Timer);
    _worldPacket << Bits<1>(Paused);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* StopMirrorTimer::Write()
{
    _worldPacket << uint8(Timer);

    return &_worldPacket;
}

WorldPacket const* ExplorationExperience::Write()
{
    _worldPacket << int32(AreaID);
    _worldPacket << int32(Experience);

    return &_worldPacket;
}

WorldPacket const* LevelUpInfo::Write()
{
    _worldPacket << int32(Level);
    _worldPacket << int32(HealthDelta);

    for (int32 power : PowerDelta)
        _worldPacket << power;

    for (int32 stat : StatDelta)
        _worldPacket << stat;

    _worldPacket << int32(NumNewTalents);
    _worldPacket << int32(NumNewPvpTalentSlots);

    return &_worldPacket;
}

WorldPacket const* PlayMusic::Write()
{
    _worldPacket << uint32(SoundKitID);

    return &_worldPacket;
}

void RandomRollClient::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Min;
    _worldPacket >> Max;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* RandomRoll::Write()
{
    _worldPacket << Roller;
    _worldPacket << RollerWowAccount;
    _worldPacket << int32(Min);
    _worldPacket << int32(Max);
    _worldPacket << int32(Result);

    return &_worldPacket;
}

WorldPacket const* EnableBarberShop::Write()
{
    _worldPacket << uint32(CustomizationFeatureMask);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, PhaseShiftDataPhase const& phaseShiftDataPhase)
{
    data << uint32(phaseShiftDataPhase.PhaseFlags);
    data << uint16(phaseShiftDataPhase.Id);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PhaseShiftData const& phaseShiftData)
{
    data << uint32(phaseShiftData.PhaseShiftFlags);
    data << WorldPackets::Size<uint32>(phaseShiftData.Phases);
    data << phaseShiftData.PersonalGUID;
    for (PhaseShiftDataPhase const& phaseShiftDataPhase : phaseShiftData.Phases)
        data << phaseShiftDataPhase;

    return data;
}

WorldPacket const* PhaseShiftChange::Write()
{
    _worldPacket << Client;
    _worldPacket << Phaseshift;
    _worldPacket << uint32(VisibleMapIDs.size() * 2);           // size in bytes
    for (uint16 visibleMapId : VisibleMapIDs)
        _worldPacket << uint16(visibleMapId);                   // Active terrain swap map id

    _worldPacket << uint32(PreloadMapIDs.size() * 2);           // size in bytes
    for (uint16 preloadMapId : PreloadMapIDs)
        _worldPacket << uint16(preloadMapId);                   // Inactive terrain swap map id

    _worldPacket << uint32(UiMapPhaseIDs.size() * 2);           // size in bytes
    for (uint16 uiMapPhaseId : UiMapPhaseIDs)
        _worldPacket << uint16(uiMapPhaseId);                   // phase id, controls only map display (visible on all maps)

    return &_worldPacket;
}

WorldPacket const* ZoneUnderAttack::Write()
{
    _worldPacket << int32(AreaID);

    return &_worldPacket;
}

WorldPacket const* DurabilityDamageDeath::Write()
{
    _worldPacket << int32(Percent);

    return &_worldPacket;
}

void ObjectUpdateFailed::Read()
{
    _worldPacket >> ObjectGUID;
}

void ObjectUpdateRescued::Read()
{
    _worldPacket >> ObjectGUID;
}

WorldPacket const* PlayObjectSound::Write()
{
    _worldPacket << int32(SoundKitID);
    _worldPacket << SourceObjectGUID;
    _worldPacket << TargetObjectGUID;
    _worldPacket << Position;
    _worldPacket << int32(BroadcastTextID);

    return &_worldPacket;
}

WorldPacket const* PlaySound::Write()
{
    _worldPacket << int32(SoundKitID);
    _worldPacket << SourceObjectGuid;
    _worldPacket << int32(BroadcastTextID);

    return &_worldPacket;
}

WorldPacket const* PlaySpeakerbotSound::Write()
{
    _worldPacket << SourceObjectGUID;
    _worldPacket << int32(SoundKitID);

    return &_worldPacket;
}

WorldPacket const* StopSpeakerbotSound::Write()
{
    _worldPacket << SourceObjectGUID;

    return &_worldPacket;
}

void FarSight::Read()
{
    _worldPacket >> Bits<1>(Enable);
}

void SaveCUFProfiles::Read()
{
    _worldPacket >> Size<uint32>(CUFProfiles);
    for (std::unique_ptr<CUFProfile>& cufProfile : CUFProfiles)
    {
        cufProfile = std::make_unique<CUFProfile>();

        _worldPacket >> SizedString::BitsSize<7>(cufProfile->ProfileName);

        // Bool Options
        for (uint8 option = 0; option < CUF_BOOL_OPTIONS_COUNT; option++)
            cufProfile->BoolOptions.set(option, _worldPacket.ReadBit());

        // Other Options
        _worldPacket >> cufProfile->FrameHeight;
        _worldPacket >> cufProfile->FrameWidth;

        _worldPacket >> cufProfile->SortBy;
        _worldPacket >> cufProfile->HealthText;

        _worldPacket >> cufProfile->TopPoint;
        _worldPacket >> cufProfile->BottomPoint;
        _worldPacket >> cufProfile->LeftPoint;

        _worldPacket >> cufProfile->TopOffset;
        _worldPacket >> cufProfile->BottomOffset;
        _worldPacket >> cufProfile->LeftOffset;

        _worldPacket >> SizedString::Data(cufProfile->ProfileName);
    }
}

WorldPacket const* LoadCUFProfiles::Write()
{
    _worldPacket << Size<uint32>(CUFProfiles);

    for (CUFProfile const* cufProfile : CUFProfiles)
    {
        _worldPacket << SizedString::BitsSize<7>(cufProfile->ProfileName);

        // Bool Options
        for (uint8 option = 0; option < CUF_BOOL_OPTIONS_COUNT; option++)
            _worldPacket.WriteBit(cufProfile->BoolOptions[option]);

        // Other Options
        _worldPacket << uint16(cufProfile->FrameHeight);
        _worldPacket << uint16(cufProfile->FrameWidth);

        _worldPacket << uint8(cufProfile->SortBy);
        _worldPacket << uint8(cufProfile->HealthText);

        _worldPacket << uint8(cufProfile->TopPoint);
        _worldPacket << uint8(cufProfile->BottomPoint);
        _worldPacket << uint8(cufProfile->LeftPoint);

        _worldPacket << uint16(cufProfile->TopOffset);
        _worldPacket << uint16(cufProfile->BottomOffset);
        _worldPacket << uint16(cufProfile->LeftOffset);

        _worldPacket << SizedString::Data(cufProfile->ProfileName);
    }

    return &_worldPacket;
}

WorldPacket const* PlayOneShotAnimKit::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint16(AnimKitID);

    return &_worldPacket;
}

WorldPacket const* SetAIAnimKit::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint16(AnimKitID);

    return &_worldPacket;
}

WorldPacket const* SetMovementAnimKit::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint16(AnimKitID);

    return &_worldPacket;
}

WorldPacket const* SetMeleeAnimKit::Write()
{
    _worldPacket << Unit;
    _worldPacket << uint16(AnimKitID);

    return &_worldPacket;
}

WorldPacket const* SetPlayHoverAnim::Write()
{
    _worldPacket << UnitGUID;
    _worldPacket << Bits<1>(PlayHoverAnim);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void SetPvP::Read()
{
    _worldPacket >> Bits<1>(EnablePVP);
}

void SetWarMode::Read()
{
    _worldPacket >> Bits<1>(Enable);
}

WorldPacket const* AccountHeirloomUpdate::Write()
{
    _worldPacket << Bits<1>(IsFullUpdate);
    _worldPacket.FlushBits();

    _worldPacket << int8(ItemCollectionType);

    // both lists have to have the same size
    _worldPacket << Size<uint32>(*Heirlooms);
    _worldPacket << Size<uint32>(*Heirlooms);

    for (auto const& [itemId, _] : *Heirlooms)
        _worldPacket << int32(itemId);

    for (auto const& [_, data] : *Heirlooms)
        _worldPacket << uint32(data.flags);

    return &_worldPacket;
}

void MountSpecial::Read()
{
    _worldPacket >> Size<uint32>(SpellVisualKitIDs);
    _worldPacket >> SequenceVariation;
    for (int32& spellVisualKitId : SpellVisualKitIDs)
        _worldPacket >> spellVisualKitId;
}

WorldPacket const* SpecialMountAnim::Write()
{
    _worldPacket << UnitGUID;
    _worldPacket << Size<uint32>(SpellVisualKitIDs);
    _worldPacket << int32(SequenceVariation);
    if (!SpellVisualKitIDs.empty())
        _worldPacket.append(SpellVisualKitIDs.data(), SpellVisualKitIDs.size());

    return &_worldPacket;
}

WorldPacket const* CrossedInebriationThreshold::Write()
{
    _worldPacket << Guid;
    _worldPacket << int32(Threshold);
    _worldPacket << int32(ItemID);

    return &_worldPacket;
}

void SetTaxiBenchmarkMode::Read()
{
    _worldPacket >> Bits<1>(Enable);
}

WorldPacket const* OverrideLight::Write()
{
    _worldPacket << int32(AreaLightID);
    _worldPacket << int32(OverrideLightID);
    _worldPacket << int32(TransitionMilliseconds);

    return &_worldPacket;
}

WorldPacket const* DisplayGameError::Write()
{
    _worldPacket << uint32(Error);
    _worldPacket << OptionalInit(Arg);
    _worldPacket << OptionalInit(Arg2);
    _worldPacket.FlushBits();

    if (Arg)
        _worldPacket << int32(*Arg);

    if (Arg2)
        _worldPacket << int32(*Arg2);

    return &_worldPacket;
}

// Length 1 + 2..18 + 4 = 7..23 bytes.
// UNVERIFIED: no capture exists (0 packets in 72 sniffs). The PackedGuid leg is calibrated against
// SMSG_XP_GAIN_ABORTED (0x45006D, 284 captured packets, 14..29 bytes for pguid + 3 x uint32).
WorldPacket const* LevelLinkingResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << PlayerGUID;
    _worldPacket << uint32(RestrictedLevel);

    return &_worldPacket;
}

WorldPacket const* AccountMountUpdate::Write()
{
    _worldPacket << Bits<1>(IsFullUpdate);
    _worldPacket << Size<uint32>(*Mounts);

    for (auto const& [spellId, flags] : *Mounts)
    {
        _worldPacket << int32(spellId);
        _worldPacket << Bits<4>(flags);
    }

    _worldPacket.FlushBits();

    return &_worldPacket;
}

void MountSetFavorite::Read()
{
    _worldPacket >> MountSpellID;
    _worldPacket >> Bits<1>(IsFavorite);
}

void MountClearFanfare::Read()
{
    _worldPacket >> MountSpellID;
}

void CloseInteraction::Read()
{
    _worldPacket >> SourceGuid;
}

WorldPacket const* StartTimer::Write()
{
    _worldPacket << TotalTime;
    _worldPacket << int32(Type);
    _worldPacket << TimeLeft;
    _worldPacket << OptionalInit(PlayerGuid);
    _worldPacket.FlushBits();

    if (PlayerGuid)
        _worldPacket << *PlayerGuid;

    return &_worldPacket;
}
WorldPacket const* StopTimer::Write()
{
    _worldPacket << int32(Type);

    return &_worldPacket;
}

// Duration first, then the id - see the ElapsedTimer comment in MiscPackets.h.
ByteBuffer& operator<<(ByteBuffer& data, ElapsedTimer const& timer)
{
    data << timer.CurrentDuration;
    data << uint32(timer.TimerID);

    return data;
}

WorldPacket const* StartElapsedTimer::Write()
{
    _worldPacket << Timer;

    return &_worldPacket;
}

WorldPacket const* StartElapsedTimers::Write()
{
    _worldPacket << uint32(Timers.size());
    for (ElapsedTimer const& timer : Timers)
        _worldPacket << timer;

    return &_worldPacket;
}

WorldPacket const* StopElapsedTimer::Write()
{
    _worldPacket << uint32(TimerID);
    _worldPacket.WriteBit(KeepTimer);
    _worldPacket.FlushBits();

    return &_worldPacket;
}


// Duration first, then the id - see the ElapsedTimer comment in MiscPackets.h.

void QueryCountdownTimer::Read()
{
    _worldPacket >> As<int32>(TimerType);
}

void DoCountdown::Read()
{
    // Wire (client serializer 0x5DDE90): bit HasType, bit Flag, FlushBits, uint32 TotalTime, [uint8 Type if HasType]
    bool hasType = _worldPacket.ReadBit();
    Flag = _worldPacket.ReadBit();
    _worldPacket >> TotalTime;
    if (hasType)
    {
        uint8 type;
        _worldPacket >> type;
        Type = type;
    }
}

WorldPacket const* GetRemainingGameTimeResponse::Write()
{
    _worldPacket << uint32(SecondsRemaining);
    _worldPacket << uint32(GameTimeParam);
    _worldPacket.WriteBit(Unlimited);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void SetStopConversation::Read()
{
    _worldPacket >> ConversationGUID;
}

void ConversationLineStarted::Read()
{
    _worldPacket >> ConversationGUID;
    _worldPacket >> LineID;
}

WorldPacket const* SplashScreenShowLatest::Write()
{
    _worldPacket << int32(UISplashScreenID);

    return &_worldPacket;
}

WorldPacket const* DisplayToast::Write()
{
    _worldPacket << uint64(Quantity);
    _worldPacket << As<uint32>(DisplayToastMethod);
    _worldPacket << uint32(QuestID);

    _worldPacket << Bits<1>(Mailed);
    _worldPacket << Bits<2>(Type);
    _worldPacket << Bits<1>(IsSecondaryResult);

    switch (Type)
    {
        case DisplayToastType::NewItem:
            _worldPacket << Bits<1>(BonusRoll);
            _worldPacket << Bits<1>(ForceToast);
            _worldPacket << Item;
            _worldPacket << int32(LootSpec);
            _worldPacket << int8(Gender);
            break;
        case DisplayToastType::NewCurrency:
            _worldPacket << uint32(CurrencyID);
            break;
        default:
            break;
    }

    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* AccountWarbandSceneUpdate::Write()
{
    _worldPacket << Bits<1>(IsFullUpdate);
    _worldPacket << Size<uint32>(*WarbandScenes);
    _worldPacket << Size<uint32>(*WarbandScenes);
    _worldPacket << Size<uint32>(*WarbandScenes);

    for (auto const& [warbandSceneId, _] : *WarbandScenes)
        _worldPacket << uint32(warbandSceneId);

    for (auto const& [_, data] : *WarbandScenes)
        _worldPacket << Bits<1>(data.Flags.HasFlag(WarbandSceneCollectionFlags::Favorite));

    for (auto const& [_, data] : *WarbandScenes)
        _worldPacket << Bits<1>(data.Flags.HasFlag(WarbandSceneCollectionFlags::HasFanfare));

    _worldPacket.FlushBits();

    return &_worldPacket;
}

// ----------------------------------------------------------------------------------------
// Einheit w4_cmsg_43_3D - Sendeseite der Sammelfamilien 0x43 / 0x3D, Phase A.
// Belege: Client-Serializer 12.1.0.69382, RVA je Methode. Bit-Sektionen sind MSB-first;
// ein Write<uint8> mit Schiebeausdruck ist kein Feld, sondern Teil der Bit-Sektion.
// ----------------------------------------------------------------------------------------

void SpectateChange::Read()
{
    // 0x6D2650: ein eingebettetes Bit, dann FlushBits.
    _worldPacket >> Bits<1>(NextTarget);
    _worldPacket.ResetBitPos();
}

void SpectateSetNextTarget::Read()
{
    // 0x6D26C0: WritePackedGuid.
    _worldPacket >> Target;
}

void LowLevelRaid1::Read()
{
    // 0x6AB1E0
    _worldPacket >> Bits<1>(Enable);
    _worldPacket.ResetBitPos();
}

void QuickJoinAutoAcceptRequests::Read()
{
    // 0x6AFAD0
    _worldPacket >> Bits<1>(AutoAccept);
    _worldPacket.ResetBitPos();
}

void RequestChatLogin::Read()
{
    // 0x6A3600
    _worldPacket >> Bits<1>(Login);
    _worldPacket.ResetBitPos();
}

void ClassTalentsNotifyEmptyConfig::Read()
{
    // 0x6CD620
    _worldPacket >> ConfigID;
}

void ClassTalentsNotifyValidationFailed::Read()
{
    // 0x6D2550
    _worldPacket >> ConfigID;
}

void GMTicketAcknowledgeSurvey::Read()
{
    // 0x6AAA60
    _worldPacket >> CaseIndex;
}

void AddAccountCosmetic::Read()
{
    // 0x6CEF70
    _worldPacket >> ItemGUID;
}

void UpdateCraftingNpcRecipes::Read()
{
    // 0x6D0550
    _worldPacket >> NpcGUID;
}

void IslandQueue::Read()
{
    // 0x6D1710: guid @+0x20, uint32 @+0x30
    _worldPacket >> NpcGUID;
    _worldPacket >> DifficultyID;
}

void ShowTradeSkill::Read()
{
    // 0x6ABE20: guid @+0x20, uint32 @+0x30, uint32 @+0x34
    _worldPacket >> PlayerGUID;
    _worldPacket >> SpellID;
    _worldPacket >> SkillLineID;
}

void QuestDrivenScenarioStateChange::Read()
{
    // Writer 0x6D3660. Die BEIDEN Zaehler stehen vorne hintereinander, beide Elementbloecke
    // kommen danach - dasselbe Deklarationsstellen-Muster wie im Lobby-Matchmaker-Block.
    // Der Client schreibt sie als int32 aus wachsenden Vektoren OHNE clientseitige Obergrenze;
    // die Schranke ist ausschliesslich serverseitig, und negative Werte muessen abgewiesen
    // werden (der Client laeuft mit vorzeichenbehafteten > 0 - Tests).
    _worldPacket >> StateChangeType;
    _worldPacket >> ScenarioID;
    _worldPacket >> QuestDrivenScenarioID;
    _worldPacket >> Field48;
    _worldPacket >> ClientUnixTime;
    _worldPacket >> Field64;
    _worldPacket >> Field68;

    _worldPacket >> Size<int32>(Stages);
    _worldPacket >> Size<int32>(Currencies);

    for (ScenarioStageInfo& stage : Stages)
    {
        _worldPacket >> stage.StartTime;
        _worldPacket >> stage.EndTime;
        _worldPacket >> stage.Field16;
        _worldPacket >> stage.StepOrderIndex;
    }

    for (ScenarioCurrencyInfo& currency : Currencies)
    {
        _worldPacket >> currency.CurrencyID;
        _worldPacket >> currency.Quantity;
    }
}

void WorldLootObjectClick::Read()
{
    // 0x6D2740: uint32 @+0x20 VOR der guid @+0x28
    _worldPacket >> ClickType;
    _worldPacket >> ObjectGUID;
}

void UpgradeRuneforgeLegendary::Read()
{
    // 0x6D1B90: uint32 aus **(a1+32), dann vier uint8 aus (*(a1+32))+4..+7
    _worldPacket >> Field0;
    _worldPacket >> LegendaryBagSlot;
    _worldPacket >> LegendarySlot;
    _worldPacket >> UpgradeItemBagSlot;
    _worldPacket >> UpgradeItemSlot;
}

void SetExcludedChatCensorSources::Read()
{
    // 0x6AFB60
    _worldPacket >> Sources;
}

void SilenceTalkerInParty::Read()
{
    // 0x6A83E0 - Hausmuster der 0x43-Gruppenopcodes. Das Praesenzbit steht ZUERST
    // (hoeherwertiges Bit des Akkumulators), das bedingte uint8 NACH der GUID.
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Bits<1>(Silence);
    _worldPacket.ResetBitPos();

    _worldPacket >> Target;

    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void Warden3Data::Read()
{
    // 0x6A2940: uint32 @+0x20, uint32 Size @+0x3C, dann Size Rohbytes aus *(a1+0x30).
    // Sniff: 1398 Pakete, 40..16280 Byte - 8 + Size geht auf.
    _worldPacket >> Kind;
    _worldPacket >> Bytes::Size<uint32>(Data);
    _worldPacket >> Bytes::Data(Data);
}

void AddonList::Read()
{
    // Aussennachricht 0x6A1C70. Reihenfolge am Draht weicht von der Objektreihenfolge ab:
    // das uint8 @+0x34 steht NACH dem uint32 @+0x38.
    _worldPacket >> RequestGUID;
    _worldPacket >> Field30;
    _worldPacket >> Field38;
    _worldPacket >> Field34;

    _worldPacket >> Size<uint32>(AddOns);

    // Element 0x69FDE0 (JamCliAddOnInfo, 88 Byte). Beide Laengen sind 10 Bit und
    // schliessen die NUL EIN. Der Elementkopf ist genau 3 Byte: 10 + 10 + 1 + 1 = 22 Bit,
    // LSB-seitig mit 2 Null-Bit gepolstert (FlushBits 0x5D4EA0, Fall 6). Die drei
    // Write<uint8> der Subplanzeile sind zwei ausgefuehrte - der dritte ist der else-Zweig
    // desselben Bytes bzw. der inline ausgeschriebene Uebertrag von WriteBits 0x5D4A20,
    // der bei 4 bzw. 5 anstehenden Bit nicht laeuft. Herleitung am Serializer in
    // MiscPackets.h ueber der Struktur AddonInfo.
    for (AddonInfo& addon : AddOns)
    {

        _worldPacket >> SizedCString::BitsSize<10>(addon.Name);
        _worldPacket >> SizedCString::BitsSize<10>(addon.Version);
        _worldPacket >> Bits<1>(addon.Flag1);
        _worldPacket >> Bits<1>(addon.Flag2);
        _worldPacket.ResetBitPos();

        _worldPacket >> SizedCString::Data(addon.Name);
        _worldPacket >> SizedCString::Data(addon.Version);
    }
}

void EngineSurvey::Read()
{
    // Rumpf 0x6AD300. Die Feldliste ist gegen das 294-Byte-Paket aus
    // 12.1.0.69273_preyandwqpart1.pkt byteweise nachgerechnet: 141 Byte feste Breite,
    // 104 Bit Bit-Sektion (13 Byte, geht ohne Fuellbits auf), 140 Byte Rohbytes.
    _worldPacket >> SurveyVersion;
    _worldPacket >> SurveyPatch;
    _worldPacket >> CpuVendorID;
    _worldPacket >> CpuPackages;
    _worldPacket >> CpuCores;
    _worldPacket >> CpuThreads;
    _worldPacket >> Const2;
    _worldPacket >> CpuField1C;
    _worldPacket >> Reserved0;
    _worldPacket >> OsField0;
    _worldPacket >> OsMajorVersion;
    _worldPacket >> OsMinorVersion;
    _worldPacket >> OsField10;
    _worldPacket >> OsBuildNumber;
    _worldPacket >> PhysicalMemory;
    _worldPacket >> Field240;
    _worldPacket >> MonitorCountMinusOne;
    _worldPacket >> DesktopWidth;
    _worldPacket >> DesktopHeight;
    _worldPacket >> MonitorWidth;
    _worldPacket >> MonitorHeight;
    _worldPacket >> GpuVendorID;
    _worldPacket >> GpuDeviceID;
    _worldPacket >> GxField0;
    _worldPacket >> GxField1;
    _worldPacket >> GxField2;
    _worldPacket >> GxField3;
    _worldPacket >> DedicatedVideoMemory;
    _worldPacket >> SharedSystemMemory;
    _worldPacket >> GxApi;
    _worldPacket >> OsField4;
    _worldPacket >> OsField18;
    _worldPacket >> OsField1C;
    _worldPacket >> OsField20;
    _worldPacket >> OsField24;
    _worldPacket >> CpuFeatureMask;
    _worldPacket >> CpuExtra;
    _worldPacket >> CpuField14;
    _worldPacket >> CpuField16;
    _worldPacket >> CpuField17;

    // Bit-Sektion: die Laengenfelder stehen an der Deklarationsposition ihres Strings,
    // die Bool-Bloecke dazwischen. Die Rohbytes wandern geschlossen ans Ende.
    std::size_t flag = 0;

    _worldPacket >> SizedString::BitsSize<6>(CpuVendor);
    _worldPacket >> SizedString::BitsSize<6>(CpuBrand);
    for (std::size_t i = 0; i < 21; ++i)
        Flags[flag++] = _worldPacket.ReadBit();

    _worldPacket >> SizedString::BitsSize<6>(GpuName);
    for (std::size_t i = 0; i < 5; ++i)
        Flags[flag++] = _worldPacket.ReadBit();

    _worldPacket >> SizedString::BitsSize<7>(OsName);
    _worldPacket >> SizedString::BitsSize<6>(OsExtra);
    for (std::size_t i = 0; i < 16; ++i)
        Flags[flag++] = _worldPacket.ReadBit();

    _worldPacket >> SizedString::BitsSize<7>(BaseBoardManufacturer);
    _worldPacket >> SizedString::BitsSize<7>(BaseBoardProduct);
    _worldPacket >> SizedString::BitsSize<7>(BiosVendor);
    _worldPacket >> SizedString::BitsSize<4>(BiosReleaseDate);
    _worldPacket >> SizedString::BitsSize<4>(BiosVersion);
    for (std::size_t i = 0; i < 2; ++i)
        Flags[flag++] = _worldPacket.ReadBit();

    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::Data(CpuVendor);
    _worldPacket >> SizedString::Data(CpuBrand);
    _worldPacket >> SizedString::Data(GpuName);
    _worldPacket >> SizedString::Data(OsName);
    _worldPacket >> SizedString::Data(OsExtra);
    _worldPacket >> SizedString::Data(BaseBoardManufacturer);
    _worldPacket >> SizedString::Data(BaseBoardProduct);
    _worldPacket >> SizedString::Data(BiosVendor);
    _worldPacket >> SizedString::Data(BiosReleaseDate);
    _worldPacket >> SizedString::Data(BiosVersion);
}

void StartSpectatorWarGame::Read()
{
    // 0x6A2150: zwei ausgeschriebene Bloecke {guid, uint32, uint16}, dann uint64, dann EIN Bit.
    for (SpectatorWarGamePlayer& player : Players)
    {
        _worldPacket >> player.PlayerGUID;
        _worldPacket >> player.VirtualRealmAddress;
        _worldPacket >> player.RealmIndex;
    }

    _worldPacket >> QueueID;
    _worldPacket >> Bits<1>(TournamentRules);
    _worldPacket.ResetBitPos();
}

void QuickJoinRespondToInvite::Read()
{
    // 0x6AF830
    _worldPacket >> QueueGUID;
    _worldPacket >> ApplicantGUID;
    _worldPacket >> Bits<1>(Accept);
    _worldPacket.ResetBitPos();
}

void QuickJoinSignalToastDisplayed::Read()
{
    // 0x6AF6B0. Der float steht bei Objekt +0x30; im Dekompilat ist er unsichtbar, weil IDA
    // das xmm-Argument verliert - im Disassemblat steht movss xmm1, [rbp+0x30].
    _worldPacket >> GroupGUID;
    _worldPacket >> Priority;

    _worldPacket >> Size<uint32>(Members);
    for (ObjectGuid& member : Members)
        _worldPacket >> member;

    _worldPacket >> Bits<1>(Flag0);
    _worldPacket >> Bits<1>(Flag1);
    _worldPacket.ResetBitPos();
}

void QuickJoinRequestInvite::Read()
{
    // 0x6AF8C0. Bit-Sektion 9 + 9 + 1 = 19 Bit -> 3 Byte.
    _worldPacket >> SizedString::BitsSize<9>(TargetName);
    _worldPacket >> SizedString::BitsSize<9>(TargetRealm);
    _worldPacket >> Bits<1>(Flag);
    _worldPacket.ResetBitPos();

    _worldPacket >> QueueID;
    _worldPacket >> GroupGUID;
    _worldPacket >> ClubID;
    _worldPacket >> Roles;

    _worldPacket >> SizedString::Data(TargetName);
    _worldPacket >> SizedString::Data(TargetRealm);
}

void QuickJoinRequestInviteWithConfirmation::Read()
{
    // 0x6B1420. Bit-Sektion 9 + 9 = 18 Bit -> ebenfalls 3 Byte, aber OHNE das dritte Bit.
    _worldPacket >> SizedString::BitsSize<9>(TargetName);
    _worldPacket >> SizedString::BitsSize<9>(TargetRealm);
    _worldPacket.ResetBitPos();

    _worldPacket >> QueueID;
    _worldPacket >> GroupGUID;
    _worldPacket >> RequestID;

    _worldPacket >> SizedString::Data(TargetName);
    _worldPacket >> SizedString::Data(TargetRealm);
}

void ServerValidationSignatureRequest::Read()
{
    // 0x6B2860: bits24 = Laenge inkl. NUL (0 bedeutet: gar keine Bytes, auch kein NUL).
    // Der FlushBits danach schreibt kein Byte - 24 mod 8 == 0.
    _worldPacket >> SizedCString::BitsSize<24>(Signature);
    _worldPacket.ResetBitPos();

    _worldPacket >> RequestID;
    _worldPacket >> Guid;

    _worldPacket >> SizedCString::Data(Signature);
}

WorldPacket const* GodMode::Write()
{
    _worldPacket << Bits<1>(Enable);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* PetGodMode::Write()
{
    _worldPacket << Bits<1>(Enable);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* CooldownCheat::Write()
{
    _worldPacket << Bits<1>(Enable);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* ConsoleWrite::Write()
{
    // The length field carries the NUL, and the client's ReadDynString (0x347D750) consumes
    // nothing at all for a length of 0 or 1 - SizedCString writes length + 1 and omits both
    // string and NUL when empty, which is exactly that case.
    ASSERT(Text.length() <= MaxTextLength);

    _worldPacket << SizedCString::BitsSize<14>(Text);
    _worldPacket.FlushBits();
    _worldPacket << uint32(ColorType);
    _worldPacket << SizedCString::Data(Text);

    return &_worldPacket;
}

WorldPacket const* GameSpeedSet::Write()
{
    _worldPacket << float(Speed);

    return &_worldPacket;
}

WorldPacket const* RuneRegenDebug::Write()
{
    _worldPacket << uint32(RegenTimer);
    _worldPacket << uint32(BaseCooldown);
    _worldPacket << uint32(ActiveRuneMask);
    // Both counts go out before either payload - see the class comment.
    _worldPacket << Size<uint32>(Cooldowns);
    _worldPacket << Size<uint32>(RuneTypes);

    for (int32 cooldown : Cooldowns)
        _worldPacket << int32(cooldown);

    for (int32 runeType : RuneTypes)
        _worldPacket << int32(runeType);

    return &_worldPacket;
}

WorldPacket const* ForceAnim::Write()
{
    ASSERT(AnimName.length() <= MaxAnimNameLength);

    _worldPacket << UnitGUID;
    _worldPacket << SizedString::BitsSize<9>(AnimName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(AnimName);    // no NUL on the wire

    return &_worldPacket;
}

WorldPacket const* ForceAnimations::Write()
{
    _worldPacket << UnitGUID;
    // Both counts go out before either payload - see the class comment.
    _worldPacket << Size<uint32>(AnimIDs);
    _worldPacket << Size<uint32>(Variations);
    _worldPacket << uint32(LoopCount);
    _worldPacket << float(Speed);
    _worldPacket << uint8(BoneType);

    for (int32 animId : AnimIDs)
        _worldPacket << int32(animId);

    for (uint8 variation : Variations)
        _worldPacket << uint8(variation);

    return &_worldPacket;
}

void KioskEnableGodMode::Read()
{
    _worldPacket >> Bits<1>(Enable);
}

void SetGameEventDebugViewState::Read()
{
    _worldPacket >> ViewIndex;
    _worldPacket >> Bits<1>(State);
}
WorldPacket const* FailedPlayerCondition::Write()
{
    _worldPacket << int32(PlayerConditionID);

    return &_worldPacket;
}

WorldPacket const* GMRequestPlayerInfo::Write()
{
    // Both bit fields belong to ONE bit section: bit 7 is Flag, bits 6..1 are the length,
    // bit 0 is padding. Flushing between them would produce one byte too many.
    _worldPacket << Bits<1>(Flag);
    _worldPacket << SizedString::BitsSize<6>(Name);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(Name);

    return &_worldPacket;
}

WorldPacket const* GMPlayerInfo::Write()
{
    _worldPacket << Player;
    _worldPacket << int32(Data1);
    _worldPacket << int32(Data2);
    _worldPacket << uint8(Data3);

    for (int32 data : Data4)
        _worldPacket << int32(data);

    _worldPacket << SizedString::BitsSize<6>(Text1);
    _worldPacket << SizedString::BitsSize<7>(Text2);
    _worldPacket << SizedString::BitsSize<11>(Text3);
    _worldPacket << SizedString::BitsSize<11>(Text4);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(Text1);
    _worldPacket << SizedString::Data(Text2);
    _worldPacket << SizedString::Data(Text3);
    _worldPacket << SizedString::Data(Text4);

    return &_worldPacket;
}

WorldPacket const* PlayerSkinned::Write()
{
    _worldPacket << Bits<1>(FreeForAll);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* PlayerTutorialHighlightSpell::Write()
{
    _worldPacket << int32(SpellID);
    _worldPacket << SizedString::BitsSize<7>(GlobalStringTag);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(GlobalStringTag);

    return &_worldPacket;
}

WorldPacket const* PlayerOpenSubscriptionInterstitial::Write()
{
    _worldPacket << Bits<2>(Type);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* ScheduledAreaPoiUpdateResponse::Write()
{
    // Both counts first, then both payload arrays - see the class comment.
    _worldPacket << Size<uint32>(AreaPoiIDs);
    _worldPacket << Size<uint32>(Events);

    for (int32 areaPoiId : AreaPoiIDs)
        _worldPacket << int32(areaPoiId);

    for (ScheduledAreaPoiEvent const& event : Events)
    {
        _worldPacket << uint64(event.StartTime);
        _worldPacket << uint64(event.EndTime);
        _worldPacket << int32(event.EventSchedulerEventID);
        _worldPacket << int32(event.Data);
    }

    return &_worldPacket;
}

WorldPacket const* PlayerShowUiEventToast::Write()
{
    _worldPacket << int32(UiEventToastID);

    return &_worldPacket;
}

WorldPacket const* PlayerShowGenericWidgetDisplay::Write()
{
    _worldPacket << int32(UiGenericWidgetDisplayID);

    return &_worldPacket;
}

WorldPacket const* PlayerShowPartyPoseUI::Write()
{
    _worldPacket << int32(PartyPoseID);
    _worldPacket << Bits<1>(Victory);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* PlayerShowArrowCallout::Write()
{
    _worldPacket << int32(ArrowCalloutID);

    return &_worldPacket;
}

WorldPacket const* PlayerHideArrowCallout::Write()
{
    _worldPacket << int32(ArrowCalloutID);

    return &_worldPacket;
}

WorldPacket const* PlayerAcknowledgeArrowCallout::Write()
{
    _worldPacket << int32(ArrowCalloutID);

    return &_worldPacket;
}

WorldPacket const* PlayerEndOfMatchDetails::Write()
{
    _worldPacket << int32(Placement);
    _worldPacket << int32(Kills);
    _worldPacket << int32(PlunderAcquired);
    _worldPacket << Bits<1>(MatchEnded);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* ChallengeModeSetLeaverPenaltyTimer::Write()
{
    _worldPacket << Timer;

    return &_worldPacket;
}

namespace
{
// JamClientPlayerUploadScreenshotHeader, reader RVA 0x67BF70. Every string is a JamDynamicString:
// the length prefix counts the terminator and the client drops the whole message when the last byte
// of a string is not 0x00.
//
// SizedCString is exactly the right helper here, including for the empty string - checked against the
// reader rather than assumed. ReadDynString (0x347D750) opens with
//     0347D76C  cmp r8, 1          ; r8 = announced length
//     0347D770  ja  0x347D792      ; only lengths ABOVE 1 take the reading path
//     0347D772  mov rax, [rcx] / mov byte ptr [rax], 0 / mov al, 1   ; empty string, SUCCESS, ret
// and that branch never touches the stream - it does not even reach the buffer fetch at 0x35AF730.
// So an announced length of 0 or 1 consumes ZERO bytes on the wire, which is precisely what
// SizedCString writes: BitsSize announces length() + 1 (PacketOperators.h:329) and Data writes
// nothing when the string is empty (:368-375). TC's reader mirrors the same rule at :341.
// Writing an explicit 0x00 for an empty field - the intuitive "fix" - would push one byte the client
// never reads and desynchronise everything after it.
// For a non-empty string the reader takes len bytes and requires the last to be 0x00
// (0347D7B6 dec rdi / cmp byte ptr [rdi+rsi], 0 / jne fail), reporting length len-1; that is the
// other half of what SizedCString writes.
void WriteUploadScreenshotHeader(ByteBuffer& data, UploadScreenshotHeader const& header)
{
    data << SizedCString::BitsSize<13>(header.Url);
    data.FlushBits();

    data << Size<uint32>(header.Headers);
    data << SizedCString::Data(header.Url);

    for (UploadScreenshotHeaderField const& field : header.Headers)
    {
        data << SizedCString::BitsSize<10>(field.Name);
        data << SizedCString::BitsSize<10>(field.Value);
        data.FlushBits();

        data << SizedCString::Data(field.Name);
        data << SizedCString::Data(field.Value);
    }
}
}

WorldPacket const* PlayerUploadScreenshot::Write()
{
    WriteUploadScreenshotHeader(_worldPacket, Header);

    return &_worldPacket;
}

WorldPacket const* PlayerDelayedUploadScreenshot::Write()
{
    // The Delayed bit gets a byte of its OWN. It does not share a bit section with the bits<13> url
    // length that follows, even though both are bit fields and nothing byte-aligned sits between them.
    // Dispatcher case 0x640033 (RVA 0x67DEF2) reads a whole byte and throws away everything but bit 7:
    //     0067DF5A  call 0x35AF050          ; Read<uint8> - advances the position by one
    //     0067DF6A  shr  al, 7              ; Delayed; bits 6..0 are dropped
    //     0067DF76  call 0x67BF70           ; header reader, three args, no bit state among them
    // and the header reader then opens with two FRESH byte reads for the 13 bit length. There is no
    // bit accumulator anywhere in this path - the whole family unpacks bits out of whole bytes by
    // hand - so no accumulator can survive the call. The delayed variant is therefore one byte longer
    // than SMSG_PLAYER_UPLOAD_SCREENSHOT, not the same length.
    // An earlier version of this function omitted the flush, which made every packet exactly one byte
    // short: the client would have built the url length from (DelayedByte << 5) | (B0 >> 3) and lost
    // sync for the rest of the message.
    _worldPacket << Bits<1>(Delayed);
    _worldPacket.FlushBits();

    WriteUploadScreenshotHeader(_worldPacket, Header);

    return &_worldPacket;
}

void AbandonNPEResponse::Read()
{
    _worldPacket >> Bits<1>(Abandon);
}

void SubscriptionInterstitialResponse::Read()
{
    _worldPacket >> Bits<3>(Response);
}

WorldPacket const* DisplayWorldText::Write()
{
    _worldPacket << Guid;
    _worldPacket << uint32(Arg1);
    _worldPacket << uint32(Arg2);
    _worldPacket << SizedString::BitsSize<12>(Text);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(Text);

    return &_worldPacket;
}

void TransferCurrencyFromAccountCharacter::Read()
{
    _worldPacket >> SourceCharacterGUID;
    _worldPacket >> CurrencyID;
    _worldPacket >> Quantity;
}

WorldPacket const* AccountCharacterCurrencyLists::Write()
{
    _worldPacket << Size<uint32>(Characters);
    _worldPacket << Size<uint32>(CurrencyData);

    for (CharacterCurrencyData const& character : Characters)
    {
        _worldPacket << character.CharacterGUID;
        _worldPacket << uint8(character.ClassID);
        _worldPacket << int32(character.Level);
        _worldPacket << SizedString::BitsSize<6>(character.CharacterName);
    }

    _worldPacket.FlushBits();

    for (CharacterCurrencyData const& character : Characters)
        _worldPacket << SizedString::Data(character.CharacterName);

    for (CurrencyQuantityData const& currency : CurrencyData)
    {
        _worldPacket << currency.CharacterGUID;
        _worldPacket << int32(currency.CurrencyTypeID);
        _worldPacket << int32(currency.Quantity);
    }

    return &_worldPacket;
}

WorldPacket const* CurrencyTransferResult::Write()
{
    _worldPacket << int32(CurrencyID);
    _worldPacket << int32(Quantity);
    _worldPacket << int32(TotalQuantity);
    _worldPacket << uint32(Result);

    return &_worldPacket;
}

WorldPacket const* CurrencyTransferLog::Write()
{
    _worldPacket << Size<uint32>(Entries);

    // Retail 12.0.7 entry layout (verified against sniff SMSG_CURRENCY_TRANSFER_LOG):
    // Source, Dest, CurrencyTypeID, QuantityReceived, QuantitySent, Timestamp, trailing int32(0).
    for (CurrencyTransferLogEntry const& entry : Entries)
    {
        _worldPacket << entry.SourceCharacterGUID;
        _worldPacket << entry.DestCharacterGUID;
        _worldPacket << int32(entry.CurrencyTypeID);
        _worldPacket << int32(entry.QuantityReceived);
        _worldPacket << int32(entry.QuantitySent);
        _worldPacket << uint32(entry.Timestamp);
        _worldPacket << int32(0);
    }

    return &_worldPacket;
}

WorldPacket const* SetCtrOptions::Write()
{
    auto writeBlock = [&](CTROptionsBlock const& block)
    {
        _worldPacket << uint32(block.ConditionalFlags.size());
        _worldPacket << uint8(block.FactionGroup);
        _worldPacket << uint32(block.ChromieTimeExpansionMask);
        for (uint32 flag : block.ConditionalFlags)
            _worldPacket << uint32(flag);
    };

    writeBlock(Previous);
    writeBlock(Current);

    return &_worldPacket;
}
}

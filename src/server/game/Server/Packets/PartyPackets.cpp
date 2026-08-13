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

#include "PartyPackets.h"
#include "Group.h"
#include "PacketOperators.h"
#include "Pet.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RealmList.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "Vehicle.h"
#include <algorithm>

namespace WorldPackets::Party
{
WorldPacket const* PartyCommandResult::Write()
{
    _worldPacket << SizedString::BitsSize<9>(Name);
    _worldPacket << Bits<4>(Command);
    _worldPacket << Bits<6>(Result);

    _worldPacket << uint32(ResultData);
    _worldPacket << ResultGUID;
    _worldPacket << SizedString::Data(Name);

    return &_worldPacket;
}

void PartyInviteClient::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);

    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::BitsSize<9>(TargetName);
    _worldPacket >> SizedString::BitsSize<9>(TargetRealm);

    _worldPacket >> ProposedRoles;
    _worldPacket >> TargetGUID;

    _worldPacket >> SizedString::Data(TargetName);
    _worldPacket >> SizedString::Data(TargetRealm);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* PartyInvite::Write()
{
    _worldPacket << Bits<1>(CanAccept);
    _worldPacket << Bits<1>(IsXRealm);
    _worldPacket << Bits<1>(IsXNativeRealm);
    _worldPacket << Bits<1>(ShouldSquelch);
    _worldPacket << Bits<1>(AllowMultipleRoles);
    _worldPacket << Bits<1>(QuestSessionActive);
    _worldPacket << SizedString::BitsSize<6>(InviterName);
    _worldPacket << Bits<1>(IsCrossFaction);

    _worldPacket << InviterRealm;
    _worldPacket << InviterGUID;
    _worldPacket << InviterBNetAccountId;
    _worldPacket << uint16(InviterCfgRealmID);
    _worldPacket << uint8(ProposedRoles);
    _worldPacket << Size<uint32>(LfgSlots);
    _worldPacket << uint32(LfgCompletedMask);

    _worldPacket << SizedString::Data(InviterName);

    for (uint32 LfgSlot : LfgSlots)
        _worldPacket << LfgSlot;

    return &_worldPacket;
}

void PartyInvite::Initialize(Player const* inviter, int32 proposedRoles, bool canAccept)
{
    CanAccept = canAccept;

    InviterName = inviter->GetName();
    InviterGUID = inviter->GetGUID();
    InviterBNetAccountId = inviter->m_playerData->BnetAccount;

    ProposedRoles = proposedRoles;

    if (std::shared_ptr<Realm const> realm = sRealmList->GetRealm(*inviter->m_playerData->VirtualPlayerRealm))
        InviterRealm = Auth::VirtualRealmInfo(realm->Id.GetAddress(), true, false, realm->Name, realm->NormalizedName);
}

void PartyInviteResponse::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Bits<1>(Accept);
    _worldPacket >> OptionalInit(RolesDesired);

    if (PartyIndex)
        _worldPacket >> *PartyIndex;

    if (RolesDesired)
        _worldPacket >> *RolesDesired;
}

void PartyUninvite::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> SizedString::BitsSize<8>(Reason);

    _worldPacket >> TargetGUID;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;

    _worldPacket >> SizedString::Data(Reason);
}

WorldPacket const* GroupDecline::Write()
{
    _worldPacket << SizedString::BitsSize<9>(Name);
    _worldPacket.FlushBits();

    _worldPacket << SizedString::Data(Name);

    return &_worldPacket;
}

WorldPacket const* GroupUninvite::Write()
{
    _worldPacket << uint8(Reason);

    return &_worldPacket;
}

void RequestPartyMemberStats::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Size<uint32>(Targets);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;

    for (ObjectGuid& target : Targets)
        _worldPacket >> target;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyMemberPhase const& phase)
{
    data << uint32(phase.Flags);
    data << uint16(phase.Id);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyMemberPhaseStates const& phases)
{
    data << uint32(phases.PhaseShiftFlags);
    data << Size<uint32>(phases.List);
    data << phases.PersonalGUID;

    for (PartyMemberPhase const& phase : phases.List)
        data << phase;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyMemberAuraStates const& aura)
{
    data << int32(aura.SpellID);
    data << uint16(aura.Flags);
    data << uint32(aura.ActiveFlags);
    data << Size<int32>(aura.Points);
    for (float points : aura.Points)
        data << float(points);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, CTROptions const& ctrOptions)
{
    data << Size<uint32>(ctrOptions.ConditionalFlags);
    data << int8(ctrOptions.FactionGroup);
    data << uint32(ctrOptions.ChromieTimeExpansionMask);

    if (!ctrOptions.ConditionalFlags.empty())
        data.append(ctrOptions.ConditionalFlags.data(), ctrOptions.ConditionalFlags.size());

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyMemberPetStats const& petStats)
{
    data << petStats.GUID;
    data << int32(petStats.ModelId);
    data << int32(petStats.CurrentHealth);
    data << int32(petStats.MaxHealth);
    data << Size<uint32>(petStats.Auras);
    for (PartyMemberAuraStates const& aura : petStats.Auras)
        data << aura;

    data << SizedString::BitsSize<8>(petStats.Name);
    data.FlushBits();

    data << SizedString::Data(petStats.Name);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyMemberStats const& memberStats)
{
    for (uint32 i = 0; i < 2; i++)
        data << uint8(memberStats.PartyType[i]);

    data << uint32(memberStats.Status);
    data << uint8(memberStats.PowerType);
    data << uint16(memberStats.PowerDisplayID);
    data << int32(memberStats.CurrentHealth);
    data << int32(memberStats.MaxHealth);
    data << uint16(memberStats.CurrentPower);
    data << uint16(memberStats.MaxPower);
    data << uint16(memberStats.Level);
    data << uint16(memberStats.SpecID);
    data << uint16(memberStats.ZoneID);
    data << uint16(memberStats.WmoGroupID);
    data << uint32(memberStats.WmoDoodadPlacementID);
    data << int16(memberStats.PositionX);
    data << int16(memberStats.PositionY);
    data << int16(memberStats.PositionZ);
    data << int32(memberStats.VehicleSeat);
    data << Size<uint32>(memberStats.Auras);
    data << memberStats.Phases;
    data << memberStats.ChromieTime;

    for (PartyMemberAuraStates const& aura : memberStats.Auras)
        data << aura;

    data << OptionalInit(memberStats.PetStats);
    data.FlushBits();

    data << memberStats.DungeonScore;

    if (memberStats.PetStats)
        data << *memberStats.PetStats;

    return data;
}

WorldPacket const* PartyMemberFullState::Write()
{
    _worldPacket << Bits<1>(ForEnemy);

    _worldPacket << MemberStats;
    _worldPacket << MemberGuid;

    return &_worldPacket;
}

void PartyMemberStatsSnapshot::Assign(PartyMemberStats const& stats)
{
    Valid = true;

    PartyType[0] = stats.PartyType[0];
    PartyType[1] = stats.PartyType[1];
    Status = stats.Status;
    PowerType = stats.PowerType;
    PowerDisplayID = stats.PowerDisplayID;
    CurrentHealth = stats.CurrentHealth;
    MaxHealth = stats.MaxHealth;
    CurrentPower = stats.CurrentPower;
    MaxPower = stats.MaxPower;
    Level = stats.Level;
    SpecID = stats.SpecID;
    ZoneID = stats.ZoneID;
    WmoGroupID = stats.WmoGroupID;
    WmoDoodadPlacementID = stats.WmoDoodadPlacementID;
    PositionX = stats.PositionX;
    PositionY = stats.PositionY;
    PositionZ = stats.PositionZ;
    VehicleSeat = stats.VehicleSeat;

    Auras = stats.Auras;
    Phases = stats.Phases;
    PetStats = stats.PetStats;

    ChromieTime.ConditionalFlags.assign(stats.ChromieTime.ConditionalFlags.begin(), stats.ChromieTime.ConditionalFlags.end());
    ChromieTime.FactionGroup = stats.ChromieTime.FactionGroup;
    ChromieTime.ChromieTimeExpansionMask = stats.ChromieTime.ChromieTimeExpansionMask;
}

PartyMemberStateDelta PartyMemberPartialState::InitializeChanged(PartyMemberStats const& current, PartyMemberStatsSnapshot const& previous)
{
    // no baseline to diff against - the recipient needs the whole state
    if (!previous.Valid)
        return PartyMemberStateDelta::RequiresFullState;

    // a pet that went away cannot be expressed here: the pet bit means "pet data included", not
    // "pet exists", so there is no way to tell the client to drop the pet it already has
    if (previous.PetStats && !current.PetStats)
        return PartyMemberStateDelta::RequiresFullState;

    if (current.PartyType[0] != previous.PartyType[0] || current.PartyType[1] != previous.PartyType[1])
        PartyType = std::array<int8, 2>{ { current.PartyType[0], current.PartyType[1] } };

    if (current.Status != previous.Status)
        Status = current.Status;

    if (current.PowerType != previous.PowerType)
        PowerType = current.PowerType;

    if (current.PowerDisplayID != previous.PowerDisplayID)
        PowerDisplayID = current.PowerDisplayID;

    if (current.CurrentHealth != previous.CurrentHealth)
        CurrentHealth = current.CurrentHealth;

    if (current.MaxHealth != previous.MaxHealth)
        MaxHealth = current.MaxHealth;

    if (current.CurrentPower != previous.CurrentPower)
        CurrentPower = current.CurrentPower;

    if (current.MaxPower != previous.MaxPower)
        MaxPower = current.MaxPower;

    if (current.Level != previous.Level)
        Level = current.Level;

    if (current.SpecID != previous.SpecID)
        SpecID = current.SpecID;

    if (current.ZoneID != previous.ZoneID)
        ZoneID = current.ZoneID;

    if (current.WmoGroupID != previous.WmoGroupID)
        WmoGroupID = current.WmoGroupID;

    if (current.WmoDoodadPlacementID != previous.WmoDoodadPlacementID)
        WmoDoodadPlacementID = current.WmoDoodadPlacementID;

    // the three coordinates share a single presence bit
    if (current.PositionX != previous.PositionX || current.PositionY != previous.PositionY || current.PositionZ != previous.PositionZ)
        Position = PartyMemberPosition{ current.PositionX, current.PositionY, current.PositionZ };

    if (current.VehicleSeat != previous.VehicleSeat)
        VehicleSeat = current.VehicleSeat;

    // lists are all or nothing - there is no per element delta on the wire
    if (current.Auras != previous.Auras)
        Auras = current.Auras;

    if (current.Phases != previous.Phases)
        Phases = current.Phases;

    if (current.PetStats)
    {
        PartyMemberPetStats const& pet = *current.PetStats;
        PartyMemberPetStats const* oldPet = previous.PetStats ? &*previous.PetStats : nullptr;
        PartyMemberPetPartialStats partialPet;

        if (!oldPet || pet.Name != oldPet->Name)
            partialPet.Name = pet.Name;

        if (!oldPet || pet.GUID != oldPet->GUID)
            partialPet.GUID = pet.GUID;

        if (!oldPet || pet.ModelId != oldPet->ModelId)
            partialPet.ModelId = pet.ModelId;

        if (!oldPet || pet.CurrentHealth != oldPet->CurrentHealth)
            partialPet.CurrentHealth = pet.CurrentHealth;

        if (!oldPet || pet.MaxHealth != oldPet->MaxHealth)
            partialPet.MaxHealth = pet.MaxHealth;

        if (!oldPet || pet.Auras != oldPet->Auras)
            partialPet.Auras = pet.Auras;

        if (partialPet.HasData())
            PetStats = std::move(partialPet);
    }

    if (current.ChromieTime.FactionGroup != previous.ChromieTime.FactionGroup
        || current.ChromieTime.ChromieTimeExpansionMask != previous.ChromieTime.ChromieTimeExpansionMask
        || !std::equal(current.ChromieTime.ConditionalFlags.begin(), current.ChromieTime.ConditionalFlags.end(),
            previous.ChromieTime.ConditionalFlags.begin(), previous.ChromieTime.ConditionalFlags.end()))
    {
        PartyMemberCTRState& ctrOptions = ChromieTime.emplace();
        ctrOptions.ConditionalFlags.assign(current.ChromieTime.ConditionalFlags.begin(), current.ChromieTime.ConditionalFlags.end());
        ctrOptions.FactionGroup = current.ChromieTime.FactionGroup;
        ctrOptions.ChromieTimeExpansionMask = current.ChromieTime.ChromieTimeExpansionMask;
    }

    bool const anything = PartyType || Status || PowerType || PowerDisplayID || CurrentHealth || MaxHealth
        || CurrentPower || MaxPower || Level || SpecID || ZoneID || WmoGroupID || WmoDoodadPlacementID
        || Position || VehicleSeat || Auras || PetStats || Phases || ChromieTime;

    return anything ? PartyMemberStateDelta::Partial : PartyMemberStateDelta::Unchanged;
}

WorldPacket const* PartyMemberPartialState::Write()
{
    uint8 mask[3] = { };

    if (ForEnemy)                   mask[0] |= 0x80;
    if (PartyType)                  mask[0] |= 0x10;
    if (Status)                     mask[0] |= 0x08;
    if (PowerType)                  mask[0] |= 0x04;
    if (PowerDisplayID)             mask[0] |= 0x02;
    if (CurrentHealth)              mask[0] |= 0x01;

    if (MaxHealth)                  mask[1] |= 0x80;
    if (CurrentPower)               mask[1] |= 0x40;
    if (MaxPower)                   mask[1] |= 0x20;
    if (Level)                      mask[1] |= 0x10;
    if (SpecID)                     mask[1] |= 0x08;
    if (ZoneID)                     mask[1] |= 0x04;
    if (WmoGroupID)                 mask[1] |= 0x02;
    if (WmoDoodadPlacementID)       mask[1] |= 0x01;

    if (Position)                   mask[2] |= 0x80;
    if (VehicleSeat)                mask[2] |= 0x40;
    if (Auras)                      mask[2] |= 0x20;
    if (PetStats)                   mask[2] |= 0x10;
    if (Phases)                     mask[2] |= 0x08;
    if (ChromieTime)                mask[2] |= 0x04;

    _worldPacket << uint8(mask[0]);
    _worldPacket << uint8(mask[1]);
    _worldPacket << uint8(mask[2]);

    // the pet block is read before the member guid
    if (PetStats)
    {
        uint8 petMask = 0;
        if (PetStats->GUID)             petMask |= 0x80;
        if (PetStats->Name)             petMask |= 0x40;
        if (PetStats->ModelId)          petMask |= 0x20;
        if (PetStats->CurrentHealth)    petMask |= 0x10;
        if (PetStats->MaxHealth)        petMask |= 0x08;
        if (PetStats->Auras)            petMask |= 0x04;

        _worldPacket << uint8(petMask);

        // name is a plain length byte followed by that many characters, no terminator
        if (PetStats->Name)
        {
            uint8 const nameLength = uint8(std::min<std::size_t>(PetStats->Name->length(), 0xFF));
            _worldPacket << uint8(nameLength);
            _worldPacket.append(PetStats->Name->c_str(), nameLength);
        }

        if (PetStats->GUID)
            _worldPacket << *PetStats->GUID;

        if (PetStats->ModelId)
            _worldPacket << int32(*PetStats->ModelId);

        if (PetStats->CurrentHealth)
            _worldPacket << int32(*PetStats->CurrentHealth);

        if (PetStats->MaxHealth)
            _worldPacket << int32(*PetStats->MaxHealth);

        if (PetStats->Auras)
        {
            _worldPacket << Size<uint32>(*PetStats->Auras);
            for (PartyMemberAuraStates const& aura : *PetStats->Auras)
                _worldPacket << aura;
        }
    }

    _worldPacket << MemberGuid;

    if (PartyType)
    {
        _worldPacket << uint8((*PartyType)[0]);
        _worldPacket << uint8((*PartyType)[1]);
    }

    if (Status)
        _worldPacket << uint32(*Status);

    if (PowerType)
        _worldPacket << uint8(*PowerType);

    if (PowerDisplayID)
        _worldPacket << uint16(*PowerDisplayID);

    if (CurrentHealth)
        _worldPacket << int32(*CurrentHealth);

    if (MaxHealth)
        _worldPacket << int32(*MaxHealth);

    if (CurrentPower)
        _worldPacket << uint16(*CurrentPower);

    if (MaxPower)
        _worldPacket << uint16(*MaxPower);

    if (Level)
        _worldPacket << uint16(*Level);

    if (SpecID)
        _worldPacket << uint16(*SpecID);

    if (ZoneID)
        _worldPacket << uint16(*ZoneID);

    if (WmoGroupID)
        _worldPacket << uint16(*WmoGroupID);

    if (WmoDoodadPlacementID)
        _worldPacket << uint32(*WmoDoodadPlacementID);

    if (Position)
    {
        _worldPacket << int16(Position->X);
        _worldPacket << int16(Position->Y);
        _worldPacket << int16(Position->Z);
    }

    if (VehicleSeat)
        _worldPacket << int32(*VehicleSeat);

    if (Auras)
    {
        _worldPacket << Size<uint32>(*Auras);
        for (PartyMemberAuraStates const& aura : *Auras)
            _worldPacket << aura;
    }

    if (Phases)
        _worldPacket << *Phases;

    if (ChromieTime)
    {
        CTROptions ctrOptions;
        ctrOptions.ConditionalFlags = ChromieTime->ConditionalFlags;
        ctrOptions.FactionGroup = ChromieTime->FactionGroup;
        ctrOptions.ChromieTimeExpansionMask = ChromieTime->ChromieTimeExpansionMask;
        _worldPacket << ctrOptions;
    }

    return &_worldPacket;
}

void SetPartyLeader::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> TargetGUID;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void SetPartyAssignment::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Bits<1>(Set);
    _worldPacket >> Assignment;
    _worldPacket >> Target;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void SetRole::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> TargetGUID;
    _worldPacket >> Role;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* RoleChangedInform::Write()
{
    _worldPacket << uint8(PartyIndex);
    _worldPacket << From;
    _worldPacket << ChangedUnit;
    _worldPacket << uint8(OldRole);
    _worldPacket << uint8(NewRole);

    return &_worldPacket;
}

void LeaveGroup::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void SetLootMethod::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> LootMethod;
    _worldPacket >> LootMasterGUID;
    _worldPacket >> LootThreshold;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void MinimapPingClient::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> PositionX;
    _worldPacket >> PositionY;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* MinimapPing::Write()
{
    _worldPacket << Sender;
    _worldPacket << PositionX;
    _worldPacket << PositionY;

    return &_worldPacket;
}

void UpdateRaidTarget::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Target;
    _worldPacket >> Symbol;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* SendRaidTargetUpdateSingle::Write()
{
    _worldPacket << PartyIndex;
    _worldPacket << Symbol;
    _worldPacket << Target;
    _worldPacket << ChangedBy;

    return &_worldPacket;
}

WorldPacket const* SendRaidTargetUpdateAll::Write()
{
    _worldPacket << uint8(PartyIndex);
    _worldPacket << Size<uint32>(TargetIcons);

    for (auto& [symbol, target] : TargetIcons)
    {
        _worldPacket << target;
        _worldPacket << uint8(symbol);
    }

    return &_worldPacket;
}

void ConvertRaid::Read()
{
    _worldPacket >> Bits<1>(Raid);
}

void RequestPartyJoinUpdates::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void SetAssistantLeader::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Bits<1>(Apply);
    _worldPacket >> Target;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void DoReadyCheck::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* ReadyCheckStarted::Write()
{
    _worldPacket << PartyIndex;
    _worldPacket << PartyGUID;
    _worldPacket << InitiatorGUID;
    _worldPacket << Duration;

    return &_worldPacket;
}

void ReadyCheckResponseClient::Read()
{
    _worldPacket >> Bits<1>(IsReady);
    _worldPacket >> OptionalInit(PartyIndex);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* ReadyCheckResponse::Write()
{
    _worldPacket << PartyGUID;
    _worldPacket << Player;

    _worldPacket << Bits<1>(IsReady);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* ReadyCheckCompleted::Write()
{
    _worldPacket << PartyIndex;
    _worldPacket << PartyGUID;

    return &_worldPacket;
}

void OptOutOfLoot::Read()
{
    _worldPacket >> Bits<1>(PassOnLoot);
}

void InitiateRolePoll::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

WorldPacket const* RolePollInform::Write()
{
    _worldPacket << PartyIndex;
    _worldPacket << From;

    return &_worldPacket;
}

WorldPacket const* GroupNewLeader::Write()
{
    _worldPacket << PartyIndex;
    _worldPacket << SizedString::BitsSize<9>(Name);

    _worldPacket << SizedString::Data(Name);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, LeaverInfo const& leaverInfo)
{
    data << leaverInfo.BnetAccountGUID;
    data << float(leaverInfo.LeaveScore);
    data << uint32(leaverInfo.SeasonID);
    data << uint32(leaverInfo.TotalLeaves);
    data << uint32(leaverInfo.TotalSuccesses);
    data << int32(leaverInfo.ConsecutiveSuccesses);
    data << leaverInfo.LastPenaltyTime;
    data << leaverInfo.LeaverExpirationTime;
    data << int32(leaverInfo.Unknown_1120);
    data << Bits<1>(leaverInfo.LeaverStatus);
    data.FlushBits();

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyPlayerInfo const& playerInfo)
{
    data << SizedString::BitsSize<6>(playerInfo.Name);
    data << SizedCString::BitsSize<6>(playerInfo.VoiceStateID);
    data << Bits<1>(playerInfo.Connected);
    data << Bits<1>(playerInfo.VoiceChatSilenced);
    data << Bits<1>(playerInfo.FromSocialQueue);
    data << playerInfo.Leaver;
    data << playerInfo.GUID;
    data << uint8(playerInfo.Subgroup);
    data << uint8(playerInfo.Flags);
    data << uint8(playerInfo.RolesAssigned);
    data << uint8(playerInfo.Class);
    data << uint8(playerInfo.FactionGroup);
    data << SizedString::Data(playerInfo.Name);
    data << SizedCString::Data(playerInfo.VoiceStateID);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, ChallengeModeData const& challengeMode)
{
    data << int32(challengeMode.MapID);
    data << int32(challengeMode.InitialPlayerCount);
    data << uint64(challengeMode.InstanceID);
    data << challengeMode.StartTime;
    data << challengeMode.KeystoneOwnerGUID;
    data << challengeMode.LeaverGUID;
    data << challengeMode.InstanceAbandonVoteCooldown;
    data << Bits<1>(challengeMode.IsActive);
    data << Bits<1>(challengeMode.HasRestrictions);
    data << Bits<1>(challengeMode.CanVoteAbandon);
    data.FlushBits();

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyLFGInfo const& lfgInfos)
{
    data << uint32(lfgInfos.Slot);
    data << uint8(lfgInfos.MyFlags);
    data << uint32(lfgInfos.MyRandomSlot);
    data << uint8(lfgInfos.MyPartialClear);
    data << float(lfgInfos.MyGearDiff);
    data << uint8(lfgInfos.MyStrangerCount);
    data << uint8(lfgInfos.MyKickVoteCount);
    data << uint8(lfgInfos.BootCount);
    data << Bits<1>(lfgInfos.Aborted);
    data << Bits<1>(lfgInfos.MyFirstReward);
    data.FlushBits();

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyLootSettings const& lootSettings)
{
    data << uint8(lootSettings.Method);
    data << lootSettings.LootMaster;
    data << uint8(lootSettings.Threshold);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PartyDifficultySettings const& difficultySettings)
{
    data << int16(difficultySettings.DungeonDifficultyID);
    data << int16(difficultySettings.RaidDifficultyID);
    data << int16(difficultySettings.LegacyRaidDifficultyID);

    return data;
}

WorldPacket const* PartyUpdate::Write()
{
    _worldPacket << uint16(PartyFlags);
    _worldPacket << uint8(PartyIndex);
    _worldPacket << uint8(PartyType);
    _worldPacket << int32(MyIndex);
    _worldPacket << PartyGUID;
    _worldPacket << uint32(SequenceNum);
    _worldPacket << LeaderGUID;
    _worldPacket << uint8(LeaderFactionGroup);
    _worldPacket << int32(PingRestriction);
    _worldPacket << Size<uint32>(PlayerList);
    _worldPacket << OptionalInit(ChallengeMode);
    _worldPacket << OptionalInit(LfgInfos);
    _worldPacket << OptionalInit(LootSettings);
    _worldPacket << OptionalInit(DifficultySettings);
    _worldPacket.FlushBits();

    for (PartyPlayerInfo const& playerInfos : PlayerList)
        _worldPacket << playerInfos;

    if (LootSettings)
        _worldPacket << *LootSettings;

    if (DifficultySettings)
        _worldPacket << *DifficultySettings;

    if (ChallengeMode)
        _worldPacket << *ChallengeMode;

    if (LfgInfos)
        _worldPacket << *LfgInfos;

    return &_worldPacket;
}

void SetEveryoneIsAssistant::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> Bits<1>(EveryoneIsAssistant);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void ChangeSubGroup::Read()
{
    _worldPacket >> TargetGUID;
    _worldPacket >> NewSubGroup;
    _worldPacket >> OptionalInit(PartyIndex);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void SwapSubGroups::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> FirstTarget;
    _worldPacket >> SecondTarget;
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void ClearRaidMarker::Read()
{
    _worldPacket >> MarkerId;
}

WorldPacket const* RaidMarkersChanged::Write()
{
    _worldPacket << uint8(PartyIndex);
    _worldPacket << uint32(ActiveMarkers);

    _worldPacket << BitsSize<4>(RaidMarkers);
    _worldPacket.FlushBits();

    for (RaidMarker const* raidMarker : RaidMarkers)
    {
        _worldPacket << raidMarker->TransportGUID;
        _worldPacket << raidMarker->Location.GetMapId();
        _worldPacket << raidMarker->Location.PositionXYZStream();
    }

    return &_worldPacket;
}

void PartyMemberFullState::Initialize(Player const* player)
{
    ForEnemy = false;

    MemberGuid = player->GetGUID();

    // Status
    MemberStats.Status = MEMBER_STATUS_ONLINE;

    if (player->IsPvP())
        MemberStats.Status |= MEMBER_STATUS_PVP;

    if (!player->IsAlive())
    {
        if (player->HasPlayerFlag(PLAYER_FLAGS_GHOST))
            MemberStats.Status |= MEMBER_STATUS_GHOST;
        else
            MemberStats.Status |= MEMBER_STATUS_DEAD;
    }

    if (player->IsFFAPvP())
        MemberStats.Status |= MEMBER_STATUS_PVP_FFA;

    if (player->isAFK())
        MemberStats.Status |= MEMBER_STATUS_AFK;

    if (player->isDND())
        MemberStats.Status |= MEMBER_STATUS_DND;

    if (player->GetVehicle())
        MemberStats.Status |= MEMBER_STATUS_VEHICLE;

    // Level
    MemberStats.Level = player->GetLevel();

    // Health
    MemberStats.CurrentHealth = player->GetHealth();
    MemberStats.MaxHealth = player->GetMaxHealth();

    // Power
    MemberStats.PowerType = player->GetPowerType();
    MemberStats.PowerDisplayID = 0;
    MemberStats.CurrentPower = player->GetPower(player->GetPowerType());
    MemberStats.MaxPower = player->GetMaxPower(player->GetPowerType());

    // Position
    MemberStats.ZoneID = player->GetZoneId();
    MemberStats.PositionX = int16(player->GetPositionX());
    MemberStats.PositionY = int16(player->GetPositionY());
    MemberStats.PositionZ = int16(player->GetPositionZ());

    MemberStats.SpecID = AsUnderlyingType(player->GetPrimarySpecialization());
    MemberStats.PartyType[0] = player->m_playerData->PartyType[0];
    MemberStats.PartyType[1] = player->m_playerData->PartyType[1];

    if (WmoLocation const* wmoLocation = player->GetCurrentWmo())
    {
        MemberStats.WmoGroupID = wmoLocation->GroupId;
        MemberStats.WmoDoodadPlacementID = wmoLocation->UniqueId;
    }

    // Vehicle
    if (::Vehicle const* vehicle = player->GetVehicle())
        if (VehicleSeatEntry const* vehicleSeat = vehicle->GetSeatForPassenger(player))
            MemberStats.VehicleSeat = vehicleSeat->ID;

    // Auras
    for (AuraApplication const* aurApp : player->GetVisibleAuras())
    {
        PartyMemberAuraStates& aura = MemberStats.Auras.emplace_back();

        aura.SpellID = aurApp->GetBase()->GetId();
        aura.ActiveFlags = aurApp->GetEffectMask();
        aura.Flags = aurApp->GetFlags();

        if (aurApp->GetFlags() & AFLAG_SCALABLE)
            for (AuraEffect const* aurEff : aurApp->GetBase()->GetAuraEffects())
                if (aurApp->HasEffect(aurEff->GetEffIndex()))
                    aura.Points.push_back(float(aurEff->GetAmount()));
    }

    // Phases
    PhasingHandler::FillPartyMemberPhase(&MemberStats.Phases, player->GetPhaseShift());

    // Pet
    if (::Pet* pet = player->GetPet())
    {
        MemberStats.PetStats.emplace();

        MemberStats.PetStats->GUID = pet->GetGUID();
        MemberStats.PetStats->Name = pet->GetName();
        MemberStats.PetStats->ModelId = pet->GetDisplayId();

        MemberStats.PetStats->CurrentHealth = pet->GetHealth();
        MemberStats.PetStats->MaxHealth = pet->GetMaxHealth();

        for (AuraApplication const* aurApp : pet->GetVisibleAuras())
        {
            PartyMemberAuraStates& aura = MemberStats.PetStats->Auras.emplace_back();

            aura.SpellID = aurApp->GetBase()->GetId();
            aura.ActiveFlags = aurApp->GetEffectMask();
            aura.Flags = aurApp->GetFlags();

            if (aurApp->GetFlags() & AFLAG_SCALABLE)
                for (AuraEffect const* aurEff : aurApp->GetBase()->GetAuraEffects())
                    if (aurApp->HasEffect(aurEff->GetEffIndex()))
                        aura.Points.push_back(float(aurEff->GetAmount()));
        }
    }

    MemberStats.ChromieTime.ConditionalFlags = player->m_playerData->CtrOptions->ConditionalFlags;
    MemberStats.ChromieTime.FactionGroup = player->m_playerData->CtrOptions->FactionGroup;
    MemberStats.ChromieTime.ChromieTimeExpansionMask = player->m_playerData->CtrOptions->ChromieTimeExpansionMask;
}

WorldPacket const* PartyKillLog::Write()
{
    _worldPacket << Player;
    _worldPacket << Victim;

    return &_worldPacket;
}

WorldPacket const* BroadcastSummonCast::Write()
{
    _worldPacket << Target;

    return &_worldPacket;
}

WorldPacket const* BroadcastSummonResponse::Write()
{
    _worldPacket << Target;
    _worldPacket << Bits<1>(Accepted);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void SetRestrictPingsToAssistants::Read()
{
    _worldPacket >> OptionalInit(PartyIndex);
    _worldPacket >> As<int32>(RestrictTo);
    if (PartyIndex)
        _worldPacket >> *PartyIndex;
}

void SendPingUnit::Read()
{
    _worldPacket >> SenderGUID;
    _worldPacket >> TargetGUID;
    _worldPacket >> As<uint8>(Type);
    _worldPacket >> PinFrameID;
    _worldPacket >> PingDuration;
    _worldPacket >> OptionalInit(CreatureID);
    _worldPacket >> OptionalInit(SpellOverrideNameID);
    if (CreatureID)
        _worldPacket >> *CreatureID;

    if (SpellOverrideNameID)
        _worldPacket >> *SpellOverrideNameID;
}

WorldPacket const* ReceivePingUnit::Write()
{
    _worldPacket << SenderGUID;
    _worldPacket << TargetGUID;
    _worldPacket << uint8(Type);
    _worldPacket << uint32(PinFrameID);
    _worldPacket << PingDuration;
    _worldPacket << OptionalInit(CreatureID);
    _worldPacket << OptionalInit(SpellOverrideNameID);
    _worldPacket.FlushBits();

    if (CreatureID)
        _worldPacket << uint32(*CreatureID);

    if (SpellOverrideNameID)
        _worldPacket << uint32(*SpellOverrideNameID);

    return &_worldPacket;
}

void SendPingWorldPoint::Read()
{
    _worldPacket >> SenderGUID;
    _worldPacket >> MapID;
    _worldPacket >> Point;
    _worldPacket >> As<int8>(Type);
    _worldPacket >> PinFrameID;
    _worldPacket >> Transport;
    _worldPacket >> PingDuration;
}

WorldPacket const* ReceivePingWorldPoint::Write()
{
    _worldPacket << SenderGUID;
    _worldPacket << MapID;
    _worldPacket << Point;
    _worldPacket << uint8(Type);
    _worldPacket << uint32(PinFrameID);
    _worldPacket << Transport;
    _worldPacket << PingDuration;

    return &_worldPacket;
}

WorldPacket const* CancelPingPin::Write()
{
    _worldPacket << SenderGUID;
    _worldPacket << PinFrameID;

    return &_worldPacket;
}
}

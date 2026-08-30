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

#include "WorldSession.h"
#include "AuthenticationPackets.h"
#include "Battleground.h"
#include "Corpse.h"
#include "DB2Stores.h"
#include "FlightPathMovementGenerator.h"
#include "GameTime.h"
#include "Garrison.h"
#include "InstanceLockMgr.h"
#include "InstancePackets.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "MovementGenerator.h"
#include "MovementPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RBAC.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Transport.h"
#include "Vehicle.h"
#include <algorithm>
#include <cmath>
#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/framework/features.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/circular_buffer.hpp>

bool WorldSession::ValidateMovementInfo(Unit const* mover, MovementInfo* mi) const
{
    //! Anti-cheat checks. Please keep them in seperate if () blocks to maintain a clear overview.
    //! Might be subject to latency, so just remove improper flags.
    #ifdef TRINITY_DEBUG
    #define REMOVE_VIOLATING_FLAGS(check, maskToRemove) do \
    { \
        if (check) \
        { \
            TC_LOG_DEBUG("entities.unit", "Player::ValidateMovementInfo: Violation of MovementFlags found ({}). " \
                "MovementFlags: {} for player {}. Mask {} will be removed.", \
                STRINGIZE(check), mi->GetMovementFlags(), GetPlayer()->GetGUID(), maskToRemove); \
            mi->RemoveMovementFlag((maskToRemove)); \
        } \
    } while (0)
    #else
    #define REMOVE_VIOLATING_FLAGS(check, maskToRemove) do \
    { \
        if (check) \
            mi->RemoveMovementFlag((maskToRemove)); \
    } while (0)
    #endif

    if (!mover)
        return false;

    if (!mi->pos.IsPositionValid())
        return false;

    if (!GetPlayer()->GetVehicleBase() || !(GetPlayer()->GetVehicle()->GetVehicleInfo()->Flags & VEHICLE_FLAG_FIXED_POSITION))
        REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_ROOT), MOVEMENTFLAG_ROOT);

    /*! This must be a packet spoofing attempt. MOVEMENTFLAG_ROOT sent from the client is not valid
        in conjunction with any of the moving movement flags such as MOVEMENTFLAG_FORWARD.
        It will freeze clients that receive this player's movement info.
    */
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_ROOT) && mi->HasMovementFlag(MOVEMENTFLAG_MASK_MOVING),
        MOVEMENTFLAG_MASK_MOVING);

    //! Cannot hover without SPELL_AURA_HOVER
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_HOVER) && !mover->HasAuraType(SPELL_AURA_HOVER),
        MOVEMENTFLAG_HOVER);

    //! Cannot ascend and descend at the same time
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_ASCENDING) && mi->HasMovementFlag(MOVEMENTFLAG_DESCENDING),
        MOVEMENTFLAG_ASCENDING | MOVEMENTFLAG_DESCENDING);

    //! Cannot move left and right at the same time
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_LEFT) && mi->HasMovementFlag(MOVEMENTFLAG_RIGHT),
        MOVEMENTFLAG_LEFT | MOVEMENTFLAG_RIGHT);

    //! Cannot strafe left and right at the same time
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_STRAFE_LEFT) && mi->HasMovementFlag(MOVEMENTFLAG_STRAFE_RIGHT),
        MOVEMENTFLAG_STRAFE_LEFT | MOVEMENTFLAG_STRAFE_RIGHT);

    //! Cannot pitch up and down at the same time
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_PITCH_UP) && mi->HasMovementFlag(MOVEMENTFLAG_PITCH_DOWN),
        MOVEMENTFLAG_PITCH_UP | MOVEMENTFLAG_PITCH_DOWN);

    //! Cannot move forwards and backwards at the same time
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_FORWARD) && mi->HasMovementFlag(MOVEMENTFLAG_BACKWARD),
        MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD);

    //! Cannot walk on water without SPELL_AURA_WATER_WALK except for ghosts
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_WATERWALKING) &&
        !mover->HasAuraType(SPELL_AURA_WATER_WALK) &&
        !mover->HasAuraType(SPELL_AURA_GHOST),
        MOVEMENTFLAG_WATERWALKING);

    //! Cannot feather fall without SPELL_AURA_FEATHER_FALL
    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_FALLING_SLOW) && !mover->HasAuraType(SPELL_AURA_FEATHER_FALL),
        MOVEMENTFLAG_FALLING_SLOW);

    /*! Cannot fly if no fly auras present. Exception is being a GM.
        Note that we check for account level instead of Player::IsGameMaster() because in some
        situations it may be feasable to use .gm fly on as a GM without having .gm on,
        e.g. aerial combat.
    */

    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_FLYING | MOVEMENTFLAG_CAN_FLY) && GetSecurity() == SEC_PLAYER &&
        !mover->HasAuraType(SPELL_AURA_FLY) &&
        !mover->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) &&
        !mover->HasAuraType(SPELL_AURA_ADV_FLYING),
        MOVEMENTFLAG_FLYING | MOVEMENTFLAG_CAN_FLY);

    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_CAN_FLY) && mi->HasMovementFlag(MOVEMENTFLAG_FALLING),
        MOVEMENTFLAG_FALLING);

    REMOVE_VIOLATING_FLAGS(mi->HasMovementFlag(MOVEMENTFLAG_SPLINE_ELEVATION) && G3D::fuzzyEq(mi->stepUpStartElevation, 0.0f), MOVEMENTFLAG_SPLINE_ELEVATION);

    // Client first checks if spline elevation != 0, then verifies flag presence
    if (G3D::fuzzyNe(mi->stepUpStartElevation, 0.0f))
        mi->AddMovementFlag(MOVEMENTFLAG_SPLINE_ELEVATION);

    //! The gravity modifier is server-owned - it is only ever changed by SPELL_AURA_MOD_GRAVITY via
    //! Unit::UpdateGravityModifier. Until this handler existed the client value was taken over
    //! unchecked, which let anyone pick their own gravity. Overwrite it with what we sent.
    mi->gravityModifier = mover->m_movementInfo.gravityModifier;

    //! removeForcesIDs is a one-shot request, not a piece of movement state. Every caller that
    //! assigns to Unit::m_movementInfo passes through here first, and from there the list would be
    //! written into every SMSG_MOVE_UPDATE (MovementPackets.cpp:85) and every SMSG_UPDATE_OBJECT
    //! (BaseEntity.cpp:290) of that mover, for the rest of the session - a client could hang a list
    //! of AreaTrigger guids of its choosing onto any CMSG_MOVE_* and have the server tell everyone
    //! around it to drop those forces. Only HandleMoveRemoveMovementForces reads the list, and it
    //! takes it out of the packet before it gets here.
    mi->removeForcesIDs.clear();

    #undef REMOVE_VIOLATING_FLAGS

    return true;
}

Unit* WorldSession::ValidateAndGetUnitBeingMoved(ObjectGuid guid, OpcodeClient opcode, bool forStatusAck) const
{
    // the client is attempting to tamper movement data
    // edit: this wouldn't happen in retail but it does in TC, even with a legitimate client.
    Unit* activelyMovedUnit = _player->GetUnitBeingMoved();
    if (!forStatusAck && (!activelyMovedUnit || activelyMovedUnit->GetGUID() != guid))
    {
        TC_LOG_DEBUG("entities.unit", "{} Attempted tampering movement data in {}, requesting not allowed mover {} but expected {}",
            GetPlayerInfo(), GetOpcodeNameForLogging(opcode), guid, Object::GetGUID(activelyMovedUnit));
        return nullptr;
    }

    if (activelyMovedUnit && activelyMovedUnit->GetGUID() == guid)
        return activelyMovedUnit;

    if (_player->GetGUID() == guid)
        return _player;

    return ObjectAccessor::GetUnit(*_player, guid);
}

uint32 WorldSession::AdjustClientMovementTime(uint32 time) const
{
    int64 movementTime = int64(time) + _timeSyncClockDelta;
    if (_timeSyncClockDelta == 0 || movementTime < 0 || movementTime > SI64LIT(0xFFFFFFFF))
    {
        TC_LOG_WARN("misc", "The computed movement time using clockDelta is erronous. Using fallback instead");
        return GameTime::GetGameTimeMS();
    }
    else
        return uint32(movementTime);
}

void WorldSession::HandleMoveWorldportAckOpcode(WorldPackets::Movement::WorldPortResponse& /*packet*/)
{
    if (_player->GetTeleportState() != TeleportState::WaitingForWorldPortAck)
        return;

    HandleMoveWorldportAck();
}

void WorldSession::HandleMoveWorldportAck()
{
    Player* player = GetPlayer();
    // ignore unexpected far teleports
    bool seamlessTeleport = player->GetTeleportOptions().HasFlag(TELE_TO_SEAMLESS);
    player->SetTeleportState(TeleportState::NotTeleporting);

    // get the teleport destination
    TeleportLocation const& loc = player->GetTeleportDest();

    // possible errors in the coordinate validity check
    if (!MapManager::IsValidMapCoord(loc.Location))
    {
        LogoutPlayer(false);
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.Location.GetMapId());

    // reset instance validity, except if going to an instance inside an instance
    if (player->m_InstanceValid == false && !mEntry->IsDungeon())
        player->m_InstanceValid = true;

    Map* oldMap = player->GetMap();
    Map* newMap = loc.InstanceId ?
        sMapMgr->FindMap(loc.Location.GetMapId(), *loc.InstanceId) :
        sMapMgr->CreateMap(loc.Location.GetMapId(), GetPlayer(), loc.LfgDungeonsId);

    if (TransportBase* transport = player->GetTransport())
        transport->RemovePassenger(player);

    if (player->IsInWorld())
    {
        TC_LOG_ERROR("network", "{} {} is still in world when teleported from map {} ({}) to new map {} ({})", player->GetGUID().ToString(), player->GetName(), oldMap->GetMapName(), oldMap->GetId(), newMap ? newMap->GetMapName() : "Unknown", loc.Location.GetMapId());
        oldMap->RemovePlayerFromMap(player, false);
    }

    // relocate the player to the teleport destination
    // the CannotEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!newMap || newMap->CannotEnter(player))
    {
        TC_LOG_ERROR("network", "Map {} ({}) could not be created for player {} ({}), porting player to homebind", loc.Location.GetMapId(), newMap ? newMap->GetMapName() : "Unknown", player->GetGUID().ToString(), player->GetName());
        player->TeleportTo(player->m_homebind);
        return;
    }

    float z = loc.Location.GetPositionZ() + player->GetHoverOffset();
    player->Relocate(loc.Location.GetPositionX(), loc.Location.GetPositionY(), z, loc.Location.GetOrientation());
    player->SetFallInformation(0, player->GetPositionZ());

    player->ResetMap();
    player->SetMap(newMap);
    player->UpdatePositionData();

    WorldPackets::Movement::ResumeToken resumeToken;
    resumeToken.SequenceIndex = player->m_movementCounter;
    resumeToken.Reason = seamlessTeleport ? 2 : 1;
    SendPacket(resumeToken.Write());

    if (!seamlessTeleport)
        player->SendInitialPacketsBeforeAddToMap();

    if (player->m_teleport_dest.TransportGuid)
    {
        if (GameObject* go = newMap->GetTransport(*player->m_teleport_dest.TransportGuid))
        {
            if (TransportBase* newTransport = go->ToTransportBase())
            {
                newTransport->AddPassenger(player, loc.Location);
                player->Relocate(newTransport->GetPositionWithOffset(loc.Location));
            }
        }
    }
    else if (TransportBase* transport = player->GetTransport())
        transport->RemovePassenger(player);

    if (!player->GetMap()->AddPlayerToMap(player, !seamlessTeleport))
    {
        TC_LOG_ERROR("network", "WORLD: failed to teleport player {} {} to map {} ({}) because of unknown reason!",
            player->GetName(), player->GetGUID().ToString(), loc.Location.GetMapId(), newMap ? newMap->GetMapName() : "Unknown");
        player->ResetMap();
        player->SetMap(oldMap);
        player->TeleportTo(player->m_homebind);
        return;
    }

    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if (player->InBattleground())
    {
        // cleanup setting if outdated
        if (!mEntry->IsBattlegroundOrArena())
        {
            // We're not in BG
            player->SetBattlegroundId(0, BATTLEGROUND_TYPE_NONE, BATTLEGROUND_QUEUE_NONE);
            // reset destination bg team
            player->SetBGTeam(TEAM_OTHER);
        }
        // join to bg case
        else if (Battleground* bg = player->GetBattleground())
        {
            if (player->IsInvitedForBattlegroundInstance(player->GetBattlegroundId()))
                bg->AddPlayer(player, player->m_bgData.queueId);
        }
    }

    if (!seamlessTeleport)
        player->SendInitialPacketsAfterAddToMap();
    else
    {
        player->UpdateVisibilityForPlayer();
        if (Garrison* garrison = player->GetGarrison())
            garrison->SendRemoteInfo();
    }

    // flight fast teleport case
    if (player->IsInFlight())
    {
        if (!player->InBattleground())
        {
            if (!seamlessTeleport)
            {
                // short preparations to continue flight
                MovementGenerator* movementGenerator = player->GetMotionMaster()->GetCurrentMovementGenerator();
                movementGenerator->Initialize(player);
            }
            return;
        }

        // battleground state prepare, stop flight
        player->FinishTaxiFlight();
    }

    if (!player->IsAlive() && player->GetTeleportOptions().HasFlag(TELE_REVIVE_AT_TELEPORT))
        player->ResurrectPlayer(0.5f);

    // resurrect character at enter into instance where his corpse exist after add to map
    if (mEntry->IsDungeon() && !player->IsAlive())
    {
        if (player->GetCorpseLocation().GetMapId() == mEntry->ID)
        {
            player->ResurrectPlayer(0.5f);
            player->SpawnCorpseBones();
        }
    }

    if (mEntry->IsDungeon())
    {
        // check if this instance has a reset time and send it to player if so
        MapDb2Entries entries{ mEntry->ID, newMap->GetDifficultyID() };
        if (entries.MapDifficulty->HasResetSchedule())
        {
            WorldPackets::Instance::RaidInstanceMessage raidInstanceMessage;
            raidInstanceMessage.Type = RAID_INSTANCE_WELCOME;
            raidInstanceMessage.MapID = mEntry->ID;
            raidInstanceMessage.DifficultyID = newMap->GetDifficultyID();
            if (InstanceLock const* playerLock = sInstanceLockMgr.FindActiveInstanceLock(GetPlayer()->GetGUID(), entries))
            {
                raidInstanceMessage.Locked = !playerLock->IsExpired();
                raidInstanceMessage.Extended = playerLock->IsExtended();
            }
            else
            {
                raidInstanceMessage.Locked = false;
                raidInstanceMessage.Extended = false;
            }
            SendPacket(raidInstanceMessage.Write());
        }

        // check if instance is valid
        if (!player->CheckInstanceValidity(false))
            player->m_InstanceValid = false;
    }

    // update zone immediately, otherwise leave channel will cause crash in mtmap
    uint32 newzone, newarea;
    player->GetZoneAndAreaId(newzone, newarea);
    player->UpdateZone(newzone, newarea);

    // honorless target
    if (player->pvpInfo.IsHostile)
        player->CastSpell(player, 2479, true);

    // in friendly area
    else if (player->IsPvP() && !player->HasPlayerFlag(PLAYER_FLAGS_IN_PVP))
        player->UpdatePvP(false, false);

    // resummon pet
    player->ResummonPetTemporaryUnSummonedIfAny();
    player->ResummonBattlePetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    player->ProcessDelayedOperations();
}

void WorldSession::HandleSuspendTokenResponse(WorldPackets::Movement::SuspendTokenResponse& /*suspendTokenResponse*/)
{
    if (_player->GetTeleportState() != TeleportState::WaitingForSuspendTokenResponse)
        return;

    TeleportLocation const& loc = GetPlayer()->GetTeleportDest();

    if (sMapStore.AssertEntry(loc.Location.GetMapId())->IsDungeon())
    {
        WorldPackets::Instance::UpdateLastInstance updateLastInstance;
        updateLastInstance.MapID = loc.Location.GetMapId();
        SendPacket(updateLastInstance.Write());
    }

    WorldPackets::Movement::NewWorld packet;
    packet.MapID = loc.Location.GetMapId();
    packet.Loc.Pos = loc.Location;
    packet.Reason = !_player->GetTeleportOptions().HasFlag(TELE_TO_SEAMLESS) ? NEW_WORLD_NORMAL : NEW_WORLD_SEAMLESS;
    packet.Counter = _player->GetNewWorldCounter();
    SendPacket(packet.Write());

    _player->SetTeleportState(TeleportState::WaitingForWorldPortAck);

    if (_player->GetTeleportOptions().HasFlag(TELE_TO_SEAMLESS))
        HandleMoveWorldportAck();
}

void WorldSession::HandleMoveTeleportAck(WorldPackets::Movement::MoveTeleportAck& packet)
{
    TC_LOG_DEBUG("network", "CMSG_MOVE_TELEPORT_ACK: Guid: {}, Sequence: {}, Time: {}", packet.MoverGUID.ToString(), packet.AckIndex, packet.MoveTime);

    Unit* mover = ValidateAndGetUnitBeingMoved(packet.MoverGUID, packet.GetOpcode(), false);
    if (!mover)
        return;

    Player* plMover = mover->ToPlayer();

    if (!plMover || plMover->GetTeleportState() != TeleportState::WaitingForTeleportAck)
        return;

    plMover->SetTeleportState(TeleportState::NotTeleporting);

    uint32 old_zone = plMover->GetZoneId();

    TeleportLocation const& dest = plMover->GetTeleportDest();
    WorldLocation destLocation = dest.Location;

    if (dest.TransportGuid)
    {
        if (GameObject* go = plMover->GetMap()->GetGameObject(*dest.TransportGuid))
        {
            if (TransportBase* transport = go->ToTransportBase())
            {
                transport->AddPassenger(plMover, destLocation);
                destLocation.Relocate(transport->GetPositionWithOffset(plMover->m_movementInfo.transport.pos));
            }
        }
    }

    plMover->UpdatePosition(destLocation, true);
    plMover->SetFallInformation(0, GetPlayer()->GetPositionZ());

    uint32 newzone, newarea;
    plMover->GetZoneAndAreaId(newzone, newarea);
    plMover->UpdateZone(newzone, newarea);

    // new zone
    if (old_zone != newzone)
    {
        // honorless target
        if (plMover->pvpInfo.IsHostile)
            plMover->CastSpell(plMover, 2479, true);

        // in friendly area
        else if (plMover->IsPvP() && !plMover->HasPlayerFlag(PLAYER_FLAGS_IN_PVP))
            plMover->UpdatePvP(false, false);
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMovementOpcodes(WorldPackets::Movement::ClientPlayerMovement& packet)
{
    HandleMovementOpcode(packet.GetOpcode(), packet.Status);
}

// Returns true when the reported movement was accepted, i.e. when mover->m_movementInfo was
// replaced by it. Every early exit below leaves the server position untouched; a caller that draws
// conclusions from where the mover now stands must not do so in that case.
bool WorldSession::HandleMovementOpcode(OpcodeClient opcode, MovementInfo& movementInfo)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(movementInfo.guid, opcode, false);
    if (!ValidateMovementInfo(mover, &movementInfo))
        return false;

    Player* plrMover = mover->ToPlayer();

    TC_LOG_TRACE("opcodes.movement", "HandleMovementOpcode Name {}: opcode {} {} Flags {} Pos {}",
        mover->GetName(), opcode, GetOpcodeNameForLogging(opcode),
        movementInfo.flags, movementInfo.pos);

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if (plrMover && plrMover->IsBeingTeleported())
        return false;

    if (!mover->movespline->Finalized())
        return false;

    // stop some emotes at player move
    if (plrMover && (plrMover->GetEmoteState() != 0))
        plrMover->SetEmoteState(EMOTE_ONESHOT_NONE);

    /* handle special cases */
    if (!movementInfo.transport.guid.IsEmpty())
    {
        // We were teleported, skip packets that were broadcast before teleport
        if (movementInfo.pos.GetExactDist2d(mover) > SIZE_OF_GRIDS)
            return false;

        // transports size limited
        // (also received at zeppelin leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if (fabs(movementInfo.transport.pos.GetPositionX()) > 75.0f || fabs(movementInfo.transport.pos.GetPositionY()) > 75.0f || fabs(movementInfo.transport.pos.GetPositionZ()) > 75.0f)
            return false;

        if (!Trinity::IsValidMapCoord(movementInfo.pos.GetPositionX() + movementInfo.transport.pos.GetPositionX(), movementInfo.pos.GetPositionY() + movementInfo.transport.pos.GetPositionY(),
            movementInfo.pos.GetPositionZ() + movementInfo.transport.pos.GetPositionZ(), movementInfo.pos.GetOrientation() + movementInfo.transport.pos.GetOrientation()))
            return false;

        // if we boarded a transport, add us to it
        if (plrMover)
        {
            if (!plrMover->GetTransport())
            {
                if (GameObject* go = plrMover->GetMap()->GetGameObject(movementInfo.transport.guid))
                    if (TransportBase* transport = go->ToTransportBase())
                        transport->AddPassenger(plrMover, movementInfo.transport.pos);
            }
            else if (plrMover->GetTransport()->GetTransportGUID() != movementInfo.transport.guid)
            {
                plrMover->GetTransport()->RemovePassenger(plrMover);
                if (GameObject* go = plrMover->GetMap()->GetGameObject(movementInfo.transport.guid))
                {
                    if (TransportBase* transport = go->ToTransportBase())
                        transport->AddPassenger(plrMover, movementInfo.transport.pos);
                    else
                        movementInfo.ResetTransport();
                }
                else
                    movementInfo.ResetTransport();
            }
        }

        if (!mover->GetTransport() && !mover->GetVehicle())
            movementInfo.transport.Reset();
    }
    else if (plrMover && plrMover->GetTransport())                // if we were on a transport, leave
        plrMover->GetTransport()->RemovePassenger(plrMover);

    /* process position-change */
    movementInfo.guid = mover->GetGUID();
    movementInfo.time = AdjustClientMovementTime(movementInfo.time);
    mover->m_movementInfo = movementInfo;

    // Some vehicles allow the passenger to turn by himself
    if (Vehicle* vehicle = mover->GetVehicle())
    {
        if (VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(mover))
        {
            if (seat->Flags & VEHICLE_SEAT_FLAG_ALLOW_TURNING)
            {
                if (movementInfo.pos.GetOrientation() != mover->GetOrientation())
                {
                    mover->SetOrientation(movementInfo.pos.GetOrientation());
                    mover->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::Turning);
                }
            }
        }
        return true;
    }

    mover->UpdatePosition(movementInfo.pos);

    WorldPackets::Movement::MoveUpdate moveUpdate;
    moveUpdate.Status = &mover->m_movementInfo;
    mover->SendMessageToSet(moveUpdate.Write(), _player);

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == CMSG_MOVE_FALL_LAND && plrMover && !plrMover->IsInFlight())
        plrMover->HandleFall();

    // interrupt parachutes upon falling or landing in water
    if (opcode == CMSG_MOVE_FALL_LAND || opcode == CMSG_MOVE_START_SWIM)
        mover->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::LandingOrFlight); // Parachutes

    if (opcode == CMSG_MOVE_SET_FLY || opcode == CMSG_MOVE_SET_ADV_FLY)
    {
        _player->UnsummonPetTemporaryIfAny(); // always do the pet removal on current client activeplayer only
        _player->UnsummonBattlePetTemporaryIfAny(true);
    }

    if (plrMover)                                            // nothing is charmed, or player charmed
    {
        if (plrMover->IsSitState() && (movementInfo.flags & (MOVEMENTFLAG_MASK_MOVING | MOVEMENTFLAG_MASK_TURNING)))
            plrMover->SetStandState(UNIT_STAND_STATE_STAND);

        plrMover->UpdateFallInformationIfNeed(movementInfo, opcode);

        if (movementInfo.pos.GetPositionZ() < plrMover->GetMap()->GetMinHeight(plrMover->GetPhaseShift(), movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY()))
        {
            if (!(plrMover->GetBattleground() && plrMover->GetBattleground()->HandlePlayerUnderMap(_player)))
            {
                // NOTE: this is actually called many times while falling
                // even after the player has been teleported away
                /// @todo discard movement packets after the player is rooted
                if (plrMover->IsAlive())
                {
                    TC_LOG_DEBUG("entities.player.falldamage", "FALLDAMAGE Below map. Map min height: {} , Player debug info:\n{}", plrMover->GetMap()->GetMinHeight(plrMover->GetPhaseShift(), movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY()), plrMover->GetDebugInfo());
                    plrMover->SetPlayerFlag(PLAYER_FLAGS_IS_OUT_OF_BOUNDS);
                    plrMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, GetPlayer()->GetMaxHealth());
                    // player can be alive if GM/etc
                    // change the death state to CORPSE to prevent the death timer from
                    // starting in the next player update
                    if (plrMover->IsAlive())
                        plrMover->KillPlayer();
                }
            }
        }
        else
            plrMover->RemovePlayerFlag(PLAYER_FLAGS_IS_OUT_OF_BOUNDS);

        if (opcode == CMSG_MOVE_JUMP)
        {
            plrMover->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags2::Jump);
            Unit::ProcSkillsAndAuras(plrMover, nullptr, PROC_FLAG_JUMP, PROC_FLAG_NONE, PROC_SPELL_TYPE_MASK_ALL, PROC_SPELL_PHASE_NONE, PROC_HIT_NONE, nullptr, nullptr, nullptr);
        }

        // Whenever a player stops a movement action, several position based checks and updates are being performed
        switch (opcode)
        {
            case CMSG_MOVE_SET_FLY:
            case CMSG_MOVE_FALL_LAND:
            case CMSG_MOVE_STOP:
            case CMSG_MOVE_STOP_STRAFE:
            case CMSG_MOVE_STOP_TURN:
            case CMSG_MOVE_STOP_SWIM:
            case CMSG_MOVE_STOP_PITCH:
            case CMSG_MOVE_STOP_ASCEND:
                plrMover->UpdateZoneAndAreaId();
                plrMover->UpdateIndoorsOutdoorsAuras();
                plrMover->UpdateTavernRestingState();
                break;
            default:
                break;
        }
    }

    return true;
}

void WorldSession::HandleForceSpeedChangeAck(WorldPackets::Movement::MovementSpeedAck& packet)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(packet.Ack.Status.guid, packet.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &packet.Ack.Status))
        return;

    /*----------------*/

    // client ACK send one packet for mounted/run case and need skip all except last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;

    static char const* const move_type_name[MAX_MOVE_TYPE] =
    {
        "Walk",
        "Run",
        "RunBack",
        "Swim",
        "SwimBack",
        "TurnRate",
        "Flight",
        "FlightBack",
        "PitchRate"
    };

    OpcodeClient opcode = packet.GetOpcode();
    switch (opcode)
    {

        case CMSG_MOVE_FORCE_WALK_SPEED_CHANGE_ACK:        move_type = MOVE_WALK;        break;
        case CMSG_MOVE_FORCE_RUN_SPEED_CHANGE_ACK:         move_type = MOVE_RUN;         break;
        case CMSG_MOVE_FORCE_RUN_BACK_SPEED_CHANGE_ACK:    move_type = MOVE_RUN_BACK;    break;
        case CMSG_MOVE_FORCE_SWIM_SPEED_CHANGE_ACK:        move_type = MOVE_SWIM;        break;
        case CMSG_MOVE_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_SWIM_BACK;   break;
        case CMSG_MOVE_FORCE_TURN_RATE_CHANGE_ACK:         move_type = MOVE_TURN_RATE;   break;
        case CMSG_MOVE_FORCE_FLIGHT_SPEED_CHANGE_ACK:      move_type = MOVE_FLIGHT;      break;
        case CMSG_MOVE_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK: move_type = MOVE_FLIGHT_BACK; break;
        case CMSG_MOVE_FORCE_PITCH_RATE_CHANGE_ACK:        move_type = MOVE_PITCH_RATE;  break;

        default:
            TC_LOG_ERROR("network", "WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: {}", opcode);
            return;
    }

    // skip all forced speed changes except last and unexpected
    // in run/mounted case used one ACK and it must be skipped. m_forced_speed_changes[MOVE_RUN] store both.
    if (_player->m_forced_speed_changes[move_type] > 0)
    {
        --_player->m_forced_speed_changes[move_type];
        if (_player->m_forced_speed_changes[move_type] > 0)
            return;
    }

    if (!_player->GetTransport() && std::fabs(_player->GetSpeed(move_type) - packet.Speed) > 0.01f)
    {
        if (_player->GetSpeed(move_type) > packet.Speed)         // must be greater - just correct
        {
            TC_LOG_ERROR("network", "{}SpeedChange player {} is NOT correct (must be {} instead {}), force set to correct value",
                move_type_name[move_type], _player->GetName(), _player->GetSpeed(move_type), packet.Speed);
            _player->SetSpeedRate(move_type, _player->GetSpeedRate(move_type));
        }
        else                                                // must be lesser - cheating
        {
            TC_LOG_DEBUG("misc", "Player {} from account id {} kicked for incorrect speed (must be {} instead {})",
                _player->GetName(), _player->GetSession()->GetAccountId(), _player->GetSpeed(move_type), packet.Speed);
            _player->GetSession()->KickPlayer("WorldSession::HandleForceSpeedChangeAck Incorrect speed");
        }
    }
}

void WorldSession::HandleSetAdvFlyingSpeedAck(WorldPackets::Movement::MovementSpeedAck& speedAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(speedAck.Ack.Status.guid, speedAck.GetOpcode(), true);
    if (!mover)
        return;

    ValidateMovementInfo(mover, &speedAck.Ack.Status);
}

void WorldSession::HandleSetAdvFlyingSpeedRangeAck(WorldPackets::Movement::MovementSpeedRangeAck& speedRangeAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(speedRangeAck.Ack.Status.guid, speedRangeAck.GetOpcode(), true);
    if (!mover)
        return;

    ValidateMovementInfo(mover, &speedRangeAck.Ack.Status);
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPackets::Movement::SetActiveMover& packet)
{
    if (GetPlayer()->IsInWorld())
        if (_player->GetUnitBeingMoved()->GetGUID() != packet.ActiveMover)
            TC_LOG_DEBUG("network", "HandleSetActiveMoverOpcode: incorrect mover guid: mover is {} and should be {}" , packet.ActiveMover.ToString(), _player->GetUnitBeingMoved()->GetGUID().ToString());
}

void WorldSession::HandleMoveKnockBackAck(WorldPackets::Movement::MoveKnockBackAck& movementAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(movementAck.Ack.Status.guid, movementAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &movementAck.Ack.Status))
        return;

    movementAck.Ack.Status.time = AdjustClientMovementTime(movementAck.Ack.Status.time);
    mover->m_movementInfo = movementAck.Ack.Status;

    WorldPackets::Movement::MoveUpdateKnockBack updateKnockBack;
    updateKnockBack.Status = &mover->m_movementInfo;
    mover->SendMessageToSet(updateKnockBack.Write(), false);
}

void WorldSession::HandleMovementAckMessage(WorldPackets::Movement::MovementAckMessage& movementAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(movementAck.Ack.Status.guid, movementAck.GetOpcode(), true);
    if (!mover)
        return;

    ValidateMovementInfo(mover, &movementAck.Ack.Status);
}

void WorldSession::HandleSummonResponseOpcode(WorldPackets::Movement::SummonResponse& packet)
{
    if (!_player->IsAlive() || _player->IsInCombat())
        return;

    _player->SummonIfPossible(packet.Accept);
}

void WorldSession::HandleSetCollisionHeightAck(WorldPackets::Movement::MoveSetCollisionHeightAck& setCollisionHeightAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(setCollisionHeightAck.Data.Status.guid, setCollisionHeightAck.GetOpcode(), true);
    if (!mover)
        return;

    ValidateMovementInfo(mover, &setCollisionHeightAck.Data.Status);
}

void WorldSession::HandleMoveApplyMovementForceAck(WorldPackets::Movement::MoveApplyMovementForceAck& moveApplyMovementForceAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(moveApplyMovementForceAck.Ack.Status.guid, moveApplyMovementForceAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &moveApplyMovementForceAck.Ack.Status))
        return;

    moveApplyMovementForceAck.Ack.Status.time = AdjustClientMovementTime(moveApplyMovementForceAck.Ack.Status.time);

    WorldPackets::Movement::MoveUpdateApplyMovementForce updateApplyMovementForce;
    updateApplyMovementForce.Status = &moveApplyMovementForceAck.Ack.Status;
    updateApplyMovementForce.Force = &moveApplyMovementForceAck.Force;
    mover->SendMessageToSet(updateApplyMovementForce.Write(), false);
}

void WorldSession::HandleMoveRemoveMovementForceAck(WorldPackets::Movement::MoveRemoveMovementForceAck& moveRemoveMovementForceAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(moveRemoveMovementForceAck.Ack.Status.guid, moveRemoveMovementForceAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &moveRemoveMovementForceAck.Ack.Status))
        return;

    moveRemoveMovementForceAck.Ack.Status.time = AdjustClientMovementTime(moveRemoveMovementForceAck.Ack.Status.time);

    WorldPackets::Movement::MoveUpdateRemoveMovementForce updateRemoveMovementForce;
    updateRemoveMovementForce.Status = &moveRemoveMovementForceAck.Ack.Status;
    updateRemoveMovementForce.TriggerGUID = moveRemoveMovementForceAck.ID;
    mover->SendMessageToSet(updateRemoveMovementForce.Write(), false);
}

void WorldSession::HandleMoveSetModMovementForceMagnitudeAck(WorldPackets::Movement::MovementSpeedAck& setModMovementForceMagnitudeAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(setModMovementForceMagnitudeAck.Ack.Status.guid, setModMovementForceMagnitudeAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &setModMovementForceMagnitudeAck.Ack.Status))
        return;

    // skip all except last
    if (_player->m_movementForceModMagnitudeChanges > 0)
    {
        --_player->m_movementForceModMagnitudeChanges;
        if (!_player->m_movementForceModMagnitudeChanges)
        {
            float expectedModMagnitude = 1.0f;
            if (MovementForces const* movementForces = mover->GetMovementForces())
                expectedModMagnitude = movementForces->GetModMagnitude();

            if (std::fabs(expectedModMagnitude - setModMovementForceMagnitudeAck.Speed) > 0.01f)
            {
                TC_LOG_DEBUG("misc", "Player {} from account id {} kicked for incorrect movement force magnitude (must be {} instead {})",
                    _player->GetName(), _player->GetSession()->GetAccountId(), expectedModMagnitude, setModMovementForceMagnitudeAck.Speed);
                _player->GetSession()->KickPlayer("WorldSession::HandleMoveSetModMovementForceMagnitudeAck Incorrect magnitude");
                return;
            }
        }
    }

    setModMovementForceMagnitudeAck.Ack.Status.time = AdjustClientMovementTime(setModMovementForceMagnitudeAck.Ack.Status.time);

    WorldPackets::Movement::MoveUpdateSpeed updateModMovementForceMagnitude(SMSG_MOVE_UPDATE_MOD_MOVEMENT_FORCE_MAGNITUDE);
    updateModMovementForceMagnitude.Status = &setModMovementForceMagnitudeAck.Ack.Status;
    updateModMovementForceMagnitude.Speed = setModMovementForceMagnitudeAck.Speed;
    mover->SendMessageToSet(updateModMovementForceMagnitude.Write(), false);
}

void WorldSession::HandleMoveGravityModifierChangeAck(WorldPackets::Movement::MovementSpeedAck& gravityModifierAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(gravityModifierAck.Ack.Status.guid, gravityModifierAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &gravityModifierAck.Ack.Status))
        return;

    // The client copies value and sequence index out of the SMSG_MOVE_SET_GRAVITY_MODIFIER it last
    // received and sends them back unchanged (consumer 0x1F111F0, sender 0x18AA570), so a mismatch
    // means the value was tampered with rather than merely stale. Same skip-all-but-the-last
    // bookkeeping as the movement force magnitude ack, for the same reason: several changes in
    // flight produce several acks and only the final one describes the current state.
    if (_player->m_gravityModifierChanges > 0)
    {
        --_player->m_gravityModifierChanges;
        if (!_player->m_gravityModifierChanges)
        {
            float expectedGravityModifier = mover->m_movementInfo.gravityModifier;
            if (std::fabs(expectedGravityModifier - gravityModifierAck.Speed) > 0.01f)
            {
                TC_LOG_DEBUG("misc", "Player {} from account id {} kicked for incorrect gravity modifier (must be {} instead {})",
                    _player->GetName(), _player->GetSession()->GetAccountId(), expectedGravityModifier, gravityModifierAck.Speed);
                _player->GetSession()->KickPlayer("WorldSession::HandleMoveGravityModifierChangeAck Incorrect gravity modifier");
                return;
            }
        }
    }

    gravityModifierAck.Ack.Status.time = AdjustClientMovementTime(gravityModifierAck.Ack.Status.time);

    WorldPackets::Movement::MoveUpdateGravityModifier updateGravityModifier;
    updateGravityModifier.Status = &gravityModifierAck.Ack.Status;
    updateGravityModifier.GravityModifier = mover->m_movementInfo.gravityModifier;
    mover->SendMessageToSet(updateGravityModifier.Write(), false);
}

void WorldSession::HandleMoveInitialObjectUpdateCompleteAck(WorldPackets::Movement::MovementAckMessage& initialObjectUpdateCompleteAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(initialObjectUpdateCompleteAck.Ack.Status.guid, initialObjectUpdateCompleteAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &initialObjectUpdateCompleteAck.Ack.Status))
        return;

    // The client mirrors the sequence index byte for byte - 18 of 18 recorded pairs - so this rejects
    // a fabricated or garbled index. It does NOT tell world entries apart, and cannot: what we send
    // on this path is invariably 0. Player::SendInitialPacketsBeforeAddToMap sets m_movementCounter
    // to 0 for every non-seamless teleport (Player.cpp, top of the function) and the handshake line a
    // few statements later is the counter's first consumer; nothing in between draws from it. An ack
    // of an earlier world entry therefore carries the same 0 as the current one and passes here.
    // Keeping the draw as m_movementCounter++ is still right - it is what the counter is for, it
    // keeps the following SetCompoundState draws in sequence, and Retail sends 0 in 11 of the 18
    // recorded entries, which is exactly what a just-reset counter yields.
    if (uint32(initialObjectUpdateCompleteAck.Ack.AckIndex) != _player->m_initialObjectUpdateCompleteIndex)
    {
        TC_LOG_DEBUG("network", "CMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE_ACK: {} acked index {} but {} was sent, ignoring",
            GetPlayerInfo(), initialObjectUpdateCompleteAck.Ack.AckIndex, _player->m_initialObjectUpdateCompleteIndex);
        return;
    }

    // Since the index cannot do it, this is what actually keeps a foreign MovementInfo out of the
    // assignment below: the same guard every other position bearing client message gets in
    // HandleMovementOpcode above. An ack that arrives while a teleport is in flight describes the
    // world the mover is leaving, not the one it is entering.
    if (Player* plrMover = mover->ToPlayer(); plrMover && plrMover->IsBeingTeleported())
        return;

    initialObjectUpdateCompleteAck.Ack.Status.time = AdjustClientMovementTime(initialObjectUpdateCompleteAck.Ack.Status.time);
    mover->m_movementInfo = initialObjectUpdateCompleteAck.Ack.Status;

    // Step 3 of the verification loop for this pair is "log in and watch the ack come back". There is
    // no Lua event to observe - the consumer only queues a state change of type 0x6B - so reaching
    // this line IS the observation: the client accepted the 13 byte message, mirrored the sequence
    // index and answered with a MovementInfo the server considers valid. Logged at INFO because the
    // pair sits in the enter world path, where the test is worth exactly one login and one grep, and
    // because it has not been run against a live client yet (see below).
    TC_LOG_INFO("network", "CMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE_ACK: {} closed the initial object update handshake for index {} after {} ms",
        GetPlayerInfo(), _player->m_initialObjectUpdateCompleteIndex,
        std::chrono::duration_cast<Milliseconds>(GameTime::Now() - _player->m_initialObjectUpdateCompleteSentAt).count());

    // This is the moment the client has worked through the whole initial SMSG_UPDATE_OBJECT wave and
    // its movement prediction is initialised.
    // UNVERIFIED: what the Retail server does with this ack. Only the client side is observable - it
    // queues a state change of type 0x6B and mirrors the sequence index. The refresh below follows
    // the CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE twin, which marks the same milestone.
    // UNVERIFIED: step 3 of the verification loop has not been run, and it is blocked on the client
    // build, not on the code. The newest build_auth_key this tree carries is 69465
    // (auth_database.sql:1717, Win/x64/WoW); the 12.1 client on the development machine is
    // 12.1.0.69497, one build past it, and WorldSocket digests that key in the world handshake, so it
    // is rejected. The 12.0.7 clients that are present cannot stand in: d9523a0811 moved this tree to
    // 12.1 opcode numbering, and this very message is 0x5E0075 here against 0x5A0075 in 12.0.7.
    // Closing it needs upstream's auth SQL for 69497 (the per-build key cannot be derived) plus a
    // realmlist gamebuild bump - a tree wide change that does not belong to this unit. The log line
    // above is then the whole test: one login, then grep the world log for
    // "closed the initial object update handshake".
    _player->UpdateObjectVisibility(false);
}

void WorldSession::HandleMoveRemoveMovementForces(WorldPackets::Movement::ClientPlayerMovement& removeMovementForces)
{
    // The whole payload of this message is the guid list inside the MovementInfo - the writer
    // (0x6997C0) is byte for byte the one of CMSG_MOVE_STOP apart from the opcode. Blizzard calls the
    // field removeAreaTriggerGUIDs, and the client fills it from its own movement/collision layer
    // (sender 0x18A1870) when it drops forces locally. It is not an ack on a server command, so the
    // position part is handled like that of any other movement message.
    std::vector<ObjectGuid> removeForcesIDs = std::move(removeMovementForces.Status.removeForcesIDs);
    removeMovementForces.Status.removeForcesIDs.clear();

    Unit* mover = ValidateAndGetUnitBeingMoved(removeMovementForces.Status.guid, removeMovementForces.GetOpcode(), false);
    if (!mover)
        return;

    // The repair below rests entirely on "the server still holds the force, so the mover has not left
    // the trigger". That premise is only worth anything if the server position is current. Whenever
    // HandleMovementOpcode rejects the packet - IsBeingTeleported, an unfinalised movespline, or any
    // of the three transport coordinate checks - mover->m_movementInfo is left standing, the mover
    // has not moved as far as the server is concerned, AreaTrigger::UpdateTargetList cannot fire
    // OnUnitExit, and the force cannot be dropped no matter how often the client asks. Re-applying it
    // then is not a repair, it is an argument the client cannot win: the knockback spline case
    // (boss_leymor.cpp:427, boss_aqusirr.cpp:682 - a gravity AreaTrigger the client leaves locally
    // while the server holds the position until the spline ends) hits exactly this and would cost an
    // SMSG_MOVE_APPLY_MOVEMENT_FORCE plus a broadcast ack per client message.
    if (!HandleMovementOpcode(removeMovementForces.GetOpcode(), removeMovementForces.Status))
        return;

    // Even with a current position the two sides can disagree - the client's collision layer and
    // AreaTrigger::UpdateTargetList do not have to agree on the boundary to the millimetre - so the
    // repair is damped as well. Unit::ResendMovementForce carries no counter, no lock and no memory
    // of what it already re-applied to whom; its only stop condition is the server losing the force.
    // UNVERIFIED: the burst limit and the window length are a server choice with no Retail model.
    // The sniff of this opcode holds 0 packets, so neither how often Retail tolerates the message nor
    // whether it damps at all is observable. 5 per 1000 ms is picked to be far above any legitimate
    // rate - the client sends this on leaving a trigger, not per frame - and low enough that a
    // looping client cannot trade packets indefinitely.
    TimePoint const now = GameTime::Now();
    if (now - _movementForceRepairWindowStart >= MOVEMENT_FORCE_REPAIR_WINDOW)
    {
        _movementForceRepairWindowStart = now;
        _movementForceRepairCount = 0;
        _movementForceRepairThrottleLogged = false;
    }

    for (ObjectGuid const& removeForcesID : removeForcesIDs)
    {
        if (_movementForceRepairCount >= MOVEMENT_FORCE_REPAIR_BURST)
        {
            // Give up for the rest of the window rather than keep trading packets. The force stays
            // on the server, so the mechanic still applies to everything the server decides; only the
            // re-assertion towards this client pauses.
            if (!_movementForceRepairThrottleLogged)
            {
                _movementForceRepairThrottleLogged = true;
                TC_LOG_DEBUG("entities.unit", "CMSG_MOVE_REMOVE_MOVEMENT_FORCES: {} exceeded {} force re-applications within {} ms, throttling for the rest of the window",
                    GetPlayerInfo(), MOVEMENT_FORCE_REPAIR_BURST, MOVEMENT_FORCE_REPAIR_WINDOW.count());
            }
            break;
        }

        // Server side, an AreaTrigger force is dropped by AreaTriggerAI::OnUnitExit. If we still hold
        // it, the mover has not left the trigger as far as we are concerned, and honouring the
        // request would let a client shrug off a boss mechanic. Repair the client instead of
        // trusting it. The usual case is that the force is already gone and this merely confirms it -
        // that case sends nothing and therefore costs no budget.
        // UNVERIFIED: that Retail answers a disagreement with SMSG_MOVE_APPLY_MOVEMENT_FORCE. What is
        // established is only the client half - the writer at 0x6997C0 and the sender at 0x18A1870,
        // which fills removeAreaTriggerGUIDs from the client's own collision layer. The server half is
        // unobserved for the same reason as in the handshake twin above: 0 packets of this opcode in
        // the recordings, and no Lua event or error code that would show the reaction. Not obeying a
        // client that claims to have left a mechanic is the conservative reading, not a measured one.
        if (mover->ResendMovementForce(removeForcesID))
        {
            ++_movementForceRepairCount;
            TC_LOG_DEBUG("entities.unit", "CMSG_MOVE_REMOVE_MOVEMENT_FORCES: {} wants force {} dropped while still inside it, re-applying",
                GetPlayerInfo(), removeForcesID.ToString());
        }
    }
}

void WorldSession::HandleMoveSetTurnRateCheat(WorldPackets::Movement::MoveSetTurnRateCheat& moveSetTurnRateCheat)
{
    // Despite the name this is not a cheat tool: it hangs off the change callback of the client CVar
    // "TurnSpeed" (0x1DEF2D0), whose help text reads "Set the keyboard turn rate in degrees per
    // second; capped by the server". Every ordinary client sends it once per world entry - all
    // eleven recorded packets, from eleven separate recordings, carry float(M_PI), which is the CVar
    // default of 180 degrees/s converted to radians. It must never be treated as an attack.
    if (!(moveSetTurnRateCheat.TurnRate > 0.0f) || !std::isfinite(moveSetTurnRateCheat.TurnRate))
        return;

    if (!HasPermission(rbac::RBAC_PERM_CHANGE_TURN_RATE))
    {
        // "capped by the server". The refusal is silent when the client is already asking for the
        // rate we hold for it, and that is what Retail does: across 73 recordings the eleven
        // CMSG_MOVE_SET_TURN_RATE_CHEAT with the CVar default float(M_PI) are answered with nothing
        // at all - SMSG_MOVE_SET_TURN_RATE does not occur a single time in any of them, while
        // SMSG_MOVE_UPDATE_TURN_RATE does (twice, minutes away and for another mover), so its
        // absence is a real observation and not a mis-numbered opcode. There is nothing to correct
        // when both sides already hold the same value.
        //
        // The tolerance separates "the same rate written by two different constants" from "a rate
        // the player actually changed": TrinityCore's playerBaseMoveSpeed[MOVE_TURN_RATE] is
        // 3.141594f, the client sends float(M_PI) = 3.14159265f, a difference of 1.4e-6 rad/s. One
        // thousandth of a rad/s is 0.057 degrees/s - 700 times above that mismatch and far below any
        // turn rate a player would set through the CVar.
        //
        // UNVERIFIED: the correcting branch itself. No recording contains a client that asks for a
        // rate other than the default, so what Retail answers in that case is not observed; sending
        // the rate we hold is derived from the CVar help text "capped by the server", which promises
        // a cap rather than a silent discard. Unit::SetSpeedRate cannot be used for it - it returns
        // without sending anything when the rate is unchanged, and the rate we put the client back
        // on is by construction the one we already hold. Unit::ResendSpeed sends
        // SMSG_MOVE_SET_TURN_RATE unconditionally and raises m_forced_speed_changes[MOVE_TURN_RATE],
        // so HandleForceSpeedChangeAck reads the CMSG_MOVE_FORCE_TURN_RATE_CHANGE_ACK that follows
        // as expected rather than as a speed hack.
        if (std::fabs(_player->GetSpeed(MOVE_TURN_RATE) - moveSetTurnRateCheat.TurnRate) > 0.001f)
            _player->ResendSpeed(MOVE_TURN_RATE);

        return;
    }

    // The same tolerance applies here, and for the same reason. Without it the one value every
    // client actually sends would be applied as a real change on every account that holds the
    // permission: Unit::SetSpeed divides by playerBaseMoveSpeed[MOVE_TURN_RATE] = 3.141594f, so
    // float(M_PI) = 3.14159265f yields a rate of 0.9999996 instead of 1.0f, and Unit::SetSpeedRate
    // compares exactly - it would not return early, would push an SMSG_MOVE_SET_TURN_RATE, would
    // raise m_forced_speed_changes[MOVE_TURN_RATE] and would leave the session on 0.9999996. The
    // permission (RBAC_PERM_CHANGE_TURN_RATE, 886) hangs off role 194 "Sec Level Moderator", so that
    // would happen on every GM world entry - and it contradicts this unit's own measurement: 11
    // recorded CMSG with float(M_PI), 0 SMSG_MOVE_SET_TURN_RATE in 73 recordings. The permission
    // decides whether a rate the player really changed is honoured or capped, not whether the
    // constant mismatch between two spellings of pi counts as a change.
    if (std::fabs(_player->GetSpeed(MOVE_TURN_RATE) - moveSetTurnRateCheat.TurnRate) <= 0.001f)
        return;

    _player->SetSpeed(MOVE_TURN_RATE, moveSetTurnRateCheat.TurnRate);
}

void WorldSession::HandleMoveApplyInertiaAck(WorldPackets::Movement::MoveApplyInertiaAck& moveApplyInertiaAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(moveApplyInertiaAck.Ack.Status.guid, moveApplyInertiaAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &moveApplyInertiaAck.Ack.Status))
        return;

    moveApplyInertiaAck.Ack.Status.time = AdjustClientMovementTime(moveApplyInertiaAck.Ack.Status.time);
    mover->m_movementInfo = moveApplyInertiaAck.Ack.Status;

    WorldPackets::Movement::MoveUpdateApplyInertia updateApplyInertia;
    updateApplyInertia.Status = &mover->m_movementInfo;
    updateApplyInertia.InertiaID = moveApplyInertiaAck.InertiaID;
    updateApplyInertia.LifetimeMs = moveApplyInertiaAck.LifetimeMs;
    mover->SendMessageToSet(updateApplyInertia.Write(), false);
}

void WorldSession::HandleMoveRemoveInertiaAck(WorldPackets::Movement::MoveRemoveInertiaAck& moveRemoveInertiaAck)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(moveRemoveInertiaAck.Ack.Status.guid, moveRemoveInertiaAck.GetOpcode(), true);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &moveRemoveInertiaAck.Ack.Status))
        return;

    moveRemoveInertiaAck.Ack.Status.time = AdjustClientMovementTime(moveRemoveInertiaAck.Ack.Status.time);
    mover->m_movementInfo = moveRemoveInertiaAck.Ack.Status;

    WorldPackets::Movement::MoveUpdateRemoveInertia updateRemoveInertia;
    updateRemoveInertia.Status = &mover->m_movementInfo;
    updateRemoveInertia.InertiaID = moveRemoveInertiaAck.InertiaID;
    mover->SendMessageToSet(updateRemoveInertia.Write(), false);
}

void WorldSession::HandleMoveSplineDoneOpcode(WorldPackets::Movement::MoveSplineDone& moveSplineDone)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(moveSplineDone.Status.guid, moveSplineDone.GetOpcode(), false);
    if (!mover)
        return;

    if (!ValidateMovementInfo(mover, &moveSplineDone.Status))
        return;

    // in taxi flight packet received in 2 case:
    // 1) end taxi path in far (multi-node) flight
    // 2) switch from one map to other in case multim-map taxi path
    // we need process only (1)

    uint32 curDest = GetPlayer()->m_taxi.GetTaxiDestination();
    if (curDest)
    {
        TaxiNodesEntry const* curDestNode = sTaxiNodesStore.LookupEntry(curDest);

        // far teleport case
        if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
        {
            if (FlightPathMovementGenerator* flight = dynamic_cast<FlightPathMovementGenerator*>(GetPlayer()->GetMotionMaster()->GetCurrentMovementGenerator()))
            {
                bool shouldTeleport = curDestNode && curDestNode->ContinentID != GetPlayer()->GetMapId();
                if (!shouldTeleport)
                {
                    TaxiPathNodeEntry const* currentNode = flight->GetPath()[flight->GetCurrentNode()];
                    shouldTeleport = currentNode->Flags & TAXI_PATH_NODE_FLAG_TELEPORT;
                }

                if (shouldTeleport)
                {
                    // short preparations to continue flight
                    flight->SetCurrentNodeAfterTeleport();
                    TaxiPathNodeEntry const* node = flight->GetPath()[flight->GetCurrentNode()];
                    flight->SkipCurrentNode();

                    GetPlayer()->TeleportTo(curDestNode->ContinentID, node->Loc.X, node->Loc.Y, node->Loc.Z, GetPlayer()->GetOrientation());
                }
            }
        }

        return;
    }

    // at this point only 1 node is expected (final destination)
    if (GetPlayer()->m_taxi.GetPath().size() != 1)
        return;

    GetPlayer()->CleanupAfterTaxiFlight();
    GetPlayer()->SetFallInformation(0, GetPlayer()->GetPositionZ());
    if (GetPlayer()->pvpInfo.IsHostile)
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);
}

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPackets::Movement::MoveTimeSkipped& moveTimeSkipped)
{
    Unit* mover = ValidateAndGetUnitBeingMoved(moveTimeSkipped.MoverGUID, moveTimeSkipped.GetOpcode(), false);
    if (!mover)
        return;

    mover->m_movementInfo.time += moveTimeSkipped.TimeSkipped;

    WorldPackets::Movement::MoveSkipTime moveSkipTime;
    moveSkipTime.MoverGUID = moveTimeSkipped.MoverGUID;
    moveSkipTime.TimeSkipped = moveTimeSkipped.TimeSkipped;
    mover->SendMessageToSet(moveSkipTime.Write(), _player);
}

void WorldSession::HandleTimeSync(uint32 counter, int64 clientTime, TimePoint responseReceiveTime)
{
    auto serverTimeAtSent = _pendingTimeSyncRequests.extract(counter);
    if (!serverTimeAtSent)
        return;

    // time it took for the request to travel to the client, for the client to process it and reply and for response to travel back to the server.
    // we are going to make 2 assumptions:
    // 1) we assume that the request processing time equals 0.
    // 2) we assume that the packet took as much time to travel from server to client than it took to travel from client to server.
    uint32 roundTripDuration = getMSTimeDiff(serverTimeAtSent.mapped(), responseReceiveTime);
    int64 lagDelay = roundTripDuration / 2;

    /*
    clockDelta = serverTime - clientTime
    where
    serverTime: time that was displayed on the clock of the SERVER at the moment when the client processed the SMSG_TIME_SYNC_REQUEST packet.
    clientTime:  time that was displayed on the clock of the CLIENT at the moment when the client processed the SMSG_TIME_SYNC_REQUEST packet.

    Once clockDelta has been computed, we can compute the time of an event on server clock when we know the time of that same event on the client clock,
    using the following relation:
    serverTime = clockDelta + clientTime
    */
    int64 clockDelta = serverTimeAtSent.mapped() + lagDelay - clientTime;
    _timeSyncClockDeltaQueue->push_back(std::pair<int64, uint32>(clockDelta, roundTripDuration));
    ComputeNewClockDelta();
}

void WorldSession::HandleTimeSyncResponse(WorldPackets::Misc::TimeSyncResponse const& timeSyncResponse)
{
    HandleTimeSync(timeSyncResponse.SequenceIndex, timeSyncResponse.ClientTime, timeSyncResponse.GetReceivedTime());
}

void WorldSession::HandleTimeSyncResponseFailed(WorldPackets::Misc::TimeSyncResponseFailed const& timeSyncResponseFailed)
{
    // "The first sync request after the world change carried index N != 0." The client demands that
    // the counter restarts at 0 per world instance (guard 0x1E2B340), and rejects exactly one
    // request per world entry.
    //
    // The rejected index has to be an ordinary sync index we really handed out. The two special
    // records SPECIAL_INIT_ACTIVE_MOVER_TIME_SYNC_COUNTER (0xFFFFFFFF) and
    // SPECIAL_RESUME_COMMS_TIME_SYNC_COUNTER (0xFFFFFFFE) live in the same map but were never sent
    // as a sequence index, so a rejection naming them is a lie by construction.
    if (!_timeSyncNextCounter)
        return;

    uint32 const highestIssuedIndex = _timeSyncNextCounter - 1;
    if (timeSyncResponseFailed.SequenceIndex > highestIssuedIndex)
        return;

    if (!_pendingTimeSyncRequests.contains(timeSyncResponseFailed.SequenceIndex))
        return;

    // The restart is honoured at most once per world entry, exactly as the client describes its own
    // behaviour. Without this the handler is not self-limiting: the restart puts the counter back to
    // 0 and the request that follows registers index 0 again, so a repeated _FAILED(0) would pass
    // the check above every time and let a client drive an endless restart loop.
    if (_timeSyncRestartedByClient)
        return;

    _timeSyncRestartedByClient = true;

    TC_LOG_DEBUG("network", "CMSG_TIME_SYNC_RESPONSE_FAILED: client rejected sequence index {} for {}, restarting the time sync counter",
        timeSyncResponseFailed.SequenceIndex, GetPlayerInfo());

    // Restart the ordinary counter at 0 - the request that follows carries index 0, which is the one
    // the client will accept. This deliberately does NOT go through ResetTimeSync(): that clears the
    // whole map, and the two special records above are open during exactly this window (registered
    // in Map::SendInitSelf and in WorldSession::HandleContinuePlayerLogin). Losing them makes
    // HandleTimeSync return without a word for CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE and
    // CMSG_QUEUED_MESSAGES_END, and the world-entry clock probe silently disappears - the same
    // damage the range sweeps in HandleTimeSyncResponseDropped and HandleDiscardedTimeSyncAcks are
    // capped against.
    _pendingTimeSyncRequests.erase(_pendingTimeSyncRequests.begin(),
        _pendingTimeSyncRequests.upper_bound(highestIssuedIndex));
    _timeSyncNextCounter = 0;

    SendTimeSync();
}

void WorldSession::HandleTimeSyncResponseDropped(WorldPackets::Misc::TimeSyncResponseDropped const& timeSyncResponseDropped)
{
    // Both fields are sequence indices, not a sequence plus a client time - the producer (0x1896AD0)
    // reads +0x14 of the oldest and of the most recent still open record. It reports a RANGE of
    // requests that expired.
    if (timeSyncResponseDropped.SequenceIndexA > timeSyncResponseDropped.SequenceIndexB)
        return;

    TC_LOG_DEBUG("network", "CMSG_TIME_SYNC_RESPONSE_DROPPED: client dropped sequence indices {}..{} for {}",
        timeSyncResponseDropped.SequenceIndexA, timeSyncResponseDropped.SequenceIndexB, GetPlayerInfo());

    // Drop them without computing a clock delta: there is no client time to pair them with, and a
    // request the client will never answer would otherwise sit in _pendingTimeSyncRequests for the
    // rest of the session - the map is only ever shrunk by a matching response or by ResetTimeSync.
    // Erasing over the map range rather than counting through the numeric one keeps a bogus
    // 0..0xFFFFFFFF range cheap.
    //
    // Both bounds come straight from the client, so the sweep has to be capped at the highest index
    // we really handed out. SPECIAL_INIT_ACTIVE_MOVER_TIME_SYNC_COUNTER (0xFFFFFFFF) and
    // SPECIAL_RESUME_COMMS_TIME_SYNC_COUNTER (0xFFFFFFFE) live in the same map at the very top of
    // the value range; an uncapped range would take them with it, and the world-entry clock probe
    // that HandleTimeSync performs for CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE and for
    // CMSG_QUEUED_MESSAGES_END would then find nothing and return without a word.
    if (!_timeSyncNextCounter)
        return;

    uint32 const highestIssuedIndex = _timeSyncNextCounter - 1;
    if (timeSyncResponseDropped.SequenceIndexA > highestIssuedIndex)
        return;

    _pendingTimeSyncRequests.erase(_pendingTimeSyncRequests.lower_bound(timeSyncResponseDropped.SequenceIndexA),
        _pendingTimeSyncRequests.upper_bound(std::min(timeSyncResponseDropped.SequenceIndexB, highestIssuedIndex)));
}

void WorldSession::HandleDiscardedTimeSyncAcks(WorldPackets::Misc::DiscardedTimeSyncAcks const& discardedTimeSyncAcks)
{
    // "Everything up to and including MaxSequenceIndex is settled." Always follows a DROPPED and
    // always sits right before the client restarts its counter at 0. The client sends it twice in
    // some of the recordings, 1-2 s apart, so this has to be idempotent - erasing a range is.
    //
    // MaxSequenceIndex is unchecked client input, so the sweep is capped at the highest index we
    // really handed out. Without the cap a MaxSequenceIndex of 0xFFFFFFFF empties the whole map,
    // including SPECIAL_INIT_ACTIVE_MOVER_TIME_SYNC_COUNTER and
    // SPECIAL_RESUME_COMMS_TIME_SYNC_COUNTER, which sit at the top of the value range - and the
    // world-entry clock probe in HandleTimeSync would silently do nothing from then on.
    if (!_timeSyncNextCounter)
        return;

    uint32 const highestIssuedIndex = _timeSyncNextCounter - 1;
    _pendingTimeSyncRequests.erase(_pendingTimeSyncRequests.begin(),
        _pendingTimeSyncRequests.upper_bound(std::min(discardedTimeSyncAcks.MaxSequenceIndex, highestIssuedIndex)));
}

void WorldSession::SendTimeAdjustment(float timeScale)
{
    // The client applies the factor to its clock (0x354EDA0), logs "Time elapse scaled by %g to %g"
    // and answers with CMSG_TIME_ADJUSTMENT_RESPONSE. It files the adjustment in the same list as an
    // ordinary sync request (kind 1 instead of 0), so it shares the sequence namespace with them and
    // its answer is a clock sample like any other.
    // UNVERIFIED: which situation makes Retail send this. The opcode appears in none of the ten
    // recordings, so there is no observed trigger and no observed range for timeScale. Rather than
    // invent a game rule, the only caller is the operator command ".debug send timeadjustment"
    // (cs_debug.cpp) - enough to exercise the pair against a live client, and honest about the fact
    // that the Retail trigger is still open.
    WorldPackets::Misc::TimeAdjustment timeAdjustment;
    timeAdjustment.SequenceIndex = _timeSyncNextCounter;
    timeAdjustment.TimeScale = timeScale;
    SendPacket(timeAdjustment.Write());

    RegisterTimeSync(_timeSyncNextCounter);
    ++_timeSyncNextCounter;
}

void WorldSession::HandleTimeAdjustmentResponse(WorldPackets::Misc::TimeAdjustmentResponse const& timeAdjustmentResponse)
{
    // Layout-identical to CMSG_TIME_SYNC_RESPONSE and drawn from the same clock, so it feeds the
    // clock delta computation the same way. The scale factor is not echoed back.
    HandleTimeSync(timeAdjustmentResponse.SequenceIndex, timeAdjustmentResponse.ClientTime, timeAdjustmentResponse.GetReceivedTime());
}

void WorldSession::HandleQueuedMessagesEnd(WorldPackets::Auth::QueuedMessagesEnd const& queuedMessagesEnd)
{
    HandleTimeSync(SPECIAL_RESUME_COMMS_TIME_SYNC_COUNTER, queuedMessagesEnd.Timestamp, queuedMessagesEnd.GetRawPacket()->GetReceivedTime());
}

// CMSG_SUSPEND_COMMS_ACK (12.1 0x440000). Same shape and the same field order as CMSG_TIME_SYNC_RESPONSE and
// CMSG_QUEUED_MESSAGES_END, so once it is established that the ack is one of ours it yields one more clock delta
// sample, exactly like those two.
//
// What ClientTick is and is not: the client fills it from its own millisecond clock (0x354ED50) while building the
// ack in consumer 0x18C1610. Over the corpus (defined above WorldPackets::Auth::SuspendComms in
// AuthenticationPackets.h) all 78 pairs across 24 recordings agree: (ClientTick - PKT tick) is constant to within
// a millisecond inside a single recording and different in every recording, in the range of a process uptime
// (1.7 h to 7.7 d). It is therefore an opaque client tick, exactly like the value CMSG_TIME_SYNC_RESPONSE
// carries - usable for a clock delta, not comparable against server time. HandleTimeSync treats it that way.
//
// The other half of that equal treatment is the receive time, and it does not come for free:
// WorldPacket::GetReceivedTime is only filled for the opcodes named in WorldSocket::ReadDataHandler, so
// CMSG_SUSPEND_COMMS_ACK is listed there next to the other three HandleTimeSync opcodes. Without that entry the
// roundTripDuration below would be computed against a default constructed TimePoint.
//
// SerialNumber is echoed from the SMSG_SUSPEND_COMMS we sent (78/78 pairs over the corpus) and is therefore
// entirely client controlled on the way back. It must NOT reach HandleTimeSync as a counter: that counter is the
// key of _pendingTimeSyncRequests, whose entries are all server minted (SendTimeSync, the resume counter, the
// init-active-mover counter), and a client that echoed the sequence index of a live SMSG_TIME_SYNC_REQUEST would
// consume that entry - dropping the genuine CMSG_TIME_SYNC_RESPONSE - and inject a clock delta of its own
// choosing into the six slot _timeSyncClockDeltaQueue, which feeds AdjustClientMovementTime and TransportServerTime.
//
// So the serial is checked, not used: it only has to match the one suspend this session actually has outstanding,
// and the sample is then booked under the reserved counter that WorldSession::SendSuspendComms registered.
// Today nothing is ever outstanding, so every ack that arrives is unsolicited and dropped here. The reason is not
// that the packet is unused in retail - it is that this server never reaches the situation retail sends it in. The
// full derivation, including the client-side condition and the lines of this tree that keep it from being met, is
// written out above WorldPackets::Auth::SuspendComms; the guard in WorldSession::AddInstanceConnection is what
// reports it if the situation ever does arise.
void WorldSession::HandleSuspendCommsAck(WorldPackets::Auth::SuspendCommsAck const& suspendCommsAck)
{
    // The two rejections are one branch on purpose. Both are triggerable by the client at will, and today the
    // first one is the only path this handler ever takes, so their log output is what an authenticated client can
    // drive - capped per session (WorldSession.h, MaxSuspendCommsAcksLoggedPerSession) exactly as the
    // attacker-controlled text of CMSG_LOG_STREAMING_ERROR is. Deciding it once here is also what keeps the two
    // reasons from drifting apart, and the counter is incremented before the cap is consulted so that
    // ~WorldSession can report what the cap swallowed.
    //
    // network.telemetry, not network, and for the same reason the sister case uses it: worldserver.conf.dist
    // declares no Logger.network, so Logger.root=5 (Error, line 4164) applies to it and TC_LOG_DEBUG("network")
    // is discarded as shipped. Logger.network.telemetry=3 (Info, line 4171) with Appender.Console=1,3,0 (line
    // 4141) is written without the operator doing anything. That matters here more than anywhere: because
    // SendSuspendComms has no caller, _suspendCommsPendingSerial is always empty and EVERY arriving ack lands in
    // this branch, so on the debug logger a session with up to ten unsolicited acks produced no output at all -
    // and the reader in ~WorldSession only speaks above the cap. Values 1..10 of the counter had no reader.
    // HandleLogStreamingError (AuthHandler.cpp) carries its lines on TC_LOG_INFO("network.telemetry") after the
    // same finding; the cap is what bounds the volume, on either logger.
    if (!_suspendCommsPendingSerial || *_suspendCommsPendingSerial != suspendCommsAck.SerialNumber)
    {
        ++_suspendCommsAcksRejected;
        if (_suspendCommsAcksRejected <= MaxSuspendCommsAcksLoggedPerSession)
        {
            if (!_suspendCommsPendingSerial)
                TC_LOG_INFO("network.telemetry", "WorldSession::HandleSuspendCommsAck: {} sent an unsolicited acknowledgement (serial {}), ignored ({}/{})",
                    GetPlayerInfo(), suspendCommsAck.SerialNumber, _suspendCommsAcksRejected, MaxSuspendCommsAcksLoggedPerSession);
            else
                TC_LOG_INFO("network.telemetry", "WorldSession::HandleSuspendCommsAck: {} acknowledged serial {} while serial {} is outstanding, ignored ({}/{})",
                    GetPlayerInfo(), suspendCommsAck.SerialNumber, *_suspendCommsPendingSerial, _suspendCommsAcksRejected, MaxSuspendCommsAcksLoggedPerSession);

            if (_suspendCommsAcksRejected == MaxSuspendCommsAcksLoggedPerSession)
                TC_LOG_INFO("network.telemetry", "WorldSession::HandleSuspendCommsAck: {} reached the per session cap on rejected acknowledgements, further ones are counted but not logged - ~WorldSession prints the total",
                    GetPlayerInfo());
        }

        return;
    }

    // One suspend, one ack. Clearing first makes a replay of the same serial fall into the branch above.
    _suspendCommsPendingSerial.reset();

    HandleTimeSync(SPECIAL_SUSPEND_COMMS_TIME_SYNC_COUNTER, suspendCommsAck.ClientTick, suspendCommsAck.GetRawPacket()->GetReceivedTime());
}

void WorldSession::HandleMoveInitActiveMoverComplete(WorldPackets::Movement::MoveInitActiveMoverComplete const& moveInitActiveMoverComplete)
{
    HandleTimeSync(SPECIAL_INIT_ACTIVE_MOVER_TIME_SYNC_COUNTER, moveInitActiveMoverComplete.Ticks, moveInitActiveMoverComplete.GetRawPacket()->GetReceivedTime());

    _player->UpdateObjectVisibility(false);
}

void WorldSession::ComputeNewClockDelta()
{
    // implementation of the technique described here: https://web.archive.org/web/20180430214420/http://www.mine-control.com/zack/timesync/timesync.html
    // to reduce the skew induced by dropped TCP packets that get resent.

    using namespace boost::accumulators;

    accumulator_set<uint32, features<tag::mean, tag::median, tag::variance(lazy)> > latencyAccumulator;

    for (auto [_, roundTripDuration] : *_timeSyncClockDeltaQueue)
        latencyAccumulator(roundTripDuration);

    uint32 latencyMedian = static_cast<uint32>(std::round(median(latencyAccumulator)));
    uint32 latencyStandardDeviation = static_cast<uint32>(std::round(sqrt(variance(latencyAccumulator))));

    accumulator_set<int64, features<tag::mean> > clockDeltasAfterFiltering;
    uint32 sampleSizeAfterFiltering = 0;
    for (auto [clockDelta, roundTripDuration] : *_timeSyncClockDeltaQueue)
    {
        if (roundTripDuration < latencyStandardDeviation + latencyMedian) {
            clockDeltasAfterFiltering(clockDelta);
            sampleSizeAfterFiltering++;
        }
    }

    if (sampleSizeAfterFiltering != 0)
    {
        int64 meanClockDelta = static_cast<int64>(std::round(mean(clockDeltasAfterFiltering)));
        if (std::abs(meanClockDelta - _timeSyncClockDelta) > 25)
            _timeSyncClockDelta = meanClockDelta;
    }
    else if (_timeSyncClockDelta == 0)
        _timeSyncClockDelta = _timeSyncClockDeltaQueue->back().first;

    if (_player)
    {
        _player->SetPlayerLocalFlag(PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME);
        _player->SetTransportServerTime(int32(_timeSyncClockDelta));
    }
}

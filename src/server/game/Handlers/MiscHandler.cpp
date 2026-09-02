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
#include "AccountMgr.h"
#include "AchievementMgr.h"
#include "AchievementPackets.h"
#include "AreaTriggerPackets.h"
#include "Battleground.h"
#include "CharacterPackets.h"
#include "Chat.h"
#include "CinematicMgr.h"
#include "ClientConfigPackets.h"
#include "CombatManager.h"
#include "Common.h"
#include "Conversation.h"
#include "ConversationAI.h"
#include "Corpse.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "DBCEnums.h"
#include "GossipDef.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstancePackets.h"
#include "InstanceScenario.h"
#include "ChallengeMode.h"
#include "InstanceScript.h"
#include "Language.h"
#include "Log.h"
#include "Map.h"
#include "MiscPackets.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "OutdoorPvP.h"
#include "Player.h"
#include "RBAC.h"
#include "RestMgr.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellPackets.h"
#include "WhoListStorage.h"
#include "WhoPackets.h"
#include "World.h"
#include <cstdarg>
#include <sstream>
#include <zlib.h>

void WorldSession::HandleRepopRequest(WorldPackets::Misc::RepopRequest& /*packet*/)
{
    if (GetPlayer()->IsAlive() || GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return;

    if (GetPlayer()->HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
        return; // silently return, client should display the error by itself

    // the world update order is sessions, players, creatures
    // the netcode runs in parallel with all of these
    // creatures can kill players
    // so if the server is lagging enough the player can
    // release spirit after he's killed but before he is updated
    if (GetPlayer()->getDeathState() == JUST_DIED)
    {
        TC_LOG_DEBUG("network", "HandleRepopRequestOpcode: got request after player {} {} was killed and before he was updated",
            GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());
        GetPlayer()->KillPlayer();
    }

    //this is spirit release confirm?
    GetPlayer()->RemovePet(nullptr, PET_SAVE_NOT_IN_SLOT, true);
    GetPlayer()->BuildPlayerRepop();
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleSetPreferredCemetery(WorldPackets::Misc::SetPreferredCemetery& packet)
{
    // Store the player's chosen graveyard; RepopAtGraveyard honors it when it is linked to the current zone.
    _player->SetPreferredGraveyard(packet.CemeteryID);
}

void WorldSession::HandleReportStuckInCombat(WorldPackets::Misc::ReportStuckInCombat& /*packet*/)
{
    // The client reports it believes the player is wrongly stuck in combat. Re-validate the player's
    // combat references and drop any that are no longer valid (dead / out of range / gone), which clears
    // a desynced combat state without ending combat that is still legitimately ongoing.
    if (!_player->IsInCombat())
        return;

    _player->GetCombatManager().RevalidateCombat();
}

void WorldSession::HandleWhoOpcode(WorldPackets::Who::WhoRequestPkt& whoRequest)
{
    WorldPackets::Who::WhoRequest& request = whoRequest.Request;

    TC_LOG_DEBUG("network", "WorldSession::HandleWhoOpcode: MinLevel: {}, MaxLevel: {}, Name: {} (VirtualRealmName: {}), Guild: {} (GuildVirtualRealmName: {}), RaceFilter: 0x{:X}{:08X}, ClassFilter: {}, Areas: {}, Words: {}.",
        request.MinLevel, request.MaxLevel, request.Name, request.VirtualRealmName, request.Guild, request.GuildVirtualRealmName,
        request.RaceFilter.RawValue[1], request.RaceFilter.RawValue[0], request.ClassFilter, whoRequest.Areas.size(), request.Words.size());

    // zones count, client limit = 10 (2.0.10)
    // can't be received from real client or broken packet
    if (whoRequest.Areas.size() > 10)
        return;

    // user entered strings count, client limit=4 (checked on 2.0.10)
    // can't be received from real client or broken packet
    if (request.Words.size() > 4)
        return;

    /// @todo: handle following packet values
    /// VirtualRealmNames
    /// ShowEnemies
    /// ShowArenaPlayers
    /// ExactName
    /// ServerInfo

    std::vector<std::wstring> wWords;
    wWords.resize(request.Words.size());
    for (size_t i = 0; i < request.Words.size(); ++i)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleWhoOpcode: Word: {}", request.Words[i].Word);

        // user entered string, it used as universal search pattern(guild+player name)?
        if (!Utf8toWStr(request.Words[i].Word, wWords[i]))
            continue;

        wstrToLower(wWords[i]);
    }

    std::wstring wPlayerName;
    std::wstring wGuildName;

    if (!(Utf8toWStr(request.Name, wPlayerName) && Utf8toWStr(request.Guild, wGuildName)))
        return;

    wstrToLower(wPlayerName);
    wstrToLower(wGuildName);

    // client send in case not set max level value 100 but Trinity supports 255 max level,
    // update it to show GMs with characters after 100 level
    if (whoRequest.Request.MaxLevel >= MAX_LEVEL)
        whoRequest.Request.MaxLevel = STRONG_MAX_LEVEL;

    uint32 team = _player->GetTeam();

    uint32 gmLevelInWhoList  = sWorld->getIntConfig(CONFIG_GM_LEVEL_IN_WHO_LIST);

    WorldPackets::Who::WhoResponsePkt response;
    response.Token = whoRequest.Token;

    WhoListInfoVector const& whoList = sWhoListStorageMgr->GetWhoList();
    for (WhoListPlayerInfo const& target : whoList)
    {
        // player can see member of other team only if has RBAC_PERM_TWO_SIDE_WHO_LIST
        if (target.GetTeam() != team && !HasPermission(rbac::RBAC_PERM_TWO_SIDE_WHO_LIST))
            continue;

        // player can see MODERATOR, GAME MASTER, ADMINISTRATOR only if has RBAC_PERM_WHO_SEE_ALL_SEC_LEVELS
        if (target.GetSecurity() > AccountTypes(gmLevelInWhoList) && !HasPermission(rbac::RBAC_PERM_WHO_SEE_ALL_SEC_LEVELS))
            continue;

        // check if target is globally visible for player
        if (_player->GetGUID() != target.GetGuid() && !target.IsVisible())
            if (AccountMgr::IsPlayerAccount(_player->GetSession()->GetSecurity()) || target.GetSecurity() > _player->GetSession()->GetSecurity())
                continue;

        // check if target's level is in level range
        uint8 lvl = target.GetLevel();
        if (lvl < request.MinLevel || lvl > request.MaxLevel)
            continue;

        // check if class matches classmask
        if (request.ClassFilter >= 0 && !(request.ClassFilter & (1 << target.GetClass())))
            continue;

        // check if race matches racemask
        if (!request.RaceFilter.HasRace(target.GetRace()))
            continue;

        if (!whoRequest.Areas.empty())
        {
            if (std::find(whoRequest.Areas.begin(), whoRequest.Areas.end(), int32(target.GetZoneId())) == whoRequest.Areas.end())
                continue;
        }

        std::wstring const& wTargetName = target.GetWidePlayerName();
        if (!(wPlayerName.empty() || wTargetName.find(wPlayerName) != std::wstring::npos))
            continue;

        std::wstring const& wTargetGuildName = target.GetWideGuildName();

        if (!wGuildName.empty() && wTargetGuildName.find(wGuildName) == std::wstring::npos)
            continue;

        if (!wWords.empty())
        {
            std::string aName;
            if (AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(target.GetZoneId()))
                aName = areaEntry->AreaName[GetSessionDbcLocale()];

            bool show = false;
            for (size_t i = 0; i < wWords.size(); ++i)
            {
                if (!wWords[i].empty())
                {
                    if (wTargetName.find(wWords[i]) != std::wstring::npos ||
                        wTargetGuildName.find(wWords[i]) != std::wstring::npos ||
                        Utf8FitTo(aName, wWords[i]))
                    {
                        show = true;
                        break;
                    }
                }
            }

            if (!show)
                continue;
        }

        WorldPackets::Who::WhoEntry whoEntry;
        if (!whoEntry.PlayerData.Initialize(target.GetGuid(), nullptr))
            continue;

        if (!target.GetGuildGuid().IsEmpty())
        {
            whoEntry.GuildGUID = target.GetGuildGuid();
            whoEntry.GuildVirtualRealmAddress = GetVirtualRealmAddress();
            whoEntry.GuildName = target.GetGuildName();
        }

        whoEntry.AreaID = target.GetZoneId();
        whoEntry.IsGM = target.IsGameMaster();

        response.Response.Entries.push_back(whoEntry);

        // 50 is maximum player count sent to client - can be overridden
        // through config, but is unstable
        if (response.Response.Entries.size() >= sWorld->getIntConfig(CONFIG_MAX_WHO))
            break;
    }

    SendPacket(response.Write());
}

void WorldSession::HandleLogoutRequestOpcode(WorldPackets::Character::LogoutRequest& logoutRequest)
{
    if (!GetPlayer()->GetLootGUID().IsEmpty())
        GetPlayer()->SendLootReleaseAll();

    bool instantLogout = GetPlayer()->IsInFlight();
    if (!logoutRequest.IdleLogout)
        instantLogout |= (GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING) && !GetPlayer()->IsInCombat())
            || HasPermission(rbac::RBAC_PERM_INSTANT_LOGOUT);

    /// TODO: Possibly add RBAC permission to log out in combat
    bool canLogoutInCombat = GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING);

    uint32 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
        reason = 1;
    else if (GetPlayer()->IsFalling())
        reason = 3;                                         // is jumping or falling
    else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
        reason = 2;                                         // FIXME - Need the correct value

    WorldPackets::Character::LogoutResponse logoutResponse;
    logoutResponse.LogoutResult = reason;
    logoutResponse.Instant = instantLogout;
    SendPacket(logoutResponse.Write());

    if (reason)
    {
        SetLogoutStartTime(0);
        return;
    }

    // instant logout in taverns/cities or on taxi or for admins, gm's, mod's if its enabled in worldserver.conf
    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if (GetPlayer()->CanFreeMove())
    {
        if (GetPlayer()->GetStandState() == UNIT_STAND_STATE_STAND)
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);
        GetPlayer()->SetRooted(true);
        GetPlayer()->SetUnitFlag(UNIT_FLAG_STUNNED);
    }

    SetLogoutStartTime(GameTime::GetGameTime());
}

void WorldSession::HandleLogoutCancelOpcode(WorldPackets::Character::LogoutCancel& /*logoutCancel*/)
{
    // Player have already logged out serverside, too late to cancel
    if (!GetPlayer())
        return;

    SetLogoutStartTime(0);

    SendPacket(WorldPackets::Character::LogoutCancelAck().Write());

    // not remove flags if can't free move - its not set in Logout request code.
    if (GetPlayer()->CanFreeMove())
    {
        //!we can move again
        GetPlayer()->SetRooted(false);

        //! Stand Up
        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveUnitFlag(UNIT_FLAG_STUNNED);
    }
}

void WorldSession::HandleLogoutInstant(WorldPackets::Character::LogoutInstant& /*logoutInstant*/)
{
    // CMSG_LOGOUT_INSTANT is the client asking to skip the LOGOUT_TIME timer / camera zoom-out of
    // CMSG_LOGOUT_REQUEST. It is still server-authoritative: the same conditions that block an instant
    // logout in HandleLogoutRequestOpcode (combat, falling, dueling/frozen) block it here, so it cannot
    // be used to escape combat. When permitted, log out immediately.
    if (!GetPlayer()->GetLootGUID().IsEmpty())
        GetPlayer()->SendLootReleaseAll();

    bool canLogoutInCombat = GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING);

    uint32 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
        reason = 1;
    else if (GetPlayer()->IsFalling())
        reason = 3;                                         // is jumping or falling
    else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
        reason = 2;

    if (reason)
    {
        WorldPackets::Character::LogoutResponse logoutResponse;
        logoutResponse.LogoutResult = reason;
        logoutResponse.Instant = false;
        SendPacket(logoutResponse.Write());
        SetLogoutStartTime(0);
        return;
    }

    LogoutPlayer(true);
}

void WorldSession::HandleTogglePvP(WorldPackets::Misc::TogglePvP& /*packet*/)
{
    if (!GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_IN_PVP))
    {
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.EndTimer)
            GetPlayer()->UpdatePvP(true, true);
    }
    else if (!GetPlayer()->IsWarModeLocalActive())
    {
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->pvpInfo.IsHostile && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.EndTimer = GameTime::GetGameTime();     // start toggle-off
    }
}

void WorldSession::HandleSetPvP(WorldPackets::Misc::SetPvP& packet)
{
    if (packet.EnablePVP)
    {
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.EndTimer)
            GetPlayer()->UpdatePvP(true, true);
    }
    else if (!GetPlayer()->IsWarModeLocalActive())
    {
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->pvpInfo.IsHostile && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.EndTimer = GameTime::GetGameTime();     // start toggle-off
    }
}

void WorldSession::HandleSetWarMode(WorldPackets::Misc::SetWarMode& packet)
{
    _player->SetWarModeDesired(packet.Enable);
}

void WorldSession::HandleOverrideScreenFlash(WorldPackets::Misc::OverrideScreenFlash& packet)
{
    // Pure client accessibility preference (disable screen-flash effects); the server has no gameplay
    // use for it and only needs to accept and remember it for the session (queryable via
    // HasScreenFlashOverride). Not persisted - the client resends its option value on every login.
    _overrideScreenFlash = packet.Override;
}

void WorldSession::HandlePortGraveyard(WorldPackets::Misc::PortGraveyard& /*packet*/)
{
    if (GetPlayer()->IsAlive() || !GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return;
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleRequestCemeteryList(WorldPackets::Misc::RequestCemeteryList& /*packet*/)
{
    uint32 zoneId = _player->GetZoneId();
    uint32 team = _player->GetTeam();

    std::vector<uint32> graveyardIds;
    auto range = sObjectMgr->GraveyardStore.equal_range(zoneId);

    for (auto it = range.first; it != range.second && graveyardIds.size() < 16; ++it) // client max
    {
        ConditionSourceInfo conditionSource(_player);
        if (!it->second.Conditions.Meets(conditionSource))
            continue;

        graveyardIds.push_back(it->first);
    }

    if (graveyardIds.empty())
    {
        TC_LOG_DEBUG("network", "No graveyards found for zone {} for {} (team {}) in CMSG_REQUEST_CEMETERY_LIST",
            zoneId, _player->GetGUID().ToString(), team);
        return;
    }

    WorldPackets::Misc::RequestCemeteryListResponse packet;
    packet.IsGossipTriggered = false;
    packet.CemeteryID.reserve(graveyardIds.size());

    for (uint32 id : graveyardIds)
        packet.CemeteryID.push_back(id);

    SendPacket(packet.Write());
}

void WorldSession::HandleGetAccountNotifications(WorldPackets::Misc::GetAccountNotifications& /*packet*/)
{
    // TrinityCore has no account-notification system; answer the client's request with an empty list so
    // it does not sit waiting for a response it would otherwise never receive.
    WorldPackets::Misc::AccountNotificationsResponse response;
    SendPacket(response.Write());
}

void WorldSession::HandleSetSelectionOpcode(WorldPackets::Misc::SetSelection& packet)
{
    _player->SetSelection(packet.Selection);
}

void WorldSession::HandleStandStateChangeOpcode(WorldPackets::Misc::StandStateChange& packet)
{
    switch (packet.StandState)
    {
        case UNIT_STAND_STATE_STAND:
        case UNIT_STAND_STATE_SIT:
        case UNIT_STAND_STATE_SLEEP:
        case UNIT_STAND_STATE_KNEEL:
            break;
        default:
            return;
    }

    _player->SetStandState(packet.StandState);
}

void WorldSession::HandleReclaimCorpse(WorldPackets::Misc::ReclaimCorpse& /*packet*/)
{
    if (_player->IsAlive())
        return;

    // do not allow corpse reclaim in arena
    if (_player->InArena())
        return;

    // body not released yet
    if (!_player->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return;

    Corpse* corpse = _player->GetCorpse();
    if (!corpse)
        return;

    // prevent resurrect before 30-sec delay after body release not finished
    if (time_t(corpse->GetGhostTime() + _player->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP)) > time_t(GameTime::GetGameTime()))
        return;

    if (!corpse->IsWithinDistInMap(_player, CORPSE_RECLAIM_RADIUS, true))
        return;

    // resurrect
    _player->ResurrectPlayer(_player->InBattleground() ? 1.0f : 0.5f);

    // spawn bones
    _player->SpawnCorpseBones();
}

void WorldSession::HandleResurrectResponse(WorldPackets::Misc::ResurrectResponse& packet)
{
    if (GetPlayer()->IsAlive())
        return;

    if (packet.Response != 0) // Accept = 0 Decline = 1 Timeout = 2
    {
        GetPlayer()->ClearResurrectRequestData();           // reject
        return;
    }

    if (!GetPlayer()->IsResurrectRequestedBy(packet.Resurrecter))
        return;

    if (Player* ressPlayer = ObjectAccessor::GetPlayer(*GetPlayer(), packet.Resurrecter))
    {
        if (InstanceScript* instance = ressPlayer->GetInstanceScript())
        {
            // Raid encounters consume a charge while the encounter runs; Mythic Keystone dungeons use the
            // run-wide pool for the entire active run (retail 12.x).
            InstanceMap* instanceMap = ressPlayer->GetMap()->ToInstanceMap();
            bool const limitActive = instance->IsEncounterInProgress()
                || (instanceMap && instanceMap->GetChallengeMode() && instanceMap->GetChallengeMode()->IsActive());
            if (limitActive)
            {
                if (!instance->GetCombatResurrectionCharges())
                    return;
                else
                    instance->UseCombatResurrection();
            }
        }
    }

    GetPlayer()->ResurrectUsingRequestData();
}

void WorldSession::HandleAreaTriggerOpcode(WorldPackets::AreaTrigger::AreaTrigger& packet)
{
    Player* player = GetPlayer();
    if (player->IsInFlight())
    {
        TC_LOG_DEBUG("network", "HandleAreaTriggerOpcode: Player '{}' {} in flight, ignore Area Trigger ID: {}",
            player->GetName(), player->GetGUID().ToString(), packet.AreaTriggerID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(packet.AreaTriggerID);
    if (!atEntry)
    {
        TC_LOG_DEBUG("network", "HandleAreaTriggerOpcode: Player '{}' {} send unknown (by DBC) Area Trigger ID: {}",
            player->GetName(), player->GetGUID().ToString(), packet.AreaTriggerID);
        return;
    }

    if (packet.Entered != player->IsInAreaTrigger(atEntry))
    {
        TC_LOG_DEBUG("network", "HandleAreaTriggerOpcode: Player '{}' {} too far, ignore Area Trigger ID: {}",
            player->GetName(), player->GetGUID().ToString(), packet.AreaTriggerID);
        return;
    }

    if (player->isDebugAreaTriggers)
        ChatHandler(player->GetSession()).PSendSysMessage(packet.Entered ? LANG_DEBUG_AREATRIGGER_ENTERED : LANG_DEBUG_AREATRIGGER_LEFT, packet.AreaTriggerID);

    if (!sConditionMgr->IsObjectMeetingNotGroupedConditions(CONDITION_SOURCE_TYPE_AREATRIGGER_CLIENT_TRIGGERED, atEntry->ID, player))
        return;

    if (sScriptMgr->OnAreaTrigger(player, atEntry, packet.Entered))
        return;

    if (atEntry->AreaTriggerActionSetID)
    {
        if (packet.Entered)
            player->UpdateCriteria(CriteriaType::EnterAreaTriggerWithActionSet, atEntry->AreaTriggerActionSetID);
        else
            player->UpdateCriteria(CriteriaType::LeaveAreaTriggerWithActionSet, atEntry->AreaTriggerActionSetID);
    }

    if (player->IsAlive() && packet.Entered)
    {
        // not using Player::UpdateQuestObjectiveProgress, ObjectID in quest_objectives can be set to -1, areatrigger_involvedrelation then holds correct id
        if (std::unordered_set<uint32> const* quests = sObjectMgr->GetQuestsForAreaTrigger(packet.AreaTriggerID))
        {
            bool anyObjectiveChangedCompletionState = false;
            for (uint32 questId : *quests)
            {
                Quest const* qInfo = sObjectMgr->GetQuestTemplate(questId);
                uint16 slot = player->FindQuestSlot(questId);
                if (qInfo && slot < MAX_QUEST_LOG_SIZE && player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                {
                    for (QuestObjective const& obj : qInfo->Objectives)
                    {
                        if (obj.Type != QUEST_OBJECTIVE_AREATRIGGER)
                            continue;

                        if (!player->IsQuestObjectiveCompletable(slot, qInfo, obj))
                            continue;

                        if (player->IsQuestObjectiveComplete(slot, qInfo, obj))
                            continue;

                        if (obj.ObjectID != -1 && obj.ObjectID != packet.AreaTriggerID)
                            continue;

                        player->SetQuestObjectiveData(obj, 1);
                        player->SendQuestUpdateAddCreditSimple(obj);
                        anyObjectiveChangedCompletionState = true;
                        break;
                    }

                    if (qInfo->HasFlag(QUEST_FLAGS_COMPLETION_AREA_TRIGGER))
                        player->AreaExploredOrEventHappens(questId);

                    if (player->CanCompleteQuest(questId))
                        player->CompleteQuest(questId);
                }
            }

            if (anyObjectiveChangedCompletionState)
                player->UpdateVisibleObjectInteractions(true, false, false, true);
        }
    }

    if (sObjectMgr->IsTavernAreaTrigger(packet.AreaTriggerID))
    {
        // set resting flag we are in the inn
        if (packet.Entered)
        {
            player->GetRestMgr().SetInnTrigger(InnAreaTrigger{ .IsDBC = true, .AreaTriggerEntryId = atEntry->ID });
        }
        else
        {
            player->GetRestMgr().RemoveRestFlag(REST_FLAG_IN_TAVERN);
            player->GetRestMgr().SetInnTrigger(std::nullopt);
        }

        if (sWorld->IsFFAPvPRealm())
        {
            if (packet.Entered)
                player->RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
            else
                player->SetPvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        }

        return;
    }

    if (OutdoorPvP* pvp = player->GetOutdoorPvP())
        if (pvp->HandleAreaTrigger(_player, packet.AreaTriggerID, packet.Entered))
            return;

    if (!packet.Entered)
        return;

    AreaTriggerTeleport const* at = sObjectMgr->GetAreaTrigger(packet.AreaTriggerID);
    if (!at)
        return;

    bool teleported = false;
    if (player->GetMapId() != at->Loc.GetMapId())
    {
        if (!player->IsAlive())
        {
            if (player->HasCorpse())
            {
                // let enter in ghost mode in instance that connected to inner instance with corpse
                uint32 corpseMap = player->GetCorpseLocation().GetMapId();
                do
                {
                    if (corpseMap == at->Loc.GetMapId())
                        break;

                    InstanceTemplate const* corpseInstance = sObjectMgr->GetInstanceTemplate(corpseMap);
                    corpseMap = corpseInstance ? corpseInstance->Parent : 0;
                } while (corpseMap);

                if (!corpseMap)
                {
                    SendPacket(WorldPackets::AreaTrigger::AreaTriggerNoCorpse().Write());
                    return;
                }

                TC_LOG_DEBUG("maps", "MAP: Player '{}' has corpse in instance {} and can enter.", player->GetName(), at->Loc.GetMapId());
            }
            else
            {
                TC_LOG_DEBUG("maps", "Map::CanPlayerEnter - player '{}' is dead but does not have a corpse!", player->GetName());
                SendPacket(WorldPackets::AreaTrigger::AreaTriggerNoCorpse().Write());
                return;
            }
        }

        if (TransferAbortParams denyReason = Map::PlayerCannotEnter(at->Loc.GetMapId(), player))
        {
            switch (denyReason.Reason)
            {
                case TRANSFER_ABORT_MAP_NOT_ALLOWED:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' attempted to enter map with id {} which has no entry", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_DIFFICULTY:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' attempted to enter instance map {} but the requested difficulty was not found", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_NEED_GROUP:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' must be in a raid group to enter map {}", player->GetName(), at->Loc.GetMapId());
                    player->SendRaidGroupOnlyMessage(RAID_GROUP_ERR_ONLY, 0);
                    break;
                case TRANSFER_ABORT_LOCKED_TO_DIFFERENT_INSTANCE:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because their permanent bind is incompatible with their group's", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_ALREADY_COMPLETED_ENCOUNTER:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because their permanent bind is incompatible with their group's", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_TOO_MANY_INSTANCES:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because he has exceeded the maximum number of instances per hour.", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_MAX_PLAYERS:
                    break;
                case TRANSFER_ABORT_ZONE_IN_COMBAT:
                    break;
                case TRANSFER_ABORT_NOT_FOUND:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because instance is resetting.", player->GetName(), at->Loc.GetMapId());
                    break;
                default:
                    break;
            }

            if (denyReason.Reason != TRANSFER_ABORT_NEED_GROUP)
                player->SendTransferAborted(at->Loc.GetMapId(), denyReason.Reason, denyReason.Arg, denyReason.MapDifficultyXConditionId);

            if (!player->IsAlive() && player->HasCorpse())
            {
                if (player->GetCorpseLocation().GetMapId() == at->Loc.GetMapId())
                {
                    player->ResurrectPlayer(0.5f);
                    player->SpawnCorpseBones();
                }
            }

            return;
        }

        if (Group* group = player->GetGroup())
            if (group->isLFGGroup() && player->GetMap()->IsDungeon())
                teleported = player->TeleportToBGEntryPoint();
    }

    if (!teleported)
    {
        WorldSafeLocsEntry const* entranceLocation = player->GetInstanceEntrance(at->Loc.GetMapId());
        if (entranceLocation && player->GetMapId() != at->Loc.GetMapId())
            player->TeleportTo(entranceLocation->Loc, TELE_TO_NOT_LEAVE_TRANSPORT);
        else
            player->TeleportTo(at->Loc, TELE_TO_NOT_LEAVE_TRANSPORT);
    }
}

void WorldSession::HandleUpdateAccountData(WorldPackets::ClientConfig::UserClientUpdateAccountData& packet)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_UPDATE_ACCOUNT_DATA: type {}, time {}, decompressedSize {}",
        packet.DataType, packet.Time.AsUnderlyingType(), packet.Size);

    if (packet.DataType >= NUM_ACCOUNT_DATA_TYPES)
        return;

    if (packet.Size == 0)                               // erase
    {
        SetAccountData(AccountDataType(packet.DataType), 0, "");

        WorldPackets::ClientConfig::UpdateAccountDataComplete updateAccountDataComplete;
        updateAccountDataComplete.Player = packet.PlayerGuid;
        updateAccountDataComplete.DataType = packet.DataType;
        updateAccountDataComplete.Result = 0;
        SendPacket(updateAccountDataComplete.Write());

        return;
    }

    if (packet.Size > 0xFFFFFF)                         // MEDIUMBLOB cap (16 MB); modern addon/UI account data exceeds the old 0xFFFF BLOB limit
    {
        TC_LOG_ERROR("network", "UAD: Account data packet too big, size {}", packet.Size);
        return;
    }

    std::string dest;
    dest.resize(packet.Size);

    uLongf realSize = packet.Size;
    if (uncompress(reinterpret_cast<Bytef*>(dest.data()), &realSize, packet.CompressedData.data(), packet.CompressedData.size()) != Z_OK)
    {
        TC_LOG_ERROR("network", "UAD: Failed to decompress account data");
        return;
    }

    SetAccountData(AccountDataType(packet.DataType), packet.Time, dest);

    WorldPackets::ClientConfig::UpdateAccountDataComplete updateAccountDataComplete;
    updateAccountDataComplete.Player = packet.PlayerGuid;
    updateAccountDataComplete.DataType = packet.DataType;
    updateAccountDataComplete.Result = 0;
    SendPacket(updateAccountDataComplete.Write());
}

void WorldSession::HandleRequestAccountData(WorldPackets::ClientConfig::RequestAccountData& request)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_REQUEST_ACCOUNT_DATA: type {}", request.DataType);

    if (request.DataType >= NUM_ACCOUNT_DATA_TYPES)
        return;

    AccountData const* adata = GetAccountData(AccountDataType(request.DataType));

    WorldPackets::ClientConfig::UpdateAccountData data;
    data.Player = _player ? _player->GetGUID() : ObjectGuid::Empty;
    data.Time = adata->Time;
    data.Size = adata->Data.size();
    data.DataType = request.DataType;

    uLongf destSize = compressBound(data.Size);

    data.CompressedData.resize(destSize);

    if (data.Size && compress(data.CompressedData.data(), &destSize, (uint8 const*)adata->Data.c_str(), data.Size) != Z_OK)
    {
        TC_LOG_ERROR("network", "RAD: Failed to compress account data");
        return;
    }

    data.CompressedData.resize(destSize);

    SendPacket(data.Write());
}

void WorldSession::HandleSetActionButtonOpcode(WorldPackets::Spells::SetActionButton& packet)
{
    uint64 action = ACTION_BUTTON_ACTION(packet.Action);
    uint8 type = ACTION_BUTTON_TYPE(packet.Action);

    TC_LOG_DEBUG("network", "CMSG_SET_ACTION_BUTTON Button: {} Action: {} Type: {}", packet.Index, action, uint32(type));

    if (!packet.Action)
        GetPlayer()->RemoveActionButton(packet.Index);
    else
        GetPlayer()->AddActionButton(packet.Index, action, type);
}

void WorldSession::HandleCompleteCinematic(WorldPackets::Misc::CompleteCinematic& /*packet*/)
{
    // If player has sight bound to visual waypoint NPC we should remove it
    GetPlayer()->GetCinematicMgr()->EndCinematic();
}

void WorldSession::HandleNextCinematicCamera(WorldPackets::Misc::NextCinematicCamera& /*packet*/)
{
    // Sent by client when cinematic actually begun. So we begin the server side process
    GetPlayer()->GetCinematicMgr()->NextCinematicCamera();
}

void WorldSession::HandleCompleteMovie(WorldPackets::Misc::CompleteMovie& /*packet*/)
{
    uint32 movie = _player->GetMovie();
    if (!movie)
        return;

    _player->SetMovie(0);
    sScriptMgr->OnMovieComplete(_player, movie);
}

void WorldSession::HandleSetActionBarToggles(WorldPackets::Character::SetActionBarToggles& packet)
{
    if (!GetPlayer())                                        // ignore until not logged (check needed because STATUS_AUTHED)
    {
        if (packet.Mask != 0)
            TC_LOG_ERROR("network", "WorldSession::HandleSetActionBarToggles in not logged state with value: {}, ignored", uint32(packet.Mask));
        return;
    }

    GetPlayer()->SetMultiActionBars(packet.Mask);
}

void WorldSession::HandlePlayedTime(WorldPackets::Character::RequestPlayedTime& packet)
{
    WorldPackets::Character::PlayedTime playedTime;
    playedTime.TotalTime = _player->GetTotalPlayedTime();
    playedTime.LevelTime = _player->GetLevelPlayedTime();
    playedTime.TriggerEvent = packet.TriggerScriptEvent;  // 0-1 - will not show in chat frame
    SendPacket(playedTime.Write());
}

void WorldSession::HandleWhoIsOpcode(WorldPackets::Who::WhoIsRequest& packet)
{
    TC_LOG_DEBUG("network", "Received whois command from player {} for character {}",
        GetPlayer()->GetName(), packet.CharName);

    if (!HasPermission(rbac::RBAC_PERM_OPCODE_WHOIS))
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (packet.CharName.empty() || !normalizePlayerName(packet.CharName))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* player = ObjectAccessor::FindConnectedPlayerByName(packet.CharName);
    if (!player)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, packet.CharName.c_str());
        return;
    }

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_WHOIS);
    stmt->setUInt32(0, player->GetSession()->GetAccountId());

    PreparedQueryResult result = LoginDatabase.Query(stmt);
    if (!result)
    {
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, packet.CharName.c_str());
        return;
    }

    Field* fields = result->Fetch();
    std::string acc = fields[0].GetString();
    if (acc.empty())
        acc = "Unknown";

    std::string email = fields[1].GetString();
    if (email.empty())
        email = "Unknown";

    std::string lastip = fields[2].GetString();
    if (lastip.empty())
        lastip = "Unknown";

    WorldPackets::Who::WhoIsResponse response;
    response.AccountName = packet.CharName + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;
    SendPacket(response.Write());
}

void WorldSession::HandleFarSightOpcode(WorldPackets::Misc::FarSight& packet)
{
    if (packet.Enable)
    {
        TC_LOG_DEBUG("network", "Added FarSight {} to player {}", _player->m_activePlayerData->FarsightObject->ToString(), _player->GetGUID().ToString());
        if (WorldObject* target = _player->GetViewpoint())
            _player->SetSeer(target);
        else
            TC_LOG_DEBUG("network", "Player {} {} requests non-existing seer {}", _player->GetName(), _player->GetGUID().ToString(), _player->m_activePlayerData->FarsightObject->ToString());
    }
    else
    {
        TC_LOG_DEBUG("network", "Player {} set vision to self", _player->GetGUID().ToString());
        _player->SetSeer(_player);
    }

    GetPlayer()->UpdateVisibilityForPlayer();
}

void WorldSession::HandleSetTitleOpcode(WorldPackets::Character::SetTitle& packet)
{
    // -1 at none
    if (packet.TitleID > 0)
    {
       if (!GetPlayer()->HasTitle(packet.TitleID))
            return;
    }
    else
        packet.TitleID = 0;

    GetPlayer()->SetChosenTitle(packet.TitleID);
}

void WorldSession::HandleResetInstancesOpcode(WorldPackets::Instance::ResetInstances& /*packet*/)
{
    Map* map = _player->FindMap();
    if (map && map->Instanceable())
        return;

    if (Group* group = _player->GetGroup())
    {
        if (!group->IsLeader(_player->GetGUID()))
            return;

        if (group->isLFGGroup())
            return;

        group->ResetInstances(InstanceResetMethod::Manual, _player);
    }
    else
        _player->ResetInstances(InstanceResetMethod::Manual);
}

void WorldSession::HandleSetDungeonDifficultyOpcode(WorldPackets::Misc::SetDungeonDifficulty& setDungeonDifficulty)
{
    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(setDungeonDifficulty.DifficultyID);
    if (!difficultyEntry)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an invalid instance mode {}!",
            _player->GetGUID().ToString(), setDungeonDifficulty.DifficultyID);
        return;
    }

    if (difficultyEntry->InstanceType != MAP_INSTANCE)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an non-dungeon instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT))
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player {} sent unselectable instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    // cannot reset while in an instance
    Map* map = _player->FindMap();
    if (map && map->Instanceable())
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player (Name: {}, {}) tried to reset the instance while player is inside!",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    Difficulty difficultyID = Difficulty(difficultyEntry->ID);

    Group* group = _player->GetGroup();
    if (group)
    {
        if (difficultyID == group->GetDungeonDifficultyID())
            return;

        if (!group->IsLeader(_player->GetGUID()))
            return;

        if (group->isLFGGroup())
        {
            // Refusing without a word leaves the difficulty button looking broken. The client has
            // one error for exactly this condition - ERR_DIFFICULTY_DISABLED_IN_LFG, result 11 in
            // its own game-error table - so say so instead of returning silently.
            SendPacket(WorldPackets::Misc::ChangePlayerDifficultyResult(
                WorldPackets::Misc::ChangePlayerDifficultyResultCode::DisabledInLFG).Write());
            return;
        }

        // the difficulty is set even if the instances can't be reset
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, _player);
        group->SetDungeonDifficultyID(difficultyID);
    }

    if (difficultyID == _player->GetDungeonDifficultyID())
        return;

    if (!group)
        _player->ResetInstances(InstanceResetMethod::OnChangeDifficulty);

    _player->SetDungeonDifficultyID(difficultyID);
    _player->SendDungeonDifficulty();
}

void WorldSession::HandleSetRaidDifficultyOpcode(WorldPackets::Misc::SetRaidDifficulty& setRaidDifficulty)
{
    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(setRaidDifficulty.DifficultyID);
    if (!difficultyEntry)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an invalid instance mode {}!",
            _player->GetGUID().ToString(), setRaidDifficulty.DifficultyID);
        return;
    }

    if (difficultyEntry->InstanceType != MAP_RAID)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an non-dungeon instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT))
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player {} sent unselectable instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    if (((difficultyEntry->Flags & DIFFICULTY_FLAG_LEGACY) != 0) != setRaidDifficulty.Legacy)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent not matching legacy difficulty {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    // cannot reset while in an instance
    Map* map = _player->FindMap();
    if (map && map->Instanceable())
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetRaidDifficultyOpcode: player (Name: {}, {}) tried to reset the instance while player is inside!",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    Difficulty difficultyID = Difficulty(difficultyEntry->ID);

    Group* group = _player->GetGroup();
    if (group)
    {
        if (difficultyID == (setRaidDifficulty.Legacy ? group->GetLegacyRaidDifficultyID() : group->GetRaidDifficultyID()))
            return;

        if (!group->IsLeader(_player->GetGUID()))
            return;

        if (group->isLFGGroup())
        {
            // Same refusal as the dungeon path, same client-side error.
            SendPacket(WorldPackets::Misc::ChangePlayerDifficultyResult(
                WorldPackets::Misc::ChangePlayerDifficultyResultCode::DisabledInLFG).Write());
            return;
        }

        // the difficulty is set even if the instances can't be reset
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, _player);
        if (setRaidDifficulty.Legacy)
            group->SetLegacyRaidDifficultyID(difficultyID);
        else
            group->SetRaidDifficultyID(difficultyID);
    }

    if (difficultyID == (setRaidDifficulty.Legacy ? _player->GetLegacyRaidDifficultyID() : _player->GetRaidDifficultyID()))
        return;

    if (!group)
        _player->ResetInstances(InstanceResetMethod::OnChangeDifficulty);

    if (setRaidDifficulty.Legacy)
        _player->SetLegacyRaidDifficultyID(difficultyID);
    else
        _player->SetRaidDifficultyID(difficultyID);

    _player->SendRaidDifficulty(setRaidDifficulty.Legacy != 0);
}

void WorldSession::HandleSetDifficultyID(WorldPackets::Misc::SetDifficultyID& setDifficultyID)
{
    // Interactive 12.x difficulty change. Unlike the out-of-instance dropdown (CMSG_SET_DUNGEON_DIFFICULTY /
    // CMSG_SET_RAID_DIFFICULTY, which reply with the SMSG_SET_DUNGEON_DIFFICULTY field update), this opcode
    // carries only the Difficulty.db2 id and the client waits for SMSG_CHANGE_PLAYER_DIFFICULTY_RESULT - the
    // rich result carrying either Success (with the applied MapID + DifficultyID) or one of the client's own
    // ERR_DIFFICULTY_CHANGE_* refusal codes. Wire + pairing recovered from the 12.1.0.69497 client (writer
    // RVA 0x1406CC920, single int16 DifficultyID) and the two captured result bodies (Success / Pending);
    // upstream leaves both this CMSG and the result SMSG unhandled.
    using ResultCode = WorldPackets::Misc::ChangePlayerDifficultyResultCode;

    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(setDifficultyID.DifficultyID);
    if (!difficultyEntry)
        return;

    // Only selectable dungeon/raid difficulties are meaningful; the client never offers others.
    if (difficultyEntry->InstanceType != MAP_INSTANCE && difficultyEntry->InstanceType != MAP_RAID)
        return;
    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT))
        return;

    auto sendResult = [this](ResultCode code)
    {
        SendPacket(WorldPackets::Misc::ChangePlayerDifficultyResult(code).Write());
    };

    // Refusals whose meaning is the client's own error code verbatim (no guessing): the client shows
    // ERR_DIFFICULTY_CHANGE_COMBAT / ERR_DIFFICULTY_DISABLED_IN_LFG for exactly these conditions.
    if (_player->IsInCombat())
    {
        sendResult(ResultCode::Combat);
        return;
    }

    bool const isRaid = difficultyEntry->InstanceType == MAP_RAID;
    bool const legacy = (difficultyEntry->Flags & DIFFICULTY_FLAG_LEGACY) != 0;
    Difficulty const difficultyID = Difficulty(difficultyEntry->ID);

    Group* group = _player->GetGroup();
    if (group)
    {
        if (group->isLFGGroup())
        {
            sendResult(ResultCode::DisabledInLFG);
            return;
        }

        // Only the leader changes the group's difficulty; the client gates this and sends no error on the
        // wire for a non-leader, so mirror that with a silent no-op rather than an invented result code.
        if (!group->IsLeader(_player->GetGUID()))
            return;
    }

    // TrinityCore only sets the difficulty of future instances; it cannot safely retune a map the player is
    // already inside without desyncing it. That is the one condition whose exact retail result code is not
    // recoverable offline, so rather than emit a guessed code we decline silently (the change simply does not
    // happen, which is truthful) - see the same guard in HandleSetDungeonDifficultyOpcode.
    if (Map* map = _player->FindMap(); map && map->Instanceable())
        return;

    // Already the active difficulty: acknowledge it but do not run the instance reset again (that would
    // needlessly wipe saved progress on a redundant re-select).
    Difficulty const current = isRaid ? (legacy ? _player->GetLegacyRaidDifficultyID() : _player->GetRaidDifficultyID())
                                      : _player->GetDungeonDifficultyID();
    if (difficultyID == current)
    {
        WorldPackets::Misc::ChangePlayerDifficultyResult unchanged(ResultCode::Success);
        unchanged.MapID = _player->GetMapId();
        unchanged.DifficultyID = uint16(difficultyID);
        SendPacket(unchanged.Write());
        return;
    }

    // Apply - identical state changes to the dropdown handlers.
    if (group)
    {
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, _player);
        if (isRaid)
            legacy ? group->SetLegacyRaidDifficultyID(difficultyID) : group->SetRaidDifficultyID(difficultyID);
        else
            group->SetDungeonDifficultyID(difficultyID);
    }
    else
    {
        _player->ResetInstances(InstanceResetMethod::OnChangeDifficulty);
        if (isRaid)
            legacy ? _player->SetLegacyRaidDifficultyID(difficultyID) : _player->SetRaidDifficultyID(difficultyID);
        else
            _player->SetDungeonDifficultyID(difficultyID);
    }

    // Canonical difficulty field the client consumes in every context (login, group join, change) ...
    if (isRaid)
        _player->SendRaidDifficulty(legacy);
    else
        _player->SendDungeonDifficulty();

    // ... and the interactive confirmation this opcode is waiting on. Success carries the applied map +
    // difficulty; the client stores DifficultyID for MapID when it is the map it is on.
    WorldPackets::Misc::ChangePlayerDifficultyResult result(ResultCode::Success);
    result.MapID = _player->GetMapId();
    result.DifficultyID = uint16(difficultyID);
    SendPacket(result.Write());
}

void WorldSession::HandleSetTaxiBenchmark(WorldPackets::Misc::SetTaxiBenchmarkMode& packet)
{
    if (packet.Enable)
        _player->SetPlayerFlag(PLAYER_FLAGS_TAXI_BENCHMARK);
    else
        _player->RemovePlayerFlag(PLAYER_FLAGS_TAXI_BENCHMARK);
}

void WorldSession::HandleGuildSetFocusedAchievement(WorldPackets::Achievement::GuildSetFocusedAchievement& setFocusedAchievement)
{
    if (Guild* guild = sGuildMgr->GetGuildById(_player->GetGuildId()))
        guild->GetAchievementMgr().SendAchievementInfo(_player, setFocusedAchievement.AchievementID);
}

void WorldSession::HandleServerTimeOffsetRequest(WorldPackets::Misc::ServerTimeOffsetRequest& /*request*/)
{
    WorldPackets::Misc::ServerTimeOffset response;
    response.Time = GameTime::GetSystemTime();
    SendPacket(response.Write());
}

void WorldSession::HandleInstanceLockResponse(WorldPackets::Instance::InstanceLockResponse& packet)
{
    if (!_player->HasPendingBind())
    {
        TC_LOG_INFO("network", "InstanceLockResponse: Player {} {} tried to bind himself/teleport to graveyard without a pending bind!",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    if (packet.AcceptLock)
        _player->ConfirmPendingBind();
    else
        _player->RepopAtGraveyard();

    _player->SetPendingBind(0, 0);
}

void WorldSession::HandleViolenceLevel(WorldPackets::Misc::ViolenceLevel& /*violenceLevel*/)
{
    // do something?
}

// CMSG_GET_CHARACTER_CURRENCY_TRANSFER_LOG (empty): the client opened the account/warband currency transfer-history
// panel. Answer with SMSG_CURRENCY_TRANSFER_LOG. TrinityCore does not implement account currency transfer, so there
// (CMSG_GET_CHARACTER_CURRENCY_TRANSFER_LOG and CMSG_REQUEST_CURRENCY_DATA_FOR_ACCOUNT_CHARACTERS
//  are implemented in CurrencyHandler.cpp with the real warband DB round-trip; the earlier empty-reply
//  stubs here were removed to avoid duplicate definitions.)

void WorldSession::HandleObjectUpdateFailedOpcode(WorldPackets::Misc::ObjectUpdateFailed& objectUpdateFailed)
{
    TC_LOG_ERROR("network", "Object update failed for {} for player {} ({})", objectUpdateFailed.ObjectGUID.ToString(), GetPlayerName(), _player->GetGUID().ToString());

    // If create object failed for current player then client will be stuck on loading screen
    if (_player->GetGUID() == objectUpdateFailed.ObjectGUID)
    {
        LogoutPlayer(true);
        return;
    }

    // Pretend we've never seen this object
    _player->m_clientGUIDs.erase(objectUpdateFailed.ObjectGUID);
}

void WorldSession::HandleObjectUpdateRescuedOpcode(WorldPackets::Misc::ObjectUpdateRescued& objectUpdateRescued)
{
    TC_LOG_ERROR("network", "Object update rescued for {} for player {} ({})", objectUpdateRescued.ObjectGUID.ToString(), GetPlayerName(), _player->GetGUID().ToString());

    // Client received values update after destroying object
    // re-register object in m_clientGUIDs to send DestroyObject on next visibility update
    _player->m_clientGUIDs.insert(objectUpdateRescued.ObjectGUID);
}

void WorldSession::HandleSaveCUFProfiles(WorldPackets::Misc::SaveCUFProfiles& packet)
{
    if (packet.CUFProfiles.size() > MAX_CUF_PROFILES)
    {
        TC_LOG_ERROR("entities.player", "HandleSaveCUFProfiles - {} tried to save more than {} CUF profiles. Hacking attempt?", GetPlayerName(), MAX_CUF_PROFILES);
        return;
    }

    for (uint8 i = 0; i < packet.CUFProfiles.size(); ++i)
        GetPlayer()->SaveCUFProfile(i, std::move(packet.CUFProfiles[i]));

    for (uint8 i = packet.CUFProfiles.size(); i < MAX_CUF_PROFILES; ++i)
        GetPlayer()->SaveCUFProfile(i, nullptr);
}

void WorldSession::SendLoadCUFProfiles()
{
    Player* player = GetPlayer();

    WorldPackets::Misc::LoadCUFProfiles loadCUFProfiles;

    for (uint8 i = 0; i < MAX_CUF_PROFILES; i++)
        if (CUFProfile* cufProfile = player->GetCUFProfile(i))
            loadCUFProfiles.CUFProfiles.push_back(cufProfile);
    SendPacket(loadCUFProfiles.Write());
}

void WorldSession::HandleSetAdvancedCombatLogging(WorldPackets::ClientConfig::SetAdvancedCombatLogging& setAdvancedCombatLogging)
{
    _player->SetAdvancedCombatLogging(setAdvancedCombatLogging.Enable);
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPackets::Misc::MountSpecial& mountSpecial)
{
    WorldPackets::Misc::SpecialMountAnim specialMountAnim;
    specialMountAnim.UnitGUID = _player->GetGUID();
    std::copy(mountSpecial.SpellVisualKitIDs.begin(), mountSpecial.SpellVisualKitIDs.end(), std::back_inserter(specialMountAnim.SpellVisualKitIDs));
    specialMountAnim.SequenceVariation = mountSpecial.SequenceVariation;
    GetPlayer()->SendMessageToSet(specialMountAnim.Write(), false);
}

void WorldSession::HandleMountSetFavorite(WorldPackets::Misc::MountSetFavorite& mountSetFavorite)
{
    _collectionMgr->MountSetFavorite(mountSetFavorite.MountSpellID, mountSetFavorite.IsFavorite);
}

void WorldSession::HandleMountClearFanfare(WorldPackets::Misc::MountClearFanfare& mountClearFanfare)
{
    _collectionMgr->MountClearFanfare(mountClearFanfare.MountSpellID);
}
void WorldSession::HandleCloseInteraction(WorldPackets::Misc::CloseInteraction& closeInteraction)
{
    InteractionData& interactionData = _player->PlayerTalkClass->GetInteractionData();

    if (interactionData.PendingAutoLaunchedQuestId)
    {
        if (interactionData.Type != PlayerInteractionType::QuestGiver
            || interactionData.SourceGuid != closeInteraction.SourceGuid)
        {
            TC_LOG_DEBUG("network", "CMSG_CLOSE_INTERACTION pending quest {} - SourceGuid mismatch (offer={} close={}), clearing",
                interactionData.PendingAutoLaunchedQuestId,
                interactionData.SourceGuid.ToString(), closeInteraction.SourceGuid.ToString());
        }

        interactionData.ClearPendingAutoLaunchedQuest(_player);
    }

    if (_player->PlayerTalkClass->GetInteractionData().IsLaunchedByQuest)
        _player->PlayerTalkClass->GetInteractionData().IsLaunchedByQuest = false;
    else if (_player->PlayerTalkClass->GetInteractionData().SourceGuid == closeInteraction.SourceGuid)
        _player->PlayerTalkClass->GetInteractionData().Reset();

    if (_player->GetStableMaster() == closeInteraction.SourceGuid)
        _player->SetStableMaster(ObjectGuid::Empty);
}

void WorldSession::HandleCloseRuneforgeInteraction(WorldPackets::Misc::CloseRuneforgeInteraction& /*closeRuneforgeInteraction*/)
{
    // Empty wire: only clear the interaction if the player is actually in the runeforge (legendary crafting) window,
    // so an unrelated concurrent interaction is never clobbered.
    if (_player->PlayerTalkClass->GetInteractionData().Type == PlayerInteractionType::LegendaryCrafting)
        _player->PlayerTalkClass->GetInteractionData().Reset();
}

void WorldSession::HandleCloseTraitSystemInteraction(WorldPackets::Misc::CloseTraitSystemInteraction& /*closeTraitSystemInteraction*/)
{
    if (_player->PlayerTalkClass->GetInteractionData().Type == PlayerInteractionType::TraitSystem)
        _player->PlayerTalkClass->GetInteractionData().Reset();
}

void WorldSession::HandleConversationLineStarted(WorldPackets::Misc::ConversationLineStarted& conversationLineStarted)
{
    if (Conversation* conversation = ObjectAccessor::GetConversation(*_player, conversationLineStarted.ConversationGUID))
        conversation->AI()->OnLineStarted(conversationLineStarted.LineID, _player);
}

void WorldSession::HandleConversationCinematicReady(WorldPackets::Misc::ConversationCinematicReady& conversationCinematicReady)
{
    Conversation* conversation = ObjectAccessor::GetConversation(*_player, conversationCinematicReady.ConversationGUID);
    if (!conversation)
        return;

    // Conversations are private objects owned by the player they play for, so only that owner can
    // meaningfully report its cinematic ready - the same ownership rule HandleSetStopConversation
    // applies. Without it one player could drive another player's conversation script.
    if (conversation->GetPrivateObjectOwner() != _player->GetGUID())
        return;

    conversation->AI()->OnCinematicReady(_player);
}

void WorldSession::HandleRequestLatestSplashScreen(WorldPackets::Misc::RequestLatestSplashScreen& /*requestLatestSplashScreen*/)
{
    UISplashScreenEntry const* splashScreen = nullptr;
    for (auto itr = sUISplashScreenStore.begin(); itr != sUISplashScreenStore.end(); ++itr)
    {
        if (!ConditionMgr::IsPlayerMeetingCondition(_player, itr->CharLevelConditionID))
            continue;

        splashScreen = *itr;
    }

    WorldPackets::Misc::SplashScreenShowLatest splashScreenShowLatest;
    splashScreenShowLatest.UISplashScreenID = splashScreen ? splashScreen->ID : 0;
    SendPacket(splashScreenShowLatest.Write());
}

void WorldSession::HandleQueryCountdownTimer(WorldPackets::Misc::QueryCountdownTimer& queryCountdownTimer)
{
    Group const* group = _player->GetGroup();
    if (!group)
        return;

    Group::CountdownInfo const* info = group->GetCountdownInfo(queryCountdownTimer.TimerType);
    if (!info)
        return;

    WorldPackets::Misc::StartTimer startTimer;
    startTimer.Type = queryCountdownTimer.TimerType;
    startTimer.TimeLeft = info->GetTimeLeft();
    startTimer.TotalTime = info->GetTotalTime();

    _player->SendDirectMessage(startTimer.Write());
}

void WorldSession::HandleDoCountdown(WorldPackets::Misc::DoCountdown& doCountdown)
{
    // The native raid/party pull countdown: the leader starts it, the server runs it and pushes the
    // ticking timer to every group member (SMSG_START_TIMER), mirroring HandleQueryCountdownTimer's reply.
    Group* group = _player->GetGroup();
    if (!group)
        return;

    if (!group->IsLeader(_player->GetGUID()))
        return;

    Seconds duration(doCountdown.TotalTime);
    if (duration <= Seconds::zero() || duration > Seconds(600))
        return;

    // A player-initiated countdown is always the PlayerCountdown slot; Pvp/ChallengeMode are server-driven.
    group->StartCountdown(CountdownTimerType::PlayerCountdown, duration);

    WorldPackets::Misc::StartTimer startTimer;
    startTimer.Type = CountdownTimerType::PlayerCountdown;
    startTimer.TotalTime = duration;
    startTimer.TimeLeft = duration;
    group->BroadcastPacket(startTimer.Write(), false);
}

void WorldSession::HandleGetRemainingGameTime(WorldPackets::Misc::GetRemainingGameTime& /*getRemainingGameTime*/)
{
    // Trinity accounts are not subscription-time-billed; report unlimited so the client clears the
    // remaining-game-time UI instead of nagging.
    WorldPackets::Misc::GetRemainingGameTimeResponse response;
    response.SecondsRemaining = 0;
    response.GameTimeParam = 0;
    response.Unlimited = true;
    SendPacket(response.Write());
}

void WorldSession::HandleSetStopConversation(WorldPackets::Misc::SetStopConversation& setStopConversation)
{
    Conversation* conversation = ObjectAccessor::GetConversation(*_player, setStopConversation.ConversationGUID);
    if (!conversation)
        return;

    // Conversations are private objects owned by the player they play for; only that owner may stop one.
    if (conversation->GetPrivateObjectOwner() == _player->GetGUID())
        conversation->Remove();
}

void WorldSession::HandleSetCurrencyFlags(WorldPackets::Misc::SetCurrencyFlags const& setCurrenctFlags)
{
    _player->SetCurrencyFlagsFromClient(setCurrenctFlags.CurrencyID, setCurrenctFlags.Flags);
}

// ================================================================================================
// Einheit w4_cmsg_43_3D - Sendeseite der Sammelfamilien 0x43 / 0x3D, Phase A.
// Track A, B3 (Telemetrie) und B11 (Warden3). B4 liegt in BattlenetHandler.cpp, B5 in
// LiveRegionHandler.cpp, B6/B7 in LobbyMatchmakerHandler.cpp, B8 in BleepHandler.cpp,
// B9 in VoiceChatHandler.cpp.
//
// Zur Ehrlichkeit dieser Datei: ein Teil dieser Opcodes gehoert zu Systemen, die TrinityCore
// nicht hat - Zuschauermodus, Inselexpeditionen, Runenschmiede, Kontokosmetik, QuickJoin-Umlauf.
// Fuer die ist der Handler bewusst NUR Eingangspruefung und Protokoll, und in
// orchestrierung/status/w4_cmsg_43_3D.json steht D2 dafuer auf "offen", nicht auf "ok".
// Eine erfundene Wirkung waere nach DEFINITION_OF_DONE_pro_opcode.md Abschnitt 1 der teurere
// Fehler: sie funktioniert im Test und weicht still von Retail ab.
// ================================================================================================

// --- Track A: reine Notify ohne Gegenstueck -----------------------------------------------------

// 0x3D0032, Writer 0x6CBEA0, leere Nutzlast.
// UNVERIFIED: es gibt kein Gegenstueck und keinen Konsumenten - die Retail-Serverwirkung ist aus
// dem Binary nicht ableitbar, weil die Nachricht nur in eine Richtung geht.
void WorldSession::HandleUsedFollow(WorldPackets::Misc::UsedFollow& /*usedFollow*/)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_USED_FOLLOW from {}", GetPlayerInfo());
}

// 0x3D02DB, Writer 0x6D29C0, leere Nutzlast.
// Abschlussvermerk des nahtlosen Kartenwechsels. Im Baum laeuft die Transferquittierung
// vollstaendig ueber MSG_MOVE_WORLDPORT_ACK.
// UNVERIFIED: kein Gegenstueck, kein Konsument.
void WorldSession::HandleSeamlessTransferComplete(WorldPackets::Misc::SeamlessTransferComplete& /*seamlessTransferComplete*/)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SEAMLESS_TRANSFER_COMPLETE from {}", GetPlayerInfo());
}

// 0x3D00BA, Writer 0x6CD470, leere Nutzlast. Gegenstueck SMSG_CHALLENGE_MODE_RESET.
// Cheat-Opcode. Retail gibt ihn ueber den Entwicklerclient frei; ein oeffentlicher Realm darf das
// nicht ungeprueft durchlassen - dieselbe Begruendung wie bei CMSG_KIOSK_ENABLE_GOD_MODE auf
// feature/gm-tools. Es wird bewusst NICHT geantwortet: die Struktur von SMSG_CHALLENGE_MODE_RESET
// ist nicht nachgelesen, und ein geratenes Antwortpaket ist schlimmer als keines (DoD Abschnitt 1,
// "falsch befuellte Antwort" - der Client verwirft still).
void WorldSession::HandleResetChallengeModeCheat(WorldPackets::Misc::ResetChallengeModeCheat& /*resetChallengeModeCheat*/)
{
    if (!HasPermission(rbac::RBAC_PERM_COMMAND_DEBUG))
    {
        TC_LOG_INFO("entities.player.cheat", "{} tried to reset the challenge mode timer without permission", GetPlayerInfo());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_RESET_CHALLENGE_MODE_CHEAT from {} - no challenge mode system in this tree", GetPlayerInfo());
}

// --- Track A: Eingangspruefung fuer Systeme, die der Baum nicht hat ------------------------------

// 0x3D0171, Writer 0x6CEF70, gepackte ObjectGuid. Gegenstueck SMSG_ACCOUNT_COSMETIC_ADDED.
// Der Baum hat keinen Kontokosmetik-Speicher; AccountStoreItem / AccountStoreCategory sind
// DB2-Tabellen ohne Serverseite. Keine Antwort - die Struktur des Gegenstuecks ist nicht gelesen.
void WorldSession::HandleAddAccountCosmetic(WorldPackets::Misc::AddAccountCosmetic& addAccountCosmetic)
{
    if (!_player->GetItemByGuid(addAccountCosmetic.ItemGUID))
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_ADD_ACCOUNT_COSMETIC from {} for item {} which the player does not own",
            GetPlayerInfo(), addAccountCosmetic.ItemGUID.ToString());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_ADD_ACCOUNT_COSMETIC from {} item {} - no account cosmetic store in this tree",
        GetPlayerInfo(), addAccountCosmetic.ItemGUID.ToString());
}

// 0x3D01E6, Writer 0x6D0550, gepackte ObjectGuid des Handwerks-NPC.
// Datenbasis waere NPCCraftingOrderSet / NPCCraftingOrderSetXCraftOrder; der Baum haelt dafuer
// keinen Serverzustand.
void WorldSession::HandleUpdateCraftingNpcRecipes(WorldPackets::Misc::UpdateCraftingNpcRecipes& updateCraftingNpcRecipes)
{
    if (!_player->GetNPCIfCanInteractWith(updateCraftingNpcRecipes.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_NONE))
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_UPDATE_CRAFTING_NPC_RECIPES from {} for unreachable npc {}",
            GetPlayerInfo(), updateCraftingNpcRecipes.NpcGUID.ToString());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_UPDATE_CRAFTING_NPC_RECIPES from {} npc {} - no crafting order system in this tree",
        GetPlayerInfo(), updateCraftingNpcRecipes.NpcGUID.ToString());
}

// 0x3D0263, Writer 0x6D1710, guid + uint32.
// Lua C_IslandsQueue.QueueForIsland(difficultyID); die GUID ist der Warteschlangen-NPC. Die
// Antwort laeuft im Retail ueber den LFG-Warteschlangenkanal, nicht ueber ein eigenes Gegenstueck.
void WorldSession::HandleIslandQueue(WorldPackets::Misc::IslandQueue& islandQueue)
{
    if (!_player->GetNPCIfCanInteractWith(islandQueue.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_NONE))
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_ISLAND_QUEUE from {} for unreachable npc {}",
            GetPlayerInfo(), islandQueue.NpcGUID.ToString());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_ISLAND_QUEUE from {} difficulty {} - no island expedition system in this tree",
        GetPlayerInfo(), islandQueue.DifficultyID);
}

// 0x4300F1, Writer 0x6ABE20, guid + uint32 + uint32.
// Ausgeloest durch Klick auf einen "trade:"-Chat-Hyperlink (ItemRef.lua:46), nicht durch eine
// C_-Funktion. Gegenstueck SMSG_SHOW_TRADE_SKILL_RESPONSE, dessen Struktur nicht gelesen ist.
void WorldSession::HandleShowTradeSkill(WorldPackets::Misc::ShowTradeSkill& showTradeSkill)
{
    if (!ObjectAccessor::FindConnectedPlayer(showTradeSkill.PlayerGUID))
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_SHOW_TRADE_SKILL from {} for offline player {}",
            GetPlayerInfo(), showTradeSkill.PlayerGUID.ToString());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_SHOW_TRADE_SKILL from {} target {} spell {} skillLine {}",
        GetPlayerInfo(), showTradeSkill.PlayerGUID.ToString(), showTradeSkill.SpellID, showTradeSkill.SkillLineID);
}

// 0x3D0291, Writer 0x6D1B90, uint32 + vier uint8.
// Lua C_LegendaryCrafting.UpgradeRuneforgeLegendary(runeforgeLegendary: ItemLocation,
// upgradeItem: ItemLocation) - die vier uint8 sind zwei ItemLocations (Tasche, Platz).
// Der Baum hat den Shadowlands-Runenschnitzer nicht; das Ergebnis kaeme im Retail ueber ein
// Item-Update, nicht ueber ein eigenes Gegenstueck.
void WorldSession::HandleUpgradeRuneforgeLegendary(WorldPackets::Misc::UpgradeRuneforgeLegendary& upgradeRuneforgeLegendary)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_UPGRADE_RUNEFORGE_LEGENDARY from {} ({}: {}/{} -> {}/{}) - no runeforge system in this tree",
        GetPlayerInfo(), upgradeRuneforgeLegendary.Field0,
        upgradeRuneforgeLegendary.LegendaryBagSlot, upgradeRuneforgeLegendary.LegendarySlot,
        upgradeRuneforgeLegendary.UpgradeItemBagSlot, upgradeRuneforgeLegendary.UpgradeItemSlot);
}

// 0x43007E, Writer 0x6A83E0. Hausmuster der 0x43-Gruppenopcodes (Praesenzbit zuerst).
// Lua ChannelSetPartyMemberSilent(partyMemberName, silenceOn) -> Ereignis
// VOICE_CHAT_CHANNEL_MEMBER_SILENCED_CHANGED. Die Stummschaltung gehoert zum Sprachchat; der Baum
// hat kein Sprach-Backend (Block B9), es gibt also keinen Zustand zu setzen.
void WorldSession::HandleSilenceTalkerInParty(WorldPackets::Misc::SilenceTalkerInParty& silenceTalkerInParty)
{
    Group* group = _player->GetGroup();
    if (!group || !group->IsMember(silenceTalkerInParty.Target))
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_SILENCE_PARTY_TALKER from {} for {} who is not in the group",
            GetPlayerInfo(), silenceTalkerInParty.Target.ToString());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_SILENCE_PARTY_TALKER from {} target {} silence {} - no voice backend in this tree",
        GetPlayerInfo(), silenceTalkerInParty.Target.ToString(), silenceTalkerInParty.Silence);
}

// 0x43000B, Writer 0x6A2150. Zwei ausgeschriebene Spielerbloecke, dann QueueID und EIN Bit.
// Gegenstueck SMSG_CHECK_WARGAME_ENTRY (StaticPopup-Dialog 63, Zusage per
// CMSG_ACCEPT_WARGAME_INVITE). Die Kriegsspielbasis liegt auf feature/war-games, wo der
// Zuschauerpfad ausdruecklich an das Commentator-System zurueckgestellt ist. Hier wird deshalb
// nicht geantwortet - eine zweite Kriegsspiel-Wahrheit neben HandleStartWarGame waere genau der
// Fehler, den CMSG_DELVE_TELEPORT_OUT dokumentiert.
void WorldSession::HandleStartSpectatorWarGame(WorldPackets::Misc::StartSpectatorWarGame& startSpectatorWarGame)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_START_SPECTATOR_WAR_GAME from {} queue {} tournamentRules {} - war game base lives on feature/war-games",
        GetPlayerInfo(), startSpectatorWarGame.QueueID, startSpectatorWarGame.TournamentRules);
}

// 0x3D02D3 / 0x3D02D4 / 0x3D02D5, Writer 0x6D2650 / 0x6D26C0 / 0x6D2710.
// Lua C_SpectatingUI.SpectateChange(nextTarget) bzw. LeaveSpectateMode().
//
// Befund fuer die spaetere Umsetzung: SMSG_SPECTATE_NEXT ist eine AUFFORDERUNG, keine Antwort.
// Der Server sagt nur "weiter"; der Client sucht das Ziel selbst aus (Konsument 0x22C61D0) und
// meldet es mit CMSG_SPECTATE_SET_NEXT_TARGET zurueck - oder beendet mit CMSG_SPECTATE_END, wenn
// die Zielliste leer ist. Ein Server, der SMSG_SPECTATE_NEXT schickt und danach nichts erwartet,
// verpasst die Antwort.
// Track A hat kein Fehlercode-Enum: Callsite-Scan auf die GameError-Anzeige 0x209AD90 ueber
// 0x22C5A00..0x22C6A00 und 0x21C1210 ergibt null Treffer. Der einzige Rueckkanal fuer Fehler ist
// die ABWESENHEIT von SMSG_SPECTATE_PLAYER bzw. ein SMSG_SPECTATE_RESET.
void WorldSession::HandleSpectateChange(WorldPackets::Misc::SpectateChange& spectateChange)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SPECTATE_CHANGE from {} nextTarget {} - no spectator system in this tree",
        GetPlayerInfo(), spectateChange.NextTarget);
}

void WorldSession::HandleSpectateSetNextTarget(WorldPackets::Misc::SpectateSetNextTarget& spectateSetNextTarget)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SPECTATE_SET_NEXT_TARGET from {} target {} - no spectator system in this tree",
        GetPlayerInfo(), spectateSetNextTarget.Target.ToString());
}

void WorldSession::HandleSpectateEnd(WorldPackets::Misc::SpectateEnd& /*spectateEnd*/)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SPECTATE_END from {} - no spectator system in this tree", GetPlayerInfo());
}

// 0x43012F / 0x430130 / 0x430131 / 0x430160.
// QuickJoin haengt vollstaendig an C_SocialQueue, also am Battle.net-Praesenzdienst: es gibt im
// 12.1-Katalog KEINEN SMSG_QUICK_JOIN_*, ueber den der Weltserver den Gruppenleiter
// benachrichtigen koennte. Der Umlauf Anfrage -> Leiter benachrichtigen -> antworten ist im
// Weltserver nicht herstellbar. Diese drei Umlaufopcodes werden deshalb geprueft und
// protokolliert, nicht beantwortet. CMSG_QUICK_JOIN_AUTO_ACCEPT_REQUESTS (Einstellung) und
// CMSG_QUICK_JOIN_SIGNAL_TOAST_DISPLAYED (Telemetrie) sind davon NICHT betroffen.
//
// UNVERIFIED: die Serverwirkung ist unbelegt. C_SocialQueue.SignalToastDisplayed erklaert den
// float Priority, nennt aber keinen Konsumenten auf der Serverseite; dass Retail die Meldung nur
// verbucht, ist Vermutung. D2 = offen.
void WorldSession::HandleQuickJoinSignalToastDisplayed(WorldPackets::Misc::QuickJoinSignalToastDisplayed& quickJoinSignalToastDisplayed)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_QUICK_JOIN_SIGNAL_TOAST_DISPLAYED from {} group {} priority {} members {}",
        GetPlayerInfo(), quickJoinSignalToastDisplayed.GroupGUID.ToString(),
        quickJoinSignalToastDisplayed.Priority, quickJoinSignalToastDisplayed.Members.size());
}

void WorldSession::HandleQuickJoinRequestInvite(WorldPackets::Misc::QuickJoinRequestInvite& quickJoinRequestInvite)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_QUICK_JOIN_REQUEST_INVITE from {} group {} target '{}'-'{}' roles {} - round trip runs over Bnet presence",
        GetPlayerInfo(), quickJoinRequestInvite.GroupGUID.ToString(),
        quickJoinRequestInvite.TargetName, quickJoinRequestInvite.TargetRealm, quickJoinRequestInvite.Roles);
}

void WorldSession::HandleQuickJoinRequestInviteWithConfirmation(WorldPackets::Misc::QuickJoinRequestInviteWithConfirmation& quickJoinRequestInviteWithConfirmation)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_QUICK_JOIN_REQUEST_INVITE_WITH_CONFIRMATION from {} group {} request {} target '{}'-'{}' - round trip runs over Bnet presence",
        GetPlayerInfo(), quickJoinRequestInviteWithConfirmation.GroupGUID.ToString(),
        quickJoinRequestInviteWithConfirmation.RequestID,
        quickJoinRequestInviteWithConfirmation.TargetName, quickJoinRequestInviteWithConfirmation.TargetRealm);
}

// --- Track A: echte Zustandsaenderungen ---------------------------------------------------------

// 0x430132, Writer 0x6AFAD0, ein Bit. Am Draht belegt: 37 Pakete, konstant 1 Byte.
// Reine Einstellung ohne Umlauf, fluechtig: der Client haelt sie im UI und sendet sie nach jedem
// Login erneut - genau das zeigen die 37 Pakete ueber vier Builds.
//
// UNVERIFIED: der Wert wird gehalten, aber von nichts gelesen - der QuickJoin-Umlauf ist im
// Weltserver gar nicht herstellbar (siehe Block oben), also gibt es nichts, was die Einstellung
// steuern koennte. Sobald ein QuickJoin-Einladungspfad existiert, ist DIES das Feld, das ihn
// unterdrueckt. Bis dahin D2 = offen und ausdruecklich keine erfundene Wirkung.
void WorldSession::HandleQuickJoinAutoAcceptRequests(WorldPackets::Misc::QuickJoinAutoAcceptRequests& quickJoinAutoAcceptRequests)
{
    _quickJoinAutoAcceptRequests = quickJoinAutoAcceptRequests.AutoAccept;
}

// 0x4300CF, Writer 0x6AB1E0, ein Bit. Schalter "niedrigstufige Schlachtzuege betreten duerfen".
//
// D2 - der Traeger ist KEIN Sitzungsfeld, sondern ein Spielerflag, das der Baum bereits fuehrt:
// PLAYER_FLAGS_LOW_LEVEL_RAID_ENABLED (0x00010000). Es wird mit m_playerData->PlayerFlags nach
// characters.playerFlags geschrieben, beim Laden ueber ReplaceAllPlayerFlags zurueckgeholt und in
// der Charakterliste nach CHARACTER_FLAG_2_LOW_LEVEL_RAID_ENABLED gespiegelt
// (CharacterPackets.cpp: EnumCharactersResult::CharacterInfo). Wer den Wert stattdessen in ein
// Sitzungsfeld legt, gibt dem Client den Schalter nach dem Relog nicht zurueck.
//
// D5 - Quelle der Semantik: PlayerScript.SetAllowLowLevelRaid(allow) / GetAllowLowLevelRaid()
// (wow-ui-source Blizzard_APIDocumentationGenerated/PlayerScriptDocumentation.lua), dazu die
// Ereignisse ENABLE_LOW_LEVEL_RAID / DISABLE_LOW_LEVEL_RAID in EncounterInfoDocumentation.lua.
// Der Getter liest den Zustand, den der Server haelt - der Schalter ist damit dauerhaft (D4:
// persistiert in characters.playerFlags), nicht fluechtig.
void WorldSession::HandleLowLevelRaid1(WorldPackets::Misc::LowLevelRaid1& lowLevelRaid1)
{
    if (lowLevelRaid1.Enable)
        _player->SetPlayerFlag(PLAYER_FLAGS_LOW_LEVEL_RAID_ENABLED);
    else
        _player->RemovePlayerFlag(PLAYER_FLAGS_LOW_LEVEL_RAID_ENABLED);
}

// 0x430133, Writer 0x6AFB60, ein uint8. Am Draht belegt: 22 Pakete, konstant 1 Byte.
// Enum.ExcludedCensorSources als BITMASKE: 0 None, 1 Friends, 2 Guild, 4/8/16/32/64/128
// Reserve1..6. Gesetzt ueber die CVar "excludedCensorSources".
//
// AUSDRUECKLICHE ABWEICHUNG von Abschnitt 11.6 des Briefs, der hier "dauerhaft, gehoert zu den
// Kontodaten" empfiehlt: der Traeger ist eine CVar, und CVars speichert der CLIENT. Er sendet den
// Wert nach jedem Login neu - genau das bilden die 22 Pakete ueber vier Builds ab. Eine
// serverseitige Spalte waere ein zweiter Wahrheitstraeger, der beim ersten Login ueberschrieben
// wird. Deshalb fluechtig in der Sitzung. D4 ist damit ENTSCHIEDEN, nicht vergessen.
//
// UNVERIFIED: D2 ist NICHT erfuellt. Der Baum hat keinen Chat-Zensurpfad, von dem die Maske etwas
// ausnehmen koennte - der Wert wird gehalten und von nichts gelesen. Sobald eine Zensur im
// Chatweg existiert, ist DIES die Maske, die sie je Quelle abschaltet. Bis dahin D2 = offen.
void WorldSession::HandleSetExcludedChatCensorSources(WorldPackets::Misc::SetExcludedChatCensorSources& setExcludedChatCensorSources)
{
    _excludedChatCensorSources = setExcludedChatCensorSources.Sources;
}

// 0x430004, Writer 0x6A1C70, Element-Serializer 0x69FDE0 (JamCliAddOnInfo, 88 Byte).
// Antwort auf SMSG_ADDON_LIST_REQUEST; die drei Kopffelder sind Echos aus dieser Anfrage
// (0x209C8B0 liest sie aus der eingegangenen Nachricht). Der Server bekommt seine eigene
// Korrelationskennung zurueck und kann damit zwei gleichzeitige Abfragen unterscheiden.
//
// Der Abgleich gegen DB2 BannedAddons (LayoutHash 56583F69: ID, Name, Version, Flags) ist
// TrinityCore-Hauskonvention, also Server-Vertrag - er ist KEIN Beleg fuer Retail-Semantik.
//
// AUF EINEM UNVERAENDERTEN REALM ERREICHT DIESE NACHRICHT DEN HANDLER NIE, und das ist keine
// Vermutung: der Ausloeser SMSG_ADDON_LIST_REQUEST (0x4500EB) steht in Opcodes.cpp weiterhin auf
// STATUS_UNHANDLED und hat im ganzen Baum weder Paketklasse noch Sendestelle - grep ueber
// src/server/game trifft ausser Opcodes.h/Opcodes.cpp nur die Kommentare dieser Einheit. CMSG ist
// hier die ANTWORT, nicht der Anfang; ohne Anfrage sendet der Client nichts. Ein beobachteter
// Umlauf braucht deshalb entweder eine Aufnahme eines Retail-Realms oder zuerst die
// Gegenrichtung (Paketklasse, Sender und Statusdrehung fuer SMSG_ADDON_LIST_REQUEST). Beides
// liegt ausserhalb dieser Einheit; der Handler ist gebaut, damit er steht, wenn die Anfrage
// entsteht.
//
// UNVERIFIED: D2 ist NICHT erfuellt. Was Retail mit der gemeldeten Addonliste tut, ist von
// aussen nicht messbar: der Client ist hier ausschliesslich Sender, es gibt kein Gegenpaket
// (D3 gegenstandslos) und kein Lua-Ereignis, an dem sich eine Serverwirkung ablesen liesse -
// Blizzard_AddOnList/AddonList.lua ist reine Clientverwaltung des Addonfensters ohne Bezug zu
// dieser Nachricht. Eine DB2-Tabelle sagt, welche Daten der Client hat, nicht welchen Zustand
// der Server aendert. Der Handler unten aendert deshalb bewusst KEINEN Zustand: er
// protokolliert den Treffer und nichts weiter. Weder Kick noch Flag noch Persistenz sind
// belegt, und keines davon wird geraten. Bis eine Aufnahme eines Retail-Realms zeigt, was auf
// einen gesperrten Addonnamen folgt, bleibt D2 = offen.
void WorldSession::HandleAddonList(WorldPackets::Misc::AddonList& addonList)
{
    for (WorldPackets::Misc::AddonInfo const& addon : addonList.AddOns)
    {
        for (BannedAddonsEntry const* banned : sBannedAddonsStore)
        {
            if (!banned->Name || addon.Name != banned->Name)
                continue;

            // Leere Version in der Tabelle heisst: jede Version ist gesperrt.
            if (banned->Version && *banned->Version && addon.Version != banned->Version)
                continue;

            TC_LOG_INFO("entities.player.cheat", "{} reported banned addon '{}' version '{}' (BannedAddons.ID {})",
                GetPlayerInfo(), addon.Name, addon.Version, banned->ID);
            break;
        }
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_ADDON_LIST from {} request {} with {} addons",
        GetPlayerInfo(), addonList.RequestGUID.ToString(), addonList.AddOns.size());
}

// 0x3D02D7, Writer 0x6D2740 - das uint32 steht VOR der GUID.
// Lua C_WorldLootObject.OnWorldLootObjectClick(unitToken, isLeftClick) -> Ereignis
// WORLD_LOOT_OBJECT_INFO_UPDATED(guid), danach LOOT_OPENED. Gegenstueck SMSG_LOOT_RESPONSE, das
// im Baum bereits auf STATUS_NEVER steht - einer von nur zwei sendefaehigen Faellen des Satzes.
//
// UNVERIFIED, und der Grund, warum hier NICHT gelootet wird: "World Loot Object" ist im
// 12.x-Client ein eigener Beutetopf mit eigener Registratur (Delve- und Weltkisten), nicht der
// normale GameObject- oder Creature-Loot. Der Baum modelliert diesen Topf nicht. Wer hier
// ersatzweise den normalen Loot oeffnet, baut eine Wirkung, die im Test funktioniert und still
// von Retail abweicht - nach DoD Abschnitt 1 der teurere Fehler. Die Eingangspruefung steht, die
// Wirkung ist in der Statusdatei als offen ausgewiesen.
void WorldSession::HandleWorldLootObjectClick(WorldPackets::Misc::WorldLootObjectClick& worldLootObjectClick)
{
    if (!ObjectAccessor::GetWorldObject(*_player, worldLootObjectClick.ObjectGUID))
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_WORLD_LOOT_OBJECT_CLICK from {} for unknown object {}",
            GetPlayerInfo(), worldLootObjectClick.ObjectGUID.ToString());
        return;
    }

    TC_LOG_DEBUG("network.opcode", "CMSG_WORLD_LOOT_OBJECT_CLICK from {} object {} clickType {} - no world loot object registry in this tree",
        GetPlayerInfo(), worldLootObjectClick.ObjectGUID.ToString(), worldLootObjectClick.ClickType);
}

// 0x3D02F6, Writer 0x6D3660 - siehe den Korrekturblock in MiscPackets.h: Subplan und Brief
// hatten hier den Draht eines FREMDEN Opcodes (CMSG_TRANSFER_CURRENCY_FROM_ACCOUNT_CHARACTER).
//
// Feldbedeutung ueber DB2 QuestDrivenScenario (LayoutHash 408DD33F, Feld 2 = int<Scenario::ID>)
// und ScenarioStep (StepOrderIndex). Gegenstueck SMSG_SCENARIO_STATE steht im Baum bereits auf
// STATUS_NEVER - der volle Umlauf ist hier ohne eine einzige Statusaenderung testbar.
//
// Der Server ist der Eigentuemer des Szenariofortschritts. Diese Nachricht ist die MELDUNG des
// Clients ueber seinen eigenen Stand, nicht eine Anweisung: der Client schickt seine Stufenliste
// mit Start- und Endzeiten und seine Waehrungsstaende mit. Die richtige Antwort ist deshalb, den
// MASSGEBLICHEN Zustand erneut zu senden - nicht, den gemeldeten zu uebernehmen. Wer die
// Clientwerte uebernaehme, haette einen Szenariofortschritt, den der Client diktiert.
void WorldSession::HandleQuestDrivenScenarioStateChange(WorldPackets::Misc::QuestDrivenScenarioStateChange& questDrivenScenarioStateChange)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_QUEST_DRIVEN_SCENARIO_STATE_CHANGE from {} type {} scenario {} questDrivenScenario {} stages {} currencies {}",
        GetPlayerInfo(), questDrivenScenarioStateChange.StateChangeType,
        questDrivenScenarioStateChange.ScenarioID, questDrivenScenarioStateChange.QuestDrivenScenarioID,
        questDrivenScenarioStateChange.Stages.size(), questDrivenScenarioStateChange.Currencies.size());

    // Der Szenariozustand haengt an der InstanceMap, nicht an Map - ausserhalb einer Instanz
    // gibt es nichts nachzuziehen.
    InstanceMap* instanceMap = _player->GetMap()->ToInstanceMap();
    if (!instanceMap)
        return;

    InstanceScenario* scenario = instanceMap->GetInstanceScenario();
    if (!scenario)
        return;

    scenario->SendScenarioState(_player);
}

// --- B3: Telemetrie & Support -------------------------------------------------------------------
//
// D4 - Persistenz: FLUECHTIG, und das ist eine Korrektur gegenueber dem ersten Stand dieser
// Einheit. Der hatte characters.client_telemetry angelegt und beide Meldungen dort abgelegt.
// Damit galt fuer B3 genau die Konstruktion, die dieselbe Einheit fuer B5 zwei Dateien weiter
// ausdruecklich zurueckweist (LiveRegionHandler.cpp): eine Tabelle, in die nur geschrieben und
// aus der nie gelesen wird, ist kein geloestes D4, sondern ein leerer Migrationssatz. Es gab
// keinen SELECT, keinen Ladepfad und keine Loeschung in AccountMgr::DeleteAccount - die Zeilen
// haetten geloeschte Konten ueberlebt und waeren unbegrenzt gewachsen. Der Massstab muss in
// beide Richtungen derselbe sein, deshalb ist die Tabelle zurueckgenommen; sie gehoert in
// dieselbe Aenderung wie der Auswertungspfad, der sie liest.
//
// Bis dahin sind die beiden Meldungen Protokollhandler wie die uebrigen 51 - mit Entprellung,
// weil beide vom Client aus ausloesbar sind und der Standardwert der DoS-Bremse 100 Pakete je
// Sekunde und Sitzung betraegt (WorldSession::DosProtection::GetMaxPacketCounterAllowed).
//
// WARUM DIE ENTPRELLUNG HIER STEHT UND NICHT IN GetMaxPacketCounterAllowed: ein Eintrag dort ist
// KEINE Drossel. Ab dem ersten ueberzaehligen Paket verwirft DosProtection::EvaluateOpcode es
// ungelesen und trennt bei der Vorgabepolitik POLICY_KICK die Sitzung; der Zaehler laeuft dabei je
// KALENDERSEKUNDE, ein Stoss von N+1 innerhalb einer beliebigen Sekunde genuegt also. Fuer
// CMSG_REPORT_SERVER_LAG, das an dem Lua-Knopf GMReportLag haengt, waere ein enger Eintrag damit
// ein Doppelklick-Kick eines ehrlichen Spielers - und der einzige Hinweis darauf waere dieselbe
// "AntiDOS: flooding packet"-Warnung, die eine echte Flut auch erzeugt. Beide Handler kosten nach
// der Ruecknahme der Tabelle nichts mehr als ein TC_LOG_DEBUG, dessen Argumente bei abgeschaltetem
// Protokoll gar nicht erst ausgewertet werden (TC_LOG_MESSAGE_BODY prueft GetEnabledLogger zuerst).
// Die Last, gegen die entprellt wird, ist damit die des PROTOKOLLS, nicht die des Weltthreads -
// und dagegen ist eine Sitzungsdrossel das richtige Mittel: sie verwirft die Wiederholung, ohne
// irgendjemanden zu trennen.

// 0x3D0273, Writer 0x6D1870, leere Nutzlast.
// WIDERSPRUCH, der nicht weggeglaettet wird: der Serializer ist leer, das Lua-Binding heisst aber
// GMReportLag(number) und nimmt einen Parameter. Entweder gehoert GMReportLag zu einem anderen
// Opcode, oder der Parameter waehlt clientseitig aus mehreren Meldungen aus. Am Draht kommt
// jedenfalls nichts an; verwertbar sind nur Zeitpunkt und Absender.
void WorldSession::HandleReportServerLag(WorldPackets::Misc::ReportServerLag& /*reportServerLag*/)
{
    // Sitzungsdrossel: hoechstens eine Meldung je REPORT_SERVER_LAG_INTERVAL. Kein DosProtection-
    // Eintrag, Begruendung im Blockkopf oben.
    time_t const now = GameTime::GetGameTime();
    if (now - _lastReportServerLagTime < REPORT_SERVER_LAG_INTERVAL)
        return;

    _lastReportServerLagTime = now;

    TC_LOG_DEBUG("network.opcode", "CMSG_REPORT_SERVER_LAG from {} latency {}ms", GetPlayerInfo(), GetLatency());
}

// 0x4300BD, Writer 0x6AAA60, ein uint32.
// FALLE: das uint32 ist NICHT die GMSurveySurveys.ID, sondern der caseIndex aus
// SMSG_UPDATE_WEB_TICKET. Wer es als Umfrage-ID liest, quittiert die falsche Umfrage.
// Der Baum hat kein Web-Ticket-System mit Umfrage-Rueckkanal.
void WorldSession::HandleGMTicketAcknowledgeSurvey(WorldPackets::Misc::GMTicketAcknowledgeSurvey& gmTicketAcknowledgeSurvey)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_GM_TICKET_ACKNOWLEDGE_SURVEY from {} caseIndex {}",
        GetPlayerInfo(), gmTicketAcknowledgeSurvey.CaseIndex);
}

// 0x430113, Writer 0x6AE330 -> Rumpf 0x6AD300, Fuellfunktion 0x20B530.
// Hardware- und Engineprofil, 294 Byte im Sniff. Der Client sendet es EINMAL je
// (Schema-Version 8, Client-Patch): 0x210940 baut und sendet nur, wenn engineSurvey < 8 oder
// engineSurveyPatch != aktueller Patch, und setzt danach beide CVars.
// Kein Gegenstueck - reine Telemetrie. D4: FLUECHTIG, siehe den Blockkopf oben.
//
// Die Retail-Invariante wird hier NACHGEPRUEFT und nicht nur beschrieben: eine Sitzung, die das
// Profil ein zweites Mal schickt, ist per Analyse kein Retail-Client. Die Wiederholung wird
// verworfen, damit eine 294-Byte-Nutzlast nicht 100 Mal je Sekunde durch den Handler laeuft.
void WorldSession::HandleEngineSurvey(WorldPackets::Misc::EngineSurvey& engineSurvey)
{
    if (_engineSurveyReceived)
    {
        TC_LOG_DEBUG("network.opcode", "CMSG_ENGINE_SURVEY from {} - repeated in the same session, dropped", GetPlayerInfo());
        return;
    }

    _engineSurveyReceived = true;

    std::ostringstream profile;
    profile << engineSurvey.CpuVendor << ' ' << engineSurvey.CpuBrand
            << " (" << engineSurvey.CpuCores << 'C' << engineSurvey.CpuThreads << "T)"
            << ", ram " << engineSurvey.PhysicalMemory
            << ", gpu " << engineSurvey.GpuName
            << " [" << engineSurvey.GpuVendorID << ':' << engineSurvey.GpuDeviceID << ']'
            << " vram " << engineSurvey.DedicatedVideoMemory
            << ", os " << engineSurvey.OsName << " build " << engineSurvey.OsBuildNumber
            << ", desktop " << engineSurvey.DesktopWidth << 'x' << engineSurvey.DesktopHeight
            << ", monitor " << engineSurvey.MonitorWidth << 'x' << engineSurvey.MonitorHeight
            << " (" << uint32(engineSurvey.MonitorCountMinusOne) + 1 << ')'
            << ", board " << engineSurvey.BaseBoardManufacturer << ' ' << engineSurvey.BaseBoardProduct
            << ", bios " << engineSurvey.BiosVendor << ' ' << engineSurvey.BiosVersion;

    TC_LOG_DEBUG("network.opcode", "CMSG_ENGINE_SURVEY from {} version {} patch {}: {}",
        GetPlayerInfo(), engineSurvey.SurveyVersion, engineSurvey.SurveyPatch, profile.str());
}

// 0x430197, Writer 0x6B2860 - bits24 Laenge INKLUSIVE NUL, dann uint32 und guid, dann die Bytes.
// OFFEN, und ausdruecklich nicht geraten: es gibt in TC 12.1 keinen SMSG_SERVER_VALIDATION_* und
// auch keinen unter den wiederhergestellten Client-Handlern. Das Gegenstueck ist unbekannt.
void WorldSession::HandleServerValidationSignatureRequest(WorldPackets::Misc::ServerValidationSignatureRequest& serverValidationSignatureRequest)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_SERVER_VALIDATION_SIGNATURE_REQUEST from {} request {} guid {} signature length {} - no counterpart known",
        GetPlayerInfo(), serverValidationSignatureRequest.RequestID,
        serverValidationSignatureRequest.Guid.ToString(), serverValidationSignatureRequest.Signature.length());
}

// --- B11: Warden3 -------------------------------------------------------------------------------

// 0x430018, Writer 0x6A2940: uint32 Kind; uint32 Size; byte Data[Size].
// Am Draht belegt: 1398 Pakete, 40..16280 Byte - 8 + Size geht auf.
//
// Der Baum hat kein Anti-Cheat. Ohne Gegenstelle schickt der Client rund 1400 Datenpakete je
// Sitzung ins Leere. Die definierte Abschaltmeldung ist SMSG_WARDEN3_DISABLED (0x4502CC); ihr
// Reader 0x606F30 nimmt den Rest des Pakets als undurchsichtigen Block, eine leere Nutzlast ist
// also gueltig.
// UNVERIFIED: der Konsument 0x1CE2CD0 liegt nicht im Dekompilat-Cache. Dass der Client danach
// aufhoert zu senden, ist nicht am Konsumenten belegt. Deshalb genau EIN Versuch je Sitzung -
// wirkt er nicht, kostet er ein Paket.
void WorldSession::HandleWarden3Data(WorldPackets::Misc::Warden3Data& warden3Data)
{
    TC_LOG_DEBUG("network.opcode", "CMSG_WARDEN3_DATA from {} kind {} size {}",
        GetPlayerInfo(), warden3Data.Kind, warden3Data.Data.size());

    if (_warden3DisabledSent)
        return;

    _warden3DisabledSent = true;
    SendPacket(WorldPackets::Misc::Warden3Disabled().Write());
}

// GM / Cheat / Debug (client 12.1.0.69382, family 0x3D)

// CMSG_KIOSK_ENABLE_GOD_MODE - writer 0x6CD330, sent from Lua Kiosk.EnableGodMode() (0x11684B0),
// called in Blizzard_Kiosk/Housing/Game.lua:186 so that a trade-show visitor cannot die inside the
// demo house. The client hardcodes the bit to true and offers no counterpart to switch it off.
//
// Retail gates this on kiosk mode, which the server announces through
// FeatureSystemStatus::KioskModeEnabled - a field TrinityCore serialises but never sets, so no
// unmodified client will ever send this opcode (open question F4 of the unit brief). That leaves an
// unauthenticated invulnerability request on the wire, so it is gated on the same permission that
// governs .cheat god: an account allowed to make itself invulnerable by command may do so here too,
// everyone else is refused and logged.
void WorldSession::HandleKioskEnableGodMode(WorldPackets::Misc::KioskEnableGodMode& kioskEnableGodMode)
{
    if (!HasPermission(rbac::RBAC_PERM_COMMAND_CHEAT_GOD))
    {
        TC_LOG_INFO("entities.player.cheat", "WorldSession::HandleKioskEnableGodMode: Player {} ({}) requested kiosk god mode without permission - refused",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    if (kioskEnableGodMode.Enable)
        _player->SetCommandStatusOn(CHEAT_GOD);
    else
        _player->SetCommandStatusOff(CHEAT_GOD);

    // Session bound on purpose: _activeCheats is never written to the database, and the client has
    // no disable path - the state has to lapse with the session.
    SendPacket(WorldPackets::Misc::GodMode(kioskEnableGodMode.Enable).Write());
}

// CMSG_SET_GAME_EVENT_DEBUG_VIEW_STATE - writer 0x6CCC40. Nothing to do with game events:
// ViewIndex addresses the client debug view table at 0x43BD1C0 (0..39). The client sends this
// either as its answer to SMSG_DEBUG_MENU_MANAGER_FULL_UPDATE (consumer 0x4EF5F0, one message per
// subscribed view) or from its own console command (0x4EF456).
//
// The views it names are the message set of family 0x4D - the AI debug channel - which has no
// server side implementation yet (no opcode of family 0x4D is declared in Opcodes.h). The switch is
// therefore recorded but currently drives nothing; that is tracked as open question F5.
void WorldSession::HandleSetGameEventDebugViewState(WorldPackets::Misc::SetGameEventDebugViewState& setGameEventDebugViewState)
{
    if (!HasPermission(rbac::RBAC_PERM_COMMAND_DEBUG))
    {
        TC_LOG_INFO("entities.player.cheat", "WorldSession::HandleSetGameEventDebugViewState: Player {} ({}) tried to subscribe to debug view {} without permission",
            _player->GetName(), _player->GetGUID().ToString(), setGameEventDebugViewState.ViewIndex);
        return;
    }

    if (setGameEventDebugViewState.ViewIndex >= MaxDebugViews)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetGameEventDebugViewState: Player {} sent out of range ViewIndex {} (client knows {})",
            _player->GetName(), setGameEventDebugViewState.ViewIndex, MaxDebugViews);
        return;
    }

    _debugViewSubscriptions.set(setGameEventDebugViewState.ViewIndex, setGameEventDebugViewState.State);
}

// Answer to SMSG_CHECK_ABANDON_NPE. The client sends this from the two buttons of
// StaticPopupDialogs["LEAVING_TUTORIAL_AREA"]: C_Tutorial.ReturnToTutorialArea writes bit 0,
// C_Tutorial.AbandonTutorialArea writes bit 1 (writer RVA 0x6D1CB0, one body byte).
void WorldSession::HandleAbandonNPEResponse(WorldPackets::Misc::AbandonNPEResponse& abandonNpeResponse)
{
    if (_player->GetCreateMode() != PlayerCreateMode::NPE)
        return;

    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(_player->GetRace(), _player->GetClass());
    if (!info || !info->createPositionNPE)
        return;

    if (!abandonNpeResponse.Abandon)
    {
        // "Return to the tutorial area" - put the character back where it started. The once-per-session
        // guard is NOT cleared here: the far teleport that follows leaves the current map first, and
        // that map leave still reports the map the character is coming from, so clearing the guard now
        // would re-ask the question during the trip back. Player::UpdateNPEExitState clears it on
        // arrival on the tutorial map instead, which is the moment the question makes sense again.
        _player->TeleportTo(info->createPositionNPE->Loc);
        return;
    }

    // "Leave for good". The create mode is what still points homebind, the graveyard fallback and the
    // intro scene at the tutorial (Player.cpp: createPositionNPE branches). Leaving it on NPE would
    // keep dragging the character back and would re-arm this prompt on the next departure.
    // D4: this is the one durable state change of the whole handshake, so it goes to the database at
    // once (characters.createMode, written by CHAR_UPD_CHARACTER_CREATE_MODE; the column already
    // exists and is read back by Player::LoadFromDB, no migration). The in-memory setter alone would
    // lose the decision on relog - the sole guard against re-asking (m_npeAbandonPrompted) is
    // transient by design, so the character would be asked once more after the next login.
    _player->SetCreateMode(PlayerCreateMode::Normal, true);
    _player->SetHomebind(*_player, _player->GetAreaId());
}

// Answer to SMSG_PLAYER_OPEN_SUBSCRIPTION_INTERSTITIAL (writer RVA 0x6D1C50, bits<3>, one body byte).
// There is no server side state behind this: the interstitial is a Battle.net store prompt and the
// three answers (Closed / Clicked / WebRedirect) only tell the retail backend what the player did with
// it. Nothing in a realm's own state depends on the answer, so this handler deliberately only records
// it - a private realm has no store to open. Reading it still matters: without a handler the packet
// counts as unhandled and the tail check logs on every click.
void WorldSession::HandleSubscriptionInterstitialResponse(WorldPackets::Misc::SubscriptionInterstitialResponse& subscriptionInterstitialResponse)
{
    TC_LOG_DEBUG("network", "CMSG_SUBSCRIPTION_INTERSTITIAL_RESPONSE: {} answered with wire value {}",
        GetPlayerInfo(), AsUnderlyingType(subscriptionInterstitialResponse.Response));
}

// Request for SMSG_SCHEDULED_AREA_POI_UPDATE_RESPONSE (writer RVA 0x6D1230, empty, 4 bytes).
// Sent by C_EventScheduler.RequestEvents() when the in-game event calendar opens.
// @todo: this tree has no AreaPOI rotation and no EventScheduler content yet, so the answer is an
// empty pair of lists. That is a valid answer - "nothing scheduled" - and it is what keeps the frame
// from waiting forever; the same shape as SMSG_WORLD_QUEST_UPDATE_RESPONSE, which also answers empty.
void WorldSession::HandleRequestScheduledAreaPoiUpdate(WorldPackets::Misc::RequestScheduledAreaPoiUpdate& /*requestScheduledAreaPoiUpdate*/)
{
    SendPacket(WorldPackets::Misc::ScheduledAreaPoiUpdateResponse().Write());
}

// CMSG_BONUS_ROLL (writer RVA 0x6D16E0, empty, 4 bytes).
// This tree has no bonus roll system - no currency, no loot table, no reroll bookkeeping. The correct
// answer to "give me a bonus roll" from a server that cannot grant one is the defined failure, not
// silence: SMSG_PLAYER_BONUS_ROLL_FAILED fires Lua BONUS_ROLL_FAILED and releases the button, whereas
// Handle_NULL leaves the client waiting on a roll that never resolves. The client shows no reason and
// there is no error code on the wire, so there is nothing else to fill in.
void WorldSession::HandleBonusRoll(WorldPackets::Misc::BonusRoll& /*bonusRoll*/)
{
    SendPacket(WorldPackets::Misc::PlayerBonusRollFailed().Write());
}

void WorldSession::HandleChromieTimeSelectExpansion(WorldPackets::Misc::ChromieTimeSelectExpansion& chromieTimeSelectExpansion)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Wire format (12.0.5): PackedGuid Vendor + int32 ExpansionID, where ExpansionID is the
    // UIChromieTimeExpansionInfo.ID (DB2 record id), not the Expansions enum.
    // Verify the vendor is a gossip NPC the player is actually interacting with.
    Creature const* vendor = player->GetNPCIfCanInteractWith(chromieTimeSelectExpansion.Vendor, UNIT_NPC_FLAG_GOSSIP, UNIT_NPC_FLAG_2_NONE);
    if (!vendor)
        return;

    // Require an active ChromieTime interaction with this exact NPC, mirroring other
    // interaction-driven handlers. The client always packages the interaction-source guid
    // into the CMSG (SelectChromieTimeOption RVA 0xB79106), so a legitimate select can
    // only arrive while the type-45 interaction started by the gossip option is open.
    if (!player->PlayerTalkClass->GetInteractionData().IsInteractingWith(chromieTimeSelectExpansion.Vendor, PlayerInteractionType::ChromieTime))
        return;

    int32 expansionId = chromieTimeSelectExpansion.ExpansionID;

    // 0 = "Return to the present"; always allowed (a chromie player above the entry
    // ceiling - the 70..80 scaling band - must still be able to leave).
    if (expansionId == 0)
    {
        player->SetChromieTime(0);
        player->SendDirectMessage(WorldPackets::Misc::ChromieTimeSelectExpansionSuccess().Write());
        return;
    }

    // Retail 12.0.x entry gate: level band [10, 70). The @68887 ShowPlayerConditionIDs
    // contain no level clause, so the ceiling is server policy (see Player.h, audit R10).
    if (player->GetLevel() < Player::ChromieTimeMinLevel || player->GetLevel() >= Player::ChromieTimeMaxEntryLevel)
        return;


    // Do NOT require ShowPlayerConditionID here: decoded @68887 each of those conditions is
    // ModifierTree { All -> PlayerIsInChromieTime(own id) } with PlayerCondition flags 0x21
    // (no InvertModifierTree) - i.e. "player is ALREADY in this timeline", the client UI's
    // alreadyOn marker, not an eligibility gate. Requiring it inverted the gate and made
    // every first-time selection fail (audit R10 decode).

    player->SetChromieTime(expansionId);

    player->SendDirectMessage(WorldPackets::Misc::ChromieTimeSelectExpansionSuccess().Write());

    // Audit R13 FIX 1: retail completes quest 85026 "Where Legends are Made" (objective
    // 453674 = QUEST_OBJECTIVE_MONSTER, ObjectID 167032 = Chromie, Amount 1,
    // "Timewalking Campaign selected") by granting Chromie kill-credit on a successful
    // selection. Always safe: KilledMonsterCredit no-ops if the player is not on the quest.
    player->KilledMonsterCredit(167032);

    // Audit R14 (G1 immersion): Chromie speaks an era-specific flavor line the instant a timeline
    // is selected. Verified against the 12.1.0.69382 ChromieOrgrimmar capture: each of the 9
    // CMSG_CHROMIE_TIME_SELECT_EXPANSION was immediately followed by a CHAT_MSG_MONSTER_SAY from
    // Chromie (SMSG_CHAT) carrying the exact lines below - extracted from the sniff, not invented.
    // The line is spoken by the interacted Chromie vendor, so it works for either faction's Chromie
    // NPC and either capital without depending on a specific creature entry / creature_text rows.
    // NOTE: the campaign-intro movie (id 470) also seen in the capture played only ONCE, before any
    // select (SMSG_PLAY_MOVIE, one occurrence in the whole capture), so it is a one-time cinematic,
    // NOT a per-select movie, and is deliberately not replayed here (replaying it every switch would
    // be un-Blizzlike). expansionId = UiChromieTimeExpansionInfo.ID.
    auto chromieSelectLine = [](int32 exp) -> char const*
    {
        switch (exp)
        {
            case  5: return "Who would have thought someone named Deathwing would bring about so much destruction?";
            case  6: return "You look quite prepared!";
            case  7: return "Do not exercise restraint when showing your great power to the Scourge!";
            case  8: return "Whatever you do, do not get between a pandaren and their brew. It'll be unbearably painful if you do!";
            case  9: return "If a scary orc offers you something to drink, you probably want to say no.";
            case 10: return "If you find a powerful weapon, just make sure it isn't corrupted by the Burning Legion or Old Gods, okay?";
            case 14: return "Death it is! And you know what they say... what doesn't kill you makes you stronger.";
            case 15: return "This might be the most important battle of them all. For our world, worth fighting for!";
            case 16: return "Dragons, dragons, dragons! I'm definitely not biased...";
            default: return nullptr;
        }
    };
    if (char const* line = chromieSelectLine(expansionId))
        const_cast<Creature*>(vendor)->Say(line, LANG_UNIVERSAL, player);

    // Audit R13 FIX 2: auto-offer the era's Chromie Time breadcrumb for the chosen timeline,
    // matching retail's per-expansion intro quest. The breadcrumbs are FACTION-SPECIFIC; each
    // expansion has an Alliance/Horde quest pair, and CanTakeQuest (AllowableRaces) rejects a
    // wrong-faction id, so offering only one faction's id reproduces the "no breadcrumb" bug.
    //
    // The HORDE column is byte/temporal-verified against the 12.1.0.69382 ChromieOrgrimmar
    // capture (C:/sniff/ymir_retail_12.1.0.69299/dumps/ChromieOrgrimmar): a Horde Undead
    // Warlock clicked every timeline, and each CMSG_CHROMIE_TIME_SELECT_EXPANSION was followed
    // within a handful of records by exactly ONE breadcrumb quest-add. Correlated select->quest
    // (TCHarvest.lua quest_template titles confirm the era):
    //   Cata(5)=60887 "Onward to Adventure in Kalimdor", TBC(6)=60123 "To Outland!",
    //   WotLK(7)=60097 "To Northrend!", MoP(8)=60126 "To Pandaria!", WoD(9)=34398 "The Dark
    //   Portal" (faction-neutral), Legion(10)=43926 "The Legion Returns", SL(14)=61874
    //   "A Chilling Summons" (Meet Darion Mograine at Grommash Hold), BfA(15)=51443 "Mission
    //   Statement" (harvest quest_accepted=51443, quest_detail offered+accepted),
    //   DF(16)=65435 "The Dragon Isles Await".
    // The previously-used Horde ids 60961/60963/60968/60970/53372 do NOT appear anywhere in the
    // capture and were wrong (53372 is the BfA war-campaign "Hour of Reckoning", not the Chromie
    // breadcrumb); corrected below to the sniff-verified ids. SL (14) Horde was deferred and is
    // now resolved to 61874.
    //
    // The ALLIANCE column is branch-asserted and NOT verified by this Horde-only capture. Cata
    // (60891) and MoP (60125) are the plausible paired counterparts of the verified Horde ids;
    // TBC/WotLK/WoD/Legion/BfA/SL Alliance ids need an Alliance-side sniff to confirm and are
    // suspect (the branch's Horde ids for those rows were wrong, so the Alliance ids likely are
    // too - e.g. BfA A=53370 is the Alliance war-campaign "Hour of Reckoning", not the Chromie
    // breadcrumb). CanTakeQuest re-validates AllowableRaces, so a wrong/absent id yields no offer
    // rather than a wrong-faction quest, keeping this safe until an Alliance capture is taken.
    //
    // expansionId = UiChromieTimeExpansionInfo.ID (DB2 record id): Cata=5, TBC=6, WotLK=7,
    // MoP=8, WoD=9, Legion=10, SL=14, BfA=15, DF=16. Excluded: The War Within - breadcrumbs
    // 81930/78713 exist and are Chromie-started, but TWW has NO UiChromieTimeExpansionInfo row
    // so it can never arrive as a selection.
    struct ChromieIntroQuest { int32 ExpansionId; uint32 AllianceQuest; uint32 HordeQuest; };
    static constexpr ChromieIntroQuest ChromieIntroQuests[] =
    {
        {  5, 60891, 60887 }, // Cataclysm              (H 60887 "...Kalimdor" verified; A 60891 = Eastern Kingdoms)
        {  6, 60959, 60123 }, // Burning Crusade        (H 60123 "To Outland!" verified; A unverified)
        {  7, 60962, 60097 }, // Wrath of the Lich King (H 60097 "To Northrend!" verified; A unverified)
        {  8, 60125, 60126 }, // Mists of Pandaria      (H 60126 "To Pandaria!" verified)
        {  9, 60969, 34398 }, // Warlords of Draenor    (H 34398 "The Dark Portal" verified, faction-neutral; A unverified)
        { 10, 60971, 43926 }, // Legion                 (H 43926 "The Legion Returns" verified; A unverified)
        { 14, 60545, 61874 }, // Shadowlands            (H 61874 "A Chilling Summons" verified; A 60545 branch-asserted)
        { 15, 53370, 51443 }, // Battle for Azeroth     (H 51443 "Mission Statement" verified; A 53370 suspect = war-campaign)
        { 16, 65436, 65435 }, // Dragonflight           (H 65435 "The Dragon Isles Await" verified)
    };

    for (ChromieIntroQuest const& intro : ChromieIntroQuests)
    {
        if (intro.ExpansionId != expansionId)
            continue;

        uint32 introQuestId = (player->GetTeam() == ALLIANCE) ? intro.AllianceQuest : intro.HordeQuest;
        if (Quest const* introQuest = sObjectMgr->GetQuestTemplate(introQuestId))
            if (player->GetQuestStatus(introQuestId) == QUEST_STATUS_NONE && player->CanTakeQuest(introQuest, false))
                player->AddQuestAndCheckCompletion(introQuest, nullptr);

        break;
    }
}

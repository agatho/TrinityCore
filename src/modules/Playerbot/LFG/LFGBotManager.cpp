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

#include "LFGBotManager.h"
#include "LFGBotSelector.h"
#include "LFGRoleDetector.h"
#include "LFGGroupCoordinator.h"
#include "Player.h"
#include "LFGMgr.h"
#include "LFG.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "World.h"
#include "Group.h"
#include "GroupMgr.h"
#include "../Core/PlayerBotHooks.h"

// Constructor
LFGBotManager::LFGBotManager()
    : _enabled(false)
    , _updateAccumulator(0)
    , _initialized(false)
{
}

// Destructor
LFGBotManager::~LFGBotManager()
{
    Shutdown();
}

// Singleton instance
LFGBotManager* LFGBotManager::instance()
{
    static LFGBotManager instance;
    return &instance;
}

void LFGBotManager::Initialize()
{
    std::lock_guard lock(_mutex);

    if (_initialized)
    {
        TC_LOG_WARN("module.playerbot", "LFGBotManager::Initialize - Already initialized, ignoring");
        return;
    }

    TC_LOG_INFO("module.playerbot", "Initializing LFG Bot Manager...");

    // Clear any existing data
    _queuedBots.clear();
    _humanPlayers.clear();
    _proposalBots.clear();
    _updateAccumulator = 0;

    // TODO: Load configuration from playerbots.conf
    // For now, enable by default
    _enabled = true;

    _initialized = true;

    TC_LOG_INFO("module.playerbot", "LFG Bot Manager initialized successfully (Enabled: {})", _enabled.load());
}

void LFGBotManager::Shutdown()
{
    std::lock_guard lock(_mutex);

    if (!_initialized)
        return;

    TC_LOG_INFO("module.playerbot", "Shutting down LFG Bot Manager...");

    // Remove all bots from queues
    for (auto const& [botGuid, queueInfo] : _queuedBots)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }
    }

    // Clear all data structures
    _queuedBots.clear();
    _humanPlayers.clear();
    _proposalBots.clear();

    _initialized = false;
    _enabled = false;

    TC_LOG_INFO("module.playerbot", "LFG Bot Manager shut down successfully");
}

void LFGBotManager::Update(uint32 diff)
{
    if (!_enabled || !_initialized)
        return;

    _updateAccumulator += diff;

    // Periodic cleanup every CLEANUP_INTERVAL
    if (_updateAccumulator >= CLEANUP_INTERVAL)
    {
        CleanupStaleAssignments();
        _updateAccumulator = 0;
    }
}

void LFGBotManager::OnPlayerJoinQueue(Player* player, uint8 playerRole, lfg::LfgDungeonSet const& dungeons)
{
    if (!_enabled || !_initialized)
        return;

    // Only process human players
    if (Playerbot::PlayerBotHooks::IsPlayerBot(player))
        return;

    if (dungeons.empty())
    {
        TC_LOG_WARN("module.playerbot", "LFGBotManager::OnPlayerJoinQueue - Player {} queued with no dungeons",
                    player->GetName());
        return;
    }

    ObjectGuid playerGuid = player->GetGUID();

    // Calculate needed roles (assuming 5-man dungeon composition)
    uint8 tanksNeeded = 0, healersNeeded = 0, dpsNeeded = 0;
    CalculateNeededRoles(playerRole, tanksNeeded, healersNeeded, dpsNeeded);

    uint8 totalNeeded = tanksNeeded + healersNeeded + dpsNeeded;

    if (totalNeeded == 0)
    {
        TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnPlayerJoinQueue - Player {} has full group, no bots needed",
                     player->GetName());
        return;
    }

    TC_LOG_INFO("module.playerbot", "LFGBotManager::OnPlayerJoinQueue - Player {} queued, need {} tanks, {} healers, {} DPS",
                player->GetName(), tanksNeeded, healersNeeded, dpsNeeded);

    // Build role mask for bot selection
    uint8 neededRoles = 0;
    if (tanksNeeded > 0)
        neededRoles |= lfg::PLAYER_ROLE_TANK;
    if (healersNeeded > 0)
        neededRoles |= lfg::PLAYER_ROLE_HEALER;
    if (dpsNeeded > 0)
        neededRoles |= lfg::PLAYER_ROLE_DAMAGE;

    // Populate queue with bots
    uint32 botsQueued = PopulateQueue(playerGuid, neededRoles, dungeons);

    if (botsQueued < totalNeeded)
    {
        TC_LOG_WARN("module.playerbot", "LFGBotManager::OnPlayerJoinQueue - Only queued {}/{} bots for player {}",
                    botsQueued, totalNeeded, player->GetName());
    }
    else
    {
        TC_LOG_INFO("module.playerbot", "LFGBotManager::OnPlayerJoinQueue - Successfully queued {} bots for player {}",
                    botsQueued, player->GetName());
    }
}

void LFGBotManager::OnPlayerLeaveQueue(ObjectGuid playerGuid)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    // Check if this is a human player with assigned bots
    auto humanItr = _humanPlayers.find(playerGuid);
    if (humanItr != _humanPlayers.end())
    {
        TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnPlayerLeaveQueue - Human player left queue, removing {} assigned bots",
                     humanItr->second.assignedBots.size());

        // Remove all assigned bots from queue
        for (ObjectGuid botGuid : humanItr->second.assignedBots)
        {
            if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
            {
                RemoveBotFromQueue(bot);
            }
            _queuedBots.erase(botGuid);
        }

        _humanPlayers.erase(humanItr);
    }
    // Check if this is a bot
    else
    {
        auto botItr = _queuedBots.find(playerGuid);
        if (botItr != _queuedBots.end())
        {
            // Remove bot from its human player's assignment list
            ObjectGuid humanGuid = botItr->second.humanPlayerGuid;
            auto humanPlayerItr = _humanPlayers.find(humanGuid);
            if (humanPlayerItr != _humanPlayers.end())
            {
                auto& bots = humanPlayerItr->second.assignedBots;
                bots.erase(std::remove(bots.begin(), bots.end(), playerGuid), bots.end());

                if (bots.empty())
                {
                    _humanPlayers.erase(humanPlayerItr);
                }
            }

            _queuedBots.erase(botItr);
        }
    }
}

void LFGBotManager::OnProposalReceived(uint32 proposalId, lfg::LfgProposal const& proposal)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    // Find all bots in this proposal
    std::unordered_set<ObjectGuid> botsInProposal;

    for (auto const& [playerGuid, proposalPlayer] : proposal.players)
    {
        // Check if this is a queued bot
        if (_queuedBots.find(playerGuid) != _queuedBots.end())
        {
            botsInProposal.insert(playerGuid);

            // Update bot's proposal ID
            _queuedBots[playerGuid].proposalId = proposalId;

            // Automatically accept the proposal for this bot
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnProposalReceived - Bot {} auto-accepting proposal {}",
                         playerGuid.ToString(), proposalId);

            // Use LFGMgr to update proposal acceptance
            sLFGMgr->UpdateProposal(proposalId, playerGuid, true);
        }
    }

    if (!botsInProposal.empty())
    {
        _proposalBots[proposalId] = botsInProposal;
        TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnProposalReceived - Proposal {} has {} bots",
                     proposalId, botsInProposal.size());
    }
}

void LFGBotManager::OnRoleCheckReceived(ObjectGuid groupGuid, ObjectGuid botGuid)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    // If specific bot GUID provided, handle just that bot
    if (!botGuid.IsEmpty())
    {
        auto itr = _queuedBots.find(botGuid);
        if (itr != _queuedBots.end())
        {
            uint8 role = itr->second.assignedRole;
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnRoleCheckReceived - Bot {} confirming role {} for group {}",
                         botGuid.ToString(), role, groupGuid.ToString());

            sLFGMgr->UpdateRoleCheck(groupGuid, botGuid, role);
        }
        return;
    }

    // Handle all bots in the group
    for (auto const& [queuedBotGuid, queueInfo] : _queuedBots)
    {
        // Check if bot is part of this group's proposal
        // We need to verify the bot is actually in this group
        if (Player* bot = ObjectAccessor::FindPlayer(queuedBotGuid))
        {
            uint8 role = queueInfo.assignedRole;
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnRoleCheckReceived - Bot {} confirming role {} for group {}",
                         queuedBotGuid.ToString(), role, groupGuid.ToString());

            sLFGMgr->UpdateRoleCheck(groupGuid, queuedBotGuid, role);
        }
    }
}

void LFGBotManager::OnGroupFormed(ObjectGuid groupGuid)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnGroupFormed - Group {} formed successfully", groupGuid.ToString());

    // Get the group
    Group* group = sGroupMgr->GetGroupByGUID(groupGuid);
    if (!group)
    {
        TC_LOG_ERROR("module.playerbot", "LFGBotManager::OnGroupFormed - Group {} not found", groupGuid.ToString());
        return;
    }

    // Get the dungeon ID for this group
    uint32 dungeonId = sLFGMgr->GetDungeon(groupGuid);
    if (dungeonId == 0)
    {
        TC_LOG_WARN("module.playerbot", "LFGBotManager::OnGroupFormed - No dungeon ID for group {}", groupGuid.ToString());
        return;
    }

    // Notify the group coordinator about group formation
    if (sLFGGroupCoordinator->IsEnabled())
    {
        if (sLFGGroupCoordinator->OnGroupFormed(groupGuid, dungeonId))
        {
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnGroupFormed - Group coordinator notified for group {}",
                groupGuid.ToString());

            // Teleport the group to the dungeon
            if (sLFGGroupCoordinator->TeleportGroupToDungeon(group, dungeonId))
            {
                TC_LOG_INFO("module.playerbot", "LFGBotManager::OnGroupFormed - Group {} teleported to dungeon {}",
                    groupGuid.ToString(), dungeonId);
            }
            else
            {
                TC_LOG_ERROR("module.playerbot", "LFGBotManager::OnGroupFormed - Failed to teleport group {} to dungeon {}",
                    groupGuid.ToString(), dungeonId);
            }
        }
        else
        {
            TC_LOG_ERROR("module.playerbot", "LFGBotManager::OnGroupFormed - Failed to register group {} with coordinator",
                groupGuid.ToString());
        }
    }

    // Find and cleanup all bots and humans associated with this group
    // This is done by checking which bots/humans were successfully grouped
    // The actual cleanup will happen when they complete/leave the dungeon
}

void LFGBotManager::OnProposalFailed(uint32 proposalId)
{
    if (!_enabled || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    auto itr = _proposalBots.find(proposalId);
    if (itr == _proposalBots.end())
        return;

    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::OnProposalFailed - Proposal {} failed, removing {} bots from queue",
                 proposalId, itr->second.size());

    // Remove all bots from this failed proposal
    for (ObjectGuid botGuid : itr->second)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }

        UnregisterBotAssignment(botGuid);
    }

    _proposalBots.erase(itr);
}

uint32 LFGBotManager::PopulateQueue(ObjectGuid playerGuid, uint8 neededRoles, lfg::LfgDungeonSet const& dungeons)
{
    if (dungeons.empty())
        return 0;

    std::lock_guard lock(_mutex);

    uint32 dungeonId = *dungeons.begin();
    uint8 minLevel = 0, maxLevel = 0;

    if (!GetDungeonLevelRange(dungeonId, minLevel, maxLevel))
    {
        TC_LOG_ERROR("module.playerbot", "LFGBotManager::PopulateQueue - Could not get level range for dungeon {}", dungeonId);
        return 0;
    }

    // Determine what roles the human player has
    Player* humanPlayer = ObjectAccessor::FindPlayer(playerGuid);
    if (!humanPlayer)
    {
        TC_LOG_ERROR("module.playerbot", "LFGBotManager::PopulateQueue - Could not find player {}", playerGuid.ToString());
        return 0;
    }

    // Calculate exact numbers needed (assuming 5-man dungeon composition)
    uint8 tanksNeeded = 0, healersNeeded = 0, dpsNeeded = 0;
    uint8 humanRole = sLFGMgr->GetRoles(playerGuid);
    CalculateNeededRoles(humanRole, tanksNeeded, healersNeeded, dpsNeeded);

    uint32 botsQueued = 0;

    // Queue tanks
    if ((neededRoles & lfg::PLAYER_ROLE_TANK) && tanksNeeded > 0)
    {
        std::vector<Player*> tanks = LFGBotSelector::instance()->FindTanks(minLevel, maxLevel, tanksNeeded);
        for (Player* tank : tanks)
        {
            if (QueueBot(tank, lfg::PLAYER_ROLE_TANK, dungeons))
            {
                RegisterBotAssignment(playerGuid, tank->GetGUID(), lfg::PLAYER_ROLE_TANK, dungeons);
                ++botsQueued;
            }
        }
    }

    // Queue healers
    if ((neededRoles & lfg::PLAYER_ROLE_HEALER) && healersNeeded > 0)
    {
        std::vector<Player*> healers = LFGBotSelector::instance()->FindHealers(minLevel, maxLevel, healersNeeded);
        for (Player* healer : healers)
        {
            if (QueueBot(healer, lfg::PLAYER_ROLE_HEALER, dungeons))
            {
                RegisterBotAssignment(playerGuid, healer->GetGUID(), lfg::PLAYER_ROLE_HEALER, dungeons);
                ++botsQueued;
            }
        }
    }

    // Queue DPS
    if ((neededRoles & lfg::PLAYER_ROLE_DAMAGE) && dpsNeeded > 0)
    {
        std::vector<Player*> dps = LFGBotSelector::instance()->FindDPS(minLevel, maxLevel, dpsNeeded);
        for (Player* dpsPlayer : dps)
        {
            if (QueueBot(dpsPlayer, lfg::PLAYER_ROLE_DAMAGE, dungeons))
            {
                RegisterBotAssignment(playerGuid, dpsPlayer->GetGUID(), lfg::PLAYER_ROLE_DAMAGE, dungeons);
                ++botsQueued;
            }
        }
    }

    // Register human player if we queued any bots
    if (botsQueued > 0)
    {
        auto& humanInfo = _humanPlayers[playerGuid];
        humanInfo.playerRole = humanRole;
        humanInfo.dungeons = dungeons;
    }

    return botsQueued;
}

bool LFGBotManager::IsBotQueued(ObjectGuid botGuid) const
{
    std::lock_guard lock(_mutex);
    return _queuedBots.find(botGuid) != _queuedBots.end();
}

void LFGBotManager::GetStatistics(uint32& totalQueued, uint32& totalAssignments) const
{
    std::lock_guard lock(_mutex);
    totalQueued = static_cast<uint32>(_queuedBots.size());
    totalAssignments = static_cast<uint32>(_humanPlayers.size());
}

void LFGBotManager::SetEnabled(bool enable)
{
    _enabled = enable;
    TC_LOG_INFO("module.playerbot", "LFG Bot Manager {}", enable ? "enabled" : "disabled");

    if (!enable)
    {
        // Remove all bots from queues when disabled
        std::lock_guard lock(_mutex);
        for (auto const& [botGuid, queueInfo] : _queuedBots)
        {
            if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
            {
                RemoveBotFromQueue(bot);
            }
        }
        _queuedBots.clear();
        _humanPlayers.clear();
        _proposalBots.clear();
    }
}

void LFGBotManager::CleanupStaleAssignments()
{
    std::lock_guard lock(_mutex);

    time_t currentTime = time(nullptr);
    std::vector<ObjectGuid> staleHumans;
    std::vector<ObjectGuid> staleBots;

    // Find stale bot assignments
    for (auto const& [botGuid, queueInfo] : _queuedBots)
    {
        // Check if bot has been queued too long
        if ((currentTime - queueInfo.queueTime) > MAX_QUEUE_TIME)
        {
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::CleanupStaleAssignments - Bot {} queue time exceeded, removing",
                         botGuid.ToString());
            staleBots.push_back(botGuid);
            continue;
        }

        // Check if bot still exists
        Player* bot = ObjectAccessor::FindPlayer(botGuid);
        if (!bot)
        {
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::CleanupStaleAssignments - Bot {} no longer exists, removing",
                         botGuid.ToString());
            staleBots.push_back(botGuid);
            continue;
        }

        // Check if bot is still actually in queue via LFGMgr
        lfg::LfgState botState = sLFGMgr->GetState(botGuid);
        if (botState != lfg::LFG_STATE_QUEUED && botState != lfg::LFG_STATE_PROPOSAL && botState != lfg::LFG_STATE_ROLECHECK)
        {
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::CleanupStaleAssignments - Bot {} not in valid LFG state ({}), removing",
                         botGuid.ToString(), botState);
            staleBots.push_back(botGuid);
        }
    }

    // Find stale human assignments
    for (auto const& [humanGuid, playerInfo] : _humanPlayers)
    {
        // Check if human still exists
        Player* human = ObjectAccessor::FindPlayer(humanGuid);
        if (!human)
        {
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::CleanupStaleAssignments - Human {} no longer exists, removing",
                         humanGuid.ToString());
            staleHumans.push_back(humanGuid);
            continue;
        }

        // Check if human is still in queue
        lfg::LfgState humanState = sLFGMgr->GetState(humanGuid);
        if (humanState != lfg::LFG_STATE_QUEUED && humanState != lfg::LFG_STATE_PROPOSAL && humanState != lfg::LFG_STATE_ROLECHECK)
        {
            TC_LOG_DEBUG("module.playerbot", "LFGBotManager::CleanupStaleAssignments - Human {} not in valid LFG state ({}), removing",
                         humanGuid.ToString(), humanState);
            staleHumans.push_back(humanGuid);
        }
    }

    // Remove stale bots
    for (ObjectGuid botGuid : staleBots)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }
        UnregisterBotAssignment(botGuid);
    }

    // Remove stale humans
    for (ObjectGuid humanGuid : staleHumans)
    {
        UnregisterAllBotsForPlayer(humanGuid);
    }

    if (!staleBots.empty() || !staleHumans.empty())
    {
        TC_LOG_DEBUG("module.playerbot", "LFGBotManager::CleanupStaleAssignments - Removed {} stale bots and {} stale humans",
                     staleBots.size(), staleHumans.size());
    }
}

void LFGBotManager::CalculateNeededRoles(uint8 humanRoles,
                                          uint8& tanksNeeded, uint8& healersNeeded, uint8& dpsNeeded) const
{
    // Standard 5-man dungeon composition
    // TODO: Add support for raid composition based on dungeon type
    tanksNeeded = lfg::LFG_TANKS_NEEDED;      // 1
    healersNeeded = lfg::LFG_HEALERS_NEEDED;  // 1
    dpsNeeded = lfg::LFG_DPS_NEEDED;          // 3

    // Subtract roles filled by human player
    if (humanRoles & lfg::PLAYER_ROLE_TANK)
    {
        tanksNeeded = (tanksNeeded > 0) ? tanksNeeded - 1 : 0;
    }
    if (humanRoles & lfg::PLAYER_ROLE_HEALER)
    {
        healersNeeded = (healersNeeded > 0) ? healersNeeded - 1 : 0;
    }
    if (humanRoles & lfg::PLAYER_ROLE_DAMAGE)
    {
        dpsNeeded = (dpsNeeded > 0) ? dpsNeeded - 1 : 0;
    }
}

bool LFGBotManager::QueueBot(Player* bot, uint8 role, lfg::LfgDungeonSet const& dungeons)
{
    if (!bot)
        return false;

    if (bot->GetGroup())
    {
        TC_LOG_WARN("module.playerbot", "LFGBotManager::QueueBot - Bot {} is already in a group", bot->GetName());
        return false;
    }

    // Check if bot has deserter debuff
    if (bot->HasAura(lfg::LFG_SPELL_DUNGEON_DESERTER))
    {
        TC_LOG_DEBUG("module.playerbot", "LFGBotManager::QueueBot - Bot {} has deserter debuff", bot->GetName());
        return false;
    }

    // Check if bot is already queued via LFGMgr
    lfg::LfgState botState = sLFGMgr->GetState(bot->GetGUID());
    if (botState == lfg::LFG_STATE_QUEUED || botState == lfg::LFG_STATE_PROPOSAL || botState == lfg::LFG_STATE_ROLECHECK)
    {
        TC_LOG_DEBUG("module.playerbot", "LFGBotManager::QueueBot - Bot {} is already queued (state: {})", bot->GetName(), botState);
        return false;
    }

    // Validate role for bot's class using LFGRoleDetector
    if (!sLFGRoleDetector->CanPerformRole(bot, role))
    {
        TC_LOG_WARN("module.playerbot", "LFGBotManager::QueueBot - Bot {} cannot perform role {}", bot->GetName(), role);
        return false;
    }
    uint8 validatedRole = role;

    // Create a mutable copy of dungeons for JoinLfg
    lfg::LfgDungeonSet dungeonsCopy = dungeons;

    // Queue the bot via LFGMgr
    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::QueueBot - Queueing bot {} as role {} for {} dungeons",
                 bot->GetName(), validatedRole, dungeonsCopy.size());

    sLFGMgr->JoinLfg(bot, validatedRole, dungeonsCopy);

    return true;
}

void LFGBotManager::RemoveBotFromQueue(Player* bot)
{
    if (!bot)
        return;

    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::RemoveBotFromQueue - Removing bot {} from LFG queue", bot->GetName());

    sLFGMgr->LeaveLfg(bot->GetGUID());
}

bool LFGBotManager::GetDungeonLevelRange(uint32 dungeonId, uint8& minLevel, uint8& maxLevel) const
{
    // Get dungeon info from LFGMgr
    uint32 dungeonEntry = sLFGMgr->GetLFGDungeonEntry(dungeonId);
    if (dungeonEntry == 0)
        return false;

    // TODO: Query actual level requirements from dungeon data
    // For now, use approximations based on dungeon ID ranges
    // This should be replaced with actual DBC/DB2 queries

    // Most dungeons are level-appropriate, use player level ±3
    // This is a simplification - real implementation should query lfg_dungeon_template or DBC
    minLevel = 1;
    maxLevel = 60; // Default range

    return true;
}

void LFGBotManager::RegisterBotAssignment(ObjectGuid humanGuid, ObjectGuid botGuid, uint8 role,
                                           lfg::LfgDungeonSet const& dungeons)
{
    // Add to human player's bot list
    auto& humanInfo = _humanPlayers[humanGuid];
    humanInfo.assignedBots.push_back(botGuid);

    // Add to bot queue info
    uint32 primaryDungeon = dungeons.empty() ? 0 : *dungeons.begin();
    _queuedBots[botGuid] = BotQueueInfo(humanGuid, role, dungeons, primaryDungeon);

    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::RegisterBotAssignment - Bot {} assigned to human {} with role {}",
                 botGuid.ToString(), humanGuid.ToString(), role);
}

void LFGBotManager::UnregisterBotAssignment(ObjectGuid botGuid)
{
    auto itr = _queuedBots.find(botGuid);
    if (itr == _queuedBots.end())
        return;

    ObjectGuid humanGuid = itr->second.humanPlayerGuid;

    // Remove from bot queue
    _queuedBots.erase(itr);

    // Remove from human player's bot list
    auto humanItr = _humanPlayers.find(humanGuid);
    if (humanItr != _humanPlayers.end())
    {
        auto& bots = humanItr->second.assignedBots;
        bots.erase(std::remove(bots.begin(), bots.end(), botGuid), bots.end());

        if (bots.empty())
        {
            _humanPlayers.erase(humanItr);
        }
    }

    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::UnregisterBotAssignment - Bot {} unregistered", botGuid.ToString());
}

std::vector<ObjectGuid> LFGBotManager::GetAssignedBots(ObjectGuid humanGuid) const
{
    auto itr = _humanPlayers.find(humanGuid);
    if (itr != _humanPlayers.end())
        return itr->second.assignedBots;

    return std::vector<ObjectGuid>();
}

void LFGBotManager::UnregisterAllBotsForPlayer(ObjectGuid humanGuid)
{
    auto itr = _humanPlayers.find(humanGuid);
    if (itr == _humanPlayers.end())
        return;

    TC_LOG_DEBUG("module.playerbot", "LFGBotManager::UnregisterAllBotsForPlayer - Removing {} bots for player {}",
                 itr->second.assignedBots.size(), humanGuid.ToString());

    for (ObjectGuid botGuid : itr->second.assignedBots)
    {
        if (Player* bot = ObjectAccessor::FindPlayer(botGuid))
        {
            RemoveBotFromQueue(bot);
        }
        _queuedBots.erase(botGuid);
    }

    _humanPlayers.erase(itr);
}

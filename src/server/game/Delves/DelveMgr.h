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

#ifndef TRINITY_DELVE_MGR_H
#define TRINITY_DELVE_MGR_H

#include "Define.h"
#include "DelvesDefines.h"
#include "Optional.h"
#include <unordered_map>
#include <vector>

class Creature;
class Player;
struct DelvesSeasonEntry;
struct PlayerCompanionInfoEntry;

namespace Delves
{

class TC_GAME_API DelveMgr
{
    DelveMgr();
    ~DelveMgr();

public:
    DelveMgr(DelveMgr const&) = delete;
    DelveMgr(DelveMgr&&) = delete;
    DelveMgr& operator=(DelveMgr const&) = delete;
    DelveMgr& operator=(DelveMgr&&) = delete;

    static DelveMgr* Instance();

    void Initialize();

    // Season
    uint32 GetActiveSeasonId() const { return _activeSeasonId; }
    DelvesSeasonEntry const* GetActiveSeason() const;

    // Templates
    DelveTemplate const* GetDelveTemplate(uint32 mapId) const;
    DelveTemplate const* GetDelveTemplateByChallengeModeId(uint32 mapChallengeModeId) const;
    DelveTemplate const* GetDelveTemplateByGossipMenuId(uint32 gossipMenuId) const;
    // Resolves an entrance NPC to the delve it opens. Every caller that starts from an entrance
    // creature must use this rather than GetDelveTemplateByGossipMenuId(creature->GetGossipMenuId()):
    // Creature::SetGossipMenuId() is never called anywhere in the core, so GetGossipMenuId() - the
    // "overridden by script" slot - is 0 for every DB-spawned creature and that lookup always misses.
    // See the implementation for the ordered fallback chain.
    DelveTemplate const* GetDelveTemplateForEntrance(Creature const* entrance) const;
    std::vector<DelveTemplate> const& GetAllDelveTemplates() const { return _delveTemplatesList; }

    // Tier rewards
    DelveTierReward const* GetTierReward(uint8 tier) const;

    // Bountiful
    bool IsDelveCurrentlyBountiful(uint32 delveTemplateId) const;
    std::vector<uint32> GetTodaysBountifulDelves() const;

    // Tiered-entrance pipeline (see TieredEntranceCVarNames documentation in
    // DelvesDefines.h for the IDA findings + obfuscation barrier).
    // Returns the PDE record id used by the client for a given tieredEntranceID.
    // Stub: returns std::nullopt because the client-side encoding rule is
    // anti-analysis-obfuscated. Will become a hardcoded (tieredEntranceID →
    // pdeRecordId) lookup once a retail sniff observes the mapping.
    static Optional<uint32> GetTieredEntrancePDEID(uint32 tieredEntranceId);

    // Returns the entrance type for a tieredEntranceID. Without the obfuscated
    // client lookup, the server falls back to inferring the type from the map:
    // any mapId that resolves via GetDelveTemplate is TIERED_ENTRANCE_TYPE_DELVE.
    static TieredEntranceType GetTieredEntranceType(uint32 tieredEntranceId);

    // Returns true if a given map should be treated as a tiered-entrance scenario.
    // Currently: true iff the map has a delve_template row.
    bool IsTieredEntranceScenarioMap(uint32 mapId) const;

    // Tier picker rows (delve_tiered_entrance_tier), ordered by tier
    std::vector<TieredEntranceTierData> const& GetTieredEntranceTiers() const { return _tieredEntranceTiers; }
    TieredEntranceTierData const* GetTieredEntranceTier(uint32 tieredEntranceTierId) const;

    // ---------------------------------------------------------------------------------------------
    // Retail run flow (12.1.0.69497 captures, C:\sniff\tcharvest\out\delve_research\REPORT.md).
    // Shared by WorldSession::Handle* (DelvesHandler.cpp) and the entrance / instance scripts.
    // ---------------------------------------------------------------------------------------------

    // Builds and sends SMSG_TIERED_ENTRANCE_OPEN_RESPONSE for the delve this entrance spawn opens (REPORT.md 1.3).
    // Used by the click path (CMSG_TIERED_ENTRANCE_OPEN) and by OpenEntranceByProximity.
    void SendTieredEntranceOpen(Player* player, Creature const* entrance);
    // REPORT.md 1.1 (work item 2): a delve entrance (creature 212407 / 251896) opens by itself when the player comes
    // into range - SMSG_NPC_INTERACTION_OPEN_RESULT(entrance, type 79) followed by the tier picker. Sent once per
    // approach: the open interaction is tracked in the player's InteractionData, cleared by CMSG_CLOSE_INTERACTION
    // (gulf 95751), by CloseEntranceByProximity and when the player leaves the map (Player::RemoveFromWorld).
    void OpenEntranceByProximity(Player* player, Creature const* entrance);
    // Counterpart for the entrance AI when the player leaves range without the client closing the interaction.
    void CloseEntranceByProximity(Player* player, Creature const* entrance);
    // REPORT.md 1.4 - 1.6 (work item 5): tier selected -> remember where to return, publish the selection and
    // seamlessly transfer to the delve map (SMSG_NEW_WORLD reason 21, no SMSG_TRANSFER_PENDING).
    void EnterDelve(Player* player, DelveTemplate const& tmpl, uint8 tier);
    // Called from the delve instance script's OnPlayerEnter: sets the observed world states on the delve map
    // (REPORT.md 1.6) and schedules the hidden entry quest reward once per run (REPORT.md 4, work item 6).
    void OnPlayerEnteredDelve(Player* player);
    // REPORT.md 5 (work item 11): load screen kit 79917 then seamless transfer to the exit coordinates on the
    // map the player entered from (fallback: delve_template exit map, then homebind).
    void LeaveDelve(Player* player);

private:
    void LoadDelveTemplates();
    void LoadTierRewards();
    void LoadTieredEntranceTiers();
    void DetermineActiveSeason();

    // Templates indexed by mapId
    std::unordered_map<uint32, DelveTemplate> _delveTemplatesByMap;
    // Templates indexed by MapChallengeModeId
    std::unordered_map<uint32, DelveTemplate const*> _delveTemplatesByChallengeModeId;
    // Templates indexed by GossipMenuId (the entrance NPC's gossip menu)
    std::unordered_map<uint32, DelveTemplate const*> _delveTemplatesByGossipMenuId;
    // Ordered list of all templates
    std::vector<DelveTemplate> _delveTemplatesList;
    // Tier rewards
    std::unordered_map<uint8, DelveTierReward> _tierRewards;
    // Active season
    uint32 _activeSeasonId = 0;
    std::vector<TieredEntranceTierData> _tieredEntranceTiers;
};

} // namespace Delves

#define sDelveMgr Delves::DelveMgr::Instance()

#endif // TRINITY_DELVE_MGR_H

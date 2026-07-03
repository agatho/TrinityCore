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

#ifndef TRINITY_DELVES_REWARDS_H
#define TRINITY_DELVES_REWARDS_H

#include "Define.h"
#include "DelvesDefines.h"

class Player;

namespace Delves
{

// Account-wide weekly progress for delves
struct DelveProgress
{
    uint8  HighestTierUnlocked = 3;   // Starts at 3 (tiers 1-3 always available)
    uint32 WeeklyCompletions = 0;     // Total delve completions this week
    uint8  HighestTierThisWeek = 0;   // For Great Vault reward calculation
    uint32 WeeklyBountifulCount = 0;  // Bountiful delves completed this week
    uint32 WeeklyCofferShards = 0;    // Coffer Key Shards earned this week
};

class TC_GAME_API DelvesRewards
{
public:
    // Core reward distribution (called on delve completion)
    static void AwardDelveCompletion(Player* player, uint8 tier, bool bountiful, bool hasRevivesRemaining);

    // Individual reward components
    static void AwardCrests(Player* player, uint8 tier);
    static void AwardCompanionXP(Player* player, uint8 tier);

    // Coffer key management
    static bool HasCofferKey(Player* player);
    static void ConsumeCofferKey(Player* player);
    static void AwardCofferKeyShards(Player* player, uint32 amount);

    // Great Vault tracking
    static void UpdateGreatVaultProgress(Player* player, uint8 tier);
    static uint8 GetGreatVaultSlotCount(uint32 weeklyCompletions);

    // Tier unlock validation
    static bool CanUnlockNextTier(uint8 currentHighest, uint8 completedTier, bool hasRevivesRemaining);

    // Progress persistence (account-wide)
    static void LoadProgress(uint32 battlenetAccountId, DelveProgress& progress);
    static void SaveProgress(uint32 battlenetAccountId, DelveProgress const& progress);
    static void ResetWeeklyProgress(uint32 battlenetAccountId, DelveProgress& progress);

    // Pushes DelveProgress to the client via the ActivePlayer JamDelveData mirror
    // (SMSG_UPDATE_OBJECT). Call after any progress mutation while the player is
    // online, and once on login. Key/field semantics UNVERIFIED — needs sniff
    // (see Player::SetDelveProgressData).
    static void PublishProgress(Player* player);
    static void PublishProgress(Player* player, DelveProgress const& progress);

    // Item context mapping (tier -> ItemContext enum value for loot generation)
    static uint8 GetItemContextForTier(uint8 tier, bool bountiful);

    // Crest type mapping
    enum CrestType : uint8
    {
        CREST_NONE      = 0,
        CREST_WEATHERED = 1,
        CREST_CARVED    = 2,
        CREST_RUNED     = 3,
        CREST_GILDED    = 4,
    };

    static CrestType GetCrestTypeForTier(uint8 tier);
};

} // namespace Delves

#endif // TRINITY_DELVES_REWARDS_H

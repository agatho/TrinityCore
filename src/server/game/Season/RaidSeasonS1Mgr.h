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

#ifndef RaidSeasonS1Mgr_h__
#define RaidSeasonS1Mgr_h__

#include "Define.h"
#include "DBCEnums.h"
#include <unordered_map>
#include <vector>

class Item;
class Player;

// ---------------------------------------------------------------------------
// Season 1 raid-season systems (TrinityCore 12.0.7 "Midnight" fork).
//
// Two related-but-distinct S1 systems live behind this manager:
//
//  1) Chiming Void Curio -- the S1 "omni-token": Item 249367 [DB2 ItemSparse
//     @68887, name "Chiming Void Curio", ExpansionID 11, desc "Find Kirana near
//     the entrance to the March on Quel'Danas to trade this for powerful class
//     set armor."]. It is redeemed at NPC Kirana for the class-set (Tier 35)
//     token of the player's choice at the MATCHING difficulty.
//
//     REDEMPTION MECHANISM (evidence): the Curio is NOT registered in
//     ItemConversion.db2 / ItemConversionEntry.db2 (verified: item 249367 has
//     zero rows in ItemConversionEntry @68887), so it is NOT a Matrix-Catalyst
//     Item-Interaction conversion. Its attested surface is a VENDOR TRADE
//     (npc_vendor + ItemExtendedCost item-cost). The shipped delivery is
//     therefore world SQL (vendor rows), NOT core code. This manager exposes a
//     thin server-side redemption seam only for validation / an optional
//     server-driven "pick any of your class's tokens" path, kept stubbed until
//     the Tier-35 token item ids are captured (CAPTURE-BLOCKED).
//
//  2) Sporefall flexible Mythic (15-25) -- the S1 raid, Map 1592 "Sporefall"
//     [DB2 Map @68887, Directory "DevMapE" (a DEV/work-in-progress map shell),
//     InstanceType 2 (raid), ExpansionID 11]. JournalInstance 1305 "Sporefall";
//     boss Rotmire (JournalEncounter 2711 / DungeonEncounter 3159, MapID 1592).
//     Its Mythic tier is the NEW flexible difficulty:
//       Difficulty 233 "Mythic - Flexible Raiding" MinPlayers=15 MaxPlayers=25
//       [DB2 Difficulty @68887] -- vs retail fixed-20 Mythic (Difficulty 16).
//     MapDifficulty 6168 binds Map 1592 -> Difficulty 233 (MaxPlayers 25,
//     ContentTuning 6119, level 90) [DB2 MapDifficulty @68887]. The full raid
//     difficulty set for 1592: LFR 17 / Normal 14 / Heroic 15 (all flex 10-30)
//     + Mythic-flex 233 (15-25).
//
//     FLEX-MYTHIC HOOK (core gap): TrinityCore's Difficulty enum does not know
//     233 (it stops at 153). InstanceMap::GetMaxPlayers() already reads the cap
//     (25) from MapDifficulty data, but the difficulty must be RECOGNISED as a
//     Mythic-style raid difficulty (heroic-style lockouts, selectable) and the
//     15-player floor made available to group-size logic. DIFFICULTY_MYTHIC_
//     RAID_FLEX = 233 is added to the enum; this manager centralises the
//     recognition helpers so callers do not sprinkle magic numbers.
//
// Evidence-vs-invention: every id below is source-anchored to DB2 @68887. No id
// is invented. Missing data (the Tier-35 token ids, the Curio difficulty bonus
// variants, the vendor/ExtendedCost ids) is left as CAPTURE-BLOCKED TODO, never
// fabricated.
// ---------------------------------------------------------------------------
namespace RaidSeasonS1
{
    // --- Chiming Void Curio (the omni-token) ---
    constexpr uint32 CURIO_ITEM_ID          = 249367;   // [DB2 ItemSparse @68887] "Chiming Void Curio"

    // --- Sporefall raid ---
    constexpr uint32 SPOREFALL_MAP_ID       = 1592;     // [DB2 Map @68887] Directory "DevMapE" (dev shell), raid
    constexpr uint32 SPOREFALL_JOURNAL_INST = 1305;     // [DB2 JournalInstance @68887] "Sporefall"
    constexpr uint32 ROTMIRE_JOURNAL_ENC    = 2711;     // [DB2 JournalEncounter @68887] "Rotmire"
    constexpr uint32 ROTMIRE_DUNGEON_ENC    = 3159;     // [DB2 DungeonEncounter @68887] MapID 1592

    // --- flexible Mythic difficulty ---
    // Difficulty 233 "Mythic - Flexible Raiding" MinPlayers 15 / MaxPlayers 25 [DB2 Difficulty @68887].
    constexpr Difficulty SPOREFALL_MYTHIC_FLEX = DIFFICULTY_MYTHIC_RAID_FLEX; // == 233
    constexpr uint32 FLEX_MYTHIC_MAPDIFFICULTY = 6168;  // [DB2 MapDifficulty @68887] Map 1592 -> Difficulty 233
    constexpr uint32 FLEX_MYTHIC_CONTENTTUNING = 6119;  // [DB2 ContentTuning @68887] level 90
    constexpr uint8  FLEX_MYTHIC_MIN_PLAYERS   = 15;    // [DB2 Difficulty.MinPlayers @68887]
    constexpr uint8  FLEX_MYTHIC_MAX_PLAYERS   = 25;    // [DB2 Difficulty.MaxPlayers @68887]
}

// Season-1 raid-season manager: constants registry + flex-Mythic recognition
// helpers + the (stubbed) Curio redemption seam. Realm-safe: LoadFromDB tolerates
// absent world tables (no S1 data => idle no-op).
class TC_GAME_API RaidSeasonS1Mgr
{
public:
    RaidSeasonS1Mgr(RaidSeasonS1Mgr const&) = delete;
    RaidSeasonS1Mgr& operator=(RaidSeasonS1Mgr const&) = delete;

    static RaidSeasonS1Mgr& Instance();

    // Loads optional world tables (raid_season_curio_reward). Call after DB2 + world DB load. Tolerant of empty.
    void Initialize();

    // --- flexible Mythic (15-25) recognition ---

    // True for the S1 flexible-Mythic raid difficulty (233). Centralises the magic number so
    // difficulty-dispatch sites do not hardcode it.
    static bool IsFlexMythicRaid(Difficulty difficulty);

    // True when (map, difficulty) is the Sporefall flexible-Mythic instance.
    static bool IsSporefallFlexMythic(uint32 mapId, Difficulty difficulty);

    // Player-count floor/ceiling for the flexible-Mythic difficulty (15 / 25).
    // The ceiling is also available (data-driven) via InstanceMap::GetMaxPlayers(); this exposes the floor.
    static uint8 GetFlexMythicMinPlayers();
    static uint8 GetFlexMythicMaxPlayers();

    // --- Chiming Void Curio redemption seam (stubbed; vendor is the shipped path) ---

    // True if the item is the Chiming Void Curio omni-token (249367).
    static bool IsCurio(Item const* item);

    // Server-side redemption entry point. The ATTESTED delivery is a vendor trade at NPC Kirana
    // (npc_vendor + ItemExtendedCost, shipped as world SQL). This seam exists only for validation /
    // an optional server-driven "pick any of your class's tokens" flow. It is intentionally a no-op
    // until the Tier-35 token item ids per (class, slot, difficulty) are captured -- CAPTURE-BLOCKED.
    // Returns the granted token item id, or 0 (no-op) when unmapped/blocked.
    uint32 RedeemCurio(Player* player, Item* curio, uint8 chosenInventoryType) const;

    // The Tier-35 token the Curio yields for (class, inventoryType, difficulty). 0 = unmapped (CAPTURE-BLOCKED).
    uint32 GetCurioReward(uint8 classId, uint8 inventoryType, Difficulty difficulty) const;

private:
    RaidSeasonS1Mgr() = default;
    ~RaidSeasonS1Mgr() = default;

    struct CurioRewardRow
    {
        uint8 ClassID = 0;          // 0 = any class
        uint8 InventoryType = 0;    // 0 = any slot
        Difficulty DifficultyId = DIFFICULTY_NONE; // 0 = any difficulty
        uint32 TokenItemID = 0;     // Tier-35 token granted -- CAPTURE-BLOCKED (no rows shipped)
    };

    // Keyed loosely; matched most-specific first in GetCurioReward. Empty until captured.
    std::vector<CurioRewardRow> _curioRewards;
};

#define sRaidSeasonS1Mgr RaidSeasonS1Mgr::Instance()

#endif // RaidSeasonS1Mgr_h__

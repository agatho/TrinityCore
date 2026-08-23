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

#ifndef ChallengeModeMgr_h__
#define ChallengeModeMgr_h__

#include "Define.h"
#include <array>
#include <unordered_map>
#include <vector>

class Item;
class Player;
struct MapChallengeModeEntry;
struct MythicPlusSeasonEntry;

// KeystoneAffix.db2 IDs (build 68275). Shared by the scaling engine and the per-run affix behaviours.
namespace ChallengeModeAffix
{
    // Legacy roster (pre-Midnight seasons; behaviours kept for operator-configured schedules)
    constexpr uint32 Raging      = 6;
    constexpr uint32 Bolstering  = 7;
    constexpr uint32 Sanguine    = 8;
    constexpr uint32 Tyrannical  = 9;   // scales bosses
    constexpr uint32 Fortified   = 10;  // scales non-boss enemies
    constexpr uint32 Bursting    = 11;
    constexpr uint32 Grievous    = 12;
    constexpr uint32 Spiteful    = 123;
    constexpr uint32 Storming    = 124;
    constexpr uint32 Entangling  = 134;
    constexpr uint32 Afflicted   = 135;
    constexpr uint32 Incorporeal = 136;

    // Midnight (12.x) roster
    constexpr uint32 XalatathsGuile              = 147; // +12+: revokes Bargain boons, 15s death penalty
    constexpr uint32 XalatathsBargainAscendant   = 148;
    constexpr uint32 XalatathsBargainVoidbound   = 158;
    constexpr uint32 XalatathsBargainDevour      = 160;
    constexpr uint32 XalatathsBargainPulsar      = 162;
    constexpr uint32 LindormisGuidance           = 165; // low keys: marked-trash training affix, no death penalty
}

// Global manager for Mythic Keystone (Challenge Mode) static data: the dungeon pool, per-map par times and
// keystone-upgrade thresholds, the season pool, and the per-level HP/damage scaling curve. Runtime per-run state
// lives on the InstanceMap (see ChallengeMode). Mirrors the GarrisonMgr singleton idiom.
class TC_GAME_API ChallengeModeMgr
{
public:
    ChallengeModeMgr();
    ChallengeModeMgr(ChallengeModeMgr const&) = delete;
    ChallengeModeMgr(ChallengeModeMgr&&) = delete;
    ChallengeModeMgr& operator=(ChallengeModeMgr const&) = delete;
    ChallengeModeMgr& operator=(ChallengeModeMgr&&) = delete;
    ~ChallengeModeMgr();

    static ChallengeModeMgr& Instance();

    void Initialize();

    // --- dungeon pool lookups ---
    MapChallengeModeEntry const* GetMapChallengeMode(uint32 challengeModeId) const;
    uint32 GetChallengeModeIdForMap(uint32 mapId) const;
    uint32 GetMapIdForChallengeMode(uint32 challengeModeId) const;

    // --- enemy forces ---
    // Kills of hostile non-boss creatures required for 100% Enemy Forces in a dungeon (challenge_mode_enemy_forces
    // world table; server content). 0 = no forces requirement (completion gates on bosses only, the pre-existing
    // behaviour), so the gate only engages for dungeons the operator has counted.
    uint32 GetEnemyForcesRequiredKills(uint32 challengeModeId) const;
    // Weighted forces (challenge_mode_enemy_forces_creature; retail model per CriteriaTree.db2 Enemy Forces
    // subtree): points a kill of this creature credits toward requiredKills. Empty optional = the dungeon has
    // no weight table (legacy 1-point-per-kill counting); 0 = weighted dungeon, this creature credits nothing
    // (retail: boss adds and unlisted spawns give no forces credit).
    Optional<uint32> GetEnemyForcesPoints(uint32 challengeModeId, uint32 creatureEntry) const;

    // --- timer / keystone upgrade (from MapChallengeMode.CriteriaCount: [0]=par, [1]=+2 @80%, [2]=+3 @60%) ---
    uint32 GetTimeLimit(uint32 challengeModeId) const;                     // par time, seconds
    std::array<uint32, 3> GetUpgradeThresholds(uint32 challengeModeId) const;
    // keystone levels gained on completion given time spent; 0 = over time (depleted / no upgrade)
    uint32 GetKeystoneUpgradeAmount(uint32 challengeModeId, uint32 timeUsedSeconds) const;

    // Per-run dungeon score (the client's "Mythic+ Rating" contribution). Implements the retail Midnight S1
    // formula (base 155 for a timed +2, +15/level, +15 per affix breakpoint at +5/+7/+10/+12, up to +15 time
    // bonus at 40% under par, decay to 0 at 40% over). The constants are community-derived (not in client/DB2),
    // so every term is config-tunable (ChallengeMode.Score.*).
    float CalculateRunScore(uint32 keystoneLevel, uint32 effectiveTimeMs, uint32 timeLimitMs) const;

    // --- Blizzlike scaling engine: creature HP/damage multiplier by keystone level ---
    // Reproduces the client's C_ChallengeMode.GetPowerLevelDamageHealthMod via GlobalCurve
    // ChallengeModeHealth(21)/ChallengeModeDamage(22) -> CurvePoint (68275: CurveID 61692/61693, ~+10%/level).
    float GetHealthMultiplier(uint32 keystoneLevel) const;
    float GetDamageMultiplier(uint32 keystoneLevel) const;

    // --- affix scaling (Fortified / Tyrannical) ---
    // Fortified boosts non-boss enemies; Tyrannical boosts bosses. Applied on top of the keystone-level
    // scaling above. The multipliers are a client-hardcoded affix effect (not in KeystoneAffix.db2 or a
    // GlobalCurve, and not traceable offline), so they are config-tunable (ChallengeMode.Affix.*) with
    // documented current-patch defaults. `affixes` is the run's active set; `isBoss` selects which applies.
    float GetAffixHealthMultiplier(std::array<uint32, 4> const& affixes, bool isBoss) const;
    float GetAffixDamageMultiplier(std::array<uint32, 4> const& affixes, bool isBoss) const;

    // Effect spell for a behavioural affix (Bolstering buff, Bursting DoT, ...). The affix effects are Blizzard
    // spells that must exist in the loaded Spell.db2; the caller guards on GetSpellInfo, so a 0/absent id is a
    // safe no-op. IDs are config-tunable (ChallengeMode.Affix.<Name>.SpellId) with the long-stable defaults, to
    // be verified against the live build. Returns 0 for an affix with no on-effect spell.
    uint32 GetAffixSpellId(uint32 affixId) const;

    // Summoned-add creature entry for a spawn affix (Spiteful Shade, ...). The creature (and its Blizzlike AI) must
    // exist in the world DB; SummonCreature no-ops on an absent entry. Config-tunable
    // (ChallengeMode.Affix.<Name>.CreatureId). Returns 0 (disabled) for affixes without a configured spawn.
    uint32 GetAffixCreatureId(uint32 affixId) const;

    // --- end-of-run crest reward ---
    // Midnight S1 Dawncrest ladder: Champion crests at +2-3, Hero at +4-8, Myth at +9+ (currency ids from
    // CurrencyTypes.db2; live ids 3343/3346/3348 per wowhead - 3345/3347 were PTR values, hence config-tunable).
    // Amount = the bracket's base + AmountPerLevel per keystone level into the bracket, growth capped at
    // AmountCapLevel (retail: +2=12C ... +8=18H, +9=10M ... +12+=16M); untimed runs lose UntimedReduction crests.
    uint32 GetCrestCurrencyForLevel(uint32 keystoneLevel) const;
    uint32 GetCrestAmountForLevel(uint32 keystoneLevel, bool timed) const;

    // Reference-loot template rolled for the end-of-run gear reward (reference_loot_template, ItemContext
    // MythicPlus_End_of_Run). The reward item POOL is server content; the item level is scaled authentically by
    // ItemBonusMgr from the keystone level. Config-tunable (ChallengeMode.Reward.LootId); 0 disables the gear drop.
    uint32 GetGearRewardLootId() const;

    // Reference-loot template rolled for the Great Vault reward options (ItemContext MythicPlus_Jackpot). Same
    // model as the end-of-run drop but at the weekly-vault item level. Config-tunable (ChallengeMode.Vault.LootId);
    // 0 disables vault rewards (progress is still tracked and shown).
    uint32 GetVaultRewardLootId() const;

    // --- Great Vault thresholds (WeeklyRewardChestThreshold.db2) ---
    // One reward slot: the DB2 threshold row id, its slot index (0/1/2) and the run count required to unlock it.
    struct VaultThreshold
    {
        uint32 ThresholdID = 0;
        uint32 Index = 0;
        uint32 Count = 0;
    };
    // The three live Mythic+ vault thresholds (Type=MythicPlus), one per slot. The DB2 keeps every past season's
    // rows with duplicate (Type,Index); per the client rule the highest-ID row per index is the live one, so this
    // auto-tracks the current season (68275: ids 202/203/204 -> counts 1/4/8) with no hardcoding.
    std::vector<VaultThreshold> GetMythicPlusVaultThresholds() const;

    // --- Great Vault reward service (Mythic+ row) ---
    // The concrete reward-item logic for the Mythic+ vault row lives HERE, not in a packet handler, because the
    // CMSG_REQUEST_WEEKLY_REWARDS / CMSG_CLAIM_WEEKLY_REWARD binding is owned by whichever weekly-rewards handler
    // an assembly ships (this branch: ChallengeModeHandler.cpp over the ChallengeMode packet family;
    // integration/all-systems: WeeklyRewardHandler.cpp over the WeeklyRewards packet family, which also serves the
    // Raid and World rows). Both call these entry points, so the M+ granting rules exist exactly once and no
    // second handler is ever bound to the same opcode.
    //
    // Everything below is keyed on the vault SLOT INDEX (0/1/2) - the one concept both packet families share.
    // GetMythicPlusVaultSlotForThreshold() maps a WeeklyRewardChestThreshold.db2 id onto it.
    static constexpr uint32 VAULT_SLOT_NONE = 0xFFFFFFFF;

    // One previewed/claimable reward for a single unlocked Mythic+ vault slot. ItemID/BonusListIDs are empty when
    // the server's vault reward pool (ChallengeMode.Vault.LootId) is not configured - the slot is still reported
    // so the client shows the unlocked row rather than a fabricated item.
    struct VaultRewardOption
    {
        uint32 ThresholdID = 0;
        uint32 SlotIndex = 0;
        uint32 RewardLevel = 0;             // keystone level the reward scales at, season cap applied
        uint32 ItemID = 0;
        std::vector<int32> BonusListIDs;
    };

    enum class VaultClaimResult : uint8
    {
        Success = 0,
        NotClaimable = 1,                   // no data, already claimed this week, or the slot is still locked
        RewardPoolUnavailable = 2           // slot is valid but the server has no vault loot configured
    };

    // Slot index for a WeeklyRewardChestThreshold.db2 id, or VAULT_SLOT_NONE when the id is not a live M+ slot.
    uint32 GetMythicPlusVaultSlotForThreshold(uint32 thresholdId) const;
    // Reward-scaling keystone level for a slot (0 = locked): the level of the Nth-best run this week, clamped to
    // the active season's MythicPlusSeasonRewardLevels cap.
    uint32 GetMythicPlusVaultSlotRewardLevel(Player* player, uint32 slotIndex) const;
    // One option per unlocked slot, each rolled from the vault reference-loot pool and scaled by ItemBonusMgr at
    // ItemContext::MythicPlus_Jackpot for that slot's level. Preview only - nothing is granted.
    std::vector<VaultRewardOption> BuildMythicPlusVaultOptions(Player* player) const;
    // Rolls and grants the reward for an unlocked slot (bags, or mail when the bags are full) and locks the vault
    // for the rest of the week. The week is only consumed on Success.
    VaultClaimResult ClaimMythicPlusVaultReward(Player* player, uint32 slotIndex) const;

    // --- season / pool / affixes ---
    uint32 GetActiveSeasonId() const { return _activeSeasonId; }
    // The active display season (MythicPlusSeasonTrackedMap/TrackedAffix/KeyFloor key). Auto-detected as the
    // newest season present in MythicPlusSeasonTrackedMap.db2; override with ChallengeMode.DisplaySeasonId.
    uint32 GetDisplaySeasonId() const { return _displaySeasonId; }
    MythicPlusSeasonEntry const* GetActiveSeason() const;
    std::vector<uint32> const& GetSeasonMapChallengeModeIds() const { return _seasonMaps; }

    // Resilient Keystone floor (MythicPlusSeasonKeyFloor.db2): the highest KeyFloor of the active display
    // season whose PlayerCondition the player meets. Weekly adjustment and depletion never go below it.
    uint32 GetKeystoneFloor(Player const* player) const;

    // Great Vault reward levels (MythicPlusSeasonRewardLevels.db2, active season): the key level reward scaling
    // caps at (0 = uncapped/no data), and the activity tier id the vault UI expects for the M+ row.
    uint32 GetVaultRewardLevelCap() const;
    int32 GetVaultActivityTierId() const;
    // The full weekly affix set (all bands), as advertised to the client in SMSG_MYTHIC_PLUS_CURRENT_AFFIXES.
    std::vector<uint32> GetWeeklyAffixes() const;
    // Affixes active for a given keystone level this week, in keystone slot order. The Midnight S1 rotation
    // (Guidance / weekly Bargain / Tyrannical-Fortified alternation / Guile) is built in and week-indexed off the
    // weekly reset; ChallengeMode.AffixSchedule overrides it verbatim when set (see LoadAffixRotation()).
    std::vector<uint32> GetActiveAffixes(uint32 keystoneLevel) const;
    // Rotation week index (weeks since epoch at the current weekly-reset boundary, plus config offset).
    uint32 GetCurrentWeekIndex() const;

    // --- keystone item service ---
    // The Mythic Keystone item (12.x: 180653), config-tunable. All keystone state lives in item modifiers
    // 17 (dungeon) / 18 (level) / 19-22 (affixes, level-band gated) -- the tooltip renders from these.
    uint32 GetKeystoneItemId() const;
    uint32 GetKeystoneMinLevel() const;
    // The player's keystone item, or nullptr (the item is unique, so first match wins).
    Item* GetKeystone(Player* player) const;
    // Writes dungeon/level and the week's level-gated affixes into the keystone item modifiers.
    void StampKeystone(Item* keystone, uint32 challengeModeId, uint32 keystoneLevel) const;
    // Creates the keystone (or restamps an existing one) for the player. Returns the item, or nullptr on failure.
    Item* CreateOrUpdateKeystone(Player* player, uint32 challengeModeId, uint32 keystoneLevel) const;
    // A random dungeon from the season pool, avoiding excludeChallengeModeId when the pool has alternatives.
    uint32 RollSeasonDungeon(uint32 excludeChallengeModeId = 0) const;
    // Mythic (M0) season-dungeon completion hook: awards a fresh minimum-level keystone to players without one.
    void OnMythicDungeonCompleted(Player* player) const;
    // Weekly keystone maintenance: adjusts the level from last week's runs and restamps the current week's
    // affixes; grants a fresh keystone when createIfMissing (the vault-open rule). Driven from three places -
    // the weekly reset itself (OnWeeklyReset, online characters), character login (characters that were offline
    // at the reset) and the Great Vault open (createIfMissing).
    void UpdateKeystoneForNewWeek(Player* player, bool createIfMissing) const;

    // --- weekly reset ---
    // Invoked by World::ResetWeeklyQuests once the world's new weekly boundary is in place. Rolls every ONLINE
    // character's Mythic+ week over immediately: the vault run history for the finished week is dropped (after
    // its summary is captured and persisted for the keystone rule) and the carried keystone is re-issued at the
    // retail level with the new week's affixes. Characters that are offline at the reset are rolled over on their
    // next login - every piece of weekly state is keyed on the reset boundary, so it is a pure catch-up, and this
    // mirrors how the core already handles per-character weekly state that lives in item modifiers.
    void OnWeeklyReset() const;

private:
    void LoadScalingCurves();
    void LoadMapPool();
    void ResolveActiveSeason();
    void LoadAffixRotation();
    void LoadEnemyForces();

    std::unordered_map<uint32 /*challengeModeId*/, MapChallengeModeEntry const*> _mapChallengeModes;
    std::unordered_map<uint32 /*mapId*/, uint32 /*challengeModeId*/> _challengeModeByMap;
    std::unordered_map<uint32 /*challengeModeId*/, uint32 /*requiredKills*/> _enemyForces;
    std::unordered_map<uint32 /*challengeModeId*/, std::unordered_map<uint32 /*creatureEntry*/, uint32 /*points*/>> _enemyForcesWeights;

    uint32 _healthCurveId = 0;
    uint32 _damageCurveId = 0;

    uint32 _activeSeasonId = 0;
    uint32 _displaySeasonId = 0;
    std::vector<uint32> _seasonMaps;

    // Weekly affix schedule: _affixSchedule[band] = keystoneAffixId, where band index maps to a level threshold
    // in _affixLevelBands (parallel arrays). Populated from config so values drop in without a rebuild.
    std::vector<uint32> _affixSchedule;
    std::vector<uint32> _affixLevelBands;
};

#define sChallengeModeMgr ChallengeModeMgr::Instance()

#endif // ChallengeModeMgr_h__

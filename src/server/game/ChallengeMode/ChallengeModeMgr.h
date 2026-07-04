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

struct MapChallengeModeEntry;
struct MythicPlusSeasonEntry;

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

    // --- timer / keystone upgrade (from MapChallengeMode.CriteriaCount: [0]=par, [1]=+2 @80%, [2]=+3 @60%) ---
    uint32 GetTimeLimit(uint32 challengeModeId) const;                     // par time, seconds
    std::array<uint32, 3> GetUpgradeThresholds(uint32 challengeModeId) const;
    // keystone levels gained on completion given time spent; 0 = over time (depleted / no upgrade)
    uint32 GetKeystoneUpgradeAmount(uint32 challengeModeId, uint32 timeUsedSeconds) const;

    // Per-run dungeon score (the client's "Mythic+ Rating" contribution). The exact retail constants are a
    // server-side design value (not present in the client binary or DB2), so the base-per-level and the
    // time bonus/penalty are config-tunable (ChallengeMode.Score*) to be matched to a sniff without a rebuild.
    // The shape is Blizzlike: score grows with keystone level, with a bonus for beating par and a penalty for
    // running over. affixCount contributes a small per-affix bonus.
    float CalculateRunScore(uint32 keystoneLevel, uint32 effectiveTimeMs, uint32 timeLimitMs, uint32 affixCount) const;

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

    // --- season / pool / affixes ---
    uint32 GetActiveSeasonId() const { return _activeSeasonId; }
    MythicPlusSeasonEntry const* GetActiveSeason() const;
    std::vector<uint32> const& GetSeasonMapChallengeModeIds() const { return _seasonMaps; }
    // The full weekly affix set (all bands), as advertised to the client in SMSG_MYTHIC_PLUS_CURRENT_AFFIXES.
    std::vector<uint32> const& GetWeeklyAffixes() const { return _affixSchedule; }
    // Affixes active for a given keystone level this week (level-band gated). Rotation is config/season driven
    // (no offline DB2 rotation table exists); see worldserver.conf ChallengeMode.* and LoadAffixRotation().
    std::vector<uint32> GetActiveAffixes(uint32 keystoneLevel) const;

private:
    void LoadScalingCurves();
    void LoadMapPool();
    void ResolveActiveSeason();
    void LoadAffixRotation();

    std::unordered_map<uint32 /*challengeModeId*/, MapChallengeModeEntry const*> _mapChallengeModes;
    std::unordered_map<uint32 /*mapId*/, uint32 /*challengeModeId*/> _challengeModeByMap;

    uint32 _healthCurveId = 0;
    uint32 _damageCurveId = 0;

    uint32 _activeSeasonId = 0;
    std::vector<uint32> _seasonMaps;

    // Weekly affix schedule: _affixSchedule[band] = keystoneAffixId, where band index maps to a level threshold
    // in _affixLevelBands (parallel arrays). Populated from config so values drop in without a rebuild.
    std::vector<uint32> _affixSchedule;
    std::vector<uint32> _affixLevelBands;
};

#define sChallengeModeMgr ChallengeModeMgr::Instance()

#endif // ChallengeModeMgr_h__

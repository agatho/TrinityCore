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

#include "ChallengeModeMgr.h"
#include "Config.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "DBCEnums.h"
#include "Log.h"
#include <algorithm>
#include <sstream>

namespace
{
    // Parse a comma-separated "1,2,3" config string into a uint32 vector.
    std::vector<uint32> ParseUInt32List(std::string const& value)
    {
        std::vector<uint32> result;
        std::stringstream ss(value);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            try
            {
                std::size_t pos = 0;
                unsigned long v = std::stoul(token, &pos);
                if (pos)
                    result.push_back(uint32(v));
            }
            catch (std::exception const&) { }
        }
        return result;
    }
}

ChallengeModeMgr::ChallengeModeMgr() = default;
ChallengeModeMgr::~ChallengeModeMgr() = default;

ChallengeModeMgr& ChallengeModeMgr::Instance()
{
    static ChallengeModeMgr instance;
    return instance;
}

void ChallengeModeMgr::Initialize()
{
    LoadScalingCurves();
    LoadMapPool();
    ResolveActiveSeason();
    LoadAffixRotation();
}

void ChallengeModeMgr::LoadScalingCurves()
{
    _healthCurveId = sDB2Manager.GetGlobalCurveId(GlobalCurve::ChallengeModeHealth);
    _damageCurveId = sDB2Manager.GetGlobalCurveId(GlobalCurve::ChallengeModeDamage);

    if (!_healthCurveId || !_damageCurveId)
        TC_LOG_ERROR("server.loading", "ChallengeModeMgr: missing GlobalCurve for ChallengeMode scaling "
            "(health={}, damage={}); Mythic+ creature scaling will be disabled.", _healthCurveId, _damageCurveId);
    else
        TC_LOG_INFO("server.loading", "ChallengeModeMgr: scaling curves health={} damage={} (per-level HP/damage curve).",
            _healthCurveId, _damageCurveId);
}

void ChallengeModeMgr::LoadMapPool()
{
    _mapChallengeModes.clear();
    _challengeModeByMap.clear();
    for (MapChallengeModeEntry const* entry : sMapChallengeModeStore)
    {
        _mapChallengeModes[entry->ID] = entry;
        _challengeModeByMap[entry->MapID] = entry->ID;
    }
    TC_LOG_INFO("server.loading", "ChallengeModeMgr: loaded {} Mythic+ dungeon definitions.", _mapChallengeModes.size());
}

void ChallengeModeMgr::ResolveActiveSeason()
{
    // Config override; 0 = auto-detect the latest-started season of the highest expansion. The concrete active
    // season is a runtime pointer in retail (not a static DB2 value) -- prefer setting ChallengeMode.SeasonId.
    _activeSeasonId = uint32(sConfigMgr->GetIntDefault("ChallengeMode.SeasonId", 0));
    if (!_activeSeasonId)
    {
        int32 bestExpansion = -1;
        int32 bestStart = -1;
        for (MythicPlusSeasonEntry const* season : sMythicPlusSeasonStore)
        {
            if (season->ExpansionLevel > bestExpansion
                || (season->ExpansionLevel == bestExpansion && season->StartTimeEvent > bestStart))
            {
                bestExpansion = season->ExpansionLevel;
                bestStart = season->StartTimeEvent;
                _activeSeasonId = season->ID;
            }
        }
    }

    // Season dungeon pool: filter the full pool by the active season's expansion. The exact tracked-map set lives
    // in MythicPlusSeasonTrackedMap.db2; until that is wired, the expansion filter is the Blizzlike approximation.
    _seasonMaps.clear();
    if (MythicPlusSeasonEntry const* season = GetActiveSeason())
    {
        for (auto const& [challengeModeId, entry] : _mapChallengeModes)
            if (int32(entry->ExpansionLevel) == season->ExpansionLevel)
                _seasonMaps.push_back(challengeModeId);
    }

    TC_LOG_INFO("server.loading", "ChallengeModeMgr: active Mythic+ season {} ({} dungeons in pool).",
        _activeSeasonId, _seasonMaps.size());
}

void ChallengeModeMgr::LoadAffixRotation()
{
    // Weekly affix rotation ordering + level bands are server-side config in retail (no offline DB2 rotation table).
    // Sourced from worldserver.conf so operators can match the live schedule without a rebuild:
    //   ChallengeMode.AffixSchedule    = comma list of KeystoneAffix IDs applied this week (lowest band first)
    //   ChallengeMode.AffixLevelBands  = comma list of keystone levels at which each successive affix turns on
    _affixSchedule = ParseUInt32List(sConfigMgr->GetStringDefault("ChallengeMode.AffixSchedule", ""));
    _affixLevelBands = ParseUInt32List(sConfigMgr->GetStringDefault("ChallengeMode.AffixLevelBands", ""));

    if (_affixSchedule.empty())
        TC_LOG_INFO("server.loading", "ChallengeModeMgr: no ChallengeMode.AffixSchedule configured; runs will start "
            "without affixes until set.");
}

MapChallengeModeEntry const* ChallengeModeMgr::GetMapChallengeMode(uint32 challengeModeId) const
{
    auto itr = _mapChallengeModes.find(challengeModeId);
    return itr != _mapChallengeModes.end() ? itr->second : nullptr;
}

uint32 ChallengeModeMgr::GetChallengeModeIdForMap(uint32 mapId) const
{
    auto itr = _challengeModeByMap.find(mapId);
    return itr != _challengeModeByMap.end() ? itr->second : 0;
}

uint32 ChallengeModeMgr::GetMapIdForChallengeMode(uint32 challengeModeId) const
{
    if (MapChallengeModeEntry const* entry = GetMapChallengeMode(challengeModeId))
        return entry->MapID;
    return 0;
}

uint32 ChallengeModeMgr::GetTimeLimit(uint32 challengeModeId) const
{
    if (MapChallengeModeEntry const* entry = GetMapChallengeMode(challengeModeId))
        return uint32(std::max<int16>(entry->CriteriaCount[0], 0));
    return 0;
}

std::array<uint32, 3> ChallengeModeMgr::GetUpgradeThresholds(uint32 challengeModeId) const
{
    if (MapChallengeModeEntry const* entry = GetMapChallengeMode(challengeModeId))
        return { uint32(std::max<int16>(entry->CriteriaCount[0], 0)),
                 uint32(std::max<int16>(entry->CriteriaCount[1], 0)),
                 uint32(std::max<int16>(entry->CriteriaCount[2], 0)) };
    return { 0, 0, 0 };
}

uint32 ChallengeModeMgr::GetKeystoneUpgradeAmount(uint32 challengeModeId, uint32 timeUsedSeconds) const
{
    std::array<uint32, 3> thresholds = GetUpgradeThresholds(challengeModeId);
    if (!thresholds[0] || timeUsedSeconds > thresholds[0])
        return 0;                               // over the par time -> depleted, no upgrade
    if (thresholds[2] && timeUsedSeconds <= thresholds[2])
        return 3;                               // beat the +3 threshold (<= 60% of par)
    if (thresholds[1] && timeUsedSeconds <= thresholds[1])
        return 2;                               // beat the +2 threshold (<= 80% of par)
    return 1;                                    // in time -> +1
}

float ChallengeModeMgr::CalculateRunScore(uint32 keystoneLevel, uint32 effectiveTimeMs, uint32 timeLimitMs, uint32 affixCount) const
{
    if (!keystoneLevel)
        return 0.0f;

    // Config-tunable Blizzlike-shaped rating. Constants are a server design value (not in the client/DB2);
    // defaults approximate the retail shape and can be matched to a live sniff without a rebuild.
    float const perLevel = sConfigMgr->GetFloatDefault("ChallengeMode.ScorePerLevel", 7.5f);
    float const perAffix = sConfigMgr->GetFloatDefault("ChallengeMode.ScorePerAffix", 5.0f);
    float const maxTimeBonus = sConfigMgr->GetFloatDefault("ChallengeMode.ScoreMaxTimeBonus", 7.5f);

    float score = keystoneLevel * perLevel + affixCount * perAffix;

    // Time bonus: +maxTimeBonus for a 40%-under-par clear, tapering to -maxTimeBonus at/over par.
    if (timeLimitMs)
    {
        float const parRatio = float(effectiveTimeMs) / float(timeLimitMs); // < 1.0 = under par
        float const t = std::clamp((1.0f - parRatio) / 0.4f, -1.0f, 1.0f);
        score += t * maxTimeBonus;
    }

    return std::max(0.0f, score);
}

float ChallengeModeMgr::GetHealthMultiplier(uint32 keystoneLevel) const
{
    if (!_healthCurveId || !keystoneLevel)
        return 1.0f;
    return sDB2Manager.GetCurveValueAt(_healthCurveId, float(keystoneLevel));
}

float ChallengeModeMgr::GetDamageMultiplier(uint32 keystoneLevel) const
{
    if (!_damageCurveId || !keystoneLevel)
        return 1.0f;
    return sDB2Manager.GetCurveValueAt(_damageCurveId, float(keystoneLevel));
}

namespace
{
    bool HasAffixId(std::array<uint32, 4> const& affixes, uint32 affixId)
    {
        return std::find(affixes.begin(), affixes.end(), affixId) != affixes.end();
    }
}

float ChallengeModeMgr::GetAffixHealthMultiplier(std::array<uint32, 4> const& affixes, bool isBoss) const
{
    // Only one of the pair applies to a given creature (Tyrannical -> bosses, Fortified -> everything else).
    if (isBoss && HasAffixId(affixes, ChallengeModeAffix::Tyrannical))
        return sConfigMgr->GetFloatDefault("ChallengeMode.Affix.Tyrannical.Health", 1.30f);
    if (!isBoss && HasAffixId(affixes, ChallengeModeAffix::Fortified))
        return sConfigMgr->GetFloatDefault("ChallengeMode.Affix.Fortified.Health", 1.20f);
    return 1.0f;
}

float ChallengeModeMgr::GetAffixDamageMultiplier(std::array<uint32, 4> const& affixes, bool isBoss) const
{
    if (isBoss && HasAffixId(affixes, ChallengeModeAffix::Tyrannical))
        return sConfigMgr->GetFloatDefault("ChallengeMode.Affix.Tyrannical.Damage", 1.15f);
    if (!isBoss && HasAffixId(affixes, ChallengeModeAffix::Fortified))
        return sConfigMgr->GetFloatDefault("ChallengeMode.Affix.Fortified.Damage", 1.30f);
    return 1.0f;
}

uint32 ChallengeModeMgr::GetAffixSpellId(uint32 affixId) const
{
    switch (affixId)
    {
        case ChallengeModeAffix::Bolstering: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Bolstering.SpellId", 209859)); // +20% max health & damage to nearby allies
        case ChallengeModeAffix::Bursting:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Bursting.SpellId", 240443));   // stacking damage-over-time on all players
        case ChallengeModeAffix::Sanguine:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Sanguine.SpellId", 226489));   // lingering ichor pool (heals allies / damages players)
        case ChallengeModeAffix::Raging:     return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Raging.SpellId", 228318));     // enrage at low health
        case ChallengeModeAffix::Grievous:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Grievous.SpellId", 240559));   // stacking bleed on wounded players
        default: return 0;
    }
}

uint32 ChallengeModeMgr::GetAffixCreatureId(uint32 affixId) const
{
    switch (affixId)
    {
        case ChallengeModeAffix::Spiteful:    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Spiteful.CreatureId", 174773));  // Spiteful Shade (fixate + self-decay AI)
        case ChallengeModeAffix::Incorporeal: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Incorporeal.CreatureId", 204560)); // Incorporeal Being (verify per build)
        case ChallengeModeAffix::Afflicted:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Afflicted.CreatureId", 0));        // needs verified entry per build
        default: return 0;
    }
}

MythicPlusSeasonEntry const* ChallengeModeMgr::GetActiveSeason() const
{
    return sMythicPlusSeasonStore.LookupEntry(_activeSeasonId);
}

std::vector<uint32> ChallengeModeMgr::GetActiveAffixes(uint32 keystoneLevel) const
{
    std::vector<uint32> affixes;
    for (std::size_t i = 0; i < _affixSchedule.size(); ++i)
    {
        uint32 requiredLevel = i < _affixLevelBands.size() ? _affixLevelBands[i] : 0;
        if (keystoneLevel >= requiredLevel)
            affixes.push_back(_affixSchedule[i]);
    }
    return affixes;
}

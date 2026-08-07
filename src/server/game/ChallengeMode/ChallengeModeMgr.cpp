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
#include "CharacterDatabase.h"
#include "ConditionMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "DBCEnums.h"
#include "Item.h"
#include "ItemBonusMgr.h"
#include "ItemDefines.h"
#include "Log.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "MythicPlusData.h"
#include "Player.h"
#include "Random.h"
#include "World.h"
#include "WorldSession.h"
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

    // Rolls the vault reward pool once (personal loot, tagged MythicPlus_Jackpot) and returns a single item id,
    // or 0 when nothing rolled. The pool itself is server content (reference_loot_template).
    uint32 RollVaultRewardItem(Player* player, uint32 lootId)
    {
        Loot loot(player->GetMap(), ObjectGuid::Empty, LOOT_NONE, nullptr);
        loot.FillLoot(lootId, LootTemplates_Reference, player, true /*personal*/, true /*noEmptyError*/,
            LOOT_MODE_DEFAULT, ItemContext::MythicPlus_Jackpot);
        for (LootItem const& item : loot.items)
            if (item.itemid)
                return item.itemid;
        return 0;
    }

    // Item bonuses that scale a vault reward to its authentic item level. The ilvl is never a hardcoded number:
    // ItemBonusMgr derives it from (ItemContext::MythicPlus_Jackpot, keystone level) through the client's own
    // ItemBonus / ItemLevelSelector DB2 chain.
    std::vector<int32> VaultRewardBonuses(uint32 itemId, int32 rewardLevel)
    {
        return ItemBonusMgr::GetBonusListsForItem(itemId,
            ItemBonusMgr::ItemBonusGenerationParams(ItemContext::MythicPlus_Jackpot, rewardLevel));
    }

    // Grants one item carrying the given scaled bonuses; falls back to mail when the bags are full so a claim is
    // never silently swallowed.
    void GrantVaultItem(Player* player, uint32 itemId, std::vector<int32> const& bonuses)
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1) == EQUIP_ERR_OK)
        {
            player->StoreNewItem(dest, itemId, true, 0, GuidSet(), ItemContext::MythicPlus_Jackpot, &bonuses);
            return;
        }

        if (Item* item = Item::CreateItem(itemId, 1, ItemContext::MythicPlus_Jackpot, player, false))
        {
            item->SetBonuses(bonuses);
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            item->SaveToDB(trans);
            MailDraft("Great Vault Reward", "Your Great Vault reward.")
                .AddItem(item)
                .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
            CharacterDatabase.CommitTransaction(trans);
        }
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
    LoadEnemyForces();
}

void ChallengeModeMgr::LoadEnemyForces()
{
    _enemyForces.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT challengeModeId, requiredKills FROM challenge_mode_enemy_forces"))
    {
        do
        {
            Field* fields = result->Fetch();
            _enemyForces[fields[0].GetUInt32()] = fields[1].GetUInt32();
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", "ChallengeModeMgr: loaded enemy-forces requirements for {} dungeons.", _enemyForces.size());
}

uint32 ChallengeModeMgr::GetEnemyForcesRequiredKills(uint32 challengeModeId) const
{
    auto itr = _enemyForces.find(challengeModeId);
    return itr != _enemyForces.end() ? itr->second : 0;
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

    // Display season (keys MythicPlusSeasonTrackedMap/TrackedAffix/KeyFloor): config override, else the newest
    // season present in the tracked-map table -- the authoritative source of the live dungeon pool.
    _displaySeasonId = uint32(sConfigMgr->GetIntDefault("ChallengeMode.DisplaySeasonId", 0));
    if (!_displaySeasonId)
        for (MythicPlusSeasonTrackedMapEntry const* trackedMap : sMythicPlusSeasonTrackedMapStore)
            _displaySeasonId = std::max(_displaySeasonId, trackedMap->DisplaySeasonID);

    // Season dungeon pool: the tracked maps of the active display season (authentic). Fallback to the
    // expansion filter when the DB2 has no rows for it (older data or stripped client).
    _seasonMaps.clear();
    for (MythicPlusSeasonTrackedMapEntry const* trackedMap : sMythicPlusSeasonTrackedMapStore)
        if (trackedMap->DisplaySeasonID == _displaySeasonId && GetMapChallengeMode(uint32(trackedMap->MapChallengeModeID)))
            _seasonMaps.push_back(uint32(trackedMap->MapChallengeModeID));

    if (_seasonMaps.empty())
        if (MythicPlusSeasonEntry const* season = GetActiveSeason())
            for (auto const& [challengeModeId, entry] : _mapChallengeModes)
                if (int32(entry->ExpansionLevel) == season->ExpansionLevel)
                    _seasonMaps.push_back(challengeModeId);

    TC_LOG_INFO("server.loading", "ChallengeModeMgr: active Mythic+ season {} (display season {}, {} dungeons in pool).",
        _activeSeasonId, _displaySeasonId, _seasonMaps.size());
}

void ChallengeModeMgr::LoadAffixRotation()
{
    // Explicit operator override (fixed weekly set): when ChallengeMode.AffixSchedule is set it is used verbatim
    // (paired with ChallengeMode.AffixLevelBands) instead of the built-in Midnight S1 rotation below.
    //   ChallengeMode.AffixSchedule    = comma list of KeystoneAffix IDs applied this week (lowest band first)
    //   ChallengeMode.AffixLevelBands  = comma list of keystone levels at which each successive affix turns on
    _affixSchedule = ParseUInt32List(sConfigMgr->GetStringDefault("ChallengeMode.AffixSchedule", ""));
    _affixLevelBands = ParseUInt32List(sConfigMgr->GetStringDefault("ChallengeMode.AffixLevelBands", ""));

    if (!_affixSchedule.empty())
        TC_LOG_INFO("server.loading", "ChallengeModeMgr: fixed affix schedule configured ({} affixes); the built-in "
            "weekly rotation is bypassed.", _affixSchedule.size());
    else
        TC_LOG_INFO("server.loading", "ChallengeModeMgr: using the built-in Midnight S1 weekly affix rotation "
            "(week index {}).", GetCurrentWeekIndex());
}

uint32 ChallengeModeMgr::GetCurrentWeekIndex() const
{
    // Anchor the rotation on the current week's reset boundary so every server sees a stable index for the whole
    // week and the index advances exactly at the weekly reset. The offset lets operators phase-align with live.
    int64 const weekStart = int64(sWorld->GetNextWeeklyQuestsResetTime()) - int64(WEEK);
    int64 const index = weekStart / int64(WEEK) + sConfigMgr->GetIntDefault("ChallengeMode.Affix.WeekOffset", 0);
    return uint32(std::max<int64>(index, 0));
}

std::vector<uint32> ChallengeModeMgr::GetActiveAffixes(uint32 keystoneLevel) const
{
    // Operator-fixed schedule: single affix per band, band N turns on at AffixLevelBands[N].
    if (!_affixSchedule.empty())
    {
        std::vector<uint32> affixes;
        for (std::size_t i = 0; i < _affixSchedule.size() && affixes.size() < 4; ++i)
        {
            uint32 requiredLevel = i < _affixLevelBands.size() ? _affixLevelBands[i] : 0;
            if (keystoneLevel >= requiredLevel)
                affixes.push_back(_affixSchedule[i]);
        }
        return affixes;
    }

    // Built-in Midnight S1 rotation. Level bands (config-tunable; retail 12.0.x defaults):
    //   +2..+5  Lindormi's Guidance (constant)
    //   +5..+11 Xal'atath's Bargain (weekly rotation Ascendant/Voidbound/Devour/Pulsar)
    //   +7      Tyrannical or Fortified (weekly alternation)
    //   +10     both Tyrannical and Fortified
    //   +12+    Xal'atath's Guile replaces the Bargain
    uint32 const week = GetCurrentWeekIndex();
    std::vector<uint32> affixes;

    uint32 const guidanceId = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Guidance.Id", int32(ChallengeModeAffix::LindormisGuidance)));
    uint32 const guidanceMax = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Guidance.MaxLevel", 5));
    if (guidanceId && keystoneLevel <= guidanceMax)
        affixes.push_back(guidanceId);

    uint32 const guileId = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Guile.Id", int32(ChallengeModeAffix::XalatathsGuile)));
    uint32 const guileStart = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Guile.StartLevel", 12));
    uint32 const bargainStart = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Bargain.StartLevel", 5));
    if (guileId && keystoneLevel >= guileStart)
        affixes.push_back(guileId);
    else if (keystoneLevel >= bargainStart)
    {
        static std::string const defaultRotation = "148,158,160,162"; // Ascendant, Voidbound, Devour, Pulsar
        std::vector<uint32> const rotation = ParseUInt32List(sConfigMgr->GetStringDefault("ChallengeMode.Affix.Bargain.Rotation", defaultRotation));
        if (!rotation.empty())
            affixes.push_back(rotation[week % rotation.size()]);
    }

    uint32 const tyrFortStart = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.TyrannicalFortified.StartLevel", 7));
    uint32 const bothLevel = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.TyrannicalFortified.BothLevel", 10));
    if (keystoneLevel >= tyrFortStart)
    {
        bool const tyrannicalFirst = (week % 2) == 0;
        affixes.push_back(tyrannicalFirst ? ChallengeModeAffix::Tyrannical : ChallengeModeAffix::Fortified);
        if (keystoneLevel >= bothLevel)
            affixes.push_back(tyrannicalFirst ? ChallengeModeAffix::Fortified : ChallengeModeAffix::Tyrannical);
    }

    if (affixes.size() > 4)
        affixes.resize(4);
    return affixes;
}

std::vector<uint32> ChallengeModeMgr::GetWeeklyAffixes() const
{
    // The full advertised weekly set = every band's affix in ascending band order, deduplicated. Derive it from
    // the per-level sets so the fixed-schedule override and the built-in rotation share one code path.
    std::vector<uint32> weekly;
    for (uint32 level : { 2u, 5u, 7u, 10u, 12u, 20u })
        for (uint32 affixId : GetActiveAffixes(level))
            if (std::find(weekly.begin(), weekly.end(), affixId) == weekly.end())
                weekly.push_back(affixId);
    return weekly;
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

float ChallengeModeMgr::CalculateRunScore(uint32 keystoneLevel, uint32 effectiveTimeMs, uint32 timeLimitMs) const
{
    if (!keystoneLevel)
        return 0.0f;

    // Retail Midnight S1 rating formula (community-derived, config-tunable): a timed +2 is worth Base points,
    // +PerLevel per keystone level above 2, +PerAffixBreakpoint at every affix-band breakpoint the level has
    // crossed (+5/+7/+10/+12, max 4). Finishing under par adds up to MaxTimeBonus (linear, capped at 40% under);
    // finishing over par decays the whole score linearly to 0 at 40% over.
    float const base = sConfigMgr->GetFloatDefault("ChallengeMode.Score.Base", 155.0f);
    float const perLevel = sConfigMgr->GetFloatDefault("ChallengeMode.Score.PerLevel", 15.0f);
    float const perBreakpoint = sConfigMgr->GetFloatDefault("ChallengeMode.Score.PerAffixBreakpoint", 15.0f);
    float const maxTimeBonus = sConfigMgr->GetFloatDefault("ChallengeMode.Score.MaxTimeBonus", 15.0f);

    float score = base + perLevel * float(keystoneLevel > 2 ? keystoneLevel - 2 : 0);

    std::vector<uint32> const breakpoints = ParseUInt32List(sConfigMgr->GetStringDefault("ChallengeMode.Score.AffixBreakpoints", "5,7,10,12"));
    for (uint32 breakpoint : breakpoints)
        if (keystoneLevel >= breakpoint)
            score += perBreakpoint;

    if (timeLimitMs)
    {
        float const parRatio = float(effectiveTimeMs) / float(timeLimitMs); // < 1.0 = under par
        if (parRatio <= 1.0f)
            score += std::min((1.0f - parRatio) / 0.4f, 1.0f) * maxTimeBonus;
        else
            score *= std::max(0.0f, 1.0f - (parRatio - 1.0f) / 0.4f);
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
        return sConfigMgr->GetFloatDefault("ChallengeMode.Affix.Fortified.Damage", 1.20f); // Midnight guides: up to +20% (TWW-era was 30%)
    return 1.0f;
}

uint32 ChallengeModeMgr::GetAffixSpellId(uint32 affixId) const
{
    switch (affixId)
    {
        // Legacy roster (pre-Midnight; usable via the AffixSchedule override)
        case ChallengeModeAffix::Bolstering: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Bolstering.SpellId", 209859)); // +20% max health & damage to nearby allies
        case ChallengeModeAffix::Bursting:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Bursting.SpellId", 240443));   // stacking damage-over-time on all players
        case ChallengeModeAffix::Sanguine:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Sanguine.SpellId", 226489));   // lingering ichor pool (heals allies / damages players)
        case ChallengeModeAffix::Raging:     return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Raging.SpellId", 228318));     // enrage at low health
        case ChallengeModeAffix::Grievous:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Grievous.SpellId", 240559));   // stacking bleed on wounded players
        // Midnight roster
        case ChallengeModeAffix::XalatathsBargainDevour: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Devour.SpellId", 440313));   // Devouring Rift debuff
        case ChallengeModeAffix::XalatathsBargainPulsar: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Pulsar.SpellId", 1216858));  // orbiting Void Pulsar
        case ChallengeModeAffix::LindormisGuidance:      return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Guidance.SpellId", 1284818)); // Temporal Sands highlight
        default: return 0;
    }
}

uint32 ChallengeModeMgr::GetAffixCreatureId(uint32 affixId) const
{
    switch (affixId)
    {
        // Legacy roster
        case ChallengeModeAffix::Spiteful:    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Spiteful.CreatureId", 174773));  // Spiteful Shade (fixate + self-decay AI)
        case ChallengeModeAffix::Incorporeal: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Incorporeal.CreatureId", 204560)); // Incorporeal Being (verify per build)
        case ChallengeModeAffix::Afflicted:   return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Afflicted.CreatureId", 0));        // needs verified entry per build
        // Midnight roster: Orbs of Ascendance / Void Emissary entries + AI are world content; 0 = disabled.
        case ChallengeModeAffix::XalatathsBargainAscendant: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Ascendant.CreatureId", 0));
        case ChallengeModeAffix::XalatathsBargainVoidbound: return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Affix.Voidbound.CreatureId", 0));
        default: return 0;
    }
}

namespace
{
    // The keystone level at which a crest bracket begins (drives the +2/level amount growth within it).
    uint32 CrestBracketStart(uint32 keystoneLevel)
    {
        if (keystoneLevel <= uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Champion.MaxLevel", 3)))
            return 2;
        if (keystoneLevel <= uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Hero.MaxLevel", 8)))
            return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Champion.MaxLevel", 3)) + 1;
        return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Hero.MaxLevel", 8)) + 1;
    }
}

uint32 ChallengeModeMgr::GetCrestCurrencyForLevel(uint32 keystoneLevel) const
{
    // Midnight S1 brackets: Champion +2-3, Hero +4-8, Myth +9+. Currency ids SNIFF-VERIFIED on 68275
    // (m+ run12.0.7.pkt: a +2 run granted 12x 3343; SETUP_CURRENCY carries 3341/3342/3343/3345/3347 -
    // the web-reported 3346/3348 do not exist on the wire).
    if (keystoneLevel <= uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Champion.MaxLevel", 3)))
        return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Champion.CurrencyId", 3343));
    if (keystoneLevel <= uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Hero.MaxLevel", 8)))
        return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Hero.CurrencyId", 3345));
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Myth.CurrencyId", 3347));
}

uint32 ChallengeModeMgr::GetCrestAmountForLevel(uint32 keystoneLevel, bool timed) const
{
    if (keystoneLevel < 2)
        return 0;

    // Retail ladder: +2=12 Champion, +3=14; +4=10 Hero ... +8=18; +9=10 Myth ... +12+=16 (cap).
    // The Champion bracket opens 2 higher than Hero/Myth (12 vs 10), hence the per-bracket base.
    uint32 const bracketStart = CrestBracketStart(keystoneLevel);
    uint32 const baseAmount = bracketStart == 2
        ? uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.Champion.BaseAmount", 12))
        : uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.BaseAmount", 10));
    uint32 const perLevel = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.AmountPerLevel", 2));
    uint32 const capLevel = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.AmountCapLevel", 12));

    uint32 const effectiveLevel = std::min(keystoneLevel, std::max(capLevel, bracketStart));
    uint32 amount = baseAmount + perLevel * (effectiveLevel - bracketStart);

    // Untimed runs still reward crests, reduced by a flat amount (retail TWW rule was -4; 12.x unpublished).
    if (!timed)
    {
        uint32 const reduction = uint32(sConfigMgr->GetIntDefault("ChallengeMode.Crest.UntimedReduction", 4));
        amount = amount > reduction ? amount - reduction : 0;
    }

    return amount;
}

uint32 ChallengeModeMgr::GetGearRewardLootId() const
{
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Reward.LootId", 0));
}

uint32 ChallengeModeMgr::GetVaultRewardLootId() const
{
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Vault.LootId", 0));
}

std::vector<ChallengeModeMgr::VaultThreshold> ChallengeModeMgr::GetMythicPlusVaultThresholds() const
{
    // WeeklyRewardChestThresholdType::MythicPlus
    constexpr uint8 TYPE_MYTHIC_PLUS = 1;

    // Keep, per slot index, the highest-ID row (the live season's) — the DB2 retains every past season's rows.
    std::unordered_map<uint32 /*index*/, WeeklyRewardChestThresholdEntry const*> liveByIndex;
    for (WeeklyRewardChestThresholdEntry const* entry : sWeeklyRewardChestThresholdStore)
    {
        if (entry->Type != TYPE_MYTHIC_PLUS)
            continue;

        auto& live = liveByIndex[uint32(entry->Index)];
        if (!live || entry->ID > live->ID)
            live = entry;
    }

    std::vector<VaultThreshold> thresholds;
    thresholds.reserve(liveByIndex.size());
    for (auto const& [index, entry] : liveByIndex)
        thresholds.push_back({ entry->ID, index, uint32(std::max(entry->Threshold, 0)) });

    std::sort(thresholds.begin(), thresholds.end(), [](VaultThreshold const& a, VaultThreshold const& b)
    {
        return a.Index < b.Index;
    });
    return thresholds;
}

uint32 ChallengeModeMgr::GetMythicPlusVaultSlotForThreshold(uint32 thresholdId) const
{
    for (VaultThreshold const& threshold : GetMythicPlusVaultThresholds())
        if (threshold.ThresholdID == thresholdId)
            return threshold.Index;

    return VAULT_SLOT_NONE;
}

uint32 ChallengeModeMgr::GetMythicPlusVaultSlotRewardLevel(Player* player, uint32 slotIndex) const
{
    MythicPlusData* data = player->GetMythicPlusData();
    if (!data)
        return 0;

    // The slot's level is the Nth-best run of the week (N = the slot's run threshold); 0 until it unlocks.
    uint32 level = data->GetVaultSlotLevel(slotIndex);
    if (!level)
        return 0;

    // Reward scaling stops at the active season's highest MythicPlusSeasonRewardLevels row; higher keys are
    // score-only. 0 = the DB2 has no rows for the season -> uncapped rather than invented.
    if (uint32 cap = GetVaultRewardLevelCap())
        level = std::min(level, cap);

    return level;
}

std::vector<ChallengeModeMgr::VaultRewardOption> ChallengeModeMgr::BuildMythicPlusVaultOptions(Player* player) const
{
    std::vector<VaultRewardOption> options;

    uint32 const lootId = GetVaultRewardLootId();
    bool const poolReady = lootId && LootTemplates_Reference.HaveLootFor(lootId);

    for (VaultThreshold const& threshold : GetMythicPlusVaultThresholds())
    {
        uint32 const rewardLevel = GetMythicPlusVaultSlotRewardLevel(player, threshold.Index);
        if (!rewardLevel)
            continue;   // slot still locked

        VaultRewardOption& option = options.emplace_back();
        option.ThresholdID = threshold.ThresholdID;
        option.SlotIndex = threshold.Index;
        option.RewardLevel = rewardLevel;

        if (poolReady)
        {
            if (uint32 itemId = RollVaultRewardItem(player, lootId))
            {
                option.ItemID = itemId;
                option.BonusListIDs = VaultRewardBonuses(itemId, int32(rewardLevel));
            }
        }
    }

    return options;
}

ChallengeModeMgr::VaultClaimResult ChallengeModeMgr::ClaimMythicPlusVaultReward(Player* player, uint32 slotIndex) const
{
    MythicPlusData* data = player->GetMythicPlusData();
    if (!data || data->IsVaultClaimedThisWeek())
        return VaultClaimResult::NotClaimable;

    uint32 const rewardLevel = GetMythicPlusVaultSlotRewardLevel(player, slotIndex);
    if (!rewardLevel)
        return VaultClaimResult::NotClaimable;

    uint32 const lootId = GetVaultRewardLootId();
    if (!lootId || !LootTemplates_Reference.HaveLootFor(lootId))
    {
        TC_LOG_ERROR("challengemode", "ChallengeModeMgr: player {} claimed Mythic+ vault slot {} but no vault reward "
            "pool is configured (ChallengeMode.Vault.LootId); the week is left unclaimed.",
            player->GetGUID().ToString(), slotIndex);
        return VaultClaimResult::RewardPoolUnavailable;
    }

    uint32 const itemId = RollVaultRewardItem(player, lootId);
    if (!itemId)
    {
        TC_LOG_ERROR("challengemode", "ChallengeModeMgr: Mythic+ vault reward pool {} rolled nothing for player {}; "
            "the week is left unclaimed.", lootId, player->GetGUID().ToString());
        return VaultClaimResult::RewardPoolUnavailable;
    }

    GrantVaultItem(player, itemId, VaultRewardBonuses(itemId, int32(rewardLevel)));

    // Only a claim that actually handed something over consumes the week.
    data->SetVaultClaimed();

    TC_LOG_INFO("challengemode", "ChallengeModeMgr: player {} claimed Mythic+ vault slot {} -> item {} at key level {}.",
        player->GetGUID().ToString(), slotIndex, itemId, rewardLevel);
    return VaultClaimResult::Success;
}

MythicPlusSeasonEntry const* ChallengeModeMgr::GetActiveSeason() const
{
    return sMythicPlusSeasonStore.LookupEntry(_activeSeasonId);
}

uint32 ChallengeModeMgr::GetKeystoneFloor(Player const* player) const
{
    // Resilient Keystone: the highest floor row of the active display season whose PlayerCondition the player
    // meets (retail: "time all season dungeons at +N"). Rows without a condition apply unconditionally.
    uint32 floor = 0;
    for (MythicPlusSeasonKeyFloorEntry const* keyFloor : sMythicPlusSeasonKeyFloorStore)
    {
        if (keyFloor->DisplaySeasonID != _displaySeasonId || keyFloor->KeyFloor <= int32(floor))
            continue;

        if (keyFloor->PlayerConditionID)
            if (!ConditionMgr::IsPlayerMeetingCondition(player, uint32(keyFloor->PlayerConditionID)))
                continue;

        floor = uint32(keyFloor->KeyFloor);
    }
    return floor;
}

uint32 ChallengeModeMgr::GetVaultRewardLevelCap() const
{
    // Modern seasons cap vault reward scaling (retail 12.x: +10); the cap is the highest DifficultyLevel with
    // a reward row for the active season. 0 = no data -> uncapped.
    uint32 cap = 0;
    for (MythicPlusSeasonRewardLevelsEntry const* rewardLevel : sMythicPlusSeasonRewardLevelsStore)
        if (rewardLevel->MythicPlusSeasonID == _activeSeasonId)
            cap = std::max(cap, uint32(std::max(rewardLevel->DifficultyLevel, 0)));
    return cap;
}

int32 ChallengeModeMgr::GetVaultActivityTierId() const
{
    for (MythicPlusSeasonRewardLevelsEntry const* rewardLevel : sMythicPlusSeasonRewardLevelsStore)
        if (rewardLevel->MythicPlusSeasonID == _activeSeasonId && rewardLevel->ActivityTierID)
            return rewardLevel->ActivityTierID;
    return 0;
}

uint32 ChallengeModeMgr::GetKeystoneItemId() const
{
    return uint32(sConfigMgr->GetIntDefault("ChallengeMode.Keystone.ItemId", 180653)); // 12.x "Mythic Keystone"
}

uint32 ChallengeModeMgr::GetKeystoneMinLevel() const
{
    return uint32(std::max(sConfigMgr->GetIntDefault("ChallengeMode.Keystone.MinLevel", 2), 2));
}

Item* ChallengeModeMgr::GetKeystone(Player* player) const
{
    uint32 const itemId = GetKeystoneItemId();
    return itemId ? player->GetItemByEntry(itemId) : nullptr;
}

void ChallengeModeMgr::StampKeystone(Item* keystone, uint32 challengeModeId, uint32 keystoneLevel) const
{
    keystone->SetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID, challengeModeId);
    keystone->SetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL, keystoneLevel);

    // Only the affixes active at this key's level are attached (a +2 key carries a single affix); the client
    // tooltip renders one line per non-zero modifier.
    std::vector<uint32> const affixes = GetActiveAffixes(keystoneLevel);
    for (uint32 i = 0; i < 4; ++i)
        keystone->SetModifier(ItemModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_1 + i), i < affixes.size() ? affixes[i] : 0u);

    if (Player* owner = keystone->GetOwner())
        keystone->SetState(ITEM_CHANGED, owner);
}

Item* ChallengeModeMgr::CreateOrUpdateKeystone(Player* player, uint32 challengeModeId, uint32 keystoneLevel) const
{
    if (!challengeModeId || !GetMapChallengeMode(challengeModeId))
        return nullptr;

    Item* keystone = GetKeystone(player);
    if (!keystone)
    {
        uint32 const itemId = GetKeystoneItemId();
        if (!itemId)
            return nullptr;

        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1) != EQUIP_ERR_OK)
        {
            TC_LOG_DEBUG("challengemode", "ChallengeModeMgr: no bag space to grant keystone to player {}.",
                player->GetGUID().ToString());
            return nullptr;
        }

        keystone = player->StoreNewItem(dest, itemId, true, 0);
        if (!keystone)
            return nullptr;
    }

    StampKeystone(keystone, challengeModeId, std::max(keystoneLevel, GetKeystoneMinLevel()));
    return keystone;
}

uint32 ChallengeModeMgr::RollSeasonDungeon(uint32 excludeChallengeModeId /*= 0*/) const
{
    if (_seasonMaps.empty())
        return 0;

    // Reroll away from the excluded dungeon when the pool offers an alternative (retail rerolls to a different map).
    if (_seasonMaps.size() > 1 && excludeChallengeModeId)
    {
        uint32 rolled;
        do
        {
            rolled = _seasonMaps[urand(0, uint32(_seasonMaps.size() - 1))];
        } while (rolled == excludeChallengeModeId);
        return rolled;
    }

    return _seasonMaps[urand(0, uint32(_seasonMaps.size() - 1))];
}

void ChallengeModeMgr::OnMythicDungeonCompleted(Player* player) const
{
    if (!sConfigMgr->GetBoolDefault("ChallengeMode.Keystone.AwardOnMythicClear", true))
        return;

    // Only season dungeons hand out keystones, and only to players who do not already hold one (unique item).
    if (!GetChallengeModeIdForMap(player->GetMapId()))
        return;

    if (GetKeystone(player))
        return;

    if (uint32 dungeon = RollSeasonDungeon())
        if (CreateOrUpdateKeystone(player, dungeon, GetKeystoneMinLevel()))
            TC_LOG_INFO("challengemode", "ChallengeModeMgr: awarded first keystone to player {} after Mythic clear of map {}.",
                player->GetGUID().ToString(), player->GetMapId());
}

void ChallengeModeMgr::UpdateKeystoneForNewWeek(Player* player, bool createIfMissing) const
{
    MythicPlusData* data = player->GetMythicPlusData();
    if (!data)
        return;

    Item* keystone = GetKeystone(player);
    if (!keystone && !createIfMissing)
        return;

    if (!data->NeedsKeystoneAdjustment())
    {
        // Already adjusted this week; the vault-open grant still applies when the key was destroyed since.
        if (!keystone && createIfMissing)
            if (uint32 dungeon = RollSeasonDungeon())
                CreateOrUpdateKeystone(player, dungeon, GetKeystoneMinLevel());
        return;
    }

    // The weekly level never drops below the player's Resilient Keystone floor (MythicPlusSeasonKeyFloor).
    uint32 const minLevel = std::max(GetKeystoneMinLevel(), GetKeystoneFloor(player));
    uint32 const currentLevel = keystone ? keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL) : minLevel;
    uint32 const newLevel = data->ComputeNewWeekKeystoneLevel(currentLevel, minLevel);

    uint32 const currentDungeon = keystone ? keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID) : 0;
    uint32 const dungeon = RollSeasonDungeon(currentDungeon);
    if (!dungeon)
        return;

    if (CreateOrUpdateKeystone(player, dungeon, newLevel))
    {
        data->SetKeystoneAdjusted();
        TC_LOG_DEBUG("challengemode", "ChallengeModeMgr: weekly keystone adjustment for player {} -> dungeon {} level {}.",
            player->GetGUID().ToString(), dungeon, newLevel);
    }
}

void ChallengeModeMgr::OnWeeklyReset() const
{
    uint32 processed = 0;

    for (auto const& [accountId, session] : sWorld->GetAllSessions())
    {
        if (!session)
            continue;

        Player* player = session->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        // Drop the finished week's vault progress first (this is what captures + persists the summary the
        // keystone rule below reads), then re-issue the keystone for the new week.
        if (MythicPlusData* data = player->GetMythicPlusData())
            data->ResetWeeklyRuns();

        // No fresh keystone here: retail only hands one out from the Great Vault or a Mythic 0 clear, so a
        // player who ended the week without a key still has to go collect one.
        UpdateKeystoneForNewWeek(player, false /*createIfMissing*/);
        ++processed;
    }

    TC_LOG_INFO("challengemode", "ChallengeModeMgr: weekly reset rolled over {} online characters (week index {}); "
        "characters offline at the reset are rolled over on their next login.", processed, GetCurrentWeekIndex());
}

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

#include "RitualSiteMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "SharedDefines.h"

RitualSiteMgr::RitualSiteMgr() = default;
RitualSiteMgr::~RitualSiteMgr() = default;

/*static*/ RitualSiteMgr* RitualSiteMgr::instance()
{
    static RitualSiteMgr instance;
    return &instance;
}

void RitualSiteMgr::LoadFromDB()
{
    _sites.clear();
    _enabled = false;

    // Realm-safe: the shipped table may not be applied on the shared realm.
    // A missing table yields a null result (logged, non-fatal) -> silent no-op.
    QueryResult result = WorldDatabase.Query("SELECT AreaPoiId, ZoneId, WorldStateId FROM ritual_site_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Ritual Sites: ritual_site_template absent or empty; Ritual Sites idle.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        RitualSiteTemplate tmpl;
        tmpl.AreaPoiId    = fields[0].GetUInt32();
        tmpl.ZoneId       = fields[1].GetUInt32();
        tmpl.WorldStateId = fields[2].GetUInt32();
        _sites.emplace(tmpl.AreaPoiId, tmpl);
        ++count;
    } while (result->NextRow());

    _enabled = count != 0;
    TC_LOG_INFO("server.loading", ">> Ritual Sites: loaded {} site template(s).", count);
}

RitualSiteTemplate const* RitualSiteMgr::GetSite(uint32 areaPoiId) const
{
    auto it = _sites.find(areaPoiId);
    return it != _sites.end() ? &it->second : nullptr;
}

void RitualSiteMgr::CompleteRitualSite(Player* player, uint32 /*areaPoiId*/)
{
    // Gated on IsEnabled() so this is a hard no-op on the shared realm.
    if (!_enabled || !player)
        return;

    // --- Renown currency 3428 "Renown - Ritual Sites" ---
    // Plain CurrencyTypes track; ModifyCurrency clamps to the DB2 cap. Amount is
    // PLACEHOLDER (CAPTURE-BLOCKED — no completion reward packet captured).
    player->ModifyCurrency(RitualSites::CURRENCY_RENOWN_RITUAL_SITES,
        int32(RitualSites::PLACEHOLDER_RENOWN_PER_SITE), CurrencyGainSource::Script);

    // --- Faction-2792 reputation ---
    // Reputation is the retail progression axis for a renown faction; feeding it
    // through ReputationMgr is the correct, non-reinvented grant. Amount PLACEHOLDER.
    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(RitualSites::FACTION_RITUAL_SITES))
        player->GetReputationMgr().ModifyReputation(factionEntry, RitualSites::PLACEHOLDER_REP_PER_SITE);

    // --- "Ritual Breaker" title at renown cap ---
    TryAwardTitle(player);
}

void RitualSiteMgr::TryAwardTitle(Player* player)
{
    // The title is earned once the player's Ritual Sites renown reaches the cap.
    // The cap LEVEL is CAPTURE-BLOCKED (placeholder); the mechanism is stock.
    CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(RitualSites::TITLE_RITUAL_BREAKER);
    if (!title || player->HasTitle(title))
        return;

    if (player->GetCurrencyQuantity(RitualSites::CURRENCY_RENOWN_RITUAL_SITES) >= RitualSites::PLACEHOLDER_TITLE_RENOWN_LEVEL)
        player->SetTitle(title);
}

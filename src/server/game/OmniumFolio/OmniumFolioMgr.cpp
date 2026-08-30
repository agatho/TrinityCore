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

#include "OmniumFolioMgr.h"
#include "DatabaseEnv.h"
#include "DBCEnums.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Player.h"

// The five weekly-unlock achievements that each source +1 "Mote of Omnial Inquiry"
// (TraitCurrency 4230) via DB2 TraitCurrencySource. Kept here for reference; the
// actual mote accrual is done live by TraitMgr from TraitCurrencySource, so this
// manager never touches currency directly.
namespace
{
    [[maybe_unused]] constexpr uint32 MoteAchievements[5] = { 62606, 62607, 62608, 62609, 62610 };
}

OmniumFolioMgr* OmniumFolioMgr::instance()
{
    static OmniumFolioMgr instance;
    return &instance;
}

void OmniumFolioMgr::LoadFromDB()
{
    _enabled    = false;
    _seasonId   = 0;
    _resetTimer = 0;

    // The folio's static definition data lives entirely in client DB2s
    // (TraitTree 1186 / TraitSystem 48 / TraitCurrency 4230) already loaded by
    // TraitMgr::Load(). This optional world table only carries the per-fork
    // seasonal schedule. It may be absent on the shared realm -> tolerate as
    // a no-op so the branch is realm-safe (mirrors the zone-events skeleton).
    QueryResult result = WorldDatabase.Query("SELECT SeasonId, Enabled FROM omnium_folio_season ORDER BY SeasonId DESC LIMIT 1");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Omnium Folio: no seasonal schedule row; system idle (definitions ride TraitMgr / DB2).");
        return;
    }

    Field* fields = result->Fetch();
    _seasonId = fields[0].GetUInt32();
    _enabled  = fields[1].GetBool();
    TC_LOG_INFO("server.loading", ">> Omnium Folio: season {} loaded (enabled={}).", _seasonId, uint32(_enabled));
}

void OmniumFolioMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    _resetTimer += diff;
    // TODO(season-reset): on a weekly/season boundary, iterate online players and
    // call ResetForNewSeason(). Wired once the seasonal schedule table is seeded.
}

void OmniumFolioMgr::OnPlayerLogin(Player* player)
{
    if (!player || !_enabled)
        return;

    EnsureFolioForPlayer(player);
}

bool OmniumFolioMgr::HasFolioConfig(Player const* player)
{
    // Any generic TraitConfig whose TraitSystemID is the folio system (48) is the
    // folio config. Mirrors the duplicate guard in Spell::EffectCreateTraitTreeConfig.
    return player->m_activePlayerData->TraitConfigs.FindIf([](UF::TraitConfig const& config)
    {
        return static_cast<TraitConfigType>(*config.Type) == TraitConfigType::Generic
            && config.TraitSystemID == TRAIT_SYSTEM_ID;
    }).second != nullptr;
}

uint32 OmniumFolioMgr::GetLastSeasonMinted(ObjectGuid guid)
{
    // Optional per-character bookkeeping table. Only queried for eligible players on
    // an enabled (seeded) DB, so the shared realm never hits it. Absent row/table
    // => 0 (never minted).
    QueryResult result = CharacterDatabase.PQuery(
        "SELECT LastSeasonMinted FROM character_omnium_folio WHERE guid = {}", guid.GetCounter());
    if (!result)
        return 0;

    return (*result)[0].GetUInt32();
}

void OmniumFolioMgr::SaveSeasonBookkeeping(ObjectGuid guid, uint32 seasonId)
{
    CharacterDatabase.PExecute(
        "INSERT INTO character_omnium_folio (guid, LastSeasonMinted, UnlockedAt) "
        "VALUES ({}, {}, UNIX_TIMESTAMP()) "
        "ON DUPLICATE KEY UPDATE LastSeasonMinted = VALUES(LastSeasonMinted)",
        guid.GetCounter(), seasonId);
}

void OmniumFolioMgr::EnsureFolioForPlayer(Player* player)
{
    // POWER-GRANT SEAM.
    //
    // The folio's power is delivered by TrinityCore's generic-trait machinery:
    //   * Spell 1279717 has SPELL_EFFECT_CREATE_TRAIT_TREE_CONFIG (MiscValue = tree
    //     1186) -> Spell::EffectCreateTraitTreeConfig -> Player::CreateTraitConfig,
    //     which mints the generic TraitConfig for TraitSystem 48.
    //   * Node runes apply through Player::ApplyTraitConfig -> LearnSpell on each
    //     node's TraitDefinition.SpellID (already implemented).
    //   * Motes (TraitCurrency 4230) are computed live from TraitCurrencySource
    //     (achievements 62606..62610 + level-1 base) inside TraitMgr -- no store.
    //
    // This coordinator only decides WHETHER to mint: season active + player eligible
    // + no existing folio config. Everything it calls already exists in core.
    if (!player || !_enabled)
        return;

    // Eligibility gate. The unlock questline ("The Magisters' Call" -> "The Omnium
    // Reawakens") that AWARDS achievement 62606 is CAPTURE-BLOCKED world content
    // seeded later; keying on the confirmed achievement id keeps this code stable.
    if (!player->HasAchieved(UNLOCK_ACHIEVEMENT_ID))
        return;

    bool const hasConfig = HasFolioConfig(player);
    uint32 const lastSeasonMinted = GetLastSeasonMinted(player->GetGUID());

    // Already provisioned for the current season -> nothing to do (idempotent relog).
    if (hasConfig && lastSeasonMinted == _seasonId)
        return;

    // Config exists but was minted for an earlier season -> season rollover.
    if (hasConfig && lastSeasonMinted != _seasonId)
    {
        ResetForNewSeason(player);
        return;
    }

    // No folio config yet -> mint it via stock trait code. The effect itself also
    // guards against duplicating a system-48 config, so this stays idempotent even
    // if bookkeeping and update-fields ever disagree.
    player->CastSpell(player, UNLOCK_SPELL_ID, true);

    SaveSeasonBookkeeping(player->GetGUID(), _seasonId);
}

void OmniumFolioMgr::ResetForNewSeason(Player* player)
{
    if (!player)
        return;

    // Cross-season reset semantics for the folio (wipe existing rune selections vs.
    // carry them forward vs. mint a fresh parallel config) are NOT source-confirmed
    // at build 12.0.7.68887. TODO(CAPTURE-BLOCKED): confirm via a two-season sniff
    // before enabling any DESTRUCTIVE wipe (DeleteTraitConfig).
    //
    // Conservative choice implemented here: never destroy the player's existing
    // selections. We only guarantee a config exists (mint if the character somehow
    // has none) and advance the season bookkeeping so the new season's achievement
    // motes become spendable on the existing ledger. This is realm-safe and
    // non-lossy; a confirmed wipe can be layered on later behind the same seam.
    if (!HasFolioConfig(player))
        player->CastSpell(player, UNLOCK_SPELL_ID, true);

    SaveSeasonBookkeeping(player->GetGUID(), _seasonId);
}

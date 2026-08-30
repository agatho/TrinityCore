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

#include "LoaBlessingMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"

LoaBlessingMgr* LoaBlessingMgr::instance()
{
    static LoaBlessingMgr instance;
    return &instance;
}

void LoaBlessingMgr::LoadFromDB()
{
    _options.clear();

    // loa_blessing_option ships on feature/loa-blessings
    // (sql/updates/world/master/2026_08_13_00_world_loa_blessings.sql). Absent
    // table is tolerated so an un-seeded realm boots cleanly.
    QueryResult result = WorldDatabase.Query(
        "SELECT MajorLoa, MinorLoa, SpellId, UnlockConditionId, Name FROM loa_blessing_option ORDER BY MajorLoa, MinorLoa");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 loa blessing options (table loa_blessing_option missing or empty).");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        LoaBlessingOption opt;
        opt.MajorLoa          = fields[0].GetUInt8();
        opt.MinorLoa          = fields[1].GetUInt8();
        opt.SpellId           = fields[2].GetUInt32();
        opt.UnlockConditionId = fields[3].GetUInt32();
        opt.Name              = fields[4].GetString();

        if (!opt.SpellId)
            continue;

        _options.push_back(std::move(opt));
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} loa blessing options.", _options.size());
}

LoaBlessingOption const* LoaBlessingMgr::GetOption(uint8 majorLoa, uint8 minorLoa) const
{
    for (LoaBlessingOption const& opt : _options)
        if (opt.MajorLoa == majorLoa && opt.MinorLoa == minorLoa)
            return &opt;
    return nullptr;
}

LoaBlessingOption const* LoaBlessingMgr::GetOptionByIndex(size_t index) const
{
    if (index >= _options.size())
        return nullptr;
    return &_options[index];
}

bool LoaBlessingMgr::IsInZulAman(Player const* player)
{
    return player && player->GetZoneId() == ZONE_ID_ZULAMAN;
}

void LoaBlessingMgr::RemoveHeldBlessing(Player* player) const
{
    if (!player)
        return;

    // Only one loa blessing may be held at a time — strip every known option
    // aura before applying a new one.
    for (LoaBlessingOption const& opt : _options)
        player->RemoveAura(opt.SpellId);
}

bool LoaBlessingMgr::ApplyBlessing(Player* player, uint8 majorLoa, uint8 minorLoa) const
{
    if (!player)
        return false;

    // Blessings only take hold in Zul'Aman.
    if (!IsInZulAman(player))
        return false;

    LoaBlessingOption const* opt = GetOption(majorLoa, minorLoa);
    if (!opt)
        return false;

    RemoveHeldBlessing(player);
    player->CastSpell(player, opt->SpellId, true);
    return true;
}

void LoaBlessingMgr::OnAbundanceHarvest(Player* player) const
{
    // TODO(CAPTURE-BLOCKED): the Loa-of-Abundance harvest event (AreaPOI
    // 8415-8418 / 8525-8528, PoiData 92418) should reinforce loa worship —
    // e.g. grant "Blessing of Abundance" (DB2 spell 1229266) or feed renown to
    // faction 2696 "Amani Tribe". Wire the reward path once a completed
    // Abundance harvest is captured; ties into feature/quelthalas-zone-events
    // ZoneEventMgr. No-op until then.
    (void)player;
}

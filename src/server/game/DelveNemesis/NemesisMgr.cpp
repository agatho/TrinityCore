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

#include "NemesisMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "Position.h"
#include "TemporarySummon.h"
#include <algorithm>

namespace DelveNemesis
{

NemesisMgr::NemesisMgr() = default;
NemesisMgr::~NemesisMgr() = default;

NemesisMgr* NemesisMgr::Instance()
{
    static NemesisMgr instance;
    return &instance;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void NemesisMgr::LoadFromDB()
{
    _packs.clear();
    _pactswornKills.clear();

    // nemesis_pactsworn_pack ships EMPTY (pack entries + coords are world-DB /
    // CAPTURE-BLOCKED content). Absent table or zero rows => Pactsworn spawning is
    // an idle no-op, which is realm-safe. The loader is tolerant on both counts.
    if (QueryResult result = WorldDatabase.Query(
            "SELECT Id, MinTier, MapId, Entry, PackSize, PosX, PosY, PosZ, Orientation FROM nemesis_pactsworn_pack"))
    {
        do
        {
            Field* f = result->Fetch();
            PactswornPack pack;
            pack.Id          = f[0].GetUInt32();
            pack.MinTier     = f[1].GetUInt8();
            pack.MapId       = f[2].GetUInt32();
            pack.Entry       = f[3].GetUInt32();
            pack.PackSize    = f[4].GetUInt8();
            pack.PosX        = f[5].GetFloat();
            pack.PosY        = f[6].GetFloat();
            pack.PosZ        = f[7].GetFloat();
            pack.Orientation = f[8].GetFloat();
            _packs.push_back(pack);
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Delve Nemesis: loaded {} Pactsworn pack template(s).", _packs.size());
}

void NemesisMgr::Update(uint32 /*diff*/)
{
    // No periodic work yet. Pactsworn packs are summoned with a duration and
    // self-despawn; the per-instance kill counters are cleared at completion. A
    // future phase may add pack respawn / re-position timers here.
}

// ---------------------------------------------------------------------------
// Delve-integration seam
// ---------------------------------------------------------------------------

void NemesisMgr::OnDelveStarted(Map* map, uint8 tier)
{
    if (!map)
        return;

    // Pactsworn packs only appear in the Tier 4+ escalation.
    if (tier < NEMESIS_PACTSWORN_MIN_TIER)
        return;

    _pactswornKills[map->GetInstanceId()] = 0;

    uint32 spawned = 0;
    for (PactswornPack const& pack : _packs)
    {
        if (pack.MinTier > tier)
            continue;
        if (pack.MapId != 0 && pack.MapId != map->GetId())
            continue;
        if (!pack.Entry)
            continue;

        Position pos(pack.PosX, pack.PosY, pack.PosZ, pack.Orientation);
        for (uint8 i = 0; i < std::max<uint8>(pack.PackSize, 1); ++i)
            if (map->SummonCreature(pack.Entry, pos))
                ++spawned;
    }

    TC_LOG_DEBUG("scripts.delves", "Nemesis: delve started (map {}, instance {}, tier {}) - spawned {} Pactsworn.",
        map->GetId(), map->GetInstanceId(), tier, spawned);
}

void NemesisMgr::OnPactswornKilled(Player* killer)
{
    if (!killer || !killer->GetMap())
        return;

    ++_pactswornKills[killer->GetMap()->GetInstanceId()];
    TC_LOG_DEBUG("scripts.delves", "Nemesis: Pactsworn slain in instance {} (run total {}).",
        killer->GetMap()->GetInstanceId(), _pactswornKills[killer->GetMap()->GetInstanceId()]);
}

uint32 NemesisMgr::GetPactswornKills(uint32 instanceId) const
{
    auto it = _pactswornKills.find(instanceId);
    return it != _pactswornKills.end() ? it->second : 0;
}

void NemesisMgr::OnDelveCompleted(Player* player, uint8 tier, uint32 mapId, bool isSolo)
{
    if (!player)
        return;

    uint32 instanceId = player->GetMap() ? player->GetMap()->GetInstanceId() : 0;
    uint32 kills = GetPactswornKills(instanceId);

    // (1) Nemesis Strongbox - the bonus chest at the end of every Tier 4+ run.
    if (tier >= NEMESIS_PACTSWORN_MIN_TIER)
        AwardNemesisStrongbox(player, tier, kills);

    // (2) Torment's Rise / Nullaeus solo challenge.
    if (IsTormentsRiseMap(mapId))
        OnNullaeusDefeated(player, tier, isSolo);

    // Clear the per-instance counter for this run.
    _pactswornKills.erase(instanceId);
}

// ---------------------------------------------------------------------------
// Nemesis Strongbox
// ---------------------------------------------------------------------------

uint8 NemesisMgr::ComputeStrongboxQuality(uint32 pactswornKills) const
{
    // Placeholder banding: more Pactsworn slain => higher strongbox quality tier.
    // The exact retail curve (kills -> quality) is CAPTURE-BLOCKED; this is a
    // monotonic stand-in that the real loot table keys off.
    if (pactswornKills >= 6)
        return 4;
    if (pactswornKills >= 4)
        return 3;
    if (pactswornKills >= 2)
        return 2;
    return 1;
}

void NemesisMgr::AwardNemesisStrongbox(Player* player, uint8 tier, uint32 pactswornKills)
{
    uint8 quality = ComputeStrongboxQuality(pactswornKills);

    // The Strongbox loot is world-DB content (a gameobject_template chest + its
    // loot, neither present in DB2 @68887 => CAPTURE-BLOCKED). We ship the LIVE
    // quality-scaling spine and route the actual grant through a config loot id
    // that defaults to 0 (inert) so a realm without the content is unaffected.
    uint32 lootId = sConfigMgr->GetIntDefault("Nemesis.Strongbox.LootId", 0);

    // TODO(integration/capture): once the Strongbox gameobject_template + loot
    // is authored, spawn the chest (or grant its loot at `quality`) here. The
    // fork's delve loot path (DelvesRewards::GrantDelveLoot / reference_loot_
    // template + ItemContext) is the model to reuse at integration.
    TC_LOG_DEBUG("scripts.delves", "Nemesis Strongbox for {} (tier {}, {} Pactsworn -> quality {}, lootId {}).",
        player->GetName(), tier, pactswornKills, quality, lootId);
}

// ---------------------------------------------------------------------------
// Nullaeus solo challenge (Torment's Rise)
// ---------------------------------------------------------------------------

void NemesisMgr::OnNullaeusDefeated(Player* player, uint8 tier, bool isSolo)
{
    if (!player)
        return;

    auto award = [player](uint32 achievementId)
    {
        if (AchievementEntry const* ach = sAchievementStore.LookupEntry(achievementId))
            player->CompletedAchievement(ach);
    };

    // First-defeat payout: 30 Hero Dawncrest. Gate on the account not yet holding
    // "My Shady Nemesis" (61797) - a persistent, no-new-schema first-kill test.
    bool firstKill = !player->HasAchieved(ACH_MY_SHADY_NEMESIS);
    if (firstKill)
        player->AddCurrency(CURRENCY_HERO_DAWNCREST, NULLAEUS_FIRST_KILL_DAWNCRESTS, CurrencyGainSource::Loot);

    // Any Nullaeus kill -> "My Shady Nemesis" (grants Nullaeus Domaneye via achievement_reward).
    award(ACH_MY_SHADY_NEMESIS);

    // Tiered kill -> "Lighting the Dark" (title "the Ominous").
    if (tier >= NULLAEUS_TIER_LOW)
        award(ACH_LIGHTING_THE_DARK);

    // Solo kill -> "Let Me Solo Him: Nullaeus" (grants Arcanovoid Construct mount).
    if (isSolo)
    {
        award(ACH_LET_ME_SOLO_NULLAEUS);
        // "Fabled" is additionally gated on being one of the first 4000 in the
        // region - that region-wide count is CAPTURE-BLOCKED, so the fabled award
        // is deliberately NOT granted here (would over-award). The achievement
        // and its criteria remain in DB2 for a future region-counter phase.
        // award(ACH_FABLED_LET_ME_SOLO); // intentionally withheld - see blueprint §h
    }

    TC_LOG_INFO("scripts.delves", "Nemesis: Nullaeus defeated by {} (tier {}, solo {}, firstKill {}).",
        player->GetName(), tier, isSolo, firstKill);
}

} // namespace DelveNemesis

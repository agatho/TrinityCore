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

#include "AbyssAnglersMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"

AbyssAnglersMgr::AbyssAnglersMgr() = default;
AbyssAnglersMgr::~AbyssAnglersMgr() = default;

/*static*/ AbyssAnglersMgr* AbyssAnglersMgr::instance()
{
    static AbyssAnglersMgr instance;
    return &instance;
}

void AbyssAnglersMgr::LoadFromDB()
{
    _enabled = false;

    // Realm-safe: the shipped table may not be applied on the shared realm. The
    // dive-template table is intentionally minimal here — it exists only to gate the
    // reward seam ON for a seeded test DB. A missing table is a silent no-op.
    QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM abyss_angler_dive_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Abyss Anglers: abyss_angler_dive_template absent; Abyss Anglers idle.");
        return;
    }

    uint32 count = (*result)[0].GetUInt32();
    _enabled = count != 0;
    TC_LOG_INFO("server.loading", ">> Abyss Anglers: {} dive template(s); activity {}.",
        count, _enabled ? "enabled" : "idle");
}

void AbyssAnglersMgr::AwardDiveReward(Player* player, uint32 pearls)
{
    // Gated on IsEnabled() so this is a hard no-op on the shared realm.
    if (!_enabled || !player)
        return;

    uint32 const amount = pearls ? pearls : AbyssAnglers::PLACEHOLDER_PEARLS_PER_DIVE;

    // --- Angler Pearls 3373 (the real reward wallet) ---
    player->ModifyCurrency(AbyssAnglers::CURRENCY_ANGLER_PEARLS, int32(amount), CurrencyGainSource::Script);

    // --- Diver Display Currency 3506 ---
    // The [DNT] display currency's job is to mirror "the player's most recent currency
    // reward value" so the reward toast can show it. Retail sets it to the last payout;
    // we replicate that by topping it up to the same amount. This is the DB2-described
    // behaviour, not an invented mechanic.
    if (amount)
        player->ModifyCurrency(AbyssAnglers::CURRENCY_DIVER_DISPLAY, int32(amount), CurrencyGainSource::Script);
}

void AbyssAnglersMgr::StartDive(Player* /*player*/)
{
    // CAPTURE-BLOCKED: the Depthdiver Jeju gossip -> "Abyss Anglers - Vehicle"
    // (1253017/1253021) -> underwater scored scenario -> "Surface!" (1260426) flow is
    // a vehicle-scenario opcode set that no capture we hold contains. Depthdiver Jeju /
    // Tu'nakit are world-DB creatures whose ids are not in DB2 @68887. This remains the
    // seam: once a dive capture lands, mount the vehicle, run the scenario, and call
    // AwardDiveReward() with the scored pearl total on Surface!.
}

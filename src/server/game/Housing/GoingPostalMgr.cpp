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

#include "GoingPostalMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Timer.h"
#include <algorithm>

namespace
{
    // Proximity radius (yards) for consuming the next checkpoint. The exact value
    // is CAPTURE-BLOCKED (real courses are unmeasured); this is a sane flagged
    // default and only matters once checkpoint coords are actually seeded.
    constexpr float GOING_POSTAL_CHECKPOINT_RADIUS = 5.0f;
}

GoingPostalMgr& GoingPostalMgr::Instance()
{
    static GoingPostalMgr instance;
    return instance;
}

void GoingPostalMgr::Initialize()
{
    _enabled = false;
    _routes.clear();
    _activeRaces.clear();

    LoadRoutes();
    LoadCheckpoints();

    // Enabled iff at least one route row is enabled. The 6 DB2-confirmed route
    // rows normally ship enabled=1 (the personal-best/currency mechanism needs no
    // captured coords); the checkpoint auto-progression stays inert until the
    // (CAPTURE-BLOCKED) checkpoint coords are seeded.
    for (auto const& [id, route] : _routes)
    {
        if (route.enabled)
        {
            _enabled = true;
            break;
        }
    }

    if (_enabled)
        TC_LOG_INFO("server.loading", "GoingPostalMgr: enabled — {} route(s) loaded (Going Postal mail-race).",
            uint32(_routes.size()));
    else
        TC_LOG_INFO("server.loading",
            "GoingPostalMgr: disabled (no enabled going_postal_route rows). The personal-best/currency seam is still "
            "available to the debug driver. Checkpoint coords + in-world completion wire are CAPTURE-BLOCKED.");
}

void GoingPostalMgr::LoadRoutes()
{
    // Tolerant: a missing going_postal_route table must never fault world load.
    QueryResult result = WorldDatabase.Query(
        "SELECT id, team, routeIndex, currencyId, enabled, name FROM going_postal_route ORDER BY team, routeIndex");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        GoingPostalRoute route;
        route.id         = fields[0].GetUInt32();
        uint8 const team = fields[1].GetUInt8();
        route.team       = team == uint8(GoingPostalTeam::Horde) ? GoingPostalTeam::Horde : GoingPostalTeam::Alliance;
        route.routeIndex = fields[2].GetUInt8();
        route.currencyId = fields[3].GetUInt32();
        route.enabled    = fields[4].GetBool();
        route.name       = fields[5].GetString();

        if (route.routeIndex < 1 || route.routeIndex > 3)
        {
            TC_LOG_ERROR("sql.sql", "GoingPostalMgr: route {} has out-of-range routeIndex {} (expected 1..3); skipping.",
                route.id, route.routeIndex);
            continue;
        }

        // Cross-check the seeded currency against the DB2-confirmed mapping.
        uint32 const expected = GetRouteCurrencyId(route.team, route.routeIndex);
        if (route.currencyId != expected)
            TC_LOG_WARN("sql.sql", "GoingPostalMgr: route {} (team {}, rt {}) currency {} != DB2-expected {}.",
                route.id, uint32(route.team), route.routeIndex, route.currencyId, expected);

        _routes[route.id] = std::move(route);
    } while (result->NextRow());
}

void GoingPostalMgr::LoadCheckpoints()
{
    // The checkpoint table normally ships EMPTY (coords CAPTURE-BLOCKED). Load
    // tolerantly so a future capture can seed rows and the proximity mechanism
    // begins working with no code change.
    QueryResult result = WorldDatabase.Query(
        "SELECT routeId, seq, mapId, posX, posY, posZ FROM going_postal_route_checkpoint ORDER BY routeId, seq");
    if (!result)
        return;

    uint32 loaded = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 const routeId = fields[0].GetUInt32();

        auto itr = _routes.find(routeId);
        if (itr == _routes.end())
        {
            TC_LOG_ERROR("sql.sql", "GoingPostalMgr: checkpoint references unknown route {}; skipping.", routeId);
            continue;
        }

        GoingPostalCheckpoint cp;
        cp.seq   = fields[1].GetUInt32();
        cp.mapId = fields[2].GetUInt32();
        cp.pos.Relocate(fields[3].GetFloat(), fields[4].GetFloat(), fields[5].GetFloat());
        itr->second.checkpoints.push_back(cp);
        ++loaded;
    } while (result->NextRow());

    if (loaded)
        TC_LOG_INFO("server.loading", "GoingPostalMgr: loaded {} route checkpoint(s).", loaded);
}

uint32 GoingPostalMgr::GetRouteCurrencyId(GoingPostalTeam team, uint8 routeIndex)
{
    if (routeIndex < 1 || routeIndex > 3)
        return 0;

    uint32 const base = team == GoingPostalTeam::Horde
        ? GOING_POSTAL_CURRENCY_HORDE_RT1
        : GOING_POSTAL_CURRENCY_ALLIANCE_RT1;
    return base + (routeIndex - 1);
}

GoingPostalTeam GoingPostalMgr::GetPlayerTeam(Player const* player)
{
    return (player && player->GetTeamId() == TEAM_HORDE) ? GoingPostalTeam::Horde : GoingPostalTeam::Alliance;
}

std::vector<GoingPostalRoute const*> GoingPostalMgr::GetRoutesForTeam(GoingPostalTeam team) const
{
    std::vector<GoingPostalRoute const*> out;
    for (auto const& [id, route] : _routes)
        if (route.team == team)
            out.push_back(&route);

    std::sort(out.begin(), out.end(), [](GoingPostalRoute const* a, GoingPostalRoute const* b)
    {
        return a->routeIndex < b->routeIndex;
    });
    return out;
}

GoingPostalRoute const* GoingPostalMgr::GetRoute(uint32 routeId) const
{
    auto itr = _routes.find(routeId);
    return itr != _routes.end() ? &itr->second : nullptr;
}

GoingPostalRoute const* GoingPostalMgr::GetRoute(GoingPostalTeam team, uint8 routeIndex) const
{
    for (auto const& [id, route] : _routes)
        if (route.team == team && route.routeIndex == routeIndex)
            return &route;
    return nullptr;
}

bool GoingPostalMgr::StartRace(Player* player, uint32 routeId)
{
    if (!player)
        return false;

    GoingPostalRoute const* route = GetRoute(routeId);
    if (!route)
    {
        TC_LOG_DEBUG("misc", "GoingPostalMgr::StartRace: unknown route {}.", routeId);
        return false;
    }

    // Guard the faction restriction — a player may only run their faction's routes.
    if (route->team != GetPlayerTeam(player))
    {
        TC_LOG_DEBUG("misc", "GoingPostalMgr::StartRace: {} cannot run cross-faction route {}.",
            player->GetName(), routeId);
        return false;
    }

    if (IsRacing(player))
    {
        // Restart cleanly rather than stacking races.
        AbandonRace(player);
    }

    GoingPostalActiveRace race;
    race.routeId          = route->id;
    race.currencyId       = route->currencyId;
    race.startMS          = getMSTime();
    race.nextCheckpoint   = 0;
    race.totalCheckpoints = uint32(route->checkpoints.size());
    _activeRaces[player->GetGUID()] = race;

    TC_LOG_DEBUG("misc", "GoingPostalMgr::StartRace: {} started route {} ({} checkpoint(s)).",
        player->GetName(), route->id, race.totalCheckpoints);
    return true;
}

bool GoingPostalMgr::StartRace(Player* player, GoingPostalTeam team, uint8 routeIndex)
{
    GoingPostalRoute const* route = GetRoute(team, routeIndex);
    return route && StartRace(player, route->id);
}

bool GoingPostalMgr::IsRacing(Player const* player) const
{
    return player && _activeRaces.count(player->GetGUID()) != 0;
}

GoingPostalActiveRace const* GoingPostalMgr::GetActiveRace(Player const* player) const
{
    if (!player)
        return nullptr;
    auto itr = _activeRaces.find(player->GetGUID());
    return itr != _activeRaces.end() ? &itr->second : nullptr;
}

bool GoingPostalMgr::TryAdvanceCheckpoint(Player* player)
{
    if (!player)
        return false;

    auto itr = _activeRaces.find(player->GetGUID());
    if (itr == _activeRaces.end())
        return false;

    GoingPostalActiveRace& race = itr->second;
    GoingPostalRoute const* route = GetRoute(race.routeId);
    if (!route || !route->HasCheckpoints())
        return false; // coords CAPTURE-BLOCKED — nothing to progress against yet

    if (race.nextCheckpoint >= route->checkpoints.size())
        return false;

    GoingPostalCheckpoint const& cp = route->checkpoints[race.nextCheckpoint];
    if (cp.mapId != 0 && player->GetMapId() != cp.mapId)
        return false;
    if (player->GetDistance(cp.pos) > GOING_POSTAL_CHECKPOINT_RADIUS)
        return false;

    ++race.nextCheckpoint;
    TC_LOG_DEBUG("misc", "GoingPostalMgr: {} reached checkpoint {}/{} on route {}.",
        player->GetName(), race.nextCheckpoint, uint32(route->checkpoints.size()), route->id);

    // Passing the final checkpoint auto-completes the race.
    if (race.nextCheckpoint >= route->checkpoints.size())
        CompleteRace(player);
    return true;
}

bool GoingPostalMgr::ForceAdvanceCheckpoint(Player* player)
{
    if (!player)
        return false;

    auto itr = _activeRaces.find(player->GetGUID());
    if (itr == _activeRaces.end())
        return false;

    GoingPostalActiveRace& race = itr->second;
    GoingPostalRoute const* route = GetRoute(race.routeId);
    if (!route || !route->HasCheckpoints())
        return false;

    if (race.nextCheckpoint >= route->checkpoints.size())
        return false;

    ++race.nextCheckpoint;
    if (race.nextCheckpoint >= route->checkpoints.size())
        CompleteRace(player);
    return true;
}

GoingPostalResult GoingPostalMgr::CompleteRace(Player* player, std::optional<uint32> elapsedOverrideMS)
{
    if (!player)
        return GoingPostalResult::NotRacing;

    auto itr = _activeRaces.find(player->GetGUID());
    if (itr == _activeRaces.end())
        return GoingPostalResult::NotRacing;

    GoingPostalActiveRace const race = itr->second;
    _activeRaces.erase(itr); // race consumed regardless of outcome

    GoingPostalRoute const* route = GetRoute(race.routeId);
    if (!route)
        return GoingPostalResult::Disabled;

    uint32 const elapsedMS = elapsedOverrideMS.value_or(getMSTimeDiff(race.startMS, getMSTime()));

    // Compare against the stored personal best (lower is better).
    std::optional<uint32> const best = GetPersonalBest(player->GetGUID(), route->id);
    bool const improved = !best.has_value() || elapsedMS < best.value();

    TC_LOG_DEBUG("misc", "GoingPostalMgr::CompleteRace: {} finished route {} in {} ms (previous best {}).",
        player->GetName(), route->id, elapsedMS, best ? std::to_string(*best) : std::string("none"));

    if (!improved)
        return GoingPostalResult::Completed;

    StorePersonalBest(player, *route, elapsedMS);
    return GoingPostalResult::RecordedBest;
}

void GoingPostalMgr::AbandonRace(Player* player)
{
    if (!player)
        return;
    _activeRaces.erase(player->GetGUID());
}

std::optional<uint32> GoingPostalMgr::GetPersonalBest(ObjectGuid guid, uint32 routeId) const
{
    // Tolerant: a missing character_going_postal table must never fault.
    QueryResult result = CharacterDatabase.PQuery(
        "SELECT bestTimeMs FROM character_going_postal WHERE guid = {} AND routeId = {}",
        guid.GetCounter(), routeId);
    if (!result)
        return std::nullopt;

    return result->Fetch()[0].GetUInt32();
}

void GoingPostalMgr::StorePersonalBest(Player* player, GoingPostalRoute const& route, uint32 timeMS)
{
    // Authoritative personal best in the characters DB (realm-safe, tolerant).
    CharacterDatabase.PExecute(
        "REPLACE INTO character_going_postal (guid, routeId, bestTimeMs, updateTime) VALUES ({}, {}, {}, UNIX_TIMESTAMP())",
        player->GetGUID().GetCounter(), route.id, timeMS);

    // Reflect the record onto the DB2 personal-best-record currency. The currency
    // literally IS the "Personal Best Record" (CurrencyCategory 251, the racing UI
    // hidden category), so we SET its quantity to the new best time via a delta so
    // the value always equals the current PB. UNIT ASSUMPTION (flagged): the value
    // is stored in MILLISECONDS to match character_going_postal — Blizzard's exact
    // encoding/unit is CAPTURE-BLOCKED. Uses stock ModifyCurrency.
    if (route.currencyId)
    {
        int32 const current = int32(player->GetCurrencyQuantity(route.currencyId));
        int32 const delta   = int32(timeMS) - current;
        if (delta != 0)
            player->ModifyCurrency(route.currencyId, delta, CurrencyGainSource::Script);
    }

    TC_LOG_DEBUG("misc", "GoingPostalMgr: recorded new best {} ms for {} on route {} (currency {}).",
        timeMS, player->GetName(), route.id, route.currencyId);
}

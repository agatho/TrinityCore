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

#include "TurbulentTimewaysMgr.h"
#include "Common.h"         // WEEK, IN_MILLISECONDS
#include "DatabaseEnv.h"
#include "GameEventMgr.h"   // IsHolidayActive
#include "GameTime.h"
#include "Log.h"
#include "SharedDefines.h"  // HolidayIds

namespace
{
    // How often the light Update() actually recomputes (ms). The rotation only
    // changes on a weekly boundary, so a slow poll is plenty.
    constexpr uint32 TIMEWAYS_UPDATE_INTERVAL_MS = 60 * IN_MILLISECONDS;

    // Seconds per rotation step. Retail advances the featured expansion weekly.
    constexpr time_t TIMEWAYS_ROTATION_PERIOD = time_t(WEEK);
}

TurbulentTimewaysMgr::TurbulentTimewaysMgr() = default;
TurbulentTimewaysMgr::~TurbulentTimewaysMgr() = default;

TurbulentTimewaysMgr* TurbulentTimewaysMgr::instance()
{
    static TurbulentTimewaysMgr instance;
    return &instance;
}

void TurbulentTimewaysMgr::LoadFromDB()
{
    _rotation.clear();
    _activeIndex = 0;

    // Tolerant load: the table may be absent on a realm that has not applied the
    // shipped SQL. A missing table throws at query time in strict setups, so we
    // guard the whole load and leave the manager idle (realm-safe) on failure.
    QueryResult result;
    try
    {
        result = WorldDatabase.Query(
            "SELECT OrderIndex, ChromieExpansionRecId, HolidayId, RandomLfgDungeonId, "
            "GateWorldStateId, WeeklyQuestId, Name FROM turbulent_timeways_rotation ORDER BY OrderIndex");
    }
    catch (std::exception const&)
    {
        result = nullptr;
    }

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 Turbulent Timeways rotation entries (table absent or empty). Event idle.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        TurbulentTimeways::TimelineOffer offer;
        offer.OrderIndex           = fields[0].GetUInt32();
        offer.ChromieExpansionRecId = fields[1].GetInt32();
        offer.HolidayId            = fields[2].GetUInt32();
        offer.RandomLfgDungeonId   = fields[3].GetUInt32();
        offer.GateWorldStateId     = fields[4].GetUInt32();
        offer.WeeklyQuestId        = fields[5].GetUInt32();
        offer.Name                 = fields[6].GetString();

        _rotation.push_back(std::move(offer));
    } while (result->NextRow());

    _activeIndex = ComputeActiveIndex();

    TC_LOG_INFO("server.loading", ">> Loaded {} Turbulent Timeways rotation entries. Active index: {}.",
        _rotation.size(), _activeIndex);
}

uint32 TurbulentTimewaysMgr::ComputeActiveIndex() const
{
    if (_rotation.empty())
        return 0;

    // Deterministic weekly cursor: number of whole rotation periods since the
    // Unix epoch, modulo the rotation length. This keeps every worldserver in a
    // cluster in agreement without persisting state, and is a stand-in until the
    // holiday/game_event calendar (feature/chromie-time merge) drives the window.
    time_t const now = GameTime::GetGameTime();
    uint64 const steps = static_cast<uint64>(now) / static_cast<uint64>(TIMEWAYS_ROTATION_PERIOD);
    return static_cast<uint32>(steps % _rotation.size());
}

bool TurbulentTimewaysMgr::IsActive() const
{
    return IsHolidayActive(HolidayIds(TurbulentTimeways::HOLIDAY_ROW_TURBULENT_MAIN));
}

TurbulentTimeways::TimelineOffer const* TurbulentTimewaysMgr::GetActiveOffer() const
{
    if (_rotation.empty())
        return nullptr;

    uint32 const idx = _activeIndex < _rotation.size() ? _activeIndex : 0;
    return &_rotation[idx];
}

void TurbulentTimewaysMgr::Update(uint32 diff)
{
    if (_rotation.empty())
        return;

    _updateAccumulator += diff;
    if (_updateAccumulator < TIMEWAYS_UPDATE_INTERVAL_MS)
        return;

    _updateAccumulator = 0;

    uint32 const newIndex = ComputeActiveIndex();
    if (newIndex != _activeIndex)
    {
        _activeIndex = newIndex;
        if (TurbulentTimeways::TimelineOffer const* offer = GetActiveOffer())
            TC_LOG_INFO("misc", "Turbulent Timeways rotated to '{}' (order {}).", offer->Name, offer->OrderIndex);

        PublishWorldStates();
    }
}

void TurbulentTimewaysMgr::AdvanceRotation()
{
    if (_rotation.empty())
        return;

    _activeIndex = (_activeIndex + 1) % _rotation.size();

    // TODO(chromie-time merge): persist the cursor via a PersistentWorldVariable
    // (mirror World::NextWeeklyQuestResetTimeVarId) and start the corresponding
    // Holiday.db2 game_event so IsActive()/IsHolidayActive() flips faithfully.
    PublishWorldStates();
}

void TurbulentTimewaysMgr::PublishWorldStates()
{
    // TODO(CAPTURE-BLOCKED): the featured expansion opens its Timewalking LFG
    // queue when the client-side PlayerCondition -> WorldStateExpression reads a
    // truthy worldstate (TBC ws 10276 / WotLK 10279 / MoP 12941 / DF 30129,
    // derived from WorldStateExpression bytes @68887). Pushing those requires a
    // Map* and the on-wire values confirmed from a live rotation capture, so the
    // publish is stubbed until the chromie-time framework and a sniff are merged.
    if (TurbulentTimeways::TimelineOffer const* offer = GetActiveOffer())
        TC_LOG_DEBUG("misc", "Turbulent Timeways: would publish worldstate {} for '{}' (stub).",
            offer->GateWorldStateId, offer->Name);
}

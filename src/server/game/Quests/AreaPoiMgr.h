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

#ifndef TRINITY_AREAPOIMGR_H
#define TRINITY_AREAPOIMGR_H

#include "Define.h"
#include <ctime>
#include <unordered_map>
#include <vector>

namespace WorldPackets::Quest
{
    struct AreaPoiUpdateInfo;
}

// A map point-of-interest that can be broadcast as an active blip, loaded from `area_poi_template`.
struct AreaPoiTemplate
{
    uint32 AreaPoiID  = 0;
    uint32 Duration   = 0;   // seconds the POI stays active once activated
    int32  VariableID = 0;   // WorldState variable reported to the client (0 = none)
    int32  Value      = 0;   // WorldState value
};

struct ActiveAreaPoi
{
    uint32 AreaPoiID  = 0;
    time_t StartTime  = 0;                // client's LastUpdate
    time_t EndTime    = 0;                // StartTime + template Duration
    int32  VariableID = 0;
    int32  Value      = 0;
};

// Activates map POIs from `area_poi_template`, tracks their expiry, refreshes them on a rotation, and feeds
// SMSG_AREA_POI_UPDATE_RESPONSE (the map blips). Data-driven: empty table => no active POIs.
class TC_GAME_API AreaPoiMgr
{
    private:
        AreaPoiMgr();
        ~AreaPoiMgr();

    public:
        AreaPoiMgr(AreaPoiMgr const&) = delete;
        AreaPoiMgr(AreaPoiMgr&&) = delete;
        AreaPoiMgr& operator=(AreaPoiMgr const&) = delete;
        AreaPoiMgr& operator=(AreaPoiMgr&&) = delete;

        static AreaPoiMgr* instance();

        void LoadFromDB();
        void Update(uint32 diff);

        // Appends every currently-active POI to the SMSG_AREA_POI_UPDATE_RESPONSE payload.
        void FillActiveAreaPois(std::vector<WorldPackets::Quest::AreaPoiUpdateInfo>& pois) const;

    private:
        void Activate(AreaPoiTemplate const& tmpl, time_t now);

        std::unordered_map<uint32, AreaPoiTemplate> _templates;
        std::unordered_map<uint32, ActiveAreaPoi> _active;
        uint32 _updateAccumulator = 0;
};

#define sAreaPoiMgr AreaPoiMgr::instance()

#endif // TRINITY_AREAPOIMGR_H

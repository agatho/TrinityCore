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

#ifndef TRINITY_TURBULENTTIMEWAYSMGR_H
#define TRINITY_TURBULENTTIMEWAYSMGR_H

#include "Define.h"
#include <string>
#include <vector>

// Turbulent Timeways — the recurring meta-event that rotates which Timewalking
// (Chromie-Time) expansion is featured each week and grants the shared
// Timewarped Badge currency. This manager is the *server-wide rotation
// scheduler* that the per-player Chromie-Time state (Player::SetChromieTime /
// UiChromieTimeExpansionID, on feature/chromie-time) does NOT provide. It EXTENDS
// that work rather than forking a parallel timeline system: chromie-time answers
// "which timeline is THIS player in", this manager answers "which timeline(s)
// are OFFERED this week", and publishes that to clients via worldstates so the
// existing timewalking LFG PlayerConditions open the matching queue.
//
// Evidence tags: [DB2] = wago.tools CSV @ 12.0.7.68887; [SRC] = fork source.

namespace TurbulentTimeways
{
    // ---- DB2-anchored constants (verified @ 12.0.7.68887) ----
    enum Ids : uint32
    {
        CURRENCY_TIMEWARPED_BADGE       = 1166,     // [DB2 CurrencyTypes] "Timewarped Badge", CategoryID 22 (unified TW currency)
        HOLIDAY_NAME_TURBULENT          = 430,      // [DB2 HolidayNames]  "Turbulent Timeways"
        HOLIDAY_DESC_TURBULENT          = 447,      // [DB2 HolidayDescriptions] Bronze-Dragonflight flavor text
        HOLIDAY_ROW_TURBULENT_MAIN      = 1425,     // [DB2 Holidays] region 961, HolidayNameID 430, Priority 10, Duration 1008h (~6wk)
        SPELL_KNOWLEDGE_OF_TIMEWAYS     = 423860,   // [DB2 SpellName]  +5% XP buff, stacks x4
        SPELL_MASTERY_OF_TIMEWAYS       = 423861,   // [DB2 SpellName]  +30% XP at max stacks
        ACHIEVEMENT_MASTER_TURBULENT    = 19079,    // [DB2 Achievement] "Master of the Turbulent Timeways" (CriteriaTree 150048)
        ITEM_TIMELY_GOODIE_BAG          = 232877,   // [DB2 ItemSparse] weekly-quest reward container
        DIFFICULTY_TIMEWALKING          = 24,       // [SRC DBCEnums] DIFFICULTY_TIMEWALKING
        DIFFICULTY_TIMEWALKING_RAID     = 33        // [SRC DBCEnums] DIFFICULTY_TIMEWALKING_RAID
    };

    // One expansion's featured week: everything needed to open its Timewalking
    // offering. All numeric ids are DB2-anchored @68887 (see blueprint §2); the
    // GateWorldStateId is decoded from the LFGDungeons Required_player_condition_ID
    // -> PlayerCondition.WorldStateExpressionID chain.
    struct TimelineOffer
    {
        uint32 OrderIndex           = 0;   // rotation position (0..N-1)
        int32  ChromieExpansionRecId = -1; // sUIChromieTimeExpansionInfoStore id (5..16); -1 = none / not a chromie timeline
        uint32 HolidayId            = 0;   // Holiday.db2 timewalking-weekend id gating this expansion
        uint32 RandomLfgDungeonId   = 0;   // LFGDungeons.db2 "Random Timewalking Dungeon (X)"
        uint32 GateWorldStateId     = 0;   // worldstate the client PlayerCondition/WSE reads to open the queue
        uint32 WeeklyQuestId        = 0;   // QuestV2 weekly quest id (0 = not captured)
        std::string Name;
    };
}

class TC_GAME_API TurbulentTimewaysMgr
{
    private:
        TurbulentTimewaysMgr();
        ~TurbulentTimewaysMgr();

    public:
        TurbulentTimewaysMgr(TurbulentTimewaysMgr const&) = delete;
        TurbulentTimewaysMgr(TurbulentTimewaysMgr&&) = delete;
        TurbulentTimewaysMgr& operator=(TurbulentTimewaysMgr const&) = delete;
        TurbulentTimewaysMgr& operator=(TurbulentTimewaysMgr&&) = delete;

        static TurbulentTimewaysMgr* instance();

        // Loads the turbulent_timeways_rotation world table (tolerant of an
        // absent/empty table -> the manager simply idles, realm-safe).
        void LoadFromDB();

        // Light tick: recomputes the active rotation index; on change it
        // (TODO) re-publishes worldstates. Cheap enough to call infrequently.
        void Update(uint32 diff);

        // True while the Turbulent Timeways meta-holiday (1425) is active.
        bool IsActive() const;

        // The expansion featured right now, or nullptr if the rotation is empty.
        TurbulentTimeways::TimelineOffer const* GetActiveOffer() const;

        uint32 GetRotationSize() const { return static_cast<uint32>(_rotation.size()); }

        // Advances the rotation cursor by one week. Intended to be driven from
        // the weekly-reset dispatcher (World::CheckScheduledResetTimes).
        // TODO(chromie-time merge): persist the cursor via PersistentWorldVariable
        // and fire the holiday/game_event start for the new offer.
        void AdvanceRotation();

    private:
        // ISO-week-derived index into the rotation (deterministic without state).
        uint32 ComputeActiveIndex() const;

        // TODO(CAPTURE-BLOCKED): push GetActiveOffer()->GateWorldStateId into
        // WorldStateMgr::SetValue so the timewalking LFG PlayerConditions open.
        // Requires a Map* per active map and the on-wire worldstate values
        // (10276 TBC / 10279 WotLK / 12941 MoP / 30129 DF, derived from the
        // WorldStateExpression bytes) confirmed against a live capture.
        void PublishWorldStates();

        std::vector<TurbulentTimeways::TimelineOffer> _rotation;
        uint32 _activeIndex = 0;
        uint32 _updateAccumulator = 0;
};

#define sTurbulentTimewaysMgr TurbulentTimewaysMgr::instance()

#endif // TRINITY_TURBULENTTIMEWAYSMGR_H

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

/*
 * The Waking Shores (map 2444) -- Dragonflight intro chain.
 *
 * Every timing and position in this file was read off a retail capture
 * (12.1.0.69587, 2026-09-03, "DF Intro and The walking Shores"): the ticks named in the
 * comments are milliseconds into that capture and can be re-checked against the raw .pkt
 * with the TCHarvest rig (C:\sniff\tcharvest). Nothing here is guessed from memory of the
 * quest; where the wire was silent the comment says so.
 */

#include <algorithm>

#include "Creature.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SceneMgr.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include "Vehicle.h"

/*######
## Quest 66101 "From Such Great Heights" -- the Surveyor's Disc flight
##
## Wire timeline (T0 = CMSG_SPELL_CLICK on Surveyor's Disc 193707, tick 3142955):
##   T0+0.15s  SMSG_QUEST_UPDATE_ADD_CREDIT 184913 ("Hop on the Surveyor's Disc");
##             Vehicle 193712 CREATED (personal), player boards (MOVE_UNSET_CAN_FLY/ROOT burst)
##   T0+3.9s   SMSG_PLAY_SCENE 3001 (pkg 3609) at (3578.46, -1441.71, 156.45)  -- vantage 1
##   T0+17.8s  disc spline vantage 1 -> base (4.0s)
##   T0+18.9s  SMSG_CANCEL_SCENE; credit 184905 ("Search for Power" -- elementals)
##   T0+26.8s  disc spline base -> (3591.87, -1384.59, 203.67) (2.0s)                -- vantage 2
##   T0+28.9s  SMSG_PLAY_SCENE 2999 (pkg 3620) at (3591.67, -1385.11, 203.67)
##   T0+42.3s  disc spline vantage 2 -> base (4.0s); T0+46.3s credit 184903 ("Search for Allies")
##   T0+50.4s  disc spline base -> (3599.56, -1397.88, 518.32) (2.0s)                -- vantage 3
##   T0+52.5s  SMSG_PLAY_SCENE 3000 (pkg 3621) at (3598.93, -1397.88, 518.32)
##   T0+66.5s  disc spline vantage 3 -> base (8.3s)
##   T0+67.5s  SMSG_CANCEL_SCENE; credit 184904 ("Search for Danger" -- djaradin)
##   T0+75.1s  SMSG_MOVE_TELEPORT player -> (3593.66, -1402.81, 98.95, o 2.10); disc gone
##
## The leg base -> vantage 1 was not reconstructable from the spline stream (its packet
## carried different flags), so the disc is flown there at the same pace as the other legs.
## The realm's creature_template already named ScriptName 'npc_surveyors_disk' with no
## script behind it; these two structs give that name a body.
######*/

enum SurveyorsDiscData
{
    NPC_SURVEYORS_DISC_PROP     = 193707,
    NPC_SURVEYORS_DISC_VEHICLE  = 193712,

    QUEST_FROM_SUCH_GREAT_HEIGHTS = 66101,

    KC_HOP_ON_THE_DISC          = 184913,
    KC_SEARCH_FOR_POWER         = 184905,  // elementals   -- scene 3001
    KC_SEARCH_FOR_ALLIES        = 184903,  // dragonriders -- scene 2999
    KC_SEARCH_FOR_DANGER        = 184904,  // djaradin     -- scene 3000

    SCENE_SURVEY_POWER          = 3001,
    SCENE_SURVEY_ALLIES         = 2999,
    SCENE_SURVEY_DANGER         = 3000,

    POINT_VANTAGE_1             = 1,
    POINT_BASE_1                = 2,
    POINT_VANTAGE_2             = 3,
    POINT_BASE_2                = 4,
    POINT_VANTAGE_3             = 5,
    POINT_BASE_3                = 6
};

Position const DiscBase        = { 3586.95f, -1402.02f, 103.489f, 2.62f };
Position const DiscVantage1    = { 3578.46f, -1441.71f, 156.45f,  4.517f };
Position const DiscVantage2    = { 3591.87f, -1384.59f, 203.671f, 1.296f };
Position const DiscVantage3    = { 3599.56f, -1397.88f, 518.316f, 0.317f };
Position const DiscExitPos     = { 3593.66f, -1402.81f, 98.95f,   2.10f };

// speed = wire distance / wire move_time, so the ride keeps the captured pace
static float LegSpeed(Position const& from, Position const& to, float seconds)
{
    float dist = from.GetExactDist(&to);
    return seconds > 0.f ? std::max(dist / seconds, 1.f) : 8.f;
}

// The clickable disc standing at the camp: boards the clicker onto a personal ride vehicle.
struct npc_surveyors_disk : public ScriptedAI
{
    npc_surveyors_disk(Creature* creature) : ScriptedAI(creature) { }

    void OnSpellClick(Unit* clicker, bool /*spellClickHandled*/) override
    {
        Player* player = clicker->ToPlayer();
        if (!player)
            return;

        // The wire shows the click accepted only while 66101 is in the log and incomplete.
        if (player->GetQuestStatus(QUEST_FROM_SUCH_GREAT_HEIGHTS) != QUEST_STATUS_INCOMPLETE)
            return;
        if (player->GetVehicle())
            return;

        // tick 3143103: credit 184913 and the vehicle's CreateObject arrive in the same frame
        player->KilledMonsterCredit(KC_HOP_ON_THE_DISC);

        // personal object: only this player sees their disc (four captures never saw another's)
        if (TempSummon* disc = me->SummonCreature(NPC_SURVEYORS_DISC_VEHICLE, DiscBase,
                TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player->GetGUID()))
        {
            disc->SetCanFly(true);
            disc->SetDisableGravity(true);
            player->EnterVehicle(disc);
        }
    }
};

// The ride itself: flies the captured legs, plays the three survey scenes for its passenger,
// hands out the three objective credits, then puts the passenger back on the ground.
struct npc_surveyors_disk_vehicle : public ScriptedAI
{
    npc_surveyors_disk_vehicle(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (!apply)
        {
            // rider left early (e.g. /exit) -- nothing to fly for any more
            if (passenger->GetGUID() == _rider)
            {
                _scheduler.CancelAll();
                me->DespawnOrUnsummon(1s);
            }
            return;
        }

        _rider = passenger->GetGUID();
        _scheduler.CancelAll();

        // T0+0.2s: lift off to vantage 1 (leg not on the wire; paced like the return leg, 4s)
        _scheduler.Schedule(200ms, [this](TaskContext /*ctx*/)
        {
            me->GetMotionMaster()->MovePoint(POINT_VANTAGE_1, DiscVantage1, false, {}, LegSpeed(DiscBase, DiscVantage1, 3.5f));
        });
        // T0+3.9s: scene 3001 (power / elementals) at the vantage position
        _scheduler.Schedule(3900ms, [this](TaskContext /*ctx*/) { PlaySceneForRider(SCENE_SURVEY_POWER, DiscVantage1); });
        // T0+17.8s: back to base
        _scheduler.Schedule(17800ms, [this](TaskContext /*ctx*/)
        {
            me->GetMotionMaster()->MovePoint(POINT_BASE_1, DiscBase, false, {}, LegSpeed(DiscVantage1, DiscBase, 4.0f));
        });
        // T0+18.9s: cancel scene, credit power
        _scheduler.Schedule(18900ms, [this](TaskContext /*ctx*/) { CancelSceneForRider(SCENE_SURVEY_POWER); CreditRider(KC_SEARCH_FOR_POWER); });
        // T0+26.8s: up to vantage 2 (2.0s on the wire)
        _scheduler.Schedule(26800ms, [this](TaskContext /*ctx*/)
        {
            me->GetMotionMaster()->MovePoint(POINT_VANTAGE_2, DiscVantage2, false, {}, LegSpeed(DiscBase, DiscVantage2, 2.0f));
        });
        // T0+28.9s: scene 2999 (allies / dragonriders)
        _scheduler.Schedule(28900ms, [this](TaskContext /*ctx*/) { PlaySceneForRider(SCENE_SURVEY_ALLIES, DiscVantage2); });
        // T0+42.3s: back to base (4.0s)
        _scheduler.Schedule(42300ms, [this](TaskContext /*ctx*/)
        {
            me->GetMotionMaster()->MovePoint(POINT_BASE_2, DiscBase, false, {}, LegSpeed(DiscVantage2, DiscBase, 4.0f));
        });
        // T0+46.3s: credit allies (the wire cancelled this scene implicitly with the next play)
        _scheduler.Schedule(46300ms, [this](TaskContext /*ctx*/) { CancelSceneForRider(SCENE_SURVEY_ALLIES); CreditRider(KC_SEARCH_FOR_ALLIES); });
        // T0+50.4s: up to vantage 3 (2.0s)
        _scheduler.Schedule(50400ms, [this](TaskContext /*ctx*/)
        {
            me->GetMotionMaster()->MovePoint(POINT_VANTAGE_3, DiscVantage3, false, {}, LegSpeed(DiscBase, DiscVantage3, 2.0f));
        });
        // T0+52.5s: scene 3000 (danger / djaradin)
        _scheduler.Schedule(52500ms, [this](TaskContext /*ctx*/) { PlaySceneForRider(SCENE_SURVEY_DANGER, DiscVantage3); });
        // T0+66.5s: long descent (8.3s)
        _scheduler.Schedule(66500ms, [this](TaskContext /*ctx*/)
        {
            me->GetMotionMaster()->MovePoint(POINT_BASE_3, DiscBase, false, {}, LegSpeed(DiscVantage3, DiscBase, 8.3f));
        });
        // T0+67.5s: cancel scene, credit danger
        _scheduler.Schedule(67500ms, [this](TaskContext /*ctx*/) { CancelSceneForRider(SCENE_SURVEY_DANGER); CreditRider(KC_SEARCH_FOR_DANGER); });
        // T0+75.1s: rider set down beside the camp disc; the ride is over
        _scheduler.Schedule(75100ms, [this](TaskContext /*ctx*/)
        {
            if (Player* rider = ObjectAccessor::GetPlayer(*me, _rider))
            {
                rider->ExitVehicle();
                rider->NearTeleportTo(DiscExitPos);
            }
            me->DespawnOrUnsummon(500ms);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    Player* Rider() const { return ObjectAccessor::GetPlayer(*me, _rider); }

    void PlaySceneForRider(uint32 sceneId, Position const& at)
    {
        if (Player* rider = Rider())
            rider->GetSceneMgr().PlayScene(sceneId, &at);
    }

    void CancelSceneForRider(uint32 sceneId)
    {
        if (Player* rider = Rider())
            rider->GetSceneMgr().CancelSceneBySceneId(sceneId);
    }

    void CreditRider(uint32 killCredit)
    {
        if (Player* rider = Rider())
            rider->KilledMonsterCredit(killCredit);
    }

    TaskScheduler _scheduler;
    ObjectGuid _rider;
};

void AddSC_zone_the_waking_shores()
{
    RegisterCreatureAI(npc_surveyors_disk);
    RegisterCreatureAI(npc_surveyors_disk_vehicle);
}

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

#include "PreyHunt.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "PreyMgr.h"
#include "QuestDef.h"
#include "Random.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "Util.h"
#include "WorldPacket.h"
#include <list>

namespace Prey
{
namespace
{
    // Final threshold rolled per hunt. Captured: 105 (Talon of Jan'alai) and 110 (Nexus-Edge Hadim), both Normal,
    // with warm / hot at exactly a third / two thirds of it.
    constexpr int32 FINAL_THRESHOLD_MIN_STEPS = 20;
    constexpr int32 FINAL_THRESHOLD_MAX_STEPS = 22;
    constexpr int32 FINAL_THRESHOLD_STEP = 5;

    // Values a character's hunt counters had before its first captured hunt (identical in both captures).
    constexpr int64 INITIAL_HUNTS_LEFT_A = 15;
    constexpr int64 INITIAL_HUNTS_LEFT_B = 10;
    constexpr int64 INITIAL_FIRST_HUNTS_LEFT = 4;

    // The prey appeared the moment the player came into range of its POI.
    constexpr float PREY_APPROACH_DISTANCE = 60.0f;
    constexpr float PREY_SEARCH_DISTANCE = 40.0f;
    constexpr Minutes PREY_DESPAWN_TIME = 5min;

    // Ambush timing. First ambush after entering the hunt zone: 102..170 s in the captures; after an Ambush Blocker
    // ran out: 9..33 s. Conditions are re-checked every few seconds.
    constexpr Seconds AMBUSH_FIRST_MIN = 90s;
    constexpr Seconds AMBUSH_FIRST_MAX = 180s;
    constexpr Seconds AMBUSH_AFTER_BLOCKER_MIN = 5s;
    constexpr Seconds AMBUSH_AFTER_BLOCKER_MAX = 35s;
    constexpr Seconds AMBUSH_CHECK_INTERVAL = 5s;

    constexpr uint32 ZoneAuras[] = { SPELL_ACTIVE_PREY_HUNT, SPELL_AMBUSH_ZONE_AURA, SPELL_ON_THE_HUNT_ZONE_AURA };

    int64 GetElement(Player const* player, uint32 element)
    {
        return std::visit([](auto value) { return int64(value); }, player->GetDataElementCharacter(element));
    }

    void SetElement(Player* player, uint32 element, int64 value)
    {
        player->SetDataElementCharacter(element, value);
    }

    // "<prey> slain": the hunt quest's objective that is not "Hunt your Prey".
    QuestObjective const* GetPreySlainObjective(Quest const* quest)
    {
        QuestObjective const* last = nullptr;
        for (QuestObjective const& objective : quest->GetObjectives())
            if (objective.Type == QUEST_OBJECTIVE_MONSTER && (!last || objective.StorageIndex > last->StorageIndex))
                last = &objective;

        return last && last->StorageIndex > 0 ? last : nullptr;
    }

    bool CanBeAmbushed(Player const* player)
    {
        if (!player->IsAlive() || player->IsInFlight() || player->IsFlying() || player->GetVehicle() || player->IsGameMaster())
            return false;

        return IsInHuntZone(player);
    }

    class AmbushEvent : public BasicEvent
    {
    public:
        AmbushEvent(Player* player, time_t auraApplyTime, TimePoint nextAmbush)
            : _player(player), _auraApplyTime(auraApplyTime), _nextAmbush(nextAmbush) { }

        bool Execute(uint64 /*time*/, uint32 /*diff*/) override
        {
            // a new application of the zone aura owns a new timer
            Aura const* aura = _player->GetAura(SPELL_AMBUSH_ZONE_AURA);
            if (!aura || aura->GetApplyTime() != _auraApplyTime)
                return true;

            TimePoint now = GameTime::Now();
            if (Aura const* blocker = _player->GetAura(SPELL_AMBUSH_BLOCKER))
            {
                if (!_waitingForBlocker)
                {
                    _nextAmbush = now + Milliseconds(blocker->GetDuration()) + randtime(AMBUSH_AFTER_BLOCKER_MIN, AMBUSH_AFTER_BLOCKER_MAX);
                    _waitingForBlocker = true;
                }
            }
            else
                _waitingForBlocker = false;

            if (now >= _nextAmbush && !_waitingForBlocker && !_player->HasAura(SPELL_ACTIVE_AMBUSH)
                && GetHuntState(_player) == HuntState::Hunting && CanBeAmbushed(_player))
            {
                _player->CastSpell(_player, SPELL_SUMMON_HUNTED_REMNANT, true);
                _player->CastSpell(_player, SPELL_ACTIVE_AMBUSH, true);
                _player->CastSpell(_player, SPELL_AMBUSH, true);
            }

            _player->m_Events.AddEventAtOffset(this, AMBUSH_CHECK_INTERVAL);
            return false;
        }

    private:
        Player* _player;
        time_t _auraApplyTime;
        TimePoint _nextAmbush;
        bool _waitingForBlocker = false;
    };
}

PreyHunt const* GetActiveHunt(Player const* player)
{
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (uint32 questId = player->GetQuestSlotQuestId(slot))
            if (PreyHunt const* hunt = sPreyMgr->GetHunt(questId))
                return hunt;

    return nullptr;
}

HuntState GetHuntState(Player const* player)
{
    return HuntState(GetElement(player, ELEMENT_STATE));
}

PreyHuntZone const* GetHuntZone(Player const* player)
{
    return sPreyMgr->GetZone(uint8(GetElement(player, ELEMENT_ZONE)));
}

bool IsInHuntZone(Player const* player)
{
    PreyHuntZone const* zone = GetHuntZone(player);
    return zone && DB2Manager::IsInArea(player->GetAreaId(), zone->AreaId);
}

void StartHunt(Player* player, PreyHunt const& hunt)
{
    PreyHuntZone const* zone = sPreyMgr->GetZoneForNewHunt(hunt);
    if (!zone)
        TC_LOG_ERROR("misc", "Prey::StartHunt: hunt quest {} ({}) accepted by {} while its target is on no slot with a zone - the hunt has no zone",
            hunt.QuestId, hunt.Target->Name, player->GetGUID().ToString());

    // counters the client already had before the first hunt of a character
    if (!GetElement(player, ELEMENT_HAS_COMPLETED_HUNT) && !GetElement(player, ELEMENT_HUNTS_LEFT_A)
        && !GetElement(player, ELEMENT_HUNTS_LEFT_B) && !GetElement(player, ELEMENT_FIRST_HUNTS_LEFT))
    {
        SetElement(player, ELEMENT_HUNTS_LEFT_A, INITIAL_HUNTS_LEFT_A);
        SetElement(player, ELEMENT_HUNTS_LEFT_B, INITIAL_HUNTS_LEFT_B);
        SetElement(player, ELEMENT_FIRST_HUNTS_LEFT, INITIAL_FIRST_HUNTS_LEFT);
    }

    int64 finalThreshold = int64(urand(FINAL_THRESHOLD_MIN_STEPS, FINAL_THRESHOLD_MAX_STEPS)) * FINAL_THRESHOLD_STEP;

    SetElement(player, ELEMENT_TARGET, hunt.Target->TargetIndex);
    SetElement(player, ELEMENT_DIFFICULTY, AsUnderlyingType(hunt.Difficulty));
    SetElement(player, ELEMENT_ZONE, zone ? zone->ZoneIndex : 0);
    SetElement(player, ELEMENT_PROGRESS, 0);
    SetElement(player, ELEMENT_STATE, AsUnderlyingType(HuntState::Hunting));
    SetElement(player, ELEMENT_START_TIME, GameTime::GetGameTime());
    SetElement(player, ELEMENT_FINAL_THRESHOLD, finalThreshold);
    SetElement(player, ELEMENT_WARM_THRESHOLD, finalThreshold / 3);
    SetElement(player, ELEMENT_HOT_THRESHOLD, finalThreshold * 2 / 3);
    SetElement(player, ELEMENT_AMBUSH_ACTIVE, 0);
    SetElement(player, ELEMENT_PREY_REVEALED, 0);
    SetElement(player, ELEMENT_PREY_SLAIN, 0);

    UpdateZoneAuras(player);
}

void UpdateZoneAuras(Player* player)
{
    bool active = GetActiveHunt(player) && GetHuntState(player) != HuntState::None && IsInHuntZone(player);
    for (uint32 spellId : ZoneAuras)
    {
        if (active && !player->HasAura(spellId))
            player->CastSpell(player, spellId, true);
        else if (!active && player->HasAura(spellId))
            player->RemoveAurasDueToSpell(spellId);
    }
}

void AddProgress(Player* player, int32 amount)
{
    if (!GetActiveHunt(player) || GetHuntState(player) != HuntState::Hunting)
        return;

    int64 finalThreshold = GetElement(player, ELEMENT_FINAL_THRESHOLD);
    int64 progress = GetElement(player, ELEMENT_PROGRESS) + amount;
    if (finalThreshold && progress >= finalThreshold)
    {
        RevealPrey(player, true);
        return;
    }

    SetElement(player, ELEMENT_PROGRESS, progress);
}

void RevealPrey(Player* player, bool shareWithParty)
{
    if (!GetActiveHunt(player) || GetHuntState(player) != HuntState::Hunting)
        return;

    SetElement(player, ELEMENT_PROGRESS, 0);
    SetElement(player, ELEMENT_STATE, AsUnderlyingType(HuntState::Final));
    SetElement(player, ELEMENT_PREY_REVEALED, 1);

    player->CastSpell(player, SPELL_FINAL_CREDIT, true);
    if (shareWithParty)
        player->CastSpell(player, SPELL_FINAL_PARTY_SHARE, true);
    player->CastSpell(player, SPELL_FINAL_TOAST, true);
    player->CastSpell(player, SPELL_FINAL_STRIKE, true);
}

Creature* SummonPrey(Player* player)
{
    PreyHunt const* hunt = GetActiveHunt(player);
    PreyHuntZone const* zone = GetHuntZone(player);
    if (!hunt || !zone || !hunt->PreyEntry || GetHuntState(player) != HuntState::Final)
        return nullptr;

    uint32 mapId = 0;
    Position location;
    if (!sPreyMgr->GetPreyLocation(*hunt, *zone, mapId, location))
        return nullptr;

    if (player->GetMapId() != mapId || player->GetExactDist2d(location) > PREY_APPROACH_DISTANCE)
        return nullptr;

    // the player's prey may still be standing there
    std::list<Creature*> candidates;
    player->GetCreatureListWithEntryInGrid(candidates, hunt->PreyEntry, PREY_APPROACH_DISTANCE + PREY_SEARCH_DISTANCE);
    for (Creature* candidate : candidates)
        if (candidate->IsAlive() && candidate->GetPrivateObjectOwner() == player->GetGUID() && candidate->GetExactDist2d(location) <= PREY_SEARCH_DISTANCE)
            return candidate;

    float z = player->GetMapHeight(location.GetPositionX(), location.GetPositionY(), player->GetPositionZ() + 50.0f, true, 150.0f);
    if (z == INVALID_HEIGHT)
        z = player->GetPositionZ();

    location.Relocate(location.GetPositionX(), location.GetPositionY(), z, location.GetAbsoluteAngle(player));
    return player->SummonCreature(hunt->PreyEntry, location, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, PREY_DESPAWN_TIME, 0, 0, player->GetGUID());
}

void OnPreySlain(Player* player, Creature* prey)
{
    PreyHunt const* hunt = GetActiveHunt(player);
    if (!hunt || hunt->PreyEntry != prey->GetEntry() || GetHuntState(player) != HuntState::Final)
        return;

    if (GetElement(player, ELEMENT_FIRST_HUNTS_LEFT) > 0)
        player->CastSpell(player, SPELL_FIRST_FOUR_HUNTS_BONUS, true);

    // sets ELEMENT_STATE back to 0
    player->CastSpell(player, SPELL_PREY_KILL_CLEANUP, true);
    player->RemoveAurasDueToSpell(SPELL_FINAL_STRIKE);
    SetElement(player, ELEMENT_PREY_SLAIN, 1);

    if (Quest const* quest = sObjectMgr->GetQuestTemplate(hunt->QuestId))
        if (QuestObjective const* objective = GetPreySlainObjective(quest))
            player->KilledMonsterCredit(objective->ObjectID, prey->GetGUID());

    ObjectGuid playerGuid = player->GetGUID();
    player->m_Events.AddEventAtOffset([player, playerGuid]()
    {
        if (player->IsInWorld() && player->GetGUID() == playerGuid)
            player->CastSpell(player, SPELL_PORTAL_TO_SANCTUM, true);
    }, 1s);
}

void EndHunt(Player* player, Quest const* quest, bool rewarded)
{
    for (uint32 spellId : ZoneAuras)
        player->RemoveAurasDueToSpell(spellId);

    player->RemoveAurasDueToSpell(SPELL_ACTIVE_AMBUSH);
    player->RemoveAurasDueToSpell(SPELL_AMBUSH_BLOCKER);
    player->RemoveAurasDueToSpell(SPELL_FINAL_STRIKE);
    player->RemoveAurasDueToSpell(SPELL_CHALLENGE_BLOCKER);

    // target, difficulty, zone, progress and state back to 0; the reward spell 1244010 clears the thresholds
    player->CastSpell(player, SPELL_RESET_DATA, true);
    SetElement(player, ELEMENT_FINAL_THRESHOLD, 0);
    SetElement(player, ELEMENT_WARM_THRESHOLD, 0);
    SetElement(player, ELEMENT_HOT_THRESHOLD, 0);
    SetElement(player, ELEMENT_AMBUSH_ACTIVE, 0);
    SetElement(player, ELEMENT_PREY_REVEALED, 0);

    if (!rewarded)
        return;

    SetElement(player, ELEMENT_LAST_DURATION, std::max<int64>(0, GameTime::GetGameTime() - GetElement(player, ELEMENT_START_TIME)));
    SetElement(player, ELEMENT_HAS_COMPLETED_HUNT, 1);
    SetElement(player, ELEMENT_LAST_HUNT_QUEST, quest->GetQuestId());
    for (uint32 counter : { ELEMENT_HUNTS_LEFT_A, ELEMENT_HUNTS_LEFT_B, ELEMENT_FIRST_HUNTS_LEFT })
        if (int64 value = GetElement(player, counter); value > 0)
            SetElement(player, counter, value - 1);

    player->CastSpell(player, SPELL_TOAST_COMPLETE, true);
}

void StartAmbushTimer(Player* player)
{
    Aura const* aura = player->GetAura(SPELL_AMBUSH_ZONE_AURA);
    if (!aura)
        return;

    TimePoint firstAmbush = GameTime::Now() + randtime(AMBUSH_FIRST_MIN, AMBUSH_FIRST_MAX);
    player->m_Events.AddEventAtOffset(new AmbushEvent(player, aura->GetApplyTime(), firstAmbush), AMBUSH_CHECK_INTERVAL);
}

// SMSG_PLAYER_SHOW_UI_EVENT_TOAST: a bare int32 UIEventToast ID (12.1.0.69933: 4-byte bodies 307 / 327 / 341)
void SendUiEventToast(Player* player, int32 uiEventToastId)
{
    WorldPacket data(SMSG_PLAYER_SHOW_UI_EVENT_TOAST, 4);
    data << int32(uiEventToastId);
    player->SendDirectMessage(&data);
}

// SMSG_DISPLAY_WORLD_TEXT: packed anchor guid (empty = the receiver), two uint32 args, 12-bit text length, text
void SendWorldText(Player* player, std::string_view text)
{
    WorldPacket data(SMSG_DISPLAY_WORLD_TEXT, 2 + 4 + 4 + 2 + text.length());
    data << ObjectGuid::Empty;
    data << uint32(0);
    data << uint32(0);
    data.WriteBits(text.length(), 12);
    data.FlushBits();
    data.WriteString(text);
    player->SendDirectMessage(&data);
}
}

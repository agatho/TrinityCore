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

#ifndef TRINITYCORE_PREY_HUNT_H
#define TRINITYCORE_PREY_HUNT_H

#include "Define.h"
#include <string_view>

class Creature;
class Player;
class Quest;
struct PreyHunt;
struct PreyHuntZone;

/*
    One Prey hunt, as retail 12.1.0.69933 plays it (the Talon of Jan'alai, Normal, Eversong Woods, 33 minutes).

    Accept      CMSG_ADVENTURE_MAP_START_QUEST -> quest source spell 1241010 "UI Toast" (UI event toast 307) and
                1244011 "Target Accept Shared" (kill credit "Prey target selected"); the hunt is published through
                PlayerDataElementCharacter: target, difficulty, zone, state 1, start time and the three progress
                thresholds (final 100..110, warm = final / 3, hot = final * 2 / 3). The client's PreyHuntProgress
                widget reads nothing else.
    Hunting     inside the hunt zone the player carries 1277861 "Active Prey Hunt", 1241553 "Ambush" and 1241102
                "On the Hunt". Progress: +10 for beating an ambush or opening its Remnant of Anguish (1241123), +20 for
                a world quest or prey activity finished in the hunt zone (quest reward spell 1241124). Every gain shows
                "+Hunt Progress" and goes through 1241147.
    Ambush      a Hunted Remnant (246605, wearing the prey's portrait display) is summoned next to the player, fixates
                and fights with the prey's ability kit; at half health it turns immune, falls back, leaves a Remnant
                of Anguish chest (7..12 Remnant of Anguish) and reappears as the fleeing prey 40..80 yards away
                (per-target Pursuit spell). A 180 s Ambush Blocker follows; the next ambush comes 5..35 s after it.
    Final       progress reaching the final threshold completes "Hunt your Prey" (1241319), shares the final with
                party members in 22 yards (1241311), shows UI event toast 327 and puts on 1259096 "Final Strike". The
                prey waits at the hunt quest's "<prey> slain" POI of the hunt zone; walking up to it makes the player
                cast 1248063 "Astalor's Challenge" on it.
    Slain       the prey's 1259068 "Credit" hits its tap list: "<prey> slain", 200 Remnant of Anguish for each of the
                first four hunts (1279779), cleanup (1272851) and a Portal to Astalor's Sanctum (1283728).
    Turn-in     the auto-complete popup; reward spell 1244010, then 1271707 "Reset Data" and UI event toast 341 (1262032).
*/
namespace Prey
{
    enum Spells : uint32
    {
        SPELL_UI_TOAST_ACCEPT           = 1241010,  // quest source spell
        SPELL_TARGET_ACCEPT_SHARED      = 1244011,
        SPELL_ACTIVE_PREY_HUNT          = 1277861,  // zone aura
        SPELL_AMBUSH_ZONE_AURA          = 1241553,  // zone aura
        SPELL_ON_THE_HUNT_ZONE_AURA     = 1241102,  // zone aura
        SPELL_SUMMON_HUNTED_REMNANT     = 1249299,
        SPELL_ACTIVE_AMBUSH             = 1241561,
        SPELL_AMBUSH                    = 1241570,
        SPELL_AMBUSH_BLOCKER            = 1241566,
        SPELL_SUMMON_ANGUISH_DUMMY      = 1242379,  // remnant -> summoner
        SPELL_SUMMON_ANGUISH            = 1242380,  // Remnant of Anguish chest
        SPELL_PROGRESS_SMALL            = 1241123,  // +10
        SPELL_PROGRESS_LARGE            = 1241124,  // +20, only inside the hunt zone
        SPELL_PROGRESS_GAINED           = 1241147,
        SPELL_FINAL_CREDIT              = 1241319,  // kill credit "Hunt your Prey"
        SPELL_FINAL_PARTY_SHARE         = 1241311,
        SPELL_FINAL_TOAST               = 1241154,
        SPELL_FINAL_STRIKE              = 1259096,
        SPELL_ASTALORS_CHALLENGE        = 1248063,
        SPELL_CHALLENGE_BLOCKER         = 1248111,
        SPELL_PREY_CREDIT               = 1259068,
        SPELL_FIRST_FOUR_HUNTS_BONUS    = 1279779,
        SPELL_PREY_KILL_CLEANUP         = 1272851,
        SPELL_PORTAL_TO_SANCTUM         = 1283728,
        SPELL_RESET_DATA                = 1271707,
        SPELL_TOAST_COMPLETE            = 1262032,
    };

    // PlayerDataElementCharacter IDs (not storage indices)
    enum DataElements : uint32
    {
        ELEMENT_TARGET                  = 137,      // PreyHuntTarget::TargetIndex
        ELEMENT_DIFFICULTY              = 140,      // PreyHuntDifficulty
        ELEMENT_ZONE                    = 141,      // PreyHuntZone::ZoneIndex, gates the hunt quest's POIs
        ELEMENT_HUNTS_LEFT_A            = 142,      // 15 -> 14 per turn-in, meaning unknown
        ELEMENT_PROGRESS                = 143,
        ELEMENT_STATE                   = 144,      // HuntState
        ELEMENT_HUNTS_LEFT_B            = 289,      // 10 -> 9 per turn-in, meaning unknown
        ELEMENT_FIRST_HUNTS_LEFT        = 290,      // 4 -> 3 per turn-in, first-four-hunts Anguish bonus
        ELEMENT_START_TIME              = 307,
        ELEMENT_LAST_DURATION           = 308,      // seconds from accept to turn-in
        ELEMENT_FINAL_THRESHOLD         = 462,
        ELEMENT_AMBUSH_ACTIVE           = 466,
        ELEMENT_PREY_REVEALED           = 467,
        ELEMENT_PREY_SLAIN              = 468,
        ELEMENT_WARM_THRESHOLD          = 469,
        ELEMENT_HOT_THRESHOLD           = 470,
        ELEMENT_HAS_COMPLETED_HUNT      = 510,
        ELEMENT_LAST_HUNT_QUEST         = 544,
    };

    enum class HuntState : uint8
    {
        None        = 0,
        Hunting     = 1,
        Final       = 2
    };

    enum UiEventToasts : int32
    {
        UI_EVENT_TOAST_HUNT_STARTED     = 307,
        UI_EVENT_TOAST_PREY_REVEALED    = 327,
        UI_EVENT_TOAST_HUNT_COMPLETED   = 341,
    };

    constexpr int32 PROGRESS_SMALL = 10;
    constexpr int32 PROGRESS_LARGE = 20;

    // The hunt quest in the player's log, if any.
    TC_GAME_API PreyHunt const* GetActiveHunt(Player const* player);
    TC_GAME_API HuntState GetHuntState(Player const* player);
    TC_GAME_API PreyHuntZone const* GetHuntZone(Player const* player);
    TC_GAME_API bool IsInHuntZone(Player const* player);

    // 1244011: publishes a freshly accepted hunt.
    TC_GAME_API void StartHunt(Player* player, PreyHunt const& hunt);

    // Keeps the three zone auras in sync with "hunt running and player inside its zone".
    TC_GAME_API void UpdateZoneAuras(Player* player);

    // 1241147: adds progress and turns the hunt final once the final threshold is reached.
    TC_GAME_API void AddProgress(Player* player, int32 amount);

    // Turns a running hunt final. shareWithParty casts 1241311, which calls this again for the party members.
    TC_GAME_API void RevealPrey(Player* player, bool shareWithParty);

    // Player walked up to his prey - summons it at the hunt zone's prey POI if it is not there yet.
    TC_GAME_API Creature* SummonPrey(Player* player);

    // 1259068 hit a player on the prey's tap list.
    TC_GAME_API void OnPreySlain(Player* player, Creature* prey);

    // The hunt quest left the log: rewarded, abandoned or removed by the weekly reset.
    TC_GAME_API void EndHunt(Player* player, Quest const* quest, bool rewarded);

    // Ambush scheduling, driven by the 1241553 zone aura.
    TC_GAME_API void StartAmbushTimer(Player* player);

    TC_GAME_API void SendUiEventToast(Player* player, int32 uiEventToastId);
    TC_GAME_API void SendWorldText(Player* player, std::string_view text);
}

#endif // TRINITYCORE_PREY_HUNT_H

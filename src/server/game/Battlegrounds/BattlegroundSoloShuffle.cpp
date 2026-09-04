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

#include "BattlegroundSoloShuffle.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellHistory.h"

BattlegroundSoloShuffle::BattlegroundSoloShuffle(BattlegroundTemplate const* battlegroundTemplate)
    : Arena(battlegroundTemplate)
{
}

void BattlegroundSoloShuffle::AddPlayer(Player* player, BattlegroundQueueTypeId queueId)
{
    // Base does the arena flag-cast + team/score bookkeeping (round 1's teams are the queue's balanced split).
    Arena::AddPlayer(player, queueId);

    if (_assigned >= _lobby.size())
        return;

    // Classify by spec role. CheckSoloQueueMatch placed exactly 2 healers + 4 dps, so healers take slots 0/1
    // and damage takes 2..5 - a stable identity used by every reshuffle.
    ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
    bool const isHealer = spec && spec->GetRole() == ChrSpecializationRole::Healer;

    uint8 slot = 6;
    if (isHealer)
        slot = _lobby[0].Guid.IsEmpty() ? 0 : 1;
    else
        for (uint8 i = 2; i < 6; ++i)
            if (_lobby[i].Guid.IsEmpty()) { slot = i; break; }

    // Defensive fallback if the role mix is not exactly 2+4 (e.g. a spec with no healer role queued a healer slot).
    if (slot >= 6)
        for (uint8 i = 0; i < 6; ++i)
            if (_lobby[i].Guid.IsEmpty()) { slot = i; break; }

    if (slot < 6)
    {
        _lobby[slot].Guid = player->GetGUID();
        _lobby[slot].IsHealer = isHealer;
        ++_assigned;
    }
}

void BattlegroundSoloShuffle::EndBattleground(Team winner)
{
    // Arena::CheckWinConditions calls this the instant one 3-stack is wiped - i.e. a ROUND ended. While the
    // series is still running and all six players remain, tally the round and reshuffle for the next one rather
    // than tearing the instance down. Round 6 (or an abnormal end where a leaver has dropped the head-count
    // below 6) falls through to the real Arena/Battleground teardown. The round counter also bounds this: even
    // a repeated non-wipe EndBattleground (e.g. a match timeout) advances the round and terminates by round 6.
    if (GetStatus() == STATUS_IN_PROGRESS && _currentRound < 6
        && (GetPlayersCountByTeam(ALLIANCE) + GetPlayersCountByTeam(HORDE)) >= 6)
    {
        for (LobbySlot& s : _lobby)
            if (Player* p = ObjectAccessor::FindPlayer(s.Guid))
                if (p->GetBGTeam() == winner)
                    ++s.RoundsWon;

        TC_LOG_DEBUG("bg.battleground", "Solo Shuffle {}: round {} won by team {}", GetInstanceID(), _currentRound, uint32(winner));

        ++_currentRound;
        StartRound();
        return;
    }

    // Final per-player record (personal-rating persistence is the P2 increment - see the Solo Shuffle plan).
    for (LobbySlot const& s : _lobby)
        if (!s.Guid.IsEmpty())
            TC_LOG_DEBUG("bg.battleground", "Solo Shuffle {}: {} finished {}/6 rounds won", GetInstanceID(), s.Guid.ToString(), s.RoundsWon);

    Battleground::EndBattleground(winner);
}

void BattlegroundSoloShuffle::StartRound()
{
    std::array<uint8, 2> const& allianceDps = kDpsPairForAllianceSide[_currentRound - 1];

    for (uint8 i = 0; i < 6; ++i)
    {
        Player* p = ObjectAccessor::FindPlayer(_lobby[i].Guid);
        if (!p)
            continue;

        // Healer 0 anchors ALLIANCE, healer 1 anchors HORDE; the four damage players split by this round's
        // fixed 2-subset, so no two rounds share a partition.
        Team team;
        if (i == 0)
            team = ALLIANCE;
        else if (i == 1)
            team = HORDE;
        else
        {
            uint8 const dpsIdx = i - 2;   // 0..3
            team = (dpsIdx == allianceDps[0] || dpsIdx == allianceDps[1]) ? ALLIANCE : HORDE;
        }

        AddOrSetPlayerToCorrectBgGroup(p, team);
        p->SetBGTeam(team);
        p->SetArenaFaction(team);

        // Hard round reset: resurrect the losers, top health/power off, wipe cooldowns and every non-permanent
        // aura (DoTs/HoTs/CC/buffs), then re-teleport to the team's start position.
        if (!p->IsAlive())
        {
            p->ResurrectPlayer(1.0f);
            p->SpawnCorpseBones();
        }
        p->SetFullHealth();
        p->ResetAllPowers();
        p->GetSpellHistory()->ResetAllCooldowns();
        p->RemoveAppliedAuras([](AuraApplication const* aurApp)
        {
            return !aurApp->GetBase()->IsPermanent();
        });

        if (WorldSafeLocsEntry const* pos = GetTeamStartPosition(GetTeamIndexByTeamId(team)))
            p->TeleportTo(pos->Loc);
    }

    UpdateWorldState(ARENA_WORLD_STATE_SOLO_SHUFFLE_ROUND, _currentRound);
    UpdateWorldState(ARENA_WORLD_STATE_SHOW_SOLO_SHUFFLE_ROUND, 1);
}

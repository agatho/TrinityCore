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
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "World.h"
#include <cmath>

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

        // Load this character's persisted solo-shuffle MMR (slot kSoloShuffleArenaSlot in character_arena_stats),
        // defaulting to the configured start rating on the first ever queue. The statement is CONNECTION_SYNCH,
        // so a direct query here is safe and mirrors ArenaTeam::LoadMemberFromDB.
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MATCH_MAKER_RATING);
        stmt->setUInt64(0, player->GetGUID().GetCounter());
        stmt->setUInt8(1, kSoloShuffleArenaSlot);
        if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
            _lobby[slot].Rating = (*result)[0].GetUInt16();
        else
            _lobby[slot].Rating = static_cast<uint16>(sWorld->getIntConfig(CONFIG_ARENA_START_MATCHMAKER_RATING));

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

    // Series over: persist each player's personal solo-shuffle MMR. Each of the six rounds is scored as one
    // game against the lobby's average rating, so a player's rating moves by a bounded Elo step driven by how
    // many of the six rounds they won. The lobby average is a fair single opponent proxy for a reshuffled pod
    // where everyone faced everyone.
    uint32 ratingSum = 0;
    uint8 present = 0;
    for (LobbySlot const& s : _lobby)
        if (!s.Guid.IsEmpty())
        {
            ratingSum += s.Rating;
            ++present;
        }

    if (present)
    {
        double const avgRating = double(ratingSum) / double(present);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (LobbySlot const& s : _lobby)
        {
            if (s.Guid.IsEmpty())
                continue;

            // Expected win probability of a single round against an average-rated opponent, then the whole-series
            // delta = K * (actualWins - expectedWins) over the six rounds. At an even lobby (rating == average)
            // expected is 3 wins, so >3 rounds gains and <3 loses, symmetric and bounded to +/- kRoundsPerSeries*kPerGameK/... .
            double const expectedPerRound = 1.0 / (1.0 + std::pow(10.0, (avgRating - double(s.Rating)) / 400.0));
            double const expectedWins = expectedPerRound * double(kRoundsPerSeries);
            int32 const delta = int32(std::lround(double(kPerGameK) * (double(s.RoundsWon) - expectedWins)));

            int32 newRating = int32(s.Rating) + delta;
            if (newRating < 0)
                newRating = 0;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHARACTER_ARENA_STATS);
            stmt->setUInt64(0, s.Guid.GetCounter());
            stmt->setUInt8(1, kSoloShuffleArenaSlot);
            stmt->setUInt16(2, uint16(newRating));
            trans->Append(stmt);

            TC_LOG_DEBUG("bg.battleground", "Solo Shuffle {}: {} finished {}/{} rounds won, rating {} -> {} ({}{})",
                GetInstanceID(), s.Guid.ToString(), s.RoundsWon, kRoundsPerSeries, s.Rating, newRating,
                delta >= 0 ? "+" : "", delta);
        }

        CharacterDatabase.CommitTransaction(trans);
    }

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

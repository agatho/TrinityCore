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

#ifndef TRINITYCORE_BATTLEGROUND_SOLO_SHUFFLE_H
#define TRINITYCORE_BATTLEGROUND_SOLO_SHUFFLE_H

#include "Arena.h"
#include <array>

// Rated Solo Shuffle: 6 solo players (2 healers + 4 damage) play 6 rounds of 3v3, the two teams reshuffled
// each round so every round is a distinct partition. Built on Arena (reuses its flag-cast AddPlayer, arena
// score plumbing and CheckWinConditions "a team is wiped" round-end trigger). The only new behaviour is that
// a round ending does NOT tear the instance down until all six rounds are played - EndBattleground intercepts
// each round end and either reshuffles-and-continues or, on round 6, runs the real Arena/Battleground teardown.
class TC_GAME_API BattlegroundSoloShuffle : public Arena
{
public:
    explicit BattlegroundSoloShuffle(BattlegroundTemplate const* battlegroundTemplate);

protected:
    void AddPlayer(Player* player, BattlegroundQueueTypeId queueId) override;

private:
    void EndBattleground(Team winner) override;   // per-round end (Arena::CheckWinConditions calls this)
    void StartRound();                            // reassign teams + reset + re-teleport the 6 players

    // The 6 distinct 2-element subsets of the 4 damage players {0,1,2,3}. Round k assigns subset k to the
    // healer-1 (ALLIANCE) side; the complement goes to the healer-2 (HORDE) side. All 6 partitions are distinct.
    static constexpr std::array<std::array<uint8, 2>, 6> kDpsPairForAllianceSide = { {
        { { 0, 1 } }, { { 0, 2 } }, { { 0, 3 } }, { { 1, 2 } }, { { 1, 3 } }, { { 2, 3 } }
    } };

    struct LobbySlot
    {
        ObjectGuid Guid;
        bool       IsHealer = false;
        uint8      RoundsWon = 0;
        uint16     Rating = 0;      // personal solo-shuffle MMR, loaded at join, re-persisted at series end
    };

    // Solo Shuffle keeps a personal matchmaker rating in character_arena_stats under a slot distinct from the
    // three team-arena slots (0=2v2, 1=3v3, 2=5v5); the series is 6 individual games, so the rating moves by a
    // bounded per-game Elo step against the lobby's average rating.
    static constexpr uint8 kSoloShuffleArenaSlot = 3;
    static constexpr uint16 kRoundsPerSeries     = 6;
    static constexpr int32  kPerGameK            = 16;   // total swing bounded to +/- (kRoundsPerSeries*kPerGameK)/2

    std::array<LobbySlot, 6> _lobby;   // slots 0/1 = the two healers, 2..5 = the four damage players
    uint8 _assigned = 0;               // how many lobby slots have been filled by AddPlayer
    uint8 _currentRound = 1;           // 1..6
};

#endif // TRINITYCORE_BATTLEGROUND_SOLO_SHUFFLE_H

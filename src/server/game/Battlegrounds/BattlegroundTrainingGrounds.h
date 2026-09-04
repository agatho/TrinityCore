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

#ifndef TRINITYCORE_BATTLEGROUND_TRAINING_GROUNDS_H
#define TRINITYCORE_BATTLEGROUND_TRAINING_GROUNDS_H

#include "Arena.h"

// Training Grounds: a solo, unrated PvP practice instance (BattlemasterList 1145). Built on Arena to reuse its
// map/flag/score plumbing, but a lone player with no opposing team must NOT be auto-declared the winner - so
// CheckWinConditions is a no-op. The practice session ends only when the player leaves.
class TC_GAME_API BattlegroundTrainingGrounds : public Arena
{
public:
    explicit BattlegroundTrainingGrounds(BattlegroundTemplate const* battlegroundTemplate);

private:
    void CheckWinConditions() override { }   // solo practice: never auto-end on "the other team is empty"
};

#endif // TRINITYCORE_BATTLEGROUND_TRAINING_GROUNDS_H

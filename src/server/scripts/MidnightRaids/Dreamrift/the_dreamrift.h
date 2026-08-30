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

#ifndef DEF_THE_DREAMRIFT_H_
#define DEF_THE_DREAMRIFT_H_

#include "CreatureAIImpl.h"

// The Dreamrift -- Midnight raid.
// Map 2939 "The Dreamrift" (DB2 @68887, CONFIRMED: InstanceType=2 raid, ExpansionID=11,
// Directory "2939"). JournalInstance 1314. One encounter.
#define TheDreamriftScriptName "instance_the_dreamrift"
#define DreamriftDataHeader "DRFT"

uint32 const DreamriftEncounterCount = 1;

enum DreamriftDataTypes
{
    // Encounters -- index == DungeonEncounter.Bit @68887
    DATA_CHIMAERUS = 0
};

// DungeonEncounter.db2 id (DB2 @68887, CONFIRMED) -- consumed by LoadDungeonEncounterData.
//   Chimaerus the Undreamt God = 3306 / JE 2795 / Bit 0 / Order 0
//
// Boss creature entry is CAPTURE-BLOCKED: world-DB spawn, NOT present in client DB2 @68887.
// Do NOT invent it. Once captured, add a creature-id enum + creatureData[] + LoadObjectData.

template <class AI, class T>
inline AI* GetTheDreamriftAI(T* obj)
{
    return GetInstanceAI<AI>(obj, TheDreamriftScriptName);
}

#define RegisterTheDreamriftCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetTheDreamriftAI)

#endif

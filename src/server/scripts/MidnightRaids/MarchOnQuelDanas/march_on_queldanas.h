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

#ifndef DEF_MARCH_ON_QUELDANAS_H_
#define DEF_MARCH_ON_QUELDANAS_H_

#include "CreatureAIImpl.h"

// March on Quel'Danas -- Midnight raid; source of the Chiming Void Curio omni-token / Tier-35.
// Map 2913 "March on Quel'Danas" (DB2 @68887, CONFIRMED: InstanceType=2 raid, ExpansionID=11,
// Directory "2913"). JournalInstance 1308. Two encounters (final = "Midnight Falls").
#define MarchOnQuelDanasScriptName "instance_march_on_queldanas"
#define MarchOnQuelDanasDataHeader "MOQD"

uint32 const MarchOnQuelDanasEncounterCount = 2;

enum MarchOnQuelDanasDataTypes
{
    // Encounters -- index == DungeonEncounter.Bit @68887
    DATA_BELOREN_CHILD_OF_ALAR = 0,
    DATA_MIDNIGHT_FALLS        = 1
};

// DungeonEncounter.db2 ids (DB2 @68887, CONFIRMED) -- consumed by LoadDungeonEncounterData.
//   Belo'ren, Child of Al'ar = 3182 / JE 2739 / Bit 0 / Order 0
//   Midnight Falls (final)   = 3183 / JE 2740 / Bit 1 / Order 1000
//
// Boss creature entries are CAPTURE-BLOCKED: world-DB spawns, NOT present in client DB2 @68887.
// Do NOT invent them. Once captured, add a creature-id enum + creatureData[] + LoadObjectData.

template <class AI, class T>
inline AI* GetMarchOnQuelDanasAI(T* obj)
{
    return GetInstanceAI<AI>(obj, MarchOnQuelDanasScriptName);
}

#define RegisterMarchOnQuelDanasCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetMarchOnQuelDanasAI)

#endif

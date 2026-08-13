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

#ifndef DEF_THE_VOIDSPIRE_H_
#define DEF_THE_VOIDSPIRE_H_

#include "CreatureAIImpl.h"

// The Voidspire -- Midnight Season 1 main raid.
// Map 2912 "The Voidspire" (DB2 @68887, CONFIRMED: InstanceType=2 raid, ExpansionID=11, Directory "2912").
// JournalInstance 1307. Six encounters (ordered by DungeonEncounter.Bit).
#define TheVoidspireScriptName "instance_the_voidspire"
#define VoidspireDataHeader "VDSP"

uint32 const VoidspireEncounterCount = 6;

enum VoidspireDataTypes
{
    // Encounters -- index == DungeonEncounter.Bit @68887
    DATA_IMPERATOR_AVERZIAN    = 0,
    DATA_VORASIUS              = 1,
    DATA_VAELGOR_EZZORAK       = 2,
    DATA_FALLEN_KING_SALHADAAR = 3,
    DATA_LIGHTBLINDED_VANGUARD = 4,
    DATA_CROWN_OF_THE_COSMOS   = 5
};

// DungeonEncounter.db2 ids (DB2 @68887, CONFIRMED) -- consumed by LoadDungeonEncounterData.
// (DungeonEncounter id / JournalEncounter id / Bit / OrderIndex)
//   Imperator Averzian    = 3176 / JE 2733 / Bit 0 / Order 0
//   Vorasius              = 3177 / JE 2734 / Bit 1 / Order 1000
//   Vaelgor & Ezzorak     = 3178 / JE 2735 / Bit 2 / Order 3500
//   Fallen-King Salhadaar = 3179 / JE 2736 / Bit 3 / Order 3000
//   Lightblinded Vanguard = 3180 / JE 2737 / Bit 4 / Order 4000
//   Crown of the Cosmos   = 3181 / JE 2738 / Bit 5 / Order 5000
//
// Boss creature entries are CAPTURE-BLOCKED: world-DB spawns, NOT present in client DB2 @68887
// (same shape as Voidscar Arena). Do NOT invent them. Once captured, add a
// VoidspireCreatureIds enum + a creatureData[] ObjectData table + LoadObjectData in the instance.

template <class AI, class T>
inline AI* GetTheVoidspireAI(T* obj)
{
    return GetInstanceAI<AI>(obj, TheVoidspireScriptName);
}

#define RegisterTheVoidspireCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetTheVoidspireAI)

#endif

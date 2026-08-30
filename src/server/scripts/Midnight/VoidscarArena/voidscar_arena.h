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

#ifndef DEF_VOIDSCAR_ARENA_H_
#define DEF_VOIDSCAR_ARENA_H_

#include "CreatureAIImpl.h"

// Voidscar Arena -- Midnight's 8th dungeon.
// Map 2923 "Voidscar Arena" (DB2 @68887, CONFIRMED: InstanceType=1 dungeon, ExpansionID=11).
// JournalInstance 1313. Bosses (in order): Taz'Rah, Atroxus, Charonus.
#define VoidscarArenaScriptName "instance_voidscar_arena"
#define DataHeader "VOIDSCAR"

uint32 const EncounterCount = 3;

enum VoidscarArenaDataTypes
{
    // Encounters
    DATA_TAZRAH   = 0,
    DATA_ATROXUS  = 1,
    DATA_CHARONUS = 2
};

// DungeonEncounter.db2 ids (DB2 @68887, CONFIRMED) -- consumed by LoadDungeonEncounterData:
//   Taz'Rah  = 3285 (OrderIndex 1, Bit 0)
//   Atroxus  = 3286 (OrderIndex 2, Bit 1)
//   Charonus = 3287 (OrderIndex 3, Bit 2)
//
// Boss creature entries are CAPTURE-BLOCKED: world-DB spawns, NOT present in client DB2 @68887
// (same shape as Slayer's Rise domanaar). Do NOT invent them. Once captured, add:
//   enum VoidscarArenaCreatureIds { NPC_TAZRAH = <capture>, NPC_ATROXUS = <capture>, NPC_CHARONUS = <capture> };
// and wire them into a creatureData[] ObjectData table + LoadObjectData in the instance script.

template <class AI, class T>
inline AI* GetVoidscarArenaAI(T* obj)
{
    return GetInstanceAI<AI>(obj, VoidscarArenaScriptName);
}

#define RegisterVoidscarArenaCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetVoidscarArenaAI)

#endif

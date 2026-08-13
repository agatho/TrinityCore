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

#ifndef DEF_SPOREFALL_H_
#define DEF_SPOREFALL_H_

#include "CreatureAIImpl.h"

// Sporefall -- Midnight 12.0.7 flex-Mythic raid (flex-Mythic Difficulty 233 built on
// feature/raid-season-s1). One encounter (Rotmire).
// Map 1592 (DB2 @68887, CONFIRMED: InstanceType=2 raid, ExpansionID=11) -- BUT its Directory is
// "DevMapE", i.e. Blizzard's DEV-SHELL map. World data for this map may NOT load / may be a WIP
// placeholder; the instance script + encounter journal are wired regardless, but do not expect a
// playable Sporefall until a real (non-DevMapE) map ships. JournalInstance 1305.
#define SporefallScriptName "instance_sporefall"
#define SporefallDataHeader "SPFL"

uint32 const SporefallEncounterCount = 1;

enum SporefallDataTypes
{
    // Encounters -- index == DungeonEncounter.Bit @68887
    DATA_ROTMIRE = 0
};

// DungeonEncounter.db2 id (DB2 @68887, CONFIRMED) -- consumed by LoadDungeonEncounterData.
//   Rotmire = 3159 / JE 2711 / Bit 0 / Order 0
//
// Boss creature entry is CAPTURE-BLOCKED: world-DB spawn, NOT present in client DB2 @68887.
// Do NOT invent it. Once captured, add a creature-id enum + creatureData[] + LoadObjectData.

template <class AI, class T>
inline AI* GetSporefallAI(T* obj)
{
    return GetInstanceAI<AI>(obj, SporefallScriptName);
}

#define RegisterSporefallCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetSporefallAI)

#endif

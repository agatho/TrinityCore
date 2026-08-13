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

// This is where scripts' loading functions should be declared:

// The Voidspire (Midnight S1 main raid, Map 2912, JournalInstance 1307) -- 6 encounters
void AddSC_instance_the_voidspire();
void AddSC_boss_imperator_averzian();
void AddSC_boss_vorasius();
void AddSC_boss_vaelgor_ezzorak();
void AddSC_boss_fallen_king_salhadaar();
void AddSC_boss_lightblinded_vanguard();
void AddSC_boss_crown_of_the_cosmos();

// March on Quel'Danas (Map 2913, JournalInstance 1308) -- 2 encounters
void AddSC_instance_march_on_queldanas();
void AddSC_boss_beloren_child_of_alar();
void AddSC_boss_midnight_falls();

// The Dreamrift (Map 2939, JournalInstance 1314) -- 1 encounter
void AddSC_instance_the_dreamrift();
void AddSC_boss_chimaerus();

// Sporefall (Map 1592 [DevMapE dev shell], JournalInstance 1305) -- 1 encounter
void AddSC_instance_sporefall();
void AddSC_boss_rotmire();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddMidnightRaidsScripts()
{
    // The Voidspire
    AddSC_instance_the_voidspire();
    AddSC_boss_imperator_averzian();
    AddSC_boss_vorasius();
    AddSC_boss_vaelgor_ezzorak();
    AddSC_boss_fallen_king_salhadaar();
    AddSC_boss_lightblinded_vanguard();
    AddSC_boss_crown_of_the_cosmos();

    // March on Quel'Danas
    AddSC_instance_march_on_queldanas();
    AddSC_boss_beloren_child_of_alar();
    AddSC_boss_midnight_falls();

    // The Dreamrift
    AddSC_instance_the_dreamrift();
    AddSC_boss_chimaerus();

    // Sporefall
    AddSC_instance_sporefall();
    AddSC_boss_rotmire();
}

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

// Dornogal
void AddSC_zone_dornogal();

// Zone Isle Of Dorn
void AddSC_zone_isle_of_dorn();

// Campaign: Visions of a Shadowed Sun
void AddSC_campaign_visions_of_a_shadowed_sun();

// The Stonevault
void AddSC_instance_the_stonevault();
void AddSC_boss_edna();
void AddSC_boss_skarmorak();

// Nerub'ar Palace
void AddSC_instance_nerubar_palace();
void AddSC_boss_ulgrax_the_devourer();

// City of Threads
void AddSC_instance_city_of_threads();
void AddSC_boss_orator_krix_vizk();

// Darkflame Cleft
void AddSC_instance_darkflame_cleft();
void AddSC_boss_the_candle_king();

// Delves - Isle of Dorn
void AddSC_instance_earthcrawl_mines();
void AddSC_instance_fungal_folly();
void AddSC_instance_kriegvals_rest();
// Delves - The Ringing Deeps
void AddSC_instance_the_waterworks();
void AddSC_instance_the_dread_pit();
// Delves - Hallowfall
void AddSC_instance_nightfall_sanctum();
void AddSC_instance_mycomancer_cavern();
void AddSC_instance_skittering_breach();
void AddSC_instance_the_sinkhole();
// Delves - Azj-Kahet
void AddSC_instance_the_spiral_weave();
void AddSC_instance_tak_rethan_abyss();
void AddSC_instance_the_underkeep();
void AddSC_instance_zekvirs_lair();
// Delves - Common
void AddSC_npc_delve_entrance();
void AddSC_npc_brann_bronzebeard_delves();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddKhazAlgarScripts()
{
    // Dornogal
    AddSC_zone_dornogal();

    // Zone Isle of Dorn
    AddSC_zone_isle_of_dorn();

    // Campaign: Visions of a Shadowed Sun
    AddSC_campaign_visions_of_a_shadowed_sun();

    // The Stonevault
    AddSC_instance_the_stonevault();
    AddSC_boss_edna();
    AddSC_boss_skarmorak();

    // Nerub'ar Palace
    AddSC_instance_nerubar_palace();
    AddSC_boss_ulgrax_the_devourer();

    // City of Threads
    AddSC_instance_city_of_threads();
    AddSC_boss_orator_krix_vizk();

    // Darkflame Cleft
    AddSC_instance_darkflame_cleft();
    AddSC_boss_the_candle_king();

    // Delves - Isle of Dorn
    AddSC_instance_earthcrawl_mines();
    AddSC_instance_fungal_folly();
    AddSC_instance_kriegvals_rest();
    // Delves - The Ringing Deeps
    AddSC_instance_the_waterworks();
    AddSC_instance_the_dread_pit();
    // Delves - Hallowfall
    AddSC_instance_nightfall_sanctum();
    AddSC_instance_mycomancer_cavern();
    AddSC_instance_skittering_breach();
    AddSC_instance_the_sinkhole();
    // Delves - Azj-Kahet
    AddSC_instance_the_spiral_weave();
    AddSC_instance_tak_rethan_abyss();
    AddSC_instance_the_underkeep();
    AddSC_instance_zekvirs_lair();
    // Delves - Common
    AddSC_npc_delve_entrance();
    AddSC_npc_brann_bronzebeard_delves();
}

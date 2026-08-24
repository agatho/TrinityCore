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

#ifndef TRINITY_PVP_SEASON_RULES_H
#define TRINITY_PVP_SEASON_RULES_H

#include "Define.h"

// ---------------------------------------------------------------------------
// Midnight Season 1 PvP ruleset constants.
//
// PLACEHOLDER VALUES — flagged for verification against the live 12.0.7 patch
// notes / a tester capture. Sourced from public patch-note coverage (Icy Veins,
// Wowhead, Blizzard PvP article), NOT from client DB2 (these are engine rules,
// not data rows). See C:\dumps\SLAYERS_RISE_BLUEPRINT.md §1b.
// ---------------------------------------------------------------------------
namespace PvPSeasonRules
{
    // Diminishing-returns reset window. Retail Midnight Season 1 shortened this
    // from 18s to 16s. (NOTE: the project backlog said "20s DR resets" — 20s is
    // the *Season 2* value; Season 1 == 16s. Corrected here per cited evidence.)
    constexpr uint32 DR_RESET_SECONDS = 16;

    // Healing received while in a battleground is reduced. Retail Midnight S1:
    // players take 20% less healing in battlegrounds (incl. Blitz / rated BG).
    constexpr float BATTLEGROUND_HEALING_MULTIPLIER = 0.80f;

    // CC immunity now triggers after 2 applications of the same category instead
    // of 3. This is a deeper DiminishingLevels change (per-spell max level in
    // Unit::IncrDiminishing) — TRACKED here for documentation; NOT yet wired.
    constexpr uint32 DR_MAX_APPLICATIONS_BEFORE_IMMUNE = 2;
}

#endif // TRINITY_PVP_SEASON_RULES_H

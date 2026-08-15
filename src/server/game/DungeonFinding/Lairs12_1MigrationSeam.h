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

#ifndef _LAIRS_12_1_MIGRATION_SEAM_H
#define _LAIRS_12_1_MIGRATION_SEAM_H

// ============================================================================
// Lairs (Patch 12.1.0, client build 69299) — migration seam.
//
// SCOPE VERDICT: Lairs are case (b) — an LFG *category* + instanced *content*,
// NOT a new client subsystem. Reverse-engineered offline from the 12.1 client
// memory dump (flat, offset==RVA, image base 0x7FF6D0200000). Full evidence:
//   c:/dumps/tools/dump121/lairs/lairs_12_1_spec.md
//
// The COMPLETE Lair footprint in the 12.1 client binary is six strings:
//   * LE_LFG_CATEGORY_LAIR  — a new LFG-category enum value (see below), UI-only
//   * LAIRS_CAN_ENTER       — Lua global, client feature/season availability gate
//   * HasActiveLFGLair      — Lua getter over client-cached LFG state
//   * HasActiveLair         — Lua getter over client-cached instance state
//   * IsInLair              — Lua getter, "is the player currently in a Lair"
//   * "Lairs"               — a UI/data-table label
//
// There is NO Lair opcode, NO Lair JAM message type, NO Lair network handler,
// NO new Lair Difficulty enum, and NO new Lair DB2 record class. Verified
// against new_69299_opcodes.json, opcode_map_68275_to_69299.json,
// wow_jam_messages_69299.json, wow_enums_69299.json, cvars_121.json and
// api_diff_68275_vs_121.json — every case-(a) marker is negative.
//
// CONSEQUENCE FOR TRINITYCORE: no server protocol change is required. A Lair is
// queued through the EXISTING Dungeon Finder / LFG pipeline (existing CMSG_DF_*
// opcodes carry LFGDungeons IDs; the client never sends the category) and run as
// an EXISTING-difficulty instanced encounter. Everything that makes a Lair a
// Lair — its Map, its Difficulty rows, its 15-25 flexible scaling, its
// LFGDungeons row, its GroupFinderActivity listing, its encounters — is DB2 /
// world-DB CONTENT (the offline ceiling), imported when TrinityCore moves to
// 12.1. No id is invented here.
//
// This header is intentionally NOT #included by any live translation unit on the
// 12.0.7 base: it records the migration facts at one auditable place without
// perturbing the current build. Wire it in (and fill IsLairDungeonType below)
// during the actual 12.1 base migration.
// ============================================================================

#include <cstdint>

namespace lfg
{
namespace lairs
{
    // ------------------------------------------------------------------------
    // Client-side LE_LFG_CATEGORY enum, as registered by the 12.1 client Lua
    // registrar (base RVA 0x21057B0). LFD..BATTLEFIELD = 1..7 are the
    // known-stable historical WoW values (which validates the disassembly);
    // LAIR = 8 is appended in 12.1 and NUM is bumped 7 -> 8.
    //
    // THIS IS A CLIENT-ONLY UI CLASSIFICATION. It groups LFGDungeons.db2 rows in
    // the client's LFG panel and is NEVER sent on the wire. The server resolves
    // queued content by LFGDungeonsEntry::TypeID / Subtype, not by this value.
    // Mirrored here for migration provenance only; it has no server consumer.
    // ------------------------------------------------------------------------
    enum class ClientCategory : uint8_t
    {
        LFD          = 1,
        LFR          = 2,
        RF           = 3,
        SCENARIO     = 4,
        FLEXRAID     = 5,
        WORLDPVP     = 6,
        BATTLEFIELD  = 7,
        LAIR         = 8,   // <-- NEW in 12.1.0 (69299)
    };

    // NUM_LE_LFG_CATEGORYS: bumped 7 -> 8 by appending LAIR.
    constexpr uint8_t NUM_CLIENT_CATEGORIES = 8;

    // The 12.1 Lair LFGDungeons.db2 TypeID / Subtype discriminant is CONTENT and
    // is not present in the client binary — it cannot be read offline. When the
    // 12.1 DB2s are imported during the base migration, set these to the real
    // LFGDungeonsEntry::TypeID / Subtype used by Lair rows (and, if it is a value
    // TrinityCore's enum LfgType does not yet name, add it to LfgType in
    // LFGMgr.h). Left UNVERIFIED / 0 until then — deliberately not invented.
    constexpr uint8_t LAIR_LFG_TYPE_ID_UNVERIFIED  = 0;
    constexpr uint8_t LAIR_LFG_SUBTYPE_UNVERIFIED  = 0;

    // Server-side classification hook: "is this LFGDungeons row a Lair?"
    // Intentionally content-gated. It returns false until the discriminant above
    // is filled from the 12.1 LFGDungeons.db2 import, because there is no way to
    // recover the Lair TypeID/Subtype from a code dump and this fork never
    // invents content ids. This is the single seam a 12.1 migrator fills in.
    inline bool IsLairDungeonType(uint8_t lfgDungeonTypeId, uint8_t lfgDungeonSubtype)
    {
        if (LAIR_LFG_TYPE_ID_UNVERIFIED == 0)
            return false; // discriminant not yet imported from 12.1 DB2 content

        return lfgDungeonTypeId == LAIR_LFG_TYPE_ID_UNVERIFIED
            && lfgDungeonSubtype == LAIR_LFG_SUBTYPE_UNVERIFIED;
    }

    // Difficulty note: the 12.1 client adds NO new DIFFICULTY_* string and NO new
    // Difficulty enum value vs 68275. Lairs reuse EXISTING Normal / Heroic /
    // flexible-Mythic difficulties (Difficulty.db2 flags) on new Map rows. No new
    // difficulty enum is introduced here because none exists in the binary.

} // namespace lairs
} // namespace lfg

#endif // _LAIRS_12_1_MIGRATION_SEAM_H

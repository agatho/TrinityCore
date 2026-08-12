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

#ifndef TRINITYCORE_LOA_BLESSING_MGR_H
#define TRINITYCORE_LOA_BLESSING_MGR_H

#include "Define.h"
#include <string>
#include <vector>

class Player;

// Midnight Zul'Aman "Loa Blessings" — worship a major loa combined with a
// minor loa at the Altar of Blessings (creature 256508) to receive a temporary
// zone-scoped buff. Unlocked by quest 93792 "Blessings of the Loa"; blessings
// are active only while in Zul'Aman (AreaTable 15947) and only one may be held
// at a time. Ties into the "Loa of Abundance" harvest event.
//
// Every spell id seeded for this system is DB2-anchored @ build 12.0.7.68887
// (SpellName.db2). See C:\dumps\LOA_BLESSINGS_BLUEPRINT.md for the evidence map.

enum class LoaMajor : uint8
{
    None        = 0,
    Akilzon     = 1,    // Eagle  — "Gale"    (base 1235600)
    Halazzi     = 2,    // Lynx   — "Guile"   (base 1227122)
    Janalai     = 3,    // Dragonhawk — "Everburn" (base 1235170)
    Nalorakk    = 4     // Bear   — "Pressure" (base 1236349)
};

// Zul'Aman zone id (DB2 AreaTable @68887: 15947 "Zul'Aman", map 0).
constexpr uint32 ZONE_ID_ZULAMAN = 15947;

// One selectable altar option = a (major, minor) pairing that maps to a
// concrete blessing aura spell. minorLoa == 0 means the base major-only blessing.
struct LoaBlessingOption
{
    uint8       MajorLoa           = 0;     // LoaMajor
    uint8       MinorLoa           = 0;     // 0 = base blessing, 1..N = minor loa slot
    uint32      SpellId            = 0;     // DB2-confirmed blessing aura
    uint32      UnlockConditionId  = 0;     // PlayerConditionID gating this option (0 = always)
    std::string Name;                       // gossip label
};

class TC_GAME_API LoaBlessingMgr
{
public:
    LoaBlessingMgr() = default;
    ~LoaBlessingMgr() = default;

    LoaBlessingMgr(LoaBlessingMgr const&) = delete;
    LoaBlessingMgr& operator=(LoaBlessingMgr const&) = delete;

    static LoaBlessingMgr* instance();

    // Loads loa_blessing_option from the world DB. Tolerates an absent/empty
    // table (realm-safe): the altar gossip simply presents no options.
    void LoadFromDB();

    std::vector<LoaBlessingOption> const& GetOptions() const { return _options; }
    LoaBlessingOption const* GetOption(uint8 majorLoa, uint8 minorLoa) const;
    LoaBlessingOption const* GetOptionByIndex(size_t index) const;

    // True while the player stands in Zul'Aman (the only place blessings apply).
    static bool IsInZulAman(Player const* player);

    // Worship loop: drop any held loa blessing then apply the chosen one.
    // Returns false when gated (wrong zone / unknown option).
    bool ApplyBlessing(Player* player, uint8 majorLoa, uint8 minorLoa) const;
    void RemoveHeldBlessing(Player* player) const;

    // Abundance tie-in seam. Called (later) when a player completes a harvest in
    // the Loa-of-Abundance event so the two systems reinforce each other.
    // CAPTURE-BLOCKED no-op: reward wire + ZoneEventMgr Abundance hook unproven.
    void OnAbundanceHarvest(Player* player) const;

private:
    std::vector<LoaBlessingOption> _options;
};

#define sLoaBlessingMgr LoaBlessingMgr::instance()

#endif // TRINITYCORE_LOA_BLESSING_MGR_H
